LDA_DIR := $(shell readlink $(dir $(lastword $(MAKEFILE_LIST))) -f)
STRADS_ROOT = $(LDA_DIR)/../../

include $(STRADS_ROOT)/defns.mk

LDA_LDFLAGS = -Wl,-rpath \
          -pthread \
          -lglog \
          -lgflags \
          -lprotobuf

LDA_SRC = $(wildcard $(LDA_DIR)/*.cpp)
LDA_HDR = $(wildcard $(LDA_DIR)/*.hpp)

LDA_BIN = $(LDA_DIR)/bin
LDA = $(LDA_BIN)/medlda

medlda: $(LDA)

$(LDA): $(LDA_SRC) $(STRADS_STRADS_LIB) $(LDA_HDR)
	mkdir -p $(LDA_BIN)
	$(STRADS_CXX) $(STRADS_CXXFLAGS) $(STRADS_INCFLAGS) -I./  \
	ldall.cpp trainer.cpp tester.cpp ll-coordinator.cpp ll-worker.cpp \
	ll-scheduler.cpp medlda-ps.cpp \
	stat.cpp topic_count.cpp $(STRADS_STRADS_LIB) \
	$(STRADS_LDFLAGS) -o $@

da: ./protobufda/lasso.proto
	protoc -I./protobufda/ --cpp_out=./protobufda ./protobufda/lasso.proto

clean:
	rm -rf $(LDA_BIN)/ldall

.PHONY: ldall clean
