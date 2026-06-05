# integration: Clipboard, notifications, and UX glue
add_library(host_integration STATIC placeholder.cpp)
target_include_directories(host_integration PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
