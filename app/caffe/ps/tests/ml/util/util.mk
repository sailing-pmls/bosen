UTIL_TESTS_DIR = $(TESTS)/ml/util
UTIL_SRC_DIR=$(SRC)/ml/util

util_test_run_all: math_util_test_run data_loading_test_run

$(TESTS_BIN)/math_util_test: $(UTIL_TESTS_DIR)/math_util_test.cpp \
	$(UTIL_SRC_DIR)/math_util.hpp $(UTIL_SRC_DIR)/math_util.o \
	$(SRC)/petuum_ps_common/util/high_resolution_timer.o
	$(CXX) $(CXXFLAGS) $(INCFLAGS) $^ $(TESTS_LDFLAGS) -o $@

math_util_test_run: $(TESTS_BIN)/math_util_test
	$<

$(TESTS_BIN)/data_loading_test: $(UTIL_TESTS_DIR)/data_loading_test.cpp \
	$(UTIL_SRC_DIR)/data_loading.hpp $(UTIL_SRC_DIR)/data_loading.o \
	$(SRC)/petuum_ps_common/util/high_resolution_timer.o
	$(CXX) $(CXXFLAGS) $(INCFLAGS) $^ $(TESTS_LDFLAGS) -o $@

data_loading_test_run: $(TESTS_BIN)/data_loading_test
	$<
