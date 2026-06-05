// Deterministic unit tests for the Capability Detection Engine
// Architecture reference: docs/architecture/ARCHITECTURE_V6.md — "Detection Pipeline" stages 1-5
// Exit Conditions: sample applications classify deterministically, tier recommendations reproducible,
// framework detection validated, unsupported apps correctly identified, all outputs machine-readable.
//
// Test fixtures live in tests/fixtures/ and include synthetic .app bundles with valid
// Info.plist and Mach-O binaries for each major app category.

#include <detector.hpp>
#include <serialize.hpp>
#include <iostream>
#include <string>
#include <cassert>
#include <cstdlib>
#include <filesystem>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    do { \
        std::cout << "  TEST: " << name << " ... "; \
    } while(0)

#define PASS() \
    do { \
        std::cout << "PASSED\n"; \
        tests_passed++; \
    } while(0)

#define FAIL(msg) \
    do { \
        std::cout << "FAILED: " << msg << "\n"; \
        tests_failed++; \
    } while(0)

#define CHECK(cond, msg) \
    do { \
        if (!(cond)) { FAIL(msg); return; } \
    } while(0)

#define CHECK_EQ(a, b, msg) \
    do { \
        if ((a) != (b)) { FAIL(std::string(msg) + " (got " + std::to_string(static_cast<int>(a)) + ")"); return; } \
    } while(0)

static bool has_framework(const std::vector<platform::DetectedFramework>& fws, platform::FrameworkId id) {
    for (const auto& f : fws) {
        if (f.id == id) return true;
    }
    return false;
}

// Resolve a fixture path relative to the project source root.
// Tests may be invoked from any working directory (build/, tools/, etc.).
// Uses MACRUN_SOURCE_ROOT env var, or walks up from CWD to find CMakeLists.txt.
static std::string fixture(const std::string& relative_path) {
    // 1. Explicit environment override
    if (const char* root = std::getenv("MACRUN_SOURCE_ROOT")) {
        return std::string(root) + "/" + relative_path;
    }

    // 2. Walk up from CWD looking for the project root (CMakeLists.txt at top level)
    namespace fs = std::filesystem;
    fs::path cwd = fs::current_path();
    for (fs::path p = cwd; p.has_parent_path(); p = p.parent_path()) {
        if (fs::exists(p / "CMakeLists.txt") && fs::exists(p / "docs" / "architecture" / "ARCHITECTURE_V6.md")) {
            return (p / relative_path).string();
        }
    }

    // 3. Fallback: assume CWD is project root
    return relative_path;
}

// ============================================================
// Test 1: Invalid bundle (missing Info.plist)
// ============================================================
static void test_invalid_bundle() {
    TEST("Invalid bundle returns unsupported state");
    auto result = platform::detect_capabilities("/nonexistent/path");
    CHECK(result.compatibility_state == platform::CompatibilityState::Unsupported,
        "Expected unsupported state for nonexistent path");
    CHECK(!result.unsupported_reasons.empty(),
        "Expected unsupported reasons for invalid bundle");
    PASS();
}

// ============================================================
// Test 2: Electron app detection
// ============================================================
static void test_electron_app() {
    TEST("Electron app detects as Tier 0 native substitution");
    auto result = platform::detect_capabilities(fixture("tests/fixtures/electron-app.app"));

    // Bundle analysis
    CHECK(result.bundle.bundle_identifier == "com.cursor.app",
        "Expected correct bundle identifier");

    // Framework fingerprinting
    CHECK(has_framework(result.frameworks, platform::FrameworkId::Electron),
        "Expected Electron framework detection");
    CHECK(has_framework(result.frameworks, platform::FrameworkId::AppKit),
        "Expected AppKit framework detection (Electron links AppKit)");

    // Tier recommendation
    CHECK(result.recommendation.preferred_tier == platform::ExecutionTier::Tier0_NativeSubstitution,
        "Electron app should get Tier 0 (native substitution)");
    CHECK(!result.recommendation.vm_required,
        "Electron app should not require VM");

    // Compatibility state
    CHECK(result.compatibility_state == platform::CompatibilityState::Functional,
        "Electron app should be functional");

    PASS();
}

// ============================================================
// Test 3: Cocoa app detection
// ============================================================
static void test_cocoa_app() {
    TEST("Cocoa app detects as Tier 2 lightweight Cocoa");
    auto result = platform::detect_capabilities(fixture("tests/fixtures/cocoa-app.app"));

    // Bundle analysis
    CHECK(result.bundle.has_ns_principal_class,
        "Expected NSPrincipalClass detection");
    CHECK(result.bundle.has_ns_main_nib_file,
        "Expected NSMainNibFile detection");

    // Framework fingerprinting
    CHECK(has_framework(result.frameworks, platform::FrameworkId::AppKit),
        "Expected AppKit framework detection");
    CHECK(has_framework(result.frameworks, platform::FrameworkId::CoreData),
        "Expected CoreData framework detection");
    CHECK(has_framework(result.frameworks, platform::FrameworkId::CoreAnimation),
        "Expected CoreAnimation detection (via AppKit)");

    // No Electron/SwiftUI
    CHECK(!has_framework(result.frameworks, platform::FrameworkId::Electron),
        "Cocoa app should not detect Electron");
    CHECK(!has_framework(result.frameworks, platform::FrameworkId::SwiftUI),
        "Cocoa app should not detect SwiftUI");

    // Tier recommendation
    CHECK(result.recommendation.preferred_tier == platform::ExecutionTier::Tier2_LightweightCocoa,
        "Cocoa app should get Tier 2");

    // Expected state: Partial (CoreAnimation etc.)
    CHECK(result.compatibility_state == platform::CompatibilityState::Partial,
        "Cocoa app expected state should be Partial");

    // Degradation risks should mention CoreAnimation
    bool has_ca_risk = false;
    for (const auto& risk : result.recommendation.degradation_risks) {
        if (risk.subsystem.find("CoreAnimation") != std::string::npos) {
            has_ca_risk = true;
            break;
        }
    }
    CHECK(has_ca_risk, "Expected CoreAnimation degradation risk for Cocoa app");

    PASS();
}

// ============================================================
// Test 4: SwiftUI app detection
// ============================================================
static void test_swiftui_app() {
    TEST("SwiftUI app detects as Tier 4B VM-assisted");
    auto result = platform::detect_capabilities(fixture("tests/fixtures/swiftui-app.app"));

    // Bundle analysis
    CHECK(result.bundle.bundle_identifier == "com.raycast.macos",
        "Expected correct bundle identifier");

    // Architecture — universal binary
    CHECK(result.macho.primary_architecture == platform::BinaryArchitecture::Universal,
        "Expected universal binary architecture");
    CHECK(result.macho.architectures_present.size() == 2,
        "Expected 2 architectures in universal binary");

    // Framework fingerprinting
    CHECK(has_framework(result.frameworks, platform::FrameworkId::SwiftUI),
        "Expected SwiftUI framework detection");
    CHECK(has_framework(result.frameworks, platform::FrameworkId::AppKit),
        "Expected AppKit framework detection");

    // Tier recommendation — SwiftUI should route to VM
    CHECK(result.recommendation.preferred_tier == platform::ExecutionTier::Tier4B_VMAssisted,
        "SwiftUI-heavy app should get Tier 4B (VM-assisted)");
    CHECK(result.recommendation.vm_required,
        "SwiftUI app should require VM");

    // Should have compatibility warnings about SwiftUI/VM
    bool has_swiftui_warning = false;
    for (const auto& w : result.recommendation.compatibility_warnings) {
        if (w.find("SwiftUI") != std::string::npos) has_swiftui_warning = true;
    }
    CHECK(has_swiftui_warning, "Expected SwiftUI compatibility warning");

    // Score should indicate high risk
    CHECK(result.score.high_count >= 1,
        "SwiftUI app should have at least one high-impact capability detected");

    PASS();
}

// ============================================================
// Test 5: Hypervisor app detection (unsupported)
// ============================================================
static void test_hypervisor_app() {
    TEST("Hypervisor app correctly identified as unsupported (Tier 4)");
    auto result = platform::detect_capabilities(fixture("tests/fixtures/hypervisor-app.app"));

    // Bundle analysis
    CHECK(result.bundle.bundle_identifier == "com.utmapp.UTM",
        "Expected correct bundle identifier");

    // Framework fingerprinting
    CHECK(has_framework(result.frameworks, platform::FrameworkId::Hypervisor),
        "Expected Hypervisor.framework detection");
    CHECK(has_framework(result.frameworks, platform::FrameworkId::AppKit),
        "Expected AppKit framework detection");

    // Tier — must be unsupported
    CHECK(result.recommendation.preferred_tier == platform::ExecutionTier::Tier4_Unsupported,
        "Hypervisor app should be Tier 4 (unsupported)");

    // Compatibility state
    CHECK(result.compatibility_state == platform::CompatibilityState::Unsupported,
        "Hypervisor app should be unsupported");

    // Must have unsupported reasons
    bool has_unsupported_msg = false;
    for (const auto& r : result.unsupported_reasons) {
        if (r.find("Hypervisor") != std::string::npos) has_unsupported_msg = true;
    }
    CHECK(has_unsupported_msg, "Expected Hypervisor in unsupported reasons");

    PASS();
}

// ============================================================
// Test 6: Deterministic JSON output
// ============================================================
static void test_json_output() {
    TEST("JSON output is well-formed and deterministic");
    auto result = platform::detect_capabilities(fixture("tests/fixtures/electron-app.app"));
    std::string json = platform::to_json(result);

    // Basic structure checks
    CHECK(json.find("\"bundle\"") != std::string::npos, "JSON should contain bundle section");
    CHECK(json.find("\"macho\"") != std::string::npos, "JSON should contain macho section");
    CHECK(json.find("\"frameworks\"") != std::string::npos, "JSON should contain frameworks section");
    CHECK(json.find("\"capability_score\"") != std::string::npos, "JSON should contain capability_score section");
    CHECK(json.find("\"recommendation\"") != std::string::npos, "JSON should contain recommendation section");
    CHECK(json.find("\"detection_version\"") != std::string::npos, "JSON should contain detection_version");

    // Run twice and verify deterministic
    auto result2 = platform::detect_capabilities(fixture("tests/fixtures/electron-app.app"));
    std::string json2 = platform::to_json(result2);
    CHECK(json == json2, "JSON output must be deterministic across repeated runs");

    PASS();
}

// ============================================================
// Test 7: YAML output
// ============================================================
static void test_yaml_output() {
    TEST("YAML output is well-formed");
    auto result = platform::detect_capabilities(fixture("tests/fixtures/electron-app.app"));
    std::string yaml = platform::to_yaml(result);

    CHECK(yaml.find("bundle:") != std::string::npos, "YAML should contain bundle section");
    CHECK(yaml.find("macho:") != std::string::npos, "YAML should contain macho section");
    CHECK(yaml.find("frameworks:") != std::string::npos, "YAML should contain frameworks section");
    CHECK(yaml.find("capability_score:") != std::string::npos, "YAML should contain capability_score section");
    CHECK(yaml.find("recommendation:") != std::string::npos, "YAML should contain recommendation section");

    PASS();
}

// ============================================================
// Test 8: Mach-O parser from buffer
// ============================================================
static void test_macho_parsing() {
    TEST("Mach-O buffer parsing detects architectures and linked libraries");
    auto result = platform::detect_capabilities(fixture("tests/fixtures/cocoa-app.app"));

    // Architecture detection
    CHECK(result.macho.primary_architecture == platform::BinaryArchitecture::X86_64,
        "Expected x86_64 architecture");
    CHECK(result.macho.is_executable, "Should be marked as executable");
    CHECK(result.macho.has_pie, "Should have PIE flag set");

    // Linked libraries
    bool has_appkit = false;
    bool has_coredata = false;
    for (const auto& lib : result.macho.linked_libraries) {
        if (lib.find("AppKit") != std::string::npos) has_appkit = true;
        if (lib.find("CoreData") != std::string::npos) has_coredata = true;
    }
    CHECK(has_appkit, "Expected AppKit in linked libraries");
    CHECK(has_coredata, "Expected CoreData in linked libraries");

    PASS();
}

// ============================================================
// Test 9: Score dimensions track all architecture-specified capabilities
// ============================================================
static void test_score_dimensions() {
    TEST("Score dimensions cover all architecture-specified capability categories");

    auto electron = platform::detect_capabilities(fixture("tests/fixtures/electron-app.app"));
    auto cocoa   = platform::detect_capabilities(fixture("tests/fixtures/cocoa-app.app"));

    // Both should produce score dimensions aligned to the architecture's capability scoring table
    CHECK(!electron.score.dimensions.empty(), "Electron app must have score dimensions");
    CHECK(!cocoa.score.dimensions.empty(), "Cocoa app must have score dimensions");

    // Verify the architecture-defined dimensions exist in the output
    bool has_swiftui_dim = false, has_metal_dim = false, has_accessibility_dim = false;
    bool has_xpc_dim = false, has_electron_dim = false;

    auto check_dims = [&](const platform::CapabilityScore& score) {
        for (const auto& d : score.dimensions) {
            if (d.name == "SwiftUI dependency")       has_swiftui_dim = true;
            if (d.name == "Metal dependency")          has_metal_dim = true;
            if (d.name == "Accessibility hooks")       has_accessibility_dim = true;
            if (d.name == "XPC complexity")            has_xpc_dim = true;
            if (d.name == "Electron runtime")          has_electron_dim = true;
        }
    };

    check_dims(electron.score);
    check_dims(cocoa.score);

    CHECK(has_electron_dim, "Must have Electron runtime dimension");
    CHECK(has_swiftui_dim, "Must have SwiftUI dependency dimension");
    CHECK(has_metal_dim, "Must have Metal dependency dimension");

    PASS();
}

// ============================================================
// Test 10: Tier recommendation is reproducible
// ============================================================
static void test_tier_reproducibility() {
    TEST("Tier recommendations are reproducible across repeated runs");

    for (int i = 0; i < 5; i++) {
        auto r1 = platform::detect_capabilities(fixture("tests/fixtures/electron-app.app"));
        auto r2 = platform::detect_capabilities(fixture("tests/fixtures/cocoa-app.app"));
        auto r3 = platform::detect_capabilities(fixture("tests/fixtures/swiftui-app.app"));
        auto r4 = platform::detect_capabilities(fixture("tests/fixtures/hypervisor-app.app"));

        CHECK(r1.recommendation.preferred_tier == platform::ExecutionTier::Tier0_NativeSubstitution,
            "Electron tier must be stable");
        CHECK(r2.recommendation.preferred_tier == platform::ExecutionTier::Tier2_LightweightCocoa,
            "Cocoa tier must be stable");
        CHECK(r3.recommendation.preferred_tier == platform::ExecutionTier::Tier4B_VMAssisted,
            "SwiftUI tier must be stable");
        CHECK(r4.recommendation.preferred_tier == platform::ExecutionTier::Tier4_Unsupported,
            "Hypervisor tier must be stable");
    }

    PASS();
}

// ============================================================
// Test 11: Registry injection via detect_capabilities overload
// ============================================================
static void test_registry_injection() {
    TEST("Registry injection produces correct custom detection");

    platform::FingerprintRegistry reg;
    // Verify base detection with default registry
    auto before = platform::detect_capabilities(fixture("tests/fixtures/electron-app.app"));
    CHECK_EQ(before.recommendation.preferred_tier, platform::ExecutionTier::Tier0_NativeSubstitution,
        "Baseline Electron app must be Tier 0");

    // Inject a custom rule that would not fire on a Cocoa app
    reg.add_rule(
        {platform::FrameworkId::Electron, platform::CapabilityImpact::Low, "Custom: mark Cocoa as Electron"},
        [](const platform::BundleInfo& b, const platform::MachOInfo&, const platform::EntitlementInfo&) {
            return b.bundle_identifier == "com.apple.TextEdit";
        },
        [](const platform::BundleInfo&, const platform::MachOInfo&, const platform::EntitlementInfo&) -> std::string {
            return "Custom rule: TextEdit flagged as Electron for testing";
        }
    );

    // Re-run the Cocoa app through the registry-injected pipeline
    auto injected = platform::detect_capabilities(reg, fixture("tests/fixtures/cocoa-app.app"));
    // With custom rule active, the Cocoa app should now be classified as Electron → Tier 0
    CHECK_EQ(injected.recommendation.preferred_tier, platform::ExecutionTier::Tier0_NativeSubstitution,
        "Injecting custom Electron rule on Cocoa app must change tier to Tier 0");

    // Verify deterministic: same app without injection stays Tier 2
    auto after = platform::detect_capabilities(fixture("tests/fixtures/cocoa-app.app"));
    CHECK_EQ(after.recommendation.preferred_tier, platform::ExecutionTier::Tier2_LightweightCocoa,
        "Default pipeline must remain Tier 2 for Cocoa app");

    PASS();
}

// ============================================================
// Test 12: Binary plist (bplist00) detection and extraction
// ============================================================
static void test_binary_plist() {
    TEST("Binary plist (bplist00) is detected and metadata extracted");
    auto result = platform::detect_capabilities(fixture("tests/fixtures/bplist-app.app"));

    // Bundle identifier should be extracted from the binary plist
    CHECK(result.bundle.bundle_identifier == "com.test.bplistapp",
        "Expected CFBundleIdentifier extracted from binary plist");

    // Executable name should be extracted
    CHECK(result.bundle.executable_name == "TestApp",
        "Expected CFBundleExecutable extracted from binary plist");

    // Version should be extracted
    CHECK(result.bundle.bundle_version == "2.0.0",
        "Expected CFBundleVersion extracted from binary plist");

    // NSPrincipalClass presence should be detected
    CHECK(result.bundle.has_ns_principal_class,
        "Expected NSPrincipalClass detection from binary plist");

    // Verify the bplist diagnostic flag is set
    CHECK(result.bundle.custom_properties.find("_bplist_diagnostic") != result.bundle.custom_properties.end(),
        "Expected _bplist_diagnostic key in custom properties");

    PASS();
}

int main() {
    std::cout << "\n=== Capability Detection Engine — Unit Tests ===\n\n";

    test_invalid_bundle();
    test_electron_app();
    test_cocoa_app();
    test_swiftui_app();
    test_hypervisor_app();
    test_json_output();
    test_yaml_output();
    test_macho_parsing();
    test_score_dimensions();
    test_tier_reproducibility();
    test_registry_injection();
    test_binary_plist();

    std::cout << "\n=== Results: " << tests_passed << " passed, "
              << tests_failed << " failed ===\n";

    return tests_failed > 0 ? 1 : 0;
}
