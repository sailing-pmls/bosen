
LDA_DIR := $(shell readlink $(dir $(lastword $(MAKEFILE_LIST))) -f)
PETUUM_ROOT = $(LDA_DIR)/../../

include $(PETUUM_ROOT)/defns.mk

LDA_SRC = $(wildcard $(LDA_DIR)/*.cpp)
LDA_HDR = $(wildcard $(LDA_DIR)/*.hpp)
LDA_BIN = $(LDA_DIR)/bin
LDA = $(LDA_BIN)/lda_main

lda: $(LDA)

$(LDA): $(LDA_SRC) $(PETUUM_PS_LIB)
	mkdir -p $(LDA_BIN)
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) $(PETUUM_INCFLAGS) $^ \
	$(PETUUM_LDFLAGS) -o $@

$(LDA_OBJ): %.o: %.cpp $(LDA_HDR)
	$(CXX) $(CXXFLAGS) -I$(LDA_DIR) $(INCFLAGS) -c $< -o $@

clean:
	rm -rf $(LDA_BIN)

.PHONY: lda clean
