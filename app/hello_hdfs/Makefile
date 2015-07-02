APP_DIR := $(shell readlink $(dir $(lastword $(MAKEFILE_LIST))) -f)
PETUUM_ROOT = $(APP_DIR)/../..

include $(PETUUM_ROOT)/defns.mk

APP_SRC = $(wildcard $(APP_DIR)/src/*.cpp)
APP_HDR = $(wildcard $(APP_DIR)/src/*.hpp)
APP_SRC+= $(wildcard $(PETUUM_ROOT)/src/io/*.cpp)
APP_HDR+= $(wildcard $(PETUUM_ROOT)/src/io/*.hpp)
APP_BIN = $(APP_DIR)/bin
APP_OBJ = $(APP_SRC:.cpp=.o)
PETUUM_INCFLAGS+= ${HAS_HDFS}

all: $(APP_BIN)/hello_hdfs

$(APP_BIN):
	mkdir -p $@

$(APP_OBJ): %.o: %.cpp $(APP_HDR)
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) $(PETUUM_INCFLAGS) -c $< -o $@

$(APP_BIN)/hello_hdfs: $(APP_OBJ) $(PETUUM_IO_LIB) $(APP_BIN)
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) $(PETUUM_INCFLAGS) \
		$(APP_OBJ) $(PETUUM_IO_LIB) $(PETUUM_LDFLAGS) -o $@

clean:
	rm -rf $(APP_OBJ) $(APP_BIN)

.PHONY: clean
