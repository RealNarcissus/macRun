// macrun Orchestration Layer — Public API
// Architecture reference: docs/architecture/ARCHITECTURE_V6.md
//   — "macrun Contract": Accepts .app/.dmg/Mach-O binary, outputs execution
//     strategy, runtime configuration, compatibility state, launch orchestration.
//   — Never implements rendering, translation, or compositor logic.
#pragma once
#include "macrun_types.hpp"
#include <detector.hpp>
#include <string>
#include <vector>

namespace macrun {

// ============================================================
// Primary entry point: analyze an application and produce a launch plan.
// Steps (aligned to the architecture's Execution Pipeline diagram):
//   1. Input resolution (DMG mount / bundle detection)
//   2. Input normalization (extract .app bundle)
//   3. Capability detection (5-stage engine)
//   4. Compat-db lookup (optional, for known records)
//   5. Backend selection (tier → backend mapping)
//   6. Capability computation (adapter-neutral requirement flags)
//   7. Launch plan generation (structured output)
// ============================================================
OrchestrationResult orchestrate(const std::string& input_path);

// ============================================================
// Generate a deterministic launch plan ID from application identity.
// Format: macrun-<bundle_id>-<timestamp_hash>
// ============================================================
std::string generate_plan_id(const std::string& bundle_identifier);

// ============================================================
// Serialize a launch plan to structured JSON
// ============================================================
std::string launch_plan_to_json(const LaunchPlan& plan);

// ============================================================
// Serialize a launch plan to YAML
// ============================================================
std::string launch_plan_to_yaml(const LaunchPlan& plan);

// ============================================================
// Execution result — produced when execute_plan() is called
// ============================================================
struct ExecutionResult {
    bool success = false;
    int child_pid = 0;
    std::string backend_used;
    std::vector<std::string> adapter_logs;
    std::string error_message;
};

// ============================================================
// Execute a launch plan by provisioning the appropriate adapter
// and launching the application through it.
// ============================================================
ExecutionResult execute_plan(const LaunchPlan& plan);

} // namespace macrun
