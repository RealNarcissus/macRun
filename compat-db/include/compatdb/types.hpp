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
#include <map>
#include <optional>

namespace compatdb {

// ============================================================
// Schema version
// ============================================================
constexpr std::string_view CURRENT_SCHEMA_VERSION = "1.1.0";

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
// Architecture class per COMPATIBILITY_SPECTRUM.md
// ============================================================
enum class ArchitectureClass : uint8_t {
    ClassA = 0,  // Self-contained, no native dependencies
    ClassB = 1,  // API drift requiring normalization
    ClassC = 2,  // Native module compilation required
    ClassD = 3,  // External backend substitution required
};

constexpr std::string_view arch_class_to_string(ArchitectureClass c) {
    switch (c) {
        case ArchitectureClass::ClassA: return "class_a";
        case ArchitectureClass::ClassB: return "class_b";
        case ArchitectureClass::ClassC: return "class_c";
        case ArchitectureClass::ClassD: return "class_d";
    }
    return "unknown";
}

constexpr ArchitectureClass arch_class_from_string(std::string_view s) {
    if (s == "class_a") return ArchitectureClass::ClassA;
    if (s == "class_b") return ArchitectureClass::ClassB;
    if (s == "class_c") return ArchitectureClass::ClassC;
    if (s == "class_d") return ArchitectureClass::ClassD;
    return ArchitectureClass::ClassA;
}

// ============================================================
// Native module tracking per Phase 4B Native ABI Compatibility
// ============================================================
struct CriticalNativeModule {
    std::string module;              // npm module name (e.g. "better-sqlite3")
    std::string role;                // architectural role (e.g. "state-database")
    bool requires_compilation = false;
};

struct ExternalProcess {
    std::string name;                // process/executable name
    std::string type;                // "backend-server" | "plugin-host" | "sidecar" | "language-server"
    std::string protocol;            // "stdio-mcp" | "http" | "grpc" | "unix-socket"
    std::string binary_type;         // "mach-o-arm64" | "mach-o-x86_64" | "elf-x86_64"
    std::string substitution_env;    // env var for Linux-native replacement path
};

struct RuntimePolicy {
    std::vector<std::string> preferred;   // preferred Electron versions (desc priority)
    std::string minimum;                   // minimum acceptable version
    std::vector<std::string> validated;   // versions validated against this app
    std::vector<std::string> fallback;    // fallback versions
};

struct KnownBadEntry {
    uint32_t electron_major = 0;
    std::string reason;
};

// ============================================================
// Native replacement record — returned by NativeRegistry::find_replacement()
// Owned by compat-db’s data layer per dependency direction.
// ============================================================
struct NativeReplacementRecord {
    std::string module_name;
    std::string npm_version;
    std::string sha256;
    uint32_t required_nmv = 0;
    std::vector<std::string> build_flags;
    std::map<std::string, std::string> build_env;
    bool known_good = false;
    std::vector<std::string> patches;
    std::vector<KnownBadEntry> known_bad_on;
    std::string shim_type;
    std::map<std::string, std::string> dependencies;
    std::string npm_package;
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

    // === Phase 4B: Native ABI Compatibility fields ===
    std::optional<ArchitectureClass> architecture_class;
    std::vector<CriticalNativeModule> critical_native_modules;
    std::vector<ExternalProcess> external_processes;
    std::optional<RuntimePolicy> runtime_policy;
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
