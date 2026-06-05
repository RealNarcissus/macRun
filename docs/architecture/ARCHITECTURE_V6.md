# macOS Runtime Platform for Linux — Production Architecture Plan v6

## Executive Summary

This project is not a general-purpose macOS reimplementation.

It is a production-oriented compatibility platform whose objective is:

> Run the macOS applications Linux users actually want, with native-feeling UX, minimal friction, and sustainable engineering scope.

The platform intentionally avoids pursuing complete macOS parity. Instead, it divides the ecosystem into execution classes and applies the least expensive viable execution strategy per class.

## System Architecture overview

```HERO SYSTEM ARCHITECTURE DIAGRAM

                         macOS Runtime Platform for Linux
┌──────────────────────────────────────────────────────────────────────────────┐
│                                                                              │
│                         User launches macOS app                              │
│                                                                              │
└──────────────────────────────────────────────────────────────────────────────┘
                                      │
                                      ▼
                     ┌────────────────────────────────┐
                     │        macrun Launcher         │
                     │  DMG mount • Bundle analysis   │
                     │  Tier classification           │
                     └────────────────────────────────┘
                                      │
          ┌───────────────────────────┼───────────────────────────┐
          │                           │                           │
          ▼                           ▼                           ▼

┌──────────────────┐      ┌──────────────────────┐     ┌──────────────────────┐
│ Tier 0           │      │ Tier 1–3             │     │ Tier 4B              │
│ Runtime Swap     │      │ Compatibility Layer  │     │ VM-Assisted Runtime  │
├──────────────────┤      ├──────────────────────┤     ├──────────────────────┤
│ Electron          │      │ Darling Runtime      │     │ macOS VM             │
│ Tauri             │      │ Mach-O Loader        │     │ Window Streaming     │
│ Wails             │      │ Cocoa Lite           │     │ Hotkey Bridge        │
│ PWAs              │      │ ARM64 Translation    │     │ Clipboard Sync       │
└──────────────────┘      └──────────────────────┘     └──────────────────────┘
          │                           │                           │
          └───────────────┬───────────┴───────────┬──────────────┘
                          ▼                       ▼

               ┌────────────────────────────────────┐
               │     Native Linux UX Integration    │
               ├────────────────────────────────────┤
               │ Wayland • Clipboard • Audio        │
               │ Notifications • Files • Hotkeys   │
               └────────────────────────────────────┘
```


The architecture is built around three core principles:

1. **Exploit app-category asymmetry**

   * Electron apps are not native Cocoa apps.
   * Tauri apps are not SwiftUI apps.
   * Raycast is not OrbStack.
   * Treating them identically creates unnecessary engineering cost.

2. **Use hybrid execution, not ideological purity**

   * Native Linux runtime substitution where possible
   * Compatibility-layer execution where feasible
   * VM-assisted execution where required

3. **Optimize for real-world utility**

   * AI desktop tooling
   * Developer tooling
   * Productivity apps
   * Launchers
   * Workflow systems
   * Modern Electron/Tauri ecosystems

The result is not “macOS on Linux.”

It is a layered macOS application runtime platform.

---

# Strategic Product Position

The project competes indirectly with:

* Wine / Proton
* WSLg
* Darling
* distrobox/toolbox/container workflows
* browser-only AI tooling

But the actual niche is:

> Native-feeling execution of modern macOS desktop tooling on Linux.

Primary target workloads:

| Category               | Examples                         |
| ---------------------- | -------------------------------- |
| AI desktop apps        | Cursor, Claude Desktop, Windsurf |
| Electron ecosystems    | VS Code derivatives, Obsidian    |
| Tauri ecosystems       | Distill-like apps                |
| Launcher/workflow apps | Raycast, Alfred                  |
| macOS CLI tooling      | Homebrew, Swift tools            |
| Lightweight Cocoa apps | utilities, note apps             |

Non-goals:

* Gaming
* Kernel extensions
* Hypervisors
* Full macOS desktop replacement
* Perfect AppKit fidelity
* Complete SwiftUI parity
* SIP/security replication

---

# Foundational Strategic Decisions

## Decision 1 — Utility-First Architecture

The architecture is explicitly optimized for:

* highest user value,
* lowest implementation cost,
* fastest usable release.

This creates a strict execution hierarchy:

| Strategy                        | Priority |
| ------------------------------- | -------- |
| Native Linux substitution       | Highest  |
| Translation + compatibility     | Medium   |
| VM-assisted execution           | High     |
| Full framework reimplementation | Lowest   |

This rule governs all future engineering decisions.

---

## Decision 2 — Electron/Tauri First

Modern macOS software is disproportionately:

* Electron,
* Chromium-based,
* WebView-based,
* or Tauri-based.

This changes feasibility dramatically.

The project therefore treats:

* Electron support,
* Tauri support,
* runtime substitution,
* and platform-API shimming

as Tier-1 product priorities.

This is the shortest path to immediate real-world usefulness.

---

## Decision 3 — VM-Assisted Execution Is a Core Pillar, Not a Fallback

VM-assisted execution is elevated from:

* “compatibility escape hatch”
  to:
* “primary architecture pillar.”

Reason:

Modern macOS workflow apps increasingly depend on:

* Accessibility APIs
* event taps
* Spotlight
* system-wide shortcuts
* private frameworks
* SwiftUI runtime assumptions
* Metal-backed rendering behavior

Attempting to fully emulate these in Linux userspace creates infinite engineering scope.

Instead:

Tier 4B executes them inside real macOS while exporting:

* windows,
* clipboard,
* notifications,
* hotkeys,
* files,
* and audio

to Linux.

Architecturally:

* WSLg reversed
* app streaming instead of desktop streaming
* Linux-first UX with macOS execution substrate

This is the only sustainable path for:

* Raycast-class apps
* Alfred-class apps
* future SwiftUI-heavy utilities

---

# Execution Tier Model

## Model Overview

```EXECUTION PIPELINE DIAGRAM
┌────────────────────┐
│ User launches app  │
└─────────┬──────────┘
          ▼
┌────────────────────┐
│ DMG/App Extraction │
└─────────┬──────────┘
          ▼
┌────────────────────┐
│ Bundle Inspection  │
│ Info.plist         │
│ Mach-O analysis    │
│ Framework probing  │
└─────────┬──────────┘
          ▼
┌────────────────────┐
│ Tier Classification│
└─────────┬──────────┘
          ▼
┌────────────────────┐
│ Capability Mapping │
│ GPU? SwiftUI? XPC? │
│ Accessibility?     │
└─────────┬──────────┘
          ▼
┌────────────────────┐
│ Execution Strategy │
│ Selection          │
└─────────┬──────────┘
          ▼
┌────────────────────┐
│ Runtime Provision  │
│ Darling / Native / │
│ VM                 │
└─────────┬──────────┘
          ▼
┌────────────────────┐
│ Linux Integration  │
│ Clipboard / Audio  │
│ Notifications      │
└─────────┬──────────┘
          ▼
┌────────────────────┐
│ Window Appears     │
└────────────────────┘
```


## Tier 0 — Runtime Substitution

### Objective

Avoid emulation entirely.

### Target

Electron, Tauri, Wails, PWAs.

### Strategy

Extract application resources and execute against native Linux runtimes.

### Why This Matters

This tier alone may cover:

* 50–70% of actual user demand.

### Electron Strategy

* Extract `.asar`
* Detect Electron major version
* Map to compatible Linux Electron runtime
* Provide platform shim layer:

  * keychain → libsecret
  * notifications → libnotify
  * shell integration → xdg-open
  * paths → XDG
  * auto-updater suppression

### Tauri Strategy

Two modes:

#### Simple Mode

* Linux WebKitGTK shell
* backend unavailable
* frontend/UI operational

#### Full Mode

* translated Rust backend
* IPC bridge
* Unix domain socket transport

### Success Criteria

If Tier 0 succeeds:

* app launches,
* renders correctly,
* persists settings,
* accesses files,
* supports clipboard,
* and behaves “Linux-native.”

The user should not perceive emulation.

---

## Tier 1 — CLI Compatibility

### Objective

Run Mach-O CLI tooling.

### Target

* Homebrew tools
* Swift tooling
* macOS-only CLI binaries

### Backend

Darling syscall/runtime layer.

### Key Requirement

Fast process startup and stable POSIX behavior.

### Critical Insight

CLI compatibility has dramatically lower complexity than GUI compatibility and should mature far earlier.

---

## Tier 2 — Lightweight Cocoa Compatibility

### Objective

Support simple native AppKit applications.

### Scope Constraints

This tier intentionally excludes:

* heavy SwiftUI assumptions,
* advanced CoreAnimation behavior,
* Metal-heavy rendering,
* accessibility-driven apps.

### Philosophy

Tier 2 is:

* “sufficient AppKit,”
  not:
* “macOS reimplementation.”

### Rendering Model

Initial rendering path:

AppKit
→ CoreGraphics
→ Cairo
→ Wayland

### CoreAnimation Strategy

Phase-1 behavior:

* flatten layer trees,
* immediate compositing,
* no GPU animation correctness.

This intentionally sacrifices:

* visual fidelity
  for:
* tractable engineering scope.

---

## Tier 3 — ARM64 Translation

### Objective

Run Apple Silicon-only binaries.

### Backend

QEMU user-mode translation.

### Strategic Constraint

Performance is acceptable for:

* utilities,
* AI apps,
* editors,
* launchers.

Not acceptable for:

* gaming,
* heavy rendering,
* media production.

### Architectural Position

ARM64 translation is mandatory because:

* Intel macOS binaries are declining rapidly.

---

## Tier 4 — Unsupported

Explicitly unsupported:

* Hypervisor.framework apps
* kernel extensions
* SIP-dependent software
* driver-level tooling

The platform must clearly distinguish:

* unsupported
  from
* broken.

---

## Tier 4B — VM-Assisted Streaming

## VM Streaming Overview

```VM STREAMING ARCHITECTURE DIAGRAM
┌──────────────────────────────────────────────────────────────────┐
│                        Linux Host                                │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Native Wayland Window                                           │
│          ▲                                                       │
│          │ GPU texture upload                                    │
│          │                                                       │
│  ┌──────────────────────────────┐                                │
│  │      Host Proxy Layer        │                                │
│  ├──────────────────────────────┤                                │
│  │ Frame decoder                │                                │
│  │ Wayland surface manager      │                                │
│  │ Clipboard bridge             │                                │
│  │ Notification bridge          │                                │
│  │ Hotkey registrar             │                                │
│  │ Audio routing                │                                │
│  └──────────────────────────────┘                                │
│                  ▲                                               │
│                  │ VirtIO stream                                 │
└──────────────────┼───────────────────────────────────────────────┘
                   │
                   ▼
┌──────────────────────────────────────────────────────────────────┐
│                         macOS VM                                 │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  macOS App                                                       │
│        │                                                         │
│        ▼                                                         │
│  CoreAnimation Surface                                           │
│        │                                                         │
│        ▼                                                         │
│  CGWindow Capture                                                │
│        │                                                         │
│        ▼                                                         │
│  Frame Encoder                                                   │
│        │                                                         │
│        ▼                                                         │
│  Bridge Daemon                                                   │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

### Objective

Run deeply integrated macOS apps without reimplementing macOS internals.

### Architecture

macOS VM
→ per-window capture
→ host-side native Wayland windows

### Strategic Role

Tier 4B is not optional.

It is the long-term sustainability mechanism for:

* SwiftUI-heavy apps,
* private-framework apps,
* accessibility-driven tooling,
* future macOS desktop ecosystems.

### UX Requirement

The user experience must feel like:

* “a Linux app that happens to run through a macOS substrate.”

Not:

* “remote desktop.”

---

## Execution Strategy overview

```EXECUTION STRATEGY MATRIX
┌───────────────┬────────────────────────────┬────────────────────────────┐
│ App Category  │ Primary Strategy           │ Reason                     │
├───────────────┼────────────────────────────┼────────────────────────────┤
│ Electron      │ Native runtime swap        │ Lowest complexity          │
│ Tauri         │ Hybrid bridge              │ IPC manageable             │
│ Mach-O CLI    │ Darling runtime            │ POSIX-compatible           │
│ Simple AppKit │ Lightweight Cocoa layer    │ Limited rendering scope    │
│ SwiftUI-heavy │ VM-assisted execution      │ Avoid CA/Metal complexity  │
│ Launcher apps │ VM-assisted execution      │ Needs global hooks         │
│ Hypervisors   │ Unsupported                │ Kernel substrate missing   │
└───────────────┴────────────────────────────┴────────────────────────────┘
```
---

# Core Architectural Constraints

## Constraint 1 — SwiftUI Is a Strategic Threat

Modern macOS ecosystems are increasingly SwiftUI-native.

SwiftUI is:

* renderer,
* state system,
* animation model,
* accessibility model,
* and layout engine.

The project therefore explicitly avoids promising:

* full SwiftUI parity.

Instead:

* lightweight SwiftUI apps may partially function under Tier 2,
* complex SwiftUI apps route toward Tier 4B.

This prevents infinite compatibility creep.

---

## Constraint 2 — CoreAnimation Is the Real Wall

CoreAnimation is not “animation.”

It is:

* retained compositing,
* geometry management,
* synchronization infrastructure,
* and rendering orchestration.

The platform therefore treats:

* CPU flattening as transitional,
* Vulkan compositing as optional future infrastructure,
* not immediate scope.

---

## Constraint 3 — Metal Is Foundational Infrastructure

Metal is increasingly assumed by:

* SwiftUI,
* WebKit,
* AVFoundation,
* modern rendering stacks.

A full Metal translation layer is beyond initial platform scope.

Therefore:

* Metal-heavy applications default toward Tier 4B.

This is a deliberate sustainability decision.

---

# Component Responsibilities

```COMPONENT OWNERSHIP DIAGRAM
┌────────────────────┬─────────────────────────────────────────────┐
│ Component          │ Responsibility                              │
├────────────────────┼─────────────────────────────────────────────┤
│ macrun             │ App orchestration and tier selection        │
│ compat-db          │ Compatibility metadata and policies         │
│ Darling runtime    │ Mach-O loading and syscall compatibility    │
│ Runtime shims      │ Electron/Tauri platform adaptation          │
│ Wayland backend    │ Native Linux window presentation            │
│ ARM64 translator   │ Apple Silicon binary execution              │
│ Host proxy         │ VM window streaming and Linux UX bridge     │
│ Bridge daemon      │ macOS-side capture and event injection      │
│ VM lifecycle mgr   │ Start/suspend/resume VM orchestration       │
└────────────────────┴─────────────────────────────────────────────┘
```

---

# VM-Assisted Architecture

## Design Philosophy

The VM subsystem is:

* app-centric,
* not desktop-centric.

The host Linux environment remains primary.

macOS exists only as:

* execution substrate.

---

## Host Responsibilities

### Host Proxy

Responsibilities:

* Wayland windows
* framebuffer presentation
* input forwarding
* hotkey registration
* clipboard sync
* notification bridge
* audio routing
* VM lifecycle management

### Performance Targets

| Operation             | Target        |
| --------------------- | ------------- |
| Warm launch           | < 200ms       |
| Clipboard sync        | < 50ms        |
| Window resize latency | < 16ms        |
| Hotkey summon         | Instantaneous |

---

## Guest Responsibilities

### Bridge Daemon

Responsibilities:

* window enumeration
* framebuffer capture
* event injection
* notification forwarding
* accessibility permission management

### Capture Strategy

Per-window capture:

* avoids full desktop streaming,
* enables Linux-native window management,
* allows compositor integration.

---

# Security and Trust Boundaries

```Trust Boundary Diagram
┌─────────────────────────────────────────────────────────────┐
│                    Linux Host (Trusted)                    │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  macrun                                                     │
│  compat-db                                                  │
│  Host proxy                                                 │
│  Wayland integration                                        │
│                                                             │
└───────────────────────┬─────────────────────────────────────┘
                        │ IPC / Streams
                        ▼
┌─────────────────────────────────────────────────────────────┐
│              Compatibility Runtime Boundary                 │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  Darling                                                    │
│  Runtime shims                                              │
│  Translated app binaries                                    │
│                                                             │
└───────────────────────┬─────────────────────────────────────┘
                        │ VirtIO / Shared FS
                        ▼
┌─────────────────────────────────────────────────────────────┐
│                     macOS Guest VM                          │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  Bridge daemon                                              │
│  macOS apps                                                 │
│  Accessibility APIs                                         │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```


---

# Compatibility Database

The compatibility database is a core product component.

Without institutionalized compatibility knowledge:

* the platform remains a developer experiment.

The database tracks:

* bundle identifiers
* execution tier
* known issues
* workarounds
* required flags
* verified Linux distros
* macOS guest requirements
* performance characteristics

Modeled after:

* ProtonDB
* Wine AppDB

---

# Strategic Scope Management

## Explicit Non-Goals

The platform intentionally does NOT pursue:

* full Cocoa parity
* complete private API support
* kernel extension support
* macOS desktop replacement
* full SIP semantics
* App Store integration
* iCloud parity
* gaming optimization

These are existential scope traps.

---

# Development Roadmap

```
Phase 1 ──────────────────────────────────────────────────────
Electron • Tauri simple mode • compat-db • launcher tooling

Phase 2 ──────────────────────────────────────────────────────
CLI runtime • ARM64 translation • persistence • networking

Phase 3 ──────────────────────────────────────────────────────
Lightweight Cocoa • clipboard • media • AppKit-lite support

Phase 4 ──────────────────────────────────────────────────────
VM maturity • hotkeys • low-latency streaming • polished UX
```


## Phase 1 — Immediate Utility

Deliver:

* Electron support
* Tauri simple mode
* launcher tooling
* compat-db
* native Linux UX

This phase alone may satisfy the majority of users.

---

## Phase 2 — Developer Runtime

Deliver:

* reliable CLI compatibility
* Homebrew workflows
* Swift tooling
* ARM64 translation
* persistence systems
* networking/keychain integration

---

## Phase 3 — Lightweight Cocoa

Deliver:

* simple AppKit support
* clipboard bridging
* text rendering
* basic media playback
* lightweight GUI utilities

Avoid:

* deep SwiftUI correctness
* advanced CoreAnimation fidelity

---

## Phase 4 — VM Platform Maturity

Deliver:

* polished Tier 4B UX
* global hotkey integration
* seamless launcher workflows
* low-latency streaming
* stable window management

This phase becomes strategically more important than full Cocoa parity.

---

# Final Strategic Position

The project succeeds if:

* Linux users can run the macOS applications they actually care about,
* with acceptable performance,
* through execution strategies matched to application categories,
* without requiring full macOS reimplementation.

The project fails if it attempts:

* infinite framework parity,
* complete SwiftUI fidelity,
* or “macOS on Linux” purity.

The sustainable path is:

* hybrid execution,
* utility-first engineering,
* and ruthless scope control.

---

# Architectural Invariants

The following invariants are non-negotiable engineering constraints that govern all future implementation decisions.

Violating these invariants requires explicit architectural review.

---

## Invariant 1 — Linux Is the Primary Environment

Linux is always the primary operating environment.

macOS execution substrates:

* Darling,
* translated runtimes,
* or VMs

exist only to execute macOS applications.

The project does not attempt to recreate:

* the macOS desktop,
* Finder,
* Dock,
* or the broader macOS user environment.

All UX integration targets native Linux behavior.

---

## Invariant 2 — Execution Strategy Must Match App Category

The platform does not pursue universal execution mechanisms.

Applications must execute through the lowest-complexity viable strategy:

| App Type                        | Preferred Strategy          |
| ------------------------------- | --------------------------- |
| Electron                        | Native runtime substitution |
| Tauri                           | Hybrid bridge               |
| CLI                             | Darling                     |
| Lightweight AppKit              | Cocoa-lite compatibility    |
| SwiftUI-heavy/system-integrated | VM-assisted execution       |

No subsystem may force:

* full compatibility-layer execution
  when:
* runtime substitution,
* bridging,
* or VM-assisted execution

would achieve the same user outcome at lower engineering cost.

---

## Invariant 3 — VM-Assisted Execution Is First-Class

Tier 4B is not:

* fallback behavior,
* temporary compatibility,
* or “failure mode.”

It is a permanent architectural pillar.

The system is explicitly designed around hybrid execution.

---

## Invariant 4 — User Experience Overrides Purity

Correct user-facing behavior is prioritized over:

* implementation purity,
* framework completeness,
* or architectural elegance.

Examples:

* runtime swapping is preferred over emulation,
* stubbing is preferred over partial broken implementations,
* VM execution is preferred over impossible compatibility work.

---

## Invariant 5 — Unsupported Must Be Explicit

The platform must clearly distinguish:

* unsupported,
* degraded,
* partial,
* and broken.

Silent failure is unacceptable.

All launch paths must produce explicit compatibility state.

---

## Invariant 6 — Scope Is Intentionally Constrained

The project intentionally avoids:

* full macOS parity,
* full SwiftUI fidelity,
* complete private API support,
* kernel extension compatibility,
* hypervisor support,
* and App Store semantics.

These are architectural non-goals.

Subsystems that implicitly expand toward those goals must be redesigned or constrained.

---

# Failure & Degradation Philosophy

The platform is designed around controlled degradation rather than binary compatibility success/failure.

Applications may launch in:

* full compatibility mode,
* degraded compatibility mode,
* partial functionality mode,
* or explicit unsupported mode.

The system must always prefer:

* visible limitation
  over:
* undefined behavior.

---

# Compatibility States

| State       | Meaning                                           |
| ----------- | ------------------------------------------------- |
| Verified    | Fully tested and supported                        |
| Functional  | Core workflows operate correctly                  |
| Partial     | App launches but some subsystems unavailable      |
| Degraded    | Major features unavailable but app remains usable |
| Unsupported | Known architectural incompatibility               |
| Broken      | Unexpected failure or regression                  |

---

# Degradation Rules

## Runtime Substitution

If:

* Electron runtime mismatch occurs

Then:

* fall back to closest compatible runtime,
* warn user,
* log compatibility state.

---

## Tauri Simple Mode

If backend bridge unavailable:

* frontend UI may still launch,
* unsupported backend calls return explicit platform errors,
* application must not hang indefinitely.

---

## Cocoa Compatibility

If unsupported CoreAnimation features appear:

* compositor may flatten layers,
* animations may degrade,
* layout correctness takes priority over animation fidelity.

---

## VM-Assisted Execution

If VM unavailable:

* system may offer VM boot prompt,
* launch enters “pending” state,
* app launch never silently fails.

If host lacks virtualization:

* Tier 4B apps become explicitly unsupported.

---

# Stubbing Philosophy

Stubs are acceptable when:

* they preserve application startup,
* preserve core workflows,
* and avoid misleading behavior.

Examples:

* CloudKit unavailable
* Sparkle disabled
* TCC always granted
* unsupported APIs returning explicit capability errors

Partially-correct fake implementations are discouraged if they create inconsistent runtime behavior.

---

# Logging & Diagnostics

All compatibility failures must emit:

* subsystem identifier,
* capability state,
* execution tier,
* failure classification,
* and remediation guidance.

The platform must optimize for diagnosability.

---

# Capability Detection Engine

The Capability Detection Engine determines:

* execution tier,
* required runtime features,
* degradation risk,
* and compatibility policy.

This subsystem is strategically critical.

Incorrect classification causes:

* execution instability,
* unnecessary VM escalation,
* or avoidable compatibility failures.

---

# Detection Pipeline

## Stage 1 — Bundle Analysis

Inspect:

* `Info.plist`
* Mach-O headers
* embedded frameworks
* entitlements
* linked libraries
* bundle resources

---

## Stage 2 — Framework Fingerprinting

Detect:

* Electron
* Tauri
* Wails
* AppKit
* SwiftUI
* Metal
* Accessibility APIs
* Hypervisor.framework
* XPC services
* private framework usage

---

## Stage 3 — Architecture Analysis

Determine:

* x86-64
* ARM64-only
* universal binary
* Rosetta assumptions

---

## Stage 4 — Capability Scoring

Applications receive weighted compatibility scores across dimensions:

| Capability          | Impact   |
| ------------------- | -------- |
| SwiftUI dependency  | High     |
| Metal dependency    | High     |
| Accessibility hooks | Critical |
| XPC complexity      | Medium   |
| CoreData usage      | Medium   |
| Electron runtime    | Low      |
| WKWebView usage     | Medium   |

---

## Stage 5 — Execution Strategy Resolution

The engine resolves:

* preferred tier,
* fallback tier,
* VM requirement,
* degradation expectations,
* compatibility warnings.

---

# Detection Outcomes

| Result              | Meaning  |
| ------------------- | -------- |
| Native substitution | Tier 0   |
| Darling-compatible  | Tier 1–3 |
| VM recommended      | Tier 4B  |
| Unsupported         | Tier 4   |

---

# Detection Database

Detection signatures are stored in:

* compat-db,
* runtime manifests,
* framework signature maps,
* and heuristic rule sets.

The system improves classification accuracy over time through telemetry and compatibility reports.

---

# Interface Contracts

Subsystem communication occurs only through explicit contracts.

No subsystem may directly depend on:

* undocumented internal behavior,
* shared mutable state,
* or implicit execution assumptions.

---

# macrun Contract

Inputs:

* `.app`
* `.dmg`
* Mach-O binary

Outputs:

* execution strategy,
* runtime configuration,
* compatibility state,
* launch orchestration.

macrun never:

* implements rendering,
* performs translation,
* or owns compositor logic.

---

# compat-db Contract

Responsibilities:

* compatibility metadata,
* known issues,
* execution recommendations,
* degradation policies,
* verification states.

compat-db never:

* launches applications,
* performs runtime analysis,
* or owns execution logic.

---

# Host Proxy Contract

Responsibilities:

* Wayland surfaces,
* clipboard bridge,
* notifications,
* input forwarding,
* VM integration.

The host proxy never:

* interprets macOS APIs,
* performs Mach emulation,
* or manages Cocoa behavior.

---

# Bridge Daemon Contract

Responsibilities:

* window capture,
* event injection,
* guest-side integration.

The bridge daemon never:

* manages Linux compositor state,
* performs Linux desktop integration,
* or owns execution policy.

---

# Runtime Shim Contract

Responsibilities:

* Electron adaptation,
* Tauri adaptation,
* API substitution.

Runtime shims never:

* emulate macOS kernels,
* implement full Cocoa semantics,
* or bypass execution-tier selection.

---

# Translation Layer Contract

Responsibilities:

* ARM64 instruction translation,
* binary execution compatibility.

Translation layers never:

* emulate macOS APIs,
* implement graphics stacks,
* or manage user interaction.

---

# Phase Exit Criteria

A development phase is complete only when:

* defined technical goals,
* compatibility targets,
* and UX expectations

are satisfied.

---

# Phase 1 Exit Criteria

The following must work reliably:

* Cursor
* Claude Desktop
* Windsurf
* VS Code derivatives
* Obsidian

Requirements:

* launch correctly,
* persist settings,
* support clipboard,
* support notifications,
* support filesystem integration,
* survive restart.

---

# Phase 2 Exit Criteria

The following workflows must function:

* Homebrew install flows
* Swift package compilation
* Git workflows
* Python tooling
* ARM64-only CLI execution

Requirements:

* stable process execution,
* correct filesystem behavior,
* persistent preferences,
* reliable networking.

---

# Phase 3 Exit Criteria

At least:

* 3–5 real lightweight Cocoa applications

must:

* launch,
* render correctly,
* handle clipboard,
* play media,
* and remain stable during interaction.

---

# Phase 4 Exit Criteria

Tier 4B applications:

* Raycast
* Alfred

must:

* launch with native-feeling UX,
* support global hotkeys,
* render as Wayland windows,
* support clipboard sync,
* and survive VM suspend/resume.

---

# Regression Policy

No phase may:

* significantly regress earlier validated workflows.

Regression testing becomes mandatory after Phase 1 stabilization.