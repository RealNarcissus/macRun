// Adapter Provisioning Bridge
// Translates orchestrator-issued RequiredCapabilities into adapter-specific
// configuration calls. This is the ONLY file that maps capability flags to
// substrate configuration. All substrate knowledge lives here, isolated
// from the orchestration layer.
//
// Architecture: SUBSTRATE_MODEL.md Section 6
//   - Adapters receive targets and configurations from the caller.
//   - Data translation happens at the adapter boundary.
//   - Raw paths, env vars, and substrate flags never cross into orchestration.

#pragma once
#include <macrun_types.hpp>
#include <detector.hpp>
#include <IDarlingAdapter.hpp>
#include <IElectronAdapter.hpp>
#include <IQemuAdapter.hpp>
#include <IWebKitAdapter.hpp>

namespace macrun {

// Configure an adapter from the capabilities computed by the orchestrator.
// The detection result provides framework/architecture context.
// Only adapter interface methods are called — no substrate internals leak.

inline void configure_adapter_for_electron(
    platform::adapters::IElectronAdapter& adapter,
    const RequiredCapabilities& caps,
    const platform::DetectionResult& detection)
{
    adapter.set_bundle_info(detection.bundle.bundle_identifier, detection.bundle.bundle_name);

    for (const auto& fw : detection.frameworks) {
        if (fw.id == platform::FrameworkId::Electron) {
            adapter.resolve_runtime_version(fw.version.empty() ? "auto" : fw.version);
        }
    }

    // Set the shims directory to ~/.cache/macrun/shims/
    std::string home;
    if (const char* h = std::getenv("HOME")) home = h;
    else home = "/tmp";
    adapter.set_shims_dir(home + "/.cache/macrun/shims");

    // Always inject basic shim preloads
    adapter.inject_preload("shims/preload-main.js");

    if (caps.needs_gpu_disabled || (std::getenv("MACRUN_SHIM_DISABLE_GPU") && std::string(std::getenv("MACRUN_SHIM_DISABLE_GPU")) == "1")) {
        adapter.inject_preload("shims/disable-gpu.js");
    }
    if (caps.needs_autoupdater_disabled) {
        adapter.inject_preload("shims/disable-sparkle.js");
    }
    if (caps.needs_wayland_integration) {
        // Path mapping, clipboard, notifications — all needed for Linux integration
        adapter.inject_preload("shims/path-mapper.js");
        adapter.inject_preload("shims/notification-bridge.js");
        adapter.inject_preload("shims/clipboard-bridge.js");
        adapter.inject_preload("shims/shell-integration.js");
    }

    // Governed API normalization — always active for Tier 0 Electron apps
    adapter.inject_preload("shims/electron-normalization-registry.js");

    if (const char* env_renderer = std::getenv("MACRUN_DIAG_RENDERER")) {
        if (std::string(env_renderer) == "1") {
            adapter.inject_preload("shims/renderer-diag.js");
        }
    }
    if (const char* env_main = std::getenv("MACRUN_DIAG_MAIN")) {
        if (std::string(env_main) == "1") {
            adapter.inject_preload("shims/main-diag.js");
        }
    }
}

inline void configure_adapter_for_darling(
    platform::adapters::IDarlingAdapter& adapter,
    const RequiredCapabilities& caps,
    const platform::DetectionResult& /*detection*/)
{
    adapter.configure_prefix("/opt/darling");

    std::unordered_map<std::string, std::string> env;
    env["DYLD_LIBRARY_PATH"] = "/opt/darling/lib";

    if (caps.needs_coreanimation_flatten) {
        env["MACRUN_CA_FLATTEN"] = "1";
    }
    if (caps.needs_gpu_disabled) {
        env["MACRUN_NO_METAL"] = "1";
    }
    adapter.set_environment(env);
}

inline void configure_adapter_for_qemu(
    platform::adapters::IQemuAdapter& adapter,
    const RequiredCapabilities& caps,
    const platform::DetectionResult& detection)
{
    adapter.configure_cpu("max");

    if (caps.needs_arm64_translation) {
        // QEMU user-mode: binfmt_misc registration is handled by the system;
        // the adapter configures the translation environment.
    }

    // For Tier 4B VM-assisted: choose between user-mode and system VM
    if (caps.needs_hotkey_bridge) {
        // System VM mode: VM lifecycle manager owns boot_vm call
    }
}

inline void configure_adapter_for_webkit(
    platform::adapters::IWebKitAdapter& adapter,
    const RequiredCapabilities& caps,
    const platform::DetectionResult& /*detection*/)
{
    // Tauri simple mode: WebKitGTK shell
    adapter.configure_window(1024, 768, "Application");
}

} // namespace macrun
