// Native Module Discovery Engine — Implementation
#include <native/native_discovery.hpp>
#include <compatdb/types.hpp>
#include <filesystem>
#include <fstream>
#include <cstring>

namespace fs = std::filesystem;

namespace platform::native {

static constexpr uint32_t ELF_MAGIC       = 0x7F454C46;
static constexpr uint32_t MACHO_MAGIC_32  = 0xFEEDFACE;
static constexpr uint32_t MACHO_CIGAM_32  = 0xCEFAEDFE;
static constexpr uint32_t MACHO_MAGIC_64  = 0xFEEDFACF;
static constexpr uint32_t MACHO_CIGAM_64  = 0xCFFAEDFE;
static constexpr uint32_t FAT_MAGIC       = 0xCAFEBABE;
static constexpr uint32_t FAT_CIGAM       = 0xBEBAFECA;

static bool is_macho_magic(uint32_t magic) {
    return magic == MACHO_MAGIC_32 || magic == MACHO_CIGAM_32 ||
           magic == MACHO_MAGIC_64 || magic == MACHO_CIGAM_64 ||
           magic == FAT_MAGIC || magic == FAT_CIGAM;
}

static std::string derive_module_name(const std::string& path) {
    std::string filename = fs::path(path).filename().string();
    if (filename.size() > 5 && filename.substr(filename.size() - 5) == ".node")
        filename = filename.substr(0, filename.size() - 5);
    fs::path p(path);
    auto parent = p.parent_path();
    if (parent.filename() == "Release" || parent.filename() == "Debug") {
        auto grandparent = parent.parent_path();
        if (grandparent.filename() == "build") {
            auto package_dir = grandparent.parent_path();
            if (!package_dir.filename().empty()) {
                std::string pkg = package_dir.filename().string();
                if (pkg != "node_modules") return pkg;
            }
        }
    }
    return filename;
}

std::vector<DiscoveredNativeModule> scan_native_modules(const std::string& app_dir) {
    std::vector<DiscoveredNativeModule> results;
    std::error_code ec;
    for (const auto& entry : fs::recursive_directory_iterator(app_dir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".node") continue;
        DiscoveredNativeModule mod;
        mod.path = entry.path().string();
        mod.module_name = derive_module_name(mod.path);
        std::ifstream file(mod.path, std::ios::binary);
        if (!file) continue;
        mod.magic = 0;
        file.read(reinterpret_cast<char*>(&mod.magic), sizeof(mod.magic));
        if (file.gcount() < 4) mod.magic = 0;
        if (mod.magic == ELF_MAGIC) {
            mod.arch = "elf";
        } else if (is_macho_magic(mod.magic)) {
            mod.arch = (mod.magic == MACHO_MAGIC_64 || mod.magic == MACHO_CIGAM_64 ||
                        mod.magic == FAT_MAGIC || mod.magic == FAT_CIGAM)
                           ? "macho-x86_64" : "macho-i386";
        } else {
            mod.arch = "unknown";
        }
        mod.node_api_version = 0;
        results.push_back(std::move(mod));
    }
    return results;
}

void classify_criticality(std::vector<DiscoveredNativeModule>& modules,
                           const compatdb::CompatibilityRecord& record) {
    for (auto& mod : modules) {
        for (const auto& cnm : record.critical_native_modules) {
            if (cnm.module == mod.module_name) { mod.is_critical = true; break; }
        }
    }
}

} // namespace platform::native
