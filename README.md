# macRun

**macRun** is a governed macOS application compatibility and runtime substitution platform for Linux.

> **Developer / Architecture Preview Release**
> This repository represents an early architecture preview and developer prototype. It is NOT a
> production-ready system, nor is it a universal tool to "run any macOS app." It is published to
> showcase the systems architecture, runtime substitution governance, and compatibility engineering
> discipline.

---

## The Problem

Modern developer tooling increasingly ships macOS-first. AI assistants, coding environments, and
productivity applications built on cross-platform runtimes like Electron end up Linux-inaccessible
not because the technology is fundamentally incompatible, but because no governed compatibility
infrastructure exists to bridge the gap.

Community workarounds exist: some maintainers manually repackage macOS Electron apps for Linux,
patching and republishing each new release by hand. These are valuable efforts, but they create
dependency on a maintainer keeping pace with upstream. When Claude Desktop or Codex ships an
update, users wait.

macRun takes a different approach. Rather than repackaging, it runs the original unmodified app
bundle directly on Linux through a governed runtime substitution and compatibility layer. No
repackaging lag. No third-party distribution chain. The app runs from source as shipped.

---

## The Approach: Governed Compatibility

Rather than building a monolithic macOS emulator or reimplementing SwiftUI/AppKit/Metal from
scratch, macRun implements a hybrid compatibility model. It classifies applications into execution
tiers and applies the lowest-complexity viable runtime strategy for each:

1. **Tier 0: Runtime Substitution** (Electron/Tauri/Wails) — Extracts the app's HTML/JS/CSS
   assets and executes them against a native Linux runtime shell (Chromium/WebKitGTK), with a
   governance-controlled API normalization and shim layer handling platform differences.

2. **Tier 1: CLI Compatibility** (Darling Substrate) — Executes CLI and POSIX-compliant macOS
   binaries using Darwin syscall translation.

3. **Tier 2: Lightweight Cocoa** (AppKit/Cairo/Wayland) — Executes simple native AppKit
   applications using flattened CoreAnimation and basic rendering proxies.

4. **Tier 3-4: Translation and Virtualization** (ARM64/QEMU and VM-Assisted Streaming) — For
   deeply integrated macOS-native applications, via high-performance window streaming, bridge
   daemons, and hotkey/clipboard synchronization.

---

## Current Status

**Tier 0 (Electron Substitution): Functional.**

Successfully validated against real, production-grade applications on Linux:

- **Claude Desktop**: Resolves Electron version-specific API drift using the governed
  normalization registry. Launches, renders, handles IPC/runtime initialization, and is fully
  functional.
- **Codex Desktop**: Executes under negotiated Electron v42.3.3 with full UI hydration,
  integrating its Rust backend CLI and native SQLite state databases on Linux.

**Tier 1-2 (Darling Integration): Infrastructure phase.** Adapter boundaries and substrate
interfaces are established. Active development.

**Tier 3 (ARM64 Translation): Planned / Exploratory.**

**Tier 4B (VM-Assisted Execution): Planned / Exploratory.**

---

## Validation Proofs

macRun has been validated against two categories of applications:

### Primary Targets (no official Linux build)

**Claude Desktop** and **Codex Desktop** have no official Linux releases. Community repackaging
repos exist for both, but depend on a maintainer manually patching and republishing each upstream
update. macRun runs the original unmodified app bundle directly, with the compatibility layer
handling platform differences transparently.

- **Claude Desktop** running on Linux (Electron 42 Substrate):

![Claude Desktop on Linux](docs/guides/claude%20desktop/images/claude_linux.png)

- **Codex Desktop** running on Linux (EndeavourOS / Wayland):

![Codex Desktop on Linux](docs/guides/codex%20desktop/images/codex_linux.png)

For detailed guides on setup, native module building, and launching, see the [Claude Desktop Launch Guide](docs/guides/claude%20desktop/README.md) and the [Codex Desktop Launch Guide](docs/guides/codex%20desktop/README.md).

### Architecture Stress Tests (official Linux builds exist)

These applications already have official Linux support. They were used to push the compatibility
platform against real-world complexity and validate that the architecture holds under non-trivial
conditions:

- **Obsidian**: Validates runtime normalization on a self-contained Electron app.
- **Cursor**: Validates native module substitution and multi-process IPC handling across a
  complex engineering tool.

---

## Non-Goals

- **Not a macOS Desktop Clone**: macRun does not recreate Finder, Dock, or the macOS desktop
  environment.
- **Not a Wine Replacement**: macRun does not target general-purpose Windows or macOS game
  translation.
- **Not a Security Sandbox**: macRun does not provide containment guarantees beyond native
  process boundaries.
- **Not Universal Compatibility**: macRun does not promise SwiftUI, Metal, or AppKit parity.
- **Not Production-Ready**: Tier 1 and above components are under active design and partially
  stubbed.

---

## Quick Start: Running Applications

### 1. Build and Install Shims

```bash
# Compile the orchestrator
cmake --build build

# Install integration shims to cached runtime path (~/.cache/macrun/shims)
./runtime/shims/install.sh
```

### 2. Prepare and Launch

#### Step A: Extract the App Bundle

```bash
7z x /path/to/Application.dmg -o/tmp/extracted-app/
```

#### Step B: Classify the Application

```bash
./build/tooling/macrun-cli/macrun-cli --plan-only "/tmp/extracted-app/Application.app"
```

#### Step C: Resolve Native Modules and Helpers

**Darwin-native Node modules (`.node`)**: Use `MACRUN_ALLOW_DARWIN_NATIVE=1` to allow execution
with dynamic runtime Proxy stubbing. For modules essential to local state storage (e.g.
`better-sqlite3` in Codex), recompile on Linux targeting the appropriate Electron version using
`@electron/rebuild` and substitute inside the app's `node_modules`.

**Helper CLI binaries**: If the application spawns a bundled macOS executable (e.g. Codex's
stdio app-server), locate or compile a Linux-native ELF equivalent and set the appropriate path
variable (e.g. `CODEX_CLI_PATH`).

#### Step D: Launch

**Claude Desktop:**
```bash
MACRUN_ALLOW_DARWIN_NATIVE=1 \
NODE_PATH=~/.local/npm-global/lib/node_modules \
MACRUN_DIAG_RENDERER=1 MACRUN_DIAG_MAIN=1 \
./build/tooling/macrun-cli/macrun-cli --launch --diagnostics "/tmp/claude-run/Claude/Claude.app"
```

**Codex Desktop:**
```bash
CODEX_CLI_PATH="/path/to/linux-native/codex" \
MACRUN_ALLOW_DARWIN_NATIVE=1 \
./build/tooling/macrun-cli/macrun-cli --launch "/tmp/codex-run/Codex Installer/Codex.app"
```

**Obsidian** (stress test):
```bash
MACRUN_ALLOW_DARWIN_NATIVE=1 \
NODE_PATH=~/.local/npm-global/lib/node_modules \
./build/tooling/macrun-cli/macrun-cli --launch "/tmp/obsidian-run/Obsidian.app"
```

**Cursor** (stress test):
```bash
MACRUN_ALLOW_DARWIN_NATIVE=1 \
NODE_PATH=~/.local/npm-global/lib/node_modules \
MACRUN_DIAG_RENDERER=1 MACRUN_DIAG_MAIN=1 \
./build/tooling/macrun-cli/macrun-cli --launch --diagnostics "/tmp/cursor-run/Cursor.app"
```

---

## Architecture and Documentation

- [Architecture Overview](docs/public/ARCHITECTURE.md): Runtime tiers, adapter boundaries, and
  execution substrates.
- [Governance and Degradation](docs/public/GOVERNANCE.md): API normalizations, shim execution,
  and graceful degradation classifications.
- [Limitations and Tiers](docs/public/LIMITATIONS.md): Honest appraisal of supported tiers,
  compatibility policies, and non-goals.
- [Detailed Project Overview](docs/public/project_overview.md): Development objectives, core
  philosophy, and component boundaries.

---

## License

MIT. See [LICENSE](LICENSE).
