// Stage 3: Mach-O Header Analysis and Architecture Detection
// Architecture reference: docs/architecture/ARCHITECTURE_V6.md — "Stage 3 — Architecture Analysis"
#include "detector.hpp"
#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>

namespace platform {

// Mach-O magic numbers
enum : uint32_t {
    MH_MAGIC      = 0xfeedface,   // 32-bit LE
    MH_CIGAM      = 0xcefaedfe,   // 32-bit BE
    MH_MAGIC_64   = 0xfeedfacf,   // 64-bit LE
    MH_CIGAM_64   = 0xcffaedfe,   // 64-bit BE
    FAT_MAGIC     = 0xcafebabe,   // Universal binary
    FAT_CIGAM     = 0xbebafeca,   // Universal binary BE
    FAT_MAGIC_64  = 0xcafebabf,   // Universal binary 64-bit
    FAT_CIGAM_64  = 0xbfbafeca,   // Universal binary 64-bit BE
};

// Mach-O file types
enum : uint32_t {
    MH_OBJECT    = 0x1,
    MH_EXECUTE   = 0x2,
    MH_DYLIB     = 0x6,
    MH_BUNDLE    = 0x8,
    MH_DYLINKER  = 0x7,
};

// Mach-O flags
enum : uint32_t {
    MH_PIE = 0x200000,
};

// Load commands
enum : uint32_t {
    LC_SEGMENT       = 0x1,
    LC_SEGMENT_64    = 0x19,
    LC_SYMTAB        = 0x2,
    LC_DYSYMTAB      = 0xb,
    LC_LOAD_DYLIB    = 0xc,
    LC_ID_DYLIB      = 0xd,
    LC_LOAD_WEAK_DYLIB = 0x80000018,
    LC_RPATH         = 0x8000001c,
    LC_VERSION_MIN_MACOSX = 0x24,
    LC_BUILD_VERSION = 0x32,
};

// Fixed-size Mach-O header structures (for parsing)
struct mach_header_64 {
    uint32_t magic;
    uint32_t cputype;
    uint32_t cpusubtype;
    uint32_t filetype;
    uint32_t ncmds;
    uint32_t sizeofcmds;
    uint32_t flags;
    uint32_t reserved;
};

struct fat_header {
    uint32_t magic;
    uint32_t nfat_arch;
};

struct fat_arch {
    uint32_t cputype;
    uint32_t cpusubtype;
    uint32_t offset;
    uint32_t size;
    uint32_t align;
};

// CPU types
enum : uint32_t {
    CPU_TYPE_X86     = 7,
    CPU_TYPE_X86_64  = 0x01000007,
    CPU_TYPE_ARM     = 12,
    CPU_TYPE_ARM64   = 0x0100000c,
};

// Subtypes we care about
enum : uint32_t {
    CPU_SUBTYPE_X86_64_ALL = 3,
    CPU_SUBTYPE_ARM64_ALL  = 0,
    CPU_SUBTYPE_ARM64E     = 2,
};

// Load command base
struct load_command {
    uint32_t cmd;
    uint32_t cmdsize;
};

struct dylib_command {
    uint32_t cmd;
    uint32_t cmdsize;
    uint32_t name_offset;  // offset from start of dylib_command to string
    uint32_t timestamp;
    uint32_t current_version;
    uint32_t compatibility_version;
};

struct rpath_command {
    uint32_t cmd;
    uint32_t cmdsize;
    uint32_t path_offset;
};

struct version_min_command {
    uint32_t cmd;
    uint32_t cmdsize;
    uint32_t version;  // X.Y.Z encoded in nibbles
    uint32_t sdk;
};

static bool is_big_endian(uint32_t magic) {
    return magic == MH_CIGAM || magic == MH_CIGAM_64 ||
           magic == FAT_CIGAM || magic == FAT_CIGAM_64;
}

// Bounded string length — safe alternative to std::strlen on untrusted buffer data
static size_t bounded_strlen(const char* data, size_t max_len) {
    size_t i = 0;
    while (i < max_len && data[i] != '\0') i++;
    return i;
}

static BinaryArchitecture cpu_to_arch(uint32_t cputype, uint32_t cpusubtype) {
    if (cputype == CPU_TYPE_X86_64) return BinaryArchitecture::X86_64;
    if (cputype == CPU_TYPE_ARM64) return BinaryArchitecture::ARM64;
    if (cputype == CPU_TYPE_X86)   return BinaryArchitecture::X86_64; // treat 32-bit x86 as x86_64 for detection
    return BinaryArchitecture::Unknown;
}

static uint32_t read_uint32(const uint8_t* data, size_t offset, bool swap) {
    uint32_t val;
    std::memcpy(&val, data + offset, sizeof(val));
    if (swap) return __builtin_bswap32(val);
    return val;
}

static uint64_t read_uint64(const uint8_t* data, size_t offset, bool swap) {
    uint64_t val;
    std::memcpy(&val, data + offset, sizeof(val));
    if (swap) return __builtin_bswap64(val);
    return val;
}

static uint32_t read_uint32(const std::string& data, size_t offset, bool swap) {
    const uint8_t* buf = reinterpret_cast<const uint8_t*>(data.data());
    uint32_t val;
    std::memcpy(&val, buf + offset, sizeof(val));
    if (swap) return __builtin_bswap32(val);
    return val;
}

// Parse a single Mach-O slice
static void parse_macho_header(const uint8_t* data, size_t size, MachOInfo& info, bool is_fat_arch = false) {
    if (size < sizeof(mach_header_64)) return;

    uint32_t magic = read_uint32(data, 0, false);
    bool swap = is_big_endian(magic);
    bool is_64 = (magic == MH_MAGIC_64 || magic == MH_CIGAM_64);

    if (magic != MH_MAGIC && magic != MH_CIGAM &&
        magic != MH_MAGIC_64 && magic != MH_CIGAM_64) {
        return;
    }

    uint32_t cputype    = read_uint32(data, 4, swap);
    uint32_t cpusubtype = read_uint32(data, 8, swap);
    uint32_t filetype   = read_uint32(data, 12, swap);
    uint32_t ncmds      = read_uint32(data, 16, swap);
    uint32_t sizeofcmds = read_uint32(data, 20, swap);
    uint32_t flags      = read_uint32(data, 24, swap);

    BinaryArchitecture arch = cpu_to_arch(cputype, cpusubtype);
    if (!is_fat_arch) {
        info.primary_architecture = arch;
    }
    if (std::find(info.architectures_present.begin(), info.architectures_present.end(), arch)
        == info.architectures_present.end()) {
        info.architectures_present.push_back(arch);
    }

    info.is_executable = (filetype == MH_EXECUTE);
    info.is_dylib      = (filetype == MH_DYLIB);
    info.is_bundle     = (filetype == MH_BUNDLE);
    info.has_pie       = (flags & MH_PIE) != 0;

    // Parse load commands
    size_t offset = is_64 ? sizeof(mach_header_64) : 28; // 28 for 32-bit mach_header
    for (uint32_t i = 0; i < ncmds && offset + sizeof(load_command) < size; i++) {
        uint32_t cmd = read_uint32(data, offset, swap);
        uint32_t cmdsize = read_uint32(data, offset + 4, swap);
        if (cmdsize == 0) break;

        switch (cmd) {
            case LC_LOAD_DYLIB:
            case LC_LOAD_WEAK_DYLIB: {
                if (offset + sizeof(dylib_command) <= size) {
                    uint32_t name_off = read_uint32(data, offset + 8, swap);
                    size_t str_offset = offset + name_off;
                    if (str_offset < size) {
                        const char* name = reinterpret_cast<const char*>(data + str_offset);
                        size_t max_len = size - str_offset;
                        size_t len = bounded_strlen(name, max_len);
                        if (len > 0) info.linked_libraries.emplace_back(name, len);
                    }
                }
                break;
            }
            case LC_ID_DYLIB: {
                if (offset + sizeof(dylib_command) <= size) {
                    uint32_t name_off = read_uint32(data, offset + 8, swap);
                    size_t str_offset = offset + name_off;
                    if (str_offset < size) {
                        const char* name = reinterpret_cast<const char*>(data + str_offset);
                        size_t max_len = size - str_offset;
                        size_t len = bounded_strlen(name, max_len);
                        if (len > 0) info.linked_libraries.emplace_back(name, len);
                    }
                }
                break;
            }
            case LC_RPATH: {
                if (offset + sizeof(rpath_command) <= size) {
                    uint32_t path_off = read_uint32(data, offset + 8, swap);
                    size_t str_offset = offset + path_off;
                    if (str_offset < size) {
                        const char* path = reinterpret_cast<const char*>(data + str_offset);
                        size_t max_len = size - str_offset;
                        size_t len = bounded_strlen(path, max_len);
                        if (len > 0) info.rpaths.emplace_back(path, len);
                    }
                }
                break;
            }
            case LC_VERSION_MIN_MACOSX: {
                if (offset + sizeof(version_min_command) <= size) {
                    uint32_t ver = read_uint32(data, offset + 8, swap);
                    info.min_os_deployment = ver;
                }
                break;
            }
            case LC_BUILD_VERSION: {
                if (offset + 24 <= size) {
                    uint32_t platform = read_uint32(data, offset + 8, swap);  // 1 = macOS
                    uint32_t minos    = read_uint32(data, offset + 12, swap);
                    if (platform == 1) info.min_os_deployment = minos;
                }
                break;
            }
        }

        offset += cmdsize;
    }
}

// Parse a fat/universal binary and extract single-arch info
static void parse_fat_header(const uint8_t* data, size_t size, MachOInfo& info) {
    if (size < sizeof(fat_header)) return;

    uint32_t magic = read_uint32(data, 0, false);
    bool swap = is_big_endian(magic);

    if (magic != FAT_MAGIC && magic != FAT_CIGAM &&
        magic != FAT_MAGIC_64 && magic != FAT_CIGAM_64) {
        return;
    }

    uint32_t nfat_arch = read_uint32(data, 4, swap);

    // Parse ALL slices for linked libraries — fat binaries may have
    // different frameworks linked per architecture (e.g. SwiftUI only in arm64 slice).
    for (uint32_t i = 0; i < nfat_arch; i++) {
        size_t arch_offset = 8 + i * sizeof(fat_arch);
        if (arch_offset + sizeof(fat_arch) > size) break;

        uint32_t cputype    = read_uint32(data, arch_offset, swap);
        uint32_t cpusubtype = read_uint32(data, arch_offset + 4, swap);
        uint32_t slice_off  = read_uint32(data, arch_offset + 8, swap);
        uint32_t slice_size = read_uint32(data, arch_offset + 12, swap);

        BinaryArchitecture arch = cpu_to_arch(cputype, cpusubtype);
        if (std::find(info.architectures_present.begin(), info.architectures_present.end(), arch)
            == info.architectures_present.end()) {
            info.architectures_present.push_back(arch);
        }

        // Parse ALL slices for metadata (linked libraries, file type, etc.)
        // but only set primary_architecture from first slice
        if (slice_off + slice_size <= size) {
            if (i == 0) {
                parse_macho_header(data + slice_off, slice_size, info, true);
            } else {
                // Parse subsequent slices to collect linked libraries
                MachOInfo slice_info;
                parse_macho_header(data + slice_off, slice_size, slice_info, true);
                // Merge linked libraries (deduplicated)
                for (const auto& lib : slice_info.linked_libraries) {
                    if (std::find(info.linked_libraries.begin(), info.linked_libraries.end(), lib)
                        == info.linked_libraries.end()) {
                        info.linked_libraries.push_back(lib);
                    }
                }
                for (const auto& rp : slice_info.rpaths) {
                    if (std::find(info.rpaths.begin(), info.rpaths.end(), rp)
                        == info.rpaths.end()) {
                        info.rpaths.push_back(rp);
                    }
                }
            }
        }
    }

    if (info.architectures_present.size() > 1) {
        info.primary_architecture = BinaryArchitecture::Universal;
    }
}

MachOInfo analyze_macho_from_buffer(const uint8_t* data, size_t size) {
    MachOInfo info;

    if (size < 4) return info;

    uint32_t magic = read_uint32(data, 0, false);

    // Check for fat/universal binary first
    if (magic == FAT_MAGIC || magic == FAT_CIGAM ||
        magic == FAT_MAGIC_64 || magic == FAT_CIGAM_64) {
        parse_fat_header(data, size, info);
        return info;
    }

    // Single-arch binary
    parse_macho_header(data, size, info);
    return info;
}

MachOInfo analyze_macho(const std::string& executable_path) {
    MachOInfo info;

    std::ifstream f(executable_path, std::ios::binary | std::ios::ate);
    if (!f) return info;

    size_t file_size = f.tellg();
    f.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(file_size);
    f.read(reinterpret_cast<char*>(buffer.data()), file_size);
    f.close();

    return analyze_macho_from_buffer(buffer.data(), buffer.size());
}

} // namespace platform
