# input: Keyboard, mouse, and hotkey forwarding
add_library(host_input STATIC placeholder.cpp)
target_include_directories(host_input PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
