// macrun Backend Dispatch: maps execution tiers to concrete runtime backends
// Architecture reference: docs/architecture/ARCHITECTURE_V6.md
//   — "Execution Strategy overview" matrix
//   — Invariant 2: Execution Strategy Must Match App Category
//   — "macrun Contract": outputs execution strategy, never implements backends
//
// SUBSTRATE_MODEL.md Section 6: orchestrator must never directly depend on
// upstream substrate internals. This file performs ONLY tier-to-backend mapping
// and capability requirement computation. Substrate provisioning (paths, env vars,
// flags) is the responsibility of the adapter layer.

#include "macrun.hpp"
#include <detector.hpp>
#include <algorithm>

namespace macrun {

static BackendType tier_to_backend(platform::ExecutionTier tier) {
    switch (tier) {
        case platform::ExecutionTier::Tier0_NativeSubstitution: return BackendType::ElectronRuntime;
        case platform::ExecutionTier::Tier1_CLICompatibility:   return BackendType::DarlingRuntime;
        case platform::ExecutionTier::Tier2_LightweightCocoa:   return BackendType::CocoaLite;
        case platform::ExecutionTier::Tier3_ARM64Translation:   return BackendType::ARM64Translator;
        case platform::ExecutionTier::Tier4B_VMAssisted:        return BackendType::VMAssisted;
        case platform::ExecutionTier::Tier4_Unsupported:        return BackendType::None;
    }
    return BackendType::None;
}

BackendSelection select_backend(const platform::DetectionResult& detection) {
    BackendSelection sel;

    const auto& rec = detection.recommendation;
    sel.primary   = tier_to_backend(rec.preferred_tier);
    sel.fallback  = tier_to_backend(rec.fallback_tier);
    sel.vm_required = rec.vm_required;

    std::string rationale;
    rationale += "bundle=" + detection.bundle.bundle_identifier;
    rationale += " tier=" + std::string(platform::to_string(rec.preferred_tier));
    rationale += " arch=" + std::string(platform::to_string(detection.macho.primary_architecture));
    rationale += " risk_score=" + std::to_string(detection.score.risk_score_total);

    for (const auto& fw : detection.frameworks) {
        if (fw.id == platform::FrameworkId::Electron) {
            sel.applied_rules.push_back("Framework fingerprint: Electron detected → ElectronRuntime backend");
            break;
        }
        if (fw.id == platform::FrameworkId::Tauri) {
            sel.applied_rules.push_back("Framework fingerprint: Tauri detected → TauriBridge backend");
            break;
        }
    }

    for (const auto& fw : detection.frameworks) {
        if (fw.id == platform::FrameworkId::SwiftUI) {
            sel.applied_rules.push_back("SwiftUI dependency detected → routing to VM-assisted backend");
            break;
        }
        if (fw.id == platform::FrameworkId::Hypervisor) {
            sel.applied_rules.push_back("Hypervisor.framework dependency → unsupported (Tier 4)");
            break;
        }
    }

    if (detection.score.critical_count > 0) {
        sel.applied_rules.push_back(
            "Critical capability gaps (count=" + std::to_string(detection.score.critical_count) +
            ") — VM execution required");
    }

    sel.rationale = rationale;
    return sel;
}

// compute_required_capabilities expresses WHAT the backend needs.
// It sets boolean capability flags only — no paths, env vars, or substrate flags.
// The adapter layer translates these to substrate-specific configuration.
RequiredCapabilities compute_required_capabilities(
    const platform::DetectionResult& detection,
    const BackendSelection& backend)
{
    RequiredCapabilities caps;

    switch (backend.primary) {
        case BackendType::ElectronRuntime:
            caps.needs_asar_extraction = true;
            caps.needs_wayland_integration = true;
            break;

        case BackendType::TauriBridge:
            caps.needs_wayland_integration = true;
            break;

        case BackendType::CocoaLite:
            caps.needs_wayland_integration = true;
            caps.needs_coreanimation_flatten = true;
            break;

        case BackendType::ARM64Translator:
            caps.needs_arm64_translation = true;
            break;

        case BackendType::VMAssisted:
            caps.needs_wayland_integration = true;
            caps.needs_hotkey_bridge = true;
            break;

        case BackendType::DarlingRuntime:
        case BackendType::None:
            break;
    }

    for (const auto& dim : detection.score.dimensions) {
        if (dim.detected && dim.name == "Metal dependency") {
            caps.needs_gpu_disabled = true;
        }
    }

    for (const auto& risk : detection.recommendation.degradation_risks) {
        if (risk.subsystem == "Sparkle auto-updater") {
            caps.needs_autoupdater_disabled = true;
        }
    }

    return caps;
}

} // namespace macrun
