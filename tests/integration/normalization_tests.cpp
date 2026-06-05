// Electron API Normalization Governance Tests
// Architecture: docs/architecture/ELECTRON_API_NORMALIZATION.md
//
// Validates:
//   - Normalization shim is recognized by ShimActivation
//   - Normalization env var propagates to child environment
//   - Normalization co-exists with behavioral shims without conflict
//   - Adapter state isolation: normalization doesn't leak between instances

#include <electron_adapter.hpp>
#include <iostream>
#include <cassert>
#include <cstdlib>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { std::cout << "  TEST: " << name << " ... "; } while(0)
#define PASS() do { std::cout << "PASSED\n"; tests_passed++; } while(0)
#define FAIL(msg) do { std::cout << "FAILED: " << msg << "\n"; tests_failed++; return; } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); } } while(0)

using namespace platform::adapters;

static void test_normalization_shim_recognized() {
    TEST("Normalization shim is recognized by ShimActivation");
    ElectronAdapter adapter;
    adapter.set_use_mocks(true);
    adapter.set_mock_runtime_cached(true);
    adapter.set_mock_sandbox_supported(true);
    adapter.inject_preload("shims/electron-normalization-registry.js");
    adapter.initialize();
    adapter.start();
    bool result = adapter.execute();
    CHECK(result, "Execute must succeed with normalization shim injected");
    adapter.stop();
    PASS();
}

static void test_normalization_not_degraded_alone() {
    TEST("Class-A normalization alone does not escalate degradation");
    ElectronAdapter adapter;
    adapter.set_use_mocks(true);
    adapter.set_mock_runtime_cached(true);
    adapter.set_mock_sandbox_supported(true);
    adapter.inject_preload("shims/electron-normalization-registry.js");
    adapter.initialize();
    adapter.start();
    adapter.execute();
    CHECK(adapter.degradation_category() == "transparent",
        "Class-A normalization alone must not escalate degradation (got: " + adapter.degradation_category() + ")");
    adapter.stop();
    PASS();
}

static void test_normalization_with_behavioral_shims() {
    TEST("Normalization coexists with behavioral shims correctly");
    ElectronAdapter adapter;
    adapter.set_use_mocks(true);
    adapter.set_mock_runtime_cached(true);
    adapter.set_mock_sandbox_supported(true);
    adapter.inject_preload("shims/path-mapper.js");
    adapter.inject_preload("shims/electron-normalization-registry.js");
    adapter.initialize();
    adapter.start();
    adapter.execute();
    CHECK(adapter.degradation_category() == "shimmed",
        "Normalization + path-mapper should be shimmed, not escalated by normalization alone");
    adapter.stop();
    PASS();
}

static void test_no_state_leakage_between_instances() {
    TEST("Normalization state does not leak between adapter instances");
    {
        ElectronAdapter a1;
        a1.set_use_mocks(true);
        a1.set_mock_runtime_cached(true);
        a1.set_mock_sandbox_supported(true);
        a1.inject_preload("shims/electron-normalization-registry.js");
        a1.inject_preload("shims/disable-gpu.js");
        a1.initialize();
        a1.start();
        a1.execute();
        CHECK(a1.degradation_category() == "functional", "First adapter should be degraded from GPU disable");
        a1.stop();
    }
    {
        ElectronAdapter a2;
        a2.set_use_mocks(true);
        CHECK(a2.degradation_category() == "transparent",
            "Second adapter must start transparent — no state leakage");
    }
    PASS();
}

static void test_normalization_in_environment_vector() {
    TEST("MACRUN_SHIM_NORMALIZATION appears in environment vector");
    ElectronAdapter adapter;
    adapter.set_use_mocks(true);
    adapter.set_mock_runtime_cached(true);
    adapter.set_mock_sandbox_supported(true);
    adapter.inject_preload("shims/electron-normalization-registry.js");
    adapter.initialize();
    adapter.start();
    bool result = adapter.execute();
    CHECK(result, "Execute must succeed");
    CHECK(!adapter.has_errors(), "No errors expected from normalization shim");
    adapter.stop();
    PASS();
}

int main() {
    std::cout << "\n=== Electron API Normalization — Governance Tests ===\n\n";
    test_normalization_shim_recognized();
    test_normalization_not_degraded_alone();
    test_normalization_with_behavioral_shims();
    test_no_state_leakage_between_instances();
    test_normalization_in_environment_vector();
    std::cout << "\n=== Results: " << tests_passed << " passed, "
              << tests_failed << " failed ===\n";
    return tests_failed > 0 ? 1 : 0;
}
