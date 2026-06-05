# Shim Governance Specification

Canonical governance for all platform shim scripts.

Authoritative for every shim in `runtime/shims/`. No shim may activate, patch, or modify behavior outside the rules defined here.

Architecture references:
- [ARCHITECTURE_V6.md](ARCHITECTURE_V6.md) — Tier 0 Runtime Substitution, Runtime Shim Contract
- [DEGRADATION_MODEL.md](DEGRADATION_MODEL.md) — Degradation categories, diagnostics requirements
- [SUBSTRATE_MODEL.md](SUBSTRATE_MODEL.md) — Adapter isolation, substrate boundaries
- [SEMANTIC_DIAGNOSTICS.md](SEMANTIC_DIAGNOSTICS.md) — Renderer observability infrastructure

---

# Shim Categories

## Category A: Always-Active (No Opt-Out)

Shims that execute unconditionally when `preload-main.js` loads.

**Current always-active shims:**
- `env-normalizer.js` — normalizes HOME, TMPDIR, USER, SHELL, LANG, sets XDG defaults
- `platform-normalizer.js` — stubs systemPreferences, nativeTheme, app.isPackaged, dlopen catch-all

**Activation rule**: Always loaded. No environment variable needed.

**Mutation boundary**: May only modify process.env (always-active shims) or Electron API stubs (platform-normalizer). Must never touch the filesystem, spawn processes, or modify global state outside Electron's API surface.

---

## Category B: Conditionally-Active (MACRUN_SHIM_* Gated)

Shims activated only when the adapter sets the corresponding environment variable.

| Shim | Gate Variable | Activation Condition |
|------|---------------|---------------------|
| `path-mapper.js` | `MACRUN_SHIM_PATHS=1` | Activated for Tier 0 apps needing XDG |
| `disable-gpu.js` | `MACRUN_SHIM_DISABLE_GPU=1` | Activated when GPU acceleration problematic |
| `disable-sparkle.js` | `MACRUN_SHIM_DISABLE_UPDATER=1` | Activated for all macOS Electron apps |
| `notification-bridge.js` | `MACRUN_SHIM_NOTIFICATIONS=1` | Activated for apps using Notification API |
| `clipboard-bridge.js` | `MACRUN_SHIM_CLIPBOARD=1` | Activated for apps using clipboard API |
| `shell-integration.js` | `MACRUN_SHIM_SHELL=1` | Activated for apps using shell.openExternal |
| `electron-normalization-registry.js` | `MACRUN_SHIM_NORMALIZATION=1` | Governed API normalization registry (per ELECTRON_API_NORMALIZATION.md) |

### Diagnostic Shims (Observability-Only)

| Shim | Gate Variable | Activation Condition |
|------|---------------|---------------------|
| `renderer-diag.js` | `MACRUN_DIAG_RENDERER=1` | Activated for renderer semantic diagnostics (console errors, unhandled rejections, IPC visibility, DOM hydration, module resolution) |
| `main-diag.js` | `MACRUN_DIAG_MAIN=1` | Activated for main-process lifecycle instrumentation (BrowserWindow, webContents, GPU crash, ready-to-show, app events) |

Both diagnostic shims are **purely observational** — never mutate behavior, never suppress errors, never auto-retry. Activated by explicit environment variable only.

**Activation rule**: Gate variable must be set to `"1"` in the child process environment by the adapter's `build_child_environment()`. The shim MUST check the gate at its entry point and return immediately if the gate is not set.

**Mutation boundary**: May modify Electron APIs within their declared scope. Must never affect other shim scopes. Must never modify global state outside Electron's API surface. Diagnostic shims modify NOTHING — they only observe and log.

---

## Category C: Adapter-Owned (Never Auto-Activates)

Behaviors owned exclusively by the adapter, never by shim scripts directly.

**Current adapter-owned behaviors:**
- `MACRUN_ALLOW_DARWIN_NATIVE` — native module bypass (unsafe compatibility)
- `MACRUN_EXPERIMENTAL_*` — any experimental compatibility pathway
- Process group management (SIGTERM/SIGKILL escalation)
- Runtime binary resolution
- ASAR extraction and sibling resource symlinking

---

# Preload Ordering Rules

`preload-main.js` is the SINGLE preload entry point. It loads sub-shims in a fixed, deterministic order:

1. `env-normalizer.js` (always-active)
2. `platform-normalizer.js` (always-active)
3. `path-mapper.js` (conditional)
4. `disable-gpu.js` (conditional)
5. `disable-sparkle.js` (conditional)
6. `notification-bridge.js` (conditional)
7. `clipboard-bridge.js` (conditional)
8. `shell-integration.js` (conditional)
9. `renderer-diag.js` (conditional, diagnostic-only)

**Ordering guarantees:**
- Always-active shims MUST run before any conditional shim
- Environment normalizers MUST run before platform normalizers
- Path mapping MUST run before any shim that accesses the filesystem
- GPU disablement MUST run before any rendering-dependent shim
- Diagnostic shims run LAST — after all behavioral shims have activated
- Notification/clipboard/shell shims have no inter-dependencies and are order-independent among themselves

**No hidden chaining**: Sub-shims must NOT require() each other. Each is loaded independently by preload-main.js. Cross-shim side effects are forbidden.

---

# Mutation Boundaries

Each shim has a declared mutation boundary:

| Shim | What it may modify | What it must NOT touch |
|------|-------------------|----------------------|
| `env-normalizer.js` | `process.env.*` | Electron APIs, filesystem, global state |
| `platform-normalizer.js` | Electron systemPreferences, nativeTheme, dlopen | `process.env`, filesystem, app.getPath() |
| `path-mapper.js` | `app.getPath()` | `process.env` (reads only), systemPreferences |
| `disable-gpu.js` | `app.commandLine`, `app.disableHardwareAcceleration` | `process.env`, filesystem |
| `disable-sparkle.js` | `autoUpdater.*`, `process.dlopen` (blocked modules only) | Non-updater APIs, filesystem |
| `notification-bridge.js` | `Notification` constructor, `child_process.spawn` | Clipboard, shell, path APIs |
| `clipboard-bridge.js` | `clipboard.readText/writeText` | Notifications, shell, path APIs |
| `shell-integration.js` | `shell.openExternal/showItemInFolder` | Clipboard, notifications, path APIs |
| `renderer-diag.js` | NOTHING — observability only | All APIs — diagnostic capture only |
| `main-diag.js` | NOTHING — observability only | All APIs — diagnostic capture only |

**Cross-boundary mutations are FORBIDDEN.**

---

# Observability Requirements

Every shim activation MUST produce a diagnostic message:
```
[macrun-shim] <shim_name>: activated (reason: <gate_variable>=1)
```

Diagnostic shims produce additional structured output to `MACRUN_DIAG_FILE`:
```
[MACRUN:DIAG:<category>] <event> <detail>
```

---

*Last updated: 2026-06-05. Authoritative for all shim governance decisions.*
