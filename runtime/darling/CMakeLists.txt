# darling: Mach-O loader and syscall compatibility (Tier 1-3)
# Architecture reference: docs/architecture/ARCHITECTURE_V6.md
#   - Tier 1 (CLI Compatibility)
#   - Tier 2 (Lightweight Cocoa Compatibility)
add_library(runtime_darling STATIC placeholder.cpp)
target_include_directories(runtime_darling PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(runtime_darling PUBLIC runtime_common)
