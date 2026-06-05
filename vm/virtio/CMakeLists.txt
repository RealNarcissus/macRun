# virtio: Shared transport layer between host and guest
add_library(vm_virtio STATIC placeholder.cpp)
target_include_directories(vm_virtio PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
