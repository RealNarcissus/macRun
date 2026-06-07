# macRun Architecture Overview

This document describes the systems architecture of **macRun**, a governed macOS
application compatibility and runtime substitution platform for Linux.

> **Implementation Status**: Tier 0 (Electron Runtime Substitution) is functional and
> validated against four real-world applications. Tiers 1–4 adapters have established
> interfaces but are not yet functional. See [ROADMAP.md](../../ROADMAP.md) for the
> current phase status.

---

## Architecture Design Philosophy

macRun is built around three core architectural tenets:

1. **Asymmetric Application Category Strategy**: Electron apps are not native Cocoa
   apps, and Cocoa apps are not SwiftUI apps. Treating them identically is inefficient.
   Decoupling application types allows targeted compatibility layers per category.

2. **Hybrid Execution Model**: Rather than striving for ideological purity (e.g., full
   OS emulation), macRun selects the lowest-complexity viable strategy: native runtime
   substitution where possible, translation where feasible, and VM-assisted
   virtualization where required.

3. **Orchestration / Substrate Separation**: The platform's orchestrator is
   substrate-agnostic. Runtimes are isolated behind clean, adapter-based boundaries to
   ensure stability and maintainability.

```
                          macOS Runtime Platform for Linux
┌──────────────────────────────────────────────────────────────────────────────┐
│                                                                              │
│                         User launches macOS app                              │
│                                                                              │
└──────────────────────────────────────────────────────────────────────────────┘
                                       │
                                       ▼
                      ┌────────────────────────────────┐
                      │        macrun Launcher         │
                      │  DMG mount • Bundle analysis   │
                      │  Tier classification           │
                      └────────────────────────────────┘
                                       │
           ┌───────────────────────────┼───────────────────────────┐
           │                           │                           │
           ▼                           ▼                           ▼
┌──────────────────┐      ┌──────────────────────┐     ┌──────────────────────┐
│ Tier 0           │      │ Tier 1–3             │     │ Tier 4B              │
│ Runtime Swap     │      │ Compatibility Layer  │     │ VM-Assisted Runtime  │
├──────────────────┤      ├──────────────────────┤     ├──────────────────────┤
│ Electron ✓       │      │ Darling Runtime      │     │ macOS VM             │
│ Tauri            │      │ Mach-O Loader        │     │ Window Streaming     │
│ Wails            │      │ Cocoa Lite           │     │ Hotkey Bridge        │
│ PWAs             │      │ ARM64 Translation    │     │ Clipboard Sync       │
└──────────────────┘      └──────────────────────┘     └──────────────────────┘
 [FUNCTIONAL]              [SKELETON ADAPTERS]           [PLANNED]
           │                           │                           │
           └───────────────┬───────────┴───────────┬──────────────┘
                           ▼                       ▼
                ┌────────────────────────────────────┐
                │     Native Linux UX Integration    │
                ├────────────────────────────────────┤
                │ Wayland • Clipboard • Audio        │
                │ Notifications • Files • Hotkeys   │
                └────────────────────────────────────┘
```

---

## The 5-Stage Capability Detection Pipeline

When an application is launched via the `macrun` command, it passes through a
structured, static-analysis pipeline to resolve its execution strategy:

1. **Stage 1: Bundle Inspection**: Analyzes `Info.plist` and the main executable header
   (checking for Mach-O magic bytes, architectures, and minimum macOS deployment
   target).
2. **Stage 2: Dependency & Framework Probing**: Recursively checks the application's
   linked frameworks, dynamic libraries, and assets (detecting Chromium frameworks,
   Tauri bundle structures, AppKit/SwiftUI imports, and native Apple Silicon `.node`
   libraries).
3. **Stage 3: Entitlements Analysis**: Parses Mach-O entitlements to identify required
   system services (e.g., hypervisor, accessibility, sandboxing).
4. **Stage 4: Capability Scoring**: Aggregates the gathered signatures into structured
   metadata and matches it against the compatibility database (`compat-db`).
5. **Stage 5: Strategy Resolution**: Computes the optimal execution tier (0 to 4B) and
   selects the appropriate adapter.

---

## Tier 0: Runtime Substitution

**Status: Functional.** Validated against four production applications.

To support modern web-native desktop applications (Electron, Tauri, Wails), macRun
avoids binary emulation entirely:

- **Decoupled Assets**: macRun extracts the JavaScript/HTML/CSS assets (e.g.,
  extracting the `.asar` file in Electron apps).
- **Environment Swap**: The assets are executed directly inside a native, compatible
  Linux runtime shell (such as a Linux Electron or WebKitGTK instance).
- **API Normalization & Shimming**: macRun runs a series of lightweight, conditional
  shims to reconcile macOS runtime assumptions with the Linux host. These shims map XDG
  paths, redirect notifications, capture clipboard actions, and handle platform
  differences.

### Application Compatibility Spectrum (Tier 0)

Empirical testing revealed that Electron applications are not uniform — they fall into
four architecture classes with distinct compatibility requirements. See
[COMPATIBILITY_SPECTRUM.md](../architecture/COMPATIBILITY_SPECTRUM.md) for the full
classification model.

| Class | Description                            | Example        | Status               |
|-------|----------------------------------------|----------------|----------------------|
| A     | Self-contained, no native dependencies | Obsidian       | Functional           |
| B     | API drift requiring normalization      | Claude Desktop | Functional           |
| C     | Native module compilation required     | Cursor         | Functional           |
| D     | External backend substitution required | Codex          | Functional           |

---

## Tier 1–2: Darling Substrate & Adapter Isolation

**Status: Skeleton adapters established. Not yet functional.**

For compiled command-line and simple graphical macOS binaries:

- **Darling Substrate**: macRun orchestrates a userspace Darling environment to provide
  Darwin Mach-O loading and syscall compatibility.
- **Adapterized Boundaries**: All substrate execution details are isolated behind strict
  interface adapters. Current implementation status:
  - `IElectronAdapter` — **Functional**. Controls asset parsing and Linux runtime
    launching.
  - `IDarlingAdapter` — **Skeleton**. Interface established; Darling hardening work is
    Phase 5 (future).
  - `IQemuAdapter` — **Skeleton**. Interface established; QEMU hybrid execution is
    Phase 7 (future).
  - `IQemuVmAdapter` / `IWebKitAdapter` — **Skeleton**. Interface established; VM
    streaming is Phase 8 (future).
- **No-Orchestration Policy**: Substrate adapters must only handle environment setup,
  execution, and local diagnostics. They are prohibited from making classification,
  policy, or global orchestration decisions.

---

## Tier 3–4: ARM64 Translation & VM-Assisted Streaming

**Status: Architecture defined. Implementation is future work (Phases 7–8).**

For complex, native macOS applications that cannot be executed in userspace:

- **ARM64 Translation (Tier 3)**: Integrates QEMU user-mode translation to compile and
  translate Apple Silicon Mach-O instructions to the x86_64 host processor.
- **VM-Assisted Streaming (Tier 4B)**: Instead of recreating macOS UI frameworks,
  Tier 4B runs the application inside a lightweight macOS virtual machine and forwards
  the individual window framebuffers.

> **Open Architecture Question**: The VM lifecycle model (persistent vs. on-demand) is
> not yet settled. A persistent VM has lower per-launch latency but higher memory
> overhead; an on-demand VM reduces idle footprint but adds launch latency. This
> decision will be made during Phase 8 implementation.

- **Host Proxy & Guest Bridge Daemon**:
  - **Guest Bridge**: A background daemon running in the macOS VM captures window
    buffers (via `CGWindow` capture), encodes frames, and routes input events.
  - **Host Proxy**: A Wayland proxy client running on Linux decodes the VirtIO streams,
    registers global hotkeys, routes clipboard events, and displays the windows
    seamlessly on the host desktop — a "reversed WSLg" model.

---

## Current Phase

Phase 4 (Advanced Native Integration) is the active development frontier. See
[ROADMAP.md](../../ROADMAP.md) for sub-phase breakdown and exit criteria.
