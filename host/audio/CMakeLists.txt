# audio: Audio routing from guest to host
add_library(host_audio STATIC placeholder.cpp)
target_include_directories(host_audio PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
