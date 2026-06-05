# Host Layer: Linux-side windowing and UX integration
# Architecture reference: docs/architecture/ARCHITECTURE_V6.md
#   - "Host Responsibilities" section
#   - "Host Proxy Contract"
#   - "Security and Trust Boundaries" diagram
# Responsibilities: Wayland surfaces, clipboard bridge, notifications,
# input forwarding, VM integration. The host never interprets macOS APIs.

add_subdirectory(proxy)
add_subdirectory(wayland)
add_subdirectory(input)
add_subdirectory(audio)
add_subdirectory(integration)
