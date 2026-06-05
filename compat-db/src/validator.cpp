// Schema Validator Implementation
// Validates CompatibilityRecord against the architecture's compatibility model.
// Architecture reference: docs/architecture/ARCHITECTURE_V6.md
//   — "Compatibility States" table
//   — "Failure & Degradation Philosophy"
//   — Invariant 5: Unsupported Must Be Explicit
#include <compatdb/validator.hpp>
#include <algorithm>

namespace compatdb {

using VLevel = ValidationError::Level;

ValidationResult validate_record(const CompatibilityRecord& rec) {
    ValidationResult result;
    result.valid = true;

    auto err = [&](const std::string& field, const std::string& msg, VLevel level = VLevel::Error) {
        result.errors.push_back({field, msg, level});
        if (level == VLevel::Error) result.valid = false;
    };

    // Required field checks
    if (rec.schema_version.empty())
        err("schema_version", "Missing required field: schema_version");
    if (rec.record_id.empty())
        err("record_id", "Missing required field: record_id");
    if (rec.bundle_identifier.empty())
        err("bundle_identifier", "Missing required field: bundle_identifier");
    if (rec.application_name.empty())
        err("application_name", "Missing required field: application_name");
    if (rec.last_updated.empty())
        err("last_updated", "Missing required field: last_updated");

    // Schema version format: X.Y.Z
    if (!rec.schema_version.empty()) {
        int dots = 0;
        bool valid_ver = true;
        for (char c : rec.schema_version) {
            if (c == '.') dots++;
            else if (c < '0' || c > '9') valid_ver = false;
        }
        if (dots != 2 || !valid_ver)
            err("schema_version", "schema_version must be in X.Y.Z format");
    }

    // Record ID format: alphanumeric with ._- allowed
    if (!rec.record_id.empty()) {
        for (char c : rec.record_id) {
            if (!std::isalnum(static_cast<unsigned char>(c)) && c != '.' && c != '_' && c != '-') {
                err("record_id", "record_id contains invalid character '" + std::string(1, c) + "'");
                break;
            }
        }
    }

    // Bundle identifier should follow reverse-DNS convention (at minimum, contain dots)
    if (!rec.bundle_identifier.empty() && rec.bundle_identifier.find('.') == std::string::npos) {
        err("bundle_identifier", "Bundle identifier should be reverse-DNS format (missing '.')",
            VLevel::Warning);
    }

    // Execution tier must be valid
    std::string tier_str = std::string(tier_to_string(rec.execution_tier));
    if (tier_str == "unknown")
        err("execution_tier", "Invalid execution_tier value");

    // Compatibility state must be valid
    std::string state_str = std::string(state_to_string(rec.compatibility_state));
    if (state_str == "unknown")
        err("compatibility_state", "Invalid compatibility_state value");

    // Known issues: each must have an id and description
    for (size_t i = 0; i < rec.known_issues.size(); i++) {
        const auto& issue = rec.known_issues[i];
        if (issue.id.empty())
            err("known_issues[" + std::to_string(i) + "].id", "Issue missing required id field");
        if (issue.description.empty())
            err("known_issues[" + std::to_string(i) + "].description", "Issue missing required description field");
    }

    // Workarounds: each must have a non-empty description
    for (size_t i = 0; i < rec.workarounds.size(); i++) {
        if (rec.workarounds[i].description.empty())
            err("workarounds[" + std::to_string(i) + "].description", "Workaround missing required description");
    }

    // Tested distros: each must have distribution and version
    for (size_t i = 0; i < rec.tested_on.size(); i++) {
        const auto& d = rec.tested_on[i];
        if (d.distribution.empty())
            err("tested_on[" + std::to_string(i) + "].distribution", "Tested distro missing distribution name");
        if (d.version.empty())
            err("tested_on[" + std::to_string(i) + "].version", "Tested distro missing version");
    }

    // VM requirements validation
    if (rec.macos_guest_requirements.has_value()) {
        const auto& vm = *rec.macos_guest_requirements;
        if (vm.minimum_ram_mb < 2048)
            err("macos_guest_requirements.minimum_ram_mb", "VM RAM must be at least 2048 MB");
        if (vm.minimum_disk_gb < 10)
            err("macos_guest_requirements.minimum_disk_gb", "VM disk must be at least 10 GB");
    }

    // Degradation metadata validation
    if (rec.degradation.has_value()) {
        const auto& d = *rec.degradation;
        if (d.category.empty()) {
            err("degradation.category", "degradation block must declare a category");
        } else {
            std::vector<std::string> valid_cats = {"none", "transparent", "shimmed", "functional", "cosmetic", "unsafe", "experimental", "hard_failure"};
            if (std::find(valid_cats.begin(), valid_cats.end(), d.category) == valid_cats.end()) {
                err("degradation.category", "Invalid degradation category: " + d.category);
            }
        }
        if (!d.confidence.empty()) {
            std::vector<std::string> valid_conf = {"verified", "functional", "degraded", "experimental", "unsupported"};
            if (std::find(valid_conf.begin(), valid_conf.end(), d.confidence) == valid_conf.end()) {
                err("degradation.confidence", "Invalid compatibility confidence: " + d.confidence);
            }
        }
    }

    return result;
}

ValidationResult validate_state_transitions(const CompatibilityRecord& rec) {
    ValidationResult result;
    result.valid = true;

    auto err = [&](const std::string& field, const std::string& msg, VLevel level = VLevel::Error) {
        result.errors.push_back({field, msg, level});
        if (level == VLevel::Error) result.valid = false;
    };

    // State-transition rules from the architecture's degradation philosophy:

    // Rule: "verified" state cannot have unresolved critical issues
    if (rec.compatibility_state == CompatibilityState::Verified) {
        for (const auto& issue : rec.known_issues) {
            if (issue.severity == IssueSeverity::Critical && issue.resolved_in_version.empty()) {
                err("compatibility_state",
                    "State 'verified' incompatible with unresolved critical issue: " + issue.id);
            }
        }
    }

    // Rule: "unsupported" must be consistent with execution tier
    if (rec.compatibility_state == CompatibilityState::Unsupported &&
        rec.execution_tier != ExecutionTier::Unsupported) {
        err("compatibility_state",
            "State 'unsupported' requires execution_tier 'unsupported'",
            VLevel::Warning);
    }

    // Rule: Tier 4B apps with metal requirement should be at most "degraded" without VM
    if (rec.macos_guest_requirements.has_value() && rec.macos_guest_requirements->requires_metal) {
        if (rec.execution_tier == ExecutionTier::NativeSubstitution ||
            rec.execution_tier == ExecutionTier::DarlingCompatible) {
            if (rec.compatibility_state == CompatibilityState::Verified ||
                rec.compatibility_state == CompatibilityState::Functional) {
                err("compatibility_state",
                    "Metal requirement on non-VM tier should not be 'verified' or 'functional'",
                    VLevel::Warning);
            }
        }
    }

    // Rule: "unsafe" degradation must not masquerade as "verified" compatibility state
    if (rec.degradation.has_value() && rec.degradation->category == "unsafe") {
        if (rec.compatibility_state == CompatibilityState::Verified) {
            err("compatibility_state", "Unsafe degradation category must not have 'verified' compatibility state");
        }
    }
    // Rule: "hard_failure" degradation must be consistent with broken compatibility state
    if (rec.degradation.has_value() && rec.degradation->category == "hard_failure") {
        if (rec.compatibility_state != CompatibilityState::Broken) {
            err("compatibility_state", "Hard failure degradation category requires 'broken' compatibility state");
        }
    }
    // Rule: "transparent" substitution prohibits active shims or degraded capabilities
    if (rec.degradation.has_value() && rec.degradation->category == "transparent") {
        if (!rec.degradation->active_shims.empty()) {
            err("degradation.active_shims", "Transparent substitution category prohibits active shims");
        }
        if (!rec.degradation->degraded_capabilities.empty()) {
            err("degradation.degraded_capabilities", "Transparent substitution category prohibits degraded capabilities");
        }
    }

    return result;
}

ValidationResult validate_full(const CompatibilityRecord& record) {
    auto schema_result = validate_record(record);
    auto state_result = validate_state_transitions(record);

    // Merge results
    schema_result.errors.insert(schema_result.errors.end(),
        state_result.errors.begin(), state_result.errors.end());
    if (!state_result.valid) schema_result.valid = false;

    return schema_result;
}

} // namespace compatdb
