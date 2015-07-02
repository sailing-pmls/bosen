
UTIL_TESTS_DIR = $(TESTS)/petuum_ps/util
UTIL_SRC_DIR=$(SRC)/petuum_ps/util

$(TESTS_BIN)/vector_clock_st_test: $(UTIL_TESTS_DIR)/vector_clock_st_test.cpp \
	$(SRC)/petuum_ps/util/vector_clock_st.cpp
	$(CXX) $(CXXFLAGS) $(INCFLAGS) $^ $(TESTS_LDFLAGS) -o $@

vector_clock_st_test_run: $(TESTS_BIN)/vector_clock_st_test
	$<

$(TESTS_BIN)/vector_clock_test: $(UTIL_TESTS_DIR)/vector_clock_test.cpp \
	$(SRC)/petuum_ps/util/vector_clock.cpp \
	$(SRC)/petuum_ps/util/vector_clock_st.cpp
	$(CXX) $(CXXFLAGS) $(INCFLAGS) $^ $(TESTS_LDFLAGS) -o $@

vector_clock_test_run: $(TESTS_BIN)/vector_clock_test
	$<

tests_class_register: $(UTIL_TESTS_DIR)/class_register_test.cpp \
	$(UTIL_SRC_DIR)/class_register.hpp
	$(CXX) $(INCFLAGS) -I$(UTIL_SRC_DIR) $(CXXFLAGS) $< $(LDFLAGS) \
	-o $(BIN)/$@

$(TESTS_BIN)/lock_perf_test: $(UTIL_TESTS_DIR)/lock_perf_test.cpp \
	$(SRC)/petuum_ps/util/lock.cpp \
	$(SRC)/petuum_ps/util/high_resolution_timer.cpp
	$(CXX) $(CXXFLAGS) $(INCFLAGS) $^ $(TESTS_LDFLAGS) -o $@

lock_perf_test_run: $(TESTS_BIN)/lock_perf_test
	GLOG_logtostderr=true $<

$(TESTS_BIN)/striped_lock_test: $(UTIL_TESTS_DIR)/striped_lock_test.cpp \
	$(SRC)/petuum_ps/util/striped_lock.hpp \
	$(SRC)/petuum_ps/util/context.hpp
	$(CXX) $(CXXFLAGS) $(INCFLAGS) $< $(SRC)/petuum_ps/util/context.cpp \
		$(TESTS_LDFLAGS) -o $@

striped_lock_test_run: $(TESTS_BIN)/striped_lock_test
	$<

# -D_GLIBCXX_USE_NANOSLEEP is a dirty hack as gcc-4.7 doesn't work for
#  std::this_thread::sleep_for.
$(TESTS_BIN)/stats_test: $(UTIL_TESTS_DIR)/stats_test.cpp \
	$(SRC)/petuum_ps/util/stats.hpp \
	$(SRC)/petuum_ps/util/stats.cpp \
	$(SRC)/petuum_ps/util/high_resolution_timer.hpp
	$(CXX) $(CXXFLAGS) $(INCFLAGS) $< $(SRC)/petuum_ps/util/stats.cpp \
		$(SRC)/petuum_ps/util/high_resolution_timer.cpp \
		$(TESTS_LDFLAGS) -D_GLIBCXX_USE_NANOSLEEP -o $@

stats_test_run: $(TESTS_BIN)/stats_test
	$<
