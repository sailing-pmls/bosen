
ONE_ROW_DIR := $(shell readlink $(dir $(lastword $(MAKEFILE_LIST))) -f)
PETUUM_ROOT = $(ONE_ROW_DIR)/../../

include $(PETUUM_ROOT)/defns.mk

ONE_ROW_SRC = $(wildcard $(ONE_ROW_DIR)/*.cpp)
ONE_ROW_HDR = $(wildcard $(ONE_ROW_DIR)/*.hpp)
ONE_ROW_BIN = $(ONE_ROW_DIR)/bin
ONE_ROW = $(ONE_ROW_BIN)/one_row_main

one_row: $(ONE_ROW)

$(ONE_ROW): $(ONE_ROW_SRC) $(PETUUM_PS_LIB)
	mkdir -p $(ONE_ROW_BIN)
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) $(PETUUM_INCFLAGS) $^ \
	$(PETUUM_LDFLAGS) -D_GLIBCXX_USE_NANOSLEEP -o $@

$(ONE_ROW_OBJ): %.o: %.cpp $(ONE_ROW_HDR)
	$(CXX) $(CXXFLAGS) -I$(ONE_ROW_DIR) $(INCFLAGS) -c $< -o $@

clean:
	rm -rf $(ONE_ROW_BIN)

.PHONY: one_row clean
