#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <variant>
#include <optional>
#include <unordered_map>

// Types aligned to docs/architecture/ARCHITECTURE_V6.md:
//   - "Execution Tier Model"
//   - "Compatibility States"
//   - "Capability Scoring" (Stage 4)
//   - "Detection Outcomes"

namespace platform {

// ============================================================
// Execution Tiers (from "Detection Outcomes" table)
// ============================================================
enum class ExecutionTier : uint8_t {
    Tier0_NativeSubstitution = 0,  // Electron/Tauri/Wails → native Linux runtime
    Tier1_CLICompatibility  = 1,   // Mach-O CLI via Darling
    Tier2_LightweightCocoa  = 2,   // Simple AppKit via Cocoa-lite
    Tier3_ARM64Translation  = 3,   // Apple Silicon binary translation
    Tier4B_VMAssisted       = 4,   // macOS VM + window streaming
    Tier4_Unsupported       = 5,   // Known architectural incompatibility
};

constexpr std::string_view to_string(ExecutionTier t) {
    switch (t) {
        case ExecutionTier::Tier0_NativeSubstitution: return "native-substitution";
        case ExecutionTier::Tier1_CLICompatibility:   return "darling-compatible";
        case ExecutionTier::Tier2_LightweightCocoa:   return "darling-compatible";
        case ExecutionTier::Tier3_ARM64Translation:   return "darling-compatible";
        case ExecutionTier::Tier4B_VMAssisted:        return "vm-recommended";
        case ExecutionTier::Tier4_Unsupported:        return "unsupported";
    }
    return "unknown";
}

// ============================================================
// Compatibility States (from "Compatibility States" table)
// ============================================================
enum class CompatibilityState : uint8_t {
    Verified    = 0,  // Fully tested and supported
    Functional  = 1,  // Core workflows operate correctly
    Partial     = 2,  // App launches but some subsystems unavailable
    Degraded    = 3,  // Major features unavailable but app remains usable
    Unsupported = 4,  // Known architectural incompatibility
    Broken      = 5,  // Unexpected failure or regression
};

constexpr std::string_view to_string(CompatibilityState s) {
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

// ============================================================
// Architecture Types (Stage 3 — Architecture Analysis)
// ============================================================
enum class BinaryArchitecture : uint8_t {
    X86_64        = 0,
    ARM64         = 1,
    Universal     = 2,   // fat binary with x86_64 + arm64
    Unknown       = 3,
};

constexpr std::string_view to_string(BinaryArchitecture a) {
    switch (a) {
        case BinaryArchitecture::X86_64:    return "x86_64";
        case BinaryArchitecture::ARM64:     return "arm64";
        case BinaryArchitecture::Universal: return "universal";
        case BinaryArchitecture::Unknown:   return "unknown";
    }
    return "unknown";
}

// ============================================================
// Framework / Technology Identifiers (Stage 2)
// ============================================================
enum class FrameworkId : uint8_t {
    Electron,
    Tauri,
    Wails,
    AppKit,
    SwiftUI,
    Metal,
    Accessibility,
    Hypervisor,
    XPCService,
    CoreData,
    WKWebView,
    CloudKit,
    Sparkle,
    CoreAnimation,
    AVFoundation,
    PrivateFramework,
    Rosetta,
};

constexpr std::string_view to_string(FrameworkId f) {
    switch (f) {
        case FrameworkId::Electron:         return "Electron";
        case FrameworkId::Tauri:            return "Tauri";
        case FrameworkId::Wails:            return "Wails";
        case FrameworkId::AppKit:           return "AppKit";
        case FrameworkId::SwiftUI:          return "SwiftUI";
        case FrameworkId::Metal:            return "Metal";
        case FrameworkId::Accessibility:    return "Accessibility";
        case FrameworkId::Hypervisor:       return "Hypervisor.framework";
        case FrameworkId::XPCService:       return "XPCService";
        case FrameworkId::CoreData:         return "CoreData";
        case FrameworkId::WKWebView:        return "WKWebView";
        case FrameworkId::CloudKit:         return "CloudKit";
        case FrameworkId::Sparkle:          return "Sparkle";
        case FrameworkId::CoreAnimation:    return "CoreAnimation";
        case FrameworkId::AVFoundation:     return "AVFoundation";
        case FrameworkId::PrivateFramework: return "PrivateFramework";
        case FrameworkId::Rosetta:          return "Rosetta";
    }
    return "unknown";
}

// ============================================================
// Capability Impact (from "Capability Scoring" table)
// ============================================================
enum class CapabilityImpact : uint8_t {
    Critical = 0,
    High     = 1,
    Medium   = 2,
    Low      = 3,
    None     = 4,
};

constexpr std::string_view to_string(CapabilityImpact i) {
    switch (i) {
        case CapabilityImpact::Critical: return "critical";
        case CapabilityImpact::High:     return "high";
        case CapabilityImpact::Medium:   return "medium";
        case CapabilityImpact::Low:      return "low";
        case CapabilityImpact::None:     return "none";
    }
    return "unknown";
}

// ============================================================
// Detected Framework Entry (Stage 2 output)
// ============================================================
struct DetectedFramework {
    FrameworkId        id;
    CapabilityImpact   impact;
    std::string        evidence;      // what triggered detection (file path, string match)
    std::string        version;       // detected version if parseable
};

// ============================================================
// Bundle Info (Stage 1 output)
// ============================================================
struct BundleInfo {
    std::string bundle_identifier;         // CFBundleIdentifier
    std::string bundle_name;               // CFBundleName / CFBundleDisplayName
    std::string bundle_version;            // CFBundleVersion
    std::string bundle_short_version;      // CFBundleShortVersionString
    std::string executable_name;           // CFBundleExecutable
    std::string bundle_type;               // APPL, FMWK, BNDL
    std::string minimum_os_version;        // LSMinimumSystemVersion / MinimumOSVersion
    std::string sdk_version;               // DTSDKName or derived
    std::vector<std::string> supported_platforms;  // from CFBundleSupportedPlatforms
    std::vector<std::string> required_capabilities; // UIRequiredDeviceCapabilities
    std::unordered_map<std::string, std::string> custom_properties;
    bool has_ns_principal_class = false;   // NSPrincipalClass present
    bool has_ns_main_nib_file   = false;   // NSMainNibFile present
};

// ============================================================
// Mach-O Analysis Result (Stage 3 output)
// ============================================================
struct MachOInfo {
    BinaryArchitecture primary_architecture = BinaryArchitecture::Unknown;
    std::vector<BinaryArchitecture> architectures_present;  // for fat binaries
    std::vector<std::string> linked_libraries;               // LC_LOAD_DYLIB entries
    std::vector<std::string> rpaths;
    bool is_dylib     = false;  // MH_DYLIB
    bool is_executable = false; // MH_EXECUTE
    bool is_bundle    = false;  // MH_BUNDLE
    uint32_t min_os_deployment = 0;     // from LC_VERSION_MIN_MACOSX
    bool has_pie      = false;  // MH_PIE flag
};

// ============================================================
// Entitlements (from Stage 1 embedded provisioning)
// ============================================================
struct EntitlementInfo {
    bool sandboxed                 = false;
    bool uses_accessibility        = false;  // com.apple.private.accessibility
    bool uses_hypervisor           = false;  // com.apple.security.hypervisor
    bool network_client            = false;
    bool network_server            = false;
    bool camera                    = false;
    bool microphone                = false;
    bool usb                       = false;
    bool bluetooth                 = false;
    bool file_access_user_selected = false;
    bool hardening_runtime         = false;
    std::vector<std::string> raw_entitlement_keys;
};

// ============================================================
// Capability Score (Stage 4 output)
// ============================================================
struct CapabilityDimension {
    std::string       name;           // e.g. "SwiftUI dependency"
    CapabilityImpact  impact_weight;  // from architecture's capability scoring table
    bool              detected = false;
    std::string       detail;
};

struct CapabilityScore {
    std::vector<CapabilityDimension> dimensions;
    unsigned  risk_score_total = 0;      // weighted sum (Critical=10, High=7, Medium=4, Low=1)
    unsigned  critical_count   = 0;
    unsigned  high_count       = 0;
    unsigned  medium_count     = 0;
    unsigned  low_count        = 0;
};

// ============================================================
// Tier Recommendation (Stage 5 output)
// ============================================================
struct DegradationRisk {
    std::string subsystem;
    std::string description;
    CapabilityImpact severity;
};

struct TierRecommendation {
    ExecutionTier     preferred_tier;
    ExecutionTier     fallback_tier = ExecutionTier::Tier4_Unsupported;
    bool              vm_required   = false;
    CompatibilityState expected_state = CompatibilityState::Functional;
    std::vector<DegradationRisk> degradation_risks;
    std::vector<std::string> compatibility_warnings;
};

// ============================================================
// Full Detection Result (aggregate of all stages)
// ============================================================
struct DetectionResult {
    BundleInfo                  bundle;
    MachOInfo                   macho;
    EntitlementInfo             entitlements;
    std::vector<DetectedFramework> frameworks;
    CapabilityScore             score;
    TierRecommendation          recommendation;
    CompatibilityState          compatibility_state = CompatibilityState::Functional;
    std::vector<std::string>    unsupported_reasons;
    std::string                 detection_version = "1.0.0";
};

} // namespace platform
