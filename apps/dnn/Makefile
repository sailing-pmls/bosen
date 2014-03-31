
DNN_DIR := $(shell readlink $(dir $(lastword $(MAKEFILE_LIST))) -f)
PETUUM_ROOT = $(DNN_DIR)/../../

include $(PETUUM_ROOT)/defns.mk

DNN_SRC = $(DNN_DIR)/*.cpp

DNN_BIN = $(DNN_DIR)/bin
DNN = $(DNN_BIN)/dnn


dnn: $(DNN)

$(DNN): $(DNN_SRC) $(PETUUM_PS_LIB)
	mkdir -p $(DNN_BIN)
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) $(PETUUM_INCFLAGS) $^ \
	$(PETUUM_LDFLAGS) -o $@



clean:
	rm -rf $(DNN_BIN)


.PHONY: dnn clean