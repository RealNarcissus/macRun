# integration: Cross-subsystem contract-level integration tests
add_executable(integration_tests macrun_tests.cpp)
target_link_libraries(integration_tests PRIVATE macrun)
target_include_directories(integration_tests PRIVATE ${CMAKE_SOURCE_DIR}/platform/macrun)

# electron_launch_tests: Phase 3B Electron adapter execution tests
add_executable(electron_launch_tests electron_launch_tests.cpp)
target_link_libraries(electron_launch_tests PRIVATE platform_adapters)
target_include_directories(electron_launch_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/platform/adapters
    ${CMAKE_SOURCE_DIR}/platform/common
)

# degradation_tests: Degradation governance & confidence classification tests
add_executable(degradation_tests degradation_tests.cpp)
target_link_libraries(degradation_tests PRIVATE platform_adapters)
target_include_directories(degradation_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/platform/adapters
    ${CMAKE_SOURCE_DIR}/platform/common
)

# normalization_tests: Governed Electron API normalization tests
add_executable(normalization_tests normalization_tests.cpp)
target_link_libraries(normalization_tests PRIVATE platform_adapters)
target_include_directories(normalization_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/platform/adapters
    ${CMAKE_SOURCE_DIR}/platform/common
)
