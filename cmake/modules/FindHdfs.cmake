# - Try to find Hdfs
##
# The following are set after configuration is done:
#  HDFS_LIBRARIES
#  HDFS_INCLUDE_DIRS


find_path(HDFS_INCLUDE_DIR NAMES hdfs.h  HINTS $ENV{HADOOP_HOME}/include)
find_library(HDFS_LIBRARY NAMES hdfs HINTS $ENV{HADOOP_HOME}/lib/native)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(HDFS DEFAULT_MSG
        HDFS_INCLUDE_DIR HDFS_LIBRARY)

if(HDFS_FOUND)
    set(HDFS_INCLUDE_DIRS ${HDFS_INCLUDE_DIR})
    set(HDFS_LIBRARIES ${HDFS_LIBRARY})
endif()
