# compat-db validation and query tests
add_executable(compatdb_tests ${CMAKE_SOURCE_DIR}/compat-db/tests/compatdb_tests.cpp)
target_link_libraries(compatdb_tests PRIVATE compatdb)
