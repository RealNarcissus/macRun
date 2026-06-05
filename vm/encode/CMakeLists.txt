# encode: Frame encoding for VirtIO transport
add_library(vm_encode STATIC placeholder.cpp)
target_include_directories(vm_encode PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
