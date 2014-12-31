DML_DIR := $(shell readlink $(dir $(lastword $(MAKEFILE_LIST))) -f)
PETUUM_ROOT = $(DML_DIR)/../../

include $(PETUUM_ROOT)/defns.mk

DML_SRC = $(wildcard $(DML_DIR)/src/dml/*.cpp)
DML_HDR = $(wildcard $(MLR_DIR)/src/dml/*.hpp)
DML_BIN = $(DML_DIR)/bin
DML_OBJ = $(DML_SRC:.cpp=.o)
DML_SN_OBJ = $(DML_SRC:.cpp=_sn.o)
GENDATA_SRC= $(DML_DIR)/src/tools/gen_data.cpp

PETUUM_CXXFLAGS+= -fopenmp

all: DML 

DML: $(DML_BIN)/DML
DML_sn: $(DML_BIN)/DML_sn


$(DML_BIN):
	mkdir -p $(DML_BIN)

$(DML_BIN)/DML: $(DML_OBJ) $(PETUUM_PS_LIB) $(DML_BIN)
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) $(PETUUM_INCFLAGS) \
	$(DML_OBJ) $(PETUUM_PS_LIB) $(PETUUM_LDFLAGS) -o $@

$(DML_OBJ): %.o: %.cpp $(DML_HDR)
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) -Wno-unused-result $(PETUUM_INCFLAGS) -c $< -o $@

$(DML_BIN)/DML_sn: $(DML_SN_OBJ) $(PETUUM_PS_SN_LIB) $(DML_BIN)
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) -DPETUUM_SINGLE_NODE $(PETUUM_INCFLAGS) \
	$(DML_SN_OBJ) $(PETUUM_PS_SN_LIB) $(PETUUM_LDFLAGS) -o $@

$(DML_SN_OBJ): %_sn.o: %.cpp $(DML_HDR)
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) -DPETUUM_SINGLE_NODE -Wno-unused-result \
	$(PETUUM_INCFLAGS) -c $< -o $@



clean:
	rm -rf $(DML_OBJ)
	rm -rf $(DML_SN_OBJ)
	rm -rf $(DML_BIN)

.PHONY: clean DML DML_sn
