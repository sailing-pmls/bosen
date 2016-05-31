# - Try to find PROTOBUF
#
# The following are set after configuration is done:
#  PROTOBUF_LIBRARIES

include(FindPackageHandleStandardArgs)
find_library(PROTOBUF_LIBRARY protobuf)
set(PROTOBUF_LIBRARIES ${PROTOBUF_LIBRARY})
