#pragma once
#include "interfaces.hpp"

namespace platform::adapters {

class IWebKitAdapter : public virtual ILifecycle, 
                       public virtual IDiagnostics, 
                       public virtual ICapability {
public:
    virtual ~IWebKitAdapter() = default;
    virtual void set_url(const std::string& url) = 0;
    virtual void configure_window(int width, int height, const std::string& title) = 0;
    virtual void bind_ipc_callback(const std::string& message_name, std::function<void(const std::string&)> callback) = 0;
    virtual bool run_window_loop() = 0;
};

} // namespace platform::adapters
