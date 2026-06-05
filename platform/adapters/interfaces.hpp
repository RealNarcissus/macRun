#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <cstdint>

namespace platform::adapters {

enum class AdapterStatus : uint8_t {
    Uninitialized,
    Ready,
    Running,
    Suspended,
    Terminated,
    Error
};

constexpr const char* status_to_string(AdapterStatus status) {
    switch (status) {
        case AdapterStatus::Uninitialized: return "uninitialized";
        case AdapterStatus::Ready:         return "ready";
        case AdapterStatus::Running:       return "running";
        case AdapterStatus::Suspended:     return "suspended";
        case AdapterStatus::Terminated:    return "terminated";
        case AdapterStatus::Error:         return "error";
    }
    return "unknown";
}

struct AdapterProcessStatus {
    AdapterStatus status = AdapterStatus::Uninitialized;
    int exit_code = 0;
    std::string detail;
};

struct DiagnosticEntry {
    std::string timestamp;
    std::string level; // "INFO", "WARN", "ERROR", "FATAL"
    std::string component;
    std::string message;
};

struct HostCapabilityCheck {
    std::string requirement_name;
    bool satisfied = false;
    std::string details;
};

// ============================================================
// Lifecycle Contract
// ============================================================
class ILifecycle {
public:
    virtual ~ILifecycle() = default;
    virtual bool initialize() = 0;
    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual bool suspend() = 0;
    virtual bool resume() = 0;
    virtual AdapterProcessStatus get_status() const = 0;
};

// ============================================================
// Diagnostics Interface
// ============================================================
class IDiagnostics {
public:
    virtual ~IDiagnostics() = default;
    virtual std::vector<DiagnosticEntry> get_logs() const = 0;
    virtual void clear_logs() = 0;
    virtual bool has_errors() const = 0;
};

// ============================================================
// Execution Capability Interface
// ============================================================
class ICapability {
public:
    virtual ~ICapability() = default;
    virtual bool is_supported() const = 0;
    virtual std::vector<HostCapabilityCheck> check_host_capabilities() const = 0;
};

} // namespace platform::adapters
