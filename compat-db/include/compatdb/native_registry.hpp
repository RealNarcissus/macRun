// Native Module Replacement Registry
// Loads compat-db/manifests/native/registry.json and queries governed replacements.
// Architecture reference: Native Module Compatibility Infrastructure Plan v3 § Governed Registry

#pragma once
#include <compatdb/types.hpp>
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace compatdb {

struct GovernedVersion {
    std::string npm_version;
    std::string sha256;
    uint32_t node_module_version = 0;
    std::vector<std::string> electron_versions;
    std::vector<std::string> build_flags;
    std::map<std::string, std::string> build_env;
    bool known_good = false;
    std::vector<std::string> patches;
    std::vector<KnownBadEntry> known_bad_on;
    std::string shim_type;
    std::map<std::string, std::string> dependencies;
    std::string npm_package;
};

class NativeRegistry {
public:
    bool load(const std::string& registry_path);

    std::optional<NativeReplacementRecord> find_replacement(
        const std::string& module_name,
        const std::string& electron_version,
        const std::string& arch) const;

    bool is_always_stub(const std::string& module_name) const;

private:
    // module_name → list of governed versions
    std::unordered_map<std::string, std::vector<GovernedVersion>> entries_;
    std::unordered_set<std::string> always_stub_modules_;
};

} // namespace compatdb
