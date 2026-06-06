# Electron Application Compatibility Spectrum

Canonical classification of Electron application architecture classes for the macOS Runtime Platform.

This document codifies the empirically discovered compatibility spectrum across four application architecture classes. All future detection, adaptation, and runtime orchestration decisions must reference these classifications.

Architecture references:
- ARCHITECTURE_V6.md — Execution Tier model, Invariants
- DEGRADATION_MODEL.md — Degradation categories
- SUBSTRATE_MODEL.md — Adapter isolation, substrate boundaries
- ELECTRON_API_NORMALIZATION.md — API drift handling

---

## Discovery Context

The platform has now tested four real-world Electron applications:

| App | Bundle ID | Architecture Class | Status |
|-----|-----------|-------------------|--------|
| Obsidian | md.obsidian | Class A: Self-Contained | Functional |
| Claude Desktop | com.anthropic.claudedesktop | Class B: API Drift | Functional |
| Cursor | com.todesktop.230313mzl4w4u92 | Class C: IDE-Class | Functional |
| Codex | com.openai.codex | Class D: Client-Server | Rendering issues (substrate drift suspected) |

These applications revealed a progressively deeper compatibility spectrum.

---

## Class A: Self-Contained Electron Apps

Example: Obsidian

### Characteristics
- All logic inside Electron renderer/main processes
- No external backend processes
- Native modules are optional (cosmetic/enhancement only)
- No critical databases
- Minimal platform assumptions

### Compatibility Strategy
- Proxy-based native module stubbing works effectively for non-critical modules
- Examples: `get-fonts`, `btime`

### Architectural Implication
- Cosmetic/optional native modules can be safely shimmed using governed degradation semantics
- No compilation of native replacements required
- Lowest compatibility engineering overhead

### Degradation Category
Typically: Transparent Substitution or Shimmed Compatibility

---

## Class B: Electron Apps with API Drift

Example: Claude Desktop

### Characteristics
- Self-contained architecture (no external processes)
- Built against newer Electron APIs than the substituted runtime
- Modern BrowserView/WebContents assumptions
- Platform validation logic (process.platform checks)
- Renderer hydration dependencies on Electron-version-specific APIs

### Discovered Incompatibilities
- `WebContentsView.prototype.setBackgroundColor` missing (Electron 28 vs 30+)
- `navigationHistory` API mismatch (renamed between versions)
- Packaged-runtime assumptions in preload scripts

### Compatibility Strategy
- Governed API normalization infrastructure (ELECTRON_API_NORMALIZATION.md)
- Normalizations must be: bounded, explicit, deterministic, diagnosable, registry-owned

### Architectural Implication
- Electron semantic API drift is a primary compatibility domain
- Normalization registry is the canonical resolution mechanism
- Version-scoped normalizations enable multi-version substrate support

### Degradation Category
Typically: Shimmed Compatibility

---

## Class C: IDE-Class Electron Platforms

Example: Cursor

### Characteristics
- VS Code-class architecture (fork or derivative)
- SQLite-backed application state (critical infrastructure dependency)
- ESM bootstrap complexity (`"type": "module"`)
- Utility process propagation (environment variable inheritance)
- Critical native modules (`@vscode/sqlite3`, `spdlog`)
- Advanced IPC infrastructure
- Multi-process orchestration

### Critical Discovery
Proxy-based stubbing FAILS for critical infrastructure modules.

| Module | Role | Proxy-Stub Result | Required Resolution |
|--------|------|-------------------|--------------------|
| `@vscode/sqlite3` | Settings/state database | Storage Error dialog, unusable editor | Linux-native compilation targeting Electron ABI |
| `spdlog` | Structured logging | Silent failure, missing diagnostics | Linux-native compilation |

### Compatibility Strategy
- Linux-native replacements must be compiled and substituted for critical native infrastructure modules
- Boot-shim must hook `process.dlopen` at process startup (before any module loads)
- Utility processes must inherit all `MACRUN_*` environment variables

### Architectural Implication
- Database/storage modules are architectural dependencies, not optional enhancements
- IDE-class apps use significantly more advanced Electron/Chromium semantics
- ESM module loading requires fundamentally different shim injection strategies
- Native module compilation against specific Electron ABIs is a required capability

### Degradation Category
Typically: Shimmed Compatibility with Functional Degradation for non-critical modules

---

## Class D: Client-Server Electron Platforms

Example: Codex Desktop

### Architecture
Codex is NOT a self-contained Electron app. It is a distributed local desktop platform:

```
+------------------------------+
|  Electron Frontend (UI)      |
|  (HTML/CSS/JS renderer)      |
|           |                  |
|      stdio MCP pipe          |
|           |                  |
|  +--------v-----------+      |
|  |  codex CLI binary   |     |
|  |  (Rust, app-server) |     |
|  |  - Authentication   |     |
|  |  - API calls        |     |
|  |  - Thread management|     |
|  |  - File operations  |     |
|  |  - Sandboxed exec   |     |
|  |  - Plugin system    |     |
|  +---------------------+     |
+------------------------------+
```

### Characteristics
- Rebranded Electron framework (custom framework name)
- Electron frontend is a thin UI shell
- Backend is a separate compiled binary (Mach-O arm64)
- Communication via stdio MCP (Model Context Protocol)
- Backend handles: authentication, API communication, thread management, file operations, sandboxed execution, plugins
- Also depends on critical SQLite infrastructure (`better-sqlite3`)

### Critical Discovery
The bundled backend binary is a macOS arm64 Mach-O executable. Linux cannot execute it. Unlike a `.node` module that can be stubbed with a Proxy, this is an entire backend server.

### Compatibility Strategy
- Substitute Linux-native CLI backend binary via `CODEX_CLI_PATH`
- Compile Linux-native `better-sqlite3` for database support
- Detection must recognize rebranded Electron frameworks

### Architectural Implication
- Backend process substitution is a first-class compatibility domain
- Distributed desktop orchestration must be supported
- Auxiliary binary replacement is a required capability
- Detection pipeline must handle non-standard Electron framework names
- Future compat-db records must declare external process dependencies

### Degradation Category
Currently: Functional Degradation (rendering issues suspected due to runtime substrate drift)

---

## Classification Decision Matrix

| Signal | Class A | Class B | Class C | Class D |
|--------|---------|---------|---------|--------|
| External backend processes | No | No | No (utility workers only) | Yes |
| Critical native modules | No | No | Yes (SQLite, logging) | Yes (SQLite + CLI) |
| API version sensitivity | Low | High | Medium-High | High |
| Proxy stubbing sufficient | Yes | Partially | No (for critical modules) | No |
| Native compilation required | No | No | Yes | Yes |
| Process substitution required | No | No | No | Yes |
| Rebranded framework | No | No | No | Possible |

---

## Integration with Detection Pipeline

The detector (`platform/detector/`) should evolve to emit an architecture class signal alongside the existing execution tier:

- Bundle analysis → framework fingerprinting → architecture classification → tier resolution

The architecture class informs the adapter about which compatibility strategies are required:
- Class A/B → standard shim/normalization pipeline
- Class C → native module compilation + ESM boot-shim
- Class D → process substitution + native module compilation + detection overrides

---

## Integration with compat-db

Future compat-db records should declare `architecture_class`:

```json
{
  "architecture_class": "class_d_client_server",
  "external_processes": [
    {
      "name": "codex-cli",
      "type": "backend-server",
      "protocol": "stdio-mcp",
      "binary_type": "mach-o-arm64",
      "substitution_env": "CODEX_CLI_PATH"
    }
  ],
  "critical_native_modules": [
    {
      "module": "better-sqlite3",
      "role": "state-database",
      "requires_compilation": true
    }
  ]
}
```

---

*Last updated: 2026-06-06. Authoritative for all Electron application classification decisions.*
