// Native Module Replacement Registry Implementation
// Loads compat-db/manifests/native/registry.json and queries governed replacements.
// Architecture reference: Native Module Compatibility Infrastructure Plan v3 § Governed Registry

#include <compatdb/native_registry.hpp>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <json.hpp>

using json = nlohmann::json;

namespace compatdb {

bool NativeRegistry::load(const std::string& registry_path) {
    try {
        std::ifstream f(registry_path);
        if (!f) return false;
        std::ostringstream ss;
        ss << f.rdbuf();
        auto j = json::parse(ss.str());

        auto entries = j.find("entries");
        if (entries == j.end() || !entries->is_object()) return false;

        for (auto& [module_name, entry_val] : entries->items()) {
            if (!entry_val.is_object()) continue;

            std::string stub_policy = entry_val.value("stub_policy", std::string{});
            if (stub_policy == "always_stub") {
                always_stub_modules_.insert(module_name);
                continue;  // no governed versions for stub-only modules
            }

            auto gv = entry_val.find("governed_versions");
            if (gv == entry_val.end() || !gv->is_object()) continue;

            std::vector<GovernedVersion> versions;
            for (auto& [ver_key, ver_val] : gv->items()) {
                if (!ver_val.is_object()) continue;

                GovernedVersion gver;
                gver.npm_version = ver_val.value("npm_version", std::string{});

                // SHA256: empty string = not yet pinned (accepted, emitted as diagnostic)
                gver.sha256 = ver_val.value("sha256", std::string{});

                // Build flags
                auto bf = ver_val.find("build_flags");
                if (bf != ver_val.end() && bf->is_array()) {
                    for (const auto& flag : *bf) {
                        if (flag.is_string()) gver.build_flags.push_back(flag.get<std::string>());
                    }
                }

                // Build env
                auto be = ver_val.find("build_env");
                if (be != ver_val.end() && be->is_object()) {
                    for (auto& [ek, ev] : be->items()) {
                        gver.build_env[ek] = ev.is_string() ? ev.get<std::string>() : "";
                    }
                }

                // Electron ABI
                auto abi = ver_val.find("electron_abi");
                if (abi != ver_val.end() && abi->is_object()) {
                    gver.node_module_version = abi->value("node_module_version", 0U);
                    auto evs = abi->find("electron_versions");
                    if (evs != abi->end() && evs->is_array()) {
                        for (const auto& ev : *evs) {
                            if (ev.is_string()) gver.electron_versions.push_back(ev.get<std::string>());
                        }
                    }
                }

                gver.known_good = ver_val.value("known_good", false);

                // Patches
                auto pt = ver_val.find("patches");
                if (pt != ver_val.end() && pt->is_array()) {
                    for (const auto& patch : *pt) {
                        if (patch.is_string()) gver.patches.push_back(patch.get<std::string>());
                    }
                }

                // Known bad on
                auto kb = ver_val.find("known_bad_on");
                if (kb != ver_val.end() && kb->is_array()) {
                    for (const auto& entry : *kb) {
                        if (entry.is_object()) {
                            KnownBadEntry kbe;
                            kbe.electron_major = entry.value("electron_major", 0U);
                            kbe.reason = entry.value("reason", "");
                            gver.known_bad_on.push_back(kbe);
                        }
                    }
                }
                gver.shim_type = ver_val.value("shim_type", "");

                // Dependencies
                auto deps = ver_val.find("dependencies");
                if (deps != ver_val.end() && deps->is_object()) {
                    for (auto& [dk, dv] : deps->items()) {
                        gver.dependencies[dk] = dv.is_string() ? dv.get<std::string>() : "";
                    }
                }
                gver.npm_package = ver_val.value("npm_package", module_name);

                if (!gver.npm_version.empty()) {
                    versions.push_back(std::move(gver));
                }
            }

            if (!versions.empty()) {
                entries_[module_name] = std::move(versions);
            }
        }

        return true;
    } catch (...) {
        return false;
    }
}

std::optional<NativeReplacementRecord> NativeRegistry::find_replacement(
    const std::string& module_name,
    const std::string& electron_version,
    const std::string& arch) const {

    auto it = entries_.find(module_name);
    if (it == entries_.end()) return std::nullopt;

    const auto& versions = it->second;
    if (versions.empty()) return std::nullopt;

    // Normalize electron_version to major (e.g. "42.3.3" → "42")
    std::string major;
    for (char c : electron_version) {
        if (c == '.') break;
        if (c >= '0' && c <= '9') major += c;
    }
    uint32_t target_major = major.empty() ? 0 : std::stoul(major);

    // Resolve target NMV from electron-abi-map.json if possible
    uint32_t target_nmv = 0;
    const char* home = std::getenv("HOME");
    if (home) {
        std::string abi_path = std::string(home) + "/.cache/macrun/manifests/electron-abi-map.json";
        std::ifstream f(abi_path);
        if (f) {
            try {
                json j = json::parse(f);
                auto abi_map = j.find("abi_map");
                if (abi_map != j.end() && abi_map->is_object()) {
                    auto entry = abi_map->find(major);
                    if (entry != abi_map->end() && entry->is_object()) {
                        target_nmv = entry->value("node_module_version", 0U);
                    }
                }
            } catch (...) {
                // ignore
            }
        }
    }

    // Phase 1: Exact electron_version match (any version entry listing this major and not known bad)
    for (const auto& gv : versions) {
        bool is_bad = false;
        for (const auto& kb : gv.known_bad_on) {
            if (kb.electron_major == target_major) {
                is_bad = true;
                break;
            }
        }
        if (is_bad) continue;

        for (const auto& ev : gv.electron_versions) {
            if (ev == major) {
                NativeReplacementRecord rec;
                rec.module_name = module_name;
                rec.npm_version = gv.npm_version;
                rec.sha256 = gv.sha256;
                rec.required_nmv = gv.node_module_version;
                rec.build_flags = gv.build_flags;
                rec.build_env = gv.build_env;
                rec.known_good = gv.known_good;
                rec.patches = gv.patches;
                rec.known_bad_on = gv.known_bad_on;
                rec.shim_type = gv.shim_type;
                rec.dependencies = gv.dependencies;
                rec.npm_package = gv.npm_package;
                return rec;
            }
        }
    }

    // Phase 2: Match by node_module_version compatibility (minimum delta, not known bad)
    const GovernedVersion* best_gv = nullptr;
    uint32_t min_delta = 0xFFFFFFFFU;

    for (const auto& gv : versions) {
        bool is_bad = false;
        for (const auto& kb : gv.known_bad_on) {
            if (kb.electron_major == target_major) {
                is_bad = true;
                break;
            }
        }
        if (is_bad) continue;

        if (target_nmv > 0 && gv.node_module_version > 0) {
            uint32_t delta = (gv.node_module_version > target_nmv)
                ? (gv.node_module_version - target_nmv)
                : (target_nmv - gv.node_module_version);
            if (delta < min_delta) {
                min_delta = delta;
                best_gv = &gv;
            }
        } else {
            if (!best_gv) {
                best_gv = &gv;
            }
        }
    }

    if (best_gv) {
        NativeReplacementRecord rec;
        rec.module_name = module_name;
        rec.npm_version = best_gv->npm_version;
        rec.sha256 = best_gv->sha256;
        rec.required_nmv = best_gv->node_module_version;
        rec.build_flags = best_gv->build_flags;
        rec.build_env = best_gv->build_env;
        rec.known_good = best_gv->known_good;
        rec.patches = best_gv->patches;
        rec.known_bad_on = best_gv->known_bad_on;
        rec.shim_type = best_gv->shim_type;
        rec.dependencies = best_gv->dependencies;
        rec.npm_package = best_gv->npm_package;
        return rec;
    }

    return std::nullopt;
}

bool NativeRegistry::is_always_stub(const std::string& module_name) const {
    return always_stub_modules_.find(module_name) != always_stub_modules_.end();
}

} // namespace compatdb
