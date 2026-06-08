// Native Module Compatibility Engine — Type Definitions
// Phase 4B: Governed native module discovery, ABI verification, caching, and build dispatch.
// Architecture reference: Native Module Compatibility Infrastructure Plan v3
//
// All enums and structs used by platform/native/ components.
// Isolated from orchestrator/detector — only consumed by ElectronAdapter internals.
#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <optional>

namespace platform::native {

// ============================================================
// Risk classification for discovered native modules
// ============================================================
enum class NativeModuleRiskClass : uint8_t {
    SAFE_ELF           = 0,  // Already ELF, no action needed
    REGISTRY_CACHED    = 1,  // Registry entry + cache hit (host ABI match)
    REGISTRY_BUILDABLE = 2,  // Registry entry, no cache → needs macrun provision
    REGISTRY_STUBBED   = 3,  // Registry entry, stub_policy=always_stub
    UNKNOWN_STUBBED    = 4,  // Not in registry → stub only (safe default)
    BYPASSED           = 5,  // MACRUN_ALLOW_DARWIN_NATIVE=1 active
};

constexpr std::string_view risk_class_to_string(NativeModuleRiskClass c) {
    switch (c) {
        case NativeModuleRiskClass::SAFE_ELF:           return "SAFE_ELF";
        case NativeModuleRiskClass::REGISTRY_CACHED:    return "REGISTRY_CACHED";
        case NativeModuleRiskClass::REGISTRY_BUILDABLE: return "REGISTRY_BUILDABLE";
        case NativeModuleRiskClass::REGISTRY_STUBBED:   return "REGISTRY_STUBBED";
        case NativeModuleRiskClass::UNKNOWN_STUBBED:    return "UNKNOWN_STUBBED";
        case NativeModuleRiskClass::BYPASSED:           return "BYPASSED";
    }
    return "UNKNOWN";
}

// ============================================================
// ABI matching result
// ============================================================
enum class ABIMatchResult : uint8_t {
    ELF_SAFE     = 0,  // .node is ELF — no ABI concern
    ABI_MATCH    = 1,  // Mach-O NODE_MODULE_VERSION matches target
    ABI_MISMATCH = 2,  // NODE_MODULE_VERSION differs → must recompile
    ABI_UNKNOWN  = 3,  // Could not determine NODE_MODULE_VERSION
};

// ============================================================
// Substitution action (what to do with a module)
// ============================================================
enum class SubstitutionAction : uint8_t {
    NONE             = 0,  // No action needed (ELF or no modules)
    STUB             = 1,  // Use dlopen Proxy stub via shim
    CACHED           = 2,  // Copy cached .node to staging
    BYPASS           = 3,  // MACRUN_ALLOW_DARWIN_NATIVE=1
    PROVISION_NEEDED = 4,  // Cache miss — user must run macrun provision
};

constexpr std::string_view substitution_action_to_string(SubstitutionAction a) {
    switch (a) {
        case SubstitutionAction::NONE:             return "none";
        case SubstitutionAction::STUB:             return "stubbed";
        case SubstitutionAction::CACHED:           return "cached";
        case SubstitutionAction::BYPASS:           return "bypassed";
        case SubstitutionAction::PROVISION_NEEDED: return "provision_needed";
    }
    return "unknown";
}

// ============================================================
// Discovered native module (post-ASAR-extraction scan)
// ============================================================
struct DiscoveredNativeModule {
    std::string path;           // full path to .node file
    std::string module_name;    // derived from path (e.g. "better-sqlite3")
    uint32_t magic = 0;         // 0xFEEDFACE (Mach-O) or 0x7F454C46 (ELF)
    std::string arch;           // "x86_64", "arm64", "unknown"
    uint32_t node_api_version = 0;  // NAPI_MODULE_VERSION from export table
    bool is_critical = false;   // from compat-db cross-reference
};

// ============================================================
// ABI Index — resolved from electron-abi-map.json
// ============================================================
struct ABIIndex {
    uint32_t node_module_version = 0;
    std::string node_version;
    std::string v8_version;
};

// ============================================================
// Native module assessment (discovery + ABI verification)
// ============================================================
struct NativeModuleAssessment {
    DiscoveredNativeModule module;
    ABIMatchResult abi_status = ABIMatchResult::ABI_UNKNOWN;
    uint32_t required_nmv = 0;
    uint32_t actual_nmv = 0;
    NativeModuleRiskClass risk_class = NativeModuleRiskClass::UNKNOWN_STUBBED;
};

// ============================================================
// Substitution entry — one per module in the plan
// ============================================================
struct SubstitutionEntry {
    SubstitutionAction action = SubstitutionAction::NONE;
    std::string module_name;
    std::string source_path;    // cache path (for CACHED) or empty
    std::string dest_path;      // staging destination in extracted app
    NativeModuleRiskClass risk_class = NativeModuleRiskClass::UNKNOWN_STUBBED;
};

// ============================================================
// Substitution plan — the complete plan for an app launch
// ============================================================
struct NativeSubstitutionPlan {
    std::vector<SubstitutionEntry> entries;
    size_t cached_count = 0;
    size_t stubbed_count = 0;
    size_t bypassed_count = 0;
    size_t provision_needed_count = 0;
    bool requires_provisioning = false;
    std::vector<std::string> provisioning_modules;  // names for user message
};

// ============================================================
// Cache result
// ============================================================
struct CacheResult {
    bool hit = false;
    std::string binary_path;
    std::string manifest_path;
};

// ============================================================
// Build specification (for macrun provision)
// ============================================================
struct BuildSpec {
    std::string module_name;
    std::string npm_version;
    std::string sha256;
    std::vector<std::string> build_flags;
    std::map<std::string, std::string> build_env;
    std::string electron_version;
    std::string arch;
    std::vector<std::string> patches;
    std::map<std::string, std::string> dependencies;
};

struct BuildResult {
    bool success = false;
    std::string binary_path;
    std::string log_path;
    uint64_t duration_ms = 0;
    std::string error_message;
};

} // namespace platform::native
