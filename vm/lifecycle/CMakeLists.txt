# lifecycle: VM start/suspend/resume orchestration
add_library(vm_lifecycle STATIC placeholder.cpp)
target_include_directories(vm_lifecycle PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
