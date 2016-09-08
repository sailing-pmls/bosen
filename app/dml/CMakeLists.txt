project(dml)

add_compile_options(-std=c++11 -Wall -Wno-sign-compare)

file(GLOB_RECURSE DML_SOURCE_FILES src/*.[ch]pp)

add_executable(dml_main ${DML_SOURCE_FILES})

target_include_directories(
    dml_main PUBLIC ${bosen_INCLUDE_DIRS})

target_link_libraries(dml_main petuum_bosen)
target_link_libraries(dml_main ${SNAPPY_LIBRARIES} ${LevelDB_LIBRARY}) 
