// Stage 5: Execution Strategy Resolution
// Architecture reference: docs/architecture/ARCHITECTURE_V6.md
//   — "Stage 5 — Execution Strategy Resolution"
//   — "Detection Outcomes" table
//   — "Execution Strategy overview" matrix
//   — "Invariant 2 — Execution Strategy Must Match App Category"

#include "detector.hpp"
#include <algorithm>

namespace platform {

// App category determination from the architecture's execution strategy matrix
enum class AppCategory {
    Electron,
    Tauri,
    WailsApp,
    MachOCLI,
    LightweightCocoa,
    SwiftUIHeavy,
    UnspportedHypervisor,
    Unknown,
};

static AppCategory classify_app(const std::vector<DetectedFramework>& frameworks) {
    bool has_electron     = false;
    bool has_tauri        = false;
    bool has_wails        = false;
    bool has_swiftui      = false;
    bool has_appkit       = false;
    bool has_accessibility = false;
    bool has_hypervisor   = false;
    bool has_metal        = false;

    for (const auto& f : frameworks) {
        switch (f.id) {
            case FrameworkId::Electron:      has_electron = true;      break;
            case FrameworkId::Tauri:         has_tauri = true;         break;
            case FrameworkId::Wails:         has_wails = true;         break;
            case FrameworkId::SwiftUI:       has_swiftui = true;       break;
            case FrameworkId::AppKit:        has_appkit = true;        break;
            case FrameworkId::Accessibility: has_accessibility = true; break;
            case FrameworkId::Hypervisor:    has_hypervisor = true;    break;
            case FrameworkId::Metal:         has_metal = true;         break;
            default: break;
        }
    }

    if (has_hypervisor)   return AppCategory::UnspportedHypervisor;
    if (has_electron)     return AppCategory::Electron;
    if (has_tauri)        return AppCategory::Tauri;
    if (has_wails)        return AppCategory::WailsApp;
    if (has_accessibility) return AppCategory::SwiftUIHeavy;
    if (has_swiftui)      return AppCategory::SwiftUIHeavy;
    if (has_appkit && has_metal) return AppCategory::SwiftUIHeavy;
    if (has_appkit)       return AppCategory::LightweightCocoa;
    return AppCategory::MachOCLI;
}

TierRecommendation resolve_execution_strategy(
    const CapabilityScore& score,
    const std::vector<DetectedFramework>& frameworks,
    const MachOInfo& macho,
    const BundleInfo& bundle)
{
    TierRecommendation rec;
    AppCategory cat = classify_app(frameworks);

    // Apply the architecture's Execution Strategy matrix (Invariant 2):
    // | App Type                        | Preferred Strategy          |
    // | Electron                        | Native runtime substitution |
    // | Tauri                           | Hybrid bridge               |
    // | CLI                             | Darling                     |
    // | Lightweight AppKit              | Cocoa-lite compatibility    |
    // | SwiftUI-heavy/system-integrated | VM-assisted execution       |

    switch (cat) {
        case AppCategory::Electron:
            rec.preferred_tier = ExecutionTier::Tier0_NativeSubstitution;
            rec.fallback_tier  = ExecutionTier::Tier4B_VMAssisted;
            rec.vm_required    = false;
            rec.expected_state = CompatibilityState::Functional;
            break;

        case AppCategory::Tauri:
            rec.preferred_tier = ExecutionTier::Tier0_NativeSubstitution;
            rec.fallback_tier  = ExecutionTier::Tier2_LightweightCocoa;
            rec.vm_required    = false;
            rec.expected_state = CompatibilityState::Functional;
            rec.degradation_risks.push_back({
                "Tauri backend bridge",
                "Native Rust backend unavailable — frontend-only mode may apply",
                CapabilityImpact::Medium
            });
            break;

        case AppCategory::WailsApp:
            rec.preferred_tier = ExecutionTier::Tier0_NativeSubstitution;
            rec.fallback_tier  = ExecutionTier::Tier2_LightweightCocoa;
            rec.vm_required    = false;
            rec.expected_state = CompatibilityState::Functional;
            break;

        case AppCategory::MachOCLI:
            rec.preferred_tier = ExecutionTier::Tier1_CLICompatibility;
            rec.fallback_tier  = ExecutionTier::Tier3_ARM64Translation;
            rec.vm_required    = false;
            rec.expected_state = CompatibilityState::Functional;
            // If ARM64-only, note translation requirement
            if (macho.primary_architecture == BinaryArchitecture::ARM64) {
                rec.preferred_tier = ExecutionTier::Tier3_ARM64Translation;
                rec.compatibility_warnings.push_back("ARM64 binary requires translation on x86 host");
            }
            break;

        case AppCategory::LightweightCocoa:
            rec.preferred_tier = ExecutionTier::Tier2_LightweightCocoa;
            rec.fallback_tier  = ExecutionTier::Tier4B_VMAssisted;
            rec.vm_required    = false;
            rec.expected_state = CompatibilityState::Partial;
            // With CoreAnimation, expect degraded rendering
            for (const auto& f : frameworks) {
                if (f.id == FrameworkId::CoreAnimation) {
                    rec.degradation_risks.push_back({
                        "CoreAnimation",
                        "CoreAnimation will be flattened — no GPU-accelerated compositing. Layout preserved, animations degraded.",
                        CapabilityImpact::High
                    });
                }
            }
            break;

        case AppCategory::SwiftUIHeavy:
            rec.preferred_tier = ExecutionTier::Tier4B_VMAssisted;
            rec.fallback_tier  = ExecutionTier::Tier4_Unsupported;
            rec.vm_required    = true;
            rec.expected_state = CompatibilityState::Degraded;
            rec.compatibility_warnings.push_back(
                "SwiftUI-heavy application requires VM-assisted execution for full functionality");

            for (const auto& f : frameworks) {
                if (f.id == FrameworkId::SwiftUI) {
                    rec.degradation_risks.push_back({
                        "SwiftUI",
                        "SwiftUI renderer, state system, animation model — full fidelity only in Tier 4B VM",
                        CapabilityImpact::High
                    });
                }
                if (f.id == FrameworkId::Accessibility) {
                    rec.degradation_risks.push_back({
                        "Accessibility APIs",
                        "Accessibility API hooks require macOS runtime — only available via Tier 4B VM",
                        CapabilityImpact::Critical
                    });
                }
            }
            break;

        case AppCategory::UnspportedHypervisor:
            rec.preferred_tier = ExecutionTier::Tier4_Unsupported;
            rec.fallback_tier  = ExecutionTier::Tier4_Unsupported;
            rec.vm_required    = false;
            rec.expected_state = CompatibilityState::Unsupported;
            rec.compatibility_warnings.push_back(
                "Hypervisor.framework dependency is unsupported — no kernel substrate available");
            break;

        case AppCategory::Unknown:
            rec.preferred_tier = ExecutionTier::Tier1_CLICompatibility;
            rec.fallback_tier  = ExecutionTier::Tier4B_VMAssisted;
            rec.vm_required    = false;
            rec.expected_state = CompatibilityState::Functional;
            break;
    }

    // Override to VM if critical capability gaps detected
    if (score.critical_count > 0) {
        rec.vm_required = true;
        if (rec.preferred_tier != ExecutionTier::Tier4_Unsupported) {
            rec.compatibility_warnings.push_back(
                "Critical compatibility gaps detected — VM-assisted execution recommended");
        }
    }

    // Add degradation risks for detected frameworks based on architecture's Degradation Rules
    for (const auto& f : frameworks) {
        switch (f.id) {
            case FrameworkId::Metal:
                rec.degradation_risks.push_back({
                    "Metal rendering",
                    "Metal-dependent rendering unavailable outside Tier 4B VM",
                    CapabilityImpact::High
                });
                break;
            case FrameworkId::PrivateFramework:
                rec.degradation_risks.push_back({
                    "Private framework",
                    "Private framework usage — Tier 4B VM required for reliable execution",
                    CapabilityImpact::High
                });
                break;
            case FrameworkId::CloudKit:
                rec.compatibility_warnings.push_back(
                    "CloudKit unavailable — cloud sync features will be disabled");
                break;
            case FrameworkId::Sparkle:
                rec.degradation_risks.push_back({
                    "Sparkle auto-updater",
                    "Sparkle auto-update mechanism disabled — updates managed by platform",
                    CapabilityImpact::Low
                });
                break;
            default:
                break;
        }
    }

    return rec;
}

} // namespace platform
