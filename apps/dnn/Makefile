DNN_DIR := $(shell readlink $(dir $(lastword $(MAKEFILE_LIST))) -f)
PETUUM_ROOT = $(DNN_DIR)/../../

include $(PETUUM_ROOT)/defns.mk

DNN_SRC = $(wildcard $(DNN_DIR)/src/dnn/*.cpp)
DNN_HDR = $(wildcard $(DNN_DIR)/src/dnn/*.hpp)
DNN_BIN = $(DNN_DIR)/bin
DNN_OBJ = $(DNN_SRC:.cpp=.o)

PRED_SRC = $(wildcard $(DNN_DIR)/src/dnn_predict/*.cpp)
PRED_HDR = $(wildcard $(DNN_DIR)/src/dnn_predict/*.hpp)
PRED_BIN = $(DNN_DIR)/bin
PRED_OBJ = $(PRED_SRC:.cpp=.o)


DNN_SN_OBJ = $(DNN_SRC:.cpp=_sn.o)
GENDATA_SRC= $(DNN_DIR)/src/tools/gen_data.cpp

#all: DNN DNN_sn GENDATA
all: DNN DNN_PRED GENDATA

DNN: $(DNN_BIN)/DNN
DNN_PRED: $(DNN_BIN)/DNN_PRED
DNN_sn: $(DNN_BIN)/DNN_sn
GENDATA: $(DNN_BIN)/gen_data

$(DNN_BIN):
	mkdir -p $(DNN_BIN)

$(DNN_BIN)/DNN: $(DNN_OBJ) $(PETUUM_PS_LIB) $(DNN_BIN)
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) $(PETUUM_INCFLAGS) \
	$(DNN_OBJ) $(PETUUM_PS_LIB) $(PETUUM_LDFLAGS) -o $@

$(DNN_BIN)/DNN_PRED: $(PRED_OBJ) $(PETUUM_PS_LIB) $(DNN_BIN)
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) $(PETUUM_INCFLAGS) \
	$(PRED_OBJ) $(PETUUM_PS_LIB) $(PETUUM_LDFLAGS) -o $@

$(DNN_OBJ): %.o: %.cpp $(DNN_HDR)
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) -Wno-unused-result $(PETUUM_INCFLAGS) -c $< -o $@

$(PRED_OBJ): %.o: %.cpp $(PRED_HDR)
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) -Wno-unused-result $(PETUUM_INCFLAGS) -c $< -o $@

$(DNN_BIN)/DNN_sn: $(DNN_SN_OBJ) $(PETUUM_PS_SN_LIB) $(DNN_BIN)
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) $(PETUUM_INCFLAGS) -DPETUUM_SINGLE_NODE \
	$(DNN_SN_OBJ) $(PETUUM_PS_SN_LIB) $(PETUUM_LDFLAGS) -o $@

$(DNN_SN_OBJ): %_sn.o: %.cpp $(DNN_HDR)
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) -DPETUUM_SINGLE_NODE -Wno-unused-result \
	$(PETUUM_INCFLAGS) -c $< -o $@


$(DNN_BIN)/gen_data: $(GENDATA_SRC) 
	mkdir -p $(DNN_BIN)
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS)  $^ \
	$(PETUUM_LDFLAGS) -o $@

clean:
	rm -rf $(DNN_OBJ)
	rm -rf $(PRED_OBJ)
	rm -rf $(DNN_SN_OBJ)
	rm -rf $(DNN_BIN)

.PHONY: clean DNN DNN_PRED DNN_sn
