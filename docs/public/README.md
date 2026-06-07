# macRun Documentation Index

Public developer documentation for the **macRun** compatibility platform. This
directory contains synthesized architectural, governance, and compatibility details for
the developer preview release.

---

## Document Status Key

| Status | Meaning |
|--------|---------|
| ✅ Current | Reflects actual project state as of June 2026 |
| 🔄 Under Revision | Content is being updated |
| 📌 Phase-specific | Applies to a specific development phase |

---

## Available Documents

### 1. [Architecture Overview](ARCHITECTURE.md) ✅ Current

The execution model, tier selection strategy, and adapter boundaries:
- Tier 0 (Electron runtime substitution) — **functional, with Class A–D spectrum**
- Tier 1–2 (Darling substrate) — **skeleton adapters, Phase 5–6 future**
- Tier 3–4 (ARM64 translation, VM streaming) — **Phase 7–8 future**
- Adapter implementation status notes
- Open architecture questions (VM lifecycle design)

> Requires the full canonical spec for deep implementation detail:
> [docs/architecture/ARCHITECTURE_V6.md](../architecture/ARCHITECTURE_V6.md)

---

### 2. [Governance & Degradation](GOVERNANCE.md) ✅ Current

The strict systems discipline applied to compatibility decisions:
- **7 Degradation Categories**: Transparent through Hard Failure
- **Shim Governance**: Preload injection ordering and normalization rules
- **Normalization Registry**: Centralized API mapping (Classes A–D)
- **Semantic Diagnostics**: 16 failure classifications, blank-window procedure

> Full degradation model:
> [docs/architecture/DEGRADATION_MODEL.md](../architecture/DEGRADATION_MODEL.md)

---

### 3. [Limitations & Compatibility Matrix](LIMITATIONS.md) ✅ Current

Honest status report on what works, what is limited, and what is out of scope:
- **Validated Apps**: Claude Desktop (Class B), Codex (Class D), Obsidian (Class A),
  Cursor (Class C) — with per-app status and required steps
- **Phase 4 Active Limitations**: Native module ABI, Electron version gaps,
  multi-view rendering
- **Explicit Non-Goals**: SwiftUI, Metal, AppKit completeness, kernel substrate

---

### 4. [Project Overview](project_overview.md) ✅ Current

The platform summary — updated to reflect an early-stage but functional system:
- Problem statement and approach vs. community repackaging
- Current state table (Phase 3 complete, Phase 4 active)
- Design principles (utility over purity, explicit degradation, asymmetric strategy)
- Repository structure with tier status annotations
- Full documentation index with status indicators
- Contributing focus (Phase 4A/4B/4C)

---

## Architecture Specification Documents

These live in [docs/architecture/](../architecture/) and are the authoritative
technical references. Public documents above are synthesized summaries.

| Document | Status | Description |
|----------|--------|-------------|
| [ARCHITECTURE_V6.md](../architecture/ARCHITECTURE_V6.md) | ✅ Current | Full canonical architecture specification |
| [COMPATIBILITY_SPECTRUM.md](../architecture/COMPATIBILITY_SPECTRUM.md) | ✅ Current | Class A–D Electron application classification |
| [RUNTIME_NEGOTIATION.md](../architecture/RUNTIME_NEGOTIATION.md) | ✅ Current | Substrate version selection governance (Phase 4B) |
| [DEGRADATION_MODEL.md](../architecture/DEGRADATION_MODEL.md) | ✅ Current | Degradation categories, escalation, confidence model |
| [SHIM_GOVERNANCE.md](../architecture/SHIM_GOVERNANCE.md) | ✅ Current | Shim execution rules and preload injection protocol |
| [ELECTRON_API_NORMALIZATION.md](../architecture/ELECTRON_API_NORMALIZATION.md) | ✅ Current | API normalization registry design |
| [SEMANTIC_DIAGNOSTICS.md](../architecture/SEMANTIC_DIAGNOSTICS.md) | ✅ Current | 16 failure classifications, diagnostic procedures |
| [SUBSTRATE_MODEL.md](../architecture/SUBSTRATE_MODEL.md) | ✅ Current | Substrate classification and adapter governance |
| [FAILURE_MODEL.md](../architecture/FAILURE_MODEL.md) | ✅ Current | Failure categories and recovery model |

---

## Application Launch Guides

| Guide | Status | Description |
|-------|--------|-------------|
| [Codex Desktop](../guides/codex/README.md) | ✅ Current | Full step-by-step: SQLite compilation, CLI substitution, launch |
| [Claude Desktop](../guides/claude/README.md) | ✅ Current | Step-by-step: extract, configure, launch via Electron 42 |

---

## Codebase Map

- **`platform/`**: Launcher (`macrun`), capability detector, adapter boundaries
  - `platform/detector/`: 5-stage capability detection engine
  - `platform/adapters/`: Interface contracts and `ElectronAdapter` implementation
- **`runtime/`**: Execution backends and normalizations
  - `runtime/shims/`: JavaScript normalizations, diagnostics, and desktop API bridges
  - `runtime/darling/`: CLI/POSIX compatibility skeleton
  - `runtime/cocoa/`: Lightweight Cocoa/AppKit proxy skeleton
- **`vm/`**: Guest macOS virtualization bridge (Tier 4B — future)
- **`host/`**: Linux-native windowing and desktop integration (Wayland, clipboard, audio)
- **`compat-db/`**: Compatibility database — signatures, reports, execution policies
- **`tooling/`**: macrun-cli, DMG extraction, and bundle inspection utilities
- **`tests/`**: Unit, integration, and compatibility validation suites
