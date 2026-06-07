# Compatibility Database

The compat-db is a core platform component. It is how macRun learns about
applications without requiring code changes for each new target. Without
institutionalized compatibility knowledge, the platform remains a developer experiment.

Architecture reference: `docs/architecture/ARCHITECTURE_V6.md`
  - "Compatibility Database" section
  - "Detection Database" section
  - "compat-db Contract"

Modelled after ProtonDB and Wine AppDB.

---

## Directory Layout

### `signatures/` — Application and framework detection signatures
Framework fingerprint data, heuristic rule sets, and detection manifests. Used by the
Capability Detection Engine (`platform/detector/`) for tier and architecture class
classification.

### `policies/` — Execution policies and degradation rules
Maps application categories to execution strategies, defines fallback behaviour, and
encodes the degradation rules from the architecture specification.

### `reports/` — Compatibility reports
Validated compatibility results for tested applications. Each report is a JSON document
following the schema below. See `reports/` for working examples (claude-desktop.json,
codex.json, obsidian.json, cursor.json).

### `manifests/` — Runtime version manifests
Electron version mappings, Tauri compatibility matrices, Darling runtime configurations,
and platform dependency declarations.

---

## Compatibility States

| State       | Meaning                                           |
|-------------|---------------------------------------------------|
| Verified    | Fully tested and supported                        |
| Functional  | Core workflows operate correctly                  |
| Partial     | App launches but some subsystems unavailable      |
| Degraded    | Major features unavailable but app remains usable |
| Unsupported | Known architectural incompatibility               |
| Broken      | Unexpected failure or regression                  |

---

## Report Schema

Each file in `reports/` follows this JSON schema:

```json
{
  "schema_version": "1.0.0",
  "record_id": "com.example.app",
  "bundle_identifier": "com.example.app",
  "application_name": "App Name",
  "application_version": "1.0.0",

  "execution_tier": "native-substitution",
  "compatibility_state": "functional",
  "architecture_class": "class_a_self_contained",

  "last_updated": "2026-06-07T00:00:00Z",

  "tested_on": [
    {
      "distribution": "Ubuntu",
      "version": "24.04",
      "kernel_version": "6.8.0",
      "arch": "x86_64"
    }
  ],

  "known_issues": [
    {
      "id": "ISSUE-001",
      "severity": "low",
      "description": "Description of the issue",
      "affected_subsystem": "subsystem-name"
    }
  ],

  "workarounds": [
    {
      "description": "How to work around the issue",
      "applies_to_issues": ["ISSUE-001"],
      "requires_flags": {
        "ENV_VAR": "value"
      }
    }
  ],

  "degradation": {
    "category": "shimmed",
    "confidence": "functional",
    "active_shims": ["path-mapper", "clipboard-bridge"],
    "degraded_capabilities": [],
    "unsafe_bypasses": [],
    "recommended_action": "None — Tier 0 functional"
  },

  "external_processes": [],

  "critical_native_modules": [],

  "runtime_policy": {
    "preferred": ["42", "32", "28"],
    "minimum": "28",
    "validated": ["42"],
    "fallback": ["28"]
  },

  "required_flags": {
    "MACRUN_ALLOW_DARWIN_NATIVE": "1"
  },

  "tags": ["electron", "ai-tool"],

  "notes": "Free-text notes on compatibility characteristics, known issues, and resolution steps."
}
```

### Field Reference

| Field | Required | Description |
|-------|----------|-------------|
| `schema_version` | Yes | Always `"1.0.0"` for current schema |
| `record_id` | Yes | Unique identifier — typically the bundle ID |
| `bundle_identifier` | Yes | macOS bundle identifier from Info.plist |
| `application_name` | Yes | Human-readable app name |
| `application_version` | Yes | Version string of the tested build |
| `execution_tier` | Yes | `"native-substitution"`, `"darling"`, `"qemu"`, `"vm"` |
| `compatibility_state` | Yes | One of the states in the table above |
| `architecture_class` | Yes | `"class_a_self_contained"`, `"class_b_api_drift"`, `"class_c_ide_class"`, `"class_d_client_server"` |
| `last_updated` | Yes | ISO 8601 timestamp of last validation |
| `tested_on` | Yes | Array of OS/kernel/arch combinations tested |
| `known_issues` | No | Array of documented issues with severity and subsystem |
| `workarounds` | No | Steps or flags to resolve known issues |
| `degradation` | No | Degradation category and shim details |
| `external_processes` | No | Required for Class D apps — backend process declarations |
| `critical_native_modules` | No | Required for Class C/D apps — native modules needing compilation |
| `runtime_policy` | No | Electron version preferences for runtime negotiation |
| `required_flags` | No | Environment variables required for functional launch |
| `tags` | No | Searchable tags: `["electron", "ai-tool", "editor", ...]` |
| `notes` | No | Free-text summary of compatibility characteristics |

---

## Architecture Class Reference

See `docs/architecture/COMPATIBILITY_SPECTRUM.md` for the full definition. Summary:

| Class | Name | Characteristics | Examples |
|-------|------|-----------------|---------|
| `class_a_self_contained` | Self-Contained | All logic in Electron; no critical native deps | Obsidian |
| `class_b_api_drift` | API Drift | Self-contained but built against newer Electron APIs | Claude Desktop |
| `class_c_ide_class` | IDE-Class | Critical native modules (SQLite, PTY, logging) | Cursor |
| `class_d_client_server` | Client-Server | External backend binary substitution required | Codex |

---

## Contributing Compatibility Reports

If you have successfully run a macOS application through macRun, a compatibility report
is a valuable contribution — even without any code changes.

### Minimum Useful Report

You do not need to fill every field. A minimal useful contribution includes:

```json
{
  "schema_version": "1.0.0",
  "record_id": "com.example.app",
  "bundle_identifier": "com.example.app",
  "application_name": "App Name",
  "application_version": "1.0.0",
  "execution_tier": "native-substitution",
  "compatibility_state": "functional",
  "architecture_class": "class_b_api_drift",
  "last_updated": "2026-06-07T00:00:00Z",
  "tested_on": [{ "distribution": "Ubuntu", "version": "24.04", "arch": "x86_64" }],
  "notes": "Launches and runs. Required MACRUN_ALLOW_DARWIN_NATIVE=1."
}
```

### What Makes a Report Valuable

- **Application name and version** — which exact build was tested
- **Architecture class** — helps others know what to expect
- **What worked** — core workflows that are functional
- **What required workarounds** — flags, module compilations, env vars
- **What is broken or degraded** — honest status, not just what works
- **Tested OS and kernel** — distro, version, kernel, architecture

Place the file in `compat-db/reports/<bundle-id>.json` and open a PR.
