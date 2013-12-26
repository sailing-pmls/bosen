# Assuming this Makefile lives in project root directory
PROJECT := $(shell readlink $(dir $(lastword $(MAKEFILE_LIST))) -f)

SRC = $(PROJECT)/src
BIN = $(PROJECT)/bin
LIB = $(PROJECT)/lib
APPS = $(PROJECT)/apps
TESTS = $(PROJECT)/tests
THIRD_PARTY = $(PROJECT)/third_party

TESTS_BIN = $(BIN)/tests
THIRD_PARTY_SRC = $(THIRD_PARTY)/src
THIRD_PARTY_LIB = $(THIRD_PARTY)/lib
THIRD_PARTY_INCLUDE = $(THIRD_PARTY)/include

NEED_MKDIR = $(BIN) \
             $(LIB) \
             $(TESTS_BIN) \
             $(THIRD_PARTY_SRC) \
             $(THIRD_PARTY_LIB) \
             $(THIRD_PARTY_INCLUDE)

CXX = g++
CXXFLAGS = -g -O3 \
           -std=c++0x \
           -Wno-strict-aliasing \
           -fno-builtin-malloc \
           -fno-builtin-calloc \
           -fno-builtin-realloc \
           -fno-builtin-free \
           -fno-omit-frame-pointer
INCFLAGS = -I$(SRC) -I$(THIRD_PARTY_INCLUDE)
LDFLAGS = -L$(THIRD_PARTY_LIB) \
          -pthread -lrt -lnsl -luuid \
          -lzmq \
          -lboost_thread \
          -lboost_system \
          -lglog \
          -lgflags \
          -ltcmalloc \
          -lconfig++

all: $(NEED_MKDIR) \
     third_party \
     ps_all
#     apps_all

$(NEED_MKDIR):
	mkdir -p $@

clean:
	rm -rf $(BIN) $(LIB) $(PS_OBJ)

distclean: clean
	rm -rf $(filter-out $(THIRD_PARTY)/third_party.mk, \
		$(wildcard $(THIRD_PARTY)/*))

.PHONY: all clean distclean


include $(SRC)/petuum.mk

include $(APPS)/apps.mk

include $(THIRD_PARTY)/third_party.mk

