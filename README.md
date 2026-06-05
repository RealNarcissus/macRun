# macOS Runtime Platform for Linux

A hybrid compatibility platform for running modern macOS applications on Linux.

This project is not a full macOS reimplementation.
Instead, it combines:

* runtime substitution,
* compatibility-layer execution,
* ARM64 translation,
* and VM-assisted application streaming

to run the macOS applications Linux users actually care about.

The platform is designed around:

* native-feeling Linux UX,
* sustainable engineering scope,
* and execution strategies matched to application categories.

---

# Project Status

Early architecture and infrastructure phase.

The current focus areas are:

* execution model definition,
* compatibility architecture,
* runtime orchestration,
* VM-assisted streaming design,
* and capability detection systems.

No stable releases are available yet.

---

# Goals

The project prioritizes:

* AI desktop tooling
* developer tooling
* Electron/Tauri ecosystems
* launcher/workflow applications
* lightweight native Cocoa utilities
* Linux-native user experience

The project intentionally does not pursue:

* full macOS parity,
* complete SwiftUI fidelity,
* kernel extension support,
* Hypervisor.framework compatibility,
* or full desktop-environment replication.

---

# Architectural Overview

The platform classifies applications into execution tiers and selects the lowest-complexity viable runtime strategy.

```text
macOS App
    ↓
Capability Detection
    ↓
Tier Classification
    ↓
Execution Strategy Selection
    ↓
Native Linux Integration
```

Execution strategies include:

| Tier    | Strategy                                    |
| ------- | ------------------------------------------- |
| Tier 0  | Runtime substitution (Electron/Tauri/Wails) |
| Tier 1  | CLI compatibility via Darling               |
| Tier 2  | Lightweight Cocoa compatibility             |
| Tier 3  | ARM64 translation                           |
| Tier 4B | VM-assisted application streaming           |

The architecture is explicitly hybrid.

---

# Core Design Principles

## Utility Over Purity

The objective is:

* usable application compatibility,
  not:
* complete operating-system emulation.

If a native Linux runtime swap achieves the same user outcome as full compatibility-layer execution, runtime substitution is preferred.

---

## VM-Assisted Execution Is First-Class

Some modern macOS applications depend on:

* Accessibility APIs,
* system-wide hooks,
* SwiftUI assumptions,
* private frameworks,
* and deep compositor integration.

For these applications, the platform uses:

* lightweight macOS virtual machines,
* per-window streaming,
* Linux-native window presentation,
* clipboard synchronization,
* notification forwarding,
* and hotkey bridging.

This model is architecturally similar to WSLg, reversed.

---

## Linux Remains the Primary Environment

Linux is always the host environment.

macOS runtimes exist only as:

* execution substrates for macOS applications.

The project does not attempt to recreate:

* Finder,
* Dock,
* or the broader macOS desktop experience.

---

# Repository Structure

```
docs/                     Architecture, design, protocols, compatibility docs
├── architecture/         Canonical architecture specification (authoritative)
├── design/               Per-subsystem design documents
├── protocols/            Interface contract definitions and wire formats
├── compatibility/        Target application profiles and degradation specs
└── diagrams/             Architecture and data flow diagrams

compat-db/                Compatibility metadata and policies (like ProtonDB)
├── signatures/           Application and framework detection fingerprints
├── policies/             Execution strategy mappings and degradation rules
├── reports/              User-submitted compatibility reports and telemetry
└── manifests/            Runtime version mappings and dependency declarations

platform/                 Application orchestration and capability detection
├── macrun/               Launcher: .app/.dmg → execution strategy
├── detector/             Five-stage capability detection pipeline
│   ├── bundle/           Stage 1: Info.plist, Mach-O header analysis
│   ├── framework/        Stage 2: Framework fingerprinting
│   ├── arch/             Stage 3: x86-64/ARM64 architecture detection
│   └── scoring/          Stage 4-5: Capability scoring and strategy resolution
└── common/               Shared platform types

runtime/                  Execution backends (Tiers 0-3)
├── shims/                Tier 0: Electron/Tauri runtime substitution
├── darling/              Tier 1-2: Mach-O loader, syscall compatibility
├── cocoa/                Tier 2: Lightweight Cocoa compatibility
├── arm64/                Tier 3: Apple Silicon binary translation
└── common/               Shared runtime types

vm/                       macOS guest integration (Tier 4B)
├── bridge/               Guest-side bridge daemon
├── capture/              Per-window framebuffer capture
├── encode/               Frame encoding for transport
├── lifecycle/            VM start/suspend/resume orchestration
└── virtio/               Host-guest shared transport layer

host/                     Linux-side windowing and UX integration
├── proxy/                Host proxy: Wayland windows, input, clipboard, audio
├── wayland/              Wayland surface management
├── input/                Keyboard, mouse, hotkey forwarding
├── audio/                Guest-to-host audio routing
└── integration/          Clipboard, notifications, UX glue

tooling/                  Development and diagnostic utilities
├── extractor/            DMG and .app bundle extraction
├── inspector/            Static bundle analysis and capability probing
├── benchmark/            Performance measurement
└── devtools/             Debugging and development workflow tools

tests/                    Test suite
├── unit/                 Subsystem-isolated tests
├── integration/          Cross-subsystem contract-level tests
├── compatibility/        Target application compatibility validation
├── performance/          Latency and throughput benchmarks
└── fixtures/             Test data: sample .app bundles, DMGs, binaries

third_party/              Vendored dependencies
```

---

# Documentation

| Document                               | Purpose                                       |
| -------------------------------------- | --------------------------------------------- |
| `docs/architecture/Architecture_V6.md` | Canonical architecture specification          |
| `ROADMAP.md`                           | Development milestones and priorities         |
| `compat-db/`                           | Compatibility metadata and execution policies |

---

# Current Priorities

Immediate engineering priorities:

1. Capability detection engine
2. Runtime orchestration (`macrun`)
3. Electron runtime substitution
4. Tauri simple-mode execution
5. VM-assisted streaming prototype
6. Compatibility database infrastructure

---

# Planned Compatibility Targets

Initial application targets include:

| Category                | Examples                         |
| ----------------------- | -------------------------------- |
| AI desktop apps         | Cursor, Claude Desktop, Windsurf |
| Electron ecosystems     | VS Code derivatives, Obsidian    |
| Launcher/workflow tools | Raycast, Alfred                  |
| CLI tooling             | Homebrew, Swift tooling          |

---

# Non-Goals

The following are intentionally out of scope:

* gaming optimization
* kernel extension support
* Hypervisor.framework compatibility
* complete AppKit parity
* full SwiftUI correctness
* App Store integration
* DRM-heavy applications
* macOS desktop replication

---

# Contributing

The project is currently in architecture formation and infrastructure planning stages.

Contribution areas that will become important early:

* runtime analysis
* Mach-O tooling
* Electron/Tauri runtime adaptation
* Wayland integration
* VM streaming infrastructure
* compatibility testing
* protocol design
* ARM64 translation research

Contribution guidelines will be formalized as the implementation stabilizes.

---

# License

License selection pending initial implementation phase.
