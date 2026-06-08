// Native ABI Database — Implementation
#include <native/native_abi_db.hpp>
#include <fstream>
#include <sstream>
#include <json.hpp>

using json = nlohmann::json;

namespace platform::native {

bool ABIDatabase::load(const std::string& manifest_path) {
    try {
        std::ifstream f(manifest_path);
        if (!f) return false;
        std::ostringstream ss;
        ss << f.rdbuf();
        auto j = json::parse(ss.str());
        auto abi_map = j.find("abi_map");
        if (abi_map == j.end() || !abi_map->is_object()) return false;
        for (auto& [key, val] : abi_map->items()) {
            ABIIndex abi;
            abi.node_module_version = val.value("node_module_version", 0U);
            abi.node_version        = val.value("node_version", std::string{});
            abi.v8_version          = val.value("v8_version", std::string{});
            abi_map_[key] = abi;
        }
        return !abi_map_.empty();
    } catch (...) {
        return false;
    }
}

ABIIndex ABIDatabase::resolve(const std::string& electron_version) const {
    std::string major;
    for (char c : electron_version) {
        if (c == '.') break;
        if (c >= '0' && c <= '9') major += c;
    }
    if (major.empty()) return ABIIndex{};
    auto it = abi_map_.find(major);
    if (it != abi_map_.end()) return it->second;
    return ABIIndex{};
}

ABIMatchResult ABIDatabase::verify(const DiscoveredNativeModule& module, const ABIIndex& abi) const {
    if (module.magic == 0x7F454C46) return ABIMatchResult::ELF_SAFE;
    bool is_macho = (module.magic == 0xFEEDFACE || module.magic == 0xFEEDFACF ||
                     module.magic == 0xCEFAEDFE || module.magic == 0xCFFAEDFE ||
                     module.magic == 0xCAFEBABE || module.magic == 0xBEBAFECA);
    if (!is_macho) return ABIMatchResult::ABI_UNKNOWN;
    if (abi.node_module_version == 0 || module.node_api_version == 0) return ABIMatchResult::ABI_UNKNOWN;
    if (module.node_api_version == abi.node_module_version) return ABIMatchResult::ABI_MATCH;
    return ABIMatchResult::ABI_MISMATCH;
}

bool ABIDatabase::is_known_version(const std::string& electron_version) const {
    std::string major;
    for (char c : electron_version) {
        if (c == '.') break;
        if (c >= '0' && c <= '9') major += c;
    }
    return abi_map_.find(major) != abi_map_.end();
}

} // namespace platform::native
