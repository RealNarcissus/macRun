// macrun Orchestration Pipeline
// Architecture reference: docs/architecture/ARCHITECTURE_V6.md
//   — Execution Pipeline diagram: DMG/App Extraction → Bundle Inspection →
//     Tier Classification → Capability Mapping → Execution Strategy Selection →
//     Runtime Provision → Linux Integration → Window Appears
//   — "macrun Contract" section
//
// SUBSTRATE_MODEL.md: the orchestrator interacts with adapters through
// abstract interfaces only. Substrate provisioning (paths, env vars, flags)
// lives in the adapter layer.

#include "macrun.hpp"
#include "adapter_bridge.hpp"
#include <detector.hpp>
#include <compatdb/database.hpp>
#include <electron_adapter.hpp>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <algorithm>
#include <sys/stat.h>

namespace macrun {

// ============================================================
// Plan ID generation (deterministic)
// ============================================================
std::string generate_plan_id(const std::string& bundle_identifier) {
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    std::ostringstream oss;
    oss << "macrun-" << bundle_identifier << "-" << ms;
    return oss.str();
}

// ============================================================
// Resolve input (kept separate for testability)
// ============================================================
ResolvedInput resolve_input_for_orchestrator(const std::string& input_path);

// ============================================================
// Backend selection / capability computation (from backend.cpp)
// ============================================================
BackendSelection select_backend(const platform::DetectionResult& detection);
RequiredCapabilities compute_required_capabilities(
    const platform::DetectionResult& detection,
    const BackendSelection& backend);

// ============================================================
// Diagnostic logger
// ============================================================
static void diag(LaunchPlan& plan, const std::string& stage, const std::string& message) {
    std::ostringstream oss;
    oss << "[" << stage << "] " << message;
    plan.diagnostic_log.push_back(oss.str());
}

// ============================================================
// Main orchestration entry point
// ============================================================
OrchestrationResult orchestrate(const std::string& input_path) {
    OrchestrationResult result;

    // Step 0: Validate input exists
    {
        struct stat st;
        if (stat(input_path.c_str(), &st) != 0) {
            result.success = false;
            result.errors.push_back("Input path does not exist: " + input_path);
            return result;
        }
    }

    // Step 1: Input resolution
    result.plan.input = resolve_input_for_orchestrator(input_path);
    diag(result.plan, "input",
        "format=" + std::string(to_string(result.plan.input.format)) +
        " path=" + input_path);

    if (result.plan.input.format == InputFormat::Unknown) {
        result.plan.can_launch = false;
        result.plan.block_reason = "Unrecognized input format — provide .app, .dmg, or Mach-O binary";
        result.success = false;
        result.errors.push_back(result.plan.block_reason);
        diag(result.plan, "error", result.plan.block_reason);
        return result;
    }

    // Step 2: Determine the bundle path for detection
    std::string bundle_for_detection;
    if (result.plan.input.format == InputFormat::AppBundle) {
        bundle_for_detection = result.plan.input.extracted_bundle_path;
    } else {
        // For DMG and raw Mach-O, we can't run full detection without a bundle
        result.plan.can_launch = false;
        result.plan.block_reason = "Input format '" +
            std::string(to_string(result.plan.input.format)) +
            "' requires additional extraction — use .app bundle directly";
        result.success = false;
        result.errors.push_back(result.plan.block_reason);
        diag(result.plan, "error", result.plan.block_reason);
        return result;
    }

    // Step 3: Capability Detection (5-stage engine)
    result.plan.detection = platform::detect_capabilities(bundle_for_detection);
    diag(result.plan, "detection",
        "bundle=" + result.plan.detection.bundle.bundle_identifier +
        " tier=" + std::string(platform::to_string(result.plan.detection.recommendation.preferred_tier)) +
        " state=" + std::string(platform::to_string(result.plan.detection.compatibility_state)));

    result.plan.application_name      = result.plan.detection.bundle.bundle_name;
    result.plan.bundle_identifier     = result.plan.detection.bundle.bundle_identifier;
    result.plan.application_version   = result.plan.detection.bundle.bundle_version;

    // Step 4: Compat-db lookup (optional — attempts to match a known record)
    {
        compatdb::CompatDatabase db;
        // Look for a record matching this bundle identifier
        std::string report_dir = "compat-db/reports";
        struct stat st;
        if (stat(report_dir.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
            db.load_directory(report_dir);
            auto matches = db.lookup_by_bundle_id(result.plan.bundle_identifier);
            if (!matches.empty()) {
                result.plan.compatibility.compat_db_match = true;
                result.plan.compatibility.compat_db_record_id = matches[0].record.record_id;
                // Carry over known issues and workarounds from compat-db
                for (const auto& issue : matches[0].record.known_issues) {
                    result.plan.compatibility.issues.push_back(
                        "[" + std::string(compatdb::severity_to_string(issue.severity)) + "] " +
                        issue.id + ": " + issue.description);
                }
                for (const auto& wa : matches[0].record.workarounds) {
                    result.plan.compatibility.workarounds.push_back(wa.description);
                }

                // Override execution tier and compatibility state based on compat-db profile
                switch (matches[0].record.execution_tier) {
                    case compatdb::ExecutionTier::NativeSubstitution:
                        result.plan.detection.recommendation.preferred_tier = platform::ExecutionTier::Tier0_NativeSubstitution;
                        break;
                    case compatdb::ExecutionTier::DarlingCompatible:
                        result.plan.detection.recommendation.preferred_tier = platform::ExecutionTier::Tier2_LightweightCocoa;
                        break;
                    case compatdb::ExecutionTier::VMRecommended:
                        result.plan.detection.recommendation.preferred_tier = platform::ExecutionTier::Tier4B_VMAssisted;
                        break;
                    case compatdb::ExecutionTier::Unsupported:
                        result.plan.detection.recommendation.preferred_tier = platform::ExecutionTier::Tier4_Unsupported;
                        break;
                }

                switch (matches[0].record.compatibility_state) {
                    case compatdb::CompatibilityState::Verified:
                        result.plan.detection.compatibility_state = platform::CompatibilityState::Verified;
                        break;
                    case compatdb::CompatibilityState::Functional:
                        result.plan.detection.compatibility_state = platform::CompatibilityState::Functional;
                        break;
                    case compatdb::CompatibilityState::Partial:
                        result.plan.detection.compatibility_state = platform::CompatibilityState::Partial;
                        break;
                    case compatdb::CompatibilityState::Degraded:
                        result.plan.detection.compatibility_state = platform::CompatibilityState::Degraded;
                        break;
                    case compatdb::CompatibilityState::Unsupported:
                        result.plan.detection.compatibility_state = platform::CompatibilityState::Unsupported;
                        break;
                    case compatdb::CompatibilityState::Broken:
                        result.plan.detection.compatibility_state = platform::CompatibilityState::Broken;
                        break;
                }

                diag(result.plan, "compat-db",
                    "matched_record=" + matches[0].record.record_id +
                    " state=" + std::string(compatdb::state_to_string(matches[0].record.compatibility_state)));
            } else {
                diag(result.plan, "compat-db", "no matching record found");
            }
        }
    }

    // Step 5: Backend Selection
    result.plan.backend = select_backend(result.plan.detection);
    diag(result.plan, "backend",
        "primary=" + std::string(to_string(result.plan.backend.primary)) +
        " fallback=" + std::string(to_string(result.plan.backend.fallback)) +
        " vm_required=" + (result.plan.backend.vm_required ? "yes" : "no"));
    for (const auto& rule : result.plan.backend.applied_rules) {
        diag(result.plan, "backend-rule", rule);
    }

    // Step 6: Runtime Capabilities (orchestration expresses requirements;
    //         adapter layer provisions substrate configuration)
    result.plan.capabilities = compute_required_capabilities(result.plan.detection, result.plan.backend);
    diag(result.plan, "capabilities",
        "asar=" + std::to_string(result.plan.capabilities.needs_asar_extraction) +
        " arm64=" + std::to_string(result.plan.capabilities.needs_arm64_translation) +
        " wayland=" + std::to_string(result.plan.capabilities.needs_wayland_integration) +
        " hotkey=" + std::to_string(result.plan.capabilities.needs_hotkey_bridge) +
        " ca_flatten=" + std::to_string(result.plan.capabilities.needs_coreanimation_flatten) +
        " gpu_disable=" + std::to_string(result.plan.capabilities.needs_gpu_disabled));

    // Step 7: Compatibility Report
    result.plan.compatibility.state = result.plan.detection.compatibility_state;
    for (const auto& w : result.plan.detection.recommendation.compatibility_warnings) {
        result.plan.compatibility.issues.push_back(w);
    }
    for (const auto& risk : result.plan.detection.recommendation.degradation_risks) {
        result.plan.compatibility.degradation_risks.push_back(
            risk.subsystem + ": " + risk.description);
    }
    for (const auto& r : result.plan.detection.unsupported_reasons) {
        result.plan.compatibility.unsupported_reasons.push_back(r);
    }
    diag(result.plan, "compatibility",
        "state=" + std::string(platform::to_string(result.plan.compatibility.state)) +
        " issues=" + std::to_string(result.plan.compatibility.issues.size()) +
        " risks=" + std::to_string(result.plan.compatibility.degradation_risks.size()));

    // Step 8: Can-launch determination
    switch (result.plan.compatibility.state) {
        case platform::CompatibilityState::Unsupported:
            result.plan.can_launch = false;
            result.plan.block_reason = "Application is unsupported (Tier 4)";
            break;
        case platform::CompatibilityState::Broken:
            result.plan.can_launch = false;
            result.plan.block_reason = "Application is broken — unexpected failure or regression";
            break;
        default:
            result.plan.can_launch = true;
            break;
    }

    if (result.plan.backend.primary == BackendType::None) {
        result.plan.can_launch = false;
        result.plan.block_reason = "No compatible execution backend available";
    }

    // Step 9: Generate plan ID and timestamp
    result.plan.plan_id = generate_plan_id(result.plan.bundle_identifier);
    {
        auto t = std::time(nullptr);
        std::ostringstream ts;
        ts << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%SZ");
        result.plan.plan_timestamp = ts.str();
    }

    diag(result.plan, "plan",
        "id=" + result.plan.plan_id +
        " can_launch=" + (result.plan.can_launch ? "yes" : "no") +
        (result.plan.can_launch ? "" : " reason=" + result.plan.block_reason));

    result.success = true;
    return result;
}

// ============================================================
// execute_plan — provision an adapter and launch the application
// Architecture: docs/architecture/SUBSTRATE_MODEL.md Section 6
//   - Adapter provisioning happens through the adapter bridge only
//   - The orchestrator knows about backends, not substrates
//   - All Electron-specific behavior is isolated in the adapter layer
// ============================================================
ExecutionResult execute_plan(const LaunchPlan& plan) {
    ExecutionResult exec_result;

    if (!plan.can_launch) {
        exec_result.success = false;
        exec_result.error_message = "Cannot execute: application is not launchable (reason: " + plan.block_reason + ")";
        return exec_result;
    }

    // Only ElectronRuntime is implemented in Phase 3B.
    // Other backends return a clear "not yet implemented" message.
    switch (plan.backend.primary) {
        case BackendType::ElectronRuntime: {
            platform::adapters::ElectronAdapter adapter;

            // Configure through the adapter bridge (the ONLY file with substrate knowledge)
            configure_adapter_for_electron(adapter, plan.capabilities, plan.detection);

            // Determine the ASAR path from the bundle
            std::string bundle_path = plan.input.extracted_bundle_path;
            std::string asar_path = bundle_path + "/Contents/Resources/app.asar";

            // Also check alternative Electron ASAR locations
            struct stat st;
            if (stat(asar_path.c_str(), &st) != 0) {
                // Try Electron's default ASAR filename
                asar_path = bundle_path + "/Contents/Resources/default_app.asar";
            }
            if (stat(asar_path.c_str(), &st) != 0) {
                // Check if it ships as a pre-extracted app directory
                std::string app_dir_path = bundle_path + "/Contents/Resources/app";
                if (stat(app_dir_path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
                    asar_path = app_dir_path;
                } else {
                    asar_path.clear();
                }
            }

            if (!asar_path.empty()) {
                adapter.set_asar_path(asar_path);
            }

            // Run the adapter lifecycle
            if (!adapter.initialize()) {
                exec_result.success = false;
                exec_result.error_message = "Electron adapter initialization failed";
                for (const auto& log : adapter.get_logs()) {
                    exec_result.adapter_logs.push_back(
                        "[" + log.timestamp + "] " + log.level + " " + log.component + ": " + log.message);
                }
                return exec_result;
            }

            if (!adapter.start()) {
                exec_result.success = false;
                exec_result.error_message = "Electron adapter start failed";
                return exec_result;
            }

            if (!adapter.execute()) {
                exec_result.success = false;
                exec_result.error_message = "Electron adapter execution failed";
                for (const auto& log : adapter.get_logs()) {
                    exec_result.adapter_logs.push_back(
                        "[" + log.timestamp + "] " + log.level + " " + log.component + ": " + log.message);
                }
                return exec_result;
            }

            exec_result.success = true;
            exec_result.child_pid = adapter.get_child_pid();
            exec_result.backend_used = "electron-runtime";
            for (const auto& log : adapter.get_logs()) {
                exec_result.adapter_logs.push_back(
                    "[" + log.timestamp + "] " + log.level + " " + log.component + ": " + log.message);
            }
            return exec_result;
        }

        case BackendType::TauriBridge:
        case BackendType::DarlingRuntime:
        case BackendType::CocoaLite:
        case BackendType::ARM64Translator:
        case BackendType::VMAssisted:
            exec_result.success = false;
            exec_result.backend_used = std::string(to_string(plan.backend.primary));
            exec_result.error_message = "Backend '" + exec_result.backend_used +
                "' is not yet implemented. Phase 3B supports ElectronRuntime only.";
            return exec_result;

        case BackendType::None:
        default:
            exec_result.success = false;
            exec_result.error_message = "No valid backend selected for execution";
            return exec_result;
    }
}

} // namespace macrun
