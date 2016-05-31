include (FindPackageHandleStandardArgs)
 
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_path (SPARSEHASH_INCLUDE_DIR google/sparse_hash_map)
 
    find_package_handle_standard_args (sparsehash DEFAULT_MSG SPARSEHASH_INCLUDE_DIR)
endif()
