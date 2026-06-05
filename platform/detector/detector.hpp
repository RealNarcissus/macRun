#pragma once
#include "fingerprint.hpp"
#include "types.hpp"

namespace platform {

// Stage 1
BundleInfo analyze_bundle(const std::string& app_bundle_path);

// Stage 2 — default registry
std::vector<DetectedFramework> fingerprint_frameworks(
    const BundleInfo& bundle,
    const MachOInfo& macho,
    const EntitlementInfo& entitlements);

// Stage 2 — with custom/injectable registry for runtime extension
std::vector<DetectedFramework> fingerprint_frameworks(
    FingerprintRegistry& registry,
    const BundleInfo& bundle,
    const MachOInfo& macho,
    const EntitlementInfo& entitlements);

// Stage 3
MachOInfo analyze_macho(const std::string& executable_path);
MachOInfo analyze_macho_from_buffer(const uint8_t* data, size_t size);

// Stage 3b
EntitlementInfo parse_entitlements(const std::string& entitlements_xml);

// Stage 4
CapabilityScore compute_capability_score(
    const std::vector<DetectedFramework>& frameworks,
    const MachOInfo& macho,
    const EntitlementInfo& entitlements);

// Stage 5
TierRecommendation resolve_execution_strategy(
    const CapabilityScore& score,
    const std::vector<DetectedFramework>& frameworks,
    const MachOInfo& macho,
    const BundleInfo& bundle);

// Full pipeline with default registry
DetectionResult detect_capabilities(const std::string& app_bundle_path);

// Full pipeline with caller-provided registry (enables dynamic rule injection)
DetectionResult detect_capabilities(FingerprintRegistry& registry, const std::string& app_bundle_path);

} // namespace platform
