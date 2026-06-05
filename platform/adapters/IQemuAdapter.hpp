#pragma once
#include "interfaces.hpp"

namespace platform::adapters {

class IQemuAdapter : public virtual ILifecycle, 
                     public virtual IDiagnostics, 
                     public virtual ICapability {
public:
    virtual ~IQemuAdapter() = default;
    virtual void configure_cpu(const std::string& cpu_model) = 0;
    virtual void configure_memory(uint64_t memory_mb) = 0;
    virtual void set_qmp_socket(const std::string& socket_path) = 0;
    virtual bool boot_vm(const std::string& base_image_path, const std::string& overlay_path) = 0;
    virtual bool execute_command(const std::string& guest_cmd) = 0;
};

} // namespace platform::adapters
