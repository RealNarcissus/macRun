// Full 5-Stage Detection Pipeline Orchestrator
// Architecture reference: docs/architecture/ARCHITECTURE_V6.md — "Detection Pipeline" stages 1-5
#include "detector.hpp"
#include "diagnostics.hpp"
#include <sys/stat.h>
#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <json.hpp>
#include <cctype>

namespace platform {

static std::string read_file(const std::string& path);

namespace fs = std::filesystem;

static std::string extract_xml_tag_value(const std::string& xml, const std::string& key) {
    std::string search = "<key>" + key + "</key>";
    auto pos = xml.find(search);
    if (pos == std::string::npos) return {};

    pos += search.length();
    while (pos < xml.length() && (xml[pos] == ' ' || xml[pos] == '\n' || xml[pos] == '\r' || xml[pos] == '\t'))
        pos++;

    if (pos >= xml.length()) return {};

    if (xml.compare(pos, 8, "<string>") == 0) {
        pos += 8;
        auto end = xml.find("</string>", pos);
        if (end == std::string::npos) return {};
        return xml.substr(pos, end - pos);
    }
    return {};
}

static std::string bplist_extract_string_value(const std::string& data, const std::string& key) {
    auto pos = data.find(key);
    if (pos == std::string::npos) return {};
    pos += key.length();

    size_t max_scan = std::min(pos + 256, data.size());
    while (pos < max_scan && (static_cast<unsigned char>(data[pos]) < 0x20 ||
                              static_cast<unsigned char>(data[pos]) > 0x7e)) {
        pos++;
    }

    size_t start = pos;
    while (pos < max_scan &&
           static_cast<unsigned char>(data[pos]) >= 0x20 &&
           static_cast<unsigned char>(data[pos]) <= 0x7e &&
           pos - start < 128) {
        if (pos > start && data[pos] == 'C' && pos + 3 < max_scan) {
            if (data.compare(pos, 9, "CFBundle") == 0) break;
        }
        pos++;
    }

    if (pos > start) {
        return data.substr(start, pos - start);
    }
    return {};
}

static std::string detect_electron_version_from_package_json(const std::string& app_bundle_path) {
    std::string pkg_path = app_bundle_path + "/Contents/Resources/app/package.json";
    std::error_code ec;
    if (fs::exists(pkg_path, ec)) {
        std::string content = read_file(pkg_path);
        try {
            auto j = nlohmann::json::parse(content);
            std::string v;
            if (j.contains("devDependencies") && j["devDependencies"].contains("electron")) {
                v = j["devDependencies"]["electron"].get<std::string>();
            } else if (j.contains("dependencies") && j["dependencies"].contains("electron")) {
                v = j["dependencies"]["electron"].get<std::string>();
            }
            if (!v.empty()) {
                size_t start = 0;
                while (start < v.size() && (v[start] == '^' || v[start] == '~' || v[start] == '=' || v[start] == ' ' || v[start] == '>')) {
                    start++;
                }
                return v.substr(start);
            }
        } catch (...) {
        }
    }
    return "";
}

static std::string detect_electron_version(const std::string& app_bundle_path) {
    std::string pkg_ver = detect_electron_version_from_package_json(app_bundle_path);
    if (!pkg_ver.empty()) {
        return pkg_ver;
    }

    std::string frameworks_dir = app_bundle_path + "/Contents/Frameworks";
    std::error_code ec;
    if (fs::exists(frameworks_dir, ec)) {
        for (const auto& entry : fs::directory_iterator(frameworks_dir, ec)) {
            if (entry.is_directory() && entry.path().extension() == ".framework") {
                std::string fw_name = entry.path().filename().string();
                std::string plist_path = entry.path().string() + "/Resources/Info.plist";
                if (!fs::exists(plist_path, ec)) {
                    plist_path = entry.path().string() + "/Versions/Current/Resources/Info.plist";
                }
                if (fs::exists(plist_path, ec)) {
                    std::string plist_data = read_file(plist_path);
                    std::string bundle_id, version;
                    if (plist_data.size() >= 6 && plist_data.compare(0, 8, "bplist00") == 0) {
                        bundle_id = bplist_extract_string_value(plist_data, "CFBundleIdentifier");
                        version = bplist_extract_string_value(plist_data, "CFBundleShortVersionString");
                    } else {
                        bundle_id = extract_xml_tag_value(plist_data, "CFBundleIdentifier");
                        version = extract_xml_tag_value(plist_data, "CFBundleShortVersionString");
                    }

                    if (bundle_id == "com.github.Electron.Framework" ||
                        fw_name == "Electron Framework.framework" ||
                        bundle_id.find("electron") != std::string::npos ||
                        fw_name.find("Electron") != std::string::npos ||
                        bundle_id.find(".framework") != std::string::npos) {
                        if (!version.empty()) {
                            return version;
                        }
                    }
                }
            }
        }
    }
    return "";
}

static bool file_exists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

static std::string find_executable(const std::string& app_bundle_path, const BundleInfo& bundle) {
    if (!bundle.executable_name.empty()) {
        std::string exe = app_bundle_path + "/Contents/MacOS/" + bundle.executable_name;
        if (file_exists(exe)) return exe;
    }
    return {};
}

static std::string find_entitlements_plist(const std::string& app_bundle_path) {
    // Embedded provisioning profile or entitlements
    std::string embedded = app_bundle_path + "/Contents/embedded.provisionprofile";
    if (file_exists(embedded)) return embedded;

    // Some apps have Entitlements.plist
    std::string ent_file = app_bundle_path + "/Contents/Entitlements.plist";
    if (file_exists(ent_file)) return ent_file;

    return {};
}

static std::string read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static DetectionResult run_pipeline(const std::string& app_bundle_path, FingerprintRegistry* registry) {
    DetectionResult result;

    // Validate bundle structure
    std::string plist_path = app_bundle_path + "/Contents/Info.plist";
    if (!file_exists(plist_path)) {
        result.unsupported_reasons.push_back("Not a valid .app bundle: missing Info.plist");
        result.compatibility_state = CompatibilityState::Unsupported;
        log_diag(LogLevel::Error, "bundle_validation missing_plist path=" + app_bundle_path);
        return result;
    }

    // Stage 1: Bundle Analysis
    result.bundle = analyze_bundle(app_bundle_path);
    log_diag(LogLevel::Debug, "stage1_bundle bundle_id=" + result.bundle.bundle_identifier +
        " name=" + result.bundle.bundle_name +
        " exec=" + result.bundle.executable_name);

    if (result.bundle.bundle_identifier.empty()) {
        result.unsupported_reasons.push_back("Missing CFBundleIdentifier — cannot identify application");
    }

    // Stage 3: Architecture Analysis (depends on Stage 1 to find executable)
    std::string exe_path = find_executable(app_bundle_path, result.bundle);
    if (!exe_path.empty()) {
        result.macho = analyze_macho(exe_path);
        log_diag(LogLevel::Debug, "stage3_macho arch=" + std::string(to_string(result.macho.primary_architecture)) +
            " libs=" + std::to_string(result.macho.linked_libraries.size()));
    } else {
        result.unsupported_reasons.push_back("Cannot locate executable in bundle");
        log_diag(LogLevel::Warn, "stage3_macho missing_executable bundle=" + result.bundle.bundle_identifier);
    }

    // Stage 3b: Entitlement parsing
    std::string ent_path = find_entitlements_plist(app_bundle_path);
    if (!ent_path.empty()) {
        std::string ent_xml = read_file(ent_path);
        result.entitlements = parse_entitlements(ent_xml);
    }

    // Stage 2: Framework Fingerprinting
    if (registry) {
        result.frameworks = fingerprint_frameworks(*registry, result.bundle, result.macho, result.entitlements);
    } else {
        result.frameworks = fingerprint_frameworks(result.bundle, result.macho, result.entitlements);
    }

    for (auto& fw : result.frameworks) {
        if (fw.id == FrameworkId::Electron) {
            fw.version = detect_electron_version(app_bundle_path);
            log_diag(LogLevel::Info, "stage2_version electron_detected_version=" + fw.version);
        }
    }

    log_diag(LogLevel::Debug, "stage2_fingerprint frameworks_detected=" +
        std::to_string(result.frameworks.size()));

    // Stage 4: Capability Scoring
    result.score = compute_capability_score(result.frameworks, result.macho, result.entitlements);

    // Stage 5: Execution Strategy Resolution
    result.recommendation = resolve_execution_strategy(
        result.score, result.frameworks, result.macho, result.bundle);

    // Determine overall compatibility state
    result.compatibility_state = result.recommendation.expected_state;

    // Check for unsupported conditions
    for (const auto& f : result.frameworks) {
        if (f.id == FrameworkId::Hypervisor) {
            result.compatibility_state = CompatibilityState::Unsupported;
            result.unsupported_reasons.push_back(
                "Hypervisor.framework dependency — unsupported (Tier 4)");
        }
    }

    if (result.score.critical_count > 0 && !result.recommendation.vm_required) {
        result.compatibility_state = CompatibilityState::Degraded;
    }

    // Emit full classification diagnostic per architecture logging requirements
    log_classification(result);

    return result;
}

DetectionResult detect_capabilities(const std::string& app_bundle_path) {
    return run_pipeline(app_bundle_path, nullptr);
}

DetectionResult detect_capabilities(FingerprintRegistry& registry, const std::string& app_bundle_path) {
    return run_pipeline(app_bundle_path, &registry);
}

} // namespace platform
