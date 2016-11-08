project(demo_lr)

add_compile_options(-std=c++11 -Wall -Wno-sign-compare)

file(GLOB DEMOLR_SOURCE_FILES src/*.[ch]pp)
add_executable(lr_main ${DEMOLR_SOURCE_FILES})
target_include_directories(
        lr_main PUBLIC ${bosen_INCLUDE_DIRS})
target_link_libraries(lr_main petuum_bosen)

