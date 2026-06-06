# Contributing to macRun

macRun is a governed runtime compatibility platform for macOS applications on Linux.
Before contributing, read this document fully. The architecture-first discipline is
not bureaucracy — it is what keeps the project from becoming an undocumented pile of
hacks.

---

## Current Project State

Phase 0 through Phase 3 are complete. macRun is an early-stage but functional runtime
platform. Tier 0 (Electron substitution) works. Claude Desktop, Codex, Obsidian, and
Cursor run on Linux from unmodified macOS app bundles.

**Phase 4 is the active development frontier.** This is where contributions have the
most immediate impact:

- Phase 4A: Native module resolution pipeline (active)
- Phase 4B: Electron runtime matrix and version negotiation (next)
- Phase 4C: Multi-view rendering reliability for VSCode-class apps (next)
- Phase 4D: Linux backend substitution framework (planned)

If you want to contribute to Darling integration, QEMU hybrid execution, or VM-assisted
streaming, those phases are future and not yet in active design. Architecture questions
about those tiers are welcome; implementation PRs are premature.

See ROADMAP.md for the full phase breakdown and exit criteria.

---

## Prerequisites

- CMake 3.20+
- C++20 compiler (GCC 12+ or Clang 16+)
- Rust toolchain (required for some Phase 4 components)
- Familiarity with Electron internals is strongly recommended for Phase 4 work
- Familiarity with Mach-O and Darwin syscall ABI is required for Phase 5+ work

---

## Building

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

## Running Tests

```bash
cmake --build build --target test
```

---

## Architecture First

All contributions must be traceable to the canonical architecture documents. The
architecture is not a suggestion — it is the contract the project is built on.

The non-negotiable architectural invariants are:

1. Linux is the primary environment. macOS exists only as an execution substrate.
2. Execution strategy must match application category. No universal mechanisms.
3. VM-assisted execution is first-class, not a fallback.
4. User experience overrides architectural purity. Correctness beats elegance.
5. Unsupported states must be explicit. Silent failure is unacceptable.
6. Scope is intentionally constrained. Infinite parity is not the goal.

Contributions that violate these invariants will be rejected or require architectural
review before proceeding.

The primary architectural reference is `docs/architecture/ARCHITECTURE_V6.md`. If your
change conflicts with an architectural invariant, the invariant wins unless you make the
case for a formal revision.

---

## Repository Structure

```
docs/           Architecture, design, protocols, compatibility documentation
compat-db/      Compatibility metadata: signatures, policies, validation reports
platform/       Application orchestration and capability detection pipeline
runtime/        Execution backends: Electron adapter, Darling, shims, translation
vm/             macOS guest integration: bridge daemon, capture, lifecycle
host/           Linux-side windowing and UX integration (Wayland, clipboard, audio)
tooling/        Development utilities, macrun-cli, diagnostic tools
tests/          Unit, integration, compatibility, and performance tests
third_party/    Vendored dependencies
scratch/        Experimental work, not subject to architecture invariants
```

See `docs/OWNERSHIP.md` for the full subsystem ownership map and allowed
cross-boundary interactions.

---

## Contribution Workflow

1. Identify which phase and subsystem your change belongs to
2. Check `docs/OWNERSHIP.md` for the owning subsystem and its boundaries
3. Confirm the change is within an active or planned phase (see ROADMAP.md)
4. Ensure your change respects all trust boundaries and subsystem contracts
5. Interface contract changes require protocol documentation updates
6. All changes must maintain build cleanliness
7. Regression against earlier phase exit criteria is not acceptable

---

## Module Boundaries

Subsystems communicate only through explicit contracts defined in public headers or
protocol documents. No subsystem may depend on undocumented internals, shared mutable
state, or implicit execution assumptions.

The no-orchestration policy applies to all substrate adapters: adapters handle
environment setup, execution, and local diagnostics only. Classification, policy
decisions, and global orchestration live in the orchestrator, not in adapters.

---

## Code Conventions

- C++20 throughout (C++17 minimum for a small number of legacy components)
- Each subsystem builds as a separate CMake library
- Public interfaces live in headers at the subsystem root
- Implementation details stay in subsystem-internal source files
- Placeholder files mark unimplemented subsystems — do not remove them without
  architectural justification

---

## Where to Contribute Right Now

### High impact, active phase (Phase 4A)

- Native module rebuild pipelines for better-sqlite3, @vscode/sqlite3, node-pty
- ABI compatibility verification tooling
- Module resolution registry design and implementation
- Rebuild automation targeting specific Electron versions via @electron/rebuild

### High impact, coming soon (Phase 4B/4C)

- Electron version capability negotiation
- BrowserView / WebContentsView orchestration under substitution
- GPU acceleration edge case investigation and fixes
- Renderer diagnostic tooling

### Always useful

- Compatibility database entries for new applications (compat-db/signatures)
- Validation reports for applications you have tested
- Mach-O analysis tooling improvements (tooling/inspector)
- Documentation corrections and clarifications

### Not yet in scope

- Darling integration implementation (Phase 5 — future)
- QEMU hybrid execution (Phase 7 — future)
- VM-assisted streaming (Phase 8 — future)
- GUI launcher or packaging (Phase 9 — future)

---

## Compatibility Database Contributions

The compat-db is how macRun learns about new applications without code changes. If you
have successfully run a macOS application through macRun, a compatibility report is
a valuable contribution even without any code.

A useful report includes: application name and version, Electron version, which
compatibility class it falls into (A/B/C/D), what modules required substitution, what
normalization was needed, and any remaining issues. See compat-db/README.md for the
schema.

---

## Questions Before Contributing

Before opening a PR, confirm you can answer:

- Which execution tier does this change affect?
- Which trust boundary does this change operate in?
- Does this change respect the relevant subsystem contract?
- Could this change cause regression in an earlier phase?
- Is this phase currently active or planned in the roadmap?

If you are unsure about any of these, open an issue first.
