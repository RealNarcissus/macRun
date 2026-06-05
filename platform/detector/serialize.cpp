// JSON and YAML serialization for DetectionResult
#include "serialize.hpp"
#include <sstream>
#include <iomanip>

namespace platform {

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

std::string to_json(const DetectionResult& result) {
    std::ostringstream out;
    out << "{\n";

    // Bundle info
    out << "  \"bundle\": {\n";
    out << "    \"identifier\": \"" << json_escape(result.bundle.bundle_identifier) << "\",\n";
    out << "    \"name\": \"" << json_escape(result.bundle.bundle_name) << "\",\n";
    out << "    \"version\": \"" << json_escape(result.bundle.bundle_version) << "\",\n";
    out << "    \"short_version\": \"" << json_escape(result.bundle.bundle_short_version) << "\",\n";
    out << "    \"executable\": \"" << json_escape(result.bundle.executable_name) << "\",\n";
    out << "    \"type\": \"" << json_escape(result.bundle.bundle_type) << "\",\n";
    out << "    \"minimum_os\": \"" << json_escape(result.bundle.minimum_os_version) << "\",\n";
    out << "    \"sdk\": \"" << json_escape(result.bundle.sdk_version) << "\",\n";
    out << "    \"supported_platforms\": [";
    for (size_t i = 0; i < result.bundle.supported_platforms.size(); i++) {
        if (i) out << ", ";
        out << "\"" << json_escape(result.bundle.supported_platforms[i]) << "\"";
    }
    out << "],\n";
    out << "    \"required_capabilities\": [";
    for (size_t i = 0; i < result.bundle.required_capabilities.size(); i++) {
        if (i) out << ", ";
        out << "\"" << json_escape(result.bundle.required_capabilities[i]) << "\"";
    }
    out << "],\n";
    out << "    \"has_ns_principal_class\": " << (result.bundle.has_ns_principal_class ? "true" : "false") << ",\n";
    out << "    \"has_ns_main_nib_file\": " << (result.bundle.has_ns_main_nib_file ? "true" : "false") << "\n";
    out << "  },\n";

    // Mach-O analysis
    out << "  \"macho\": {\n";
    out << "    \"primary_architecture\": \"" << to_string(result.macho.primary_architecture) << "\",\n";
    out << "    \"architectures_present\": [";
    for (size_t i = 0; i < result.macho.architectures_present.size(); i++) {
        if (i) out << ", ";
        out << "\"" << to_string(result.macho.architectures_present[i]) << "\"";
    }
    out << "],\n";
    out << "    \"is_executable\": " << (result.macho.is_executable ? "true" : "false") << ",\n";
    out << "    \"is_dylib\": " << (result.macho.is_dylib ? "true" : "false") << ",\n";
    out << "    \"has_pie\": " << (result.macho.has_pie ? "true" : "false") << ",\n";
    out << "    \"min_os_deployment\": " << result.macho.min_os_deployment << ",\n";
    out << "    \"linked_libraries\": [\n";
    for (size_t i = 0; i < result.macho.linked_libraries.size(); i++) {
        out << "      \"" << json_escape(result.macho.linked_libraries[i]) << "\"";
        if (i < result.macho.linked_libraries.size() - 1) out << ",";
        out << "\n";
    }
    out << "    ],\n";
    out << "    \"rpaths\": [";
    for (size_t i = 0; i < result.macho.rpaths.size(); i++) {
        if (i) out << ", ";
        out << "\"" << json_escape(result.macho.rpaths[i]) << "\"";
    }
    out << "]\n";
    out << "  },\n";

    // Entitlements
    out << "  \"entitlements\": {\n";
    out << "    \"sandboxed\": " << (result.entitlements.sandboxed ? "true" : "false") << ",\n";
    out << "    \"uses_accessibility\": " << (result.entitlements.uses_accessibility ? "true" : "false") << ",\n";
    out << "    \"uses_hypervisor\": " << (result.entitlements.uses_hypervisor ? "true" : "false") << ",\n";
    out << "    \"network_client\": " << (result.entitlements.network_client ? "true" : "false") << ",\n";
    out << "    \"network_server\": " << (result.entitlements.network_server ? "true" : "false") << ",\n";
    out << "    \"hardening_runtime\": " << (result.entitlements.hardening_runtime ? "true" : "false") << ",\n";
    out << "    \"file_access_user_selected\": " << (result.entitlements.file_access_user_selected ? "true" : "false") << ",\n";
    out << "    \"raw_keys\": [";
    for (size_t i = 0; i < result.entitlements.raw_entitlement_keys.size(); i++) {
        if (i) out << ", ";
        out << "\"" << json_escape(result.entitlements.raw_entitlement_keys[i]) << "\"";
    }
    out << "]\n";
    out << "  },\n";

    // Frameworks
    out << "  \"frameworks\": [\n";
    for (size_t i = 0; i < result.frameworks.size(); i++) {
        out << "    {\n";
        out << "      \"framework\": \"" << to_string(result.frameworks[i].id) << "\",\n";
        out << "      \"impact\": \"" << to_string(result.frameworks[i].impact) << "\",\n";
        out << "      \"evidence\": \"" << json_escape(result.frameworks[i].evidence) << "\"\n";
        out << "    }";
        if (i < result.frameworks.size() - 1) out << ",";
        out << "\n";
    }
    out << "  ],\n";

    // Capability Score
    out << "  \"capability_score\": {\n";
    out << "    \"risk_score_total\": " << result.score.risk_score_total << ",\n";
    out << "    \"critical_count\": " << result.score.critical_count << ",\n";
    out << "    \"high_count\": " << result.score.high_count << ",\n";
    out << "    \"medium_count\": " << result.score.medium_count << ",\n";
    out << "    \"low_count\": " << result.score.low_count << ",\n";
    out << "    \"dimensions\": [\n";
    for (size_t i = 0; i < result.score.dimensions.size(); i++) {
        out << "      {\n";
        out << "        \"name\": \"" << json_escape(result.score.dimensions[i].name) << "\",\n";
        out << "        \"impact_weight\": \"" << to_string(result.score.dimensions[i].impact_weight) << "\",\n";
        out << "        \"detected\": " << (result.score.dimensions[i].detected ? "true" : "false") << ",\n";
        out << "        \"detail\": \"" << json_escape(result.score.dimensions[i].detail) << "\"\n";
        out << "      }";
        if (i < result.score.dimensions.size() - 1) out << ",";
        out << "\n";
    }
    out << "    ]\n";
    out << "  },\n";

    // Tier Recommendation
    out << "  \"recommendation\": {\n";
    out << "    \"preferred_tier\": \"" << to_string(result.recommendation.preferred_tier) << "\",\n";
    out << "    \"fallback_tier\": \"" << to_string(result.recommendation.fallback_tier) << "\",\n";
    out << "    \"vm_required\": " << (result.recommendation.vm_required ? "true" : "false") << ",\n";
    out << "    \"expected_state\": \"" << to_string(result.recommendation.expected_state) << "\",\n";
    out << "    \"degradation_risks\": [\n";
    for (size_t i = 0; i < result.recommendation.degradation_risks.size(); i++) {
        out << "      {\n";
        out << "        \"subsystem\": \"" << json_escape(result.recommendation.degradation_risks[i].subsystem) << "\",\n";
        out << "        \"description\": \"" << json_escape(result.recommendation.degradation_risks[i].description) << "\",\n";
        out << "        \"severity\": \"" << to_string(result.recommendation.degradation_risks[i].severity) << "\"\n";
        out << "      }";
        if (i < result.recommendation.degradation_risks.size() - 1) out << ",";
        out << "\n";
    }
    out << "    ],\n";
    out << "    \"warnings\": [";
    for (size_t i = 0; i < result.recommendation.compatibility_warnings.size(); i++) {
        if (i) out << ", ";
        out << "\"" << json_escape(result.recommendation.compatibility_warnings[i]) << "\"";
    }
    out << "]\n";
    out << "  },\n";

    // Top-level
    out << "  \"compatibility_state\": \"" << to_string(result.compatibility_state) << "\",\n";
    out << "  \"unsupported_reasons\": [";
    for (size_t i = 0; i < result.unsupported_reasons.size(); i++) {
        if (i) out << ", ";
        out << "\"" << json_escape(result.unsupported_reasons[i]) << "\"";
    }
    out << "],\n";
    out << "  \"detection_version\": \"" << result.detection_version << "\"\n";
    out << "}\n";

    return out.str();
}

std::string to_yaml(const DetectionResult& result) {
    std::ostringstream out;
    int d = 0;

    auto yaml_kv = [&](const std::string& key, const std::string& value) {
        indent(out, d);
        out << key << ": " << value << "\n";
    };

    auto yaml_list_item = [&](int level, const std::string& value) {
        indent(out, level);
        out << "- " << value << "\n";
    };

    out << "detection_version: \"" << result.detection_version << "\"\n";
    out << "compatibility_state: " << to_string(result.compatibility_state) << "\n";

    // Bundle
    out << "bundle:\n";
    d = 2;
    yaml_kv("identifier", "\"" + result.bundle.bundle_identifier + "\"");
    yaml_kv("name", "\"" + result.bundle.bundle_name + "\"");
    yaml_kv("version", "\"" + result.bundle.bundle_version + "\"");
    yaml_kv("short_version", "\"" + result.bundle.bundle_short_version + "\"");
    yaml_kv("type", "\"" + result.bundle.bundle_type + "\"");
    yaml_kv("minimum_os", "\"" + result.bundle.minimum_os_version + "\"");
    yaml_kv("has_ns_principal_class", result.bundle.has_ns_principal_class ? "true" : "false");
    d = 0;

    // Mach-O
    out << "macho:\n";
    d = 2;
    yaml_kv("primary_architecture", std::string(to_string(result.macho.primary_architecture)));
    out << "  linked_libraries:\n";
    for (const auto& lib : result.macho.linked_libraries) {
        yaml_list_item(4, "\"" + lib + "\"");
    }
    d = 0;

    // Frameworks
    out << "frameworks:\n";
    for (const auto& f : result.frameworks) {
        indent(out, 2);
        out << "- framework: " << to_string(f.id) << "\n";
        indent(out, 4);
        out << "impact: " << to_string(f.impact) << "\n";
        indent(out, 4);
        out << "evidence: \"" << f.evidence << "\"\n";
    }

    // Score
    out << "capability_score:\n";
    d = 2;
    yaml_kv("risk_score_total", std::to_string(result.score.risk_score_total));
    yaml_kv("critical_count", std::to_string(result.score.critical_count));
    yaml_kv("high_count", std::to_string(result.score.high_count));
    yaml_kv("medium_count", std::to_string(result.score.medium_count));
    yaml_kv("low_count", std::to_string(result.score.low_count));
    out << "  dimensions:\n";
    for (const auto& dim : result.score.dimensions) {
        yaml_list_item(4, "name: \"" + dim.name + "\"");
        indent(out, 6);
        out << "impact: " << to_string(dim.impact_weight) << "\n";
        indent(out, 6);
        out << "detected: " << (dim.detected ? "true" : "false") << "\n";
    }
    d = 0;

    // Recommendation
    out << "recommendation:\n";
    d = 2;
    yaml_kv("preferred_tier", std::string(to_string(result.recommendation.preferred_tier)));
    yaml_kv("fallback_tier", std::string(to_string(result.recommendation.fallback_tier)));
    yaml_kv("vm_required", result.recommendation.vm_required ? "true" : "false");
    yaml_kv("expected_state", std::string(to_string(result.recommendation.expected_state)));
    if (!result.recommendation.compatibility_warnings.empty()) {
        out << "  warnings:\n";
        for (const auto& w : result.recommendation.compatibility_warnings) {
            yaml_list_item(4, "\"" + w + "\"");
        }
    }
    d = 0;

    // Unsupported reasons
    if (!result.unsupported_reasons.empty()) {
        out << "unsupported_reasons:\n";
        for (const auto& r : result.unsupported_reasons) {
            yaml_list_item(2, "\"" + r + "\"");
        }
    }

    return out.str();
}

} // namespace platform
