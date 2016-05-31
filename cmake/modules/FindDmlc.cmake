# - Try to find Dmlc
#
# The following variables are optionally searched for defaults
#  DMLC_ROOT_DIR:            Base directory containing libdmlc.a
#
# The following are set after configuration is done:
#  DMLC_LIBRARIES

include(FindPackageHandleStandardArgs)

set(DMLC_ROOT_DIR "" CACHE PATH "Folder contains Yaml")

find_library(DMLC_LIBRARY dmlc
  HINTS ${DMLC_ROOT_DIR})
set(DMLC_LIBRARIES ${DMLC_LIBRARY})
