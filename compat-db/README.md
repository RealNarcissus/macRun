# Compatibility Database

The compatibility database is a core product component. Without institutionalized
compatibility knowledge, the platform remains a developer experiment.

Architecture reference: docs/architecture/ARCHITECTURE_V6.md
  - "Compatibility Database" section
  - "Detection Database" section
  - "compat-db Contract"

Modeled after ProtonDB and Wine AppDB.

## Directory Layout

### `signatures/` — Application and framework detection signatures
Framework fingerprint data, heuristic rule sets, and detection manifests.
Used by the Capability Detection Engine (platform/detector/) for tier classification.

### `policies/` — Execution policies and degradation rules
Maps application categories to execution strategies, defines fallback behavior,
and encodes the degradation rules from the architecture specification.

### `reports/` — Compatibility reports and telemetry data
User-submitted compatibility reports, automated test results, and
compatibility state tracking (verified, functional, partial, degraded,
unsupported, broken).

### `manifests/` — Runtime version manifests
Electron version mappings, Tauri compatibility matrices, Darling runtime
configurations, and platform dependency declarations.

## Compatibility States

| State       | Meaning                                           |
| ----------- | ------------------------------------------------- |
| Verified    | Fully tested and supported                        |
| Functional  | Core workflows operate correctly                  |
| Partial     | App launches but some subsystems unavailable      |
| Degraded    | Major features unavailable but app remains usable |
| Unsupported | Known architectural incompatibility               |
| Broken      | Unexpected failure or regression                  |

## Data Schema

Database entries track:
- bundle identifiers
- execution tier
- known issues
- workarounds
- required flags
- verified Linux distros
- macOS guest requirements
- performance characteristics
