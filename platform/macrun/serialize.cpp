// macrun Launch Plan Serialization (JSON/YAML)
// Architecture reference: docs/architecture/ARCHITECTURE_V6.md
//   — "Logging & Diagnostics": must emit subsystem identifier, capability state,
//     execution tier, failure classification, remediation guidance.

#include "macrun.hpp"
#include <sstream>

namespace macrun {

static std::string json_escape(const std::string& s) {
    std::ostringstream o;
    for (char c : s) {
        switch (c) {
            case '"':  o << "\\\""; break;
            case '\\': o << "\\\\"; break;
            case '\n': o << "\\n"; break;
            case '\r': o << "\\r"; break;
            case '\t': o << "\\t"; break;
            default:   o << c;
        }
    }
    return o.str();
}

static void indent(std::ostringstream& out, int level) {
    for (int i = 0; i < level; i++) out << "  ";
}

std::string launch_plan_to_json(const LaunchPlan& plan) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"plan_id\": \"" << json_escape(plan.plan_id) << "\",\n";
    out << "  \"timestamp\": \"" << json_escape(plan.plan_timestamp) << "\",\n";
    out << "  \"application\": {\n";
    out << "    \"name\": \"" << json_escape(plan.application_name) << "\",\n";
    out << "    \"bundle_id\": \"" << json_escape(plan.bundle_identifier) << "\",\n";
    out << "    \"version\": \"" << json_escape(plan.application_version) << "\"\n";
    out << "  },\n";

    // Input
    out << "  \"input\": {\n";
    out << "    \"format\": \"" << to_string(plan.input.format) << "\",\n";
    out << "    \"path\": \"" << json_escape(plan.input.original_path) << "\",\n";
    if (!plan.input.extracted_bundle_path.empty())
        out << "    \"extracted_bundle\": \"" << json_escape(plan.input.extracted_bundle_path) << "\",\n";
    out << "    \"extraction_warnings\": [";
    for (size_t i = 0; i < plan.input.extraction_warnings.size(); i++) {
        if (i) out << ", ";
        out << "\"" << json_escape(plan.input.extraction_warnings[i]) << "\"";
    }
    out << "]\n";
    out << "  },\n";

    // Detection summary
    out << "  \"detection\": {\n";
    out << "    \"tier\": \"" << platform::to_string(plan.detection.recommendation.preferred_tier) << "\",\n";
    out << "    \"fallback_tier\": \"" << platform::to_string(plan.detection.recommendation.fallback_tier) << "\",\n";
    out << "    \"architecture\": \"" << platform::to_string(plan.detection.macho.primary_architecture) << "\",\n";
    out << "    \"risk_score\": " << plan.detection.score.risk_score_total << ",\n";
    out << "    \"frameworks_detected\": " << plan.detection.frameworks.size() << ",\n";
    if (!plan.detection.frameworks.empty()) {
        out << "    \"frameworks\": [\n";
        for (size_t i = 0; i < plan.detection.frameworks.size(); i++) {
            out << "      \"" << platform::to_string(plan.detection.frameworks[i].id) << "\"";
            if (i < plan.detection.frameworks.size() - 1) out << ",";
            out << "\n";
        }
        out << "    ]\n";
    }
    out << "  },\n";

    // Backend
    out << "  \"backend\": {\n";
    out << "    \"primary\": \"" << to_string(plan.backend.primary) << "\",\n";
    out << "    \"fallback\": \"" << to_string(plan.backend.fallback) << "\",\n";
    out << "    \"vm_required\": " << (plan.backend.vm_required ? "true" : "false") << ",\n";
    out << "    \"rationale\": \"" << json_escape(plan.backend.rationale) << "\",\n";
    out << "    \"rules\": [\n";
    for (size_t i = 0; i < plan.backend.applied_rules.size(); i++) {
        out << "      \"" << json_escape(plan.backend.applied_rules[i]) << "\"";
        if (i < plan.backend.applied_rules.size() - 1) out << ",";
        out << "\n";
    }
    out << "    ]\n";
    out << "  },\n";

    // Capabilities (orchestration-expressed requirements, not substrate config)
    out << "  \"capabilities\": {\n";
    out << "    \"needs_asar_extraction\": " << (plan.capabilities.needs_asar_extraction ? "true" : "false") << ",\n";
    out << "    \"needs_arm64_translation\": " << (plan.capabilities.needs_arm64_translation ? "true" : "false") << ",\n";
    out << "    \"needs_wayland\": " << (plan.capabilities.needs_wayland_integration ? "true" : "false") << ",\n";
    out << "    \"needs_hotkey_bridge\": " << (plan.capabilities.needs_hotkey_bridge ? "true" : "false") << ",\n";
    out << "    \"needs_coreanimation_flatten\": " << (plan.capabilities.needs_coreanimation_flatten ? "true" : "false") << ",\n";
    out << "    \"needs_gpu_disabled\": " << (plan.capabilities.needs_gpu_disabled ? "true" : "false") << ",\n";
    out << "    \"needs_autoupdater_disabled\": " << (plan.capabilities.needs_autoupdater_disabled ? "true" : "false") << "\n";
    out << "  },\n";

    // Compatibility
    out << "  \"compatibility\": {\n";
    out << "    \"state\": \"" << platform::to_string(plan.compatibility.state) << "\",\n";
    out << "    \"compat_db_match\": " << (plan.compatibility.compat_db_match ? "true" : "false") << ",\n";
    if (!plan.compatibility.compat_db_record_id.empty())
        out << "    \"compat_db_record\": \"" << json_escape(plan.compatibility.compat_db_record_id) << "\",\n";
    out << "    \"issues\": [\n";
    for (size_t i = 0; i < plan.compatibility.issues.size(); i++) {
        out << "      \"" << json_escape(plan.compatibility.issues[i]) << "\"";
        if (i < plan.compatibility.issues.size() - 1) out << ",";
        out << "\n";
    }
    out << "    ],\n";
    out << "    \"degradation_risks\": [\n";
    for (size_t i = 0; i < plan.compatibility.degradation_risks.size(); i++) {
        out << "      \"" << json_escape(plan.compatibility.degradation_risks[i]) << "\"";
        if (i < plan.compatibility.degradation_risks.size() - 1) out << ",";
        out << "\n";
    }
    out << "    ]\n";
    out << "  },\n";

    // Launch status
    out << "  \"can_launch\": " << (plan.can_launch ? "true" : "false");
    if (!plan.block_reason.empty())
        out << ",\n  \"block_reason\": \"" << json_escape(plan.block_reason) << "\"";
    out << ",\n";

    // Diagnostics
    out << "  \"diagnostic_log\": [\n";
    for (size_t i = 0; i < plan.diagnostic_log.size(); i++) {
        out << "    \"" << json_escape(plan.diagnostic_log[i]) << "\"";
        if (i < plan.diagnostic_log.size() - 1) out << ",";
        out << "\n";
    }
    out << "  ]\n";
    out << "}\n";

    return out.str();
}

std::string launch_plan_to_yaml(const LaunchPlan& plan) {
    std::ostringstream out;
    out << "plan_id: \"" << plan.plan_id << "\"\n";
    out << "timestamp: \"" << plan.plan_timestamp << "\"\n";
    out << "can_launch: " << (plan.can_launch ? "true" : "false") << "\n";
    if (!plan.block_reason.empty())
        out << "block_reason: \"" << plan.block_reason << "\"\n";

    out << "application:\n";
    out << "  name: \"" << plan.application_name << "\"\n";
    out << "  bundle_id: \"" << plan.bundle_identifier << "\"\n";
    out << "  version: \"" << plan.application_version << "\"\n";

    out << "input:\n";
    out << "  format: " << to_string(plan.input.format) << "\n";
    out << "  path: \"" << json_escape(plan.input.original_path) << "\"\n";

    out << "detection:\n";
    out << "  tier: " << platform::to_string(plan.detection.recommendation.preferred_tier) << "\n";
    out << "  architecture: " << platform::to_string(plan.detection.macho.primary_architecture) << "\n";
    out << "  risk_score: " << plan.detection.score.risk_score_total << "\n";

    out << "backend:\n";
    out << "  primary: " << to_string(plan.backend.primary) << "\n";
    out << "  fallback: " << to_string(plan.backend.fallback) << "\n";
    out << "  vm_required: " << (plan.backend.vm_required ? "yes" : "no") << "\n";

    out << "compatibility:\n";
    out << "  state: " << platform::to_string(plan.compatibility.state) << "\n";
    out << "  compat_db_match: " << (plan.compatibility.compat_db_match ? "yes" : "no") << "\n";

    out << "diagnostic_log:\n";
    for (const auto& entry : plan.diagnostic_log) {
        out << "  - \"" << entry << "\"\n";
    }

    return out.str();
}

} // namespace macrun
