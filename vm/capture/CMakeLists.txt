# capture: Per-window framebuffer capture
# Architecture reference: docs/architecture/ARCHITECTURE_V6.md
#   - "Capture Strategy" section
add_library(vm_capture STATIC placeholder.cpp)
target_include_directories(vm_capture PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
