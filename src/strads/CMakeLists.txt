cmake_minimum_required(VERSION 2.8.7)
project(FASTNET CXX)
set(FASTNET_VERSION_MAJOR 2)
set(FASTNET_VERSION_MINOR 0)
set(CMAKE_COLOR_MAKEFILE ON)
enable_testing()
include(CTest)
include(ExternalProject)

#set(APP_COMPILE LASSO) 
#set(APP_COMPILE LOGISTIC) 


if("${APP_COMPILE}" MATCHES "LASSO")
set(FASTNET_EXTRA_CXX_FLAGS "-I/usr/lib/openmpi/include -I/usr/lib -I/usr/lib/openmpi/include/openmpi -pthread -DTEST_ATOMIC -DCOL_SORT_INPUT -DLASSO_COMPILE")
elseif("${APP_COMPILE}" MATCHES "LOGI")
set(FASTNET_EXTRA_CXX_FLAGS "-I/usr/lib/openmpi/include -I/usr/lib -I/usr/lib/openmpi/include/openmpi -pthread -DTEST_ATOMIC -DCOL_SORT_INPUT -DLOGI_COMPILE")
endif()

# MPIC++
set(FASTNET_EXTRA_LINKER_FLAGS "-pthread -L/usr/lib/openmpi/lib -lmpi_cxx -lmpi -lopen-rte -lopen-pal -ldl -Wl,--export-dynamic -lnsl -lutil -lm -ldl -rdynamic")

# from old Makefile
set(FASTNET_EXTRA_LINKER_FLAGS "-lm -lpthread -L/usr/local/lib -lcrypto -lssl ${FASTNET_EXTRA_LINKER_FLAGS}")
#set(FASTNET_EXTRA_LINKER_FLAGS "-lm  -lgsl -lgslcblas -lpthread -L/usr/local/lib  ${FASTNET_EXTRA_LINKER_FLAGS}")

################################################################
# Set Module Path
################################################################
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")

################################################################
# Initialize CXXFLAGS.
################################################################
set(CMAKE_CXX_FLAGS                "-g -O2 -Wall -Wno-strict-aliasing -std=c++0x -Werror -Wno-unused-function -Werror -Wno-unused-but-set-variable ${FASTNET_EXTRA_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS_DEBUG          "-g -DFASTNET_DEBUG -DFASTNET_TRACE -fno-inline")
set(CMAKE_CXX_FLAGS_MINSIZEREL     "-Os -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE        "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g")

################################################################
# Fix Paths
################################################################
SET(CMAKE_SYSTEM_INCLUDE_PATH ${CMAKE_SYSTEM_INCLUDE_PATH}
  /usr/local/include /opt/local/include
  /usr/include/w32api
  /usr/X11R6/include /usr/include/X11
  /usr/pkg/include
  /opt/csw/include /opt/include
  /usr/include /include
  )
SET(CMAKE_SYSTEM_LIBRARY_PATH ${CMAKE_SYSTEM_LIBRARY_PATH}
  $ENV{LD_LIBRARY_PATH}
  /usr/local/lib /opt/local/lib
  /usr/lib/w32api
  /usr/X11R6/lib /usr/lib/X11
  /usr/pkg/lib
  /opt/csw/lib /opt/lib
  /usr/lib /lib
  /usr/lib/x86_64-linux-gnu/
  )
SET(CMAKE_SYSTEM_PROGRAM_PATH ${CMAKE_SYSTEM_PROGRAM_PATH}
  /usr/local/bin /usr/pkg/bin /usr/bin /sbin /bin
  )
SET(CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES
  ${CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES}
  /usr/lib64 /usr/lib32 /usr/lib /lib
  )

################################################################
# External: MPI
################################################################
find_package(MPI REQUIRED)
include_directories(${MPI_INCLUDE_PATH})

################################################################
# External: gflags (Google Flags)
################################################################
ExternalProject_Add(GFLAGS
  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/thirdparty
  URL ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/gflags.zip
  CONFIGURE_COMMAND cmake -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER} "-DCMAKE_CXX_FLAGS=${EXTERNAL_CXX_FLAGS} -fPIC -DNDEBUG -w" -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_CURRENT_BINARY_DIR} ./
  BUILD_COMMAND make -j
  BUILD_IN_SOURCE 1
  INSTALL_COMMAND make install
  )
set(EXTRA_LIBS ${EXTRA_LIBS} "gflags")

################################################################
# External: glog (Google Log)
################################################################
ExternalProject_Add(GLOG
  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/thirdparty
  URL ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/glog.tar.gz
  CONFIGURE_COMMAND autoreconf --install > /dev/null 2>&1 || autoreconf && ./configure CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER} --disable-shared "CXX_FLAGS=-DNDEBUG ${EXTERNAL_CXX_FLAGS}" CFLAGS=-DNDEBUG "LDFLAGS=${EXTERNAL_LD_FLAGS}" --prefix=${CMAKE_CURRENT_BINARY_DIR} --with-gflags=${CMAKE_CURRENT_BINARY_DIR}/thirdparty/src/GFLAGS
  BUILD_COMMAND make -j
  BUILD_IN_SOURCE 1
  INSTALL_COMMAND make install
  )
set(EXTRA_LIBS ${EXTRA_LIBS} "glog")

################################################################
# External: ZMQ
################################################################
ExternalProject_Add(ZMQ
  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/thirdparty
  URL ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/zeromq-3.2.4.tar.gz
  CONFIGURE_COMMAND autoreconf --install > /dev/null 2>&1 || autoreconf && ./configure CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER} "CXX_FLAGS=-DNDEBUG ${EXTERNAL_CXX_FLAGS}" CFLAGS=-DNDEBUG "LDFLAGS=${EXTERNAL_LD_FLAGS}" --prefix=${CMAKE_CURRENT_BINARY_DIR} 
  BUILD_COMMAND make -j
  BUILD_IN_SOURCE 1
  INSTALL_COMMAND make install
  )
set(EXTRA_LIBS ${EXTRA_LIBS} "libzmq.a" "rt")

################################################################
# External: OpenSSL 
################################################################
#find_package(OpenSSL REQUIRED)
#message(STATUS "OPENSSL LIB: " ${OpenSSL_LIBRARIES})
#set(EXTRA_LIBS ${EXTRA_LIBS} ${OpenSSL_LIBRARIES})

################################################################
# External: GSL 
################################################################
#find_package(GSL REQUIRED)
#message(STATUS "GSL LIB: " ${GSL_LIBRARIES})
#set(EXTRA_LIBS ${EXTRA_LIBS} ${GSL_LIBRARIES})

################################################################
# External: set up include dir and libpath
################################################################
include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)
set(EXTERNAL_LD_FLAGS "-L${CMAKE_CURRENT_BINARY_DIR}/lib")

################################################################
# Include Directories
################################################################
include_directories(${FASTNET_SOURCE_DIR})
include_directories(${FASTNET_BINARY_DIR})

add_subdirectory(pi)
set(FASTNET_LIBS ${FASTNET_LIBS} pi)

add_subdirectory(com)
set(FASTNET_LIBS ${FASTNET_LIBS} comm)

add_subdirectory(scheduler)
set(FASTNET_LIBS ${FASTNET_LIBS} sched)

add_subdirectory(worker)
set(FASTNET_LIBS ${FASTNET_LIBS} worker)

add_subdirectory(coordinator)
set(FASTNET_LIBS ${FASTNET_LIBS} coordinator)

add_subdirectory(ds)
set(FASTNET_LIBS ${FASTNET_LIBS} ds)

add_subdirectory(platform)
set(FASTNET_LIBS ${FASTNET_LIBS} platform)


add_subdirectory(com/zmq/)
set(FASTNET_LIBS ${FASTNET_LIBS} zmq)



if("${APP_COMPILE}" MATCHES "LASSO")

add_subdirectory(app/dlasso/)
set(FASTNET_LIBS ${FASTNET_LIBS} dlasso)
set(TARGET_FILE execlasso)
add_dependencies(dlasso GLOG)

set(CMAKE_EXE_LINKER_FLAGS "${EXTERNAL_LD_FLAGS} ${FASTNET_EXTRA_LINKER_FLAGS} ${CMAKE_EXE_LINKER_FLAGS}")
set(EXTRA_LIBS ${FASTNET_LIBS} ${EXTRA_LIBS})
add_executable(${TARGET_FILE} ./src/main.cpp ./src/utility.cpp ./src/common.cpp)
target_link_libraries(${TARGET_FILE} ${EXTRA_LIBS} ${MPI_LIBRARIES})

elseif("${APP_COMPILE}" MATCHES "LOGI")

add_subdirectory(app/dlr/)
set(FASTNET_LIBS ${FASTNET_LIBS} dlr)
set(TARGET_FILE execlogi)
add_dependencies(dlr GLOG)

set(CMAKE_EXE_LINKER_FLAGS "${EXTERNAL_LD_FLAGS} ${FASTNET_EXTRA_LINKER_FLAGS} ${CMAKE_EXE_LINKER_FLAGS}")
set(EXTRA_LIBS ${FASTNET_LIBS} ${EXTRA_LIBS})
add_executable(${TARGET_FILE} ./src/main.cpp ./src/utility.cpp ./src/common.cpp)
target_link_libraries(${TARGET_FILE} ${EXTRA_LIBS} ${MPI_LIBRARIES})

endif()


add_dependencies(GLOG GFLAGS)
add_dependencies(GLOG ZMQ)
add_dependencies(${TARGET_FILE} GLOG)
add_dependencies(comm GLOG)
add_dependencies(pi GLOG)
add_dependencies(sched GLOG)
add_dependencies(worker GLOG)
add_dependencies(coordinator GLOG)
add_dependencies(ds GLOG)
add_dependencies(platform GLOG)
add_dependencies(zmq GLOG)
