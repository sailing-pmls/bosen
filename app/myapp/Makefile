APP_DIR := $(shell readlink $(dir $(lastword $(MAKEFILE_LIST))) -f)
PETUUM_ROOT = $(APP_DIR)/../../

include $(PETUUM_ROOT)/defns.mk

APP_SRC = $(wildcard $(APP_DIR)/src/*.cpp)
APP_HDR = $(wildcard $(APP_DIR)/src/*.hpp)
APP_BIN = $(APP_DIR)/bin
APP_OBJ = $(APP_SRC:.cpp=.o)

#PETUUM_CXXFLAGS+= -fopenmp

all: APP

APP: $(APP_BIN)/myapp

$(APP_BIN):
	mkdir -p $(APP_BIN)

$(APP_BIN)/myapp: $(APP_OBJ) $(PETUUM_PS_LIB) $(PETUUM_ML_LIB) $(APP_BIN)
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) $(PETUUM_INCFLAGS) \
	$(APP_OBJ) $(PETUUM_PS_LIB) $(PETUUM_ML_LIB) $(PETUUM_LDFLAGS) -o $@

$(APP_OBJ): %.o: %.cpp $(APP_HDR)
	$(PETUUM_CXX) $(NDEBUG) $(PETUUM_CXXFLAGS) -Wno-unused-result \
		$(PETUUM_INCFLAGS) -c $< -o $@

clean:
	rm -rf $(APP_OBJ)
	rm -rf $(APP_BIN)

.PHONY: clean APP
