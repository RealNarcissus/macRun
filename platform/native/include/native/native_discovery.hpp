// Native Module Discovery — Header
// Scans extracted app directory for .node files post-ASAR extraction.
// Architecture reference: Native Module Compatibility Infrastructure Plan v3
#pragma once
#include <native/native_types.hpp>
#include <vector>
#include <string>

namespace compatdb { struct CompatibilityRecord; }

namespace platform::native {

std::vector<DiscoveredNativeModule> scan_native_modules(const std::string& app_dir);
void classify_criticality(std::vector<DiscoveredNativeModule>& modules,
                           const compatdb::CompatibilityRecord& record);

} // namespace platform::native
