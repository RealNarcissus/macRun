# Compatibility database query CLI tool
add_executable(compdb-query main.cpp)
target_link_libraries(compdb-query PRIVATE compatdb)
