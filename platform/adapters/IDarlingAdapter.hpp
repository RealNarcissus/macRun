#pragma once
#include "interfaces.hpp"

namespace platform::adapters {

class IDarlingAdapter : public virtual ILifecycle, 
                        public virtual IDiagnostics, 
                        public virtual ICapability {
public:
    virtual ~IDarlingAdapter() = default;
    virtual void configure_prefix(const std::string& prefix_path) = 0;
    virtual void set_environment(const std::unordered_map<std::string, std::string>& env) = 0;
    virtual bool launch_binary(const std::string& binary_path, const std::vector<std::string>& args) = 0;
};

} // namespace platform::adapters
