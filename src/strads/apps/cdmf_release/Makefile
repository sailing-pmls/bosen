CCDMF_DIR := $(shell readlink $(dir $(lastword $(MAKEFILE_LIST))) -f)
STRADS_ROOT = $(CCDMF_DIR)/../../

include $(STRADS_ROOT)/defns.mk

CCDMF_LDFLAGS = -Wl,-rpath \
          -pthread \
          -lglog \
          -lgflags \
          -lprotobuf 

CCDMF_SRC = $(wildcard $(CCDMF_DIR)/*.cpp)
CCDMF_HDR = $(wildcard $(CCDMF_DIR)/*.hpp)

CCDMF_BIN = $(CCDMF_DIR)/bin
CCDMF = $(CCDMF_BIN)/lccdmf

lccdmf: $(CCDMF)

$(CCDMF): $(CCDMF_SRC) $(STRADS_STRADS_LIB) $(CCDMF_HDR)
	mkdir -p $(CCDMF_BIN)
	$(STRADS_CXX) $(STRADS_CXXFLAGS) $(STRADS_INCFLAGS) $^ \
	$(STRADS_LDFLAGS) $(CCDMF_LDFLAGS) -I./ -o $@
clean:
	rm -rf $(CCDMF_BIN)/lccdmf

.PHONY: lccdmf clean
