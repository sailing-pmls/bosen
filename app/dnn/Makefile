DNN_DIR := $(shell readlink $(dir $(lastword $(MAKEFILE_LIST))) -f)
PETUUM_ROOT = $(DNN_DIR)/../../

include $(PETUUM_ROOT)/defns.mk

PETUUM_INCFLAGS+= ${HAS_HDFS}

DNN_SRC = $(wildcard $(DNN_DIR)/src/dnn/*.cpp)
DNN_HDR = $(wildcard $(DNN_DIR)/src/dnn/*.hpp)
DNN_SRC += $(wildcard $(PETUUM_ROOT)/src/io/*.cpp)
DNN_HDR += $(wildcard $(PETUUM_ROOT)/src/io/*.hpp)

DNN_BIN = $(DNN_DIR)/bin
DNN_OBJ = $(DNN_SRC:.cpp=.o)

PRED_SRC = $(wildcard $(DNN_DIR)/src/dnn_predict/*.cpp)
PRED_HDR = $(wildcard $(DNN_DIR)/src/dnn_predict/*.hpp)
PRED_BIN = $(DNN_DIR)/bin
PRED_OBJ = $(PRED_SRC:.cpp=.o)


GENDATA_SRC= $(DNN_DIR)/src/tools/gen_data.cpp


all: DNN DNN_PRED GENDATA

DNN: $(DNN_BIN)/DNN
DNN_PRED: $(DNN_BIN)/DNN_PRED
GENDATA: $(DNN_BIN)/gen_data

$(DNN_BIN):
	mkdir -p $(DNN_BIN)

$(DNN_BIN)/DNN: $(DNN_OBJ) $(PETUUM_PS_LIB) $(PETUUM_ML_LIB) $(DNN_BIN)
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) $(PETUUM_INCFLAGS) \
	$(DNN_OBJ) $(PETUUM_PS_LIB) $(PETUUM_ML_LIB) $(PETUUM_LDFLAGS) -o $@

$(DNN_BIN)/DNN_PRED: $(PRED_OBJ) $(PETUUM_PS_LIB) $(PETUUM_ML_LIB) $(DNN_BIN)
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) $(PETUUM_INCFLAGS) \
	$(PRED_OBJ) $(PETUUM_PS_LIB) $(PETUUM_ML_LIB) $(PETUUM_LDFLAGS) -o $@

$(DNN_OBJ): %.o: %.cpp $(DNN_HDR)
	$(PETUUM_CXX) $(NDEBUG) $(PETUUM_CXXFLAGS) -Wno-unused-result $(PETUUM_INCFLAGS) -c $< -o $@

$(PRED_OBJ): %.o: %.cpp $(PRED_HDR)
	$(PETUUM_CXX) $(NDEBUG) $(PETUUM_CXXFLAGS) -Wno-unused-result $(PETUUM_INCFLAGS) -c $< -o $@



$(DNN_BIN)/gen_data: $(GENDATA_SRC) 
	mkdir -p $(DNN_BIN)
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS)  $^ \
	$(PETUUM_LDFLAGS) -o $@

clean:
	rm -rf $(DNN_OBJ)
	rm -rf $(PRED_OBJ)
	rm -rf $(DNN_BIN)

.PHONY: clean DNN DNN_PRED