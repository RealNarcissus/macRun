// Stage 2: Framework Fingerprinting Registry
// Architecture reference: docs/architecture/ARCHITECTURE_V6.md — "Stage 2 — Framework Fingerprinting"
//
// This registry maps detection rules to framework IDs.
// Each rule is a deterministic check — no ML, no heuristics that vary between runs.
// Rules can be added dynamically for runtime expansion without modifying the pipeline.

#pragma once
#include "types.hpp"
#include <string>
#include <vector>
#include <functional>

namespace platform {

struct FingerprintRule {
    FrameworkId      framework;
    CapabilityImpact impact;
    std::string      description;
};

class FingerprintRegistry {
public:
    FingerprintRegistry();

    using DetectFn  = std::function<bool(const BundleInfo&, const MachOInfo&, const EntitlementInfo&)>;
    using EvidenceFn = std::function<std::string(const BundleInfo&, const MachOInfo&, const EntitlementInfo&)>;

    // Returns list of frameworks detected, with evidence.
    // Iterates both static checks (constructor) and dynamically added rules.
    std::vector<DetectedFramework> detect(
        const BundleInfo& bundle,
        const MachOInfo& macho,
        const EntitlementInfo& entitlements) const;

    // Add a custom detection rule with check function for future expansion.
    // Dynamically registered rules participate in detection alongside built-in rules.
    void add_rule(FingerprintRule rule, DetectFn detect_fn, EvidenceFn evidence_fn);

private:
    struct Check {
        FingerprintRule rule;
        DetectFn fn;
        EvidenceFn evidence;
    };

    std::vector<Check> checks_;
};

} // namespace platform
