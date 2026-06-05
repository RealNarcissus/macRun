# Platform Layer

Application orchestration and capability detection.

Architecture reference: docs/architecture/ARCHITECTURE_V6.md
  - "Execution Strategy overview" diagram
  - "Capability Detection Engine" section
  - "macrun Contract"
  - "Detection Pipeline" stages 1-5

## Subsystems

### macrun — Application Orchestration
Accepts .app/.dmg/Mach-O binary, outputs execution strategy and runtime configuration.
Never implements rendering, translation, or compositor logic.

### detector — Capability Detection Engine
Five-stage detection pipeline:
1. Bundle analysis (Info.plist, Mach-O headers, entitlements)
2. Framework fingerprinting (Electron, Tauri, AppKit, SwiftUI, Metal)
3. Architecture analysis (x86-64, ARM64-only, universal binary)
4. Capability scoring (weighted risk assessment across dimensions)
5. Execution strategy resolution (preferred tier, fallback, VM requirement)

### common — Shared Types
Platform-layer types and utilities shared between macrun and detector.
