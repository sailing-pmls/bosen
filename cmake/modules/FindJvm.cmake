# - Try to find Jvm
#
# The following variables are optionally searched for defaults
#  JVM_ROOT_DIR:            Base directory containing libhdfs.so 
#
# The following are set after configuration is done:
#  JVM_LIBRARIES


find_library(JAVA_LIBRARY NAMES jvm HINTS $ENV{JAVA_HOME}/jre/lib/amd64/server)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(JVM DEFAULT_MSG
        JAVA_LIBRARY)

if(JVM_FOUND)
    set(JVM_LIBRARIES ${JAVA_LIBRARY})
endif()
