# Compatibility Database Schema

Version: 1.0.0
Architecture reference: docs/architecture/ARCHITECTURE_V6.md — "Compatibility Database" section

## Overview

The compat-db schema defines the structure for compatibility records in the macOS Runtime Platform database. Each record describes a macOS application's compatibility with the platform, including its execution tier, known issues, workarounds, and performance characteristics.

The schema is modeled after ProtonDB and Wine AppDB.

## Record Structure

### Required Fields

| Field | Type | Description |
|-------|------|-------------|
| `schema_version` | string (X.Y.Z) | Schema version this record conforms to. Current: `1.0.0`. |
| `record_id` | string | Unique record identifier. Convention: bundle identifier. |
| `bundle_identifier` | string | CFBundleIdentifier from Info.plist. Reverse-DNS format. |
| `application_name` | string | Human-readable application name. |
| `execution_tier` | enum | Recommended execution tier. See Execution Tiers below. |
| `compatibility_state` | enum | Current compatibility state. See Compatibility States below. |
| `last_updated` | string (ISO 8601) | Timestamp of last record modification. |

### Optional Fields

| Field | Type | Description |
|-------|------|-------------|
| `application_version` | string | Tested application version. |
| `tested_on` | array of TestedDistro | Linux distributions this record has been verified against. |
| `known_issues` | array of KnownIssue | Documented issues and incompatibilities. |
| `workarounds` | array of Workaround | Known workarounds for issues. |
| `required_flags` | object | Key-value launch flags required by this application. |
| `macos_guest_requirements` | object | VM requirements if Tier 4B execution is needed. |
| `performance_characteristics` | object | Observed performance data. |
| `capability_requirements` | object | Detected capability requirements from the detection engine. |
| `contributor` | object | Record contribution metadata. |
| `notes` | string | Free-form notes and context. |
| `tags` | array of strings | Searchable categorization tags. |

### Execution Tiers

| Value | Description |
|-------|-------------|
| `native-substitution` | Tier 0 — Runs via native Linux runtime swap (Electron/Tauri/Wails). |
| `darling-compatible` | Tier 1-3 — Runs via Darling compatibility layer or translation. |
| `vm-recommended` | Tier 4B — Requires macOS VM with window streaming. |
| `unsupported` | Tier 4 — Known architectural incompatibility. |

### Compatibility States

Per architecture spec (ARCHITECTURE_V6.md — "Compatibility States" table):

| Value | Meaning |
|-------|---------|
| `verified` | Fully tested and supported |
| `functional` | Core workflows operate correctly |
| `partial` | App launches but some subsystems unavailable |
| `degraded` | Major features unavailable but app remains usable |
| `unsupported` | Known architectural incompatibility |
| `broken` | Unexpected failure or regression |

### KnownIssue Object

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `id` | string | Yes | Unique issue identifier within the record. |
| `severity` | enum | No | critical, high, medium, low, cosmetic. Default: medium. |
| `description` | string | Yes | Human-readable issue description. |
| `affected_subsystem` | string | No | Subsystem affected (e.g. rendering, clipboard). |
| `reproduction_steps` | string | No | Steps to reproduce the issue. |
| `resolved_in_version` | string | No | Platform version where issue was resolved. |

### Workaround Object

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `description` | string | Yes | Workaround instructions. |
| `applies_to_issues` | array of strings | No | Issue IDs this workaround addresses. |
| `requires_flags` | array of strings | No | Launch flags required for this workaround. |

### TestedDistro Object

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `distribution` | string | Yes | Distribution name (e.g. Ubuntu, Fedora). |
| `version` | string | Yes | Distribution version string. |
| `kernel_version` | string | No | Linux kernel version. |
| `arch` | string | No | Host CPU architecture: x86_64 or aarch64. |

### MacOSGuestRequirements Object

| Field | Type | Description |
|-------|------|-------------|
| `minimum_macos_version` | string | Minimum macOS version required. |
| `recommended_macos_version` | string | Recommended macOS version. |
| `minimum_ram_mb` | integer | Minimum RAM for VM (≥2048). |
| `minimum_disk_gb` | integer | Minimum disk for VM (≥10). |
| `requires_metal` | boolean | Whether Metal GPU support is required. |
| `requires_accessibility` | boolean | Whether Accessibility API access is required. |
| `special_configuration` | string | Any special VM configuration notes. |

### PerformanceCharacteristics Object

| Field | Type | Description |
|-------|------|-------------|
| `startup_time_ms` | integer | Cold-start time in milliseconds. |
| `memory_usage_mb` | integer | Typical resident memory usage in MB. |
| `cpu_overhead` | enum | negligible, low, moderate, high, extreme. |
| `notes` | string | Additional performance notes. |

## Validation Rules

The validator enforces the following rules in addition to the schema structure:

1. **Required fields**: schema_version, record_id, bundle_identifier, application_name, execution_tier, compatibility_state, last_updated must all be present.
2. **Schema version format**: Must match X.Y.Z pattern (e.g. 1.0.0, 2.1.3).
3. **Record ID format**: Only alphanumeric characters, dots, underscores, and hyphens.
4. **Bundle identifier**: Should be reverse-DNS format (at minimum, contain dots).
5. **Enum values**: execution_tier and compatibility_state must be valid enum values.
6. **Known issues**: Each issue must have non-empty id and description.
7. **Workarounds**: Each workaround must have a non-empty description.
8. **Tested distros**: Each must have non-empty distribution and version.
9. **VM requirements**: minimum_ram_mb ≥ 2048, minimum_disk_gb ≥ 10.

### State Transition Rules

1. **Verified + critical issue**: Records in "verified" state must not have unresolved critical issues.
2. **Unsupported + tier**: Records in "unsupported" state should have "unsupported" execution tier.
3. **Metal + non-VM tier**: Records with Metal requirements on non-VM tiers should not be "verified" or "functional".

## Extending the Schema

The schema is designed for forward compatibility:

1. **Additive changes** (new fields) — bump the minor version. Validators ignore unknown fields so old records remain valid.
2. **Breaking changes** (removing fields, changing types) — bump the major version and provide migration tooling.
3. **Custom fields** — Add new properties to the schema; validators skip unrecognized keys rather than rejecting them.

## Example

```json
{
  "schema_version": "1.0.0",
  "record_id": "com.example.app",
  "bundle_identifier": "com.example.app",
  "application_name": "ExampleApp",
  "application_version": "2.0.0",
  "execution_tier": "native-substitution",
  "compatibility_state": "verified",
  "last_updated": "2026-06-04T00:00:00Z",
  "tested_on": [
    {
      "distribution": "Ubuntu",
      "version": "24.04",
      "arch": "x86_64"
    }
  ],
  "known_issues": [],
  "workarounds": [],
  "required_flags": {},
  "tags": ["electron", "verified"],
  "notes": "Fully functional via runtime substitution."
}
```
