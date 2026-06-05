// Schema Validator for Compatibility Database Records
// Architecture reference: docs/architecture/ARCHITECTURE_V6.md
//   — "Compatibility Database" section
//   — "Compatibility States" table
//
// Validates CompatibilityRecord against the JSON schema (record.schema.json).
// Uses a state-machine approach: each validation rule is a discrete check
// that produces a deterministic pass/fail result with structured error output.
#pragma once
#include <compatdb/types.hpp>
#include <vector>
#include <string>

namespace compatdb {

struct ValidationError {
    std::string field;
    std::string message;
    enum class Level { Error, Warning } level = Level::Error;
};

struct ValidationResult {
    bool valid = false;
    std::vector<ValidationError> errors;

    explicit operator bool() const { return valid; }
};

// Validates a compatibility record against the schema.
// Returns structured errors for every violation — no silent failures.
ValidationResult validate_record(const CompatibilityRecord& record);

// State-transition validation: certain state transitions are invalid.
// e.g. "verified" with unresolved "critical" issues is a violation.
ValidationResult validate_state_transitions(const CompatibilityRecord& record);

// Full validation: schema + state transitions
ValidationResult validate_full(const CompatibilityRecord& record);

} // namespace compatdb
