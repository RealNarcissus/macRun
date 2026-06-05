#pragma once
#include "IQemuAdapter.hpp"

namespace platform::adapters {

class QemuAdapter : public virtual IQemuAdapter {
public:
    QemuAdapter();
    ~QemuAdapter() override = default;

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

    // IQemuAdapter
    void configure_cpu(const std::string& cpu_model) override;
    void configure_memory(uint64_t memory_mb) override;
    void set_qmp_socket(const std::string& socket_path) override;
    bool boot_vm(const std::string& base_image_path, const std::string& overlay_path) override;
    bool execute_command(const std::string& guest_cmd) override;

    // Test support controls — set use_mocks(true) before mock setters take effect
    void set_use_mocks(bool mocks) { use_mocks_ = mocks; }
    void set_mock_qemu_installed(bool installed);
    void set_mock_kvm_available(bool available);

private:
    void log(const std::string& level, const std::string& message);

    AdapterStatus status_;
    int exit_code_;
    std::string detail_;
    std::string cpu_model_;
    uint64_t memory_mb_;
    std::string qmp_socket_path_;
    std::vector<DiagnosticEntry> logs_;

    bool use_mocks_ = false;  // false → real system probes
    bool mock_qemu_installed_ = false;
    bool mock_kvm_available_ = false;
};

} // namespace platform::adapters
