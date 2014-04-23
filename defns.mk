# Requires PETUUM_ROOT to be defined

PETUUM_SRC = $(PETUUM_ROOT)/src
PETUUM_LIB = $(PETUUM_ROOT)/lib
PETUUM_THIRD_PARTY = $(PETUUM_ROOT)/third_party
PETUUM_THIRD_PARTY_SRC = $(PETUUM_THIRD_PARTY)/src
PETUUM_THIRD_PARTY_INCLUDE = $(PETUUM_THIRD_PARTY)/include
PETUUM_THIRD_PARTY_LIB = $(PETUUM_THIRD_PARTY)/lib

PETUUM_CXX = g++
PETUUM_CXXFLAGS = -g -O3 \
           -std=c++0x \
           -Wall \
	   -Wno-sign-compare \
           -fno-builtin-malloc \
           -fno-builtin-calloc \
           -fno-builtin-realloc \
           -fno-builtin-free \
           -fno-omit-frame-pointer \
					 -DNDEBUG
PETUUM_INCFLAGS = -I$(PETUUM_SRC) -I$(PETUUM_THIRD_PARTY_INCLUDE)
PETUUM_LDFLAGS = -Wl,-rpath,$(PETUUM_THIRD_PARTY_LIB) \
          -L$(PETUUM_THIRD_PARTY_LIB) \
          -pthread -lrt -lnsl -luuid \
          -lzmq \
          -lboost_thread \
          -lboost_system \
          -lglog \
          -lgflags \
          -ltcmalloc \
          -lconfig++
PETUUM_PS_LIB = $(PETUUM_LIB)/libpetuum-ps.a
