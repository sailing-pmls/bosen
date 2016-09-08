project(mlr)

add_compile_options(-std=c++11 -Wall -Wno-sign-compare)

file(GLOB MLR_SOURCE_FILES src/*.[ch]pp)
add_executable(mlr_main ${MLR_SOURCE_FILES})
target_include_directories(
    mlr_main PUBLIC ${bosen_INCLUDE_DIRS})
target_link_libraries(mlr_main petuum_bosen)

file(GLOB MLR_GENDATA_SOURCE_FILES src/tools/*.[ch]pp)
add_executable(mlr_gendata_main ${MLR_GENDATA_SOURCE_FILES})
target_include_directories(
    mlr_gendata_main PUBLIC ${bosen_INCLUDE_DIRS})
target_link_libraries(mlr_gendata_main petuum_bosen)
