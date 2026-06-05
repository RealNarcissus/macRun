# bridge: macOS-side bridge daemon (Tier 4B guest)
# Architecture reference: docs/architecture/ARCHITECTURE_V6.md
#   - "Bridge Daemon Contract"
# Responsibilities: window capture, event injection, guest-side integration.
# Never manages Linux compositor state or owns execution policy.
add_library(vm_bridge STATIC placeholder.cpp)
target_include_directories(vm_bridge PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
