project(dnn_speech_main)

add_compile_options(-std=c++11 -Wall)

file(GLOB DNN_SPEECH_SOURCE_FILES src/dnn/*.[ch]pp)
file(GLOB DNN_SPEECH_GENDATA_SOURCE_FILES src/dnn/*.[ch]pp)

add_executable(dnn_speech_main ${DNN_SPEECH_SOURCE_FILES})
target_include_directories(
    dnn_speech_main PUBLIC ${bosen_INCLUDE_DIRS})
target_link_libraries(dnn_speech_main petuum_bosen)
target_link_libraries(dnn_speech_main ${SNAPPY_LIBRARIES} ${LevelDB_LIBRARY}) 

add_executable(dnn_speech_gendata_main ${DNN_SPEECH_GENDATA_SOURCE_FILES})
target_include_directories(
    dnn_speech_gendata_main PUBLIC ${bosen_INCLUDE_DIRS})
target_link_libraries(dnn_speech_gendata_main petuum_bosen)
target_link_libraries(dnn_speech_gendata_main ${SNAPPY_LIBRARIES} ${LevelDB_LIBRARY}) 
