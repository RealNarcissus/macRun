# macRun — Project Overview

macRun is a governed runtime compatibility platform for macOS applications on Linux.

It is not a macOS reimplementation. It is not a Wine-style translation layer. It is a
platform that classifies macOS applications by their underlying runtime architecture and
applies the minimum-complexity compatibility strategy to each — preferring runtime
substitution over emulation wherever the application's foundations allow it.

---

## What macRun Does

Modern developer tooling increasingly ships macOS-first. AI assistants, coding
environments, and productivity tools built on cross-platform runtimes like Electron end
up Linux-inaccessible — not because the technology is fundamentally incompatible, but
because no governed compatibility infrastructure exists to bridge the gap.

Community workarounds exist: some maintainers manually repackage macOS Electron apps for
Linux, patching and republishing each new release by hand. When Claude Desktop or Codex
ships an update, users on those forks wait.

macRun takes a different approach. Rather than repackaging, it runs the original
unmodified app bundle directly on Linux through a governed runtime substitution and
compatibility layer. No repackaging lag. No third-party distribution chain.

---

## Current State

**Phase 3 complete. Phase 4 is the active development frontier.**

Tier 0 (Electron Runtime Substitution) is functional and validated:

| Application    | Class | Status     | Notes                              |
|----------------|-------|------------|------------------------------------|
| Claude Desktop | B     | Functional | API drift resolved via normalization |
| Codex Desktop  | D     | Functional | Backend + SQLite substitution required |
| Obsidian       | A     | Functional | Stress test — official Linux build exists |
| Cursor         | C     | Functional | Stress test — official Linux build exists |

Tiers 1–4 (Darling, QEMU, VM) have established adapter boundaries but are not yet
implemented. See [ROADMAP.md](../../ROADMAP.md) for the full phase plan.

---

## Design Principles

### Utility Over Purity

The objective is usable application compatibility, not complete operating-system
emulation. If a native Linux runtime swap achieves the same user outcome as a full
compatibility layer, runtime substitution is preferred.

### Explicit Degradation Over Silent Failure

Every compatibility compromise is explicit, diagnosable, bounded, reversible, and
adapter-owned. macRun defines seven degradation categories from Transparent Substitution
to Hard Failure. No compatibility decision is implicit.

### Asymmetric Strategy by Application Category

Electron apps are not native Cocoa apps. They require different strategies. The platform
classifies applications into execution tiers and compatibility classes before selecting
an approach. See [COMPATIBILITY_SPECTRUM.md](../architecture/COMPATIBILITY_SPECTRUM.md)
for the Class A–D model.

### Linux Is the Primary Environment

macOS runtimes exist only as execution substrates. The platform does not recreate Finder,
Dock, or the macOS desktop experience. Applications should feel native to Linux, not
foreign.

---

## Architecture

The platform classifies applications into execution tiers and selects the
lowest-complexity viable runtime strategy.

```
macOS .app bundle
    ↓
5-Stage Capability Detection Pipeline
  1. Bundle inspection (Info.plist, Mach-O headers)
  2. Framework and dependency probing
  3. Entitlements analysis
  4. Capability scoring against compat-db
  5. Execution tier and compatibility class resolution
    ↓
Adapter dispatch
    ↓
Native Linux integration (Wayland, XDG, clipboard, notifications)
```

| Tier    | Strategy                                    | Status         |
|---------|---------------------------------------------|----------------|
| Tier 0  | Runtime substitution (Electron/Tauri/Wails) | Functional     |
| Tier 1  | CLI compatibility via Darling               | Phase 5 future |
| Tier 2  | Lightweight Cocoa compatibility             | Phase 6 future |
| Tier 3  | ARM64 translation                           | Phase 7 future |
| Tier 4B | VM-assisted application streaming           | Phase 8 future |

---

## Repository Structure

```
docs/                     Architecture, design, protocols, compatibility docs
├── architecture/         Canonical architecture specification (authoritative)
├── design/               Per-subsystem design documents
├── protocols/            Interface contract definitions and wire formats
├── compatibility/        Target application profiles and degradation specs
├── guides/               Application-specific launch guides
└── diagrams/             Architecture and data flow diagrams

compat-db/                Compatibility metadata and policies
├── signatures/           Application and framework detection fingerprints
├── policies/             Execution strategy mappings and degradation rules
├── reports/              Validated compatibility reports
└── manifests/            Runtime version mappings and dependency declarations

platform/                 Application orchestration and capability detection
├── detector/             Five-stage capability detection pipeline
├── adapters/             Substrate adapter implementations
└── common/               Shared platform types

runtime/                  Execution backends (Tier 0 functional)
├── shims/                Tier 0: Electron/Tauri runtime substitution and normalization
├── darling/              Tier 1-2: Mach-O loader skeleton
├── cocoa/                Tier 2: Lightweight Cocoa skeleton
└── arm64/                Tier 3: ARM64 translation skeleton

vm/                       macOS guest integration (Tier 4B — future)
host/                     Linux-side windowing and UX integration
tooling/                  macrun-cli, diagnostic tools, bundle inspector
tests/                    Unit, integration, compatibility, and performance tests
```

---

## Documentation

| Document                                                  | Status  | Purpose                                              |
|-----------------------------------------------------------|---------|------------------------------------------------------|
| [ROADMAP.md](../../ROADMAP.md)                            | Current | Phase status, exit criteria, strategic position      |
| [ARCHITECTURE.md](ARCHITECTURE.md)                        | Current | Execution tier model, adapter boundaries             |
| [GOVERNANCE.md](GOVERNANCE.md)                            | Current | Degradation categories, shim governance, diagnostics |
| [LIMITATIONS.md](LIMITATIONS.md)                         | Current | Validated apps, active limitations, non-goals        |
| [docs/architecture/ARCHITECTURE_V6.md](../architecture/ARCHITECTURE_V6.md) | Current | Full canonical architecture specification |
| [docs/architecture/COMPATIBILITY_SPECTRUM.md](../architecture/COMPATIBILITY_SPECTRUM.md) | Current | Class A–D application classification model |
| [docs/architecture/RUNTIME_NEGOTIATION.md](../architecture/RUNTIME_NEGOTIATION.md) | Current | Substrate version selection governance |
| [docs/architecture/DEGRADATION_MODEL.md](../architecture/DEGRADATION_MODEL.md) | Current | Degradation categories and escalation logic |
| [docs/guides/codex/README.md](../guides/codex/README.md) | Current | Step-by-step guide: Codex Desktop on Linux |

---

## Contributing

Phase 4 is the active development frontier. The highest-impact contribution areas are:

- **Phase 4A (Active)**: Native module rebuild pipelines (better-sqlite3, node-pty,
  @vscode/sqlite3), module resolution registry, ABI verification tooling
- **Phase 4B/4C (Next)**: Electron version capability negotiation, BrowserView
  rendering reliability, renderer diagnostic tooling
- **Always useful**: Compatibility database entries, validation reports, Mach-O analysis
  tooling improvements

See [CONTRIBUTING.md](../../CONTRIBUTING.md) for the full contribution guide.

---

## License

MIT. See [LICENSE](../../LICENSE).
