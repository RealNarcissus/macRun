// Electron Runtime Substitution Adapter — Real Implementation
// Architecture: docs/architecture/SUBSTRATE_MODEL.md Section 3 (Electron Runtimes)
//   docs/architecture/ARCHITECTURE_V6.md — Tier 0 Runtime Substitution
//
// Responsibilities:
//   - Probe host for cached Electron runtimes via real filesystem checks
//   - Extract .asar archives via fork+execvp (no shell — argument array only)
//   - Detect Darwin-native .node modules and reject them explicitly
//   - Build and execute Electron process with shim preload scripts
//   - Track child process via LifecycleManager PID registration
//   - Generate XDG .desktop launcher entries
//   - Process group management for robust child cleanup (SIGTERM→SIGKILL escalation)
//   - Environment isolation: all child env vars set via execvpe, not setenv in parent
//
// Production code uses real system probes. Mock controls exist for unit testing.

#pragma once
#include "IElectronAdapter.hpp"
#include <vector>
#include <string>
#include <unordered_map>
#include <unistd.h>

namespace platform::adapters {

// Shim activation descriptor — type-safe, not based on string.find()
struct ShimActivation {
    bool paths            = false;
    bool disable_gpu      = false;
    bool disable_updater  = false;
    bool notifications    = false;
    bool clipboard        = false;
    bool shell            = false;

    // API normalization (governed by ELECTRON_API_NORMALIZATION.md)
    bool normalization    = false;   // MACRUN_SHIM_NORMALIZATION=1

    // Diagnostics (observability-only, never mutates behavior)
    bool diag_renderer    = false;   // MACRUN_DIAG_RENDERER=1
    bool diag_main        = false;   // MACRUN_DIAG_MAIN=1
};

class ElectronAdapter : public virtual IElectronAdapter {
public:
    ElectronAdapter();
    ~ElectronAdapter() override = default;

    // ILifecycle
    bool initialize() override;
    bool start() override;
    bool stop() override;
    bool suspend() override;
    bool resume() override;
    AdapterProcessStatus get_status() const override;

    // IDiagnostics
    std::vector<DiagnosticEntry> get_logs() const override;
    void clear_logs() override;
    bool has_errors() const override;

    // ICapability
    bool is_supported() const override;
    std::vector<HostCapabilityCheck> check_host_capabilities() const override;

    // IElectronAdapter
    void resolve_runtime_version(const std::string& version) override;
    void set_bundle_info(const std::string& bundle_id, const std::string& app_name) override;
    void set_asar_path(const std::string& asar_path) override;
    void inject_preload(const std::string& preload_script_path) override;
    bool execute() override;

    // Shim directory configuration
    void set_shims_dir(const std::string& dir) override { shims_dir_ = dir; }
    std::string get_resolved_runtime_path() const override { return runtime_binary_path_; }
    int get_child_pid() const override { return child_pid_; }

    // Native module detection — scan extracted app for Mach-O .node files
    bool detect_native_modules(const std::string& app_dir) override;

    // Optional: generate XDG .desktop launcher entry (with quoted Exec, sanitized name)
    bool generate_xdg_desktop_launcher(const std::string& app_name,
                                        const std::string& app_path,
                                        const std::string& icon_path = "");

    // Degradation diagnostics — per DEGRADATION_MODEL.md
    // Returns the current degradation category for this execution
    std::string degradation_category() const;
    std::string degradation_confidence() const;
    bool is_degraded() const { return degradation_category_ != "transparent"; }
    std::string degradation_report() const;  // structured diagnostic string

    // Test support controls — set use_mocks(true) before mock setters take effect
    void set_use_mocks(bool mocks) { use_mocks_ = mocks; }
    void set_mock_runtime_cached(bool cached);
    void set_mock_sandbox_supported(bool supported);

private:
    void log(const std::string& level, const std::string& message);

    // Internal execution pipeline
    std::string resolve_runtime_binary();
    std::string extract_asar(const std::string& asar_path);
    std::vector<std::string> build_command_line(const std::string& app_dir);

    // Build a child-only environment (env var overrides applied in child after fork)
    // Never calls setenv in the parent process.
    std::vector<std::string> build_child_environment();

    // Resolve the path to the asar-extract.js helper (pinned tooling script)
    std::string resolve_asar_extract_tool();

    // Determine which shims to activate based on inject_preload calls
    ShimActivation compute_shim_activation() const;

    // Record degradation state for observability (per DEGRADATION_MODEL.md)
    void record_degradation(const std::string& category, const std::string& confidence,
                           const std::string& capability, const std::string& reason);

    // State
    AdapterStatus status_;
    int exit_code_ = 0;
    std::string detail_;
    std::string resolved_version_;
    std::string bundle_id_;
    std::string app_name_;
    std::string asar_path_;
    std::vector<std::string> preload_scripts_;
    std::vector<DiagnosticEntry> logs_;
    std::string shims_dir_;

    // Execution state
    std::string runtime_binary_path_;
    std::string extracted_app_dir_;
    bool native_modules_safe_ = false;
    pid_t child_pid_ = 0;

    // Degradation tracking (per DEGRADATION_MODEL.md)
    // Starts as "transparent", escalated as degradation is detected
    std::string degradation_category_  = "transparent";
    std::string degradation_confidence_ = "verified";
    std::vector<std::string> degradation_events_;  // structured diagnostic entries
    bool native_module_bypass_active_ = false;

    // Mock controls (false = real probes in production)
    bool use_mocks_ = false;
    bool mock_runtime_cached_ = false;
    bool mock_sandbox_supported_ = false;
};

} // namespace platform::adapters
