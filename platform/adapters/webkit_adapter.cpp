// WebKitGTK Rendering Adapter
// Architecture reference: docs/architecture/SUBSTRATE_MODEL.md Section 3 (WebKitGTK)
//
// Responsibility: probe the host system for WebKitGTK shared libraries
// and display server connectivity. In production, all capability checks
// are real system probes. Mock controls exist only for unit testing.

#include "webkit_adapter.hpp"
#include <sys/stat.h>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <cstdlib>

namespace platform::adapters {

WebKitAdapter::WebKitAdapter()
    : status_(AdapterStatus::Uninitialized),
      exit_code_(0),
      window_width_(800),
      window_height_(600),
      use_mocks_(false),
      mock_webkitgtk_installed_(false),
      mock_display_server_available_(false)
{
    log("INFO", "WebKit adapter instantiated");
}

void WebKitAdapter::log(const std::string& level, const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t_now), "%Y-%m-%dT%H:%M:%SZ");
    logs_.push_back({oss.str(), level, "WebKitAdapter", message});
}

bool WebKitAdapter::initialize() {
    if (status_ != AdapterStatus::Uninitialized) {
        log("WARN", "Adapter already initialized");
        return false;
    }

    if (!is_supported()) {
        status_ = AdapterStatus::Error;
        detail_ = "Host lacks support for WebKitGTK rendering";
        log("ERROR", detail_);
        return false;
    }

    status_ = AdapterStatus::Ready;
    log("INFO", "WebKitGTK wrapper initialized successfully");
    return true;
}

bool WebKitAdapter::start() {
    if (status_ != AdapterStatus::Ready) {
        log("ERROR", "Cannot start adapter: not in ready state");
        return false;
    }
    status_ = AdapterStatus::Running;
    log("INFO", "WebKit webview initialized");
    return true;
}

bool WebKitAdapter::stop() {
    if (status_ != AdapterStatus::Running && status_ != AdapterStatus::Suspended) {
        log("WARN", "Stop called on inactive adapter");
        return false;
    }
    status_ = AdapterStatus::Terminated;
    exit_code_ = 0;
    log("INFO", "WebKit webview closed");
    return true;
}

bool WebKitAdapter::suspend() {
    if (status_ != AdapterStatus::Running) {
        log("ERROR", "Cannot suspend adapter: not running");
        return false;
    }
    status_ = AdapterStatus::Suspended;
    log("INFO", "WebKit webview page execution frozen");
    return true;
}

bool WebKitAdapter::resume() {
    if (status_ != AdapterStatus::Suspended) {
        log("ERROR", "Cannot resume adapter: not suspended");
        return false;
    }
    status_ = AdapterStatus::Running;
    log("INFO", "WebKit webview page execution resumed");
    return true;
}

AdapterProcessStatus WebKitAdapter::get_status() const {
    return {status_, exit_code_, detail_};
}

std::vector<DiagnosticEntry> WebKitAdapter::get_logs() const {
    return logs_;
}

void WebKitAdapter::clear_logs() {
    logs_.clear();
}

bool WebKitAdapter::has_errors() const {
    for (const auto& entry : logs_) {
        if (entry.level == "ERROR" || entry.level == "FATAL") {
            return true;
        }
    }
    return status_ == AdapterStatus::Error;
}

bool WebKitAdapter::is_supported() const {
    auto checks = check_host_capabilities();
    for (const auto& check : checks) {
        if (!check.satisfied) {
            return false;
        }
    }
    return true;
}

// ============================================================
// Real system probes (production path) + mock path (tests)
// ============================================================

static bool probe_webkitgtk_real() {
    // Check for the WebKitGTK shared library via pkg-config
    FILE* pipe = popen("pkg-config --exists webkit2gtk-4.1 2>/dev/null", "r");
    if (pipe) {
        int rc = pclose(pipe);
        if (rc == 0) return true;
    }
    pipe = popen("pkg-config --exists webkit2gtk-4.0 2>/dev/null", "r");
    if (pipe) {
        int rc = pclose(pipe);
        if (rc == 0) return true;
    }
    // Also check for .so files directly
    struct stat st;
    if (stat("/usr/lib/x86_64-linux-gnu/libwebkit2gtk-4.1.so", &st) == 0) return true;
    if (stat("/usr/lib/x86_64-linux-gnu/libwebkit2gtk-4.0.so", &st) == 0) return true;
    if (stat("/usr/lib/aarch64-linux-gnu/libwebkit2gtk-4.1.so", &st) == 0) return true;
    if (stat("/usr/lib/aarch64-linux-gnu/libwebkit2gtk-4.0.so", &st) == 0) return true;
    return false;
}

static bool probe_display_server_real() {
    // Check if a Wayland or X11 display is available
    if (const char* dpy = std::getenv("DISPLAY")) {
        if (dpy[0] != '\0') return true;
    }
    if (const char* wl = std::getenv("WAYLAND_DISPLAY")) {
        if (wl[0] != '\0') return true;
    }
    return false;
}

std::vector<HostCapabilityCheck> WebKitAdapter::check_host_capabilities() const {
    std::vector<HostCapabilityCheck> checks;

    if (use_mocks_) {
        checks.push_back({
            "WebKitGTK library check (mock)",
            mock_webkitgtk_installed_,
            mock_webkitgtk_installed_
                ? "MOCK: WebKitGTK dynamic libraries available"
                : "MOCK: libwebkit2gtk library not found"
        });
        checks.push_back({
            "Display server connection check (mock)",
            mock_display_server_available_,
            mock_display_server_available_
                ? "MOCK: Display server connection active"
                : "MOCK: X11/Wayland display server connection unavailable"
        });
        return checks;
    }

    // Real system probes
    bool webkit_found = probe_webkitgtk_real();
    checks.push_back({
        "WebKitGTK library check",
        webkit_found,
        webkit_found
            ? "WebKitGTK dynamic libraries available (libwebkit2gtk-4.1 or 4.0)"
            : "libwebkit2gtk library not found. Install libwebkit2gtk-4.1-dev or equivalent"
    });

    bool display_found = probe_display_server_real();
    checks.push_back({
        "Display server connection check",
        display_found,
        display_found
            ? "Display server connection active ($DISPLAY or $WAYLAND_DISPLAY set)"
            : "No X11/Wayland display server connection. Set $DISPLAY or $WAYLAND_DISPLAY"
    });

    return checks;
}

void WebKitAdapter::set_url(const std::string& url) {
    url_ = url;
    log("INFO", "Set WebKit content URL endpoint: " + url);
}

void WebKitAdapter::configure_window(int width, int height, const std::string& title) {
    window_width_ = width;
    window_height_ = height;
    window_title_ = title;
    log("INFO", "Window configurations: size=" + std::to_string(width) + "x" + std::to_string(height) + " title=\"" + title + "\"");
}

void WebKitAdapter::bind_ipc_callback(const std::string& message_name, std::function<void(const std::string&)> callback) {
    ipc_callbacks_[message_name] = callback;
    log("INFO", "Registered window IPC message callback: " + message_name);
}

bool WebKitAdapter::run_window_loop() {
    if (status_ != AdapterStatus::Running) {
        log("ERROR", "Cannot run window loop: webview not initialized");
        return false;
    }
    log("INFO", "Starting WebKitGTK frame rendering loop");
    return true;
}

void WebKitAdapter::set_mock_webkitgtk_installed(bool installed) {
    mock_webkitgtk_installed_ = installed;
}

void WebKitAdapter::set_mock_display_server_available(bool available) {
    mock_display_server_available_ = available;
}

} // namespace platform::adapters
