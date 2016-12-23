project(Bosen)

add_compile_options(-std=c++11 -fPIC -Wall -Wno-sign-compare)

file(GLOB_RECURSE Bosen_SOURCE_FILES *.[ch]pp)

add_library(petuum_bosen ${Bosen_SOURCE_FILES})

# uncomment to enable HDFS.
set(USE_HDFS 0)

# Required libraries
find_package(Boost 1.54.0 REQUIRED COMPONENTS thread system)
find_package(Eigen3 REQUIRED)
find_package(Gflags REQUIRED)
find_package(Glog 0.3.4 REQUIRED)
find_package(LevelDB REQUIRED)
find_package(Libconfig REQUIRED)
find_package(Snappy REQUIRED)
find_package(sparsehash 2.0.2 REQUIRED)
find_package(Tcmalloc REQUIRED)
find_package(Threads REQUIRED)
find_package(Yaml REQUIRED)
find_package(ZeroMQ 3.2.0 REQUIRED)

if(${USE_HDFS})
    find_package(HDFS REQUIRED)
    find_package(JVM REQUIRED)
    add_definitions(-DHAS_HADOOP)
    target_include_directories(petuum_bosen PUBLIC ${HDFS_INCLUDE_DIRS})
    target_link_libraries(petuum_bosen PUBLIC
            ${HDFS_LIBRARY} ${JVM_LIBRARIES})
endif(${USE_HDFS})

set(Bosen_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
        CACHE PATH "Path to Bosen include files.")

MESSAGE(STATUS "Third Party Dir: ${third_INCLUDE_DIRS}")
MESSAGE(STATUS "Bosen Includ Dir: ${Bosen_INCLUDE_DIRS}")
MESSAGE(STATUS "ZMQ Test Dir: ${cppzmq_INCLUDE_DIRS}")
MESSAGE(STATUS "HDFS and JVM: ${HDFS_LIBRARIES}, ${JVM_LIBRARIES}")
target_include_directories(petuum_bosen PUBLIC
        ${Bosen_INCLUDE_DIRS} ${third_INCLUDE_DIRS}
        ${ZeroMQ_INCLUDE_DIRS} ${GLOG_INCLUDE_DIRS}
        ${YAML_INCLUDE_DIRS} ${SNAPPY_INCLUDE_DIRS}
        ${SPARSEHASH_INCLUDE_DIR}  ${LevelDB_INCLUDE_PATH} ${GFLAGS_INCLUDE_DIRS}
        ${LIBCONFIG_INCLUDE_DIR}
        ${Tcmalloc_INCLUDE_DIR} ${EIGEN3_INCLUDE_DIR})

target_link_libraries(petuum_bosen PUBLIC
        rt nsl
        ${ZeroMQ_LIBRARIES}
        ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT}
        ${GLOG_LIBRARIES}  ${Tcmalloc_LIBRARIES}
        ${LIBCONFIG_LIBRARIES} ${SNAPPY_LIBRARIES} ${Boost_LIBRARIES}
        ${YAML_LIBRARIES} ${LevelDB_LIBRARY} ${GFLAGS_LIBRARIES})
