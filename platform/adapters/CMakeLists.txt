# platform/adapters: Substrate Adapter Layer Foundation
# Architecture reference: docs/architecture/SUBSTRATE_MODEL.md (Section 6)

add_library(platform_adapters STATIC
    darling_adapter.cpp
    qemu_adapter.cpp
    electron_adapter.cpp
    webkit_adapter.cpp
)

target_include_directories(platform_adapters PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(platform_adapters PUBLIC platform_common)
