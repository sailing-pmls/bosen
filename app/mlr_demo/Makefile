MLR_DIR := $(shell readlink $(dir $(lastword $(MAKEFILE_LIST))) -f)
PETUUM_ROOT = $(MLR_DIR)/../../

include $(PETUUM_ROOT)/defns.mk

MLR_SRC = $(wildcard $(MLR_DIR)/src/*.cpp)
MLR_HDR = $(wildcard $(MLR_DIR)/src/*.hpp)
MLR_BIN = $(MLR_DIR)/bin
MLR_OBJ = $(MLR_SRC:.cpp=.o)
NDEBUG = -DNDEBUG

all: $(MLR_BIN)/mlr_demo_main $(MLR_BIN)/gen_data_sparse

$(MLR_BIN):
	mkdir -p $(MLR_BIN)

$(MLR_BIN)/mlr_demo_main: $(MLR_OBJ) $(PETUUM_PS_LIB) $(PETUUM_ML_LIB) $(MLR_BIN)
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) $(PETUUM_INCFLAGS) \
	$(MLR_OBJ) $(PETUUM_PS_LIB) $(PETUUM_ML_LIB) $(PETUUM_LDFLAGS) -o $@

$(MLR_OBJ): %.o: %.cpp $(MLR_HDR)
	$(PETUUM_CXX) $(NDEBUG) $(PETUUM_CXXFLAGS) -Wno-unused-result \
		$(PETUUM_INCFLAGS) -c $< -o $@

$(MLR_BIN)/gen_data_sparse: $(MLR_DIR)/src/tools/gen_data_sparse.cpp $(MLR_BIN)
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) $(PETUUM_INCFLAGS) \
	$< $(PETUUM_PS_LIB) $(PETUUM_LDFLAGS) -o $@

clean:
	rm -rf $(MLR_OBJ)
	rm -rf $(MLR_BIN)

.PHONY: clean
