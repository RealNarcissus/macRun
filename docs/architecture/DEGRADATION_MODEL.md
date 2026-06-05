# Degradation Governance Model

Canonical degradation semantics for the macOS Runtime Platform.

This document is authoritative for all degradation decisions across every execution tier. No subsystem may introduce ungoverned degradation behavior.

Architecture references:
- [ARCHITECTURE_V6.md](ARCHITECTURE_V6.md) — Invariants 4 (user experience over purity) and 5 (unsupported must be explicit)
- [FAILURE_MODEL.md](FAILURE_MODEL.md) — Graceful degradation philosophy, failure isolation boundaries
- [SUBSTRATE_MODEL.md](SUBSTRATE_MODEL.md) — Adapter ownership of substrate-specific behavior
- [EXECUTION_MODEL.md](EXECUTION_MODEL.md) — Tier classification and backend selection

---

# Philosophy

> *"Degradation is a governed contract, not a runtime accident."*

Every degradation path must be:
- **Explicit** — never triggered implicitly by runtime state
- **Diagnosable** — always visible to the user, logs, and compat-db
- **Bounded** — degrades a specific capability category, never the entire execution
- **Reversible** — user can see working degraded behavior and choose to escalate (VM)
- **Adatper-owned** — never moved into orchestration or detection

# Degradation Categories

## Category 1: Transparent Substitution

**Definition**: The macOS application runs identically on Linux through runtime replacement. No user-perceptible difference.

| Property          | Value                                                              |
|-------------------|--------------------------------------------------------------------|
| User-visible      | No difference from native Linux application                       |
| Runtime guarantees | Full feature parity with macOS behavior                           |
| Allowed behaviors | Native Electron/Tauri runtime, WebKitGTK rendering, XDG paths     |
| Forbidden         | Any shim activation, any API patching, any env normalization      |
| Diagnostics       | `[INFO] Transparent substitution: <app> running natively on Linux` |
| Compat-db tag     | `compatibility_tier: verified` with `degradation: none`            |
| Escalation        | N/A — no degradation                                              |
| Safety            | Safe. No runtime mutation.                                        |

**Examples**: VS Code running as Linux binary, Tauri apps with WebKitGTK shell, Electron apps with zero patching needed.

---

## Category 2: Shimmed Compatibility

**Definition**: The app runs through runtime substitution with a bounded set of activation-gated shims. Behavior is functionally equivalent but some platform APIs are remapped.

| Property          | Value                                                                           |
|-------------------|----------------------------------------------------------------------------------|
| User-visible      | App works correctly. Some platform behaviors differ (e.g., notifications route differently) |
| Runtime guarantees | All features work; platform APIs are redirected, not removed                     |
| Allowed behaviors | Path mapping (XDG), notification bridging, clipboard bridging, shell integration, updater suppression |
| Forbidden         | API stubbing with no functional fallback, GPU disablement unless necessary, dlopen patching |
| Diagnostics       | `[WARN] Shimmed compatibility: <list of active shims>`                            |
| Compat-db tag     | `compatibility_tier: verified` with `degradation: shimmed` and `active_shims: [...]` |
| Escalation        | Shim count > 5 → review for VM-assisted execution                                |
| Safety            | Safe. Each shim activation is explicit and logged.                              |

**Examples**: Obsidian with path-mapper + shell-integration, Claude Desktop with XDG + notification + clipboard bridges.

---

## Category 3: Functional Degradation

**Definition**: A specific capability is degraded (e.g., GPU acceleration disabled, CoreAnimation flattened) but all core workflows remain functional.

| Property          | Value                                                                              |
|-------------------|------------------------------------------------------------------------------------|
| User-visible      | App works but may render slower, lack animations, or have visual differences       |
| Runtime guarantees | Core workflows functional; degraded capability is explicitly documented           |
| Allowed behaviors | GPU disablement, CoreAnimation CPU flattening, software rendering fallback         |
| Forbidden         | Silent capability removal without diagnostics, auto-activation of degradation     |
| Diagnostics       | `[WARN] Functional degradation: <capability> degraded due to <reason>`             |
| Compat-db tag     | `compatibility_tier: functional` with `degradation: functional` and `degraded_capabilities: [...]` |
| Escalation        | If the degraded capability is user-critical → recommend VM-assisted execution      |
| Safety            | Safe. Degradation is isolated to the declared capability category.                |

**Examples**: Apps with `--disable-gpu` flag, Metal → software renderer, CoreAnimation → CPU static draw.

---

## Category 4: Cosmetic Degradation

**Definition**: Visual presentation or non-functional behaviors differ from macOS (dark mode, system font, window chrome) but no capability is lost.

| Property          | Value                                                                                  |
|-------------------|----------------------------------------------------------------------------------------|
| User-visible      | Looks slightly different, feels slightly different. No functionality lost.             |
| Runtime guarantees | All capabilities work; visual presentation is approximated                            |
| Allowed behaviors | Default dark mode, system font substitution, window decoration differences             |
| Forbidden         | Functional capability removal disguised as cosmetic difference                        |
| Diagnostics       | `[INFO] Cosmetic degradation: <difference> (no functional impact)`                     |
| Compat-db tag     | `compatibility_tier: verified` with `degradation: cosmetic` and `cosmetic_differences: [...]` |
| Escalation        | N/A — cosmetic differences are acceptable for Tier 0-2                                |
| Safety            | Safe. No functional mutation.                                                         |

**Examples**: `systemPreferences.isDarkMode()` returns `false` instead of reading macOS system setting.

---

## Category 5: Unsafe Compatibility

**Definition**: The platform bypasses a safety check that would normally block execution. The app MAY work but behavior is not guaranteed. This is a governed escape hatch, not a compatibility strategy.

| Property          | Value                                                                                     |
|-------------------|-------------------------------------------------------------------------------------------|
| User-visible      | App may crash, behave unpredictably, or corrupt state. Explicit warning REQUIRED.         |
| Runtime guarantees | NONE. No guarantees of any kind.                                                          |
| Allowed behaviors | Darwin-native .node module bypass (MACRUN_ALLOW_DARWIN_NATIVE=1), any future unsafe bypass |
| Forbidden         | Auto-activation, silent bypass, absence of user-facing warning, bypass without logging    |
| Diagnostics       | `[WARN] *** UNSAFE COMPATIBILITY MODE *** <reason>. Behavior is not guaranteed.`          |
| Compat-db tag     | `compatibility_tier: functional` with `degradation: unsafe` and `unsafe_bypasses: [...]`   |
| Escalation        | User MUST be informed that execution is unsafe. VM-assisted execution recommended.        |
| Safety            | UNSAFE. Opt-in only via explicit environment variable. Never default.                    |

**Governance rules for `MACRUN_ALLOW_DARWIN_NATIVE`**:
1. Must NEVER be set in production orchestration
2. Must ALWAYS produce a visible WARN-level diagnostic listing each bypassed module
3. Must ALWAYS set `degradation: unsafe` in compat-db metadata
4. Must NEVER be implicitly activated
5. Must ALWAYS be documented in compat-db with known risks
6. Adapter-level bypass only — never in orchestration

---

## Category 6: Experimental Compatibility

**Definition**: A compatibility pathway that has not been validated. May work, may crash, may produce undefined behavior. Governed separately from unsafe because it targets NEW pathways rather than bypassing existing safety checks.

| Property          | Value                                                                                         |
|-------------------|-----------------------------------------------------------------------------------------------|
| User-visible      | Warning required. May fail at any point.                                                      |
| Runtime guarantees | NONE. Experimental only.                                                                       |
| Allowed behaviors | Any env-var-gated experimental behavior (MACRUN_EXPERIMENTAL_*), new shim types, untested paths |
| Forbidden         | Auto-activation, production defaults, hiding behind stable execution                          |
| Diagnostics       | `[WARN] *** EXPERIMENTAL COMPATIBILITY *** <feature>. This pathway has not been validated.`    |
| Compat-db tag     | `compatibility_tier: experimental` with `degradation: experimental`                           |
| Escalation        | Must graduate to a governed degradation category or be removed                               |
| Safety            | UNKNOWN. Opt-in only. Must produce warnings.                                                 |

**Governance rules for experimental modes**:
1. All experimental env vars must be prefixed `MACRUN_EXPERIMENTAL_`
2. Experimental pathways must be adapter-owned (never in orchestration)
3. Must produce structured diagnostics on activation
4. Cannot be combined with `unsafe` — must be mutually exclusive governance buckets
5. Platform must track which experimental features are in use via compat-db metadata

---

## Category 7: Hard Failure

**Definition**: Execution is blocked. No degradation pathway exists. The platform refuses to launch.

| Property          | Value                                                                                      |
|-------------------|--------------------------------------------------------------------------------------------|
| User-visible      | Launch blocked with explicit reason                                                       |
| Runtime guarantees | N/A — execution prevented                                                                  |
| Allowed behaviors | Rejecting Mach-O .node modules (without bypass), hypervisor.framework, kernel extensions    |
| Forbidden         | Partial launch, timeout-based failure, silent termination                                 |
| Diagnostics       | `[FATAL] Hard failure: <reason>. Application cannot run on this platform.`                 |
| Compat-db tag     | `compatibility_tier: broken` with `degradation: hard_failure`                              |
| Escalation        | N/A — user must accept that the app cannot run or use VM-assisted execution               |
| Safety            | SAFE. No execution occurs.                                                                |

---

# Degradation Escalation Policy

| Current State          | Trigger                                                  | Escalation Target                |
|------------------------|----------------------------------------------------------|----------------------------------|
| Transparent            | Shim activation required                                 | Shimmed Compatibility            |
| Shimmed                | GPU must be disabled                                     | Functional Degradation           |
| Shimmed                | More than 5 shims active                                 | Review for VM-assisted execution |
| Functional             | User reports feature loss                                | Review tier escalation           |
| Functional             | Performance unacceptable                                 | VM-assisted execution            |
| Unsafe                 | Any crash or undefined behavior                          | Hard Failure                     |
| Experimental           | Validation complete                                      | Appropriate governed category    |
| Experimental           | Crash or data corruption                                 | Hard Failure                     |

---

# Degradation Diagnostics Requirements

Every degradation activation MUST produce structured diagnostics:

```
[MACRUN:DEGRADATION] tier=<tier> category=<category> capability=<name> reason=<reason> recommended_action=<action>
```

Example:
```
[MACRUN:DEGRADATION] tier=Tier0 category=unsafe capability=native-module-bypass reason=MACRUN_ALLOW_DARWIN_NATIVE=1 recommended_action=Consider_Tier4B_VM
```

The platform MUST NOT silently degrade. Every degradation event must be:
1. Logged to the adapter diagnostics stream
2. Surfaced in the compat-db degradation metadata
3. Visible in macrun-cli output when `--diagnostics` is set

---

# Prohibited Degradation Behaviors

The following are FORBIDDEN under ALL circumstances:

- **Silent bypass**: Any safety check that fails without user-visible diagnostics
- **Heuristic degradation**: "The app might need X so we'll just do Y" without explicit opt-in
- **App-specific degradation**: "This is Obsidian so we'll bypass native module check" — NEVER
- **Tier contamination**: Moving degradation logic from adapters into orchestration
- **Cross-shim side effects**: One shim's behavior changing another shim's activation state
- **Runtime-global mutation**: Shim-activated behaviors persisting across adapter sessions
- **Hidden fallbacks**: Degraded behavior that activates without the adapter logging it
- **Auto-escalation**: Automatically moving to a higher degradation tier without explicit logging

---

# Integration with compat-db

Every compat-db record MUST declare its degradation state:

```json
{
  "degradation": {
    "category": "shimmed",
    "active_shims": ["path-mapper", "shell-integration", "clipboard-bridge"],
    "degraded_capabilities": [],
    "unsafe_bypasses": [],
    "confidence": "functional",
    "recommended_action": "None — Tier 0 functional"
  }
}
```

For unsafe compatibility:
```json
{
  "degradation": {
    "category": "unsafe",
    "active_shims": ["path-mapper", "platform-normalizer"],
    "degraded_capabilities": [],
    "unsafe_bypasses": ["MACRUN_ALLOW_DARWIN_NATIVE"],
    "bypassed_modules": ["get-fonts.node", "btime.node"],
    "confidence": "experimental",
    "recommended_action": "Upgrade to Tier 4B VM-assisted for full compatibility"
  }
}
```

---

# Cross-Reference

- **FAILURE_MODEL.md** Section "Standardized Degradation Pathways" — CoreAnimation flattening, Metal GPU fallback, keyring recovery
- **SUBSTRATE_MODEL.md** Section 6 "Adapter Invariants" — No Orchestration Policy, Diagnostics Normalization
- **ARCHITECTURE_V6.md** Invariant 5 — "Unsupported Must Be Explicit"

---

*Last updated: 2026-06-05. Authoritative for all degradation decisions.*
