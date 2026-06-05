// macrun Input Resolution: DMG mounting, bundle extraction, format detection
// Architecture reference: docs/architecture/ARCHITECTURE_V6.md
//   — "macrun Contract": Accepts .app/.dmg/Mach-O binary
//   — Execution Pipeline diagram: DMG/App Extraction → Bundle Inspection
#include "macrun.hpp"
#include <sys/stat.h>
#include <filesystem>
#include <fstream>
#include <cstring>

namespace macrun {

namespace fs = std::filesystem;

static bool file_exists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

static bool is_directory(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

static bool ends_with(const std::string& s, const std::string& suffix) {
    if (s.length() < suffix.length()) return false;
    return s.compare(s.length() - suffix.length(), suffix.length(), suffix) == 0;
}

// Stage 1: Determine input format
static InputFormat detect_format(const std::string& path) {
    if (ends_with(path, ".dmg") || ends_with(path, ".DMG")) {
        return InputFormat::DMGImage;
    }

    if (ends_with(path, ".app") || ends_with(path, ".APP")) {
        if (is_directory(path) && file_exists(path + "/Contents/Info.plist")) {
            return InputFormat::AppBundle;
        }
    }

    // Check for directory with .app structure
    if (is_directory(path) && file_exists(path + "/Contents/Info.plist")) {
        return InputFormat::AppBundle;
    }

    // Check for raw Mach-O binary
    struct stat st;
    if (stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
        // Quick Mach-O magic check
        std::ifstream f(path, std::ios::binary);
        if (f) {
            uint32_t magic = 0;
            f.read(reinterpret_cast<char*>(&magic), sizeof(magic));
            if (magic == 0xfeedfacf || magic == 0xfeedface ||   // MH_MAGIC / MH_MAGIC_64
                magic == 0xcafebabe || magic == 0xcafebabf ||   // FAT_MAGIC / FAT_MAGIC_64
                magic == 0xcefaedfe || magic == 0xcffaedfe ||   // CIGAM variants
                magic == 0xbebafeca || magic == 0xbfbafeca) {
                return InputFormat::MachOBinary;
            }
        }
    }

    return InputFormat::Unknown;
}

// Stage 2: Resolve input to a usable .app bundle
static ResolvedInput resolve_input(const std::string& input_path) {
    ResolvedInput ri;
    ri.original_path = input_path;
    ri.format = detect_format(input_path);

    switch (ri.format) {
        case InputFormat::AppBundle:
            ri.extracted_bundle_path = input_path;
            ri.bundle_extraction_succeeded = true;
            // Find the executable
            {
                std::string plist = input_path + "/Contents/Info.plist";
                // Use a quick scan to find CFBundleExecutable name
                if (file_exists(plist)) {
                    std::ifstream f(plist);
                    std::string content((std::istreambuf_iterator<char>(f)),
                                         std::istreambuf_iterator<char>());
                    auto pos = content.find("<key>CFBundleExecutable</key>");
                    if (pos != std::string::npos) {
                        auto val = content.find("<string>", pos);
                        if (val != std::string::npos) {
                            auto end = content.find("</string>", val);
                            if (end != std::string::npos) {
                                std::string exe_name = content.substr(val + 8, end - val - 8);
                                ri.executable_path = input_path + "/Contents/MacOS/" + exe_name;
                            }
                        }
                    }
                }
            }
            break;

        case InputFormat::DMGImage:
            // DMG mounting is platform-specific and deferred to implementation.
            // This orchestrator records the intent; the actual mount tooling
            // (hdiutil-alternative, 7z, etc.) is handled by the host layer.
            ri.extraction_warnings.push_back(
                "DMG mounting not yet implemented — use extractor tool or provide .app path directly");
            ri.dmg_mount_succeeded = false;
            break;

        case InputFormat::MachOBinary:
            // Raw binary: wrap in a synthetic minimal bundle for detection
            ri.executable_path = input_path;
            ri.extraction_warnings.push_back(
                "Raw Mach-O binary provided — limited bundle metadata available");
            break;

        case InputFormat::Unknown:
            ri.extraction_warnings.push_back(
                "Unrecognized input format — provide .app, .dmg, or Mach-O binary");
            break;
    }

    return ri;
}

// ============================================================
// Exported input resolution for the orchestrator
// ============================================================
ResolvedInput resolve_input_for_orchestrator(const std::string& input_path) {
    return resolve_input(input_path);
}

} // namespace macrun

// Declare the resolve_input_for_orchestrator in the header implicitly via macrun.hpp
