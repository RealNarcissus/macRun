# Roadmap

This roadmap defines the current development direction for the macOS Runtime Platform for Linux.

The roadmap is intentionally capability-driven rather than release-driven.

The project prioritizes:

* sustainable architecture,
* execution correctness,
* and compatibility utility

over rapid feature accumulation.

---

# Guiding Principles

Development priorities are governed by the following rules:

1. Deliver the highest user utility earliest
2. Prefer runtime substitution over emulation when viable
3. Prefer explicit degradation over undefined behavior
4. Keep Linux as the primary user environment
5. Avoid infinite compatibility scope

---

# Phase 0 — Architecture and Infrastructure

Status: In Progress

Objectives:

* finalize execution model
* define subsystem contracts
* establish capability detection architecture
* define VM-assisted execution model
* formalize compatibility policies
* harden architectural invariants

Deliverables:

* canonical architecture RFC
* execution-tier model
* compatibility-state definitions
* subsystem ownership boundaries
* roadmap and governance structure

Exit Criteria:

* architecture stable enough for implementation
* subsystem responsibilities clearly defined
* no unresolved foundational execution contradictions

---

# Phase 1 — Runtime Substitution Layer

Priority: Highest

Goal:
Run modern Electron/Tauri desktop applications with Linux-native UX.

Focus Areas:

* Electron runtime mapping
* `.asar` extraction
* platform API shims
* Tauri frontend execution
* application launcher (`macrun`)
* compatibility database infrastructure

Target Applications:

* Cursor
* Claude Desktop
* Windsurf
* VS Code derivatives
* Obsidian

Expected Outcome:
Linux users can run major AI/developer desktop applications without perceiving emulation.

Exit Criteria:

* applications launch reliably
* filesystem integration works
* clipboard and notifications function
* settings persist correctly
* launch stability acceptable

---

# Phase 2 — Developer Runtime Compatibility

Goal:
Provide reliable CLI/runtime compatibility for macOS developer tooling.

Focus Areas:

* Darling hardening
* Mach-O execution stability
* Homebrew workflows
* Swift tooling
* ARM64 translation
* preference persistence
* networking integration
* keychain bridging

Target Workflows:

* Homebrew package installation
* Swift package compilation
* Git tooling
* Python tooling
* ARM64-only CLI binaries

Exit Criteria:

* stable CLI execution
* persistent runtime environments
* functional networking
* acceptable translation overhead
* regression stability maintained

---

# Phase 3 — Lightweight Cocoa Compatibility

Goal:
Support lightweight native AppKit applications.

Focus Areas:

* Wayland backend integration
* lightweight Cocoa compatibility
* CoreText integration
* clipboard bridging
* basic media playback
* CoreAnimation flattening

Constraints:
This phase intentionally avoids:

* deep SwiftUI correctness
* advanced Metal integration
* full compositor parity

Target Applications:

* lightweight utilities
* note-taking apps
* simple AppKit tools

Exit Criteria:

* applications render correctly
* interaction stability acceptable
* clipboard integration functional
* media playback operational
* application crashes minimized

---

# Phase 4 — VM-Assisted Runtime Maturity

Goal:
Deliver polished execution for deeply integrated macOS applications.

Focus Areas:

* VM lifecycle management
* per-window streaming
* hotkey forwarding
* clipboard synchronization
* notification bridging
* low-latency rendering
* suspend/resume reliability

Target Applications:

* Raycast
* Alfred
* SwiftUI-heavy utilities

Expected Outcome:
Applications behave as native-feeling Linux windows while executing inside macOS guest environments.

Exit Criteria:

* warm launch latency acceptable
* global hotkeys reliable
* VM suspend/resume stable
* low-latency interaction achieved
* host integration seamless

---

# Long-Term Research Areas

The following areas remain exploratory:

* Vulkan-backed CoreAnimation
* Metal translation layers
* advanced SwiftUI compatibility
* WKWebView routing
* compositor acceleration
* complex text rendering
* cross-architecture optimization

These are not required for initial platform success.

---

# Compatibility Philosophy

The platform is designed around:

* controlled degradation,
  not:
* binary compatibility assumptions.

Applications may operate in:

* verified,
* functional,
* degraded,
* partial,
* or unsupported states.

The system prioritizes:

* explicit compatibility reporting,
* diagnosability,
* and stable user workflows.

---

# Success Criteria

The project succeeds if Linux users can:

* run the macOS applications they actually need,
* with acceptable performance,
* through execution strategies appropriate to each application category,
* without requiring complete macOS reimplementation.

The project fails if it expands toward:

* infinite framework parity,
* unconstrained compatibility scope,
* or full operating-system replication.
