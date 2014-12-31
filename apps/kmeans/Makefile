KMeans_DIR := $(shell readlink $(dir $(lastword $(MAKEFILE_LIST))) -f)
PETUUM_ROOT = $(KMeans_DIR)/../../

include $(PETUUM_ROOT)/defns.mk

KMeans_SRC = $(wildcard $(KMeans_DIR)/src/*.cpp)
KMeans_HDR = $(wildcard $(KMeans_DIR)/src/*.h)
KMeans_BIN = $(KMeans_DIR)/bin
KMeans_OBJ = $(KMeans_SRC:.cpp=.o)
NDEBUG = -DNDEBUG

all: $(KMeans_BIN)/KMeans_main

$(KMeans_BIN):
	mkdir -p $(KMeans_BIN)

$(KMeans_BIN)/KMeans_main: $(KMeans_OBJ) $(PETUUM_PS_LIB) $(PETUUM_ML_LIB) $(KMeans_BIN)
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) $(PETUUM_INCFLAGS) \
	$(KMeans_OBJ) $(PETUUM_PS_LIB) $(PETUUM_ML_LIB) $(PETUUM_LDFLAGS) -o $@

$(KMeans_OBJ): %.o: %.cpp $(KMeans_HDR)
	$(PETUUM_CXX) $(NDEBUG) $(PETUUM_CXXFLAGS) -Wno-unused-result \
		$(PETUUM_INCFLAGS) -c $< -o $@


clean:
	rm -rf $(KMeans_OBJ)
	rm -rf $(KMeans_BIN)

.PHONY: clean
