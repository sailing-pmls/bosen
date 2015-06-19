# Assuming this Makefile lives in project root directory
PROJECT := $(shell readlink $(dir $(lastword $(MAKEFILE_LIST))) -f)

PETUUM_ROOT = $(PROJECT)

include $(PROJECT)/defns.mk

# defined in defns.mk
SRC = $(PETUUM_SRC)
LIB = $(PETUUM_LIB)
THIRD_PARTY = $(PETUUM_THIRD_PARTY)
THIRD_PARTY_SRC = $(PETUUM_THIRD_PARTY_SRC)
THIRD_PARTY_LIB = $(PETUUM_THIRD_PARTY_LIB)
THIRD_PARTY_INCLUDE = $(PETUUM_THIRD_PARTY_INCLUDE)
THIRD_PARTY_BIN = $(PETUUM_THIRD_PARTY_BIN)

BIN = $(PROJECT)/bin
TESTS = $(PROJECT)/tests
TESTS_BIN = $(BIN)/tests

NEED_MKDIR = $(BIN) \
             $(LIB) \
             $(TESTS_BIN) \
             $(THIRD_PARTY_SRC) \
             $(THIRD_PARTY_LIB) \
             $(THIRD_PARTY_INCLUDE)

CXX = $(PETUUM_CXX)
CXXFLAGS = $(PETUUM_CXXFLAGS)
CXXFLAGS += -DPETUUM_MAX_NUM_CLIENTS=128
INCFLAGS = $(PETUUM_INCFLAGS)
LDFLAGS = $(PETUUM_LDFLAGS)

all: path \
     third_party_core \
     ps_lib \
		 ml_lib

path: $(NEED_MKDIR)

$(NEED_MKDIR):
	mkdir -p $@

clean:
	rm -rf $(BIN) $(LIB) $(PS_OBJ) $(ML_OBJ) $(PS_SN_OBJ) $(PS_COMMON_OBJ)

distclean: clean
	rm -rf $(filter-out $(THIRD_PARTY)/third_party.mk, \
		            $(wildcard $(THIRD_PARTY)/*))

.PHONY: all path clean distclean

include $(SRC)/petuum.mk

include $(THIRD_PARTY)/third_party.mk
