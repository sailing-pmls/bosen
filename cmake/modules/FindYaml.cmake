# - Try to find Yaml
#
# The following variables are optionally searched for defaults
#  YAML_ROOT_DIR:            Base directory where all YAML components are found
#
# The following are set after configuration is done:
#  YAML_LIBRARIES

include(FindPackageHandleStandardArgs)

set(YAML_ROOT_DIR "" CACHE PATH "Folder contains Yaml")

find_path(YAML_INCLUDE_DIR yaml-cpp/yaml.h
        PATHS ${YAML_ROOT_DIR})

find_library(YAML_LIBRARY yaml-cpp)

find_package_handle_standard_args(YAML DEFAULT_MSG
        YAML_INCLUDE_DIR YAML_LIBRARY)

if(YAML_FOUND)
    set(YAML_INCLUDE_DIRS ${YAML_INCLUDE_DIR})
    set(YAML_LIBRARIES ${YAML_LIBRARY})
endif()
