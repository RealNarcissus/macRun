# macRun — Roadmap

This roadmap reflects the actual current state of the project. It is intentionally
honest about what is complete, what is in progress, and what is genuinely speculative.

The roadmap is capability-driven, not release-driven. Phases do not have fixed dates.
Exit criteria define completion, not calendars.

---

## Guiding Principles

1. Deliver the highest user utility earliest
2. Prefer runtime substitution over emulation when viable
3. Prefer explicit degradation over silent failure
4. Keep Linux as the primary user environment
5. Avoid infinite compatibility scope — the project fails if it chases full macOS parity

---

## Strategic Position

macRun started as a macOS app launcher concept. It has evolved into something more
specific and more useful: a governed runtime compatibility platform for modern desktop
applications that ship macOS-first but run on cross-platform foundations.

That distinction matters. The goal is not to recreate macOS. The goal is to make the
applications Linux developers actually need work on Linux, with architecture that scales
as those applications evolve.

---

## Realistic Current Position

| Area                    | Status              |
|-------------------------|---------------------|
| Architecture            | Mature              |
| Governance model        | Mature              |
| Electron substitution   | Strong, early-stage |
| Complex Electron apps   | Emerging            |
| Native module pipeline  | Active development  |
| Darling / native apps   | Not started         |
| VM-assisted execution   | Not started         |
| Ecosystem tooling       | Minimal             |
| Public visibility       | Very early          |

---

## What Has Been Proven

Before the phased detail, the headline results:

**Electron runtime substitution works.** macRun runs Claude Desktop, Codex, Obsidian,
and Cursor on Linux from their original unmodified macOS app bundles. No repackaging.
No waiting on a maintainer to publish a new build when upstream ships an update.

**Backend substitution works.** Codex depends on a macOS-only Rust CLI backend. macRun
successfully replaces it with a Linux-native equivalent, redirects the frontend's
process bindings, and preserves full application functionality. This is not a stub or a
workaround — it is a generalisable runtime-platform capability.

**Governance at this stage is unusual.** Most early compatibility projects accumulate
undocumented hacks that become impossible to reason about. macRun has a degradation
model, a shim governance layer, an API normalization registry, and structured
diagnostics. The architecture can be extended without breaking what already works.

---

## Completed Phases

### Phase 0 — Architecture and Governance
**Status: Complete**

Established the canonical architecture, execution model, substrate model, failure model,
degradation model, shim governance, API normalization model, and semantic diagnostics
model. This phase produced the architectural identity of the project and is not expected
to require major revision.

Key documents produced: ARCHITECTURE_V6.md, EXECUTION_MODEL.md, SUBSTRATE_MODEL.md,
FAILURE_MODEL.md, DEGRADATION_MODEL.md, SHIM_GOVERNANCE.md,
ELECTRON_API_NORMALIZATION.md, SEMANTIC_DIAGNOSTICS.md.

---

### Phase 1 — Detection and Orchestration
**Status: Complete**

macRun can inspect any macOS .app bundle and determine what it is, which execution tier
applies, and whether execution is supported. The pipeline covers bundle detection,
Info.plist parsing, Mach-O classification, capability scoring, and tier routing.

---

### Phase 2 — Runtime Adapter Layer
**Status: Complete**

The orchestrator is now substrate-independent. All execution backends (Electron, Darling,
QEMU, WebKit) operate behind clean adapter interfaces. Substrate decisions do not leak
into orchestration logic.

Adapters currently implemented: ElectronAdapter (functional), DarlingAdapter (skeleton),
QemuAdapter (skeleton), WebKitAdapter (skeleton).

---

### Phase 3 — Runtime Substitution Execution
**Status: Complete**

This phase grew significantly beyond its original scope and represents the largest
engineering milestone to date.

**3A — Runtime Acquisition:** Electron, QEMU, Darling, and WebKit acquisition
pipelines. Lifecycle management, health validation, integrity verification, runtime
caching.

**3B — Electron Runtime Integration:** ASAR extraction, preload injection, shim
infrastructure, native module inspection, process containment, XDG integration,
renderer lifecycle management.

**3C — Degradation Governance:** Degradation categories, unsafe mode governance,
normalization governance, confidence classification, monotonic escalation, structured
degradation reports.

**3D — Semantic Diagnostics:** Renderer observability, IPC tracing, module-resolution
tracing, DOM hydration analysis, semantic error classification, blank-window
diagnostics.

**3E — API Normalization:** Governed normalization registry, version-scoped API
patching, transparent runtime normalization across Electron API drift.

**3F — Real-World Validation:** Obsidian (Class A), Claude Desktop (Class B), Cursor
(Class C), Codex Desktop (Class D — partial, rendering issues under investigation).

The application compatibility spectrum that emerged from this phase:

| Class | Description                            | Example        | Status               |
|-------|----------------------------------------|----------------|----------------------|
| A     | Self-contained, no native dependencies | Obsidian       | Functional           |
| B     | API drift requiring normalization      | Claude Desktop | Functional           |
| C     | Native module compilation required     | Cursor         | Functional           |
| D     | External backend substitution required | Codex          | Functional (partial) |

---

## Active Phase

### Phase 4 — Advanced Native Integration and Runtime Translation
**Status: In Progress**

This is where the project becomes significantly harder. The work in Phase 3 operated
largely within the Electron runtime boundary. Phase 4 deals with everything outside it:
native modules, external backends, GPU layers, PTY subsystems, and multi-version runtime
management.

---

#### Phase 4A — Critical Native Module Resolution
**Status: Active**

The focus right now. Class C and D applications depend on native Linux functionality
that cannot be stubbed: SQLite engines, filesystem watchers, PTY subsystems, logging
libraries, GPU bindings. The strategy is not generic stubbing but a proper native module
substitution pipeline: ABI-compatible rebuilds, a module resolution registry, and
rebuild automation targeting specific Electron versions via @electron/rebuild.

Modules in scope: better-sqlite3, @vscode/sqlite3, node-pty, spdlog, filesystem
watchers, GPU bindings, helper daemons.

This phase is required to stabilise Cursor, Codex, and VSCode-class applications.

Exit criteria: Cursor and Codex launch reliably without manual module intervention.
Module substitution is declarative and registry-driven, not ad-hoc per application.

---

#### Phase 4B — Electron Runtime Matrix
**Status: Next**

macRun currently relies primarily on Electron 28 as its substitution runtime. Many
modern applications target Electron 30, 32, 35, and 40+. Maintaining a full matrix of
installed runtimes is not the goal — that path leads to unbounded maintenance surface.

The goal instead is smarter version negotiation: a capability-based resolution model
that selects the minimum viable runtime version for a given application's API surface,
rather than trying to match version numbers exactly. The normalization registry handles
API drift within a range; the matrix expansion reduces the range that normalization
needs to cover.

Exit criteria: macRun can negotiate runtime selection across at least three major
Electron generations without manual configuration.

---

#### Phase 4C — Workspace and View Rendering Reliability
**Status: Next**

Complex Electron applications (VSCode-class, Codex) use multi-view rendering
architectures: BrowserView, WebContentsView, split-pane orchestration, utility
processes. These interact with GPU acceleration and React hydration in ways that produce
rendering failures specific to the substitution environment. This phase investigates
and resolves those failure modes systematically.

Exit criteria: Codex renders without blank-window or hydration failures. Cursor
workspace view is stable under normal usage.

---

#### Phase 4D — Linux Backend Substitution Framework
**Status: Planned**

The Codex backend substitution was solved manually. This phase generalises it into a
first-class subsystem: a backend compatibility registry that maps macOS-only helper
binaries and app-server processes to Linux-native equivalents, with declarative
redirection rules and a resolution pipeline.

This is potentially one of the most transferable innovations in the project. Any
application that spawns a macOS-only subprocess becomes tractable through this
framework, not just Codex.

Exit criteria: Backend substitution is declarative. Adding a new application's backend
mapping does not require code changes, only registry entries.

---

## Future Tiers

The following phases are directionally committed but not yet in active planning. They
are described honestly at the level of current understanding rather than padded with
false specificity.

---

### Phase 5 — Darling Integration (Native Mach-O Execution)
**Status: Future**

This is where macRun moves beyond Electron into compiled native macOS binaries. It is
also where the engineering difficulty increases by an order of magnitude.

Darling provides Darwin syscall translation and Mach-O loading on Linux. macRun's
DarlingAdapter skeleton is in place. The actual work involves: hardening Darling's
stability for real-world applications, establishing a reliable Mach-O execution
environment, bridging Foundation and AppKit at a level sufficient for lightweight
utilities, and integrating with the existing degradation and diagnostics infrastructure.

Target application category: menu bar utilities, lightweight Cocoa tools, CLI-adjacent
native binaries, Objective-C applications with minimal UI surface.

What will not be attempted in this phase: deep SwiftUI correctness, Metal integration,
full AppKit parity. The scope boundary matters here — Phase 5 succeeds if a meaningful
class of lightweight native apps becomes usable, not if all native apps work.

Open questions that need answers before serious implementation begins: Darling's current
stability characteristics on modern Linux kernels, which AppKit surface area is
realistically achievable without a full compositor, and whether GNUstep's Wayland
backend is a viable WindowServer replacement for this use case.

---

### Phase 6 — WebKit and Cocoa-Lite Runtime
**Status: Future**

A subset of macOS applications are built around WKWebView or lightweight SwiftUI
wrappers over web content. These do not need full AppKit — they need a functional
WebKit embedding and basic window management. WebKitGTK on Wayland is the likely
foundation.

This phase is narrower than Phase 5 and may be more tractable. It depends on Phase 5
establishing the Darling substrate, but the rendering path diverges significantly.

Open questions: WKWebView API surface compatibility via WebKitGTK, message handler
bridging, and JavaScript context isolation.

---

### Phase 7 — QEMU Hybrid Execution
**Status: Future**

For applications that need deeper macOS system integration than Darling can provide,
a hybrid model: Linux-hosted UI with a QEMU-backed execution environment connected
via vsock bridges. This is the approach that handles Xcode-adjacent tooling and
complex AppKit applications.

This is substantially harder than Phases 5-6 and the architecture is less settled.
The QemuAdapter skeleton exists but the streaming, bridging, and input routing design
is not finalised. This phase will not begin until Phases 5 and 6 provide a clear
picture of what Darling can and cannot handle.

---

### Phase 8 — Virtualized macOS Runtime
**Status: Future — Last Resort Execution**

Full VM-backed compatibility for applications that cannot run any other way. This is
not the primary value proposition of macRun and will not be prioritised over the lower
tiers. It exists as a ceiling for the compatibility model, not as a near-term goal.

The VM lifecycle design (persistent vs on-demand), per-window framebuffer capture,
hotkey forwarding, and clipboard synchronisation are all open architecture questions
at this stage.

---

### Phase 9 — Productionisation and Ecosystem
**Status: Future**

Turning macRun from a developer platform into something a broader audience can use:
package manager integration, Flatpak/AppImage support, a GUI launcher, a compatibility
database website, automated CI testing against upstream app releases, and community
runtime manifests.

This phase is what makes the sustainability argument real. An automated pipeline that
detects when Claude Desktop or Codex ships a new version and validates it against the
macRun compatibility layer is the answer to the repackaging problem at scale.

---

## What Failure Looks Like

The project fails if it:

- expands toward infinite framework parity with macOS,
- loses the governance model and becomes an undocumented collection of hacks,
- attempts to solve every tier simultaneously instead of shipping working tiers,
- or mistakes architectural completeness for user utility.

The project succeeds if Linux users can run the applications they actually need, through
execution strategies appropriate to each application category, without requiring full
macOS reimplementation.
