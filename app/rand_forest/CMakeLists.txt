project(rand_forest_main)

add_compile_options(-std=c++11 -Wall)

file(GLOB_RECURSE RAND_FOREST_SOURCE_FILES src/*.[ch]pp)

add_executable(rand_forest_main ${RAND_FOREST_SOURCE_FILES})

target_include_directories(
    rand_forest_main PUBLIC ${bosen_INCLUDE_DIRS})

target_link_libraries(rand_forest_main petuum_bosen)
target_link_libraries(rand_forest_main ${SNAPPY_LIBRARIES} ${LevelDB_LIBRARY}) 
