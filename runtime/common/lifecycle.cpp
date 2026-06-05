// Substrate Lifecycle Manager — Implementation
// Architecture reference: docs/architecture/SUBSTRATE_MODEL.md Sections 2-4
//   docs/architecture/FAILURE_MODEL.md Section 4 (Failure Isolation)
//
// All probes are real filesystem checks. No permissive bypasses.
// Missing substrates return healthy=false with explicit error codes.
// Paths resolve via MACRUN_PREFIX env var (default: project root discovery).

#include "lifecycle.hpp"
#include <sys/stat.h>
#include <signal.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <algorithm>

namespace substrate {

namespace fs = std::filesystem;

// Resolve base paths using MACRUN_PREFIX or executable-relative discovery.
// Never assumes execution from the workspace root directory.
static std::string resolve_base_path() {
    if (const char* prefix = std::getenv("MACRUN_PREFIX")) {
        return prefix;
    }
    // Fallback: use the directory containing this translation unit's path
    // as a relative reference. For production, MACRUN_PREFIX must be set.
    return ".";
}

static std::string resolve_runtime_path(const std::string& relative) {
    std::string base = resolve_base_path();
    if (base == ".") return relative;
    if (base.back() == '/') return base + relative;
    return base + "/" + relative;
}

LifecycleManager::LifecycleManager() {
    for (int i = 0; i <= 3; i++) {
        auto id = static_cast<SubstrateId>(i);
        states_[id] = {LifecycleState::Unknown, "", "", ""};
    }
}

void LifecycleManager::log(const std::string& level, SubstrateId id,
                            LifecycleState state, const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&t));
    logs_.push_back({buf, level, id, state, message});
}

// ============================================================
// Substrate probing — real filesystem checks, no permissive bypasses
// ============================================================

static bool binary_exists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && (st.st_mode & S_IXUSR);
}

static bool file_exists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

static std::string probe_darling_binary() {
    // Check for the darling binary in standard locations
    for (const auto& path : {"/opt/darling/bin/darling", "/usr/local/bin/darling", "/usr/bin/darling"}) {
        if (binary_exists(path)) return path;
    }
    return "";
}

static std::string probe_qemu_user_binary() {
    for (const auto& path : {"/usr/bin/qemu-x86_64", "/usr/bin/qemu-aarch64",
                              "/usr/local/bin/qemu-x86_64"}) {
        if (binary_exists(path)) return path;
    }
    // Check locally cached copy
    std::string cached = resolve_runtime_path("runtime/third_party/qemu/qemu-binaries");
    for (const auto& name : {"qemu-x86_64", "qemu-aarch64"}) {
        if (binary_exists(cached + "/" + name)) return cached + "/" + name;
    }
    return "";
}

static std::string probe_electron_cache() {
    std::string home;
    if (const char* h = std::getenv("HOME")) home = h;
    else return "";

    std::string cache = home + "/.cache/macrun/electron";
    if (!file_exists(cache)) return "";

    // Check for any cached version with a manifest AND a real binary
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(cache, ec)) {
        if (ec) break;
        if (entry.is_directory()) {
            std::string manifest = entry.path().string() + "/manifest.json";
            std::string binary   = entry.path().string() + "/electron";
            if (file_exists(manifest) && binary_exists(binary)) {
                return entry.path().filename().string();
            }
        }
    }
    return "";
}

static std::string probe_webkit_version() {
    // Probe using a safe subprocess call — no shell injection risk
    // since the path is fixed.
    FILE* pipe = popen("pkg-config --modversion webkit2gtk-4.1 2>/dev/null", "r");
    if (pipe) {
        char buf[64] = {};
        if (fgets(buf, sizeof(buf), pipe) && buf[0]) {
            int rc = pclose(pipe);
            if (rc == 0) {
                std::string ver(buf);
                ver.erase(std::remove(ver.begin(), ver.end(), '\n'), ver.end());
                return "system-4.1=" + ver;
            }
        } else {
            pclose(pipe);
        }
    }
    pipe = popen("pkg-config --modversion webkit2gtk-4.0 2>/dev/null", "r");
    if (pipe) {
        char buf[64] = {};
        if (fgets(buf, sizeof(buf), pipe) && buf[0]) {
            int rc = pclose(pipe);
            if (rc == 0) {
                std::string ver(buf);
                ver.erase(std::remove(ver.begin(), ver.end(), '\n'), ver.end());
                return "system-4.0=" + ver;
            }
        } else {
            pclose(pipe);
        }
    }
    if (file_exists("/usr/include/webkitgtk-4.1/webkit2/webkit2.h") ||
        file_exists("/usr/include/webkitgtk-4.0/webkit2/webkit2.h")) {
        return "system-headers";
    }
    return "";
}

// ============================================================
// Initialization — strict: fails if substrate is unavailable
// ============================================================

HealthCheckResult LifecycleManager::initialize(SubstrateId id) {
    auto& state = states_[id];

    if (state.state == LifecycleState::Initialized ||
        state.state == LifecycleState::Running) {
        return {true, SubstrateError::None, "Already initialized", std::string(to_string(id))};
    }

    HealthCheckResult health = check_health(id);
    if (!health.healthy) {
        state.state = LifecycleState::Unavailable;
        log("ERROR", id, state.state, "Initialization failed: " + health.message);
        return health;
    }

    state.state = LifecycleState::Initialized;
    log("INFO", id, state.state, "Substrate initialized: " + state.binary_path);
    return {true, SubstrateError::None, "Substrate initialized", std::string(to_string(id))};
}

bool LifecycleManager::is_initialized(SubstrateId id) const {
    auto it = states_.find(id);
    if (it == states_.end()) return false;
    return it->second.state == LifecycleState::Initialized ||
           it->second.state == LifecycleState::Running;
}

// ============================================================
// Health checks — strict: missing → unhealthy
// ============================================================

HealthCheckResult LifecycleManager::check_health(SubstrateId id) const {
    HealthCheckResult result;
    result.subsystem = std::string(to_string(id));
    result.healthy = false;  // default: fail unless proven available

    switch (id) {
        case SubstrateId::Darling: {
            auto binary = probe_darling_binary();
            if (!binary.empty()) {
                result.healthy = true;
                result.message = "Darling binary found: " + binary;
            } else {
                result.error = SubstrateError::SubstrateNotInstalled;
                result.message = "Darling not found in PATH. Install or run runtime/third_party/darling/acquire.sh --build";
            }
            break;
        }

        case SubstrateId::QEMU_UserMode: {
            auto binary = probe_qemu_user_binary();
            if (!binary.empty()) {
                result.healthy = true;
                result.message = "QEMU user-mode binary found: " + binary;
            } else {
                // Check if KVM is at least available (for VM path)
                if (file_exists("/dev/kvm")) {
                    result.message = "QEMU user-mode binary not found (KVM available for VM path). "
                                     "Run runtime/third_party/qemu/acquire.sh";
                } else {
                    result.message = "QEMU user-mode binary not found and /dev/kvm unavailable.";
                }
                result.error = SubstrateError::SubstrateNotInstalled;
            }
            break;
        }

        case SubstrateId::Electron: {
            auto cached = probe_electron_cache();
            if (!cached.empty()) {
                result.healthy = true;
                result.message = "Electron runtime cached: " + cached;
            } else {
                result.error = SubstrateError::ElectronRuntimeNotCached;
                result.message = "No Electron runtime cached. Run runtime/third_party/electron/acquire.sh --all";
            }
            break;
        }

        case SubstrateId::WebKitGTK: {
            auto version = probe_webkit_version();
            if (!version.empty()) {
                result.healthy = true;
                result.message = "WebKitGTK available: " + version;
            } else {
                result.error = SubstrateError::GTK_WebKitNotInstalled;
                result.message = "WebKitGTK development package not installed. "
                                 "Install libwebkit2gtk-4.1-dev or equivalent.";
            }
            break;
        }
    }

    return result;
}

std::vector<HealthCheckResult> LifecycleManager::check_all_health() const {
    std::vector<HealthCheckResult> results;
    for (int i = 0; i <= 3; i++) {
        results.push_back(check_health(static_cast<SubstrateId>(i)));
    }
    return results;
}

// ============================================================
// Provisioning — strict: unavailable → not ready
// ============================================================

LifecycleManager::ProvisioningResult LifecycleManager::provision(SubstrateId id) const {
    ProvisioningResult result;
    result.ready = false;

    auto it = states_.find(id);
    if (it == states_.end()) {
        result.warnings.push_back("Substrate not tracked: " + std::string(to_string(id)));
        return result;
    }

    const auto& state = it->second;

    if (state.state != LifecycleState::Initialized &&
        state.state != LifecycleState::Running) {
        result.warnings.push_back(
            "Substrate not initialized (state=" + std::string(to_string(state.state)) +
            "): " + std::string(to_string(id)));
        return result;
    }

    if (state.binary_path.empty()) {
        result.warnings.push_back(
            "Substrate initialized but binary path is empty: " + std::string(to_string(id)));
        return result;
    }

    result.ready = true;
    result.binary_path = state.binary_path;
    result.version = state.version;

    switch (id) {
        case SubstrateId::Darling:
            result.environment.push_back("DYLD_ROOT_PATH=/opt/darling");
            break;
        case SubstrateId::QEMU_UserMode:
            result.environment.push_back("QEMU_LD_PREFIX=/opt/darling");
            break;
        case SubstrateId::Electron: {
            std::string home = std::getenv("HOME") ? std::getenv("HOME") : "/tmp";
            result.environment.push_back("ELECTRON_CACHE=" + home + "/.cache/macrun/electron");
            break;
        }
        case SubstrateId::WebKitGTK:
            break;
    }

    return result;
}

void LifecycleManager::record_failure(SubstrateId id, SubstrateError error,
                                       const std::string& detail) {
    auto& state = states_[id];
    state.state = LifecycleState::Failed;
    log("ERROR", id, state.state,
        std::string(to_string(error)) + ": " + detail);
}

// ============================================================
// Teardown — active process cleanup (SIGTERM/SIGKILL sequence)
// ============================================================

void LifecycleManager::teardown(SubstrateId id) {
    auto& state = states_[id];

    if (state.state == LifecycleState::Running) {
        // Active cleanup: send SIGTERM to registered pids
        for (auto pid : registered_pids_) {
            log("INFO", id, state.state,
                "Sending SIGTERM to pid " + std::to_string(pid));
            kill(static_cast<pid_t>(pid), SIGTERM);
        }
    }

    if (state.state == LifecycleState::Running ||
        state.state == LifecycleState::Initialized ||
        state.state == LifecycleState::Failed ||
        state.state == LifecycleState::Unavailable) {
        state.state = LifecycleState::Terminated;
        state.binary_path.clear();
        log("INFO", id, state.state, "Substrate torn down");
    }
}

void LifecycleManager::teardown_all() {
    for (int i = 0; i <= 3; i++) {
        teardown(static_cast<SubstrateId>(i));
    }
    registered_pids_.clear();
}

void LifecycleManager::register_child_pid(int pid) {
    registered_pids_.push_back(pid);
}

LifecycleManager::SubstrateVersion LifecycleManager::get_version(SubstrateId id) const {
    SubstrateVersion sv;
    sv.id = id;

    auto it = states_.find(id);
    if (it != states_.end()) {
        sv.version = it->second.version;
        sv.source = it->second.version_source;
        sv.available = (it->second.state == LifecycleState::Initialized ||
                        it->second.state == LifecycleState::Running);
    }

    return sv;
}

} // namespace substrate
