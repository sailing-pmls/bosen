
TOOLS_DIR := $(shell readlink $(dir $(lastword $(MAKEFILE_LIST))) -f)
PETUUM_ROOT = $(TOOLS_DIR)/../../

include $(PETUUM_ROOT)/defns.mk

TOOLS_SRC = $(wildcard $(TOOLS_DIR)/*.cpp)
TOOLS_BIN = $(TOOLS_DIR)/bin
TOOLS = $(TOOLS_BIN)/lda_data_processor

tools: $(TOOLS)

$(TOOLS): $(TOOLS_SRC) $(PETUUM_PS_LIB)
	mkdir -p $(TOOLS_BIN)
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) $(PETUUM_INCFLAGS) $^ \
	$(PETUUM_LDFLAGS) -o $@

clean:
	rm -f $(TOOLS)

.PHONY: tools clean
