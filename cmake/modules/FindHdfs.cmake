# - Try to find Hdfs
#
# The following variables are optionally searched for defaults
#  HDFS_ROOT_DIR:            Base directory containing libhdfs.so 
#
# The following are set after configuration is done:
#  HDFS_LIBRARIES

include(FindPackageHandleStandardArgs)

#set(HDFS_ROOT_DIR "/usr/lib64")
#set(JVM_ROOT_DIR "/usr/java/latest/jre/lib/amd64/server")

#find_path(HDFS_INCLUDE_DIR yaml-cpp/yaml.h
#        PATHS ${HDFS_ROOT_DIR})

find_library(HDFS_LIBRARY hdfs)
set(HDFS_LIBRARIES ${HDFS_LIBRARY})

#find_package_handle_standard_args(HDFS DEFAULT_MSG
#        HDFS_INCLUDE_DIR HDFS_LIBRARY)

#if(HDFS_FOUND)
#    set(HDFS_INCLUDE_DIRS ${HDFS_INCLUDE_DIR})
#    set(HDFS_LIBRARIES ${HDFS_LIBRARY})
#endif()
