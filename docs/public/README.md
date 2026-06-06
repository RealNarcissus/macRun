# macRun Documentation Index

Welcome to the public developer documentation for the **macRun** compatibility platform. This directory contains synthesized architectural, governance, and tier details for the developer preview release.

## Available Documents

### 1. **[Architecture Overview](ARCHITECTURE.md)**
Deep dive into the hybrid execution model. It covers:
- The execution tier selection strategy.
- Tier 0 (Electron runtime substitution and preload normalization).
- Tier 1-2 (Darling runtime substrate, adapter boundaries, and POSIX integration).
- Tier 3-4 (ARM64 binary translation and VM-assisted application streaming).
- Substrate ownership boundaries and the adapterized architecture.

### 2. **[Governance & Degradation](GOVERNANCE.md)**
Details the strict systems discipline applied to application compatibility:
- **Philosophy**: "Degradation is a governed contract, not a runtime accident."
- **Degradation Categories**: Category 1 (Transparent Substitution) through Category 7 (Hard Failure).
- **Shim Governance**: Normalization rules, preload injection ordering, and XDG/clipboard/notification bridges.
- **Normalization Registry**: Centralized API mapping rules for Electron to maintain stability and prevent silent failures.
- **Semantic Diagnostics**: 16 failure classifications and the blank-window diagnostic procedure.

### 3. **[Limitations & Tiers](LIMITATIONS.md)**
Honest status and constraints report:
- **Current Status**: Verified functional Tier 0 targets (Claude Desktop, Obsidian) and current infrastructure/planned phase tracking.
- **Explicit Non-Goals**: SwiftUI, Metal, and general AppKit limitations.
- **Tiers Table**: Comprehensive overview of Tier 0 through Tier 4B.

### 4. **[Project Overview](project_overview.md)**
The original internal project summary detailing our utility-over-purity philosophy, VM-assisted execution pillars, codebase structure, and early design priorities.

---

## Codebase Map

- **`platform/`**: Launcher (`macrun`), capability detector, and adapter boundaries.
  - `platform/macrun/`: Core orchestration and strategy resolution.
  - `platform/detector/`: 5-stage capability detection engine.
  - `platform/adapters/`: Interface contracts (`IDarlingAdapter`, `IElectronAdapter`, etc.).
- **`runtime/`**: Execution backends and normalizations.
  - `runtime/shims/`: JavaScript normalizations, diagnostics, and desktop API bridges.
  - `runtime/darling/`: CLI/POSIX compatibility.
  - `runtime/cocoa/`: Lightweight Cocoa/AppKit proxy.
- **`vm/`**: Guest macOS virtualization bridge (Tier 4B).
- **`host/`**: Linux-native windowing and desktop integration (Wayland, clipboard, audio).
- **`compat-db/`**: Compatibility database containing signatures, reports, and execution policies.
- **`tooling/`**: DMG extraction and bundle inspection utilities.
- **`tests/`**: Unit, integration, and compatibility validation suites.
