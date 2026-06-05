# arm64: Apple Silicon binary translation (Tier 3)
# Architecture reference: docs/architecture/ARCHITECTURE_V6.md
#   - "Tier 3 — ARM64 Translation"
#   - "Translation Layer Contract"
add_library(runtime_arm64 STATIC placeholder.cpp)
target_include_directories(runtime_arm64 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(runtime_arm64 PUBLIC runtime_common)
