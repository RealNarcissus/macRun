// Native Module Builder — Header
// Sandboxed build pipeline for allowlisted native modules.
// Only called from 'macrun provision' — NEVER from the launch path.
#pragma once
#include <native/native_types.hpp>
#include <string>

namespace platform::native {

bool is_bubblewrap_available();

BuildResult build_in_sandbox(const BuildSpec& spec, const std::string& output_dir);

} // namespace platform::native
