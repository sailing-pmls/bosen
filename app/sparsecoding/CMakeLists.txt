project(sparsecoding)

add_compile_options(-std=c++11 -Wall)

file(GLOB_RECURSE SPARSECODING_SOURCE_FILES src/*.[ch]pp)

add_executable(sparsecoding_main ${SPARSECODING_SOURCE_FILES})

target_include_directories(
    sparsecoding_main PUBLIC ${bosen_INCLUDE_DIRS})

target_link_libraries(sparsecoding_main petuum_bosen)
target_link_libraries(sparsecoding_main ${SNAPPY_LIBRARIES} ${LevelDB_LIBRARY}) 
