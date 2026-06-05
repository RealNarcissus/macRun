// Stage 3b: Entitlement parsing
// Key/value pair association: finds each entitlement key and reads the
// value element immediately following it, avoiding global substring false positives.
// Architecture reference: docs/architecture/ARCHITECTURE_V6.md — "Stage 1 — Bundle Analysis"
#include "detector.hpp"
#include <string>
#include <string_view>

namespace platform {

// Find a <key> in XML and return the boolean value immediately following it.
// Scans from key position forward to find <true/> or <false/> in the next value element.
// Returns false if key not found or value is not a boolean true.
static bool get_key_bool(const std::string& xml, const std::string& key_name) {
    std::string search = "<key>" + key_name + "</key>";
    auto pos = xml.find(search);
    if (pos == std::string::npos) return false;

    pos += search.length();

    // Skip whitespace between </key> and <true/> or <false/>
    while (pos < xml.length() && (xml[pos] == ' ' || xml[pos] == '\n' || xml[pos] == '\r' || xml[pos] == '\t'))
        pos++;

    if (pos >= xml.length()) return false;

    return xml.compare(pos, 7, "<true/>") == 0;
}

// Find a key in XML and return true if merely present (for boolean entitlements
// where presence itself is significant, or for string-valued entitlements).
static bool has_key(const std::string& xml, const std::string& key_name) {
    std::string search = "<key>" + key_name + "</key>";
    return xml.find(search) != std::string::npos;
}

EntitlementInfo parse_entitlements(const std::string& entitlements_xml) {
    EntitlementInfo ent;
    if (entitlements_xml.empty()) return ent;

    ent.raw_entitlement_keys.clear();

    // Boolean entitlements: key/value association is critical.
    // Each get_key_bool call finds the specific key and checks the immediately
    // following <true/> or <false/> value — not a document-wide substring match.
    ent.sandboxed          = get_key_bool(entitlements_xml, "com.apple.security.app-sandbox");
    ent.uses_accessibility = has_key(entitlements_xml, "com.apple.private.accessibility") ||
                             has_key(entitlements_xml, "kTCCServiceAccessibility");
    ent.uses_hypervisor = has_key(entitlements_xml, "com.apple.security.hypervisor");
    ent.network_client   = get_key_bool(entitlements_xml, "com.apple.security.network.client");
    ent.network_server   = get_key_bool(entitlements_xml, "com.apple.security.network.server");
    ent.camera           = get_key_bool(entitlements_xml, "com.apple.security.device.camera");
    ent.microphone       = get_key_bool(entitlements_xml, "com.apple.security.device.microphone");
    ent.usb              = get_key_bool(entitlements_xml, "com.apple.security.device.usb");
    ent.bluetooth        = get_key_bool(entitlements_xml, "com.apple.security.device.bluetooth") ||
                           get_key_bool(entitlements_xml, "com.apple.security.device.bluetooth-le");
    ent.file_access_user_selected = get_key_bool(entitlements_xml, "com.apple.security.files.user-selected.read-write") ||
                                    get_key_bool(entitlements_xml, "com.apple.security.files.user-selected.read-only");
    ent.hardening_runtime = get_key_bool(entitlements_xml, "com.apple.security.cs.disable-library-validation") ||
                            get_key_bool(entitlements_xml, "com.apple.security.cs.allow-jit") ||
                            get_key_bool(entitlements_xml, "com.apple.security.cs.allow-unsigned-executable-memory") ||
                            get_key_bool(entitlements_xml, "com.apple.security.cs.allow-dyld-environment-variables");

    // Collect known entitlement keys present in the document
    const char* known_keys[] = {
        "com.apple.security.app-sandbox",
        "com.apple.security.network.client",
        "com.apple.security.network.server",
        "com.apple.security.device.camera",
        "com.apple.security.device.microphone",
        "com.apple.security.device.usb",
        "com.apple.security.device.bluetooth",
        "com.apple.security.files.user-selected.read-write",
        "com.apple.security.cs.disable-library-validation",
        "com.apple.security.cs.allow-jit",
        "com.apple.security.cs.allow-unsigned-executable-memory",
        "com.apple.security.cs.allow-dyld-environment-variables",
        "com.apple.security.hypervisor",
        "com.apple.private.accessibility",
        "com.apple.application-identifier",
        "com.apple.developer.team-identifier",
        "keychain-access-groups",
    };

    for (const auto* key : known_keys) {
        if (has_key(entitlements_xml, key)) {
            ent.raw_entitlement_keys.push_back(key);
        }
    }

    return ent;
}

} // namespace platform
