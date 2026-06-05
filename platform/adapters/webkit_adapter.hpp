#pragma once
#include "IWebKitAdapter.hpp"

namespace platform::adapters {

class WebKitAdapter : public virtual IWebKitAdapter {
public:
    WebKitAdapter();
    ~WebKitAdapter() override = default;

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

    // IWebKitAdapter
    void set_url(const std::string& url) override;
    void configure_window(int width, int height, const std::string& title) override;
    void bind_ipc_callback(const std::string& message_name, std::function<void(const std::string&)> callback) override;
    bool run_window_loop() override;

    // Test support controls — set use_mocks(true) before mock setters take effect
    void set_use_mocks(bool mocks) { use_mocks_ = mocks; }
    void set_mock_webkitgtk_installed(bool installed);
    void set_mock_display_server_available(bool available);

private:
    void log(const std::string& level, const std::string& message);

    AdapterStatus status_;
    int exit_code_;
    std::string detail_;
    std::string url_;
    int window_width_;
    int window_height_;
    std::string window_title_;
    std::unordered_map<std::string, std::function<void(const std::string&)>> ipc_callbacks_;
    std::vector<DiagnosticEntry> logs_;

    bool use_mocks_ = false;  // false → real system probes
    bool mock_webkitgtk_installed_ = false;
    bool mock_display_server_available_ = false;
};

} // namespace platform::adapters
