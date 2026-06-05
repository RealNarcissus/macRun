// macrun Orchestration Layer — Unit Tests
// Architecture reference: docs/architecture/ARCHITECTURE_V6.md — "macrun Contract"
// Validates: tier routing, backend selection, plan determinism,
// unsupported rejection, structured diagnostics, input format detection.

#include <macrun.hpp>
#include <iostream>
#include <cassert>
#include <cstdlib>
#include <filesystem>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { std::cout << "  TEST: " << name << " ... "; } while(0)
#define PASS() do { std::cout << "PASSED\n"; tests_passed++; } while(0)
#define FAIL(msg) do { std::cout << "FAILED: " << msg << "\n"; tests_failed++; return; } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); } } while(0)

static std::string fixture(const std::string& relative_path) {
    if (const char* root = std::getenv("MACRUN_SOURCE_ROOT")) {
        return std::string(root) + "/" + relative_path;
    }
    namespace fs = std::filesystem;
    fs::path cwd = fs::current_path();
    for (fs::path p = cwd; p.has_parent_path(); p = p.parent_path()) {
        if (fs::exists(p / "CMakeLists.txt") && fs::exists(p / "docs" / "architecture" / "ARCHITECTURE_V6.md")) {
            return (p / relative_path).string();
        }
    }
    return relative_path;
}

// ============================================================
// Test 1: Electron app routes to Tier 0 via ElectronRuntime backend
// ============================================================
static void test_electron_routes_to_tier0() {
    TEST("Electron app routes to Tier 0 ElectronRuntime backend");
    auto result = macrun::orchestrate(fixture("tests/fixtures/electron-app.app"));
    CHECK(result.success, "Orchestration should succeed");
    CHECK(result.plan.can_launch, "Electron app should be launchable");
    CHECK(result.plan.backend.primary == macrun::BackendType::ElectronRuntime,
        "Primary backend should be ElectronRuntime");
    CHECK(!result.plan.backend.vm_required, "Electron apps should not require VM");
    CHECK(result.plan.capabilities.needs_asar_extraction,
        "Electron apps should need ASAR extraction");
    PASS();
}

// ============================================================
// Test 2: Cocoa app routes to Tier 2 CocoaLite backend
// ============================================================
static void test_cocoa_routes_to_tier2() {
    TEST("Cocoa app routes to Tier 2 CocoaLite backend");
    auto result = macrun::orchestrate(fixture("tests/fixtures/cocoa-app.app"));
    CHECK(result.success, "Orchestration should succeed");
    CHECK(result.plan.can_launch, "Cocoa app should be launchable");
    CHECK(result.plan.backend.primary == macrun::BackendType::CocoaLite,
        "Primary backend should be CocoaLite");
    CHECK(result.plan.capabilities.needs_wayland_integration,
        "Cocoa apps should need Wayland integration");
    CHECK(result.plan.compatibility.state == platform::CompatibilityState::Partial,
        "Cocoa app should have Partial compatibility state");
    PASS();
}

// ============================================================
// Test 3: SwiftUI app routes to VM-assisted backend
// ============================================================
static void test_swiftui_routes_to_vm() {
    TEST("SwiftUI app routes to VM-assisted backend");
    auto result = macrun::orchestrate(fixture("tests/fixtures/swiftui-app.app"));
    CHECK(result.success, "Orchestration should succeed");
    CHECK(result.plan.can_launch, "SwiftUI app should be launchable (via VM)");
    CHECK(result.plan.backend.primary == macrun::BackendType::VMAssisted,
        "Primary backend should be VMAssisted");
    CHECK(result.plan.backend.vm_required, "SwiftUI apps should require VM");
    PASS();
}

// ============================================================
// Test 4: Hypervisor app is explicitly rejected (unsupported)
// ============================================================
static void test_hypervisor_rejected() {
    TEST("Hypervisor app is explicitly rejected as unsupported");
    auto result = macrun::orchestrate(fixture("tests/fixtures/hypervisor-app.app"));
    CHECK(result.success, "Orchestration should succeed (detection completed)");
    CHECK(!result.plan.can_launch, "Hypervisor app should NOT be launchable");
    CHECK(!result.plan.block_reason.empty(), "Should have block reason");
    CHECK(result.plan.compatibility.state == platform::CompatibilityState::Unsupported,
        "Compatibility state should be Unsupported");
    PASS();
}

// ============================================================
// Test 5: Nonexistent path fails gracefully
// ============================================================
static void test_nonexistent_path() {
    TEST("Nonexistent path fails with clear error");
    auto result = macrun::orchestrate("/nonexistent/path/to/app.app");
    CHECK(!result.success, "Orchestration should fail for nonexistent path");
    CHECK(!result.errors.empty(), "Should have error messages");
    PASS();
}

// ============================================================
// Test 6: Launch plans are deterministic for same input
// ============================================================
static void test_plan_determinism() {
    TEST("Launch plans are deterministic for identical inputs");
    auto r1 = macrun::orchestrate(fixture("tests/fixtures/electron-app.app"));
    auto r2 = macrun::orchestrate(fixture("tests/fixtures/electron-app.app"));

    CHECK(r1.plan.backend.primary == r2.plan.backend.primary,
        "Same input must produce same backend");
    CHECK(r1.plan.can_launch == r2.plan.can_launch,
        "Same input must produce same can_launch");
    CHECK(r1.plan.application_name == r2.plan.application_name,
        "Same input must produce same application name");
    CHECK(r1.plan.bundle_identifier == r2.plan.bundle_identifier,
        "Same input must produce same bundle identifier");
    PASS();
}

// ============================================================
// Test 7: Structured diagnostics are emitted for every stage
// ============================================================
static void test_diagnostics_emitted() {
    TEST("Diagnostic log covers all pipeline stages");
    auto result = macrun::orchestrate(fixture("tests/fixtures/electron-app.app"));

    bool has_input = false, has_detection = false, has_backend = false;
    bool has_capabilities = false, has_compatibility = false, has_plan_stage = false;

    for (const auto& entry : result.plan.diagnostic_log) {
        if (entry.find("[input]") != std::string::npos) has_input = true;
        if (entry.find("[detection]") != std::string::npos) has_detection = true;
        if (entry.find("[backend]") != std::string::npos) has_backend = true;
        if (entry.find("[capabilities]") != std::string::npos) has_capabilities = true;
        if (entry.find("[compatibility]") != std::string::npos) has_compatibility = true;
        if (entry.find("[plan]") != std::string::npos) has_plan_stage = true;
    }

    CHECK(has_input, "Diagnostics must include input stage");
    CHECK(has_detection, "Diagnostics must include detection stage");
    CHECK(has_backend, "Diagnostics must include backend stage");
    CHECK(has_capabilities, "Diagnostics must include capabilities stage");
    CHECK(has_compatibility, "Diagnostics must include compatibility stage");
    CHECK(has_plan_stage, "Diagnostics must include final plan stage");
    PASS();
}

// ============================================================
// Test 8: JSON output is well-formed and contains all sections
// ============================================================
static void test_json_output_complete() {
    TEST("JSON output contains all required sections");
    auto result = macrun::orchestrate(fixture("tests/fixtures/electron-app.app"));
    std::string json = macrun::launch_plan_to_json(result.plan);

    CHECK(json.find("\"plan_id\"") != std::string::npos, "JSON must have plan_id");
    CHECK(json.find("\"application\"") != std::string::npos, "JSON must have application section");
    CHECK(json.find("\"input\"") != std::string::npos, "JSON must have input section");
    CHECK(json.find("\"detection\"") != std::string::npos, "JSON must have detection section");
    CHECK(json.find("\"backend\"") != std::string::npos, "JSON must have backend section");
    CHECK(json.find("\"capabilities\"") != std::string::npos, "JSON must have capabilities section");
    CHECK(json.find("\"compatibility\"") != std::string::npos, "JSON must have compatibility section");
    CHECK(json.find("\"diagnostic_log\"") != std::string::npos, "JSON must have diagnostic_log");
    CHECK(json.find("\"can_launch\"") != std::string::npos, "JSON must have can_launch");

    // Verify it's proper JSON (starts with {, ends with } after trimming whitespace)
    auto first_non_ws = json.find_first_not_of(" \t\n\r");
    auto last_non_ws = json.find_last_not_of(" \t\n\r");
    CHECK(first_non_ws != std::string::npos && json[first_non_ws] == '{',
        "JSON must start with {");
    CHECK(last_non_ws != std::string::npos && json[last_non_ws] == '}',
        "JSON must end with }");
    PASS();
}

// ============================================================
// Test 9: YAML output is well-formed
// ============================================================
static void test_yaml_output_complete() {
    TEST("YAML output contains all required sections");
    auto result = macrun::orchestrate(fixture("tests/fixtures/electron-app.app"));
    std::string yaml = macrun::launch_plan_to_yaml(result.plan);

    CHECK(yaml.find("plan_id:") != std::string::npos, "YAML must have plan_id");
    CHECK(yaml.find("application:") != std::string::npos, "YAML must have application");
    CHECK(yaml.find("input:") != std::string::npos, "YAML must have input");
    CHECK(yaml.find("detection:") != std::string::npos, "YAML must have detection");
    CHECK(yaml.find("backend:") != std::string::npos, "YAML must have backend");
    CHECK(yaml.find("compatibility:") != std::string::npos, "YAML must have compatibility");
    CHECK(yaml.find("diagnostic_log:") != std::string::npos, "YAML must have diagnostic_log");
    PASS();
}

// ============================================================
// Test 10: Capability flags are correct per backend type
// ============================================================
static void test_capabilities_per_backend() {
    TEST("Required capabilities match backend type");

    auto electron = macrun::orchestrate(fixture("tests/fixtures/electron-app.app"));
    CHECK(electron.plan.capabilities.needs_asar_extraction, "Electron needs ASAR extraction");
    CHECK(electron.plan.capabilities.needs_wayland_integration, "Electron needs Wayland");

    auto cocoa = macrun::orchestrate(fixture("tests/fixtures/cocoa-app.app"));
    CHECK(cocoa.plan.capabilities.needs_wayland_integration, "Cocoa needs Wayland");
    CHECK(cocoa.plan.capabilities.needs_coreanimation_flatten, "Cocoa-lite should require CA flattening");

    auto swiftui = macrun::orchestrate(fixture("tests/fixtures/swiftui-app.app"));
    CHECK(swiftui.plan.capabilities.needs_hotkey_bridge, "VM-assisted needs hotkey bridge");

    PASS();
}

int main() {
    std::cout << "\n=== macrun Orchestration — Unit Tests ===\n\n";

    test_electron_routes_to_tier0();
    test_cocoa_routes_to_tier2();
    test_swiftui_routes_to_vm();
    test_hypervisor_rejected();
    test_nonexistent_path();
    test_plan_determinism();
    test_diagnostics_emitted();
    test_json_output_complete();
    test_yaml_output_complete();
    test_capabilities_per_backend();

    std::cout << "\n=== Results: " << tests_passed << " passed, "
              << tests_failed << " failed ===\n";

    return tests_failed > 0 ? 1 : 0;
}
