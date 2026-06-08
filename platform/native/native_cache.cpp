// Native Module Binary Cache — Implementation
#include <native/native_cache.hpp>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <chrono>
#include <algorithm>
#include <cstring>
#include <sys/file.h>
#include <unistd.h>
#include <gnu/libc-version.h>
#include <openssl/evp.h>
#include <iostream>
#include <json.hpp>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace platform::native {

static std::string detect_host_glibc() {
    const char* version = gnu_get_libc_version();
    if (version && version[0]) return std::string(version);
    return "unknown";
}

static std::string sha256_hex(const std::string& input) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len = 0;
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return {};
    EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
    EVP_DigestUpdate(ctx, input.data(), input.size());
    EVP_DigestFinal_ex(ctx, hash, &hash_len);
    EVP_MD_CTX_free(ctx);
    std::string hex;
    hex.reserve(hash_len * 2);
    for (unsigned int i = 0; i < hash_len; i++) {
        char buf[3];
        snprintf(buf, sizeof(buf), "%02x", hash[i]);
        hex += buf;
    }
    return hex;
}

std::string compute_cache_key(const std::string& module_name,
                               const std::string& npm_version,
                               const std::string& electron_version,
                               const std::string& arch) {
    std::string glibc = detect_host_glibc();
    std::string glibc_short;
    int dots = 0;
    for (char c : glibc) {
        if (c == '.') { dots++; if (dots >= 3) break; glibc_short += c; }
        else if (c >= '0' && c <= '9') glibc_short += c;
        else break;
    }
    std::string input = module_name + "@" + npm_version +
                        "+electron" + electron_version +
                        "+" + arch + "+glibc" + glibc_short;
    return sha256_hex(input);
}

CacheResult probe_cache(const std::string& cache_root, const std::string& cache_key) {
    CacheResult result;
    if (cache_key.length() < 4) return result;
    std::string prefix = cache_key.substr(0, 2);
    fs::path cache_dir = fs::path(cache_root) / prefix / cache_key;
    if (!fs::exists(cache_dir) || !fs::is_directory(cache_dir)) return result;
    fs::path binary_path = cache_dir / (cache_key + ".node");
    fs::path manifest_path = cache_dir / "manifest.json";
    if (!fs::exists(binary_path) || !fs::exists(manifest_path)) return result;
    fs::path sha256_path = cache_dir / (cache_key + ".node.sha256");
    if (fs::exists(sha256_path)) {
        std::ifstream sf(sha256_path);
        std::string expected_sha256;
        std::getline(sf, expected_sha256);
        std::ifstream bf(binary_path, std::ios::binary);
        if (!bf) return result;
        std::ostringstream bss;
        bss << bf.rdbuf();
        if (expected_sha256 != sha256_hex(bss.str())) return result;
    }
    result.hit = true;
    result.binary_path = binary_path.string();
    result.manifest_path = manifest_path.string();
    return result;
}

bool stage_from_cache(const CacheResult& cached, const std::string& dest_path) {
    if (!cached.hit || cached.binary_path.empty()) {
        std::cerr << "[macrun:cache:error] stage_from_cache failed: cache hit is false or binary path empty\n";
        return false;
    }
    std::error_code ec;
    fs::path dest(dest_path);
    if (dest.has_parent_path()) {
        fs::create_directories(dest.parent_path(), ec);
        if (ec) {
            std::cerr << "[macrun:cache:error] create_directories failed for " << dest.parent_path() << ": " << ec.message() << "\n";
            return false;
        }
    }
    fs::copy_file(cached.binary_path, dest, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        std::cerr << "[macrun:cache:error] copy_file failed from " << cached.binary_path << " to " << dest << ": " << ec.message() << "\n";
        return false;
    }
    fs::permissions(dest, fs::perms::owner_read | fs::perms::group_read | fs::perms::others_read,
                    fs::perm_options::replace, ec);
    return true;
}

bool write_to_cache(const std::string& cache_root, const std::string& cache_key,
                    const std::string& binary_path, const nlohmann::json& manifest,
                    const std::string& build_log) {
    if (cache_key.length() < 4) return false;
    std::error_code ec;
    fs::create_directories(fs::path(cache_root), ec);
    fs::path lock_path = fs::path(cache_root) / ".lock";
    int lock_fd = open(lock_path.c_str(), O_CREAT | O_RDWR, 0644);
    if (lock_fd < 0) return false;
    if (flock(lock_fd, LOCK_EX) != 0) { close(lock_fd); return false; }
    std::string prefix = cache_key.substr(0, 2);
    fs::path cache_dir = fs::path(cache_root) / prefix / cache_key;
    fs::create_directories(cache_dir, ec);
    if (ec) { flock(lock_fd, LOCK_UN); close(lock_fd); return false; }
    fs::path dest_binary = cache_dir / (cache_key + ".node");
    fs::copy_file(binary_path, dest_binary, fs::copy_options::overwrite_existing, ec);
    if (ec) { flock(lock_fd, LOCK_UN); close(lock_fd); return false; }
    std::ifstream bf(dest_binary, std::ios::binary);
    std::ostringstream bss;
    bss << bf.rdbuf();
    std::ofstream sha256_file(cache_dir / (cache_key + ".node.sha256"));
    sha256_file << sha256_hex(bss.str()) << "\n";
    sha256_file.close();
    std::ofstream manifest_file(cache_dir / "manifest.json");
    manifest_file << manifest.dump(2) << "\n";
    manifest_file.close();
    if (!build_log.empty()) { std::ofstream log_file(cache_dir / "build.log"); log_file << build_log; }
    flock(lock_fd, LOCK_UN);
    close(lock_fd);
    return true;
}

void evict_if_needed(const std::string& cache_root, size_t max_bytes) {
    fs::path root(cache_root);
    if (!fs::exists(root)) return;
    fs::path lock_path = root / ".lock";
    int lock_fd = open(lock_path.c_str(), O_CREAT | O_RDWR, 0644);
    if (lock_fd < 0) return;
    if (flock(lock_fd, LOCK_EX) != 0) { close(lock_fd); return; }
    struct CacheEntry { fs::path dir; uint64_t size_bytes = 0; std::string build_timestamp; };
    std::vector<CacheEntry> entries;
    uint64_t total_size = 0;
    std::error_code ec;
    for (const auto& prefix_entry : fs::directory_iterator(root, ec)) {
        if (!prefix_entry.is_directory()) continue;
        if (prefix_entry.path().filename() == ".lock") continue;
        for (const auto& entry : fs::directory_iterator(prefix_entry.path(), ec)) {
            if (!entry.is_directory()) continue;
            CacheEntry ce; ce.dir = entry.path();
            for (const auto& file : fs::directory_iterator(entry.path(), ec))
                if (file.is_regular_file()) ce.size_bytes += file.file_size(ec);
            std::ifstream mf(entry.path() / "manifest.json");
            if (mf) { std::ostringstream mss; mss << mf.rdbuf();
                try { auto j = json::parse(mss.str()); ce.build_timestamp = j.value("build_timestamp", std::string{}); } catch (...) {} }
            total_size += ce.size_bytes; entries.push_back(ce);
        }
    }
    std::sort(entries.begin(), entries.end(), [](const CacheEntry& a, const CacheEntry& b) { return a.build_timestamp < b.build_timestamp; });
    for (const auto& entry : entries) { if (total_size <= max_bytes) break; fs::remove_all(entry.dir, ec); if (!ec) total_size -= entry.size_bytes; }
    flock(lock_fd, LOCK_UN);
    close(lock_fd);
}

} // namespace platform::native
