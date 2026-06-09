# macRun Limitations, Tiers & Compatibility Matrix

This document provides a realistic assessment of the capabilities, limitations, and
development tiers of the **macRun** platform as of June 2026.

> [!TIP]
> **Having trouble launching an application?**
> If your application fails to launch, crashes, or displays a blank screen, please consult the **[Blank-Window Investigation & Diagnostics Procedure](GOVERNANCE.md#4-semantic-diagnostics-infrastructure)** in [GOVERNANCE.md](GOVERNANCE.md) for step-by-step troubleshooting instructions.

---

## 1. Execution Tiers Matrix

macRun divides application compatibility into five execution tiers. Lower-numbered tiers
run with native-level performance, while higher tiers rely on virtualization to handle
complex macOS-native requirements.

| Tier    | Strategy                  | Supported Applications                                | Status                           |
|---------|---------------------------|-------------------------------------------------------|----------------------------------|
| **Tier 0**  | **Runtime Substitution**  | Electron, Tauri, Wails apps                       | **Functional** — 4 apps validated |
| **Tier 1**  | **CLI Compatibility**     | Mach-O CLI binaries, Homebrew tools, Swift compiler   | **Phase 5 — Future**             |
| **Tier 2**  | **Lightweight Cocoa**     | Simple native AppKit/Cocoa graphical applications     | **Phase 6 — Future**             |
| **Tier 3**  | **ARM64 Translation**     | Apple Silicon CLI binaries on x86_64 host processors  | **Phase 7 — Future**             |
| **Tier 4B** | **VM-Assisted Streaming** | Complex, deeply integrated macOS apps                 | **Phase 8 — Future**             |

Tiers 1–4 have established adapter interfaces but are not yet implemented. See
[ROADMAP.md](../../ROADMAP.md) for the phase plan.

---

## 2. Validated Applications (Tier 0)

### Primary Targets (no official Linux build)

These applications have no official Linux release. macRun runs them directly from their
original unmodified macOS app bundles.

#### Claude Desktop — Class B: API Drift
- **Status**: Functional
- **Compatibility Class**: B — self-contained with Electron API drift requiring
  normalization
- **Validated On**: Ubuntu 24.04, Electron 42 substrate
- **Working**: Conversation management, file upload, window scaling, clipboard transfer,
  desktop notifications
- **Degradation**: Auto-updater stubbed; credential storage bridged to `libsecret`
- **Required Flags**: `MACRUN_ALLOW_DARWIN_NATIVE=1`
- **Guide**: [docs/guides/claude/README.md](../guides/claude%20desktop/README.md)

#### Codex Desktop — Class D: Client-Server
- **Status**: Functional
- **Compatibility Class**: D — external backend substitution required; critical native
  module compilation required
- **Validated On**: Ubuntu 24.04, Electron 42.3.3 substrate (via substrate negotiation)
- **Working**: Full UI hydration, Rust backend CLI integration, SQLite state, dark/light
  mode rendering
- **Required Steps**: Linux-native `better-sqlite3` compilation; `CODEX_CLI_PATH`
  backend substitution
- **Required Flags**: `MACRUN_ALLOW_DARWIN_NATIVE=1`, `CODEX_CLI_PATH=/path/to/codex`
- **Guide**: [docs/guides/codex/README.md](../guides/codex%20desktop/README.md)

#### DeepSeek GUI — Class C: IDE-Class
- **Status**: Functional
- **Compatibility Class**: C — native module compilation required (better-sqlite3)
- **Validated On**: Ubuntu 24.04, Electron 42.3.3 substrate (via substrate negotiation)
- **Working**: Main application window rendering, local agent runtime (`kun`), database initialization
- **Required Steps**: Copy or recompile Linux-native `better-sqlite3` module
- **Required Flags**: `MACRUN_ELECTRON_VERSION=42.3.3`, `MACRUN_ALLOW_DARWIN_NATIVE=1`
- **Guide**: [docs/guides/deepseek/README.md](../guides/deepseek%20GUI/README.md)

### Architecture Stress Tests (official Linux builds exist)

These applications have official Linux support and were used to validate the
compatibility architecture under real-world complexity. They demonstrate the platform
works — they are not primary use cases for macRun.

#### Obsidian — Class A: Self-Contained
- **Status**: Fully functional
- **Compatibility Class**: A — self-contained, no native dependencies
- **Working**: Filesystem reads/writes, plugins, community themes, hotkeys, markdown
  rendering
- **Degradation**: Path-mapping shimmed to XDG directories

#### Cursor — Class C: IDE-Class
- **Status**: Functional
- **Compatibility Class**: C — native module compilation required
- **Working**: Editor core, workspace management, extension host, terminal
- **Required Steps**: Linux-native `@vscode/sqlite3` and `spdlog` compilation

---

## 3. Phase 4 Active Limitations

The following limitations are known, understood, and being addressed in Phase 4:

### Native Module ABI Incompatibility (Phase 4A — Active)
macOS-compiled `.node` native modules (SQLite, PTY, logging) cannot load on Linux. Each
must be recompiled targeting the exact Electron ABI version in use. This process is
currently manual; Phase 4A is building a declarative module substitution pipeline.

**Impact**: Class C and D applications require manual native module compilation steps
before first launch.

### Electron Version Constraints (Phase 4B — Next)
macRun currently maintains Electron 28.x and 42.x in its runtime substrate. Applications
built against Electron 30–40 may fall into a version gap where normalization overhead
increases. The runtime matrix expansion in Phase 4B addresses this systematically.

**Impact**: Some applications may show minor rendering or API-compatibility differences
if built against an Electron version not in the current substrate matrix.

### Multi-View Rendering Reliability (Phase 4C — Next)
Complex Electron applications (VSCode-class, Codex) use split-pane and multi-view
rendering architectures (BrowserView, WebContentsView). These interact with GPU
acceleration and compositor semantics in ways that can produce layout issues specific to
the substitution environment.

**Impact**: Codex and Cursor may exhibit occasional rendering anomalies in complex
workspace layouts. Core functionality is not affected.

---

## 4. Explicit Non-Goals & Architectural Limits

To maintain a sustainable engineering scope, the following are explicitly out of scope:

1. **No SwiftUI Parity**: SwiftUI is a complex, state-retaining rendering engine.
   Reimplementing SwiftUI on Linux userspace is not planned. Applications with complex
   SwiftUI UI will require Tier 4B VM-assisted execution (Phase 8 — future).

2. **No Metal Acceleration**: A general-purpose Metal-to-Vulkan translation layer is not
   planned. Applications requiring native Metal drivers for GPU-intensive rendering are
   unsupported.

3. **No AppKit Completeness**: Tier 2 targets "sufficient AppKit" for lightweight
   utilities. Advanced accessibility hooks, complex system compositors, and custom window
   styles are flattened or ignored.

4. **No Kernel Substrate**: macRun does not support macOS kernel extensions (KEXTs) or
   Hypervisor.framework applications.

5. **No Apple Ecosystem Integrations**: Apple ID sign-in, iCloud sync, iMessage, and App
   Store DRM are not supported.

6. **No Production Sandbox Guarantee**: Processes run in standard Linux environments.
   The platform does not implement the Darwin sandbox and is intended for developer
   desktop compatibility, not secure untrusted binary execution.
