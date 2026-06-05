# unit: Subsystem-isolated unit tests
# Tests here must not depend on other subsystems' internals.

# Existing capability detection unit tests
add_executable(unit_tests detector_tests.cpp)
target_link_libraries(unit_tests PRIVATE platform_detector)
target_include_directories(unit_tests PRIVATE ${CMAKE_SOURCE_DIR}/platform/detector)

# New runtime adapter foundation unit tests
add_executable(adapter_tests adapter_tests.cpp)
target_link_libraries(adapter_tests PRIVATE platform_adapters)
target_include_directories(adapter_tests PRIVATE 
    ${CMAKE_SOURCE_DIR}/platform/adapters
    ${CMAKE_SOURCE_DIR}/platform/common
)
