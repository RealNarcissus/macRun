# platform/common: Shared types and utilities for the platform layer
# Header-only library — types.hpp provides all common definitions
add_library(platform_common INTERFACE)
target_include_directories(platform_common INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
