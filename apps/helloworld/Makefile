
HELLOWORLD_DIR := $(shell readlink $(dir $(lastword $(MAKEFILE_LIST))) -f)
PETUUM_ROOT = $(HELLOWORLD_DIR)/../../

include $(PETUUM_ROOT)/defns.mk

HELLOWORLD_SRC = $(HELLOWORLD_DIR)/helloworld.cpp
HELLOWORLD_BIN = $(HELLOWORLD_DIR)/bin
HELLOWORLD = $(HELLOWORLD_BIN)/helloworld

helloworld: $(HELLOWORLD)

$(HELLOWORLD): $(HELLOWORLD_SRC) $(PETUUM_PS_LIB)
	mkdir -p $(HELLOWORLD_BIN)
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) $(PETUUM_INCFLAGS) $^ \
	$(PETUUM_LDFLAGS) -o $@

clean:
	rm -rf $(HELLOWORLD_BIN)


.PHONY: helloworld clean