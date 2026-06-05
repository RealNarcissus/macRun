# shims: Electron/Tauri platform adaptation (Tier 0)
# Architecture reference: docs/architecture/ARCHITECTURE_V6.md
#   - "Tier 0 — Runtime Substitution"
#   - "Runtime Shim Contract"
# Contract: Electron adaptation, Tauri adaptation, API substitution.
# Never emulates macOS kernels or implements full Cocoa semantics.
#
# Shim scripts (.js) are installed to the build output directory and can be
# copied to ~/.cache/macrun/shims/ via the install.sh script.

add_library(runtime_shims STATIC placeholder.cpp)
target_include_directories(runtime_shims PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(runtime_shims PUBLIC runtime_common)

# Install shim scripts to the build output directory for packaging
set(SHIM_FILES
    preload-main.js
    env-normalizer.js
    platform-normalizer.js
    path-mapper.js
    disable-sparkle.js
    disable-gpu.js
    notification-bridge.js
    clipboard-bridge.js
    shell-integration.js
    electron-normalization-registry.js
    renderer-diag.js
    main-diag.js
    install.sh
)

foreach(SHIM ${SHIM_FILES})
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/${SHIM}
        ${CMAKE_CURRENT_BINARY_DIR}/${SHIM}
        COPYONLY
    )
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/${SHIM}
        DESTINATION share/macrun/shims
    )
endforeach()
