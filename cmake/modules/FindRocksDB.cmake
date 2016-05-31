# - Try to find RocksDB
#
# The following variables are optionally searched for defaults
#  ROCKSDB_ROOT_DIR:            Base directory where all ROCKSDB components are found
#
# The following are set after configuration is done:
#  ROCKSDB_LIBRARIES

include(FindPackageHandleStandardArgs)

set(ROCKSDB_ROOT_DIR "" CACHE PATH "Folder contains RocksDB")

find_path(ROCKSDB_INCLUDE_DIR yaml-cpp/yaml.h
        PATHS ${ROCKSDB_ROOT_DIR})

find_library(ROCKSDB_LIBRARY rocksdb)

find_package_handle_standard_args(ROCKSDB DEFAULT_MSG
        ROCKSDB_INCLUDE_DIR ROCKSDB_LIBRARY)

if(ROCKSDB_FOUND)
    set(ROCKSDB_INCLUDE_DIRS ${ROCKSDB_INCLUDE_DIR})
    set(ROCKSDB_LIBRARIES ${ROCKSDB_LIBRARY})
endif()
