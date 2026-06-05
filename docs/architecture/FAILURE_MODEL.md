# Failure Model and Degradation Architecture

This document formalizes the failure-handling specifications, degradation pathways, and recovery boundaries for the macOS Runtime Platform for Linux.

---

## 1. Architectural Philosophy and Failure Strategy

To maintain execution safety and platform stability, the platform prioritizes **graceful degradation over outright crashes**.
When an application encounters compatibility limits, missing host libraries, or hardware virtualization constraints, the platform adaptively scales back fidelity, isolates execution substrates, and reports structured warning diagnostics rather than aborting process execution.

### cascading Safety Hierarchy:
- **Functional Fallback**: If a substrate subsystem fails but an emulator fallback exists (e.g., CoreAnimation CPU surface flattening vs. GPU acceleration), execution continues in a degraded state.
- **Explicit Blockers**: If a hard runtime dependency is missing (e.g., KVM support is unavailable for a Tier 4B SwiftUI app, or Darling's kernel module is missing for Tier 1), execution is cleanly blocked with a standardized error code.
- **Crash Containment**: A crash inside a runtime substrate (e.g., a segment fault inside Darling or a translation crash in QEMU) must be contained inside its isolated process sandbox and never crash the orchestrator daemon ([macrun](file:///home/charleton/Desktop/agentProjects/Codex/MacOS%20Emulator/platform/macrun/)).

---

## 2. Failure Modes Across Emulation Tiers

### Tiers 0–3 (User Space Emulation & Translation)

| Failure Mode | Affected Tier | Detection Mechanism | Platform Mitigation / Fallback |
| :--- | :--- | :--- | :--- |
| **ASAR Payload Corruption** | Tier 0 (Electron) | Verification of checksum or launch failure inside [electron-adapter](file:///home/charleton/Desktop/agentProjects/Codex/MacOS%20Emulator/platform/adapters/IElectronAdapter.hpp). | Halted execution; block reason written to diagnostic log. |
| **Missing Native Shim** | Tier 0 (Electron/Tauri) | Dynamic linker failure or preload hook exception. | Default callback stub returns empty/success value. |
| **Darling Prefix Corruption** | Tiers 1–2 (Darling) | Write failure to `DPREFIX` or overlay storage. | Automatically rebuild prefix using base reference templates. |
| **Darling Server Crash** | Tiers 1–2 (Darling) | Mach port timeout or daemon death detection in [darling-adapter](file:///home/charleton/Desktop/agentProjects/Codex/MacOS%20Emulator/platform/adapters/IDarlingAdapter.hpp). | Terminate prefix processes and attempt safe daemon restart. |
| **Instruction Translation Fault** | Tier 3 (QEMU User-Mode) | System crash signal (SIGILL, SIGSEGV) intercepted by dynamic translator. | Map fault to unified platform error code; terminate process. |

### Tier 4B (Virtualization & Streaming)

Virtualization failure management is critical due to the complex interaction between host and guest.

#### 1. Host-Guest Communication (VirtIO/vsock) Failures
- **Description**: Sockets or vsock channels drop unexpectedly.
- **Detection**: Heartbeat ping timeout in host proxy.
- **Mitigation**: The `vm-lifecycle-manager` attempts to re-establish vsock connection. If retries fail within 5 seconds, the running session is terminated and client Wayland windows are closed with `VM_COMMUNICATION_LOST` status.

#### 2. Guest macOS Kernel Panics
- **Description**: Virtualized guest OS encounters an unrecoverable kernel panic.
- **Detection**: Host monitors guest console output via QEMU serial port hooks (detects panic signatures) or QMP status query returns `paused` with reason `guest-panicked`.
- **Mitigation**: `vm-lifecycle-manager` forces VM power-down via QMP (`system_powerdown` or `quit`), discards the transient session overlay disk file, and boots a clean VM instance from the immutable base image.

#### 3. Compositor Starvation & Render Queue Anomalies
- **Description**: Network/storage latency prevents frame delivery, causing rendering stutter.
- **Detection**: Render frame queue latency in host proxy exceeds 100ms.
- **Mitigation**: Automatically reduce frame stream resolution or toggle H.264/HEVC compression ratios to stabilize frame rates, falling back to lower rendering profiles.

#### 4. Wayland Display Connection Loss
- **Description**: Host Wayland compositor crashes or socket drops.
- **Detection**: Wayland connection error hook triggered in `IWaylandAdapter`.
- **Mitigation**: Gracefully stop active application adapters, save VM snapshot to RAM (if Tier 4B application is running), and exit launcher daemon.

---

## 3. Standardized Degradation Pathways

When capabilities are degraded, the platform maps execution to specific fallback modes:

### CoreAnimation Compositing Flattening
- **Default**: Tier 2 emulates CoreAnimation layers using Vulkan-accelerated compositing.
- **Failure Trigger**: Vulkan initialization failure or shader compilation crash.
- **Fallback**: The emulation layer discards active hardware transitions and performs a static draw CPU fallback. Windows continue to render UI layouts correctly but without transition animations.

### Metal GPU API Failures
- **Default**: Metal API commands are forwarded to hardware acceleration layers or run natively in guest VM virtual GPUs.
- **Failure Trigger**: Host graphics driver crashes or guest virtualization GPU lacks Vulkan/Metal capabilities.
- **Fallback**: Metal translation defaults to a software renderer shim (e.g. LLVMpipe) or fails over to a CLI-only fallback tier with compatibility warnings: `MACRUN_NO_METAL=1`.

### Keyring and Keychain Service Fallbacks
- **Default**: Keychain commands are routed via `libsecret` to the native Linux keyring (GNOME Keyring/KWallet).
- **Failure Trigger**: Host keyring service daemon is stopped or unreachable over DBus.
- **Fallback**: The platform intercepts the call and falls back to a secure file-based encrypted credential store local to the application cache, warning the user.

---

## 4. Failure Isolation: The Architectural Firewall

To prevent a crash in a third-party substrate (e.g., a Darling segment fault or a QEMU translator crash) from crashing the parent platform launcher, the adapter layer acts as a safety firewall.

### Sandbox Isolation Rules
1. **Out-of-Process Execution**: Substrates run in separate POSIX processes isolated by cgroups and user namespaces. The orchestrator communicates with them asynchronously.
2. **Error Translation & Codes**: Adapters must intercept substrate-specific signals and map them to standardized platform error codes:
   - `DARLING_LAUNCH_FAILED`: Mach-O loader failure.
   - `DARLING_PREFIX_LOCK_FAILED`: Lock collision on overlay storage.
   - `QEMU_KVM_UNAVAILABLE`: Host hardware virtualization disabled.
   - `QEMU_QMP_DISCONNECT`: QEMU daemon terminated abruptly.
   - `ELECTRON_ASAR_CORRUPTED`: Target app payload corrupted.
   - `GTK_DISPLAY_ERROR`: GTK/display server socket connection failed.
3. **State Resets & Cleanup**: When an adapter transitions to the `Error` status, it must run cleanup procedures. This includes releasing file locks, unmounting temporary namespaces, terminating orphan helper threads, and reporting detailed diagnostics.
