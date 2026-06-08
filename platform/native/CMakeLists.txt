# Native Module Compatibility Engine
# Phase 4B: Governed native module discovery, ABI verification, caching, and build dispatch.
# Architecture reference: docs/architecture/Native Module Compatibility Infrastructure Plan v3
# Dependencies: compatdb (for registry + ABI manifest access), nlohmann/json

add_library(platform_native STATIC
    include/native/native_types.hpp
    native_abi_db.cpp
    native_cache.cpp
    native_discovery.cpp
    native_builder.cpp
)
target_link_libraries(platform_native PUBLIC compatdb OpenSSL::Crypto)
target_include_directories(platform_native PUBLIC include ${CMAKE_SOURCE_DIR}/third_party)
target_include_directories(platform_native PRIVATE
    ${CMAKE_SOURCE_DIR}/compat-db/include
)
