#pragma once
#include "interfaces.hpp"

namespace platform::adapters {

class IElectronAdapter : public virtual ILifecycle, 
                         public virtual IDiagnostics, 
                         public virtual ICapability {
public:
    virtual ~IElectronAdapter() = default;
    virtual void resolve_runtime_version(const std::string& version) = 0;
    virtual void set_asar_path(const std::string& asar_path) = 0;
    virtual void inject_preload(const std::string& preload_script_path) = 0;
    virtual bool execute() = 0;
    
    // Shim directory — where preload .js scripts are installed
    virtual void set_shims_dir(const std::string& dir) = 0;
    
    // Returns the resolved runtime binary path after execute()
    virtual std::string get_resolved_runtime_path() const = 0;
    
    // Scan extracted app directory for Darwin-native .node modules
    // Returns true if all .node files are Linux-safe (ELF or no native modules)
    virtual bool detect_native_modules(const std::string& app_dir) = 0;
    
    // Returns the PID of the spawned child process (0 if not running)
    virtual int get_child_pid() const = 0;
};

} // namespace platform::adapters
