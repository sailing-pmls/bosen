MATRIXFACT_DIR := $(shell readlink $(dir $(lastword $(MAKEFILE_LIST))) -f)
PETUUM_ROOT = $(MATRIXFACT_DIR)/../../

include $(PETUUM_ROOT)/defns.mk

MATRIXFACT_SRC = $(MATRIXFACT_DIR)/matrixfact.cpp
MATRIXFACT_BIN = $(MATRIXFACT_DIR)/bin
MATRIXFACT = $(MATRIXFACT_BIN)/matrixfact

matrixfact: $(MATRIXFACT)

$(MATRIXFACT): $(MATRIXFACT_SRC) $(PETUUM_PS_LIB)
	mkdir -p $(MATRIXFACT_BIN)
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) $(PETUUM_INCFLAGS) $^ \
	$(PETUUM_LDFLAGS) -o $@

clean:
	rm -rf $(MATRIXFACT_BIN)

.PHONY: matrixfact clean
