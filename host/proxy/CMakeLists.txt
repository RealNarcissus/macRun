# proxy: Host proxy layer (Tier 4B host side)
# Architecture reference: docs/architecture/ARCHITECTURE_V6.md
#   - "Host Proxy Contract"
# Responsibilities: Wayland windows, framebuffer presentation, input forwarding,
# hotkey registration, clipboard sync, notification bridge, audio routing,
# VM lifecycle management.
# Never interprets macOS APIs, performs Mach emulation, or manages Cocoa behavior.
add_library(host_proxy STATIC placeholder.cpp)
target_include_directories(host_proxy PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(host_proxy PUBLIC host_wayland host_input host_audio host_integration)
