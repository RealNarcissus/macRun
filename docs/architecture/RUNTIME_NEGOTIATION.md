# Runtime Substrate Negotiation

Canonical governance for Electron runtime version selection and substrate negotiation.

This document formalizes the discovery that Electron runtime version selection must become a governed orchestration problem rather than a static default.

Architecture references:
- SUBSTRATE_MODEL.md — Substrate classification, version governance
- SUBSTRATE_GOVERNANCE.md — Pinning policies, upgrade cadence
- COMPATIBILITY_SPECTRUM.md — Application architecture classes
- DEGRADATION_MODEL.md — Degradation categories

---

## Problem Statement

The platform currently defaults to Electron 28.x for all Tier 0 runtime substitution. Testing has revealed that modern AI IDE applications (Codex, Cursor, and future VS Code derivatives) are built against significantly newer Electron generations.

### Empirical Evidence

| App | Built Against (approx.) | Substituted With | Rendering Result |
|-----|------------------------|-------------------|------------------|
| Obsidian | Electron 25-28 | Electron 28.3.3 | Correct |
| Claude Desktop | Electron 30+ | Electron 28.3.3 | Correct (with normalization) |
| Cursor | Electron 28-32 | Electron 28.3.3 | Correct (with native modules) |
| Codex | Electron 32-41+ | Electron 28.3.3 | Rendering corruption |

### Root Cause Analysis

Electron 28 to Electron 41 is NOT an incremental change. It represents:
- Major Chromium evolution (Chromium 120 → 134+)
- BrowserView / WebContentsView evolution
- Compositor and layout engine evolution
- IPC architecture evolution
- GPU process architecture evolution
- Electron API surface evolution

The Codex rendering corruption is likely caused by:
- BrowserView/WebContents layout semantics mismatch
- Compositor/layout pipeline mismatch between app expectations and host runtime
- Electron API drift beyond what normalization can bridge
- Chromium feature evolution (CSS features, compositor behavior)

---

## Strategic Direction

The platform must NOT:
- Globally switch to latest Electron
- Abandon deterministic runtime governance
- Spoof Electron versions
- Apply heuristic version guessing

Instead, Electron runtime selection must become a governed orchestration problem.

### Architecture: Compatibility-Directed Runtime Negotiation

The platform should evolve toward:

```
macOS .app bundle
  -> capability detection
  -> runtime requirement analysis
  -> Electron runtime negotiation
  -> optimal Linux runtime substrate selection
```

This becomes: **runtime capability orchestration**.

---

## Future Runtime Matrix

The Electron version matrix (currently defined in `compat-db/manifests/electron/electron-versions.json`) should evolve to include multiple governed substrate versions:

| Version | Status | Primary Use Case |
|---------|--------|------------------|
| 28.x | Stable | Obsidian, older Electron apps |
| 32.x | Planned | Cursor-class VS Code derivatives |
| 38.x | Planned | Transition-era AI IDEs |
| 41.x | Planned | Modern AI IDE platforms (Codex, future) |

NOT "always latest Electron". Instead: **version-governed substrate orchestration**.

---

## Runtime Selection Policy

Future compat-db records should declare runtime requirements:

```json
{
  "runtime_policy": {
    "preferred": ["41", "38", "32", "28"],
    "minimum": "30",
    "validated": ["41"],
    "fallback": ["32", "28"]
  }
}
```

### Negotiation Algorithm (Conceptual)

1. Read app's declared Electron version from framework metadata
2. Map to nearest available cached runtime version
3. If exact match exists → use it (transparent substitution)
4. If nearest-higher exists → use it (minimal normalization expected)
5. If only lower exists → use it with normalization registry (API drift bridging)
6. Record substrate selection decision in diagnostics

### Selection Invariants

1. **Deterministic**: Same app bundle + same runtime matrix = same substrate selection. Always.
2. **Diagnosable**: Every selection decision produces `[MACRUN:SUBSTRATE]` diagnostic output
3. **Explicit**: Selection logic is registry-driven, never heuristic
4. **Governed**: Runtime versions are pinned in `compat-db/manifests/`, never resolved dynamically from upstream
5. **Bounded**: The runtime matrix is finite and curated, never unbounded
6. **Reversible**: User can force a specific runtime via `MACRUN_ELECTRON_VERSION=28`

---

## Immediate Experimental Direction

The critical next experiment is:

> Re-test Codex under Electron 41 runtime substrate.

This experiment answers a fundamental question:

> Does a semantically closer runtime substrate naturally eliminate compatibility drift?

Expected outcomes if the hypothesis holds:
- Major layout/compositor improvement
- Fewer normalization requirements
- Fewer BrowserView mismatches
- Reduced semantic drift
- Healthier compatibility architecture

If validated, runtime negotiation becomes a core architecture pillar.

---

## Integration with Existing Infrastructure

### Detection Pipeline
The detector must evolve to extract the app's expected Electron version from:
- `Electron Framework.framework` version metadata
- `package.json` dependencies
- Info.plist embedded version strings

### Adapter Layer
The ElectronAdapter must accept a runtime version parameter from the orchestrator rather than hardcoding a default.

### Diagnostics
New diagnostic domain: `[MACRUN:SUBSTRATE]` for runtime selection events:
```
[MACRUN:SUBSTRATE] app=com.openai.codex detected_version=41.0.0 selected_runtime=41.2.1 strategy=nearest-match
[MACRUN:SUBSTRATE] app=md.obsidian detected_version=28.0.0 selected_runtime=28.3.3 strategy=exact-match
```

---

## New Diagnostic Frontier

The project now requires diagnostics for a domain that did not previously exist:

### Renderer Layout and Compositor Diagnostics

Current diagnostics cover: semantic failures, IPC failures, module resolution, preload failures, hydration failures, Electron API mismatches.

New required domains:
- BrowserView layout semantics mismatch detection
- Compositor pipeline diagnostics
- Layout tree observation
- GPU process fallback detection
- Resize propagation tracing
- Chromium compositor capability analysis
- View hierarchy instrumentation

These compose the next compatibility frontier and are required to diagnose substrate-drift-related rendering failures.

---

## Prohibited Behaviors

- **Global Electron version spoofing**: Faking `process.versions.electron` — NEVER
- **Uncontrolled runtime drift**: Using npm-resolved latest Electron — NEVER
- **Heuristic version guessing**: "This looks modern so use 41" — NEVER
- **App-specific version hardcoding**: `if (app === 'codex') use 41` in orchestrator — NEVER
- **Rolling runtime updates**: Automatically pulling newer Electron — NEVER

All version selection must be registry-driven and compat-db-governed.

---

*Last updated: 2026-06-06. Authoritative for all Electron runtime substrate selection decisions.*
