LASSO_DIR := $(shell readlink $(dir $(lastword $(MAKEFILE_LIST))) -f)
PETUUM_ROOT = $(LASSO_DIR)/../../

include $(PETUUM_ROOT)/defns.mk

LASSO_SRC = $(LASSO_DIR)/lasso_ps.cpp
LASSO_BIN = $(LASSO_DIR)/bin
LASSO = $(LASSO_BIN)/lasso_ps

lasso: $(LASSO)

$(LASSO): $(LASSO_SRC) $(PETUUM_PS_LIB)
	mkdir -p $(LASSO_BIN)
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) $(PETUUM_INCFLAGS) $^ \
	$(PETUUM_LDFLAGS) -o $@

clean:
	rm -rf $(LASSO_BIN)

.PHONY: lasso clean
