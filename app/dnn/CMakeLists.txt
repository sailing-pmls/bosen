project(dnn)

add_compile_options(-std=c++11 -Wall -Wno-sign-compare)

file(GLOB DNN_SOURCE_FILES src/dnn/*.[ch]pp)
file(GLOB DNN_PREDICT_SOURCE_FILES src/dnn_predict/*.[ch]pp)
file(GLOB DNN_GENDATA_SOURCE_FILES src/tools/*.[ch]pp)

add_executable(dnn_main ${DNN_SOURCE_FILES})
target_include_directories(
    dnn_main PUBLIC ${bosen_INCLUDE_DIRS})
target_link_libraries(dnn_main petuum_bosen)
target_link_libraries(dnn_main ${SNAPPY_LIBRARIES} ${LevelDB_LIBRARY}) 

add_executable(dnn_predict_main ${DNN_PREDICT_SOURCE_FILES})
target_include_directories(
    dnn_predict_main PUBLIC ${bosen_INCLUDE_DIRS})
target_link_libraries(dnn_predict_main petuum_bosen)
target_link_libraries(dnn_predict_main ${SNAPPY_LIBRARIES} ${LevelDB_LIBRARY}) 

add_executable(dnn_gendata_main ${DNN_GENDATA_SOURCE_FILES})
target_include_directories(
    dnn_gendata_main PUBLIC ${bosen_INCLUDE_DIRS})
target_link_libraries(dnn_gendata_main petuum_bosen)
target_link_libraries(dnn_gendata_main ${SNAPPY_LIBRARIES} ${LevelDB_LIBRARY}) 
