# Substrate Model — macOS Runtime Platform for Linux

This document formalizes the runtime substrates used by the macOS Runtime Platform for Linux, detailing their classification, ownership boundaries, lifecycle management, and architectural decoupling rules.

---

## 1. Architectural Principles and Invariants

To maintain systems integrity and prevent structural decay, all substrate integrations must adhere to the following invariants:
1. **Linux remains the Primary Environment:** Substrates exist solely as execution mechanics for macOS applications. The platform must never recreate the macOS desktop shell (Dock, Finder, global menu bars). All UX elements must integrate natively with standard Linux desktop services.
2. **Orchestration Decoupling:** The orchestrator ([macrun](file:///home/charleton/Desktop/agentProjects/Codex/MacOS%20Emulator/platform/macrun/)) and capability detection pipeline must never directly depend on upstream internals or private APIs of any substrate.
3. **Boundary Enforcement via Adapters:** Substrates must be isolated behind explicit adapter interfaces to ensure that changes in external dependencies (e.g. upgrades to Darling or QEMU) do not leak into platform logic or trigger cascading compilation breaks.
4. **Declarative Configuration:** Substrate selection, environment setup, and flag overrides must remain strictly declarative (queried from [compat-db](file:///home/charleton/Desktop/agentProjects/Codex/MacOS%20Emulator/compat-db/)) rather than procedurally hardcoded.

---

## 2. Substrate Classification Matrix

Substrates are categorized into three distinct classes based on their role in the execution tiers:

```
┌────────────────────────────────────────────────────────────────────────┐
│                              Linux Host                                │
├────────────────────────────────────────────────────────────────────────┤
│                                                                        │
│   Integration Substrates (Wayland • DBus • PipeWire • libsecret)       │
│                                                                        │
│    Foundational Substrates                                             │
│    ┌───────────────────┐  ┌───────────────────┐  ┌──────────────────┐  │
│    │ Electron Runtime  │  │  Darling Runtime  │  │   QEMU (ARM64)   │  │
│    │ (Tier 0)          │  │  (Tier 1-2)       │  │   (Tier 3)       │  │
│    └───────────────────┘  └───────────────────┘  └──────────────────┘  │
│                                                                        │
│   Virtualization (VirtIO)                                              │
│                                                                        │
└─────────────────────────────────┬──────────────────────────────────────┘
                                  │ VirtIO Connection
                                  ▼
┌────────────────────────────────────────────────────────────────────────┐
│                             macOS Guest                                │
├────────────────────────────────────────────────────────────────────────┤
│   SwiftUI / Metal Apps • CoreAnimation Surface (Tier 4B)               │
└────────────────────────────────────────────────────────────────────────┘
```

| Substrate | Class | Execution Tier | Abstraction Level |
| :--- | :--- | :--- | :--- |
| **Electron Runtime** | Foundational | Tier 0 | Substitutive application runtime |
| **WebKitGTK** | Foundational | Tier 0 | WebView runtime (Tauri Simple) |
| **Darling** | Foundational | Tier 1–2 | System call translation & environment |
| **QEMU User-Mode** | Foundational | Tier 3 | Instruction-level translation |
| **Wayland** | Integration | All GUI Tiers | Native graphics presentation |
| **DBus** | Integration | All GUI Tiers | Host service IPC integration |
| **PipeWire/PulseAudio** | Integration | Tiers 2 & 4B | Low-latency audio rendering |
| **VirtIO** | Integration | Tier 4B | Hypervisor-isolated bridge protocol |
| **libsecret** | Integration | Tiers 0 & 2 | Keyring credential storage |
| **libarchive** | Integration | Setup & Extract | Payload & DMG extraction |
| **Cairo** | Integration | Tier 2 | CoreGraphics vector layout emulation |
| **XDG Integration** | Integration | All Tiers | Directory & launcher standards |
| **Metal / SwiftUI / Vulkan** | Deferred | Tier 4 (VM only) | GPU & Renderer (Out of scope for host) |

---

## 3. Foundational Substrates

### Electron Runtimes
- **Description:** Pre-packaged native Linux Electron runtimes matching the major version used by the target macOS application.
- **Responsibility:** Running extracted macOS `.asar` application bundles natively on Linux.
- **Ownership Boundary:** [runtime/shims/](file:///home/charleton/Desktop/agentProjects/Codex/MacOS%20Emulator/runtime/shims/) handles Electron-specific shim libraries.
- **Adapter Requirements:** Browser interface overrides (keychain accesses, desktop notifications, window decorations) must be shimmed using preload scripts or injected node-modules. Native platform orchestration must never link against Electron binaries.
- **Lifecycle Ownership:** Spawned as a child process by `macrun`; exits with the application.
- **Coupling Rules:** Substituted Electron runtimes are decoupled from the host graphics engine. They communicate only with standard Linux desktop services via environment variables.
- **Upgrade Strategy:** Managed declaratively through version maps in `compat-db/manifests/`. Swapping Electron versions requires only updating the manifest file, requiring no changes to the launcher binary.
- **Replacement Policy:** Highly replaceable; Electron bundles can be swapped with equivalent versions of standard Linux Electron releases.

### WebKitGTK
- **Description:** Linux-native HTML5 browser rendering engine wrapper.
- **Responsibility:** Running the user interface for Tauri-based apps in WebKitGTK shells when the Rust backend is unavailable (Tauri Simple Mode).
- **Ownership Boundary:** Managed by shims inside [runtime/shims/](file:///home/charleton/Desktop/agentProjects/Codex/MacOS%20Emulator/runtime/shims/).
- **Adapter Requirements:** Decoupled behind a standard C/C++ WebView interface to isolate GTK runtime properties from platform core logic.
- **Lifecycle Ownership:** Instantiated by `macrun` when launching Tier 0 Tauri apps; bound to the window lifecycle.
- **Coupling Rules:** Strictly isolated to the Tauri Simple mode launcher.
- **Upgrade Strategy:** Upgraded through host system library packages.
- **Replacement Policy:** Can be swapped for QtWebEngine or a headless Chromium frame if GTK dependencies become undesirable.

### Darling
- **Description:** Mach-O loader, Objective-C runtime, and system call translation layer.
- **Responsibility:** Loading Mach-O binaries, shimming Mach/POSIX system interfaces, and supplying the framework foundation for native Cocoa utilities (Tiers 1–2).
- **Ownership Boundary:** Isolated inside [runtime/darling/](file:///home/charleton/Desktop/agentProjects/Codex/MacOS%20Emulator/runtime/darling/).
- **Adapter Requirements:** All operations must occur through the `darling` shell runner or isolated library loaders. The host launcher must never directly link with Darling-internal shared libraries.
- **Lifecycle Ownership:** Initialized as a prefix container at boot or runtime by the `macrun` orchestrator; shuts down when container processes complete.
- **Coupling Rules:** Communicates only with [runtime/cocoa/](file:///home/charleton/Desktop/agentProjects/Codex/MacOS%20Emulator/runtime/cocoa/) for vector drawing. No direct coupling with host-side window proxies or virtualization lifecycles.
- **Upgrade Strategy:** Darling container environments are packaged as self-contained prefixes. Upgrading Darling requires swapping the container prefix path without recompiling core host binaries.
- **Replacement Policy:** Difficult to replace. Darling constitutes the foundational engine for Mach-O execution; replacement would require a custom Mach-O loader and Cocoa reimplementation.

### QEMU User-Mode
- **Description:** Dynamic instruction-level translator for user space binaries.
- **Responsibility:** Translating ARM64 instructions to host x86_64 instructions when running Apple Silicon-only CLI and Cocoa binaries (Tier 3).
- **Ownership Boundary:** Managed inside [runtime/arm64/](file:///home/charleton/Desktop/agentProjects/Codex/MacOS%20Emulator/runtime/arm64/).
- **Adapter Requirements:** QEMU static user binaries are registered as `binfmt_misc` interpreters or prefixed during Darling loader execution.
- **Lifecycle Ownership:** Launched transparently by the kernel on execution of ARM64 Mach-O files; exits on thread termination.
- **Coupling Rules:** Couples exclusively to the dynamic translation path.
- **Upgrade Strategy:** Upstream QEMU versions are dropped in as binary upgrades to the static translator binary.
- **Replacement Policy:** Straightforward. Can be replaced with FEX-Emu or Box64 if performance characteristics warrant migration.

---

## 4. Integration Substrates

### Wayland
- **Description:** Modern Linux graphics protocol and window manager integration standard.
- **Responsibility:** Displaying and managing window surfaces for all graphical execution tiers (Tiers 0, 2, and 4B).
- **Ownership Boundary:** Managed inside [host/wayland/](file:///home/charleton/Desktop/agentProjects/Codex/MacOS%20Emulator/host/wayland/).
- **Adapter Requirements:** Direct Wayland connection handles are hidden behind a windowing manager interface class. Runtimes pass framebuffers or texture handles to this manager rather than accessing raw compositing sockets.
- **Lifecycle Ownership:** Starts with the platform daemon; registers connections as windows are created.
- **Coupling Rules:** Integrates with [host/input/](file:///home/charleton/Desktop/agentProjects/Codex/MacOS%20Emulator/host/input/) to route key events and mouse coordinates.
- **Upgrade Strategy:** Aligned with host compositor protocol extensions.
- **Replacement Policy:** Modular; windows can fall back to X11 rendering via XWayland if host client compositor libraries are unavailable.

### DBus
- **Description:** Standard desktop service message bus.
- **Responsibility:** Relaying system notifications, launching default handler utilities, and querying security subsystems.
- **Ownership Boundary:** Encapsulated in [host/integration/](file:///home/charleton/Desktop/agentProjects/Codex/MacOS%20Emulator/host/integration/).
- **Adapter Requirements:** Wrappers shield callers from raw DBus message connection blocks.
- **Lifecycle Ownership:** Long-lived connection maintained throughout the life of the orchestrator.
- **Coupling Rules:** Accessible to all integration shims requiring desktop capabilities (e.g. notifications mapping to `/org/freedesktop/Notifications`).
- **Upgrade Strategy:** Governed by highly stable freedesktop specifications.
- **Replacement Policy:** Highly stable; replacement is not anticipated due to Linux desktop standards.

### PipeWire / PulseAudio
- **Description:** Linux multimedia and audio routing servers.
- **Responsibility:** Routing audio output from VM-assisted streams (Tier 4B) and emulation runtimes to the host speaker output.
- **Ownership Boundary:** Handled by [host/audio/](file:///home/charleton/Desktop/agentProjects/Codex/MacOS%20Emulator/host/audio/).
- **Adapter Requirements:** Wrapped under a generic `AudioStream` interface class. Audio data is streamed as raw PCM buffers.
- **Lifecycle Ownership:** Instantiated on demand when applications initiate audio pipelines; torn down when audio channels close.
- **Coupling Rules:** Couples exclusively to the host audio mix hardware.
- **Upgrade Strategy:** Standard host library upgrade path.
- **Replacement Policy:** Swappable between PulseAudio, PipeWire, or direct ALSA sinks.

### VirtIO
- **Description:** Standardized I/O virtualization protocol.
- **Responsibility:** Transferring window framebuffers, input commands, audio data, and clipboard payloads between host proxies and guest bridge daemons (Tier 4B).
- **Ownership Boundary:** Protocol code defined in [vm/virtio/](file:///home/charleton/Desktop/agentProjects/Codex/MacOS%20Emulator/vm/virtio/).
- **Adapter Requirements:** The communication protocol is decoupled from hypervisor interfaces. Standard sockets (e.g., vsock) are used for channel transport.
- **Lifecycle Ownership:** Spawned during VM boot sequence; shut down when guest VM is suspended or terminated.
- **Coupling Rules:** Direct consumer is [host/proxy/](file:///home/charleton/Desktop/agentProjects/Codex/MacOS%20Emulator/host/proxy/).
- **Upgrade Strategy:** Changes to VirtIO message serialization use version-tagged protocol headers to permit host-guest protocol updates.
- **Replacement Policy:** Sockets can be redirected to Standard TCP connections for remote or test-harness execution without hypervisor headers.

### libsecret
- **Description:** Client library for the GNOME Keyring and Secret Service API.
- **Responsibility:** Mapping macOS Keychain calls from Electron/Tauri shims to native Linux secret storage.
- **Ownership Boundary:** Used by [runtime/shims/](file:///home/charleton/Desktop/agentProjects/Codex/MacOS%20Emulator/runtime/shims/).
- **Adapter Requirements:** Hidden behind a standard `CredentialStore` interface.
- **Lifecycle Ownership:** Loaded dynamically on credential queries.
- **Coupling Rules:** Couples to DBus and GNOME Keyring/KWallet.
- **Upgrade Strategy:** Standard host library updates.
- **Replacement Policy:** Easily replaced with custom file-based encrypted credential sinks or keepass-compatible stores.

### libarchive
- **Description:** Multi-format archive read/write library.
- **Responsibility:** Extracting macOS application packages, DMG filesystem streams, and tar/xar payloads.
- **Ownership Boundary:** Contained within [tooling/extractor/](file:///home/charleton/Desktop/agentProjects/Codex/MacOS%20Emulator/tooling/extractor/).
- **Adapter Requirements:** Wrapped inside a simple `ArchiveExtractor` class.
- **Lifecycle Ownership:** Transient execution during package installations.
- **Coupling Rules:** Called only during installation phases.
- **Upgrade Strategy:** Standard host package updates.
- **Replacement Policy:** Easily swapped for shell command invocations (`dmg2img`, `tar`, `7z`) if dependency removal is required.

### Cairo
- **Description:** 2D vector graphics library supporting hardware acceleration.
- **Responsibility:** emulating CoreGraphics rendering for lightweight AppKit windows (Tier 2).
- **Ownership Boundary:** Under [runtime/cocoa/](file:///home/charleton/Desktop/agentProjects/Codex/MacOS%20Emulator/runtime/cocoa/).
- **Adapter Requirements:** Decoupled behind the `CoreGraphics` class structures. Drawing code calls standard CG functions, which translate internally to Cairo surfaces.
- **Lifecycle Ownership:** Bound to Cocoa AppKit refresh loops.
- **Coupling Rules:** Limited entirely to the Tier 2 emulated graphics runtime.
- **Upgrade Strategy:** Standard library package upgrades.
- **Replacement Policy:** Can be swapped for Skia or Pixman if rendering fidelity or performance characteristics mandate a change.

### XDG Integration
- **Description:** Freedesktop standards for file associations, config directories, and application launchers.
- **Responsibility:** Standardizing file system layouts (config and cache maps), generating `.desktop` files, and executing web page redirections (`xdg-open`).
- **Ownership Boundary:** Implemented in [host/integration/](file:///home/charleton/Desktop/agentProjects/Codex/MacOS%20Emulator/host/integration/).
- **Adapter Requirements:** Wrapped in desktop launcher utility helper classes.
- **Lifecycle Ownership:** Active at install and launch configuration.
- **Coupling Rules:** Utilized across all execution tiers to achieve native look and feel.
- **Upgrade Strategy:** Stable freedesktop standards.
- **Replacement Policy:** Core to Linux integration; non-replaceable.

---

## 5. Deferred Substrates

To ensure sustainable development progress, the following rendering substrates are explicitly classified as **Deferred**. Re-implementing these on Linux would expand engineering scope unsustainably:

1. **Metal Graphics Translation:** Emulating Apple's Metal graphics API on top of Vulkan on the host Linux machine is deferred. Applications demanding Metal rendering default to VM-assisted streaming (Tier 4B), where frames are rendered natively inside macOS guest systems using virtual GPU hardware.
2. **SwiftUI Runtime:** Complex SwiftUI-native apps (e.g. launchers with global system hooks) are deferred for local user space emulation. They route to Tier 4B.
3. **Vulkan CoreAnimation Compositing:** High-fidelity hardware-accelerated CoreAnimation layer compositing is deferred. Tier 2 uses simple CPU surface flattening. Complex layer composition is delegated to guest VM streaming.

---

## 6. Adapter Layer Model

The adapter layer serves as the **architectural firewall** isolating the platform's core orchestration engine from third-party runtime substrate implementation details. Without strict enforcement of this layer, upstream API churn, ABI shifts, and substrate-specific assumptions will leak directly into the orchestrator ([macrun](file:///home/charleton/Desktop/agentProjects/Codex/MacOS%20Emulator/platform/macrun/)), leading to dependency coupling, build instability, and high maintenance overhead.

### Design Patterns & Decoupling Rules
1. **Header Isolation (No Leakage)**: Core orchestration headers and code must never include headers from Darling, QEMU, Electron, or GTK.
2. **Abstract Interface Design**: Adapters are defined as abstract base classes (C++ pure virtual interfaces) under `platform/common/adapters/`. These interfaces declare only standard C++ types or domain models defined in [platform/common/types.hpp](file:///home/charleton/Desktop/agentProjects/Codex/MacOS%20Emulator/platform/common/types.hpp).
3. **Data Translation Boundary**: Adapters are strictly responsible for translation. Substrate-specific processes, diagnostic strings, or proprietary return codes must be parsed and mapped to domain-level objects (e.g. mapping internal Darling container exit codes to generic `ProcessExitStatus` domain structures). Raw pointers or vendor-specific data structures must never cross the boundary.
4. **Mocking & Separate Testability**: All adapters must inherit from their respective interface, enabling full test stubbing/mocking in [tests/unit/](file:///home/charleton/Desktop/agentProjects/Codex/MacOS%20Emulator/tests/unit/). Host orchestration unit tests must compile and execute successfully without linking against Darling, WebKitGTK, or host-native virtualization headers.

---

### Adapter Responsibilities and Mapping

| Adapter | Technical Responsibility | Isolation Pattern |
| :--- | :--- | :--- |
| **darling-adapter** | Darling prefix provisioning, container setup, darling-server daemon lifecycle, Mach-O launch, and error propagation. | Abstract interface `IDarlingAdapter`. Interacts with `darling` client binary and manages prefix mounts inside isolated user namespaces. |
| **qemu-adapter** | `binfmt_misc` registration verification, CPU thread architecture mapping, translation flags setup, and VM process execution parameters. | Abstract interface `IQemuAdapter`. Wraps calls to QEMU static interpreters. |
| **electron-adapter** | Locating and verifying compatible cached runtimes via `compat-db`, sandbox setup, node-modules preloads, and path shimming. | Abstract interface `IElectronAdapter`. Wraps the target application execution path. |
| **webkit-adapter** | WebKitGTK frame rendering, main event loop binding, Tauri IPC callback routing, and GTK window configuration. | Abstract interface `IWebKitAdapter`. Separates GTK library symbols from core orchestrator compiling paths. |
| **wayland-adapter** | Wayland frame presentation, client compositor socket management, and input routing. | Abstract interface `IWaylandAdapter`. Wraps Wayland client structures. |
| **dbus-adapter** | Relaying freedesktop portal calls, desktop notifications, system settings query, and credential caching IPC. | Abstract interface `IDbusAdapter`. Normalizes DBus connection states. |

---

### Adapter Invariants
- **No Orchestration Policy**: Adapters must execute commands deterministically. They do not decide *which* execution tier to run, nor do they query [compat-db](file:///home/charleton/Desktop/agentProjects/Codex/MacOS%20Emulator/compat-db/) directly; they receive targets and configurations from the caller.
- **Diagnostics Normalization**: Any error or stack trace generated inside a substrate (e.g., a Darling kernel module crash or a QEMU translation fault) must be captured, parsed, and logged through a unified platform diagnostic logger.

---

## 7. Substrate Version Governance

To prevent regressions and maintain binary reproducibility, all foundational substrates must adhere to version pinning and compatibility governance:

### Governance Policies

| Concern | Strategy & Rule | Target Substrates |
| :--- | :--- | :--- |
| **Pinned vs Rolling** | Foundational runtimes must be pinned to exact releases or git commits. Rolling releases or dynamically resolved upstream updates are prohibited to ensure build reproducibility. | Darling, QEMU, Electron |
| **ABI Assumptions** | Host-guest interface uses version-locked VirtIO drivers. Darling uses target-versioned framework SDK shims to isolate binary updates. | Darling, VirtIO |
| **Upgrade Cadence** | Bi-monthly schedule to evaluate, validate, and pull upstream updates rather than immediate upstream adoption. | All Foundational Substrates |
| **CI Integration** | Matrix-based automated builds running integration tests across diverse Linux environments to prevent regressions. | Darling, QEMU, Electron, WebKitGTK |

### Version Baselines
- **Darling**: Pinned to custom fork hash `c3e5d0a` mapping system call compatibility requirements.
- **QEMU**: Pinned to stable major release version `8.2.0` (static translation) and `9.0.0` (VM virtualization).
- **Electron**: Locked to a predefined matrix matching `compat-db` manifests (specifically versions 22.x, 24.x, and 28.x).
- **WebKitGTK**: Bound to a minimum host package system baseline of `2.40.0`.

---

## 8. VM Hypervisor Architecture

The VM-Assisted Streaming engine (Tier 4B) executes complex macOS applications (including SwiftUI and Metal-heavy apps) by running them inside a virtualized macOS guest and streaming windows natively to the Linux host via VirtIO and Wayland. To prevent design drift and maintain high performance, the hypervisor runtime is governed by the following rules:

### Standardized Execution Model
1. **Is QEMU the only VM backend?**
   **Yes.** QEMU (with KVM hardware acceleration) is the exclusive hypervisor backend. The platform does not support alternative hypervisors (such as VirtualBox or VMware) to maintain a minimal dependency footprint and highly optimized KVM-specific configurations.
2. **Is libvirt involved?**
   **No.** Libvirt is completely bypassed. The host orchestrator launches QEMU processes directly and communicates via the QEMU Machine Protocol (QMP) over local UNIX domain sockets. Bypassing libvirt eliminates startup latency, configuration overhead, and unnecessary daemon dependencies.
3. **Is VM lifecycle centralized?**
   **Yes.** All VM instantiation, state transition, and host resource allocations are controlled by a dedicated host-side service daemon, the `vm-lifecycle-manager`, located under [vm/lifecycle/](file:///home/charleton/Desktop/agentProjects/Codex/MacOS%20Emulator/vm/lifecycle/).
4. **Are VMs ephemeral or persistent?**
   **Ephemeral session state with a persistent shared base.** The system maintains an immutable read-only base guest image (`macos-base.qcow2`). When a virtualized application starts, the platform creates a transient Copy-on-Write (COW) overlay file (`macos-session-xxx.qcow2`) backing the session. Any writes written to the guest filesystem are stored in this overlay and discarded upon session completion, ensuring a clean state for every execution.
5. **Is there one shared VM or per-app VM?**
   **Single shared VM instance by default.** To conserve memory and eliminate cold-start latency, a single running guest macOS VM instance processes and streams multiple guest applications concurrently. High-security profiles can declare dedicated, isolated VMs via [compat-db](file:///home/charleton/Desktop/agentProjects/Codex/MacOS%20Emulator/compat-db/) metadata, which forces the host to spawn separate guest instances.
6. **Who owns suspend/resume?**
   **The host `vm-lifecycle-manager`.** The host monitors guest application activity. When the last streamed application exits or goes idle, the host service sends QMP pause commands (`stop`) to suspend the VM state to RAM, reclaiming CPU resources. Activity triggers a resume command (`cont`).
7. **Where is guest image management?**
   **Managed exclusively on the Linux Host.** The host manages the storage repository, creates the session COW overlays, and mounts guest drives via `qemu-img` and `nbd` tools. The guest OS has no control over its disk provisioning or hardware layouts.

---

## 9. CI & Build Isolation Strategy

With multiple substrates, integration adapters, and runtimes, the risk of dependency collisions ("dependency hell") is high. The project mandates build-time and execution-time isolation:

### Concern to Strategy Map

| Concern | Isolation Strategy |
| :--- | :--- |
| **Darling builds** | Pinned Darling builds are compiled inside isolated containers or chroots (e.g., via bubblewrap or Docker) with their own LLVM/clang sysroots. This prevents compile-time library leakage or conflicts with standard host C++ libraries. |
| **QEMU builds** | QEMU static binaries are compiled out-of-tree or pulled from pre-compiled verified binary toolchains to minimize build-time CPU usage. |
| **Electron runtimes** | Electron binaries are never compiled. The build system pulls pre-packaged native Linux binaries matching specific targets, caching them in `~/.cache/macrun/electron/`. |
| **Adapters** | Compiled as modular units and separately tested in [tests/unit/](file:///home/charleton/Desktop/agentProjects/Codex/MacOS%20Emulator/tests/unit/) using mock objects, removing dependencies on live substrates during CI runs. |
| **Orchestration** | Core platform execution logic (`macrun`) has zero compilation dependencies on Darling or QEMU source files, relying entirely on interface classes. |
| **CI Execution** | Automated tests execute on a matrix representing supported host OS profiles (Ubuntu 22.04 LTS, Ubuntu 24.04 LTS, Arch Linux) to detect runtime issues. |