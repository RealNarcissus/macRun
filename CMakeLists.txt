cmake_minimum_required(VERSION 3.20)
project(macos-runtime-platform LANGUAGES C CXX
        DESCRIPTION "Hybrid macOS application runtime platform for Linux"
        VERSION 0.1.0)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_TESTS "Build test suite" ON)
option(BUILD_TOOLING "Build development tooling" ON)
option(ENABLE_VM_SUPPORT "Build VM-assisted execution subsystem" ON)
option(ENABLE_ARM64_TRANSLATION "Build ARM64 translation layer" ON)
option(ENABLE_COCOA_COMPAT "Build Cocoa compatibility layer" ON)

# --- Architecture-aligned subsystem boundaries ---
# Each subsystem is an independent build target. Subsystems communicate
# only through explicit interface contracts (headers/protocols).
# No subsystem may directly depend on another subsystem's internals.

# Compatibility Database: Metadata, schemas, validation, query tooling
add_subdirectory(compat-db)

# Platform Layer: Launcher and capability detection
add_subdirectory(platform)

# Runtime Layer: Execution backends (Darling, shims, translation, Cocoa)
add_subdirectory(runtime)

# VM Layer: macOS guest integration (bridge, capture, encoding, lifecycle)
if(ENABLE_VM_SUPPORT)
    add_subdirectory(vm)
endif()

# Host Layer: Linux-side windowing and UX integration
add_subdirectory(host)

# Tooling: Development and diagnostic utilities
if(BUILD_TOOLING)
    add_subdirectory(tooling)
endif()

# Tests: Unit, integration, compatibility, and performance validation
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
