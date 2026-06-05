// Substrate Lifecycle Infrastructure
// Architecture reference: docs/architecture/SUBSTRATE_MODEL.md Sections 2-4
//   docs/architecture/FAILURE_MODEL.md Section 4 (Failure Isolation)
//
// Provides deterministic initialization, health validation, teardown,
// structured provisioning logs, and failure-state visibility for all
// runtime substrates.
//
// This is a header-only library. No substrate binaries are linked.
// The lifecycle manager queries adapters through abstract interfaces only.
//
// FAILURE_MODEL.md compliance: each lifecycle method produces structured
// status codes that map to the standardized error codes in Failure Model
// Section 4.2 (Error Translation & Codes).
#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <functional>
#include <chrono>
#include <unordered_map>

namespace substrate {

// ============================================================
// Substrate Identity
// ============================================================
enum class SubstrateId : uint8_t {
    Darling,
    QEMU_UserMode,
    Electron,
    WebKitGTK,
};

constexpr std::string_view to_string(SubstrateId id) {
    switch (id) {
        case SubstrateId::Darling:       return "darling";
        case SubstrateId::QEMU_UserMode: return "qemu-user";
        case SubstrateId::Electron:      return "electron";
        case SubstrateId::WebKitGTK:     return "webkitgtk";
    }
    return "unknown";
}

// ============================================================
// Lifecycle States (deterministic state machine)
// ============================================================
enum class LifecycleState : uint8_t {
    Unknown,
    Unavailable,    // Substrate not installed / not found
    Acquired,       // Binary sourced and verified
    Initialized,    // Ready for use
    Running,        // Active execution
    Suspended,
    Failed,         // Irrecoverable error
    Terminated,
};

constexpr std::string_view to_string(LifecycleState s) {
    switch (s) {
        case LifecycleState::Unknown:     return "unknown";
        case LifecycleState::Unavailable: return "unavailable";
        case LifecycleState::Acquired:    return "acquired";
        case LifecycleState::Initialized: return "initialized";
        case LifecycleState::Running:     return "running";
        case LifecycleState::Suspended:   return "suspended";
        case LifecycleState::Failed:      return "failed";
        case LifecycleState::Terminated:  return "terminated";
    }
    return "unknown";
}

// ============================================================
// Standardized Error Codes
// Architecture reference: docs/architecture/FAILURE_MODEL.md Section 4.2
// ============================================================
enum class SubstrateError : uint8_t {
    None = 0,
    // Darling errors
    DarlingLaunchFailed      = 1,
    DarlingPrefixLockFailed  = 2,
    DarlingPrefixCorrupted   = 3,
    DarlingServerCrashed     = 4,
    // QEMU errors
    QEMU_KVM_Unavailable     = 10,
    QEMU_QMP_Disconnect      = 11,
    QEMU_TranslationFault    = 12,
    // Electron errors
    ElectronAsarCorrupted       = 20,
    ElectronRuntimeNotCached    = 21,
    ElectronSandboxUnsupported  = 22,
    // WebKitGTK errors
    GTK_DisplayError         = 30,
    GTK_WebKitNotInstalled   = 31,
    // Generic errors
    SubstrateNotInstalled    = 40,
    SubstrateVersionMismatch = 41,
    SubstrateIntegrityFail   = 42,
};

constexpr std::string_view to_string(SubstrateError e) {
    switch (e) {
        case SubstrateError::None:                     return "none";
        case SubstrateError::DarlingLaunchFailed:       return "DARLING_LAUNCH_FAILED";
        case SubstrateError::DarlingPrefixLockFailed:   return "DARLING_PREFIX_LOCK_FAILED";
        case SubstrateError::DarlingPrefixCorrupted:    return "DARLING_PREFIX_CORRUPTED";
        case SubstrateError::DarlingServerCrashed:      return "DARLING_SERVER_CRASHED";
        case SubstrateError::QEMU_KVM_Unavailable:      return "QEMU_KVM_UNAVAILABLE";
        case SubstrateError::QEMU_QMP_Disconnect:       return "QEMU_QMP_DISCONNECT";
        case SubstrateError::QEMU_TranslationFault:      return "QEMU_TRANSLATION_FAULT";
        case SubstrateError::ElectronAsarCorrupted:      return "ELECTRON_ASAR_CORRUPTED";
        case SubstrateError::ElectronRuntimeNotCached:    return "ELECTRON_RUNTIME_NOT_CACHED";
        case SubstrateError::ElectronSandboxUnsupported:  return "ELECTRON_SANDBOX_UNSUPPORTED";
        case SubstrateError::GTK_DisplayError:           return "GTK_DISPLAY_ERROR";
        case SubstrateError::GTK_WebKitNotInstalled:     return "GTK_WEBKIT_NOT_INSTALLED";
        case SubstrateError::SubstrateNotInstalled:      return "SUBSTRATE_NOT_INSTALLED";
        case SubstrateError::SubstrateVersionMismatch:   return "SUBSTRATE_VERSION_MISMATCH";
        case SubstrateError::SubstrateIntegrityFail:     return "SUBSTRATE_INTEGRITY_FAIL";
    }
    return "unknown";
}

// ============================================================
// Health Check Result
// ============================================================
struct HealthCheckResult {
    bool healthy = false;
    SubstrateError error = SubstrateError::None;
    std::string message;
    std::string subsystem;
};

// ============================================================
// Provisioning Log Entry
// ============================================================
struct ProvisioningEntry {
    std::string timestamp;
    std::string level;      // INFO, WARN, ERROR
    SubstrateId substrate;
    LifecycleState state;
    std::string message;
};

// ============================================================
// Substrate Lifecycle Manager
// Manages lifecycle state for ALL substrates without linking
// against any substrate binary. Communicates through adapter
// interfaces only.
// ============================================================
class LifecycleManager {
public:
    LifecycleManager();

    // ==========================================================
    // Initialization — deterministic setup of all substrates
    // ==========================================================
    HealthCheckResult initialize(SubstrateId id);
    bool is_initialized(SubstrateId id) const;

    // ==========================================================
    // Health checks — verify substrate availability without executing
    // ==========================================================
    HealthCheckResult check_health(SubstrateId id) const;
    std::vector<HealthCheckResult> check_all_health() const;

    // ==========================================================
    // Provisioning — substrate readiness for execution
    // ==========================================================
    struct ProvisioningResult {
        bool ready = false;
        std::string binary_path;
        std::string version;
        std::vector<std::string> warnings;
        std::vector<std::string> environment;
    };

    ProvisioningResult provision(SubstrateId id) const;

    // ==========================================================
    // Diagnostics
    // ==========================================================
    std::vector<ProvisioningEntry> get_logs() const { return logs_; }
    void clear_logs() { logs_.clear(); }

    // ==========================================================
    // Failure isolation: record a substrate error without crashing
    // ==========================================================
    void record_failure(SubstrateId id, SubstrateError error, const std::string& detail);

    // ==========================================================
    // Teardown — deterministic cleanup with process termination
    // ==========================================================
    void teardown(SubstrateId id);
    void teardown_all();

    // ==========================================================
    // Child process tracking — adapters register PIDs for cleanup
    // ==========================================================
    void register_child_pid(int pid);

    // ==========================================================
    // Version visibility
    // ==========================================================
    struct SubstrateVersion {
        SubstrateId id;
        std::string version;
        std::string source; // "cached-binary", "system-package", "cloned-repo"
        bool available = false;
    };
    SubstrateVersion get_version(SubstrateId id) const;

private:
    struct SubstrateState {
        LifecycleState state = LifecycleState::Unknown;
        std::string binary_path;
        std::string version;
        std::string version_source;
    };

    std::unordered_map<SubstrateId, SubstrateState> states_;
    std::vector<int> registered_pids_;
    std::vector<ProvisioningEntry> logs_;
    void log(const std::string& level, SubstrateId id, LifecycleState state,
             const std::string& message);
};

} // namespace substrate

// Number of known substrate error codes (for validation)
constexpr int SUBSTRATE_ERROR_COUNT = 16;
