# Changelog

All notable changes to macRun are documented in this file.

This changelog tracks compatibility milestones, architecture changes, phase
completions, and significant engineering events — not just code commits.

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

---

## [Unreleased] — Phase 4 Active

### In Progress
- Phase 4A: Native module substitution pipeline (better-sqlite3, node-pty,
  @vscode/sqlite3) — declarative rebuild registry targeting specific Electron ABIs
- Phase 4B: Electron runtime matrix expansion — capability-based version negotiation
  across Electron 28, 32, 38, 41+
- Phase 4C: Multi-view rendering reliability — BrowserView/WebContentsView compositor
  diagnostics for VSCode-class applications

---

## [Phase 3 Complete] — 2026-06-06

### Compatibility Milestones

#### Codex Desktop (Class D — Client-Server) — Functional
- Successfully executed Codex Desktop on Linux from an unmodified macOS app bundle
- Negotiated Electron 42.3.3 substrate (app targets Electron 42.1.0)
- Resolved macOS-only Rust CLI backend via `CODEX_CLI_PATH` environment substitution
- Compiled Linux-native `better-sqlite3@12.9.0` with V8 v13+ sandbox patches targeting
  Electron 42.3.3 ABI — required C++ patches to `macros.cpp`, `better_sqlite3.cpp`,
  and `helpers.cpp`
- Resolved rebranded Electron framework detection (`Codex Framework.framework`)
- Implemented `setBackgroundColor` and `setVibrancy` prototype-level intercepts on
  `BrowserWindow`, `WebContentsView`, and `BrowserView` to eliminate transparency
  rendering corruption
- Full UI hydration, dark/light mode rendering, and Rust backend connectivity confirmed

#### Claude Desktop (Class B — API Drift) — Functional
- Executed Claude Desktop on Linux via Electron 42 substrate
- Resolved `WebContentsView.prototype.setBackgroundColor` API drift between Electron
  versions
- Resolved `navigationHistory` API rename via normalization registry
- All core workflows validated: conversation management, file upload, clipboard, notifications

#### Cursor (Class C — IDE-Class) — Functional (stress test)
- Validated IDE-class native module pipeline: `@vscode/sqlite3`, `spdlog`
- Confirmed ESM boot-shim injection strategy for `"type": "module"` entry points
- Utility process environment variable propagation (`MACRUN_*` vars) confirmed working

#### Obsidian (Class A — Self-Contained) — Functional (stress test)
- Validated proxy-based native module stubbing for non-critical cosmetic modules
- Filesystem integration, plugins, hotkeys, and markdown rendering confirmed

### Architecture Milestones

- **Compatibility Spectrum codified**: Empirical validation revealed Class A–D
  application taxonomy. Documented in `docs/architecture/COMPATIBILITY_SPECTRUM.md`.
- **Runtime Substrate Negotiation**: Formalized the requirement for capability-directed
  Electron version selection. Documented in `docs/architecture/RUNTIME_NEGOTIATION.md`.
  Electron 42.3.3 substrate successfully negotiated for Codex (targeting 42.1.0).
- **Prototype-level shim intercepts**: `electron_adapter.cpp` updated to intercept
  `setBackgroundColor` and `setVibrancy` at `BrowserWindow`, `WebContentsView`, and
  `BrowserView` prototype level — resolves transparency rendering failures for
  macOS-targeting layout assumptions.
- **Phase 3F validation complete**: All four Class A–D application categories
  demonstrated functional execution.

### Phase Completions
- Phase 0 (Architecture and Governance) — Complete
- Phase 1 (Detection and Orchestration) — Complete
- Phase 2 (Runtime Adapter Layer) — Complete
- Phase 3 (Runtime Substitution Execution) — Complete

---

## [Phase 3 Begun] — 2026-05 (approx.)

### Architecture
- Governance model established: seven degradation categories, normalization registry,
  shim governance, semantic diagnostics
- `ARCHITECTURE_V6.md` published as canonical architecture specification
- Adapter boundaries established for all four substrate types (Electron, Darling, QEMU,
  WebKit)
- `ElectronAdapter` implementation begun — ASAR extraction, preload injection, shim
  infrastructure, process containment, XDG integration

### Compatibility
- Initial Obsidian and Claude Desktop validation (Phase 3F first wave)
- Blank-window diagnostic procedure developed and documented
- 16 semantic failure classifications established in `SEMANTIC_DIAGNOSTICS.md`

---

## [Phase 2 Complete] — 2026-04 (approx.)

### Architecture
- Orchestrator refactored to be substrate-independent
- All four adapter interfaces established behind clean boundaries:
  `IElectronAdapter`, `IDarlingAdapter`, `IQemuAdapter`, `IWebKitAdapter`
- No-orchestration policy enforced: adapters handle only local execution, not policy

---

## [Phase 1 Complete] — 2026-03 (approx.)

### Architecture
- 5-stage capability detection pipeline implemented: bundle inspection, framework
  probing, entitlements analysis, capability scoring, strategy resolution
- `compat-db` signature and policy schema established
- Tier classification routing implemented in orchestrator

---

## [Phase 0 Complete] — 2026-02 (approx.)

### Architecture
- Canonical architecture, execution model, substrate model, failure model, degradation
  model, shim governance, API normalization model, and semantic diagnostics model
  established
- Architectural invariants defined and documented
- Roadmap and governance structure published
