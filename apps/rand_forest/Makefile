RF_DIR := $(shell readlink $(dir $(lastword $(MAKEFILE_LIST))) -f)
PETUUM_ROOT = $(RF_DIR)/../../
RF_SRC_DIR = $(RF_DIR)/src
RF_TEST_DIR = $(RF_DIR)/test
RF_TEST_INCFLAGS = $(PETUUM_INCFLAGS) -I$(RF_SRC_DIR)
RF_TEST_LDFLAGS = $(PETUUM_LDFLAGS) -lgtest_main

include $(PETUUM_ROOT)/defns.mk

RF_SRC = $(wildcard $(RF_SRC_DIR)/*.cpp)
RF_HDR = $(wildcard $(RF_SRC_DIR)/*.hpp)
RF_BIN = $(RF_DIR)/bin
RF_OBJ = $(RF_SRC:.cpp=.o)

all: $(RF_BIN)/rand_forest_main $(RF_BIN)/random_split $(RF_BIN)/libsvm2binary

test: split_finder_test_run

$(RF_BIN)/random_split: src/tool/random_split.cpp
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) $(PETUUM_INCFLAGS) -Wno-unused-result \
		$< -o $@ $(PETUUM_LDFLAGS)

$(RF_BIN)/libsvm2binary: src/tool/libsvm2binary.cpp
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) $(PETUUM_INCFLAGS) -Wno-unused-result \
		$< -o $@ $(PETUUM_ML_LIB) $(PETUUM_PS_LIB) $(PETUUM_LDFLAGS)

$(RF_BIN):
	mkdir -p $(RF_BIN)

$(RF_BIN)/rand_forest_main: $(RF_OBJ) $(PETUUM_PS_LIB) \
	$(PETUUM_ML_LIB) $(RF_BIN)
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) $(PETUUM_INCFLAGS) \
		$(RF_OBJ) $(PETUUM_ML_LIB) $(PETUUM_PS_LIB) $(PETUUM_LDFLAGS) -o $@

$(RF_OBJ): %.o: %.cpp $(RF_HDR)
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) -Wno-unused-result \
		$(PETUUM_INCFLAGS) -c $< -o $@

# ============= Tests =============

$(RF_BIN)/split_finder_test: $(RF_TEST_DIR)/split_finder_test.cpp \
	$(RF_SRC_DIR)/split_finder.hpp $(RF_SRC_DIR)/split_finder.o \
	$(RF_SRC_DIR)/utils.hpp $(RF_SRC_DIR)/utils.cpp
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) $(RF_TEST_INCFLAGS) $^ \
		$(RF_TEST_LDFLAGS) -o $@

split_finder_test_run: $(RF_BIN)/split_finder_test
	$<

clean:
	rm -rf $(RF_OBJ) src/tool/random_split.o src/tool/libsvm2binary.o
	rm -rf $(RF_BIN) 

.PHONY: clean
