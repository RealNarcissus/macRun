# VM Layer: macOS guest integration for Tier 4B
# Architecture reference: docs/architecture/ARCHITECTURE_V6.md
#   - "VM-Assisted Architecture" section
#   - "Host Proxy Contract"
#   - "Bridge Daemon Contract"
# Per-window capture, frame encoding, bridge daemon, lifecycle management.

add_subdirectory(bridge)
add_subdirectory(capture)
add_subdirectory(encode)
add_subdirectory(lifecycle)
add_subdirectory(virtio)
