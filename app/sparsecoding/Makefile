# Sparse Coding Makefile

# Figure out app path
SPARSECODING_DIR := $(shell readlink $(dir $(lastword $(MAKEFILE_LIST))) -f)
PETUUM_ROOT = $(SPARSECODING_DIR)/../../

include $(PETUUM_ROOT)/defns.mk

# Define macros in src/matrix_loader.hpp
# Element in matrix whose absolute value is smaller than INFINITESIMAL is 
# considered 0 at output, such that you will not see something like 1e-10 in the
# result of coefficients S
PETUUM_CXXFLAGS += -DINFINITESIMAL=0.00001
# Element in matrix is set to MAXELEVAL if its value exceeds MAXELEVAL
# This bound is to prevent numeric overflow
PETUUM_CXXFLAGS += -DMAXELEVAL=100000
# Element in matrix is set to MINELEVAL if its value is lower than MINELEVAL
# This bound is to prevent numeric overflow
PETUUM_CXXFLAGS += -DMINELEVAL=-100000
PETUUM_INCFLAGS+= ${HAS_HDFS}

SPARSECODING_SRC = $(wildcard $(SPARSECODING_DIR)/src/*.cpp)
SPARSECODING_HDR = $(wildcard $(SPARSECODING_DIR)/src/*.hpp)
SPARSECODING_SRC += $(wildcard $(PETUUM_ROOT)/src/io/*.cpp)
SPARSECODING_HDR += $(wildcard $(PETUUM_ROOT)/src/io/*.hpp)

SPARSECODING_BIN = $(SPARSECODING_DIR)/bin
SPARSECODING_OBJ = $(SPARSECODING_SRC:.cpp=.o)
UTIL_SRC = $(wildcard $(SPARSECODING_DIR)/src/util/*.cpp)
UTIL_HDR = $(wildcard $(SPARSECODING_DIR)/src/util/*.hpp)
UTIL_OBJ = $(UTIL_SRC:.cpp=.o)

PETUUM_CXXFLAGS += -static-libstdc++

all: sparsecoding_main

sparsecoding_main: $(SPARSECODING_BIN)/sparsecoding_main

$(SPARSECODING_BIN):
	mkdir -p $(SPARSECODING_BIN)

$(SPARSECODING_BIN)/sparsecoding_main: $(SPARSECODING_OBJ) $(UTIL_OBJ) $(PETUUM_PS_LIB) $(PETUUM_ML_LIB) $(SPARSECODING_BIN)
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) $(PETUUM_INCFLAGS) \
	$(SPARSECODING_OBJ) $(UTIL_OBJ) $(PETUUM_PS_LIB) $(PETUUM_ML_LIB) $(PETUUM_LDFLAGS) -o $@

$(SPARSECODING_OBJ): %.o: %.cpp $(SPARSECODING_HDR) $(UTIL_HDR)
	$(PETUUM_CXX) $(NDEBUG)  $(PETUUM_CXXFLAGS) -Wno-unused-result $(PETUUM_INCFLAGS) -c $< -o $@

$(UTIL_OBJ): %.o: %.cpp $(UTIL_HDR)
	$(PETUUM_CXX) $(NDEBUG)  $(PETUUM_CXXFLAGS) -Wno-unused-result $(PETUUM_INCFLAGS) -c $< -o $@

clean:
	rm -rf $(SPARSECODING_OBJ)
	rm -rf $(UTIL_OBJ)
	rm -rf $(SPARSECODING_BIN)

.PHONY: clean sparsecoding_main
