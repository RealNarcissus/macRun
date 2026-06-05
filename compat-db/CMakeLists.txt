# Compatibility Database — compdb library and query CLI
# Architecture reference: docs/architecture/ARCHITECTURE_V6.md — "compat-db Contract"
# Pure data layer. Never launches applications, performs runtime analysis,
# or owns execution logic.
# Uses nlohmann/json (vendored in third_party/) for robust JSON handling.

add_library(compatdb STATIC
    src/validator.cpp
    src/database.cpp
)

target_include_directories(compatdb PUBLIC include)
target_include_directories(compatdb PRIVATE ${CMAKE_SOURCE_DIR}/third_party)

if(BUILD_TOOLING)
    add_subdirectory(tool)
endif()
