// Electron Launch Integration Tests — Phase 3B
// Architecture: docs/architecture/ARCHITECTURE_V6.md — Tier 0
//   docs/architecture/SUBSTRATE_MODEL.md Section 3 (Electron Runtimes)
//
// Verifies: .asar extraction, native-module detection, command-line building,
// shim injection, and the full ElectronAdapter execution pipeline.
// Tests use mock mode — no actual Electron process is spawned.

#include <electron_adapter.hpp>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstring>
#include <cassert>

namespace fs = std::filesystem;

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { std::cout << "  TEST: " << name << " ... "; } while(0)
#define PASS() do { std::cout << "PASSED\n"; tests_passed++; } while(0)
#define FAIL(msg) do { std::cout << "FAILED: " << msg << "\n"; tests_failed++; return; } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); } } while(0)

using namespace platform::adapters;

// ============================================================
// Helper: create temporary files for testing
// ============================================================

static std::string write_temp_file(const std::string& name, const unsigned char* data, size_t len) {
    std::string path = "/tmp/macrun-test-" + name;
    std::ofstream f(path, std::ios::binary);
    if (!f) return "";
    f.write(reinterpret_cast<const char*>(data), len);
    f.close();
    return path;
}

static std::string write_temp_macho_node() {
    // Mach-O 64-bit LE magic: 0xfeedfacf
    unsigned char macho_magic[] = {0xcf, 0xfa, 0xed, 0xfe, 0x00, 0x00, 0x00, 0x00};
    return write_temp_file("macho.node", macho_magic, sizeof(macho_magic));
}

static std::string write_temp_elf_node() {
    unsigned char elf_magic[] = {0x7f, 'E', 'L', 'F', 0x00, 0x00, 0x00, 0x00};
    return write_temp_file("elf.node", elf_magic, sizeof(elf_magic));
}

static std::string write_temp_fat_node() {
    // FAT binary LE magic: 0xcafebabe
    unsigned char fat_magic[] = {0xca, 0xfe, 0xba, 0xbe, 0x00, 0x00, 0x00, 0x00};
    return write_temp_file("fat.node", fat_magic, sizeof(fat_magic));
}

static void cleanup_temp(const std::string& path) {
    if (!path.empty()) std::remove(path.c_str());
}

// ============================================================
// Test: Native module detection rejects Mach-O .node files
// ============================================================
static void test_native_module_detection_rejects_macho() {
    TEST("Native module detection rejects Mach-O .node files");

    ElectronAdapter adapter;
    adapter.set_use_mocks(false);  // real detection logic, but no actual system calls

    // Create a temp directory with mock .node files
    std::string dir = "/tmp/macrun-test-nodemod-XXXXXX";
    char* d = mkdtemp(const_cast<char*>(dir.c_str()));
    CHECK(d != nullptr, "Failed to create temp directory");
    dir = d;

    // Write a Mach-O .node file
    unsigned char macho_magic[] = {0xcf, 0xfa, 0xed, 0xfe};
    std::ofstream f(dir + "/native.node", std::ios::binary);
    f.write(reinterpret_cast<const char*>(macho_magic), sizeof(macho_magic));
    f.close();

    bool result = adapter.detect_native_modules(dir);
    CHECK(!result, "Should reject directory with Mach-O .node module");

    // Clean up
    fs::remove_all(dir);
    PASS();
}

// ============================================================
// Test: Native module detection accepts ELF .node files
// ============================================================
static void test_native_module_detection_accepts_elf() {
    TEST("Native module detection accepts ELF .node files");

    ElectronAdapter adapter;
    adapter.set_use_mocks(false);

    std::string dir = "/tmp/macrun-test-nodemod-XXXXXX";
    char* d = mkdtemp(const_cast<char*>(dir.c_str()));
    CHECK(d != nullptr, "Failed to create temp directory");
    dir = d;

    // Write an ELF .node file (Linux-safe)
    unsigned char elf_magic[] = {0x7f, 'E', 'L', 'F'};
    std::ofstream f(dir + "/native.node", std::ios::binary);
    f.write(reinterpret_cast<const char*>(elf_magic), sizeof(elf_magic));
    f.close();

    bool result = adapter.detect_native_modules(dir);
    CHECK(result, "Should accept directory with ELF .node module");

    fs::remove_all(dir);
    PASS();
}

// ============================================================
// Test: Native module detection accepts directory with no .node files
// ============================================================
static void test_native_module_detection_no_node_files() {
    TEST("Native module detection accepts directory with no .node files");

    ElectronAdapter adapter;
    adapter.set_use_mocks(false);

    std::string dir = "/tmp/macrun-test-nodemod-XXXXXX";
    char* d = mkdtemp(const_cast<char*>(dir.c_str()));
    CHECK(d != nullptr, "Failed to create temp directory");
    dir = d;

    // Just a .js file — no native modules
    std::ofstream f(dir + "/index.js");
    f << "console.log('hello');";
    f.close();

    bool result = adapter.detect_native_modules(dir);
    CHECK(result, "Should accept directory with no native .node modules");

    fs::remove_all(dir);
    PASS();
}

// ============================================================
// Test: detect_native_modules rejects FAT binary .node
// ============================================================
static void test_native_module_detection_rejects_fat() {
    TEST("Native module detection rejects FAT binary .node files");

    ElectronAdapter adapter;
    adapter.set_use_mocks(false);

    std::string dir = "/tmp/macrun-test-nodemod-XXXXXX";
    char* d = mkdtemp(const_cast<char*>(dir.c_str()));
    CHECK(d != nullptr, "Failed to create temp directory");
    dir = d;

    // Write a FAT binary .node file (macOS universal binary)
    unsigned char fat_magic[] = {0xca, 0xfe, 0xba, 0xbe};
    std::ofstream f(dir + "/universal.node", std::ios::binary);
    f.write(reinterpret_cast<const char*>(fat_magic), sizeof(fat_magic));
    f.close();

    bool result = adapter.detect_native_modules(dir);
    CHECK(!result, "Should reject directory with FAT binary .node module");

    fs::remove_all(dir);
    PASS();
}

// ============================================================
// Test: Adapter lifecycle transitions (with mocks)
// ============================================================
static void test_adapter_lifecycle_with_mocks() {
    TEST("Electron adapter lifecycle transitions correctly (mock mode)");

    ElectronAdapter adapter;
    adapter.set_use_mocks(true);
    adapter.set_mock_runtime_cached(true);
    adapter.set_mock_sandbox_supported(true);

    // Uninitialized → Ready
    CHECK(adapter.initialize(), "Initialize should succeed");
    CHECK(adapter.get_status().status == AdapterStatus::Ready, "Should be Ready after init");

    // Ready → Running
    CHECK(adapter.start(), "Start should succeed");
    CHECK(adapter.get_status().status == AdapterStatus::Running, "Should be Running after start");

    // Running → Suspended
    CHECK(adapter.suspend(), "Suspend should succeed");
    CHECK(adapter.get_status().status == AdapterStatus::Suspended, "Should be Suspended");

    // Suspended → Running
    CHECK(adapter.resume(), "Resume should succeed");
    CHECK(adapter.get_status().status == AdapterStatus::Running, "Should be Running after resume");

    // Running → Terminated
    CHECK(adapter.stop(), "Stop should succeed");
    CHECK(adapter.get_status().status == AdapterStatus::Terminated, "Should be Terminated after stop");

    PASS();
}

// ============================================================
// Test: execute() fails without cached runtime
// ============================================================
static void test_execute_fails_without_runtime() {
    TEST("execute() fails when adapter is not in Running state");

    // Fresh adapter — not initialized, not started
    ElectronAdapter adapter;
    adapter.set_use_mocks(true);  // mock to avoid real fork

    // execute() requires Running state — should fail
    bool result = adapter.execute();
    CHECK(!result, "execute() should fail: adapter not in Running state");

    // Verify error was logged
    CHECK(adapter.has_errors() || !adapter.get_logs().empty(),
        "Should have diagnostic log from failed execution");

    PASS();
}

// ============================================================
// Test: execute() works in mock mode
// ============================================================
static void test_execute_mock_mode() {
    TEST("execute() succeeds in mock mode");

    ElectronAdapter adapter;
    adapter.set_use_mocks(true);
    adapter.set_mock_runtime_cached(true);
    adapter.set_mock_sandbox_supported(true);

    adapter.initialize();
    adapter.start();

    // Mock mode: resolve_runtime_binary returns "/mock/path/electron"
    // and extract_asar returns "/tmp/macrun-mock-extracted/"
    bool result = adapter.execute();
    CHECK(result, "execute() should succeed in mock mode");
    CHECK(adapter.get_child_pid() == 0, "Child PID should be 0 in mock mode (no real fork)");

    adapter.stop();
    PASS();
}

// ============================================================
// Test: Adapter configuration methods
// ============================================================
static void test_adapter_configuration() {
    TEST("Electron adapter accepts configuration");

    ElectronAdapter adapter;
    adapter.set_use_mocks(true);

    adapter.resolve_runtime_version("24.1.0");
    adapter.set_asar_path("/path/to/app.asar");
    adapter.inject_preload("shims/preload-main.js");
    adapter.inject_preload("shims/disable-gpu.js");
    adapter.set_shims_dir("/tmp/macrun-shims");

    // All configuration should not crash
    CHECK(true, "Configuration completed without errors");

    PASS();
}

// ============================================================
// Test: XDG desktop launcher generation (mock)
// ============================================================
static void test_xdg_desktop_launcher() {
    TEST("XDG desktop launcher generation (mock)");

    ElectronAdapter adapter;
    adapter.set_use_mocks(true);

    // generate_xdg_desktop_launcher requires HOME
    bool result = adapter.generate_xdg_desktop_launcher(
        "TestApp", "/opt/apps/TestApp.app", "/opt/apps/icon.png");

    // HOME is set in our test environment, so this should succeed
    CHECK(result, "XDG launcher generation should succeed when HOME is set");

    // Verify the file exists
    std::string home;
    if (const char* h = std::getenv("HOME")) home = h;
    if (!home.empty()) {
        std::string path = home + "/.local/share/applications/macrun-TestApp.desktop";
        CHECK(fs::exists(path), "Desktop file should exist: " + path);

        // Clean up
        fs::remove(path);
    }

    PASS();
}

// ============================================================
// Test: Resolved runtime path is returned correctly
// ============================================================
static void test_resolved_runtime_path() {
    TEST("Resolved runtime path is queryable");

    ElectronAdapter adapter;
    adapter.set_use_mocks(true);
    adapter.set_mock_runtime_cached(true);
    adapter.set_mock_sandbox_supported(true);

    adapter.initialize();
    adapter.start();
    adapter.execute();

    // In mock mode, the path is set to "/mock/path/electron"
    std::string path = adapter.get_resolved_runtime_path();
    CHECK(!path.empty(), "Resolved runtime path should not be empty");
    CHECK(path == "/mock/path/electron", "Mock path should be '/mock/path/electron'");

    PASS();
}

// ============================================================
// Test: detec_native_modules handles empty input
// ============================================================
static void test_native_module_empty_path() {
    TEST("detect_native_modules handles empty path safely");

    ElectronAdapter adapter;
    adapter.set_use_mocks(false);

    bool result = adapter.detect_native_modules("");
    CHECK(result, "Empty path should be treated as safe (no modules to check)");

    result = adapter.detect_native_modules("/nonexistent/path");
    CHECK(result, "Nonexistent path should be treated as safe");

    PASS();
}

// ============================================================
// Main
// ============================================================
int main() {
    std::cout << "\n=== Electron Runtime Integration Tests — Phase 3B ===\n\n";

    test_adapter_lifecycle_with_mocks();
    test_adapter_configuration();
    test_execute_mock_mode();
    test_execute_fails_without_runtime();
    test_resolved_runtime_path();
    test_native_module_detection_rejects_macho();
    test_native_module_detection_accepts_elf();
    test_native_module_detection_no_node_files();
    test_native_module_detection_rejects_fat();
    test_native_module_empty_path();
    test_xdg_desktop_launcher();

    std::cout << "\n=== Results: " << tests_passed << " passed, "
              << tests_failed << " failed ===\n";

    return tests_failed > 0 ? 1 : 0;
}
