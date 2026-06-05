# cocoa: Lightweight AppKit compatibility (Tier 2)
# Architecture reference: docs/architecture/ARCHITECTURE_V6.md
#   - "Tier 2 — Lightweight Cocoa Compatibility"
add_library(runtime_cocoa STATIC placeholder.cpp)
target_include_directories(runtime_cocoa PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(runtime_cocoa PUBLIC runtime_common runtime_darling)
