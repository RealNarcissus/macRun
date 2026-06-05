// Compatibility Database Record Types
// Architecture reference: docs/architecture/ARCHITECTURE_V6.md
//   — "Compatibility Database" section
//   — "Compatibility States" table
//   — "compat-db Contract"
//
// Mirrors the JSON schema at compat-db/schema/record.schema.json.
// All fields are deterministic and version-tagged for forward compatibility.
#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <optional>

namespace compatdb {

// ============================================================
// Schema version
// ============================================================
constexpr std::string_view CURRENT_SCHEMA_VERSION = "1.0.0";

// ============================================================
// Execution Tier (mirrors platform::ExecutionTier, isolated here)
// ============================================================
enum class ExecutionTier : uint8_t {
    NativeSubstitution = 0,
    DarlingCompatible  = 1,
    VMRecommended      = 2,
    Unsupported        = 3,
};

constexpr std::string_view tier_to_string(ExecutionTier t) {
    switch (t) {
        case ExecutionTier::NativeSubstitution: return "native-substitution";
        case ExecutionTier::DarlingCompatible:  return "darling-compatible";
        case ExecutionTier::VMRecommended:      return "vm-recommended";
        case ExecutionTier::Unsupported:        return "unsupported";
    }
    return "unknown";
}

constexpr ExecutionTier tier_from_string(std::string_view s) {
    if (s == "native-substitution") return ExecutionTier::NativeSubstitution;
    if (s == "darling-compatible")  return ExecutionTier::DarlingCompatible;
    if (s == "vm-recommended")      return ExecutionTier::VMRecommended;
    return ExecutionTier::Unsupported;
}

// ============================================================
// Compatibility State (mirrors platform::CompatibilityState)
// ============================================================
enum class CompatibilityState : uint8_t {
    Verified    = 0,
    Functional  = 1,
    Partial     = 2,
    Degraded    = 3,
    Unsupported = 4,
    Broken      = 5,
};

constexpr std::string_view state_to_string(CompatibilityState s) {
    switch (s) {
        case CompatibilityState::Verified:    return "verified";
        case CompatibilityState::Functional:  return "functional";
        case CompatibilityState::Partial:     return "partial";
        case CompatibilityState::Degraded:    return "degraded";
        case CompatibilityState::Unsupported: return "unsupported";
        case CompatibilityState::Broken:      return "broken";
    }
    return "unknown";
}

constexpr CompatibilityState state_from_string(std::string_view s) {
    if (s == "verified")    return CompatibilityState::Verified;
    if (s == "functional")  return CompatibilityState::Functional;
    if (s == "partial")     return CompatibilityState::Partial;
    if (s == "degraded")    return CompatibilityState::Degraded;
    if (s == "unsupported") return CompatibilityState::Unsupported;
    return CompatibilityState::Broken;
}

// ============================================================
// Issue Severity
// ============================================================
enum class IssueSeverity : uint8_t {
    Critical = 0,
    High     = 1,
    Medium   = 2,
    Low      = 3,
    Cosmetic = 4,
};

constexpr std::string_view severity_to_string(IssueSeverity s) {
    switch (s) {
        case IssueSeverity::Critical: return "critical";
        case IssueSeverity::High:     return "high";
        case IssueSeverity::Medium:   return "medium";
        case IssueSeverity::Low:      return "low";
        case IssueSeverity::Cosmetic: return "cosmetic";
    }
    return "unknown";
}

constexpr IssueSeverity severity_from_string(std::string_view s) {
    if (s == "critical") return IssueSeverity::Critical;
    if (s == "high")     return IssueSeverity::High;
    if (s == "medium")   return IssueSeverity::Medium;
    if (s == "low")      return IssueSeverity::Low;
    return IssueSeverity::Cosmetic;
}

// ============================================================
// Sub-records
// ============================================================
struct TestedDistro {
    std::string distribution;
    std::string version;
    std::string kernel_version;
    std::string arch; // "x86_64" or "aarch64"
};

struct KnownIssue {
    std::string id;
    IssueSeverity severity = IssueSeverity::Medium;
    std::string description;
    std::string affected_subsystem;
    std::string reproduction_steps;
    std::string resolved_in_version;
};

struct Workaround {
    std::string description;
    std::vector<std::string> applies_to_issues;
    std::vector<std::string> requires_flags;
};

struct MacOSGuestRequirements {
    std::string minimum_macos_version;
    std::string recommended_macos_version;
    uint32_t minimum_ram_mb = 4096;
    uint32_t minimum_disk_gb = 30;
    bool requires_metal = false;
    bool requires_accessibility = false;
    std::string special_configuration;
};

struct PerformanceCharacteristics {
    uint32_t startup_time_ms = 0;
    uint32_t memory_usage_mb = 0;
    std::string cpu_overhead; // "negligible" | "low" | "moderate" | "high" | "extreme"
    std::string notes;
};

struct CapabilityRequirements {
    struct FrameworkEntry {
        std::string framework;
        std::string impact; // "critical" | "high" | "medium" | "low"
        bool detected = false;
    };
    std::vector<FrameworkEntry> frameworks;
    std::string architecture; // "x86_64" | "arm64" | "universal" | "unknown"
    uint32_t risk_score_total = 0;
};

struct ContributorInfo {
    std::string name;
    std::string contact;
    std::string verification_method; // "manual" | "automated" | "user-report" | "developer"
};

// ============================================================
// Degradation metadata — per DEGRADATION_MODEL.md
// ============================================================
struct DegradationMetadata {
    // Degradation category per DEGRADATION_MODEL.md Section "Degradation Categories"
    // One of: "none", "transparent", "shimmed", "functional", "cosmetic", "unsafe", "experimental", "hard_failure"
    std::string category;

    // Compatibility confidence per COMPATIBILITY_CONFIDENCE.md
    // One of: "verified", "functional", "degraded", "experimental", "unsupported"
    std::string confidence;

    // Active shims for this application (e.g., ["path-mapper", "clipboard-bridge"])
    std::vector<std::string> active_shims;

    // Capabilities that are degraded but still functional
    std::vector<std::string> degraded_capabilities;

    // Unsafe bypasses in effect (e.g., ["MACRUN_ALLOW_DARWIN_NATIVE"])
    std::vector<std::string> unsafe_bypasses;

    // Specific modules that were bypassed (for native-module bypass)
    std::vector<std::string> bypassed_modules;

    // Recommended action for the user
    std::string recommended_action;

    // Experimental features in use (e.g., ["MACRUN_EXPERIMENTAL_METAL_SOFTWARE"])
    std::vector<std::string> experimental_features;
};

// ============================================================
// Main Compatibility Record
// ============================================================
struct CompatibilityRecord {
    // Required fields
    std::string schema_version;
    std::string record_id;
    std::string bundle_identifier;
    std::string application_name;
    ExecutionTier execution_tier = ExecutionTier::NativeSubstitution;
    CompatibilityState compatibility_state = CompatibilityState::Functional;
    std::string last_updated;

    // Optional fields
    std::string application_version;
    std::vector<TestedDistro> tested_on;
    std::vector<KnownIssue> known_issues;
    std::vector<Workaround> workarounds;
    std::optional<MacOSGuestRequirements> macos_guest_requirements;
    std::optional<PerformanceCharacteristics> performance_characteristics;
    std::optional<CapabilityRequirements> capability_requirements;
    std::optional<ContributorInfo> contributor;
    std::optional<DegradationMetadata> degradation;
    std::string notes;
    std::vector<std::string> tags;

    // Key-value flags (e.g. {"ELECTRON_RUNTIME": "28", "DISABLE_GPU": "1"})
    std::vector<std::pair<std::string, std::string>> required_flags;
};

// ============================================================
// Query result
// ============================================================
struct QueryResult {
    CompatibilityRecord record;
    std::string source_file;
    double relevance_score = 1.0; // for future fuzzy matching
};

} // namespace compatdb
