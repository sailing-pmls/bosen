project(nmf)

add_compile_options(-std=c++11 -Wall -Wno-sign-compare)

file(GLOB_RECURSE NMF_SOURCE_FILES src/*.[ch]pp)

add_executable(nmf_main ${NMF_SOURCE_FILES})

target_include_directories(
    nmf_main PUBLIC ${bosen_INCLUDE_DIRS})
target_include_directories(
    nmf_main PUBLIC ${EIGEN3_INCLUDE_DIR})

target_link_libraries(nmf_main petuum_bosen)
target_link_libraries(nmf_main ${SNAPPY_LIBRARIES} ${LevelDB_LIBRARY})
