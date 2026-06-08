// Native ABI Database — Header
// Loads electron-abi-map.json and resolves Electron version → ABIIndex.
// Architecture reference: Native Module Compatibility Infrastructure Plan v3 § ABI Governance
#pragma once
#include <native/native_types.hpp>
#include <string>
#include <unordered_map>

namespace platform::native {

class ABIDatabase {
public:
    bool load(const std::string& manifest_path);
    ABIIndex resolve(const std::string& electron_version) const;
    ABIMatchResult verify(const DiscoveredNativeModule& module, const ABIIndex& abi) const;
    bool is_known_version(const std::string& electron_version) const;

private:
    std::unordered_map<std::string, ABIIndex> abi_map_;
};

} // namespace platform::native
