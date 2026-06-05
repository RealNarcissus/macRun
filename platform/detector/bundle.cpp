// Stage 1: Bundle Analysis — Info.plist parser
// Architecture reference: docs/architecture/ARCHITECTURE_V6.md — "Stage 1 — Bundle Analysis"
//
// Supports both XML plists and binary plists (bplist00).
// Binary plist parsing is limited to ASCII string extraction from the data buffer
// since a full bplist parser requires Apple's CoreFoundation. Detected bplists
// emit a diagnostic for partial metadata, avoiding false-negative classifications.
#include "detector.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <cstdint>
#include <sys/stat.h>

namespace platform {

static std::string read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static bool file_exists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

// ============================================================
// Binary plist detection and limited extraction
// ============================================================

// bplist00 magic: 6 bytes at start of file
static bool is_binary_plist(const std::string& data) {
    return data.size() >= 6 && data.compare(0, 8, "bplist00") == 0;
}

// Extract well-known string values by scanning the binary plist buffer
// for recognizable key patterns followed by string data.
// This is a best-effort approach for compiled bundles on Linux where
// CoreFoundation/plutil aren't available.
//
// Strategy: for each known key, search for the literal ASCII key string
// in the buffer, then scan forward for the next ASCII string that looks
// like a value. This works because bplist00 stores most keys/values
// as embedded ASCII strings when possible.

static std::string bplist_extract_string(const std::string& data, const std::string& key) {
    // Find the key literal in the binary data
    auto pos = data.find(key);
    if (pos == std::string::npos) return {};

    // Skip past the key bytes
    pos += key.length();

    // Skip non-printable control bytes that bplist00 may insert between key and value
    size_t max_scan = std::min(pos + 256, data.size());
    while (pos < max_scan && (static_cast<unsigned char>(data[pos]) < 0x20 ||
                              static_cast<unsigned char>(data[pos]) > 0x7e)) {
        pos++;
    }

    // Collect the next run of printable ASCII characters,
    // stopping at: non-printable, next key-like boundary, or reasonable max length
    size_t start = pos;
    while (pos < max_scan &&
           static_cast<unsigned char>(data[pos]) >= 0x20 &&
           static_cast<unsigned char>(data[pos]) <= 0x7e &&
           pos - start < 128) {

        // Stop if we encounter what looks like the start of another Cocoa key
        if (pos > start && data[pos] == 'C' && pos + 3 < max_scan) {
            if (data.compare(pos, 9, "CFBundle") == 0) break;
        }
        if (pos > start && data[pos] == 'N' && pos + 2 < max_scan) {
            if (data.compare(pos, 2, "NS") == 0 && std::isupper(static_cast<unsigned char>(data[pos + 2]))) break;
        }
        if (pos > start && data[pos] == 'L' && pos + 2 < max_scan) {
            if (data.compare(pos, 2, "LS") == 0 && std::isupper(static_cast<unsigned char>(data[pos + 2]))) break;
        }
        pos++;
    }

    if (pos > start) {
        return data.substr(start, pos - start);
    }

    return {};
}

// ============================================================
// XML plist parser
// ============================================================

static std::string extract_string_value(const std::string& xml, const std::string& key) {
    std::string search = "<key>" + key + "</key>";
    auto pos = xml.find(search);
    if (pos == std::string::npos) return {};

    pos += search.length();

    while (pos < xml.length() && (xml[pos] == ' ' || xml[pos] == '\n' || xml[pos] == '\r' || xml[pos] == '\t'))
        pos++;

    if (pos >= xml.length()) return {};

    if (xml.compare(pos, 8, "<string>") == 0) {
        pos += 8;
        auto end = xml.find("</string>", pos);
        if (end == std::string::npos) return {};
        return xml.substr(pos, end - pos);
    }

    if (xml.compare(pos, 7, "<true/>") == 0) return "true";
    if (xml.compare(pos, 8, "<false/>") == 0) return "false";

    if (xml.compare(pos, 9, "<integer>") == 0) {
        pos += 9;
        auto end = xml.find("</integer>", pos);
        if (end == std::string::npos) return {};
        return xml.substr(pos, end - pos);
    }

    if (xml.compare(pos, 6, "<real>") == 0) {
        pos += 6;
        auto end = xml.find("</real>", pos);
        if (end == std::string::npos) return {};
        return xml.substr(pos, end - pos);
    }

    return {};
}

static std::vector<std::string> extract_string_array(const std::string& xml, const std::string& key) {
    std::vector<std::string> result;
    std::string search = "<key>" + key + "</key>";
    auto pos = xml.find(search);
    if (pos == std::string::npos) return result;

    pos += search.length();
    while (pos < xml.length() && (xml[pos] == ' ' || xml[pos] == '\n' || xml[pos] == '\r' || xml[pos] == '\t'))
        pos++;

    if (pos >= xml.length() || xml.compare(pos, 6, "<array>") != 0) return result;

    pos += 7;
    while (true) {
        auto string_start = xml.find("<string>", pos);
        auto array_end = xml.find("</array>", pos);
        if (string_start == std::string::npos || (array_end != std::string::npos && array_end < string_start))
            break;
        string_start += 8;
        auto string_end = xml.find("</string>", string_start);
        if (string_end == std::string::npos) break;
        result.push_back(xml.substr(string_start, string_end - string_start));
        pos = string_end + 9;
    }

    return result;
}

BundleInfo analyze_bundle(const std::string& app_bundle_path) {
    BundleInfo info;
    std::string plist_path = app_bundle_path + "/Contents/Info.plist";

    if (!file_exists(plist_path)) {
        return info;
    }

    std::string data = read_file(plist_path);
    if (data.empty()) return info;

    if (is_binary_plist(data)) {
        // Binary plist: best-effort key extraction from embedded ASCII strings.
        // The extracted values may be incomplete compared to a full property list parser.
        info.custom_properties["_plist_format"] = "binary (bplist00) — best-effort extraction";

        info.bundle_identifier      = bplist_extract_string(data, "CFBundleIdentifier");
        info.bundle_name            = bplist_extract_string(data, "CFBundleDisplayName");
        if (info.bundle_name.empty())
            info.bundle_name        = bplist_extract_string(data, "CFBundleName");
        info.bundle_version         = bplist_extract_string(data, "CFBundleVersion");
        info.bundle_short_version   = bplist_extract_string(data, "CFBundleShortVersionString");
        info.executable_name        = bplist_extract_string(data, "CFBundleExecutable");
        info.bundle_type            = bplist_extract_string(data, "CFBundlePackageType");
        info.minimum_os_version     = bplist_extract_string(data, "LSMinimumSystemVersion");
        if (info.minimum_os_version.empty())
            info.minimum_os_version = bplist_extract_string(data, "MinimumOSVersion");

        // NSPrincipalClass / NSMainNibFile — key presence check
        info.has_ns_principal_class = data.find("NSPrincipalClass") != std::string::npos;
        info.has_ns_main_nib_file   = data.find("NSMainNibFile") != std::string::npos;

        // Emit diagnostic that plist was binary
        info.custom_properties["_bplist_diagnostic"] =
            "Binary plist detected. Metadata extracted via best-effort string scanning. "
            "Some values may be missing or incomplete. Consider pre-converting with plutil on macOS.";

        return info;
    }

    // XML plist path: full structured parsing
    info.bundle_identifier      = extract_string_value(data, "CFBundleIdentifier");
    info.bundle_name            = extract_string_value(data, "CFBundleDisplayName");
    if (info.bundle_name.empty())
        info.bundle_name        = extract_string_value(data, "CFBundleName");
    info.bundle_version         = extract_string_value(data, "CFBundleVersion");
    info.bundle_short_version   = extract_string_value(data, "CFBundleShortVersionString");
    info.executable_name        = extract_string_value(data, "CFBundleExecutable");
    info.bundle_type            = extract_string_value(data, "CFBundlePackageType");
    info.minimum_os_version     = extract_string_value(data, "LSMinimumSystemVersion");
    if (info.minimum_os_version.empty())
        info.minimum_os_version = extract_string_value(data, "MinimumOSVersion");
    info.sdk_version            = extract_string_value(data, "DTSDKName");

    info.supported_platforms    = extract_string_array(data, "CFBundleSupportedPlatforms");
    info.required_capabilities  = extract_string_array(data, "UIRequiredDeviceCapabilities");

    std::string principal = extract_string_value(data, "NSPrincipalClass");
    info.has_ns_principal_class = !principal.empty();

    std::string nib_file = extract_string_value(data, "NSMainNibFile");
    info.has_ns_main_nib_file = !nib_file.empty();

    std::vector<std::string> interesting_keys = {
        "CFBundleDocumentTypes",
        "CFBundleURLTypes",
        "NSSupportsAutomaticGraphicsSwitching",
        "LSBackgroundOnly",
        "LSUIElement",
        "NSHighResolutionCapable",
    };
    for (const auto& k : interesting_keys) {
        auto v = extract_string_value(data, k);
        if (!v.empty()) info.custom_properties[k] = v;
    }

    return info;
}

} // namespace platform
