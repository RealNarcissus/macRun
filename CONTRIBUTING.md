# Contributing

This document describes how to contribute to the macOS Runtime Platform for Linux.

Architecture reference: docs/architecture/ARCHITECTURE_V6.md
  - "Architectural Invariants" (non-negotiable constraints)
  - "Interface Contracts" section
  - "Phase Exit Criteria"
  - "Regression Policy"

## Architecture First

All contributions must be traceable to sections in docs/architecture/ARCHITECTURE_V6.md.
This is the canonical and authoritative source. If a change conflicts with an
architectural invariant, the invariant wins unless explicitly revised.

## Getting Started

### Prerequisites
- CMake 3.20+
- C++20 compiler (GCC 12+ or Clang 16+)
- Rust toolchain (for components written in Rust)

### Building
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

### Running Tests
```bash
cmake --build build --target test
```

## Repository Structure

```
docs/           Architecture, design, protocols, compatibility docs
compat-db/      Compatibility metadata (signatures, policies, reports)
platform/       Application orchestration and capability detection
runtime/        Execution backends (Darling, shims, translation, Cocoa)
vm/             macOS guest integration (bridge, capture, lifecycle)
host/           Linux-side windowing and UX integration
tooling/        Development and diagnostic utilities
tests/          Unit, integration, compatibility, and performance tests
third_party/    Vendored dependencies
```

See docs/OWNERSHIP.md for the full subsystem ownership map.

## Contribution Workflow

1. Identify the architecture section your change relates to
2. Check docs/OWNERSHIP.md for the owning subsystem
3. Ensure your change respects trust boundaries
4. Interface contract changes require protocol documentation
5. All changes must maintain build cleanliness
6. Phase exit criteria regression is prohibited

## Module Boundaries

Subsystems communicate only through explicit contracts (headers/protocols).
No subsystem may directly depend on undocumented internals, shared mutable
state, or implicit execution assumptions.

See docs/OWNERSHIP.md for allowed and disallowed cross-boundary interactions.

## Code Conventions

- C++20 standard throughout (C++17 minimum for some components)
- Each subsystem builds as a separate CMake library
- Public interfaces go in headers at the subsystem root
- Implementation details stay in subsystem-internal source files
- Placeholder files mark unimplemented subsystems

## Architecture Invariants

Six non-negotiable constraints from ARCHITECTURE_V6.md:

1. Linux is the primary environment — macOS exists only as execution substrate
2. Execution strategy must match app category — no universal mechanisms
3. VM-assisted execution is first-class — not a fallback
4. User experience overrides purity — correctness > elegance
5. Unsupported must be explicit — silent failure unacceptable
6. Scope is intentionally constrained — avoid infinite parity

Contributions that violate these invariants will be rejected or require
architectural review.

## Current Development Phase

Phase 0 — Architecture and Infrastructure (in progress).

Active focus: architecture stabilization, subsystem contracts,
infrastructure scaffolding. No runtime implementation yet.

## Priority Contribution Areas

When implementation begins:
- Electron runtime mapping (platform/shims)
- Capability detection signatures (compat-db/signatures)
- Mach-O analysis tooling (tooling/inspector)
- Wayland integration (host/wayland)
- VM streaming protocols (docs/protocols)
- Compatibility testing (tests/compatibility)

## Questions

Before contributing, ensure you understand:
- Which execution tier(s) your change affects
- Which trust boundary your change operates in
- Whether your change respects the relevant subsystem contract
- Whether your change could degrade earlier phase exit criteria
