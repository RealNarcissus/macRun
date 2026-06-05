// Stage 2: Framework Fingerprinting — Implementation
// Architecture reference: docs/architecture/ARCHITECTURE_V6.md — "Stage 2 — Framework Fingerprinting"
#include "fingerprint.hpp"
#include <functional>
#include <algorithm>

namespace platform {

static bool has_lib(const MachOInfo& macho, const std::string& substr) {
    for (const auto& lib : macho.linked_libraries) {
        if (lib.find(substr) != std::string::npos) return true;
    }
    return false;
}

static bool has_rpath(const MachOInfo& macho, const std::string& substr) {
    for (const auto& rp : macho.rpaths) {
        if (rp.find(substr) != std::string::npos) return true;
    }
    return false;
}

static bool bid_contains(const BundleInfo& bundle, const std::string& substr) {
    std::string lower_id = bundle.bundle_identifier;
    std::string lower_sub = substr;
    std::transform(lower_id.begin(), lower_id.end(), lower_id.begin(), ::tolower);
    std::transform(lower_sub.begin(), lower_sub.end(), lower_sub.begin(), ::tolower);
    return lower_id.find(lower_sub) != std::string::npos;
}

using DetectFn = std::function<bool(const BundleInfo&, const MachOInfo&, const EntitlementInfo&)>;
using EvidenceFn = std::function<std::string(const BundleInfo&, const MachOInfo&, const EntitlementInfo&)>;

FingerprintRegistry::FingerprintRegistry() {
    checks_.emplace_back(
        FingerprintRule{FrameworkId::Electron, CapabilityImpact::Low, "Electron framework detected"},
        DetectFn([](const BundleInfo& b, const MachOInfo& m, const EntitlementInfo&) {
            if (has_lib(m, "Electron Framework.framework")) return true;
            if (has_rpath(m, "Electron Framework")) return true;
            if (bid_contains(b, "com.github.Electron")) return true;
            return false;
        }),
        EvidenceFn([](const BundleInfo& b, const MachOInfo& m, const EntitlementInfo&) -> std::string {
            if (has_lib(m, "Electron Framework.framework"))
                return "Linked against Electron Framework.framework";
            if (has_rpath(m, "Electron Framework"))
                return "RPATH references Electron Framework";
            return "Bundle identifier matches Electron pattern";
        })
    );

    checks_.emplace_back(
        FingerprintRule{FrameworkId::Tauri, CapabilityImpact::Low, "Tauri framework detected"},
        DetectFn([](const BundleInfo& b, const MachOInfo& m, const EntitlementInfo&) {
            if (bid_contains(b, "com.tauri")) return true;
            if (has_lib(m, "WebKit") && !has_lib(m, "Electron")) {
                if (b.bundle_identifier.find("tauri") != std::string::npos) return true;
            }
            return false;
        }),
        EvidenceFn([](const BundleInfo& b, const MachOInfo&, const EntitlementInfo&) -> std::string {
            return "Bundle identifier indicates Tauri: " + b.bundle_identifier;
        })
    );

    checks_.emplace_back(
        FingerprintRule{FrameworkId::Wails, CapabilityImpact::Low, "Wails framework detected"},
        DetectFn([](const BundleInfo& b, const MachOInfo&, const EntitlementInfo&) {
            return bid_contains(b, "com.wails");
        }),
        EvidenceFn([](const BundleInfo& b, const MachOInfo&, const EntitlementInfo&) -> std::string {
            return "Bundle identifier indicates Wails: " + b.bundle_identifier;
        })
    );

    checks_.emplace_back(
        FingerprintRule{FrameworkId::AppKit, CapabilityImpact::Medium, "AppKit framework detected"},
        DetectFn([](const BundleInfo& b, const MachOInfo& m, const EntitlementInfo&) {
            if (has_lib(m, "AppKit")) return true;
            if (b.has_ns_principal_class) return true;
            if (b.has_ns_main_nib_file) return true;
            if (has_lib(m, "Cocoa")) return true;
            return false;
        }),
        EvidenceFn([](const BundleInfo& b, const MachOInfo& m, const EntitlementInfo&) -> std::string {
            if (has_lib(m, "AppKit")) return "Linked against AppKit.framework";
            if (b.has_ns_principal_class) return "Has NSPrincipalClass in Info.plist";
            if (b.has_ns_main_nib_file) return "Has NSMainNibFile in Info.plist";
            return "Linked against Cocoa.framework";
        })
    );

    checks_.emplace_back(
        FingerprintRule{FrameworkId::SwiftUI, CapabilityImpact::High, "SwiftUI framework detected"},
        DetectFn([](const BundleInfo&, const MachOInfo& m, const EntitlementInfo&) {
            return has_lib(m, "SwiftUI");
        }),
        EvidenceFn([](const BundleInfo&, const MachOInfo&, const EntitlementInfo&) -> std::string {
            return "Linked against SwiftUI.framework";
        })
    );

    checks_.emplace_back(
        FingerprintRule{FrameworkId::Metal, CapabilityImpact::High, "Metal framework detected"},
        DetectFn([](const BundleInfo&, const MachOInfo& m, const EntitlementInfo&) {
            if (has_lib(m, "Metal")) return true;
            if (has_lib(m, "MetalKit")) return true;
            if (has_lib(m, "MetalPerformanceShaders")) return true;
            if (has_lib(m, "MetalFX")) return true;
            return false;
        }),
        EvidenceFn([](const BundleInfo&, const MachOInfo& m, const EntitlementInfo&) -> std::string {
            if (has_lib(m, "MetalKit")) return "Linked against MetalKit.framework";
            if (has_lib(m, "MetalPerformanceShaders")) return "Linked against MetalPerformanceShaders.framework";
            return "Linked against Metal.framework";
        })
    );

    checks_.emplace_back(
        FingerprintRule{FrameworkId::Accessibility, CapabilityImpact::Critical, "Accessibility API usage detected"},
        DetectFn([](const BundleInfo&, const MachOInfo& m, const EntitlementInfo& e) {
            if (has_lib(m, "Accessibility")) return true;
            if (e.uses_accessibility) return true;
            return false;
        }),
        EvidenceFn([](const BundleInfo&, const MachOInfo& m, const EntitlementInfo& e) -> std::string {
            if (e.uses_accessibility) return "Entitlements grant accessibility access";
            return "Linked against Accessibility.framework";
        })
    );

    checks_.emplace_back(
        FingerprintRule{FrameworkId::Hypervisor, CapabilityImpact::Critical, "Hypervisor.framework detected"},
        DetectFn([](const BundleInfo&, const MachOInfo& m, const EntitlementInfo& e) {
            if (has_lib(m, "Hypervisor")) return true;
            if (e.uses_hypervisor) return true;
            return false;
        }),
        EvidenceFn([](const BundleInfo&, const MachOInfo& m, const EntitlementInfo& e) -> std::string {
            if (e.uses_hypervisor) return "Entitlements grant hypervisor access";
            return "Linked against Hypervisor.framework";
        })
    );

    checks_.emplace_back(
        FingerprintRule{FrameworkId::XPCService, CapabilityImpact::Medium, "XPC services detected"},
        DetectFn([](const BundleInfo&, const MachOInfo& m, const EntitlementInfo&) {
            if (has_lib(m, "XPCService")) return true;
            if (has_lib(m, "libxpc")) return true;
            return false;
        }),
        EvidenceFn([](const BundleInfo&, const MachOInfo& m, const EntitlementInfo&) -> std::string {
            if (has_lib(m, "libxpc")) return "Linked against libxpc.dylib";
            return "Linked against XPCService.framework";
        })
    );

    checks_.emplace_back(
        FingerprintRule{FrameworkId::CoreData, CapabilityImpact::Medium, "CoreData framework detected"},
        DetectFn([](const BundleInfo&, const MachOInfo& m, const EntitlementInfo&) {
            return has_lib(m, "CoreData");
        }),
        EvidenceFn([](const BundleInfo&, const MachOInfo&, const EntitlementInfo&) -> std::string {
            return "Linked against CoreData.framework";
        })
    );

    checks_.emplace_back(
        FingerprintRule{FrameworkId::WKWebView, CapabilityImpact::Medium, "WKWebView detected"},
        DetectFn([](const BundleInfo&, const MachOInfo& m, const EntitlementInfo&) {
            if (has_lib(m, "WebKit")) return true;
            if (has_lib(m, "WKWebView")) return true;
            return false;
        }),
        EvidenceFn([](const BundleInfo&, const MachOInfo& m, const EntitlementInfo&) -> std::string {
            if (has_lib(m, "WebKit")) return "Linked against WebKit.framework";
            return "Linked against WKWebView";
        })
    );

    checks_.emplace_back(
        FingerprintRule{FrameworkId::CloudKit, CapabilityImpact::Low, "CloudKit framework detected"},
        DetectFn([](const BundleInfo&, const MachOInfo& m, const EntitlementInfo&) {
            return has_lib(m, "CloudKit");
        }),
        EvidenceFn([](const BundleInfo&, const MachOInfo&, const EntitlementInfo&) -> std::string {
            return "Linked against CloudKit.framework";
        })
    );

    checks_.emplace_back(
        FingerprintRule{FrameworkId::Sparkle, CapabilityImpact::Low, "Sparkle update framework detected"},
        DetectFn([](const BundleInfo&, const MachOInfo& m, const EntitlementInfo&) {
            if (has_lib(m, "Sparkle")) return true;
            if (has_rpath(m, "Sparkle")) return true;
            return false;
        }),
        EvidenceFn([](const BundleInfo&, const MachOInfo& m, const EntitlementInfo&) -> std::string {
            return "Linked against Sparkle.framework";
        })
    );

    checks_.emplace_back(
        FingerprintRule{FrameworkId::CoreAnimation, CapabilityImpact::High, "CoreAnimation framework detected"},
        DetectFn([](const BundleInfo&, const MachOInfo& m, const EntitlementInfo&) {
            if (has_lib(m, "QuartzCore")) return true;
            if (has_lib(m, "CoreAnimation")) return true;
            if (has_lib(m, "AppKit")) return true;
            return false;
        }),
        EvidenceFn([](const BundleInfo&, const MachOInfo& m, const EntitlementInfo&) -> std::string {
            if (has_lib(m, "QuartzCore")) return "Linked against QuartzCore.framework";
            if (has_lib(m, "AppKit")) return "Detected via AppKit dependency (implicit CoreAnimation)";
            return "Linked against CoreAnimation";
        })
    );

    checks_.emplace_back(
        FingerprintRule{FrameworkId::AVFoundation, CapabilityImpact::Medium, "AVFoundation framework detected"},
        DetectFn([](const BundleInfo&, const MachOInfo& m, const EntitlementInfo&) {
            return has_lib(m, "AVFoundation");
        }),
        EvidenceFn([](const BundleInfo&, const MachOInfo&, const EntitlementInfo&) -> std::string {
            return "Linked against AVFoundation.framework";
        })
    );

    checks_.emplace_back(
        FingerprintRule{FrameworkId::PrivateFramework, CapabilityImpact::High, "Private framework usage detected"},
        DetectFn([](const BundleInfo&, const MachOInfo& m, const EntitlementInfo&) {
            for (const auto& lib : m.linked_libraries) {
                if (lib.find("/System/Library/PrivateFrameworks/") != std::string::npos)
                    return true;
                if (lib.find("PrivateFrameworks") != std::string::npos)
                    return true;
            }
            return false;
        }),
        EvidenceFn([](const BundleInfo&, const MachOInfo& m, const EntitlementInfo&) -> std::string {
            for (const auto& lib : m.linked_libraries) {
                if (lib.find("PrivateFrameworks") != std::string::npos)
                    return "Links to private framework: " + lib;
            }
            return "Private framework linkage detected";
        })
    );

    checks_.emplace_back(
        FingerprintRule{FrameworkId::Rosetta, CapabilityImpact::Low, "Rosetta translation assumed"},
        DetectFn([](const BundleInfo&, const MachOInfo& m, const EntitlementInfo&) {
            if (m.primary_architecture == BinaryArchitecture::ARM64) return false;
            if (m.primary_architecture == BinaryArchitecture::Universal) return false;
            uint32_t min_ver = m.min_os_deployment;
            if (min_ver > 0) {
                uint32_t major = (min_ver >> 16) & 0xffff;
                return major >= 11;
            }
            return false;
        }),
        EvidenceFn([](const BundleInfo&, const MachOInfo& m, const EntitlementInfo&) -> std::string {
            return "x86_64 binary with macOS 11+ deployment target (Rosetta 2 assumed)";
        })
    );
}

void FingerprintRegistry::add_rule(FingerprintRule rule, DetectFn detect_fn, EvidenceFn evidence_fn) {
    checks_.push_back(Check{std::move(rule), std::move(detect_fn), std::move(evidence_fn)});
}

std::vector<DetectedFramework> FingerprintRegistry::detect(
    const BundleInfo& bundle,
    const MachOInfo& macho,
    const EntitlementInfo& entitlements) const
{
    std::vector<DetectedFramework> results;

    for (const auto& check : checks_) {
        if (check.fn(bundle, macho, entitlements)) {
            DetectedFramework df;
            df.id       = check.rule.framework;
            df.impact   = check.rule.impact;
            df.evidence = check.evidence(bundle, macho, entitlements);
            results.push_back(df);
        }
    }

    return results;
}

} // namespace platform
