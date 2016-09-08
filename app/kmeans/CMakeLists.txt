project(kmeans)

add_compile_options(-std=c++11 -Wall -Wno-sign-compare)

file(GLOB_RECURSE KMEANS_SOURCE_FILES src/*.[ch]pp)

add_executable(kmeans_main ${KMEANS_SOURCE_FILES})

target_include_directories(
    kmeans_main PUBLIC ${bosen_INCLUDE_DIRS})

target_link_libraries(kmeans_main petuum_bosen)
target_link_libraries(kmeans_main ${SNAPPY_LIBRARIES} ${LevelDB_LIBRARY}) 
