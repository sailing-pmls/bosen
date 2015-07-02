DISK_STREAM_TESTS_DIR = $(TESTS)/ml/disk_stream
DISK_STREAM_SRC_DIR=$(SRC)/ml/disk_stream

disk_stream_test_run_all: disk_streamer_test_run

$(TESTS_BIN)/disk_streamer_test: \
	$(DISK_STREAM_TESTS_DIR)/disk_streamer_test.cpp \
	$(DISK_STREAM_SRC_DIR)/disk_streamer.hpp \
	$(DISK_STREAM_SRC_DIR)/disk_reader.o \
	$(DISK_STREAM_SRC_DIR)/multi_buffer.o \
	$(DISK_STREAM_SRC_DIR)/byte_buffer.o \
	$(DISK_STREAM_SRC_DIR)/parsers/abstract_parser.hpp \
	$(DISK_STREAM_SRC_DIR)/parsers/libsvm_parser.hpp \
	$(SRC)/petuum_ps_common/util/high_resolution_timer.o
	$(CXX) $(CXXFLAGS) $(INCFLAGS) $^ $(TESTS_LDFLAGS) -o $@

disk_streamer_test_run: $(TESTS_BIN)/disk_streamer_test
	$<
