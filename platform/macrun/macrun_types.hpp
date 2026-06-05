// macrun Orchestration Types
// Architecture reference: docs/architecture/ARCHITECTURE_V6.md
//   — "macrun Contract": accepts .app/.dmg/Mach-O binary, outputs execution
//     strategy, runtime configuration, compatibility state, launch orchestration.
//   — Never implements rendering, translation, or compositor logic.
#pragma once
#include <detector.hpp>
#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <chrono>

namespace macrun {

// ============================================================
// Input normalization — what format did the user provide?
// ============================================================
enum class InputFormat : uint8_t {
    AppBundle,        // path/to/Example.app
    DMGImage,         // path/to/Example.dmg
    MachOBinary,      // path/to/Example (raw Mach-O executable)
    Unknown,
};

constexpr std::string_view to_string(InputFormat f) {
    switch (f) {
        case InputFormat::AppBundle:  return "app-bundle";
        case InputFormat::DMGImage:   return "dmg-image";
        case InputFormat::MachOBinary: return "mach-o";
        case InputFormat::Unknown:    return "unknown";
    }
    return "unknown";
}

// ============================================================
// Input resolution result
// ============================================================
struct ResolvedInput {
    InputFormat format = InputFormat::Unknown;
    std::string original_path;
    std::string extracted_bundle_path;  // for DMG: path to mounted/extracted .app
    std::string executable_path;        // Mach-O within the bundle
    bool dmg_mount_succeeded = false;
    bool bundle_extraction_succeeded = false;
    std::vector<std::string> extraction_warnings;
};

// ============================================================
// Runtime Backend Abstraction
// Architecture reference: ARCHITECTURE_V6.md — "Execution Strategy overview"
// ============================================================
enum class BackendType : uint8_t {
    ElectronRuntime,    // Tier 0: native Electron runtime substitution
    TauriBridge,        // Tier 0: Tauri hybrid bridge
    DarlingRuntime,     // Tier 1-2: Mach-O via Darling
    CocoaLite,          // Tier 2: lightweight Cocoa compatibility
    ARM64Translator,    // Tier 3: QEMU user-mode ARM64 translation
    VMAssisted,         // Tier 4B: macOS VM + window streaming
    None,               // No backend (unsupported)
};

constexpr std::string_view to_string(BackendType b) {
    switch (b) {
        case BackendType::ElectronRuntime: return "electron-runtime";
        case BackendType::TauriBridge:     return "tauri-bridge";
        case BackendType::DarlingRuntime:  return "darling-runtime";
        case BackendType::CocoaLite:       return "cocoa-lite";
        case BackendType::ARM64Translator:  return "arm64-translator";
        case BackendType::VMAssisted:      return "vm-assisted";
        case BackendType::None:            return "none";
    }
    return "unknown";
}

// ============================================================
// Backend selection criteria
// ============================================================
struct BackendSelection {
    BackendType primary   = BackendType::None;
    BackendType fallback  = BackendType::None;
    bool vm_required      = false;
    std::string rationale;
    std::vector<std::string> applied_rules;
};

// ============================================================
// Required capabilities — orchestration-expressed requirements.
// Substrate-specific paths, env vars, and flags live in adapters only.
// Orchestration never owns substrate provisioning knowledge.
// ============================================================
struct RequiredCapabilities {
    bool needs_asar_extraction    = false;
    bool needs_arm64_translation  = false;
    bool needs_wayland_integration = false;
    bool needs_hotkey_bridge      = false;
    bool needs_coreanimation_flatten = false;
    bool needs_gpu_disabled        = false;
    bool needs_autoupdater_disabled = false;
};

// ============================================================
// Compatibility report (per-application)
// ============================================================
struct CompatibilityReport {
    platform::CompatibilityState state;
    std::vector<std::string> issues;
    std::vector<std::string> workarounds;
    std::vector<std::string> degradation_risks;
    std::vector<std::string> unsupported_reasons;
    std::string compat_db_record_id;   // matching record from compat-db, if any
    bool compat_db_match = false;
};

// ============================================================
// Launch Plan — the final orchestration output
// ============================================================
struct LaunchPlan {
    // Identifiers
    std::string application_name;
    std::string bundle_identifier;
    std::string application_version;

    // Input resolution
    ResolvedInput input;

    // Detection results from the capability engine
    platform::DetectionResult detection;

    // Backend selection
    BackendSelection backend;

    // Runtime provisioning
    RequiredCapabilities capabilities;

    // Compatibility assessment
    CompatibilityReport compatibility;

    // Launch metadata
    std::string plan_id;          // unique deterministic plan ID
    std::string plan_timestamp;

    // Whether this plan is launchable
    bool can_launch = false;
    std::string block_reason;     // why it can't launch, if applicable

    // Full diagnostic log
    std::vector<std::string> diagnostic_log;
};

// ============================================================
// Orchestration result wrapper
// ============================================================
struct OrchestrationResult {
    LaunchPlan plan;
    bool success = false;                // orchestration itself succeeded
    std::vector<std::string> errors;     // orchestration errors (not app errors)
};

} // namespace macrun
