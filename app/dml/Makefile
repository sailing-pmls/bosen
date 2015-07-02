DML_DIR := $(shell readlink $(dir $(lastword $(MAKEFILE_LIST))) -f)
PETUUM_ROOT = $(DML_DIR)/../../

include $(PETUUM_ROOT)/defns.mk

DML_SRC = $(wildcard $(DML_DIR)/src/dml/*.cpp)
DML_HDR = $(wildcard $(MLR_DIR)/src/dml/*.hpp)

DML_SRC += $(wildcard $(PETUUM_ROOT)/src/io/*.cpp)

DML_HDR += $(wildcard $(PETUUM_ROOT)/src/io/*.hpp)

DML_BIN = $(DML_DIR)/bin
DML_OBJ = $(DML_SRC:.cpp=.o)

PETUUM_INCFLAGS+= ${HAS_HDFS}
PETUUM_CXXFLAGS+= -fopenmp

all: DML 

DML: $(DML_BIN)/DML


$(DML_BIN):
	mkdir -p $(DML_BIN)

$(DML_BIN)/DML: $(DML_OBJ) $(PETUUM_PS_LIB) $(PETUUM_ML_LIB) $(DML_BIN)
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) $(PETUUM_INCFLAGS) \
	$(DML_OBJ) $(PETUUM_PS_LIB) $(PETUUM_ML_LIB) $(PETUUM_LDFLAGS) -o $@

$(DML_OBJ): %.o: %.cpp $(DML_HDR)
	$(PETUUM_CXX) $(NDEBUG) $(PETUUM_CXXFLAGS) -Wno-unused-result $(PETUUM_INCFLAGS) -c $< -o $@

$(DML_BIN)/DML_sn: $(DML_SN_OBJ) $(PETUUM_PS_SN_LIB) $(DML_BIN)
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) -DPETUUM_SINGLE_NODE $(PETUUM_INCFLAGS) \
	$(DML_SN_OBJ) $(PETUUM_PS_SN_LIB) $(PETUUM_LDFLAGS) -o $@




clean:
	rm -rf $(DML_OBJ)
	rm -rf $(DML_BIN)

.PHONY: clean DML
