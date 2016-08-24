
LDA_SRC_DIR := $(shell readlink $(dir $(lastword $(MAKEFILE_LIST))) -f)/src
PETUUM_ROOT = $(LDA_SRC_DIR)/../../../

include $(PETUUM_ROOT)/defns.mk

PETUUM_CXXFLAGS += -Wno-unused-result

LDA_SRC = $(filter-out $(LDA_SRC_DIR)/doc_loader_test.cpp, \
					$(filter-out $(LDA_SRC_DIR)/lda_doc_test.cpp, \
					$(filter-out $(LDA_SRC_DIR)/data_preprocessor.cpp, \
					$(wildcard $(LDA_SRC_DIR)/*.cpp))))
LDA_HDR = $(wildcard $(LDA_SRC_DIR)/*.hpp)
LDA_BIN = $(LDA_SRC_DIR)/../bin
LDA_OBJ = $(LDA_SRC:.cpp=.o)
LDA_SN_OBJ = $(LDA_SRC:.cpp=_sn.o)

all: lda data_preprocessor

lda: $(LDA_BIN)/lda_main
lda_sn: $(LDA_BIN)/lda_main_sn

data_preprocessor: $(LDA_BIN)/data_preprocessor

$(LDA_BIN):
	mkdir -p $(LDA_BIN)

$(LDA_BIN)/lda_main: $(LDA_OBJ) $(PETUUM_PS_LIB) $(LDA_BIN)
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) $(PETUUM_INCFLAGS) \
	$(LDA_OBJ) $(PETUUM_PS_LIB) $(PETUUM_LDFLAGS) -lleveldb -o $@

$(LDA_BIN)/data_preprocessor: $(LDA_SRC_DIR)/data_preprocessor.cpp \
	$(LDA_SRC_DIR)/ldb_comparator.hpp $(LDA_BIN)
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) -Wno-unused-result $(PETUUM_INCFLAGS) \
	$(LDA_SRC_DIR)/ldb_comparator.hpp $(LDA_SRC_DIR)/data_preprocessor.cpp $(PETUUM_LDFLAGS) \
	-lleveldb -o $@

$(LDA_OBJ): %.o: %.cpp $(LDA_HDR)
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) -Wno-unused-result $(PETUUM_INCFLAGS) -c $< -o $@

$(LDA_BIN)/lda_main_sn: $(LDA_SN_OBJ) $(PETUUM_PS_SN_LIB) $(LDA_BIN)
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) $(PETUUM_INCFLAGS) -DPETUUM_SINGLE_NODE \
	$(LDA_SN_OBJ) $(PETUUM_PS_SN_LIB) $(PETUUM_LDFLAGS) -lleveldb -o $@

$(LDA_SN_OBJ): %_sn.o: %.cpp $(LDA_HDR)
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) -DPETUUM_SINGLE_NODE -Wno-unused-result \
		$(PETUUM_INCFLAGS) -c $< -o $@

clean:
	rm -rf $(LDA_OBJ)
	rm -rf $(LDA_SN_OBJ)
	rm -rf $(LDA_BIN)

.PHONY: clean lda lda_sn data_preprocessor
