// Deterministic test fixture generator — creates minimal Mach-O binaries
// for testing the detection pipeline without requiring real macOS executables.
//
// Generates valid Mach-O headers that the parser can detect.
// Architecture reference: docs/architecture/ARCHITECTURE_V6.md — "Stage 3 — Architecture Analysis"

#include <cstdint>
#include <cstring>
#include <string>
#include <fstream>
#include <vector>
#include <iostream>
#include <cstdlib>

// Mach-O header structures
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

struct load_command {
    uint32_t cmd;
    uint32_t cmdsize;
};

struct dylib_command {
    uint32_t cmd;
    uint32_t cmdsize;
    uint32_t name_offset;
    uint32_t timestamp;
    uint32_t current_version;
    uint32_t compatibility_version;
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

enum : uint32_t {
    MH_MAGIC_64     = 0xfeedfacf,
    MH_EXECUTE      = 0x2,
    MH_PIE          = 0x200000,
    LC_LOAD_DYLIB   = 0xc,
    LC_BUILD_VERSION = 0x32,
    CPU_TYPE_X86_64 = 0x01000007,
    CPU_TYPE_ARM64  = 0x0100000c,
    CPU_SUBTYPE_X86_64_ALL = 3,
    CPU_SUBTYPE_ARM64_ALL  = 0,
    FAT_MAGIC       = 0xcafebabe,
};

static void write_dylib_command(std::vector<uint8_t>& buf, uint32_t& offset, const std::string& path) {
    uint32_t cmd_start = offset;
    dylib_command cmd{};
    cmd.cmd = LC_LOAD_DYLIB;
    cmd.name_offset = sizeof(dylib_command);
    cmd.cmdsize = cmd.name_offset + static_cast<uint32_t>(path.length() + 1);
    // align cmdsize to 8
    cmd.cmdsize = (cmd.cmdsize + 7) & ~7;

    buf.resize(buf.size() + cmd.cmdsize);
    std::memcpy(buf.data() + cmd_start, &cmd, sizeof(dylib_command));
    std::memcpy(buf.data() + cmd_start + sizeof(dylib_command), path.c_str(), path.length() + 1);
    // pad to alignment
    for (uint32_t i = sizeof(dylib_command) + path.length() + 1; i < cmd.cmdsize; i++) {
        buf[cmd_start + i] = 0;
    }
    offset += cmd.cmdsize;
}

static void write_macho(const std::string& path, uint32_t cputype, uint32_t cpusubtype,
                        const std::vector<std::string>& linked_libs) {
    std::vector<uint8_t> buf;

    // Reserve space for header + load commands
    uint32_t lc_start = sizeof(mach_header_64);

    // Calculate load commands size
    std::vector<uint8_t> lc_buf;
    uint32_t lc_offset = 0;
    for (const auto& lib : linked_libs) {
        write_dylib_command(lc_buf, lc_offset, lib);
    }

    mach_header_64 header{};
    header.magic = MH_MAGIC_64;
    header.cputype = cputype;
    header.cpusubtype = cpusubtype;
    header.filetype = MH_EXECUTE;
    header.flags = MH_PIE;
    header.ncmds = static_cast<uint32_t>(linked_libs.size());
    header.sizeofcmds = lc_offset;

    buf.resize(sizeof(mach_header_64) + lc_offset);
    std::memcpy(buf.data(), &header, sizeof(mach_header_64));
    if (!lc_buf.empty()) {
        std::memcpy(buf.data() + sizeof(mach_header_64), lc_buf.data(), lc_offset);
    }

    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(buf.data()), buf.size());
    f.close();
}

static void write_fat_macho(const std::string& path) {
    // Generate two slices: x86_64 and arm64
    std::vector<uint8_t> x86_buf, arm64_buf;

    // x86_64 slice with some x86-specific libs
    {
        std::vector<uint8_t> lc_buf;
        uint32_t lc_offset = 0;
        write_dylib_command(lc_buf, lc_offset, "/System/Library/Frameworks/AppKit.framework/Versions/C/AppKit");

        mach_header_64 hdr{};
        hdr.magic = MH_MAGIC_64;
        hdr.cputype = CPU_TYPE_X86_64;
        hdr.cpusubtype = CPU_SUBTYPE_X86_64_ALL;
        hdr.filetype = MH_EXECUTE;
        hdr.flags = MH_PIE;
        hdr.ncmds = 1;
        hdr.sizeofcmds = lc_offset;

        x86_buf.resize(sizeof(mach_header_64) + lc_offset);
        std::memcpy(x86_buf.data(), &hdr, sizeof(mach_header_64));
        std::memcpy(x86_buf.data() + sizeof(mach_header_64), lc_buf.data(), lc_offset);
    }

    // arm64 slice with some arm64-specific libs
    {
        std::vector<uint8_t> lc_buf;
        uint32_t lc_offset = 0;
        write_dylib_command(lc_buf, lc_offset, "/System/Library/Frameworks/AppKit.framework/Versions/C/AppKit");
        write_dylib_command(lc_buf, lc_offset, "/System/Library/Frameworks/SwiftUI.framework/Versions/A/SwiftUI");

        mach_header_64 hdr{};
        hdr.magic = MH_MAGIC_64;
        hdr.cputype = CPU_TYPE_ARM64;
        hdr.cpusubtype = CPU_SUBTYPE_ARM64_ALL;
        hdr.filetype = MH_EXECUTE;
        hdr.flags = MH_PIE;
        hdr.ncmds = 2;
        hdr.sizeofcmds = lc_offset;

        arm64_buf.resize(sizeof(mach_header_64) + lc_offset);
        std::memcpy(arm64_buf.data(), &hdr, sizeof(mach_header_64));
        std::memcpy(arm64_buf.data() + sizeof(mach_header_64), lc_buf.data(), lc_offset);
    }

    // Build fat binary
    uint32_t fat_size = sizeof(fat_header) + 2 * sizeof(fat_arch);
    uint32_t x86_offset = fat_size;
    uint32_t arm64_offset = fat_size + static_cast<uint32_t>(x86_buf.size());
    // align arm64 to 16384 boundary (power of 2 alignment for Mach-O slices)
    arm64_offset = (arm64_offset + 16383) & ~16383u;

    fat_header fhdr{};
    fhdr.magic = FAT_MAGIC;
    fhdr.nfat_arch = 2;

    fat_arch x86_arch{};
    x86_arch.cputype = CPU_TYPE_X86_64;
    x86_arch.cpusubtype = CPU_SUBTYPE_X86_64_ALL;
    x86_arch.offset = x86_offset;
    x86_arch.size = static_cast<uint32_t>(x86_buf.size());
    x86_arch.align = 14; // 2^14 = 16384

    fat_arch arm64_arch{};
    arm64_arch.cputype = CPU_TYPE_ARM64;
    arm64_arch.cpusubtype = CPU_SUBTYPE_ARM64_ALL;
    arm64_arch.offset = arm64_offset;
    arm64_arch.size = static_cast<uint32_t>(arm64_buf.size());
    arm64_arch.align = 14;

    std::vector<uint8_t> fat_buf(arm64_offset + arm64_buf.size(), 0);
    std::memcpy(fat_buf.data(), &fhdr, sizeof(fhdr));
    std::memcpy(fat_buf.data() + sizeof(fhdr), &x86_arch, sizeof(fat_arch));
    std::memcpy(fat_buf.data() + sizeof(fhdr) + sizeof(fat_arch), &arm64_arch, sizeof(fat_arch));
    std::memcpy(fat_buf.data() + x86_offset, x86_buf.data(), x86_buf.size());
    std::memcpy(fat_buf.data() + arm64_offset, arm64_buf.data(), arm64_buf.size());

    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(fat_buf.data()), fat_buf.size());
    f.close();
}

int main() {
    std::string cwd = "tests/fixtures";

    // Electron app: x86_64 binary linked against Electron Framework
    write_macho(cwd + "/electron-app.app/Contents/MacOS/Cursor", CPU_TYPE_X86_64, CPU_SUBTYPE_X86_64_ALL,
        {
            "@rpath/Electron Framework.framework/Versions/A/Electron Framework",
            "/System/Library/Frameworks/AppKit.framework/Versions/C/AppKit",
            "/usr/lib/libSystem.B.dylib",
        });

    // Cocoa app: x86_64 binary linked against AppKit
    write_macho(cwd + "/cocoa-app.app/Contents/MacOS/TextEdit", CPU_TYPE_X86_64, CPU_SUBTYPE_X86_64_ALL,
        {
            "/System/Library/Frameworks/AppKit.framework/Versions/C/AppKit",
            "/System/Library/Frameworks/CoreData.framework/Versions/A/CoreData",
            "/usr/lib/libSystem.B.dylib",
        });

    // SwiftUI app: universal binary with SwiftUI linkage
    write_fat_macho(cwd + "/swiftui-app.app/Contents/MacOS/Raycast");

    // Hypervisor app: x86_64 binary linked against Hypervisor
    write_macho(cwd + "/hypervisor-app.app/Contents/MacOS/UTM", CPU_TYPE_X86_64, CPU_SUBTYPE_X86_64_ALL,
        {
            "/System/Library/Frameworks/Hypervisor.framework/Versions/A/Hypervisor",
            "/System/Library/Frameworks/Cocoa.framework/Versions/A/Cocoa",
            "/usr/lib/libSystem.B.dylib",
        });

    std::cout << "Generated 4 test fixtures.\n";
    return 0;
}
