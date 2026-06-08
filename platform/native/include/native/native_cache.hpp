// Native Module Binary Cache — Header
// Content-addressed cache at ~/.cache/macrun/native/. Cache key includes host glibc.
// Architecture reference: Native Module Compatibility Infrastructure Plan v3 § Cache Architecture
#pragma once
#include <native/native_types.hpp>
#include <string>
#include <json.hpp>

namespace platform::native {

std::string compute_cache_key(const std::string& module_name,
                               const std::string& npm_version,
                               const std::string& electron_version,
                               const std::string& arch);

CacheResult probe_cache(const std::string& cache_root, const std::string& cache_key);

bool stage_from_cache(const CacheResult& cached, const std::string& dest_path);

bool write_to_cache(const std::string& cache_root, const std::string& cache_key,
                    const std::string& binary_path, const nlohmann::json& manifest,
                    const std::string& build_log);

void evict_if_needed(const std::string& cache_root, size_t max_bytes);

} // namespace platform::native
