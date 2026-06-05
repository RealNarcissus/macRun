# Compatibility Database (compat-db) Architecture

This document formalizes the specifications, schemas, validation invariants, and evolution strategies of the Compatibility Database (`compat-db`) for the macOS Runtime Platform for Linux.

---

## 1. Role and Design Philosophy

The Compatibility Database (`compat-db`) is a declarative, read-only metadata engine. It acts as the central platform authority on application compatibility, isolating runtime execution policies from core orchestration logic.

### Structural Separation
Rather than hardcoding application-specific overrides, shims, or flags inside the C++ orchestrator source files, the orchestrator queries `compat-db` using the target application's bundle identifier. The database returns compatibility records that dynamically instruct adapters how to configure execution parameters.

This separation guarantees:
- **Zero-Recompile Updates**: Profiles for new applications, updated workarounds, and tier configurations can be deployed by shipping updated JSON metadata files without recompiling core binaries.
- **Deterministic Selection**: Decoupled database lookup prevents race conditions and ensures repeatable configurations for given bundle profiles.

---

## 2. Schema Specification

The database stores metadata as independent JSON records under the [reports/](file:///home/charleton/Desktop/agentProjects/Codex/MacOS%20Emulator/compat-db/reports/) directory. Each file conforms to the schema defined in [record.schema.json](file:///home/charleton/Desktop/agentProjects/Codex/MacOS%20Emulator/compat-db/schema/record.schema.json) and maps to C++ structures inside [types.hpp](file:///home/charleton/Desktop/agentProjects/Codex/MacOS%20Emulator/compat-db/include/compatdb/types.hpp).

### Required Schema Fields
- `schema_version`: Semantic version string of the schema file (e.g., `1.0.0`).
- `record_id`: Unique lexicographically sortable report identifier (e.g., `vscode`).
- `bundle_identifier`: Canonical macOS bundle ID (e.g., `com.microsoft.VSCode`).
- `application_name`: User-facing name of the application.
- `execution_tier`: Target execution class (`native-substitution`, `darling-compatible`, `vm-recommended`, `unsupported`).
- `compatibility_state`: Target state mapping (`verified`, `functional`, `partial`, `degraded`, `unsupported`, `broken`).
- `last_updated`: Timestamp of last validation run.

### Optional Schema Blocks
1. **`tested_on`**: An array of host Linux system details (distribution, version, kernel version, architecture) to track test coverage.
2. **`known_issues`**: Documented failure points within emulation subsystems. Each issue defines `id`, `severity` (`critical`, `high`, `medium`, `low`, `cosmetic`), `description`, and `affected_subsystem`.
3. **`workarounds`**: Recommended adapter shims. Each workpiece defines a `description`, a list of `applies_to_issues`, and the required environment parameters `requires_flags`.
4. **`macos_guest_requirements`**: VM provisioning parameters for Tier 4B streaming, including RAM sizes, disk allocations, and CPU specifications.
5. **`required_flags`**: Generic key-value pairs parsed by adapters to inject env vars and command-line parameters.

---

## 3. Validation Invariants

To prevent corrupt metadata from destabilizing runtime execution, the validator ([validator.hpp](file:///home/charleton/Desktop/agentProjects/Codex/MacOS%20Emulator/compat-db/include/compatdb/validator.hpp)) enforces strict rules during database compilation and verification:

1. **State Invariant**:
   - A compatibility record cannot declare its state as `verified` if it contains any unresolved `critical` or `high` severity known issues.
2. **Execution Tier Invariant**:
   - If the `compatibility_state` is set to `unsupported`, the `execution_tier` must also be set to `unsupported` (and vice-versa) to prevent contradictory execution paths.
3. **Record ID Uniqueness**:
   - The directory loader ([database.cpp](file:///home/charleton/Desktop/agentProjects/Codex/MacOS%20Emulator/compat-db/src/database.cpp)) sorts reports lexicographically and rejects duplicate record IDs with an explicit error to prevent non-deterministic record shadowing.

---

## 4. Schema Evolution & Compatibility

To support future additions without breaking backward compatibility:

1. **Schema Versioning**:
   - All records are version-pinned via `schema_version`.
   - The platform parser evaluates the version and falls back to default values for missing new fields instead of failing.
2. **Additive Schema Updates**:
   - New properties must be added as optional fields. The database parser must provide default configurations (e.g. `minimum_ram_mb = 4096` if missing) to ensure old records parse cleanly.
3. **Breaking Modifications**:
   - When structural changes are unavoidable, a version bump (e.g., `2.0.0`) must be declared, and a migration utility script must convert old JSON configurations before compile time.

---

## 5. Directory and File Organization

The database files are organized under the following standard directories:

- [schema/](file:///home/charleton/Desktop/agentProjects/Codex/MacOS%20Emulator/compat-db/schema/): JSON validation schemas (`record.schema.json`).
- [reports/](file:///home/charleton/Desktop/agentProjects/Codex/MacOS%20Emulator/compat-db/reports/): Static JSON records representing tested applications (e.g., `vscode.json`, `textedit.json`).
- [manifests/](file:///home/charleton/Desktop/agentProjects/Codex/MacOS%20Emulator/compat-db/manifests/): Declarative maps specifying replacement runtime mappings (e.g. Electron versions to fetch).
- [policies/](file:///home/charleton/Desktop/agentProjects/Codex/MacOS%20Emulator/compat-db/policies/): Access policies, security rules, and compatibility profiles.
- [signatures/](file:///home/charleton/Desktop/agentProjects/Codex/MacOS%20Emulator/compat-db/signatures/): Cryptographic signatures verifying the authenticity of downloaded reports.
