# MacRun Governance, Normalization & Degradation Model

This document outlines the governance and diagnostics framework that regulates application compatibility, runtime substitution, and graceful degradation in **MacRun**.

---

## 1. Degradation Governance Philosophy

> *"Degradation is a governed contract, not a runtime accident."*

To avoid the chaotic, untraceable failures common in compatibility layers, MacRun mandates that every compatibility compromise is explicit, diagnosable, bounded, reversible, and adapter-owned.

We define 7 strict **Degradation Categories**:

| Category | Definition | User-Visible Impact |
| --- | --- | --- |
| **Category 1: Transparent** | Runtime substitution with no difference from native Linux. | None. Full feature parity. |
| **Category 2: Shimmed** | Equivalent functionality using path, clipboard, or notification bridges. | Minimal. Minor system integration differences. |
| **Category 3: Functional** | Specific capabilities degraded (e.g., GPU disabled, CoreAnimation flattened). | Slower rendering, flattened animations. Core workflows intact. |
| **Category 4: Cosmetic** | Non-functional differences (e.g., dark mode state hardcoded to dark). | Dark mode/styling differences only. |
| **Category 5: Unsafe** | Bypasses safety checks (e.g., native Darwin `.node` module bypass). | Potential crash/instability. Explicit warning required. |
| **Category 6: Experimental** | Untested/unvalidated compatibility pathways. | Potential crash/failures. Opt-in via env vars only. |
| **Category 7: Hard Failure** | Launcher blocks execution because the app cannot run on the platform. | Launch blocked with an explicit error reason. |

---

## 2. Shim Governance & Preload Injection

In Tier 0 (Electron Substitution), normalizations and bridges are injected into the target application lifecycle via a structured preloading pipeline:

- **Preload Ordering**:
  1. **Platform Normalizer (`platform-normalizer.js`)**: Modifies `process.platform`, `process.arch`, and path mappings before any application code executes.
  2. **Environment Normalizer (`env-normalizer.js`)**: Maps XDG variables and normalizes environment inputs.
  3. **Diagnostics (`main-diag.js` & `renderer-diag.js`)**: Integrates hooks to catch unhandled errors and blank windows.
  4. **API Bridges (`clipboard-bridge.js`, `notification-bridge.js`, `shell-integration.js`)**: Bridges Electron-specific desktop integration calls to Linux-native DBus/XDG interfaces.
  5. **Suppressions (`disable-sparkle.js`, `disable-gpu.js`)**: Disables autoupdating and hardware acceleration when requested by compatibility policies.

---

## 3. Electron API Normalization Governance

To prevent runtime errors when applications request macOS-specific APIs, MacRun routes all Electron API patches through a centralized **Normalization Registry**:

- **Cosmetic No-Op (Class A)**: Returns safe mock values (e.g., `systemPreferences.isSwipeTrackingFromScrollEventsEnabled()` returns `false`).
- **Passive Intercept (Class B)**: Intercepts read requests and returns mock objects or redirected properties (e.g., system configuration reads).
- **Active Stub (Class C)**: Stubbing features with minimal fallbacks (e.g., dock modifications, touchbar APIs).
- **Forbidden/Security (Class D)**: APIs that could compromise host safety or cause silent failures are blocked with explicit warnings.

---

## 4. Semantic Diagnostics Infrastructure

MacRun incorporates a dedicated diagnostics layer to detect compatibility regressions during runtime substitution:

- **Observability Hooks**: Diagnostic scripts intercept the Main and Renderer processes to capture API access patterns, file-system mapping failures, and binary linkage mismatches.
- **16 Semantic Failure Classifications**: Errors are grouped into recognizable, actionable labels (e.g., `MISSING_DEPENDENCY`, `DARWIN_NATIVE_LOAD_FAILURE`, `NETWORK_RESTRICTION`, `IPC_TIMEOUT`).
- **Blank-Window Investigation Procedure**: A standardized, 9-step diagnostic sequence in the launcher detects and reports silent renderer crashes, missing assets, and Javascript preload errors, outputting a structured error card to guide debugging.
