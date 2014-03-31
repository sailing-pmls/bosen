
STORAGE_TESTS_DIR = $(TESTS)/petuum_ps/storage
STORAGE_TESTS_SRC = $(wildcard $(STORAGE_TESTS_DIR)/*.cpp)
STORAGE_TESTS = $(STORAGE_TESTS_SRC:$(STORAGE_TESTS_DIR)/%.cpp=$(TESTS_BIN)/%)

storage_tests: $(STORAGE_TESTS)

#$(STORAGE_TESTS): $(TESTS_BIN)/%: $(STORAGE_TESTS_DIR)/%.cpp
#	$(CXX) $(CXXFLAGS) $(INCFLAGS) $< $(TESTS_LDFLAGS) -o $@

simple_row_storage_test_run: $(TESTS_BIN)/simple_row_storage_test
	GLOG_v=-1 GLOG_logtostderr=false \
	LD_LIBRARY_PATH=$(THIRD_PARTY_LIB):$$LD_LIBRARY_PATH $<

$(TESTS_BIN)/clock_lru_test: $(STORAGE_TESTS_DIR)/clock_lru_test.cpp \
	$(SRC)/petuum_ps/storage/clock_lru.o \
	$(SRC)/petuum_ps/util/striped_lock.hpp \
	$(SRC)/petuum_ps/thread/context.o
	$(CXX) $(CXXFLAGS) $(INCFLAGS) $< \
		$(SRC)/petuum_ps/storage/clock_lru.o \
		$(SRC)/petuum_ps/thread/context.o \
		$(TESTS_LDFLAGS) -o $@

clock_lru_test_run: $(TESTS_BIN)/clock_lru_test
	env HEAPCHECK=normal GLOG_v=3 GLOG_logtostderr=true $<

$(TESTS_BIN)/sparse_row_test: $(STORAGE_TESTS_DIR)/sparse_row_test.cpp \
	$(SRC)/petuum_ps/storage/sparse_row.hpp \
	$(SRC)/petuum_ps/util/lock.o $(SRC)/petuum_ps/util/lock.cpp
	$(CXX) $(CXXFLAGS) $(INCFLAGS) $< $(SRC)/petuum_ps/util/lock.o \
		$(TESTS_LDFLAGS)  -o $@

sparse_row_test_run: $(TESTS_BIN)/sparse_row_test
	env HEAPCHECK=normal GLOG_v=3 GLOG_logtostderr=true $<

$(TESTS_BIN)/sorted_vector_map_row_test: \
	$(STORAGE_TESTS_DIR)/sorted_vector_map_row_test.cpp \
	$(SRC)/petuum_ps/storage/sorted_vector_map_row.hpp \
	$(SRC)/petuum_ps/util/lock.o $(SRC)/petuum_ps/util/lock.cpp
	$(CXX) $(CXXFLAGS) $(INCFLAGS) $< $(SRC)/petuum_ps/util/lock.o \
		$(TESTS_LDFLAGS)  -o $@

sorted_vector_map_row_test_run: $(TESTS_BIN)/sorted_vector_map_row_test
	env HEAPCHECK=normal GLOG_v=3 GLOG_logtostderr=true $<

$(TESTS_BIN)/process_storage_test: $(STORAGE_TESTS_DIR)/process_storage_test.cpp \
	$(SRC)/petuum_ps/storage/process_storage.o \
	$(SRC)/petuum_ps/storage/clock_lru.o \
	$(SRC)/petuum_ps/storage/sparse_row.hpp \
	$(SRC)/petuum_ps/include/row_access.hpp \
	$(SRC)/petuum_ps/storage/client_row.o \
	$(SRC)/petuum_ps/thread/context.o \
	$(SRC)/petuum_ps/util/lock.o \
	$(SRC)/petuum_ps/util/striped_lock.hpp
	$(CXX) $(CXXFLAGS) $(INCFLAGS) \
		$(SRC)/petuum_ps/storage/process_storage.o \
		$(SRC)/petuum_ps/storage/client_row.o \
		$(SRC)/petuum_ps/storage/clock_lru.o \
		$(SRC)/petuum_ps/thread/context.o \
		$(SRC)/petuum_ps/util/lock.o \
		$< $(TESTS_LDFLAGS) -o $@

process_storage_test_run: $(TESTS_BIN)/process_storage_test
	env HEAPCHECK=normal GLOG_v=3 GLOG_logtostderr=true $<

#storage_tests_run_all: lru_row_storage_test_run \
#	thread_safe_lru_row_storage_test_run concurrent_row_storage_test_run \
#	dense_row_test_run sparse_row_test_run

#lru_row_storage_test_run: lru_row_storage_test
#	GLOG_v=0 \
#	LD_LIBRARY_PATH=$(THIRD_PARTY_INSTALLED)/lib $(TESTS_BIN)/$<
#
#thread_safe_lru_row_storage_test: \
#	$(STORAGE_TESTS_DIR)/thread_safe_lru_row_storage_test.cpp
#	$(CXX) $(INCFLAGS) $(CXXFLAGS) $^ -lgtest_main $(PETUUM_LIB) \
#		-lboost_system -o $(TESTS_BIN)/$@
#
#thread_safe_lru_row_storage_test_run: thread_safe_lru_row_storage_test
#	GLOG_v=0 \
#	LD_LIBRARY_PATH=$(THIRD_PARTY_INSTALLED)/lib $(TESTS_BIN)/$<
#
#concurrent_row_storage_test: $(STORAGE_TESTS_DIR)/concurrent_row_storage_test.cpp
#	$(CXX) $(INCFLAGS) $(CXXFLAGS) $^ -lgtest_main $(PETUUM_LIB) \
#		-lboost_system -lboost_thread -ltbb -o $(TESTS_BIN)/$@
#
#concurrent_row_storage_test_run: concurrent_row_storage_test
#	GLOG_v=0 GLOG_logtostderr=false \
#	LD_LIBRARY_PATH=$(THIRD_PARTY_INSTALLED)/lib $(TESTS_BIN)/$<
#
#dense_row_test: $(STORAGE_TESTS_DIR)/dense_row_test.cpp
#	$(CXX) $(INCFLAGS) $(CXXFLAGS) $^ -lgtest_main $(PETUUM_LIB) \
#		-o $(TESTS_BIN)/$@
#
#dense_row_test_run: dense_row_test
#	LD_LIBRARY_PATH=$(THIRD_PARTY_INSTALLED)/lib $(TESTS_BIN)/$<
#
#sparse_row_test: $(STORAGE_TESTS_DIR)/sparse_row_test.cpp
#	$(CXX) $(INCFLAGS) $(CPPFLAGS_TESTS) $^ -lgtest_main $(PETUUM_LIB) \
#		-o $(TESTS_BIN)/$@
#
#sparse_row_test_run: sparse_row_test
#	LD_LIBRARY_PATH=$(THIRD_PARTY_INSTALLED)/lib $(TESTS_BIN)/$<
#
#
#lru_eviction_logic_test: $(STORAGE_TESTS_DIR)/lru_eviction_logic_test.cpp
#	$(CXX) $(INCFLAGS) $(CPPFLAGS_TESTS) $^ -lgtest_main $(PETUUM_LIB) \
#		-o $(TESTS_BIN)/$@
#
#lru_eviction_logic_test_run: lru_eviction_logic_test
#	LD_LIBRARY_PATH=$(THIRD_PARTY_INSTALLED)/lib $(TESTS_BIN)/$<
#
#
lru_buffer_test_run: $(TESTS_BIN)/lru_buffer_test
	GLOG_v=0 GLOG_logtostderr=false \
	LD_LIBRARY_PATH=$(THIRD_PARTY_LIB):$$LD_LIBRARY_PATH $<


lru_concurrent_storage_test_run: $(TESTS_BIN)/lru_concurrent_storage_test
	GLOG_v=0 GLOG_logtostderr=false \
	LD_LIBRARY_PATH=$(THIRD_PARTY_LIB):$$LD_LIBRARY_PATH $<
