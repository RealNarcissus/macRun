# wayland: Wayland surface management and compositor integration
add_library(host_wayland STATIC placeholder.cpp)
target_include_directories(host_wayland PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
