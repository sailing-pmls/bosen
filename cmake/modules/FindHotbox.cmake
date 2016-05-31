# - Try to find Hotbox headers and libraries
#
# Usage of this module as follows:
#
#     find_package(Hotbox)
#
# Variables used by this module, they can change the default behaviour and need
# to be set before calling find_package:
#
#  Hotbox_ROOT_DIR  Set this variable to the root installation of
#                            Hotbox if the module has problems finding
#                            the proper installation path.
#
# Variables defined by this module:
#
#  HOTBOX_FOUND              System has Hotbox libs/headers
#  Hotbox_LIBRARIES          The Hotbox libraries
#  Hotbox_INCLUDE_DIRS       The location of Hotbox headers

find_path(Hotbox_ROOT_DIR
    NAMES include/hotbox/client/hb_client.hpp
)

find_library(Hotbox_LIBRARIES
    NAMES hotbox libhotbox
    HINTS ${Hotbox_ROOT_DIR}/lib
)

find_path(Hotbox_INCLUDE_DIRS1
    NAMES hotbox/client/hb_client.hpp
    HINTS ${Hotbox_ROOT_DIR}/include
)

find_path(Hotbox_INCLUDE_DIRS2
    NAMES util/all.hpp
    HINTS ${Hotbox_ROOT_DIR}/include/hotbox
)

find_path(Hotbox_INCLUDE_DIRS3
    NAMES util/proto/util.pb.h
    HINTS ${Hotbox_ROOT_DIR}/include/hotbox/build
)

set(Hotbox_INCLUDE_DIRS ${Hotbox_INCLUDE_DIRS1}
  ${Hotbox_INCLUDE_DIRS2}
  ${Hotbox_INCLUDE_DIRS3})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Hotbox DEFAULT_MSG
    Hotbox_LIBRARIES
)

mark_as_advanced(
    Hotbox_ROOT_DIR
    Hotbox_LIBRARIES
    Hotbox_INCLUDE_DIRS
)
