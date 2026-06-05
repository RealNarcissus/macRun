# macrun: Application orchestration and tier selection
# Architecture reference: docs/architecture/ARCHITECTURE_V6.md
#   - "macrun Contract": Inputs/outputs/never-does
# Contract: Accepts .app/.dmg/Mach-O binary, outputs execution strategy.
# Never implements rendering, translation, or compositor logic.

add_library(macrun STATIC
    input.cpp
    backend.cpp
    orchestrator.cpp
    serialize.cpp
)

target_include_directories(macrun PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(macrun PUBLIC platform_detector platform_adapters compatdb)
