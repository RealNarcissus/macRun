#pragma once
#include "IDarlingAdapter.hpp"

namespace platform::adapters {

class DarlingAdapter : public virtual IDarlingAdapter {
public:
    DarlingAdapter();
    ~DarlingAdapter() override = default;

    // ILifecycle
    bool initialize() override;
    bool start() override;
    bool stop() override;
    bool suspend() override;
    bool resume() override;
    AdapterProcessStatus get_status() const override;

    // IDiagnostics
    std::vector<DiagnosticEntry> get_logs() const override;
    void clear_logs() override;
    bool has_errors() const override;

    // ICapability
    bool is_supported() const override;
    std::vector<HostCapabilityCheck> check_host_capabilities() const override;

    // IDarlingAdapter
    void configure_prefix(const std::string& prefix_path) override;
    void set_environment(const std::unordered_map<std::string, std::string>& env) override;
    bool launch_binary(const std::string& binary_path, const std::vector<std::string>& args) override;

    // Test support controls — set use_mocks(true) before mock setters take effect
    void set_use_mocks(bool mocks) { use_mocks_ = mocks; }
    void set_mock_darling_installed(bool installed) { mock_darling_installed_ = installed; }
    void set_mock_kernel_module_loaded(bool loaded) { mock_kernel_module_loaded_ = loaded; }

private:
    void log(const std::string& level, const std::string& message);

    AdapterStatus status_;
    int exit_code_;
    std::string detail_;
    std::string prefix_path_;
    std::unordered_map<std::string, std::string> env_;
    std::vector<DiagnosticEntry> logs_;
    
    bool use_mocks_ = false;  // false → real system probes
    bool mock_darling_installed_ = false;
    bool mock_kernel_module_loaded_ = false;
};

} // namespace platform::adapters
