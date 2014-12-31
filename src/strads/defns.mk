# Requires STRADS_ROOT to be defined
STRADS_SRC = $(STRADS_ROOT)/src
STRADS_LIB = $(STRADS_ROOT)/lib
STRADS_THIRD_PARTY = $(STRADS_ROOT)/third_party
STRADS_THIRD_PARTY_SRC = $(STRADS_THIRD_PARTY)/src
STRADS_THIRD_PARTY_INCLUDE = $(STRADS_THIRD_PARTY)/include
STRADS_THIRD_PARTY_LIB = $(STRADS_THIRD_PARTY)/lib
STRADS_THIRD_PARTY_BIN = $(STRADS_THIRD_PARTY)/bin

# replace std=c++11 with std=c++0x  
#STRADS_CXX = g++
STRADS_CXX = mpic++
STRADS_CXXFLAGS = -g \
	   -O2 \
           -Wall \
           -std=c++11 \
	   -Wno-sign-compare \
           -fno-builtin-malloc \
           -fno-builtin-calloc \
           -fno-builtin-realloc \
           -fno-builtin-free \
           -fno-omit-frame-pointer 

# comment out if not planning to use petuum stats
STRADS_CXXFLAGS += -DSTRADS_STATS
STRADS_INCFLAGS = -I$(STRADS_SRC) -I$(STRADS_THIRD_PARTY_INCLUDE) -I/usr/include/mpi 
STRADS_LDFLAGS = -Wl,-rpath,$(STRADS_THIRD_PARTY_LIB) \
          -L$(STRADS_THIRD_PARTY_LIB) \
          -pthread -lrt -lnsl \
          -lzmq \
          -lglog \
          -lgflags \
          -ltcmalloc \
	  -L/usr/lib/openmpi/lib -lmpi_cxx -lmpi -lopen-rte -lopen-pal\
          -lconfig++\
          -lprotobuf 

STRADS_STRADS_LIB = $(STRADS_LIB)/libpetuum-strads.a