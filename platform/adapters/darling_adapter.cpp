#include "darling_adapter.hpp"
#include <sys/stat.h>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <fstream>

namespace platform::adapters {

DarlingAdapter::DarlingAdapter()
    : status_(AdapterStatus::Uninitialized),
      exit_code_(0),
      mock_darling_installed_(true),
      mock_kernel_module_loaded_(true)
{
    log("INFO", "Darling adapter instantiated");
}

void DarlingAdapter::log(const std::string& level, const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t_now), "%Y-%m-%dT%H:%M:%SZ");
    logs_.push_back({oss.str(), level, "DarlingAdapter", message});
}

bool DarlingAdapter::initialize() {
    if (status_ != AdapterStatus::Uninitialized) {
        log("WARN", "Adapter already initialized");
        return false;
    }
    
    if (!is_supported()) {
        status_ = AdapterStatus::Error;
        detail_ = "Host lacks support for Darling execution";
        log("ERROR", detail_);
        return false;
    }

    status_ = AdapterStatus::Ready;
    log("INFO", "Darling prefix initialized at: " + prefix_path_);
    return true;
}

bool DarlingAdapter::start() {
    if (status_ != AdapterStatus::Ready) {
        log("ERROR", "Cannot start adapter: not in ready state");
        return false;
    }
    status_ = AdapterStatus::Running;
    log("INFO", "Darling execution started");
    return true;
}

bool DarlingAdapter::stop() {
    if (status_ != AdapterStatus::Running && status_ != AdapterStatus::Suspended) {
        log("WARN", "Stop called on inactive adapter");
        return false;
    }
    status_ = AdapterStatus::Terminated;
    exit_code_ = 0;
    log("INFO", "Darling execution stopped");
    return true;
}

bool DarlingAdapter::suspend() {
    if (status_ != AdapterStatus::Running) {
        log("ERROR", "Cannot suspend adapter: not running");
        return false;
    }
    status_ = AdapterStatus::Suspended;
    log("INFO", "Darling prefix operations suspended");
    return true;
}

bool DarlingAdapter::resume() {
    if (status_ != AdapterStatus::Suspended) {
        log("ERROR", "Cannot resume adapter: not suspended");
        return false;
    }
    status_ = AdapterStatus::Running;
    log("INFO", "Darling prefix operations resumed");
    return true;
}

AdapterProcessStatus DarlingAdapter::get_status() const {
    return {status_, exit_code_, detail_};
}

std::vector<DiagnosticEntry> DarlingAdapter::get_logs() const {
    return logs_;
}

void DarlingAdapter::clear_logs() {
    logs_.clear();
}

bool DarlingAdapter::has_errors() const {
    for (const auto& entry : logs_) {
        if (entry.level == "ERROR" || entry.level == "FATAL") {
            return true;
        }
    }
    return status_ == AdapterStatus::Error;
}

bool DarlingAdapter::is_supported() const {
    auto checks = check_host_capabilities();
    for (const auto& check : checks) {
        if (!check.satisfied) {
            return false;
        }
    }
    return true;
}

std::vector<HostCapabilityCheck> DarlingAdapter::check_host_capabilities() const {
    std::vector<HostCapabilityCheck> checks;

    if (use_mocks_) {
        checks.push_back({
            "Darling binary check (mock)",
            mock_darling_installed_,
            mock_darling_installed_ ? "MOCK: Darling binary found" : "MOCK: darling binary not found"
        });
        checks.push_back({
            "Darling kernel module check (mock)",
            mock_kernel_module_loaded_,
            mock_kernel_module_loaded_ ? "MOCK: darling-mach kernel module loaded" : "MOCK: module not loaded"
        });
        return checks;
    }

    // Real system probe: check for darling binary in PATH
    bool binary_found = false;
    for (const auto* path : {"/opt/darling/bin/darling", "/usr/local/bin/darling", "/usr/bin/darling"}) {
        struct stat st;
        if (stat(path, &st) == 0 && (st.st_mode & S_IXUSR)) {
            binary_found = true;
            break;
        }
    }
    checks.push_back({
        "Darling binary check",
        binary_found,
        binary_found ? "Darling binary found in PATH" : "darling binary not found"
    });

    // Check for darling kernel module
    bool module_loaded = false;
    std::ifstream modules("/proc/modules");
    if (modules) {
        std::string line;
        while (std::getline(modules, line)) {
            if (line.starts_with("darling_mach")) {
                module_loaded = true;
                break;
            }
        }
    }
    checks.push_back({
        "Darling kernel module check",
        module_loaded,
        module_loaded ? "darling-mach kernel module loaded" : "darling-mach module not loaded"
    });

    return checks;
}

void DarlingAdapter::configure_prefix(const std::string& prefix_path) {
    prefix_path_ = prefix_path;
    log("INFO", "Configured prefix path: " + prefix_path);
}

void DarlingAdapter::set_environment(const std::unordered_map<std::string, std::string>& env) {
    env_ = env;
    log("INFO", "Set environment variables: count=" + std::to_string(env.size()));
}

bool DarlingAdapter::launch_binary(const std::string& binary_path, const std::vector<std::string>& args) {
    if (status_ != AdapterStatus::Running) {
        log("ERROR", "Cannot launch binary: adapter is not running");
        return false;
    }
    log("INFO", "Launching Mach-O binary: " + binary_path + " (arg count=" + std::to_string(args.size()) + ")");
    return true;
}

} // namespace platform::adapters
