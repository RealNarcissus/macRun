# runtime/common: Shared types and utilities for the runtime layer
# Includes substrate lifecycle management — header-only library
# Architecture reference: docs/architecture/SUBSTRATE_MODEL.md
add_library(runtime_common STATIC
    lifecycle.cpp
)

target_include_directories(runtime_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(runtime_common PUBLIC stdc++fs)
