MATRIXFACT_DIR := $(shell readlink $(dir $(lastword $(MAKEFILE_LIST))) -f)
PETUUM_ROOT = $(MATRIXFACT_DIR)/../../

include $(PETUUM_ROOT)/defns.mk

MATRIXFACT_SRC = $(wildcard $(MATRIXFACT_DIR)/src/*.cpp)
MATRIXFACT_HDR = $(wildcard $(MATRIXFACT_DIR)/src/*.hpp)
MATRIXFACT_BIN = $(MATRIXFACT_DIR)/bin
MATRIXFACT_OBJ = $(MATRIXFACT_SRC:.cpp=.o)
MATRIXFACT_SN_OBJ = $(MATRIXFACT_SRC:.cpp=_sn.o)

all: matrixfact matrixfact_sn

matrixfact: $(MATRIXFACT_BIN)/matrixfact
matrixfact_sn: $(MATRIXFACT_BIN)/matrixfact_sn

$(MATRIXFACT_BIN):
	mkdir -p $(MATRIXFACT_BIN)

$(MATRIXFACT_BIN)/matrixfact: $(MATRIXFACT_OBJ) $(PETUUM_PS_LIB) $(MATRIXFACT_BIN)
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) $(PETUUM_INCFLAGS) \
	$(MATRIXFACT_OBJ) $(PETUUM_PS_LIB) $(PETUUM_LDFLAGS) -o $@

$(MATRIXFACT_OBJ): %.o: %.cpp $(MATRIXFACT_HDR)
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) -Wno-unused-result $(PETUUM_INCFLAGS) -c $< -o $@

$(MATRIXFACT_BIN)/matrixfact_sn: $(MATRIXFACT_SN_OBJ) $(PETUUM_PS_SN_LIB) $(MATRIXFACT_BIN)
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) -DPETUUM_SINGLE_NODE $(PETUUM_INCFLAGS) \
	$(MATRIXFACT_SN_OBJ) $(PETUUM_PS_SN_LIB) $(PETUUM_LDFLAGS) -o $@

$(MATRIXFACT_SN_OBJ): %_sn.o: %.cpp $(MATRIXFACT_HDR)
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) -DPETUUM_SINGLE_NODE -Wno-unused-result \
	$(PETUUM_INCFLAGS) -c $< -o $@

clean:
	rm -rf $(MATRIXFACT_OBJ)
	rm -rf $(MATRIXFACT_SN_OBJ)
	rm -rf $(MATRIXFACT_BIN)

.PHONY: clean matrixfact matrixfact_sn
