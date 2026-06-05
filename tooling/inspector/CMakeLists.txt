# inspector: Bundle analysis and capability probing tool
# Uses the Capability Detection Engine to inspect .app bundles
add_executable(inspector main.cpp)
target_link_libraries(inspector PRIVATE platform_detector)
