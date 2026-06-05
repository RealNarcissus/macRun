# Electron API Normalization Governance

Canonical governance for all Electron API normalization behavior.

This document is authoritative for every Electron API patch, shim, or compatibility adjustment. No normalization may exist outside the rules defined here.

Architecture references:
- [ARCHITECTURE_V6.md](ARCHITECTURE_V6.md) — Invariant 4 (user experience over purity), Invariant 5 (unsupported must be explicit)
- [DEGRADATION_MODEL.md](DEGRADATION_MODEL.md) — Degradation categories, prohibited behaviors
- [SHIM_GOVERNANCE.md](SHIM_GOVERNANCE.md) — Shim lifecycle, activation rules, mutation boundaries
- [SEMANTIC_DIAGNOSTICS.md](SEMANTIC_DIAGNOSTICS.md) — Renderer observability infrastructure
- [SUBSTRATE_MODEL.md](SUBSTRATE_MODEL.md) — Adapter isolation, substrate boundaries

---

# Philosophy

> *"Normalization is a governed extension of the runtime, not a compatibility hack."*

Every normalization must be:
- **Registry-owned** — registered in `electron-normalization-registry.js`, never inline
- **Version-scoped** — explicitly targets specific Electron versions
- **Diagnostic-visible** — every normalization activation produces `[MACRUN:NORMALIZATION]` output
- **Risk-classified** — assigned to a risk class that governs allowed behavior
- **Adapter-owned** — never leaks into orchestration, detection, or compat-db policy

---

# Normalization Risk Classes

## Class A: Cosmetic No-Op

**Definition**: An API that exists on macOS Electron but has no semantic effect on Linux. Stubbing it to a no-op has zero behavioral impact.

| Property | Value |
|----------|-------|
| Behavioral impact | None. API call is silently consumed. |
| User-visible effect | None. |
| Allowed mutations | Empty function body, `return undefined`. |
| Degradation | `transparent` — no degradation escalation. |
| Diagnostics | `[MACRUN:NORMALIZATION] Class-A setBackgroundColor no-op (Electron 28.x does not expose this)`. |
| Experimental required? | No. |
| Key constraint | Must NOT return a value that downstream code depends on unless that value would also be meaningless. |

**Examples**: `WebContentsView.prototype.setBackgroundColor` — a visual hint with no rendering impact on Linux. `BrowserWindow.setVibrancy` — macOS-only window effect.

---

## Class B: Safe Compatibility Alias

**Definition**: An API whose interface changed between Electron versions but the semantics are identical. Forwarding from the old signature to the new equivalent.

| Property | Value |
|----------|-------|
| Behavioral impact | None. Identical behavior via different API surface. |
| User-visible effect | None. |
| Allowed mutations | Method rename forwarding, argument reordering/filtering, default value insertion. |
| Degradation | `shimmed` — minor semantic adjustment. |
| Diagnostics | `[MACRUN:NORMALIZATION] Class-B <old_api> → <new_api> alias (Electron <version> compatibility)`. |
| Experimental required? | No. |
| Key constraint | Must produce identical behavior. Must NOT add or remove functionality. |

**Examples**: `BrowserWindow.webContents.id` moved to `BrowserWindow.id` in some versions. `dialog.showOpenDialog` argument shape changes.

---

## Class C: Dangerous Behavioral Emulation

**Definition**: An API whose behavior cannot be precisely replicated on Linux, but a functional approximation prevents crashes. This is governed degradation territory.

| Property | Value |
|----------|-------|
| Behavioral impact | Medium. Approximate behavior, not exact. |
| User-visible effect | May differ from macOS behavior. Explicit warning required. |
| Allowed mutations | Stateful emulation with explicit diagnostics, IPC bridge emulation, storage fallback. |
| Degradation | `functional` or `experimental` — depends on fidelity. |
| Diagnostics | `[MACRUN:NORMALIZATION] Class-C <api> emulated (Electron <version> mismatch). Behavior may differ from macOS.`. |
| Experimental required? | Yes — `MACRUN_EXPERIMENTAL_*` gate required. |
| Key constraint | Must degrade explicitly. Must NOT silently approximate. Must report the gap to user. |

**Examples**: `systemPreferences.getUserDefault` with real macOS plist reading → fallback to env vars. `app.setUserActivity` emulation.

---

## Class D: Forbidden

**Definition**: Mutations that are NEVER allowed under any circumstances. These are architectural violations, not compatibility tools.

| Property | Value |
|----------|-------|
| Status | FORBIDDEN PERMANENTLY. |
| Examples | Global `process.platform` spoofing to `'darwin'`. Silent IPC polyfills that fake main↔renderer communication. Hidden auth/session bypasses. App identity spoofing. Renderer semantic rewriting. Orchestration-owned normalization. App-specific `if (bundleId === 'claude')` branching. |
| Consequences | Any Class-D mutation is grounds for architectural rejection. |

---

# Normalization Registry Structure

All normalizations are registered in `runtime/shims/electron-normalization-registry.js`. The registry is a structured array with the following schema per entry:

```javascript
{
    // Unique identifier for this normalization
    id: 'setBackgroundColor-noop',

    // API target: the full path to the API being normalized
    target: 'WebContentsView.prototype.setBackgroundColor',

    // Electron versions this normalization targets (semver ranges)
    electron_version_min: '28.0.0',
    electron_version_max: '28.99.99',

    // Risk classification: 'A', 'B', 'C' (D is forbidden — never registered)
    risk_class: 'A',

    // Degradation category per DEGRADATION_MODEL.md
    degradation_category: 'transparent',

    // Human-readable description
    description: 'setBackgroundColor is not exposed in Electron 28 Linux. No-op stub.',

    // Whether experimental mode (MACRUN_EXPERIMENTAL_*) is required
    experimental_required: false,

    // The normalization function: receives the Electron module and returns
    // true if the normalization was successfully applied, false if skipped
    // (e.g., because the API already exists or the version doesn't match).
    apply: function(electron) { ... }
}
```

---

# Normalization Diagnostics Format

Every normalization activation produces a structured diagnostic:

```
[MACRUN:NORMALIZATION] class=<A|B|C> api=<target> version=<electron_version> degradation=<category> reason=<description>
```

Example:
```
[MACRUN:NORMALIZATION] class=A api=WebContentsView.prototype.setBackgroundColor version=28.3.3 degradation=transparent reason=setBackgroundColor is not exposed in Electron 28 Linux. No-op stub.
```

Normalization MUST NEVER be silent. Every activation — even Class-A no-ops — must produce a diagnostic.

---

# Integration with Degradation Governance

| Normalization Risk Class | Degradation Category | Confidence | Experimental Required? |
|---|---|---|---|
| Class A (Cosmetic No-Op) | `transparent` | `verified` | No |
| Class B (Safe Alias) | `shimmed` | `functional` | No |
| Class C (Dangerous Emulation) | `experimental` | `experimental` | Yes |
| Class D (Forbidden) | N/A — never registered | N/A | N/A |

Normalization events are surfaced through the adapter's `record_degradation()` mechanism. The degradation model in DEGRADATION_MODEL.md already handles escalation — normalization feeds into the existing categories without creating new ones.

---

# Version-Scoping Rules

1. Every normalization MUST declare a version range (min/max)
2. The registry MUST check the runtime Electron version before applying any normalization
3. Normalizations for unsupported version ranges MUST be skipped with a diagnostic
4. When Electron is upgraded (new cached runtime), registry entries for old versions automatically become inactive
5. A normalization targeting "all versions" is FORBIDDEN — must specify explicit range

---

# Prohibited Normalization Behaviors

The following are FORBIDDEN under ALL circumstances:

- **Unregistered normalization**: Any API patching outside the registry
- **Silent normalization**: Any normalization without `[MACRUN:NORMALIZATION]` diagnostic
- **Version-blind normalization**: Normalizing an API without checking the Electron version
- **Orchestration-owned normalization**: Normalization logic in macrun, detector, or compat-db
- **App-specific normalization**: `if (app.getName() === 'Claude') { ... }` — NEVER
- **Global spoofing**: Making `process.platform === 'darwin'` or faking Electron version
- **Hidden retry**: Normalization that silently retries failed operations
- **Invisible state**: Normalization that maintains hidden mutable state across calls

---

# Adding a New Normalization

To add a governed normalization:

1. **Classify the risk** (A/B/C — D is never registered)
2. **Register in the registry** with version range, risk class, and apply function
3. **Write the apply function** that checks API existence and patches deterministically
4. **Add to diagnostics** — every registration auto-produces `[MACRUN:NORMALIZATION]`
5. **Update degradation metadata** if risk class triggers escalation
6. **Add a regression test** validating activation, version scoping, and diagnostic output
7. **Document in the registry** with human-readable description

---

# Relationship to Existing Subsystems

- **SHIM_GOVERNANCE.md**: The normalization registry shim is Category B (conditional, `MACRUN_SHIM_NORMALIZATION=1`). It has the same mutation boundary rules as other shims.
- **DEGRADATION_MODEL.md**: Normalization feeds into the existing degradation categories. No new categories are created.
- **SEMANTIC_DIAGNOSTICS.md**: Normalization diagnostics complement renderer diagnostics. The `renderer-diag.js` observe-and-log pattern is the model.
- **SUBSTRATE_MODEL.md**: Normalization is adapter-owned. The ElectronAdapter injects the normalization preload and tracks degradation.
- **COMPAT_DB.md**: Apps using normalization should have `active_shims: ["normalization"]` and appropriate degradation metadata on their records.

---

*Last updated: 2026-06-05. Authoritative for all Electron API normalization decisions.*
