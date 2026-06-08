# macrun CLI executable
add_executable(macrun-cli main.cpp provision.cpp)
target_link_libraries(macrun-cli PRIVATE macrun platform_native compatdb)
