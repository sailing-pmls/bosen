# Nonnegative Matrix Factorization Makefile

# Figure out app path
NMF_DIR := $(shell readlink $(dir $(lastword $(MAKEFILE_LIST))) -f)
PETUUM_ROOT = $(NMF_DIR)/../../

include $(PETUUM_ROOT)/defns.mk

PETUUM_INCFLAGS+= ${HAS_HDFS}

# Define macros in src/matrix_loader.hpp
# Element in matrix whose absolute value is smaller than INFINITESIMAL is 
# considered 0 at output
PETUUM_CXXFLAGS += -DINFINITESIMAL=0.00001
# Element in matrix is set to MAXELEVAL if its value exceeds MAXELEVAL
# This bound is to prevent numeric overflow
PETUUM_CXXFLAGS += -DMAXELEVAL=100000
# Element in matrix is set to MINELEVAL if its value is lower than MINELEVAL
# This bound is to prevent numeric overflow
PETUUM_CXXFLAGS += -DMINELEVAL=-100000

NMF_SRC = $(wildcard $(NMF_DIR)/src/*.cpp)
NMF_HDR = $(wildcard $(NMF_DIR)/src/*.hpp)
NMF_BIN = $(NMF_DIR)/bin
NMF_OBJ = $(NMF_SRC:.cpp=.o)
UTIL_SRC = $(wildcard $(NMF_DIR)/src/util/*.cpp)
UTIL_HDR = $(wildcard $(NMF_DIR)/src/util/*.hpp)
UTIL_OBJ = $(UTIL_SRC:.cpp=.o)

all: nmf_main 

nmf_main: $(NMF_BIN)/nmf_main

$(NMF_BIN):
	mkdir -p $(NMF_BIN)

$(NMF_BIN)/nmf_main: $(NMF_OBJ) $(UTIL_OBJ) $(PETUUM_PS_LIB) $(PETUUM_ML_LIB) $(NMF_BIN)
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) $(PETUUM_INCFLAGS) \
	$(NMF_OBJ) $(UTIL_OBJ) $(PETUUM_PS_LIB) $(PETUUM_ML_LIB) $(PETUUM_LDFLAGS) -o $@

$(NMF_OBJ): %.o: %.cpp $(NMF_HDR) $(UTIL_HDR)
	$(PETUUM_CXX) $(NDEBUG) $(PETUUM_CXXFLAGS) -Wno-unused-result $(PETUUM_INCFLAGS) -c $< -o $@

$(UTIL_OBJ): %.o: %.cpp $(UTIL_HDR)
	$(PETUUM_CXX) $(NDEBUG) $(PETUUM_CXXFLAGS) -Wno-unused-result $(PETUUM_INCFLAGS) -c $< -o $@

clean:
	rm -rf $(NMF_OBJ)
	rm -rf $(UTIL_OBJ)
	rm -rf $(NMF_BIN)

.PHONY: clean nmf_main
