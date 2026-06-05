# detector: Capability Detection Engine
# Architecture reference: docs/architecture/ARCHITECTURE_V6.md
#   - "Capability Detection Engine" section
#   - "Detection Pipeline" stages 1-5
# Subsystems: bundle analysis, framework fingerprinting, arch analysis, scoring
# Five-stage pipeline: bundle → fingerprint → macho → score → strategy

add_library(platform_detector STATIC
    bundle.cpp
    fingerprint.cpp
    macho.cpp
    entitlements.cpp
    scoring.cpp
    strategy.cpp
    pipeline.cpp
    serialize.cpp
)

target_include_directories(platform_detector PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(platform_detector PUBLIC platform_common)
