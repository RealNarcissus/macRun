# Substrate lifecycle integration tests
add_executable(substrate_tests lifecycle_tests.cpp)
target_link_libraries(substrate_tests PRIVATE runtime_common)
