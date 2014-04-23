
TOOLS_DIR := $(shell readlink $(dir $(lastword $(MAKEFILE_LIST))) -f)
PETUUM_ROOT = $(TOOLS_DIR)/../../

include $(PETUUM_ROOT)/defns.mk

TOOLS_SRC = $(wildcard $(TOOLS_DIR)/*.cpp)
TOOLS_BIN = $(TOOLS_DIR)/bin
TOOLS = $(TOOLS_BIN)/lda_processor

tools: $(TOOLS)

$(TOOLS_BIN)/lda_processor: lda_processor.cpp
	mkdir -p $(TOOLS_BIN)
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) $(PETUUM_INCFLAGS) $^ \
	$(PETUUM_LDFLAGS) -o $@

clean:
	rm -f $(TOOLS)

.PHONY: tools clean
