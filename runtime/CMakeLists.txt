# Runtime Layer: Execution backends for macOS applications
# Architecture reference: docs/architecture/ARCHITECTURE_V6.md
#   - "Execution Tier Model" section
#   - "Runtime Shim Contract"
#   - "Translation Layer Contract"

add_subdirectory(darling)
add_subdirectory(shims)
if(ENABLE_ARM64_TRANSLATION)
    add_subdirectory(arm64)
endif()
if(ENABLE_COCOA_COMPAT)
    add_subdirectory(cocoa)
endif()
add_subdirectory(common)
