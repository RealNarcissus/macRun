# MacRun

**MacRun** is a governed macOS application compatibility and runtime substitution platform for Linux.

> [!WARNING]
> **Developer / Architecture Preview Release**
> This repository represents an early architecture preview and developer prototype. It is NOT a production-ready system, nor is it a universal tool to "run any macOS app." It is published to showcase the systems architecture, runtime substitution governance, and compatibility engineering discipline.

---

## The Core Concept: Governed Compatibility

Rather than trying to build a monolithic macOS emulator or fully reimplementing the massive SwiftUI/AppKit/Metal rendering stacks, **MacRun** implements a hybrid compatibility model. It classifies applications into execution tiers and selects the lowest-complexity viable runtime strategy:

1. **Tier 0: Runtime Substitution** (Electron/Tauri/Wails) — Runs the app's HTML/JS/CSS assets against a native Linux runtime shell (Chromium/WebKitGTK) coupled with a governance-controlled API normalization and shim layer.
2. **Tier 1: CLI Compatibility** (Darling Substrate) — Executes CLI and POSIX-compliant macOS binaries using translation/compatibility layers.
3. **Tier 2: Lightweight Cocoa** (AppKit/Cairo/Wayland) — Executes simple native AppKit applications using flattened CoreAnimation and basic rendering proxies.
4. **Tier 3-4: Translation & Virtualization** (ARM64/QEMU & VM-Assisted Streaming) — Designed for heavy, deeply integrated macOS-native applications via high-performance window streaming, bridge daemons, and hotkey/clipboard synchronization.

---

## Current Status

- **Tier 0 (Electron Substitution)**: **Functional**. Successfully validates real, production-grade applications on Linux.
  - **Obsidian**: Launches, renders, and is fully usable.
  - **Claude Desktop**: Launches, renders, handles IPC/runtime initialization, and is fully functional.
- **Darling Integration (Tier 1-2)**: Infrastructure phase (adapters and substrate boundaries established).
- **ARM64 Translation (Tier 3)**: Planned / Exploratory.
- **VM-Assisted Execution (Tier 4B)**: Planned / Exploratory.

---

## Validation Proofs

We have proven the hybrid runtime substitution architecture using **Claude Desktop** and **Obsidian** (which contain preload isolation, native module assumptions, and complex multi-process IPC startup). By decoupling the HTML/renderer assets from the macOS Electron runtime and substituting a normalized Linux Electron environment with governed shims, we achieve:
- Deterministic Electron runtime substitution.
- Structured, transparent compatibility degradation.
- Decoupled, adapterized orchestration boundaries.

---

## Non-Goals

To maintain project scope and integrity, the following are explicitly **out of scope**:
- **Not a macOS Desktop Clone**: We do not recreate Finder, Dock, or the macOS desktop environment.
- **Not a Wine Replacement**: We do not target general-purpose Windows or macOS game translation.
- **Not a Security Sandbox**: This platform does not provide containment guarantees beyond native process boundaries.
- **Not Universal Compatibility**: We do not promise SwiftUI, Metal, or AppKit parity.
- **Not Production-Ready**: Underactive components are under active design and stubbed.

---

## Architecture & Documentation

To explore the systems engineering details, governance rules, and implementation models, please refer to the public documentation index:

* **[Public Documentation Index](docs/public/README.md)**
* **[Architecture Overview](docs/public/ARCHITECTURE.md)**: Details runtime tiers, adapter boundaries, and execution substrates.
* **[Governance & Degradation](docs/public/GOVERNANCE.md)**: Governs API normalizations, shim execution, and graceful degradation classifications.
* **[Limitations & Tiers](docs/public/LIMITATIONS.md)**: Honest appraisal of supported tiers, compatibility policies, and non-goals.

---

## License

Subject to license finalization. Refer to individual source files.
