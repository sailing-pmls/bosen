# Assuming this Makefile lives in project root directory
PROJECT := $(shell readlink $(dir $(lastword $(MAKEFILE_LIST))) -f)

STRADS_ROOT = $(PROJECT)

include $(PROJECT)/defns.mk

# defined in defns.mk
SRC = $(STRADS_SRC)
LIB = $(STRADS_LIB)
THIRD_PARTY = $(STRADS_THIRD_PARTY)
THIRD_PARTY_SRC = $(STRADS_THIRD_PARTY_SRC)
THIRD_PARTY_LIB = $(STRADS_THIRD_PARTY_LIB)
THIRD_PARTY_INCLUDE = $(STRADS_THIRD_PARTY_INCLUDE)
THIRD_PARTY_BIN = $(STRADS_THIRD_PARTY_BIN)

BIN = $(PROJECT)/bin
TESTS = $(PROJECT)/tests
TESTS_BIN = $(BIN)/tests

NEED_MKDIR = $(BIN) \
             $(LIB) \
             $(TESTS_BIN) \
             $(THIRD_PARTY_SRC) \
             $(THIRD_PARTY_LIB) \
             $(THIRD_PARTY_INCLUDE)

CXX = $(STRADS_CXX)
CXXFLAGS = $(STRADS_CXXFLAGS)
INCFLAGS = $(STRADS_INCFLAGS)
LDFLAGS = $(STRADS_LDFLAGS)

all: path \
     third_party_core \
     st_lib

path: $(NEED_MKDIR)

$(NEED_MKDIR):
	mkdir -p $@

clean:
	rm -rf $(BIN) $(LIB) $(ST_OBJ)

distclean: clean
	rm -rf $(filter-out $(THIRD_PARTY)/third_party.mk, \
		            $(wildcard $(THIRD_PARTY)/*))

.PHONY: all path clean distclean

include $(SRC)/strads.mk

include $(THIRD_PARTY)/third_party.mk