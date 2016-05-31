# - Try to find Jvm
#
# The following variables are optionally searched for defaults
#  JVM_ROOT_DIR:            Base directory containing libhdfs.so 
#
# The following are set after configuration is done:
#  JVM_LIBRARIES

include(FindPackageHandleStandardArgs)

#set(JVM_ROOT_DIR "/usr/lib64")
#set(JVM_ROOT_DIR "/usr/java/latest/jre/lib/amd64/server")

#find_path(JVM_INCLUDE_DIR yaml-cpp/yaml.h
#        PATHS ${JVM_ROOT_DIR})

find_library(JVM_LIBRARY jvm)
set(JVM_LIBRARIES ${JVM_LIBRARY})

#find_package_handle_standard_args(JVM DEFAULT_MSG
#        JVM_INCLUDE_DIR JVM_LIBRARY)

#if(JVM_FOUND)
#    set(JVM_INCLUDE_DIRS ${JVM_INCLUDE_DIR})
#    set(JVM_LIBRARIES ${JVM_LIBRARY})
#endif()
