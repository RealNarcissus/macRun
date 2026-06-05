// Stage 4: Capability Scoring
// Architecture reference: docs/architecture/ARCHITECTURE_V6.md — "Stage 4 — Capability Scoring"
//
// Maps framework detections to weighted risk scores using the exact table
// from the architecture spec.

#include "detector.hpp"
#include "fingerprint.hpp"

namespace platform {

// Weight mapping from architecture's Capability Scoring table
static unsigned impact_to_score(CapabilityImpact impact) {
    switch (impact) {
        case CapabilityImpact::Critical: return 10;
        case CapabilityImpact::High:     return 7;
        case CapabilityImpact::Medium:   return 4;
        case CapabilityImpact::Low:      return 1;
        case CapabilityImpact::None:     return 0;
    }
    return 0;
}

CapabilityScore compute_capability_score(
    const std::vector<DetectedFramework>& frameworks,
    const MachOInfo& macho,
    const EntitlementInfo& entitlements)
{
    CapabilityScore score;

    // Architecturally defined dimensions from ARCHITECTURE_V6.md Stage 4 table:
    struct DimensionTemplate {
        FrameworkId      framework;
        std::string      name;
        CapabilityImpact impact;
    };

    // Each row in the architecture's scoring table becomes a dimension
    static const DimensionTemplate templates[] = {
        {FrameworkId::SwiftUI,       "SwiftUI dependency",  CapabilityImpact::High},
        {FrameworkId::Metal,         "Metal dependency",    CapabilityImpact::High},
        {FrameworkId::Accessibility, "Accessibility hooks", CapabilityImpact::Critical},
        {FrameworkId::XPCService,    "XPC complexity",      CapabilityImpact::Medium},
        {FrameworkId::CoreData,      "CoreData usage",      CapabilityImpact::Medium},
        {FrameworkId::Electron,      "Electron runtime",    CapabilityImpact::Low},
        {FrameworkId::WKWebView,     "WKWebView usage",     CapabilityImpact::Medium},
        {FrameworkId::Hypervisor,    "Hypervisor dependency", CapabilityImpact::Critical},
        {FrameworkId::Tauri,         "Tauri runtime",       CapabilityImpact::Low},
        {FrameworkId::Wails,         "Wails runtime",       CapabilityImpact::Low},
        {FrameworkId::AppKit,        "AppKit dependency",   CapabilityImpact::Medium},
        {FrameworkId::CoreAnimation,"CoreAnimation dependency", CapabilityImpact::High},
        {FrameworkId::PrivateFramework, "Private framework usage", CapabilityImpact::High},
    };

    // Build detection lookup
    auto is_detected = [&](FrameworkId id) -> const DetectedFramework* {
        for (const auto& f : frameworks) {
            if (f.id == id) return &f;
        }
        return nullptr;
    };

    for (const auto& tmpl : templates) {
        CapabilityDimension dim;
        dim.name         = tmpl.name;
        dim.impact_weight = tmpl.impact;

        const auto* det = is_detected(tmpl.framework);
        dim.detected = (det != nullptr);

        if (dim.detected) {
            dim.detail = det->evidence;
            score.risk_score_total += impact_to_score(tmpl.impact);
            switch (tmpl.impact) {
                case CapabilityImpact::Critical: score.critical_count++; break;
                case CapabilityImpact::High:     score.high_count++;     break;
                case CapabilityImpact::Medium:   score.medium_count++;   break;
                case CapabilityImpact::Low:      score.low_count++;      break;
                default: break;
            }
        } else {
            dim.detail = "not detected";
        }

        score.dimensions.push_back(std::move(dim));
    }

    // Additional risk signals not from framework table but from architecture analysis
    if (macho.architectures_present.size() > 1) {
        // Universal binary — lower risk (more compatible)
        // Don't penalize, just note it
    }

    if (macho.primary_architecture == BinaryArchitecture::ARM64 &&
        !entitlements.uses_hypervisor) {
        // ARM64 binary needs translation on x86 Linux host — medium inherent risk
        CapabilityDimension arm64_dim;
        arm64_dim.name = "ARM64 binary on x86 host";
        arm64_dim.impact_weight = CapabilityImpact::Medium;
        arm64_dim.detected = true;
        arm64_dim.detail = "ARM64-only binary requires translation";
        score.dimensions.push_back(arm64_dim);
        score.risk_score_total += impact_to_score(CapabilityImpact::Medium);
        score.medium_count++;
    }

    return score;
}

std::vector<DetectedFramework> fingerprint_frameworks(
    const BundleInfo& bundle,
    const MachOInfo& macho,
    const EntitlementInfo& entitlements)
{
    FingerprintRegistry registry;
    return registry.detect(bundle, macho, entitlements);
}

std::vector<DetectedFramework> fingerprint_frameworks(
    FingerprintRegistry& registry,
    const BundleInfo& bundle,
    const MachOInfo& macho,
    const EntitlementInfo& entitlements)
{
    return registry.detect(bundle, macho, entitlements);
}

} // namespace platform
