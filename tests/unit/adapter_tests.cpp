// Unit tests for the Substrate Adapter Layer Foundation
// Architecture reference: docs/architecture/SUBSTRATE_MODEL.md (Section 6)
//
// Tests use mock controls (set_use_mocks(true) + mock setters) to verify
// adapter state machines and capability detection without requiring
// actual substrates to be installed on the test host.

#include <darling_adapter.hpp>
#include <qemu_adapter.hpp>
#include <electron_adapter.hpp>
#include <webkit_adapter.hpp>
#include <iostream>
#include <string>
#include <cassert>

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
        if ((a) != (b)) { FAIL(std::string(msg) + " (expected " + std::to_string(static_cast<int>(b)) + ", got " + std::to_string(static_cast<int>(a)) + ")"); return; } \
    } while(0)

using namespace platform::adapters;

// ============================================================
// Darling Adapter Tests
// ============================================================
static void test_darling_adapter_lifecycle() {
    TEST("Darling adapter state machine and configuration");
    DarlingAdapter adapter;
    
    // Enable mocks so test passes without actual Darling installed
    adapter.set_use_mocks(true);
    adapter.set_mock_darling_installed(true);
    adapter.set_mock_kernel_module_loaded(true);

    // 1. Initial State
    auto state = adapter.get_status();
    CHECK_EQ(state.status, AdapterStatus::Uninitialized, "Initial status must be Uninitialized");
    
    // 2. Configuration
    adapter.configure_prefix("/opt/custom-darling");
    adapter.set_environment({{"DYLD_PRINT_APIS", "1"}});

    // 3. Initialization
    CHECK(adapter.initialize(), "Initialization should succeed");
    CHECK_EQ(adapter.get_status().status, AdapterStatus::Ready, "Status must transition to Ready");

    // 4. Start execution
    CHECK(adapter.start(), "Starting adapter should succeed");
    CHECK_EQ(adapter.get_status().status, AdapterStatus::Running, "Status must transition to Running");

    // 5. Binary execution
    CHECK(adapter.launch_binary("/Applications/Calculator.app/Contents/MacOS/Calculator", {}), "Launching binary should succeed");

    // 6. Suspend / Resume
    CHECK(adapter.suspend(), "Suspending adapter should succeed");
    CHECK_EQ(adapter.get_status().status, AdapterStatus::Suspended, "Status must transition to Suspended");

    CHECK(adapter.resume(), "Resuming adapter should succeed");
    CHECK_EQ(adapter.get_status().status, AdapterStatus::Running, "Status must transition to Running");

    // 7. Stop execution
    CHECK(adapter.stop(), "Stopping adapter should succeed");
    CHECK_EQ(adapter.get_status().status, AdapterStatus::Terminated, "Status must transition to Terminated");

    PASS();
}

static void test_darling_capabilities() {
    TEST("Darling capability checks and logs");
    DarlingAdapter adapter;

    // Enable mocks to simulate missing darling installation
    adapter.set_use_mocks(true);
    adapter.set_mock_darling_installed(false);
    adapter.set_mock_kernel_module_loaded(true);

    CHECK(!adapter.is_supported(), "Should not be supported if Darling is missing");
    
    auto checks = adapter.check_host_capabilities();
    bool found_binary_fail = false;
    for (const auto& check : checks) {
        if (check.requirement_name == "Darling binary check (mock)" && !check.satisfied) {
            found_binary_fail = true;
        }
    }
    CHECK(found_binary_fail, "Expected binary check failure logged in capabilities");

    // Verify logs capture failure
    CHECK(!adapter.initialize(), "Initialization should fail when unsupported");
    CHECK(adapter.has_errors(), "Adapter should register errors on failed init");

    auto logs = adapter.get_logs();
    bool logged_error = false;
    for (const auto& log : logs) {
        if (log.level == "ERROR") {
            logged_error = true;
        }
    }
    CHECK(logged_error, "Expected error log entry");

    PASS();
}

// ============================================================
// Qemu Adapter Tests
// ============================================================
static void test_qemu_adapter_lifecycle() {
    TEST("QEMU adapter state machine and VM orchestration");
    QemuAdapter adapter;

    adapter.set_use_mocks(true);
    adapter.set_mock_qemu_installed(true);
    adapter.set_mock_kvm_available(true);

    CHECK_EQ(adapter.get_status().status, AdapterStatus::Uninitialized, "Initial status must be Uninitialized");

    // Configure VM parameters
    adapter.configure_cpu("host");
    adapter.configure_memory(4096);
    adapter.set_qmp_socket("/tmp/qmp.sock");

    CHECK(adapter.initialize(), "Initialization should succeed");
    CHECK_EQ(adapter.get_status().status, AdapterStatus::Ready, "Status must transition to Ready");

    CHECK(adapter.start(), "Starting VM should succeed");
    CHECK_EQ(adapter.get_status().status, AdapterStatus::Running, "Status must transition to Running");

    CHECK(adapter.boot_vm("macos-base.qcow2", "session-overlay.qcow2"), "Booting VM should succeed");
    CHECK(adapter.execute_command("open -a Terminal"), "Command execution in guest should succeed");

    CHECK(adapter.suspend(), "Suspending VM should succeed");
    CHECK_EQ(adapter.get_status().status, AdapterStatus::Suspended, "Status must transition to Suspended");

    CHECK(adapter.resume(), "Resuming VM should succeed");
    CHECK_EQ(adapter.get_status().status, AdapterStatus::Running, "Status must transition to Running");

    CHECK(adapter.stop(), "Stopping VM should succeed");
    CHECK_EQ(adapter.get_status().status, AdapterStatus::Terminated, "Status must transition to Terminated");

    PASS();
}

static void test_qemu_capabilities() {
    TEST("QEMU virtualization capabilities and logs");
    QemuAdapter adapter;

    adapter.set_use_mocks(true);
    adapter.set_mock_qemu_installed(true);
    adapter.set_mock_kvm_available(false);

    CHECK(!adapter.is_supported(), "Should fail capability checks when KVM is unavailable");

    CHECK(!adapter.initialize(), "Initialization must fail when KVM is missing");
    CHECK(adapter.has_errors(), "Adapter status should indicate errors");

    PASS();
}

// ============================================================
// Electron Adapter Tests
// ============================================================
static void test_electron_adapter_lifecycle() {
    TEST("Electron runtime adapter lifecycle and sandboxing");
    ElectronAdapter adapter;

    adapter.set_use_mocks(true);
    adapter.set_mock_runtime_cached(true);
    adapter.set_mock_sandbox_supported(true);

    CHECK_EQ(adapter.get_status().status, AdapterStatus::Uninitialized, "Initial status must be Uninitialized");

    // Configure Electron app
    adapter.resolve_runtime_version("24.1.0");
    adapter.set_asar_path("/opt/apps/app.asar");
    adapter.inject_preload("/opt/shims/preload.js");

    CHECK(adapter.initialize(), "Initialization should succeed");
    CHECK_EQ(adapter.get_status().status, AdapterStatus::Ready, "Status must transition to Ready");

    CHECK(adapter.start(), "Starting runner should succeed");
    CHECK(adapter.execute(), "Executing application payload should succeed");
    CHECK_EQ(adapter.get_status().status, AdapterStatus::Running, "Status must transition to Running");

    CHECK(adapter.stop(), "Stopping runner should succeed");
    CHECK_EQ(adapter.get_status().status, AdapterStatus::Terminated, "Status must transition to Terminated");

    PASS();
}

static void test_electron_capabilities() {
    TEST("Electron cached runtimes capabilities");
    ElectronAdapter adapter;

    adapter.set_use_mocks(true);
    adapter.set_mock_runtime_cached(false);
    adapter.set_mock_sandbox_supported(true);

    CHECK(!adapter.is_supported(), "Should fail capability checks when target runtime is not cached");

    PASS();
}

// ============================================================
// WebKit Adapter Tests
// ============================================================
static void test_webkit_adapter_lifecycle() {
    TEST("WebKitGTK wrapper adapter lifecycle");
    WebKitAdapter adapter;

    adapter.set_use_mocks(true);
    adapter.set_mock_webkitgtk_installed(true);
    adapter.set_mock_display_server_available(true);

    CHECK_EQ(adapter.get_status().status, AdapterStatus::Uninitialized, "Initial status must be Uninitialized");

    // Configure window and page
    adapter.set_url("file:///opt/apps/index.html");
    adapter.configure_window(1024, 768, "Tauri Application Shell");
    adapter.bind_ipc_callback("tauri_ipc", [](const std::string& msg) {
        std::cout << "Received IPC message: " << msg << std::endl;
    });

    CHECK(adapter.initialize(), "Initialization should succeed");
    CHECK_EQ(adapter.get_status().status, AdapterStatus::Ready, "Status must transition to Ready");

    CHECK(adapter.start(), "Starting webview should succeed");
    CHECK_EQ(adapter.get_status().status, AdapterStatus::Running, "Status must transition to Running");

    CHECK(adapter.run_window_loop(), "Running event loop should succeed");

    CHECK(adapter.stop(), "Stopping webview should succeed");
    CHECK_EQ(adapter.get_status().status, AdapterStatus::Terminated, "Status must transition to Terminated");

    PASS();
}

static void test_webkit_capabilities() {
    TEST("WebKitGTK system dependencies capability checks");
    WebKitAdapter adapter;

    adapter.set_use_mocks(true);
    adapter.set_mock_webkitgtk_installed(true);
    adapter.set_mock_display_server_available(false);

    CHECK(!adapter.is_supported(), "Should fail capability checks when display server connection is unavailable");

    PASS();
}

// ============================================================
// Main entry point
// ============================================================
int main() {
    std::cout << "\n=== Substrate Adapter Layer — Unit Tests ===\n\n";

    test_darling_adapter_lifecycle();
    test_darling_capabilities();

    test_qemu_adapter_lifecycle();
    test_qemu_capabilities();

    test_electron_adapter_lifecycle();
    test_electron_capabilities();

    test_webkit_adapter_lifecycle();
    test_webkit_capabilities();

    std::cout << "\n=== Results: " << tests_passed << " passed, "
              << tests_failed << " failed ===\n";

    return tests_failed > 0 ? 1 : 0;
}
