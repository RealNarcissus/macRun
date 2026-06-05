# Tests

Architecture reference: docs/architecture/ARCHITECTURE_V6.md
  - "Phase Exit Criteria" sections (Phases 1-4)
  - "Regression Policy"

## Test Categories

### unit — Subsystem-Isolated Tests
Tests must not depend on other subsystems' internals.
Only interface contracts (public headers/protocols) are allowed dependencies.

### integration — Contract-Level Integration Tests
Validates cross-subsystem communication through explicit contracts.
No subsystem may directly depend on undocumented internals.

### compatibility — Application Compatibility Validation
Validates target applications against exit criteria:
- Phase 1: Cursor, Claude Desktop, Windsurf, VS Code derivatives, Obsidian
- Phase 2: Homebrew, Swift tools, CLI binaries
- Phase 3: Lightweight Cocoa applications (3-5 real apps)
- Phase 4: Raycast, Alfred via Tier 4B

### performance — Latency and Throughput Benchmarks
Validates performance targets: warm launch, clipboard sync,
window resize latency, hotkey summon, translation overhead.

### fixtures — Test Data
Sample .app bundles, DMGs, Mach-O binaries, and test manifests.
No production logic — test data only.
