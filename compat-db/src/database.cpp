// Compatibility Database Storage, Indexing, and Query Engine
// Architecture reference: docs/architecture/ARCHITECTURE_V6.md — "compat-db Contract"
// Responsibilities: compatibility metadata, known issues, execution recommendations,
// degradation policies, verification states.
// Never: launches applications, performs runtime analysis, owns execution logic.
//
// Uses nlohmann/json for robust, collision-free JSON parsing/serialization.
// Files loaded deterministically (lexicographic sort), duplicate IDs rejected.

#include <compatdb/database.hpp>
#include <compatdb/validator.hpp>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <json.hpp>

using json = nlohmann::json;

namespace compatdb {

// Forward: parse and serialize using nlohmann/json
static CompatibilityRecord parse_record(const json& j, const std::string& source_file);
static json serialize_record(const CompatibilityRecord& rec);

// ============================================================
// JSON → C++ type conversion helpers
// ============================================================

static std::string json_opt_string(const json& j, const std::string& key) {
    auto it = j.find(key);
    if (it == j.end() || it->is_null()) return {};
    if (it->is_string()) return it->get<std::string>();
    if (it->is_number()) return std::to_string(it->get<int64_t>());
    return {};
}

static int64_t json_opt_int(const json& j, const std::string& key, int64_t def = 0) {
    auto it = j.find(key);
    if (it == j.end() || it->is_null()) return def;
    if (it->is_number()) return it->get<int64_t>();
    return def;
}

static bool json_opt_bool(const json& j, const std::string& key) {
    auto it = j.find(key);
    if (it == j.end() || it->is_null()) return false;
    return it->get<bool>();
}

static std::vector<std::string> json_opt_string_array(const json& j, const std::string& key) {
    std::vector<std::string> result;
    auto it = j.find(key);
    if (it == j.end() || !it->is_array()) return result;
    for (const auto& v : *it) {
        if (v.is_string()) result.push_back(v.get<std::string>());
    }
    return result;
}

static CompatibilityRecord parse_record(const json& j, const std::string& source_file) {
    CompatibilityRecord rec;

    rec.schema_version      = json_opt_string(j, "schema_version");
    rec.record_id           = json_opt_string(j, "record_id");
    rec.bundle_identifier   = json_opt_string(j, "bundle_identifier");
    rec.application_name    = json_opt_string(j, "application_name");
    rec.application_version = json_opt_string(j, "application_version");
    rec.last_updated        = json_opt_string(j, "last_updated");
    rec.notes               = json_opt_string(j, "notes");

    rec.execution_tier      = tier_from_string(json_opt_string(j, "execution_tier"));
    rec.compatibility_state = state_from_string(json_opt_string(j, "compatibility_state"));

    // Tags
    rec.tags = json_opt_string_array(j, "tags");

    // Required flags
    auto rf_it = j.find("required_flags");
    if (rf_it != j.end() && rf_it->is_object()) {
        for (auto& [key, val] : rf_it->items()) {
            rec.required_flags.push_back({key, val.is_string() ? val.get<std::string>() : ""});
        }
    }

    // Tested distros
    auto td_it = j.find("tested_on");
    if (td_it != j.end() && td_it->is_array()) {
        for (const auto& d : *td_it) {
            TestedDistro td;
            td.distribution   = json_opt_string(d, "distribution");
            td.version        = json_opt_string(d, "version");
            td.kernel_version = json_opt_string(d, "kernel_version");
            td.arch           = json_opt_string(d, "arch");
            if (!td.distribution.empty()) rec.tested_on.push_back(td);
        }
    }

    // Known issues
    auto ki_it = j.find("known_issues");
    if (ki_it != j.end() && ki_it->is_array()) {
        for (const auto& iss : *ki_it) {
            KnownIssue ki;
            ki.id                   = json_opt_string(iss, "id");
            ki.description          = json_opt_string(iss, "description");
            ki.affected_subsystem   = json_opt_string(iss, "affected_subsystem");
            ki.reproduction_steps   = json_opt_string(iss, "reproduction_steps");
            ki.resolved_in_version  = json_opt_string(iss, "resolved_in_version");
            ki.severity             = severity_from_string(json_opt_string(iss, "severity"));
            if (!ki.id.empty()) rec.known_issues.push_back(ki);
        }
    }

    // Workarounds
    auto wa_it = j.find("workarounds");
    if (wa_it != j.end() && wa_it->is_array()) {
        for (const auto& w : *wa_it) {
            Workaround wa;
            wa.description = json_opt_string(w, "description");
            wa.requires_flags = json_opt_string_array(w, "requires_flags");
            // applies_to_issues
            auto ati = w.find("applies_to_issues");
            if (ati != w.end() && ati->is_array()) {
                for (const auto& ai : *ati) {
                    if (ai.is_string()) wa.applies_to_issues.push_back(ai.get<std::string>());
                }
            }
            if (!wa.description.empty()) rec.workarounds.push_back(wa);
        }
    }

    // macOS guest requirements
    auto vm_it = j.find("macos_guest_requirements");
    if (vm_it != j.end() && vm_it->is_object()) {
        MacOSGuestRequirements vm;
        vm.minimum_macos_version     = json_opt_string(*vm_it, "minimum_macos_version");
        vm.recommended_macos_version = json_opt_string(*vm_it, "recommended_macos_version");
        vm.special_configuration     = json_opt_string(*vm_it, "special_configuration");
        vm.minimum_ram_mb            = static_cast<uint32_t>(json_opt_int(*vm_it, "minimum_ram_mb", 4096));
        vm.minimum_disk_gb           = static_cast<uint32_t>(json_opt_int(*vm_it, "minimum_disk_gb", 30));
        vm.requires_metal            = json_opt_bool(*vm_it, "requires_metal");
        vm.requires_accessibility    = json_opt_bool(*vm_it, "requires_accessibility");
        if (!vm.minimum_macos_version.empty())
            rec.macos_guest_requirements = vm;
    }

    // Performance characteristics
    auto pc_it = j.find("performance_characteristics");
    if (pc_it != j.end() && pc_it->is_object()) {
        PerformanceCharacteristics perf;
        perf.startup_time_ms  = static_cast<uint32_t>(json_opt_int(*pc_it, "startup_time_ms"));
        perf.memory_usage_mb  = static_cast<uint32_t>(json_opt_int(*pc_it, "memory_usage_mb"));
        perf.cpu_overhead     = json_opt_string(*pc_it, "cpu_overhead");
        perf.notes            = json_opt_string(*pc_it, "notes");
        rec.performance_characteristics = perf;
    }

    // Contributor
    auto ci_it = j.find("contributor");
    if (ci_it != j.end() && ci_it->is_object()) {
        ContributorInfo ci;
        ci.name                = json_opt_string(*ci_it, "name");
        ci.contact             = json_opt_string(*ci_it, "contact");
        ci.verification_method = json_opt_string(*ci_it, "verification_method");
        if (!ci.name.empty()) rec.contributor = ci;
    }

    // Degradation metadata (per DEGRADATION_MODEL.md)
    auto dg_it = j.find("degradation");
    if (dg_it != j.end() && dg_it->is_object()) {
        DegradationMetadata dm;
        dm.category             = json_opt_string(*dg_it, "category");
        dm.confidence           = json_opt_string(*dg_it, "confidence");
        dm.recommended_action   = json_opt_string(*dg_it, "recommended_action");
        dm.active_shims         = json_opt_string_array(*dg_it, "active_shims");
        dm.degraded_capabilities = json_opt_string_array(*dg_it, "degraded_capabilities");
        dm.unsafe_bypasses      = json_opt_string_array(*dg_it, "unsafe_bypasses");
        dm.bypassed_modules     = json_opt_string_array(*dg_it, "bypassed_modules");
        dm.experimental_features = json_opt_string_array(*dg_it, "experimental_features");
        if (!dm.category.empty()) rec.degradation = dm;
    }

    // === Phase 4B: Architecture class (optional, default ClassA) ===
    auto ac = json_opt_string(j, "architecture_class");
    if (!ac.empty()) rec.architecture_class = arch_class_from_string(ac);

    // === Phase 4B: Critical native modules ===
    auto cnm_it = j.find("critical_native_modules");
    if (cnm_it != j.end() && cnm_it->is_array()) {
        for (const auto& m : *cnm_it) {
            CriticalNativeModule cnm;
            cnm.module               = json_opt_string(m, "module");
            cnm.role                 = json_opt_string(m, "role");
            cnm.requires_compilation = json_opt_bool(m, "requires_compilation");
            if (!cnm.module.empty()) rec.critical_native_modules.push_back(cnm);
        }
    }

    // === Phase 4B: External processes ===
    auto ep_it = j.find("external_processes");
    if (ep_it != j.end() && ep_it->is_array()) {
        for (const auto& p : *ep_it) {
            ExternalProcess ep;
            ep.name              = json_opt_string(p, "name");
            ep.type              = json_opt_string(p, "type");
            ep.protocol          = json_opt_string(p, "protocol");
            ep.binary_type       = json_opt_string(p, "binary_type");
            ep.substitution_env  = json_opt_string(p, "substitution_env");
            if (!ep.name.empty()) rec.external_processes.push_back(ep);
        }
    }

    // === Phase 4B: Runtime policy ===
    auto rp_it = j.find("runtime_policy");
    if (rp_it != j.end() && rp_it->is_object()) {
        RuntimePolicy rp;
        rp.minimum    = json_opt_string(*rp_it, "minimum");
        auto pref = rp_it->find("preferred");
        if (pref != rp_it->end() && pref->is_array()) {
            for (const auto& v : *pref) { if (v.is_string()) rp.preferred.push_back(v.get<std::string>()); }
        }
        auto val = rp_it->find("validated");
        if (val != rp_it->end() && val->is_array()) {
            for (const auto& v : *val) { if (v.is_string()) rp.validated.push_back(v.get<std::string>()); }
        }
        auto fb = rp_it->find("fallback");
        if (fb != rp_it->end() && fb->is_array()) {
            for (const auto& v : *fb) { if (v.is_string()) rp.fallback.push_back(v.get<std::string>()); }
        }
        if (!rp.preferred.empty() || !rp.minimum.empty()) rec.runtime_policy = rp;
    }

    return rec;
}

static json serialize_record(const CompatibilityRecord& rec) {
    json j;

    j["schema_version"] = rec.schema_version;
    j["record_id"] = rec.record_id;
    j["bundle_identifier"] = rec.bundle_identifier;
    j["application_name"] = rec.application_name;
    if (!rec.application_version.empty())
        j["application_version"] = rec.application_version;
    j["execution_tier"] = tier_to_string(rec.execution_tier);
    j["compatibility_state"] = state_to_string(rec.compatibility_state);
    j["last_updated"] = rec.last_updated;
    if (!rec.notes.empty())
        j["notes"] = rec.notes;

    if (!rec.tags.empty()) {
        json tags = json::array();
        for (const auto& t : rec.tags) tags.push_back(t);
        j["tags"] = tags;
    }

    if (!rec.required_flags.empty()) {
        json flags = json::object();
        for (const auto& [k, v] : rec.required_flags) flags[k] = v;
        j["required_flags"] = flags;
    }

    if (!rec.tested_on.empty()) {
        json dists = json::array();
        for (const auto& d : rec.tested_on) {
            json entry;
            entry["distribution"] = d.distribution;
            entry["version"] = d.version;
            if (!d.kernel_version.empty()) entry["kernel_version"] = d.kernel_version;
            if (!d.arch.empty()) entry["arch"] = d.arch;
            dists.push_back(entry);
        }
        j["tested_on"] = dists;
    }

    if (!rec.known_issues.empty()) {
        json issues = json::array();
        for (const auto& ki : rec.known_issues) {
            json issue;
            issue["id"] = ki.id;
            issue["severity"] = severity_to_string(ki.severity);
            issue["description"] = ki.description;
            if (!ki.affected_subsystem.empty()) issue["affected_subsystem"] = ki.affected_subsystem;
            if (!ki.reproduction_steps.empty()) issue["reproduction_steps"] = ki.reproduction_steps;
            if (!ki.resolved_in_version.empty()) issue["resolved_in_version"] = ki.resolved_in_version;
            issues.push_back(issue);
        }
        j["known_issues"] = issues;
    }

    if (!rec.workarounds.empty()) {
        json was = json::array();
        for (const auto& w : rec.workarounds) {
            json wa;
            wa["description"] = w.description;
            if (!w.applies_to_issues.empty()) {
                json ait = json::array();
                for (const auto& ai : w.applies_to_issues) ait.push_back(ai);
                wa["applies_to_issues"] = ait;
            }
            if (!w.requires_flags.empty()) {
                json rf = json::array();
                for (const auto& f : w.requires_flags) rf.push_back(f);
                wa["requires_flags"] = rf;
            }
            was.push_back(wa);
        }
        j["workarounds"] = was;
    }

    if (rec.macos_guest_requirements.has_value()) {
        json vm;
        const auto& v = *rec.macos_guest_requirements;
        vm["minimum_macos_version"] = v.minimum_macos_version;
        if (!v.recommended_macos_version.empty())
            vm["recommended_macos_version"] = v.recommended_macos_version;
        vm["minimum_ram_mb"] = v.minimum_ram_mb;
        vm["minimum_disk_gb"] = v.minimum_disk_gb;
        vm["requires_metal"] = v.requires_metal;
        vm["requires_accessibility"] = v.requires_accessibility;
        if (!v.special_configuration.empty())
            vm["special_configuration"] = v.special_configuration;
        j["macos_guest_requirements"] = vm;
    }

    if (rec.performance_characteristics.has_value()) {
        json perf;
        const auto& p = *rec.performance_characteristics;
        if (p.startup_time_ms > 0) perf["startup_time_ms"] = p.startup_time_ms;
        if (p.memory_usage_mb > 0) perf["memory_usage_mb"] = p.memory_usage_mb;
        if (!p.cpu_overhead.empty()) perf["cpu_overhead"] = p.cpu_overhead;
        if (!p.notes.empty()) perf["notes"] = p.notes;
        j["performance_characteristics"] = perf;
    }

    if (rec.contributor.has_value()) {
        json contrib;
        contrib["name"] = rec.contributor->name;
        if (!rec.contributor->contact.empty()) contrib["contact"] = rec.contributor->contact;
        if (!rec.contributor->verification_method.empty())
            contrib["verification_method"] = rec.contributor->verification_method;
        j["contributor"] = contrib;
    }

    if (rec.degradation.has_value()) {
        json dm;
        const auto& d = *rec.degradation;
        dm["category"] = d.category;
        if (!d.confidence.empty()) dm["confidence"] = d.confidence;
        if (!d.recommended_action.empty()) dm["recommended_action"] = d.recommended_action;
        if (!d.active_shims.empty()) dm["active_shims"] = d.active_shims;
        if (!d.degraded_capabilities.empty()) dm["degraded_capabilities"] = d.degraded_capabilities;
        if (!d.unsafe_bypasses.empty()) dm["unsafe_bypasses"] = d.unsafe_bypasses;
        if (!d.bypassed_modules.empty()) dm["bypassed_modules"] = d.bypassed_modules;
        if (!d.experimental_features.empty()) dm["experimental_features"] = d.experimental_features;
        j["degradation"] = dm;
    }

    // === Phase 4B: New schema v1.1.0 fields ===
    if (rec.architecture_class.has_value()) {
        j["architecture_class"] = arch_class_to_string(*rec.architecture_class);
    }

    if (!rec.critical_native_modules.empty()) {
        json cnms = json::array();
        for (const auto& cnm : rec.critical_native_modules) {
            json m;
            m["module"] = cnm.module;
            m["role"] = cnm.role;
            m["requires_compilation"] = cnm.requires_compilation;
            cnms.push_back(m);
        }
        j["critical_native_modules"] = cnms;
    }

    if (!rec.external_processes.empty()) {
        json eps = json::array();
        for (const auto& ep : rec.external_processes) {
            json p;
            p["name"] = ep.name;
            p["type"] = ep.type;
            if (!ep.protocol.empty()) p["protocol"] = ep.protocol;
            if (!ep.binary_type.empty()) p["binary_type"] = ep.binary_type;
            if (!ep.substitution_env.empty()) p["substitution_env"] = ep.substitution_env;
            eps.push_back(p);
        }
        j["external_processes"] = eps;
    }

    if (rec.runtime_policy.has_value()) {
        json rp;
        const auto& r = *rec.runtime_policy;
        if (!r.minimum.empty()) rp["minimum"] = r.minimum;
        if (!r.preferred.empty()) {
            json pref = json::array();
            for (const auto& v : r.preferred) pref.push_back(v);
            rp["preferred"] = pref;
        }
        if (!r.validated.empty()) {
            json val = json::array();
            for (const auto& v : r.validated) val.push_back(v);
            rp["validated"] = val;
        }
        if (!r.fallback.empty()) {
            json fb = json::array();
            for (const auto& v : r.fallback) fb.push_back(v);
            rp["fallback"] = fb;
        }
        j["runtime_policy"] = rp;
    }

    return j;
}

// ============================================================
// Static helpers retained for external callers
// ============================================================

CompatibilityRecord CompatDatabase::parse_json(const std::string& json_text, const std::string& source_file) {
    return parse_record(json::parse(json_text), source_file);
}

std::string CompatDatabase::serialize_json(const CompatibilityRecord& record) {
    return serialize_record(record).dump(2);
}

// ============================================================
// Database implementation
// ============================================================

CompatDatabase::CompatDatabase() = default;

void CompatDatabase::rebuild_indices() {
    id_index_.clear();
    bundle_index_.clear();
    tag_index_.clear();

    for (size_t i = 0; i < records_.size(); i++) {
        id_index_[records_[i].record.record_id] = i;
        bundle_index_[records_[i].record.bundle_identifier].push_back(i);
        for (const auto& tag : records_[i].record.tags) {
            tag_index_[tag].push_back(i);
        }
    }
}

size_t CompatDatabase::load_directory(const std::string& directory_path) {
    std::error_code ec;
    namespace fs = std::filesystem;

    // Collect and sort file paths deterministically (lexicographic)
    std::vector<std::string> json_files;
    for (const auto& entry : fs::directory_iterator(directory_path, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".json") continue;
        json_files.push_back(entry.path().string());
    }
    std::sort(json_files.begin(), json_files.end());

    size_t loaded = 0;
    std::unordered_map<std::string, std::string> seen_ids; // track duplicates

    for (const auto& file_path : json_files) {
        auto json_text = [&]() -> std::string {
            std::ifstream f(file_path);
            if (!f) return {};
            std::ostringstream ss;
            ss << f.rdbuf();
            return ss.str();
        }();

        if (json_text.empty()) {
            load_errors_.push_back({file_path, "Empty or unreadable file"});
            invalid_count_++;
            continue;
        }

        CompatibilityRecord record;
        try {
            record = parse_record(json::parse(json_text), file_path);
        } catch (const json::parse_error& e) {
            load_errors_.push_back({file_path, "JSON parse error: " + std::string(e.what())});
            invalid_count_++;
            continue;
        }

        auto validation = validate_full(record);
        if (!validation.valid) {
            invalid_count_++;
            for (const auto& e : validation.errors) {
                load_errors_.push_back({file_path, e.field + ": " + e.message});
            }
            continue;
        }

        // Reject duplicate record IDs
        auto dup = seen_ids.find(record.record_id);
        if (dup != seen_ids.end()) {
            load_errors_.push_back({file_path,
                "Duplicate record_id '" + record.record_id + "' — already loaded from " + dup->second});
            invalid_count_++;
            continue;
        }

        records_.push_back({std::move(record), file_path});
        seen_ids[records_.back().record.record_id] = file_path;
        valid_count_++;
        loaded++;
    }

    rebuild_indices();
    return loaded;
}

bool CompatDatabase::load_file(const std::string& file_path) {
    auto json_text = [&]() -> std::string {
        std::ifstream f(file_path);
        if (!f) return {};
        std::ostringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }();

    if (json_text.empty()) {
        load_errors_.push_back({file_path, "Empty or unreadable file"});
        return false;
    }

    CompatibilityRecord record;
    try {
        record = parse_record(json::parse(json_text), file_path);
    } catch (const json::parse_error& e) {
        load_errors_.push_back({file_path, "JSON parse error: " + std::string(e.what())});
        return false;
    }

    auto validation = validate_full(record);
    if (!validation.valid) {
        invalid_count_++;
        for (const auto& e : validation.errors) {
            load_errors_.push_back({file_path, e.field + ": " + e.message});
        }
        return false;
    }

    records_.push_back({std::move(record), file_path});
    valid_count_++;
    rebuild_indices();
    return true;
}

void CompatDatabase::add_record(CompatibilityRecord record) {
    auto validation = validate_full(record);
    if (!validation.valid) {
        invalid_count_++;
        return;
    }
    // Programmatic records have no source file
    records_.push_back({std::move(record), ""});
    valid_count_++;
    rebuild_indices();
}

// ============================================================
// Query helpers
// ============================================================

static QueryResult make_result(const InternalRecord& ir, double score = 1.0) {
    return {ir.record, ir.source_file, score};
}

const CompatibilityRecord* CompatDatabase::lookup_by_id(const std::string& record_id) const {
    auto it = id_index_.find(record_id);
    if (it == id_index_.end()) return nullptr;
    return &records_[it->second].record;
}

std::vector<QueryResult> CompatDatabase::lookup_by_bundle_id(const std::string& bundle_identifier) const {
    std::vector<QueryResult> results;
    auto it = bundle_index_.find(bundle_identifier);
    if (it == bundle_index_.end()) return results;
    for (auto idx : it->second) results.push_back(make_result(records_[idx]));
    return results;
}

std::vector<QueryResult> CompatDatabase::lookup_by_bundle_pattern(const std::string& pattern) const {
    std::vector<QueryResult> results;
    for (const auto& ir : records_) {
        if (ir.record.bundle_identifier.find(pattern) != std::string::npos)
            results.push_back(make_result(ir));
    }
    return results;
}

std::vector<QueryResult> CompatDatabase::filter_by_state(CompatibilityState state) const {
    std::vector<QueryResult> results;
    for (const auto& ir : records_) {
        if (ir.record.compatibility_state == state) results.push_back(make_result(ir));
    }
    return results;
}

std::vector<QueryResult> CompatDatabase::filter_by_tier(ExecutionTier tier) const {
    std::vector<QueryResult> results;
    for (const auto& ir : records_) {
        if (ir.record.execution_tier == tier) results.push_back(make_result(ir));
    }
    return results;
}

std::vector<QueryResult> CompatDatabase::filter_by_tag(const std::string& tag) const {
    std::vector<QueryResult> results;
    auto it = tag_index_.find(tag);
    if (it == tag_index_.end()) return results;
    for (auto idx : it->second) results.push_back(make_result(records_[idx]));
    return results;
}

std::vector<QueryResult> CompatDatabase::search_by_name(const std::string& name) const {
    std::vector<QueryResult> results;
    std::string lower_name = name;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(),
        [](unsigned char c) { return std::tolower(c); });

    for (const auto& ir : records_) {
        std::string lower_app = ir.record.application_name;
        std::transform(lower_app.begin(), lower_app.end(), lower_app.begin(),
            [](unsigned char c) { return std::tolower(c); });
        if (lower_app.find(lower_name) != std::string::npos)
            results.push_back(make_result(ir));
    }
    return results;
}

std::vector<QueryResult> CompatDatabase::all_records() const {
    std::vector<QueryResult> results;
    for (const auto& ir : records_) results.push_back(make_result(ir));
    return results;
}

} // namespace compatdb
