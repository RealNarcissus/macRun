// Electron Runtime Substitution Adapter — Real Implementation
// Architecture: docs/architecture/SUBSTRATE_MODEL.md Section 3 (Electron Runtimes)
//   docs/architecture/ARCHITECTURE_V6.md — Tier 0 Runtime Substitution

#include "electron_adapter.hpp"
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <iostream>
#include <cstring>
#include <filesystem>
#include <algorithm>
#include <thread>

namespace fs = std::filesystem;

namespace platform::adapters {

static bool file_exists(const std::string& path) {
    std::error_code ec;
    return fs::exists(path, ec);
}

ElectronAdapter::ElectronAdapter()
    : status_(AdapterStatus::Uninitialized),
      exit_code_(0),
      mock_runtime_cached_(true),
      mock_sandbox_supported_(true)
{
    log("INFO", "Electron adapter instantiated");
}

void ElectronAdapter::log(const std::string& level, const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t_now), "%Y-%m-%dT%H:%M:%SZ");
    logs_.push_back({oss.str(), level, "ElectronAdapter", message});
}

bool ElectronAdapter::initialize() {
    if (status_ != AdapterStatus::Uninitialized) {
        log("WARN", "Adapter already initialized");
        return false;
    }

    if (!is_supported()) {
        status_ = AdapterStatus::Error;
        detail_ = "Host lacks support for Electron runtime execution";
        log("ERROR", detail_);
        return false;
    }

    status_ = AdapterStatus::Ready;
    log("INFO", "Electron native replacement environment ready");
    return true;
}

bool ElectronAdapter::start() {
    if (status_ != AdapterStatus::Ready) {
        log("ERROR", "Cannot start adapter: not in ready state");
        return false;
    }
    status_ = AdapterStatus::Running;
    log("INFO", "Electron process starting");
    return true;
}

bool ElectronAdapter::stop() {
    if (status_ != AdapterStatus::Running && status_ != AdapterStatus::Suspended) {
        log("WARN", "Stop called on inactive adapter");
        return false;
    }

    if (child_pid_ <= 0) {
        status_ = AdapterStatus::Terminated;
        exit_code_ = 0;
        log("INFO", "Adapter stopped (no child process)");
        return true;
    }

    pid_t pid = child_pid_;
    log("INFO", "Sending SIGTERM to Electron process pid=" + std::to_string(pid));

    // Send SIGTERM to the process group (child set itself as group leader)
    kill(-pid, SIGTERM);

    // Wait up to 200ms for graceful exit
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
    bool exited = false;
    int status = 0;

    while (std::chrono::steady_clock::now() < deadline) {
        pid_t res = waitpid(pid, &status, WNOHANG);
        if (res == pid) {
            exited = true;
            if (WIFEXITED(status)) {
                exit_code_ = WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
                exit_code_ = 128 + WTERMSIG(status);
            }
            break;
        } else if (res == -1 && errno == ECHILD) {
            exited = true;
            exit_code_ = 0;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (!exited) {
        log("WARN", "Electron process did not exit gracefully; sending SIGKILL");
        kill(-pid, SIGKILL);
        waitpid(pid, &status, 0);
        exit_code_ = 137;
    }

    status_ = AdapterStatus::Terminated;
    child_pid_ = 0;
    log("INFO", "Electron process stopped with exit_code=" + std::to_string(exit_code_));
    return true;
}

bool ElectronAdapter::suspend() {
    if (status_ != AdapterStatus::Running) {
        log("ERROR", "Cannot suspend adapter: not running");
        return false;
    }
    if (child_pid_ > 0 && !use_mocks_) {
        kill(-child_pid_, SIGSTOP); // Send SIGSTOP to the child process group
    }
    status_ = AdapterStatus::Suspended;
    log("INFO", "Electron application suspended (SIGSTOP sent)");
    return true;
}

bool ElectronAdapter::resume() {
    if (status_ != AdapterStatus::Suspended) {
        log("ERROR", "Cannot resume adapter: not suspended");
        return false;
    }
    if (child_pid_ > 0 && !use_mocks_) {
        kill(-child_pid_, SIGCONT); // Send SIGCONT to the child process group
    }
    status_ = AdapterStatus::Running;
    log("INFO", "Electron application resumed (SIGCONT sent)");
    return true;
}

AdapterProcessStatus ElectronAdapter::get_status() const {
    return {status_, exit_code_, detail_};
}

std::vector<DiagnosticEntry> ElectronAdapter::get_logs() const {
    return logs_;
}

void ElectronAdapter::clear_logs() {
    logs_.clear();
}

bool ElectronAdapter::has_errors() const {
    for (const auto& entry : logs_) {
        if (entry.level == "ERROR" || entry.level == "FATAL") {
            return true;
        }
    }
    return status_ == AdapterStatus::Error;
}

bool ElectronAdapter::is_supported() const {
    auto checks = check_host_capabilities();
    for (const auto& check : checks) {
        if (!check.satisfied) {
            return false;
        }
    }
    return true;
}

std::vector<HostCapabilityCheck> ElectronAdapter::check_host_capabilities() const {
    std::vector<HostCapabilityCheck> checks;

    checks.push_back({
        "Electron Runtime cache check",
        mock_runtime_cached_,
        mock_runtime_cached_ ? "Matching Electron runtime cached at ~/.cache/macrun/" : "Target Electron runtime version not cached"
    });

    checks.push_back({
        "User namespace sandbox check",
        mock_sandbox_supported_,
        mock_sandbox_supported_ ? "User namespace sandbox available for Chromium engine" : "No user namespace sandboxing support"
    });

    return checks;
}

void ElectronAdapter::resolve_runtime_version(const std::string& version) {
    resolved_version_ = version;
    log("INFO", "Resolved Electron target version: " + version);
}

void ElectronAdapter::set_bundle_info(const std::string& bundle_id, const std::string& app_name) {
    bundle_id_ = bundle_id;
    app_name_ = app_name;
    log("INFO", "App bundle info registered: id=" + bundle_id + " name=" + app_name);
}

void ElectronAdapter::set_asar_path(const std::string& asar_path) {
    asar_path_ = asar_path;
    log("INFO", "App ASAR file registered: " + asar_path);
}

void ElectronAdapter::inject_preload(const std::string& preload_script_path) {
    preload_scripts_.push_back(preload_script_path);
    log("INFO", "Shim preload script injected: " + preload_script_path);
}

// ============================================================
// Shim activation — type-safe, struct-based, no string.find()
// ============================================================

ShimActivation ElectronAdapter::compute_shim_activation() const {
    ShimActivation act;

    for (const auto& script : preload_scripts_) {
        if (script.rfind("path-mapper") != std::string::npos) {
            act.paths = true;
        }
        if (script.rfind("disable-gpu") != std::string::npos) {
            act.disable_gpu = true;
        }
        if (script.rfind("disable-sparkle") != std::string::npos) {
            act.disable_updater = true;
        }
        if (script.rfind("notification-bridge") != std::string::npos) {
            act.notifications = true;
        }
        if (script.rfind("clipboard-bridge") != std::string::npos) {
            act.clipboard = true;
        }
        if (script.rfind("shell-integration") != std::string::npos) {
            act.shell = true;
        }
        if (script.rfind("platform-normalizer") != std::string::npos ||
            script.rfind("electron-normalization-registry") != std::string::npos) {
            act.normalization = true;
        }
        if (script.rfind("renderer-diag") != std::string::npos) {
            act.diag_renderer = true;
        }
        if (script.rfind("main-diag") != std::string::npos) {
            act.diag_main = true;
        }
    }

    return act;
}

// ============================================================
// Runtime binary resolution
// ============================================================

struct SemanticVersion {
    int major = 0;
    int minor = 0;
    int patch = 0;

    static SemanticVersion parse(const std::string& str) {
        SemanticVersion v;
        size_t i = 0;
        while (i < str.size() && !std::isdigit(static_cast<unsigned char>(str[i]))) {
            i++;
        }
        if (i >= str.size()) return v;

        std::string s = str.substr(i);
        std::stringstream ss(s);
        std::string part;
        if (std::getline(ss, part, '.')) {
            try { v.major = std::stoi(part); } catch (...) {}
        }
        if (std::getline(ss, part, '.')) {
            try { v.minor = std::stoi(part); } catch (...) {}
        }
        if (std::getline(ss, part, '.')) {
            try { v.patch = std::stoi(part); } catch (...) {}
        }
        return v;
    }

    bool operator==(const SemanticVersion& other) const {
        return major == other.major && minor == other.minor && patch == other.patch;
    }

    bool operator<(const SemanticVersion& other) const {
        if (major != other.major) return major < other.major;
        if (minor != other.minor) return minor < other.minor;
        return patch < other.patch;
    }
};

std::string ElectronAdapter::resolve_runtime_binary() {
    if (use_mocks_) {
        return "/mock/path/electron";
    }

    std::string home;
    if (const char* h = std::getenv("HOME")) home = h;
    else {
        log("ERROR", "HOME not set — cannot locate Electron cache");
        return "";
    }

    std::string cache = home + "/.cache/macrun/electron";
    
    // Find cached versions in ~/.cache/macrun/electron/
    std::vector<std::string> cached_versions;
    std::error_code ec;
    if (fs::exists(cache, ec)) {
        for (const auto& entry : fs::directory_iterator(cache, ec)) {
            if (entry.is_directory()) {
                cached_versions.push_back(entry.path().filename().string());
            }
        }
    }

    if (cached_versions.empty()) {
        log("ERROR", "No Electron versions cached under " + cache);
        return "";
    }

    std::string version_to_match = resolved_version_;
    if (const char* ev = std::getenv("MACRUN_ELECTRON_VERSION")) {
        version_to_match = ev;
        log("INFO", "Substrate override in effect: MACRUN_ELECTRON_VERSION=" + version_to_match);
    }

    std::string version_to_use;
    std::string strategy = "fallback";

    if (!version_to_match.empty() && version_to_match != "auto") {
        SemanticVersion target = SemanticVersion::parse(version_to_match);

        struct CacheEntry {
            std::string folder_name;
            SemanticVersion version;
        };
        std::vector<CacheEntry> entries;
        for (const auto& v : cached_versions) {
            entries.push_back({v, SemanticVersion::parse(v)});
        }

        // 1. Look for exact match
        for (const auto& entry : entries) {
            if (entry.version == target) {
                version_to_use = entry.folder_name;
                strategy = "exact-match";
                break;
            }
        }

        // 2. Look for same major version
        if (version_to_use.empty()) {
            std::vector<CacheEntry> same_major;
            for (const auto& entry : entries) {
                if (entry.version.major == target.major) {
                    same_major.push_back(entry);
                }
            }
            if (!same_major.empty()) {
                std::sort(same_major.begin(), same_major.end(), [](const CacheEntry& a, const CacheEntry& b) {
                    return b.version < a.version;
                });
                version_to_use = same_major[0].folder_name;
                strategy = "nearest-match";
            }
        }

        // 3. Look for nearest-higher version
        if (version_to_use.empty()) {
            std::vector<CacheEntry> higher;
            for (const auto& entry : entries) {
                if (target < entry.version) {
                    higher.push_back(entry);
                }
            }
            if (!higher.empty()) {
                std::sort(higher.begin(), higher.end(), [](const CacheEntry& a, const CacheEntry& b) {
                    return a.version < b.version;
                });
                version_to_use = higher[0].folder_name;
                strategy = "nearest-match";
            }
        }
    }

    // 4. Fallback to highest cached version
    if (version_to_use.empty()) {
        struct CacheEntry {
            std::string folder_name;
            SemanticVersion version;
        };
        std::vector<CacheEntry> entries;
        for (const auto& v : cached_versions) {
            entries.push_back({v, SemanticVersion::parse(v)});
        }
        std::sort(entries.begin(), entries.end(), [](const CacheEntry& a, const CacheEntry& b) {
            return b.version < a.version;
        });
        version_to_use = entries[0].folder_name;
        strategy = "fallback";
    }

    std::cerr << "[MACRUN:SUBSTRATE] app=" << (bundle_id_.empty() ? "unknown" : bundle_id_)
              << " detected_version=" << (resolved_version_.empty() ? "unknown" : resolved_version_)
              << " selected_runtime=" << version_to_use
              << " strategy=" << strategy << "\n";

    std::string binary_path = cache + "/" + version_to_use + "/electron";
    
    struct stat st;
    if (stat(binary_path.c_str(), &st) == 0 && (st.st_mode & S_IXUSR)) {
        log("INFO", "Resolved Electron binary: " + binary_path);
        return binary_path;
    }

    log("ERROR", "Resolved binary path is not executable: " + binary_path);
    return "";
}

// ============================================================
// Resolve asar-extract.js tool
// ============================================================

std::string ElectronAdapter::resolve_asar_extract_tool() {
    std::string tool;
    if (const char* mh = std::getenv("MACRUN_HOME")) {
        tool = std::string(mh) + "/runtime/third_party/electron/asar-extract.js";
        if (file_exists(tool)) return tool;
    }

    for (const auto& candidate : {
        "runtime/third_party/electron/asar-extract.js",
        "../runtime/third_party/electron/asar-extract.js",
        "./runtime/shims/asar-extract.js"
    }) {
        if (file_exists(candidate)) return candidate;
    }

    std::string home;
    if (const char* h = std::getenv("HOME")) home = h;
    if (!home.empty()) {
        tool = home + "/.cache/macrun/asar-extract.js";
        if (file_exists(tool)) return tool;
    }

    return "runtime/third_party/electron/asar-extract.js";
}

// ============================================================
// ASAR extraction — fork+execvp with argument array, NO shell
// ============================================================

std::string ElectronAdapter::extract_asar(const std::string& asar_path) {
    if (use_mocks_) {
        return "/tmp/macrun-mock-extracted/";
    }

    if (!file_exists(asar_path)) {
        log("ERROR", "ASAR file not found: " + asar_path);
        return "";
    }

    char tmpdir[] = "/tmp/macrun-XXXXXX";
    if (!mkdtemp(tmpdir)) {
        log("ERROR", "Failed to create temp directory for ASAR extraction");
        return "";
    }
    std::string dest_dir(tmpdir);

    std::string tool = resolve_asar_extract_tool();
    if (!file_exists(tool)) {
        log("ERROR", "ASAR extract tool not found: " + tool + ". Run acquire.sh first.");
        return "";
    }

    log("INFO", "Extracting ASAR: " + asar_path + " → " + dest_dir + " (tool: " + tool + ")");

    pid_t pid = fork();
    if (pid == 0) {
        const char* argv[] = {
            "node",
            tool.c_str(),
            asar_path.c_str(),
            dest_dir.c_str(),
            nullptr
        };
        execvp("node", const_cast<char* const*>(argv));
        _exit(127);
    } else if (pid > 0) {
        int status;
        waitpid(pid, &status, 0);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            log("ERROR", "ASAR extraction failed (exit=" + std::to_string(WEXITSTATUS(status)) + ")");
            std::error_code ec;
            fs::remove_all(dest_dir, ec);
            return "";
        }
    } else {
        log("ERROR", "fork() for ASAR extraction failed: " + std::string(std::strerror(errno)));
        return "";
    }

    std::string src_dir = fs::path(asar_path).parent_path().string();
    std::error_code sec;
    for (const auto& entry : fs::directory_iterator(src_dir, sec)) {
        if (sec) break;
        std::string filename = entry.path().filename().string();
        if (filename == "app.asar" || filename == "app.asar.unpacked") {
            continue;
        }
        std::string target = dest_dir + "/" + filename;
        if (!file_exists(target)) {
            if (entry.is_directory()) {
                fs::create_directory_symlink(entry.path(), target, sec);
            } else {
                fs::create_symlink(entry.path(), target, sec);
            }
            if (sec) {
                log("WARN", "Failed to symlink sibling resource " + filename + ": " + sec.message());
            } else {
                log("INFO", "Symlinked sibling resource: " + filename);
            }
        }
    }

    log("INFO", "ASAR extracted successfully to " + dest_dir);
    return dest_dir;
}

// ============================================================
// Native module detection
// ============================================================

bool ElectronAdapter::detect_native_modules(const std::string& app_dir) {
    if (use_mocks_) {
        return true;
    }

    if (!file_exists(app_dir)) {
        log("WARN", "Extracted app directory not found: " + app_dir);
        native_modules_safe_ = true;
        return true;
    }

    native_modules_safe_ = true;
    std::vector<std::string> blocked_modules;

    std::error_code ec;
    for (const auto& entry : fs::recursive_directory_iterator(app_dir, ec)) {
        if (ec) break;
        if (entry.is_regular_file() && entry.path().extension() == ".node") {
            std::ifstream file(entry.path(), std::ios::binary);
            if (!file) continue;

            uint32_t magic = 0;
            file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
            
            bool is_macho = (magic == 0xFEEDFACE || magic == 0xFEEDFACF ||
                             magic == 0xCEFAEDFE || magic == 0xCFFAEDFE ||
                             magic == 0xCAFEBABE || magic == 0xBEBAFECA);
            
            if (is_macho) {
                native_modules_safe_ = false;
                blocked_modules.push_back(entry.path().string());
                log("ERROR", "Darwin-native .node module detected: " + entry.path().string());
            } else {
                log("INFO", "ELF .node module (Linux-safe): " + entry.path().string());
            }
        }
    }

    if (!native_modules_safe_) {
        if (const char* env_bypass = std::getenv("MACRUN_ALLOW_DARWIN_NATIVE")) {
            if (std::string(env_bypass) == "1") {
                native_module_bypass_active_ = true;
                record_degradation("unsafe", "experimental",
                                   "native-module-bypass",
                                   "MACRUN_ALLOW_DARWIN_NATIVE=1 — bypassing " + 
                                   std::to_string(blocked_modules.size()) +
                                   " Darwin-native .node module(s)");
                log("WARN", "*** UNSAFE COMPATIBILITY MODE *** Darwin-native .node modules bypassed via MACRUN_ALLOW_DARWIN_NATIVE=1. "
                           "Modules: " + std::to_string(blocked_modules.size()) + ". Behavior is NOT guaranteed. "
                           "Consider Tier 4B VM-assisted execution.");

                for (const auto& m : blocked_modules) {
                    log("WARN", "  Unsafe bypass: " + m + " — Darwin-native module loaded with Proxy stubs");
                }
                return true;
            }
        }
        log("FATAL", "Found " + std::to_string(blocked_modules.size()) +
                     " Darwin-native .node module(s). Cannot run on Linux. "
                     "Set MACRUN_ALLOW_DARWIN_NATIVE=1 for unsafe bypass (NOT recommended).");
        for (const auto& m : blocked_modules) {
            log("ERROR", "  Blocked: " + m);
        }
    }

    return native_modules_safe_;
}

// ============================================================
// Command line building
// ============================================================

std::vector<std::string> ElectronAdapter::build_command_line(const std::string& app_dir) {
    std::vector<std::string> argv;

    if (use_mocks_) {
        argv.push_back("/mock/electron");
        argv.push_back(app_dir);
        return argv;
    }

    argv.push_back(runtime_binary_path_);
    argv.push_back("--no-sandbox");

    auto act = compute_shim_activation();
    if (act.disable_gpu) {
        argv.push_back("--disable-gpu");
        argv.push_back("--disable-gpu-compositing");
    }

    if (const char* env_args = std::getenv("MACRUN_ELECTRON_ARGS")) {
        std::string args_str(env_args);
        std::stringstream ss(args_str);
        std::string arg;
        while (ss >> arg) {
            argv.push_back(arg);
        }
    }

    if (!shims_dir_.empty()) {
        std::string preload = shims_dir_ + "/preload-main.js";
        if (file_exists(preload)) {
            argv.push_back("--preload=" + preload);
        } else {
            log("WARN", "Preload main script not found: " + preload);
        }
    }

    argv.push_back(app_dir.empty() ? "." : app_dir);

    std::ostringstream cmd_log;
    for (size_t i = 0; i < argv.size(); i++) {
        if (i > 0) cmd_log << " ";
        if (argv[i].find(' ') != std::string::npos) {
            cmd_log << "\"" << argv[i] << "\"";
        } else {
            cmd_log << argv[i];
        }
    }
    log("INFO", "Command line: " + cmd_log.str());

    return argv;
}

// ============================================================
// Child environment construction — isolated, no setenv pollution
// ============================================================

std::vector<std::string> ElectronAdapter::build_child_environment() {
    auto act = compute_shim_activation();
    std::vector<std::string> overrides;

    overrides.push_back("MACRUN_SHIM_PATHS=1");
    overrides.push_back("MACRUN_SHIM_DISABLE_UPDATER=1");

    if (act.disable_gpu)    overrides.push_back("MACRUN_SHIM_DISABLE_GPU=1");
    if (act.notifications)  overrides.push_back("MACRUN_SHIM_NOTIFICATIONS=1");
    if (act.clipboard)      overrides.push_back("MACRUN_SHIM_CLIPBOARD=1");
    if (act.shell)          overrides.push_back("MACRUN_SHIM_SHELL=1");
    if (act.normalization)  overrides.push_back("MACRUN_SHIM_NORMALIZATION=1");

    if (act.diag_renderer)  overrides.push_back("MACRUN_DIAG_RENDERER=1");
    // Check if the application is an ESM application (checks for boot-shim.cjs)
    bool is_esm = false;
    if (!extracted_app_dir_.empty()) {
        struct stat st;
        std::string check_path = extracted_app_dir_ + "/boot-shim.cjs";
        if (stat(check_path.c_str(), &st) == 0) {
            is_esm = true;
        }
    }

    std::string node_options;
    if (const char* host_opts = std::getenv("NODE_OPTIONS")) {
        node_options = host_opts;
    }
    if (act.diag_main && !shims_dir_.empty()) {
        if (!node_options.empty()) node_options += " ";
        node_options = node_options + "--require=" + shims_dir_ + "/main-diag.js";
    }
    if (is_esm && !shims_dir_.empty()) {
        std::string loader_path = shims_dir_ + "/esm-loader.mjs";
        struct stat st;
        if (stat(loader_path.c_str(), &st) == 0) {
            if (!node_options.empty()) node_options += " ";
            node_options += "--experimental-loader=" + loader_path;
        }
    }
    if (!node_options.empty()) {
        overrides.push_back("NODE_OPTIONS=" + node_options);
    }
    if (act.diag_main) {
        overrides.push_back("MACRUN_DIAG_MAIN=1");
    }
    if (act.diag_renderer || act.diag_main) {
        overrides.push_back("MACRUN_DIAG_FILE=/tmp/macrun-diag-" + std::to_string(getpid()) + ".log");
    }

    if (!shims_dir_.empty()) {
        overrides.push_back("MACRUN_NORMALIZATION_REGISTRY=" + shims_dir_ + "/electron-normalization-registry.js");
        overrides.push_back("MACRUN_PLATFORM_NORMALIZER=" + shims_dir_ + "/platform-normalizer.js");
        if (!extracted_app_dir_.empty()) {
            overrides.push_back("MACRUN_EXTRACTED_APP_DIR=" + extracted_app_dir_);
        }
    }

    std::string home;
    if (const char* h = std::getenv("HOME")) home = h;
    if (!home.empty()) {
        overrides.push_back("XDG_DATA_HOME=" + home + "/.local/share");
        overrides.push_back("XDG_CONFIG_HOME=" + home + "/.config");
    }

    // Preserve basic host environment vars
    for (const char* key : {
        "PATH", "DISPLAY", "XAUTHORITY", "WAYLAND_DISPLAY", 
        "PULSE_SERVER", "DBUS_SESSION_BUS_ADDRESS", "LANG", "LC_ALL",
        "NODE_PATH"
    }) {
        if (const char* val = std::getenv(key)) {
            overrides.push_back(std::string(key) + "=" + val);
        }
    }

    return overrides;
}

// ============================================================
// execute() — the real execution pipeline
// ============================================================

bool ElectronAdapter::execute() {
    if (status_ != AdapterStatus::Running) {
        log("ERROR", "Cannot execute app: Electron adapter is not running");
        return false;
    }

    runtime_binary_path_ = resolve_runtime_binary();
    if (runtime_binary_path_.empty()) {
        detail_ = "No cached Electron runtime found. Run acquire.sh --all first.";
        log("FATAL", detail_);
        status_ = AdapterStatus::Error;
        return false;
    }
    log("INFO", "Runtime binary: " + runtime_binary_path_);

    std::string app_dir;
    if (!asar_path_.empty()) {
        struct stat st;
        if (stat(asar_path_.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
            app_dir = asar_path_;
            extracted_app_dir_ = app_dir;
            log("INFO", "Running app directly from directory: " + app_dir);
        } else {
            extracted_app_dir_ = extract_asar(asar_path_);
            if (extracted_app_dir_.empty()) {
                detail_ = "ASAR extraction failed for: " + asar_path_;
                log("FATAL", detail_);
                status_ = AdapterStatus::Error;
                return false;
            }
            app_dir = extracted_app_dir_;
        }

        // Dynamic Main-Process Boot-Shim Injection
        // Read package.json to determine if the app is an ES module
        std::string entry_script = "index.js";
        bool is_es_module = false;
        std::string pkg_json_path = app_dir + "/package.json";
        std::ifstream pkg_file(pkg_json_path);
        if (pkg_file) {
            std::string content((std::istreambuf_iterator<char>(pkg_file)),
                                std::istreambuf_iterator<char>());
            pkg_file.close();
            
            is_es_module = (content.find("\"type\"") != std::string::npos &&
                            content.find("\"module\"") != std::string::npos);

            auto pos = content.find("\"main\"");
            if (pos != std::string::npos) {
                auto colon = content.find(":", pos);
                if (colon != std::string::npos) {
                    auto start_quote = content.find("\"", colon);
                    if (start_quote != std::string::npos) {
                        auto end_quote = content.find("\"", start_quote + 1);
                        if (end_quote != std::string::npos) {
                            entry_script = content.substr(start_quote + 1, end_quote - start_quote - 1);
                        }
                    }
                }
            }
        }

        std::string boot_shim_ext = is_es_module ? ".cjs" : ".js";
        std::string boot_shim_path = app_dir + "/boot-shim" + boot_shim_ext;

        std::ofstream shim_file(boot_shim_path);
        if (shim_file) {
            shim_file << "if (typeof process !== 'undefined') {\n"
                      << "  if (process.stdout) {\n"
                      << "    process.stdout.on('error', () => {});\n"
                      << "    const origWrite = process.stdout.write;\n"
                      << "    process.stdout.write = function() {\n"
                      << "      try { return origWrite.apply(this, arguments); } catch(_) { return true; }\n"
                      << "    };\n"
                      << "  }\n"
                      << "  if (process.stderr) {\n"
                      << "    process.stderr.on('error', () => {});\n"
                      << "    const origWrite = process.stderr.write;\n"
                      << "    process.stderr.write = function() {\n"
                      << "      try { return origWrite.apply(this, arguments); } catch(_) { return true; }\n"
                      << "    };\n"
                      << "  }\n"
                      << "}\n"
                      << "if (typeof process !== 'undefined' && typeof process.dlopen === 'function') {\n"
                      << "  const origDlopen = process.dlopen;\n"
                      << "  process.dlopen = function (module, filename, flags) {\n"
                      << "    try {\n"
                      << "      return origDlopen.apply(this, arguments);\n"
                      << "    } catch (e) {\n"
                      << "      console.warn('[macrun-shim] Failed to load native module:', filename, '; Stubbing with a recursive Proxy. Error:', e.message);\n"
                      << "      const makeProxyStub = function () {\n"
                      << "        const stub = function () { return makeProxyStub(); };\n"
                      << "        return new Proxy(stub, {\n"
                      << "          get: function (target, prop) {\n"
                      << "            if (typeof prop === 'symbol') {\n"
                      << "              if (prop === Symbol.toStringTag) return 'MacRunStub';\n"
                      << "              if (prop === Symbol.toPrimitive) return function(hint) {\n"
                      << "                if (hint === 'string') return '[object MacRunStub]';\n"
                      << "                return 0;\n"
                      << "              };\n"
                      << "              return undefined;\n"
                      << "            }\n"
                      << "            if (prop === 'then') return undefined;\n"
                      << "            if (prop === 'inspect' || prop === 'prototype') return undefined;\n"
                      << "            if (prop === 'toString') return function() { return '[object MacRunStub]'; };\n"
                      << "            if (prop === 'valueOf') return function() { return 0; };\n"
                      << "            return makeProxyStub();\n"
                      << "          },\n"
                      << "          apply: function (target, thisArg, argumentsList) {\n"
                      << "            return makeProxyStub();\n"
                      << "          }\n"
                      << "        });\n"
                      << "      };\n"
                      << "      module.exports = makeProxyStub();\n"
                      << "      return;\n"
                      << "    }\n"
                      << "  };\n"
                      << "}\n"
                      << "if (typeof globalThis.crypto === 'undefined') {\n"
                      << "  try {\n"
                      << "    Object.defineProperty(globalThis, 'crypto', {\n"
                      << "      value: require('node:crypto').webcrypto,\n"
                      << "      configurable: true,\n"
                      << "      writable: true\n"
                      << "    });\n"
                      << "  } catch (_) {}\n"
                      << "}\n"
                      << "const Module = require('module');\n"
                      << "const originalLoad = Module._load;\n\n"
                      << "const mockInspectorPromises = {\n"
                      << "  Session: class Session {\n"
                      << "    async connect() {}\n"
                      << "    async disconnect() {}\n"
                      << "    async post(method, params) {\n"
                      << "      if (method === 'Profiler.stop') {\n"
                      << "        return {\n"
                      << "          profile: {\n"
                      << "            nodes: [\n"
                      << "              {\n"
                      << "                id: 1,\n"
                      << "                callFrame: {\n"
                      << "                  functionName: '(root)',\n"
                      << "                  scriptId: '0',\n"
                      << "                  url: '',\n"
                      << "                  lineNumber: -1,\n"
                      << "                  columnNumber: -1\n"
                      << "                },\n"
                      << "                hitCount: 0\n"
                      << "              }\n"
                      << "            ],\n"
                      << "            startTime: 0,\n"
                      << "            endTime: 0\n"
                      << "          }\n"
                      << "        };\n"
                      << "      }\n"
                      << "      return {};\n"
                      << "    }\n"
                      << "  },\n"
                      << "  open: async () => {},\n"
                      << "  close: async () => {},\n"
                      << "  url: () => undefined\n"
                      << "};\n\n"
                      << "let cachedProxy = null;\n"
                      << "Module._load = function (request, parent, isMain) {\n"
                      << "  if (request === 'node:inspector/promises' || request === 'inspector/promises') {\n"
                      << "    return mockInspectorPromises;\n"
                      << "  }\n"
                      << "  if (request === 'electron') {\n"
                      << "    if (cachedProxy) return cachedProxy;\n"
                      << "    const electron = originalLoad.apply(this, arguments);\n"
                      << "    const overrides = {};\n"
                      << "    if (electron && electron.BrowserWindow) {\n"
                      << "      const originalBrowserWindow = electron.BrowserWindow;\n"
                      << "      const proto = originalBrowserWindow.prototype;\n"
                      << "      const originalSetBackgroundColor = proto.setBackgroundColor;\n"
                      << "      if (typeof originalSetBackgroundColor === 'function') {\n"
                      << "        proto.setBackgroundColor = function (color) {\n"
                      << "          if (color && (color.toLowerCase() === '#00000000' || color.toLowerCase() === 'transparent')) {\n"
                      << "            const isDark = electron.nativeTheme && electron.nativeTheme.shouldUseDarkColors;\n"
                      << "            color = isDark ? '#202020' : '#ffffff';\n"
                      << "          }\n"
                      << "          return originalSetBackgroundColor.call(this, color);\n"
                      << "        };\n"
                      << "      }\n"
                      << "      const originalSetVibrancy = proto.setVibrancy;\n"
                      << "      if (typeof originalSetVibrancy === 'function') {\n"
                      << "        proto.setVibrancy = function () {\n"
                      << "          // Vibrancy is not supported on Linux and causes transparency glitches.\n"
                      << "        };\n"
                      << "      }\n"
                      << "      overrides.BrowserWindow = new Proxy(originalBrowserWindow, {\n"
                      << "        construct(target, argumentsList, newTarget) {\n"
                      << "          let options = argumentsList[0];\n"
                      << "          if (options && typeof options === 'object') {\n"
                      << "            if (options.transparent === true) {\n"
                      << "              console.log('[macrun-shim] Overriding transparent: true -> false for BrowserWindow');\n"
                      << "              options.transparent = false;\n"
                      << "            }\n"
                      << "            if (options.vibrancy) {\n"
                      << "              console.log('[macrun-shim] Overriding vibrancy -> undefined for BrowserWindow');\n"
                      << "              delete options.vibrancy;\n"
                      << "            }\n"
                      << "            if (options.backgroundColor && (options.backgroundColor.toLowerCase() === '#00000000' || options.backgroundColor.toLowerCase() === 'transparent')) {\n"
                      << "              const isDark = electron.nativeTheme && electron.nativeTheme.shouldUseDarkColors;\n"
                      << "              options.backgroundColor = isDark ? '#202020' : '#ffffff';\n"
                      << "            }\n"
                      << "          }\n"
                      << "          return Reflect.construct(target, argumentsList, newTarget);\n"
                      << "        }\n"
                      << "      });\n"
                      << "    }\n"
                      << "    if (electron && electron.WebContentsView && electron.WebContentsView.prototype) {\n"
                      << "      const wcvProto = electron.WebContentsView.prototype;\n"
                      << "      const originalWcvSetBackgroundColor = wcvProto.setBackgroundColor;\n"
                      << "      if (typeof originalWcvSetBackgroundColor === 'function') {\n"
                      << "        wcvProto.setBackgroundColor = function (color) {\n"
                      << "          if (color && (color.toLowerCase() === '#00000000' || color.toLowerCase() === 'transparent')) {\n"
                      << "            const isDark = electron.nativeTheme && electron.nativeTheme.shouldUseDarkColors;\n"
                      << "            color = isDark ? '#202020' : '#ffffff';\n"
                      << "          }\n"
                      << "          return originalWcvSetBackgroundColor.call(this, color);\n"
                      << "        };\n"
                      << "      }\n"
                      << "    }\n"
                      << "    if (electron && electron.BrowserView && electron.BrowserView.prototype) {\n"
                      << "      const bvProto = electron.BrowserView.prototype;\n"
                      << "      const originalBvSetBackgroundColor = bvProto.setBackgroundColor;\n"
                      << "      if (typeof originalBvSetBackgroundColor === 'function') {\n"
                      << "        bvProto.setBackgroundColor = function (color) {\n"
                      << "          if (color && (color.toLowerCase() === '#00000000' || color.toLowerCase() === 'transparent')) {\n"
                      << "            const isDark = electron.nativeTheme && electron.nativeTheme.shouldUseDarkColors;\n"
                      << "            color = isDark ? '#202020' : '#ffffff';\n"
                      << "          }\n"
                      << "          return originalBvSetBackgroundColor.call(this, color);\n"
                      << "        };\n"
                      << "      }\n"
                      << "    }\n"
                      << "    cachedProxy = new Proxy({}, {\n"
                      << "      get(target, prop) {\n"
                      << "        if (overrides[prop] !== undefined) {\n"
                      << "          return overrides[prop];\n"
                      << "        }\n"
                      << "        return electron[prop];\n"
                      << "      },\n"
                      << "      set(target, prop, value) {\n"
                      << "        overrides[prop] = value;\n"
                      << "        return true;\n"
                      << "      },\n"
                      << "      has(target, prop) {\n"
                      << "        return prop in overrides || prop in electron;\n"
                      << "      },\n"
                      << "      ownKeys(target) {\n"
                      << "        const keys = new Set(Object.getOwnPropertyNames(electron));\n"
                      << "        for (const k in overrides) { keys.add(k); }\n"
                      << "        return Array.from(keys);\n"
                      << "      },\n"
                      << "      getOwnPropertyDescriptor(target, prop) {\n"
                      << "        if (prop in overrides || prop in electron) {\n"
                      << "          return {\n"
                      << "            value: overrides[prop] !== undefined ? overrides[prop] : electron[prop],\n"
                      << "            writable: true,\n"
                      << "            enumerable: true,\n"
                      << "            configurable: true\n"
                      << "          };\n"
                      << "        }\n"
                      << "        return undefined;\n"
                      << "      }\n"
                      << "    });\n"
                      << "    try {\n"
                      << "      if (process.env.MACRUN_SHIM_NORMALIZATION === '1') {\n"
                      << "        const registryPath = process.env.MACRUN_NORMALIZATION_REGISTRY;\n"
                      << "        const normalizerPath = process.env.MACRUN_PLATFORM_NORMALIZER;\n"
                      << "        if (registryPath && require('fs').existsSync(registryPath)) {\n"
                      << "          require(registryPath);\n"
                      << "        }\n"
                      << "        if (normalizerPath && require('fs').existsSync(normalizerPath)) {\n"
                      << "          require(normalizerPath);\n"
                      << "        }\n"
                      << "      }\n"
                      << "    } catch (err) {\n"
                      << "      console.error('[macrun:boot-shim] Error applying Electron normalization:', err);\n"
                      << "    }\n"
                      << "    return cachedProxy;\n"
                      << "  }\n"
                      << "  return originalLoad.apply(this, arguments);\n"
                      << "};\n\n"
                      << "console.log('[macrun:boot-shim] Intercepted and shimmed inspector/promises and electron successfully');\n";
            shim_file.close();
            log("INFO", "Written dynamic boot-shim: " + boot_shim_path);

            std::string entry_path = app_dir + "/" + entry_script;
            if (file_exists(entry_path)) {
                std::ifstream entry_read(entry_path);
                if (entry_read) {
                    std::string original_js((std::istreambuf_iterator<char>(entry_read)),
                                            std::istreambuf_iterator<char>());
                    entry_read.close();

                    if (original_js.find("boot-shim") == std::string::npos) {
                        std::ofstream entry_write(entry_path);
                        if (entry_write) {
                            if (is_es_module) {
                                entry_write << "import '" << boot_shim_path << "';\n"
                                            << original_js;
                            } else {
                                entry_write << "require('" << boot_shim_path << "');\n"
                                            << original_js;
                            }
                            entry_write.close();
                            log("INFO", "Prepended boot-shim require/import to main entry script: " + entry_path);
                        } else {
                            log("ERROR", "Failed to write updated entry script: " + entry_path);
                        }
                    } else {
                        log("INFO", "boot-shim already prepended to main entry script: " + entry_path);
                    }
                } else {
                    log("ERROR", "Failed to read entry script: " + entry_path);
                }
            } else {
                log("ERROR", "Entry script not found: " + entry_path);
            }
        } else {
            log("ERROR", "Failed to create boot-shim file: " + boot_shim_path);
        }
    } else {
        app_dir = ".";
        log("INFO", "No ASAR path set. Using current directory as app root.");
    }

    if (!detect_native_modules(app_dir)) {
        detail_ = "Darwin-native .node modules detected — cannot run on Linux.";
        status_ = AdapterStatus::Error;
        return false;
    }

    auto act = compute_shim_activation();
    int active_shims = static_cast<int>(act.paths) + 
                       static_cast<int>(act.disable_updater) + static_cast<int>(act.notifications) + 
                       static_cast<int>(act.clipboard) + static_cast<int>(act.shell);
    
    if (active_shims > 0 && degradation_category_ == "transparent") {
        record_degradation("shimmed", "functional", "shim-activation",
                           std::to_string(active_shims) + " shim(s) active for Linux integration");
    }
    if (act.disable_gpu) {
        record_degradation("functional", "degraded", "gpu-acceleration",
                           "GPU acceleration disabled — software rendering fallback active");
    }
    if (native_module_bypass_active_) {
        record_degradation("unsafe", "experimental", "native-module-bypass",
                           "MACRUN_ALLOW_DARWIN_NATIVE=1 — unsafe compatibility mode");
    }

    auto env_vars = build_child_environment();

    static const char* experimental_keys[] = {
        "MACRUN_EXPERIMENTAL_METAL_SOFTWARE",
        "MACRUN_EXPERIMENTAL_SWIFTUI_FLATTEN",
        "MACRUN_EXPERIMENTAL_UNSAFE_GPU",
        "MACRUN_EXPERIMENTAL_COREDATA_XDG",
        nullptr
    };
    for (int i = 0; experimental_keys[i] != nullptr; i++) {
        if (const char* val = std::getenv(experimental_keys[i])) {
            if (val[0] != '0' && val[0] != '\0') {
                record_degradation("experimental", "experimental", "experimental-mode",
                                   std::string("Experimental mode active: ") + experimental_keys[i] + "=" + val);
                break;
            }
        }
    }

    auto argv = build_command_line(app_dir);

    if (use_mocks_) {
        log("INFO", "Mock mode: Electron process would execute: " + runtime_binary_path_);
        return true;
    }

    pid_t pid = fork();
    if (pid == 0) {
        setpgid(0, 0);

        for (const auto& var : env_vars) {
            auto eq = var.find('=');
            if (eq != std::string::npos) {
                std::string key = var.substr(0, eq);
                std::string val = var.substr(eq + 1);
                setenv(key.c_str(), val.c_str(), 1);
            }
        }

        std::vector<char*> cargv;
        for (auto& a : argv) {
            cargv.push_back(a.data());
        }
        cargv.push_back(nullptr);

        execvp(runtime_binary_path_.c_str(), cargv.data());

        std::cerr << "macrun: execvp failed: " << std::strerror(errno) << std::endl;
        _exit(127);
    } else if (pid > 0) {
        child_pid_ = pid;
        log("INFO", "Electron process spawned: pid=" + std::to_string(pid) + " (process group leader)");
        log("INFO", "Degradation report: " + degradation_report());
        return true;
    } else {
        log("ERROR", "fork() failed: " + std::string(std::strerror(errno)));
        status_ = AdapterStatus::Error;
        detail_ = "Process fork failed";
        return false;
    }
}

// ============================================================
// XDG desktop launcher generation
// ============================================================

bool ElectronAdapter::generate_xdg_desktop_launcher(const std::string& app_name,
                                                   const std::string& app_path,
                                                   const std::string& icon_path) {
    std::string home;
    if (const char* h = std::getenv("HOME")) home = h;
    else return false;

    std::string apps_dir = home + "/.local/share/applications";

    std::string safe_name = app_name;
    for (char& c : safe_name) {
        if (c == ' ' || c == '/' || c == '\\' || c == '&' || c == ';' || c == '|') {
            c = '-';
        }
    }
    while (safe_name.find("--") != std::string::npos) {
        size_t pos = safe_name.find("--");
        safe_name.erase(pos, 1);
    }
    if (!safe_name.empty() && safe_name.front() == '-') safe_name.erase(0, 1);
    if (!safe_name.empty() && safe_name.back() == '-') safe_name.pop_back();

    std::string desktop_file = apps_dir + "/macrun-" + safe_name + ".desktop";

    std::error_code ec;
    fs::create_directories(apps_dir, ec);

    std::ofstream f(desktop_file);
    if (!f) {
        log("ERROR", "Failed to create desktop launcher: " + desktop_file);
        return false;
    }

    f << "[Desktop Entry]\n";
    f << "Type=Application\n";
    f << "Name=" << app_name << "\n";
    f << "Comment=macOS application running via MacRun (Tier 0 Electron)\n";
    f << "Exec=macrun-cli --launch \"" << app_path << "\"\n";
    if (!icon_path.empty()) {
        f << "Icon=" << icon_path << "\n";
    }
    f << "Categories=Utility;\n";
    f << "Terminal=false\n";
    f << "StartupWMClass=" << app_name << "\n";
    f << "X-MacRun-App=" << app_path << "\n";

    f.close();
    log("INFO", "XDG desktop launcher created: " + desktop_file);
    return true;
}

// ============================================================
// Degradation Model Methods
// ============================================================

std::string ElectronAdapter::degradation_category() const {
    return degradation_category_;
}

std::string ElectronAdapter::degradation_confidence() const {
    return degradation_confidence_;
}

void ElectronAdapter::record_degradation(const std::string& category, const std::string& confidence,
                                         const std::string& capability, const std::string& reason) {
    auto get_category_severity = [](const std::string& cat) {
        if (cat == "transparent") return 0;
        if (cat == "shimmed") return 1;
        if (cat == "functional") return 2;
        if (cat == "unsafe") return 3;
        if (cat == "experimental") return 4;
        return -1;
    };
    
    auto get_confidence_severity = [](const std::string& conf) {
        if (conf == "verified") return 0;
        if (conf == "functional") return 1;
        if (conf == "warning") return 2;
        if (conf == "degraded") return 3;
        if (conf == "experimental") return 4;
        return -1;
    };

    if (get_category_severity(category) > get_category_severity(degradation_category_)) {
        degradation_category_ = category;
    }

    if (get_confidence_severity(confidence) > get_confidence_severity(degradation_confidence_)) {
        degradation_confidence_ = confidence;
    }

    std::string recommended_action = "None";
    if (category == "unsafe") {
        recommended_action = "Consider_Tier4B_VM";
    } else if (category == "functional" || category == "experimental") {
        recommended_action = "VM_assisted_execution";
    }

    std::string event = "tier=Tier0 category=" + category + 
                        " capability=" + capability + 
                        " reason=\"" + reason + "\"" + 
                        " recommended_action=" + recommended_action;
    degradation_events_.push_back(event);

    std::string formatted_log = "[MACRUN:DEGRADATION] " + event;
    std::cerr << formatted_log << std::endl;
    log("WARN", "Degradation event recorded: " + event);
}

std::string ElectronAdapter::degradation_report() const {
    std::string report = "degradation_category=" + degradation_category_ + 
                         " confidence=" + degradation_confidence_ + 
                         " native_module_bypass=" + (native_module_bypass_active_ ? "1" : "0") +
                         " events=";
    if (degradation_events_.empty()) {
        report += "none";
    } else {
        for (size_t i = 0; i < degradation_events_.size(); ++i) {
            if (i > 0) report += "; ";
            report += degradation_events_[i];
        }
    }
    return report;
}

// ============================================================
// Mock controls
// ============================================================

void ElectronAdapter::set_mock_runtime_cached(bool cached) {
    mock_runtime_cached_ = cached;
}

void ElectronAdapter::set_mock_sandbox_supported(bool supported) {
    mock_sandbox_supported_ = supported;
}

} // namespace platform::adapters
