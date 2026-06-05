# Electron Semantic Diagnostics & Renderer Observability

Canonical guide for diagnosing Electron renderer failures in the macOS Runtime Platform.

This document defines the diagnostic infrastructure, semantic failure categories, and investigation procedure for blank-window / renderer-failure scenarios.

Architecture references:
- [ARCHITECTURE_V6.md](ARCHITECTURE_V6.md) — Tier 0 Runtime Substitution
- [DEGRADATION_MODEL.md](DEGRADATION_MODEL.md) — Degradation categories, unsafe/experimental governance
- [SHIM_GOVERNANCE.md](SHIM_GOVERNANCE.md) — Shim lifecycle, activation rules, mutation boundaries
- [FAILURE_MODEL.md](FAILURE_MODEL.md) — Failure modes, crash containment

---

# Diagnostic Infrastructure

## renderer-diag.js (Conditional, MACRUN_DIAG_RENDERER=1)

A purely observational shim loaded in the Electron preload context. Activates when `MACRUN_DIAG_RENDERER=1` is set.

**Captures (non-mutating, diagnostic-only):**
- `console.error`, `console.warn`, `console.log` — timestamps, process attribution, content (first 300 chars)
- `unhandledRejection` / `uncaughtException` — full stack traces, semantic classification
- `window.onerror` / `window.onunhandledrejection` — renderer-level error capture
- `Module._resolveFilename` failures — require() resolution tracing
- Electron IPC: `ipcRenderer.on`, `ipcRenderer.invoke`, `ipcRenderer.send` — channel registration + invocation visibility
- `contextBridge` availability detection
- `DOMContentLoaded` / `window.load` — DOM hydration visibility (body children count, innerHTML preview)
- WebGL context probe — GPU/rendering availability

**Writes to:** `MACRUN_DIAG_FILE` (default: `/tmp/macrun-diag-<pid>.log`)

## main-diag.js (Conditional, MACRUN_DIAG_MAIN=1)

Injected via `NODE_OPTIONS=--require` in the Electron main process. Instruments the Electron app lifecycle.

**Captures (non-mutating, diagnostic-only):**
- `app.*` events: ready, window-all-closed, quit, gpu-process-crashed, render-process-gone
- `BrowserWindow` lifecycle: construction, ready-to-show, show, close
- `webContents` lifecycle: did-start-loading, did-finish-load, did-fail-load (with error codes), dom-ready, crashed, unresponsive
- `process.on('uncaughtException')` / `process.on('unhandledRejection')` in main process

**Writes to:** `MACRUN_DIAG_FILE` (same file as renderer-diag — unified diagnostic stream)

---

# Semantic Failure Categories

Every captured error is classified into one of these categories:

| Category | What it means | Typical blank-renderer correlation |
|----------|--------------|-----------------------------------|
| `module_resolution` | `require()` or `import()` failed to find a module | MISSING: preload globals, missing node_modules |
| `preload_contextBridge` | `contextBridge.exposeInMainWorld` failed or not available | MISSING: IPC bridge — renderer can't communicate with main |
| `ipc` | IPC channel registration/invocation failed | MISSING: renderer → main communication broken |
| `api_mismatch` | An Electron/Node API returned unexpected type | UNSTABLE: API contract violated |
| `missing_global` | `ReferenceError: X is not defined` in renderer | MISSING: preload didn't expose expected global |
| `resource_load` | `did-fail-load` in webContents | MISSING: HTML/JS/CSS failed to load from disk or network |
| `protocol_handler` | Custom protocol (`file://`, `app://`) registration failed | MISSING: custom protocol not registered |
| `sandbox` | Sandbox policy prevented access | UNSTABLE: sandbox blocking required API |
| `gpu` | GPU process crashed, WebGL unavailable | UNSTABLE: software rendering fallback |
| `webContents` | webContents lifecycle error | MISSING: renderer process died |
| `network` | HTTP/HTTPS request failed | UNSTABLE: network-dependent renderer |
| `csp` | Content-Security-Policy violation blocked a resource | MISSING: CSP blocks required inline script |
| `hydration` | DOM initialization logic failed | MISSING: React/Vue/Svelte hydration crashed silently |
| `file_not_found` | `ENOENT` — a required file is missing | MISSING: extracted ASAR missing sibling files |
| `permission` | `EACCES` / `EPERM` — file permission error | MISSING: read-only cache or temp dir |
| `runtime_exception` | Unclassified runtime error | UNKNOWN: needs deeper investigation |

---

# Investigation Procedure for Blank Renderer Window

## Step 1: Enable diagnostics

```bash
MACRUN_DIAG_RENDERER=1 MACRUN_DIAG_MAIN=1 \
  ./build/tooling/macrun-cli/macrun-cli --launch /path/to/app.app
```

This enables both renderer and main-process diagnostics. Output goes to stderr AND to `/tmp/macrun-diag-<pid>.log`.

## Step 2: Check electron launch + window creation events

Look for these markers in the diagnostic output:

```
[MACRUN:DIAG:lifecycle] main_diag_active
[MACRUN:DIAG:lifecycle] main_diag_hooks_installed
```

If missing → main-diag.js didn't load. Check that `NODE_OPTIONS` was propagated correctly.

## Step 3: Check BrowserWindow creation

```
[MACRUN:DIAG:window] BrowserWindow_created
[MACRUN:DIAG:window] BrowserWindow_ready
```

If `BrowserWindow_created` is present but `BrowserWindow_ready` is NOT → the app created a window but it never became "ready" (webContents failed to initialize).

## Step 4: Check webContents lifecycle

```
[MACRUN:DIAG:webContents] wc#1.did-start-loading
[MACRUN:DIAG:webContents] wc#1.did-finish-load    ← KEY: content loaded successfully
```

If `did-start-loading` fires but `did-finish-load` does NOT → the renderer process crashed or hung during content loading.

If `did-fail-load` fires → check the error code and URL. Common causes:
- `errorCode: -105` → DNS resolution failure (network-dependent boot)
- `errorCode: -6` → FILE_NOT_FOUND (HTML entry point missing)
- `errorCode: -3` → ABORTED (renderer crashed)

## Step 5: Check for IPC registration

```
[MACRUN:DIAG:ipc] ipcRenderer_available
[MACRUN:DIAG:ipc] ipc_channel_registered {"channel":"..."}
[MACRUN:DIAG:ipc] ipc_invoke {"channel":"..."}
```

If `ipcRenderer_available` is present but NO channels are registered → the preload script loaded but the app's IPC layer failed to initialize. Often indicates contextBridge or preload API mismatch.

If NO `ipcRenderer_available` → `contextBridge` is not available or the preload cannot access `electron.ipcRenderer`.

## Step 6: Check module resolution

```
[MACRUN:DIAG:module_resolution] require_failed {"request":"some-module"}
```

If module resolution failures appear during startup → the app is missing dependencies that need to be in the extracted ASAR directory or sibling symlinks.

## Step 7: Check DOM hydration

```
[MACRUN:DIAG:hydration] DOMContentLoaded {"readyState":"complete","bodyChildren":0}
[MACRUN:DIAG:hydration] window_load {"bodyChildren":0,"bodyHTML":"no-body"}
```

If `bodyChildren` is 0 at both DOMContentLoaded and window.load → the renderer loaded but the JavaScript framework (React/Vue/Svelte) failed to hydrate the DOM. Check for console errors in step 8.

## Step 8: Check renderer console errors

```
[MACRUN:DIAG:runtime_exception] uncaught_exception
[MACRUN:DIAG:api_mismatch] window_error
[MACRUN:DIAG:preload_contextBridge] ...
```

These are the most useful signals. Each error is classified by semantic category. The category tells you which subsystem to investigate:

- `module_resolution` → check extracted app directory, sibling symlinks
- `preload_contextBridge` → check preload script compatibility
- `ipc` → check IPC bridge setup
- `api_mismatch` → Electron version mismatch or API deprecation
- `missing_global` → preload didn't expose expected API to renderer
- `resource_load` → entry file not found or protocol not registered
- `csp` → Content-Security-Policy is blocking rendering
- `hydration` → framework mount failed

## Step 9: Symptom-to-category diagnostic flowchart

```
Blank window?
├── BrowserWindow_created present?
│   ├── NO → App never created a window. Main process log/error.
│   └── YES → did-fail-load?
│       ├── YES with errorCode → resource_load or network category
│       └── NO → did-finish-load?
│           ├── NO → renderer crashed. Check gpu/process events.
│           └── YES → DOM hydration issue. Check bodyChildren.
│               ├── bodyChildren > 0 → App rendered, possibly CSS-hidden.
│               │   Check console errors for CSP or styling issues.
│               └── bodyChildren == 0 → Framework failure.
│                   ├── require_failed entries → module_resolution
│                   ├── ipc_channel_registered empty → preload/IPC
│                   ├── contextBridge_missing → preload/env issue
│                   └── runtime_exception → check full stack trace
```

---

# Integration with Degradation Governance

Diagnostic activation does NOT change degradation state. `MACRUN_DIAG_RENDERER` and `MACRUN_DIAG_MAIN` are Category B (conditional) shims per SHIM_GOVERNANCE.md — activated by env var, purely observational, no behavioral mutation.

When investigation identifies a root cause, the fix must be registered under the appropriate degradation category:
- Missing module → `degraded_capabilities: ["module_resolution"]`
- IPC bridge failure → `degraded_capabilities: ["ipc"]`
- CSP violation → `degraded_capabilities: ["csp"]`
- API mismatch → `degraded_capabilities: ["api_mismatch"]`

---

*Last updated: 2026-06-05.*
