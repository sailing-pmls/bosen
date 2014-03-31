
CONSISTENCY_TESTS_DIR = $(TESTS)/petuum_ps/consistency
CONSISTENCY_TESTS_SRC = $(wildcard $(CONSISTENCY_TESTS_DIR)/*.cpp)
CONSISTENCY_TESTS = $(CONSISTENCY_TESTS_SRC:$(CONSISTENCY_TESTS_DIR)/%.cpp=$(TESTS_BIN)/%)

consistency_tests: $(CONSISTENCY_TESTS)

# ========================= Consistency Tests =========================

# To allow dummy test classes to be included as "tests/petuum_ps/..."
TEST_INCFLAGS = -I$(PROJ_DIR)

$(TESTS_BIN)/ssp_consistency_controller_test: \
	$(CONSISTENCY_TESTS_DIR)/ssp_consistency_controller_test.cpp \
	$(SRC)/petuum_ps/storage/process_storage.o \
	$(SRC)/petuum_ps/consistency/ssp_consistency_controller.o \
	$(SRC)/petuum_ps/storage/clock_lru.o \
	$(SRC)/petuum_ps/storage/sparse_row.hpp \
	$(SRC)/petuum_ps/include/row_access.hpp \
	$(SRC)/petuum_ps/include/configs.hpp \
	$(SRC)/petuum_ps/storage/client_row.o \
	$(SRC)/petuum_ps/thread/context.o \
	$(SRC)/petuum_ps/thread/bg_workers.o \
	$(SRC)/petuum_ps/oplog/oplog.hpp \
	$(SRC)/petuum_ps/oplog/oplog_partition.o \
	$(SRC)/petuum_ps/util/lock.o \
	$(SRC)/petuum_ps/util/vector_clock.o \
	$(SRC)/petuum_ps/util/striped_lock.hpp
	$(CXX) $(CXXFLAGS) $(INCFLAGS) \
		$(SRC)/petuum_ps/consistency/ssp_consistency_controller.o \
		$(SRC)/petuum_ps/storage/process_storage.o \
		$(SRC)/petuum_ps/thread/bg_workers.o \
		$(SRC)/petuum_ps/storage/client_row.o \
		$(SRC)/petuum_ps/storage/clock_lru.o \
		$(SRC)/petuum_ps/thread/context.o \
		$(SRC)/petuum_ps/util/vector_clock.o \
		$(SRC)/petuum_ps/oplog/oplog_partition.o \
		$(SRC)/petuum_ps/util/lock.o \
		$< $(TESTS_LDFLAGS) -o $@

ssp_consistency_controller_test_run: $(TESTS_BIN)/ssp_consistency_controller_test
	env HEAPCHECK=normal GLOG_v=3 GLOG_logtostderr=true $<

#consistency_tests_run_all: consistency_controller_test_run \
#	op_log_manager_test_run

#consistency_controller_test: \
#	$(PETUUM_CONSISTENCY_TESTS_DIR)/consistency/consistency_controller_test.cpp
#	$(CXX) $(INCFLAGS) $(TEST_INCFLAGS) $(CPPFLAGS) \
#		$(SRC)/petuum_ps/comm_handler/*.cpp \
#		$(SRC)/petuum_ps/consistency/*.cpp \
#                $^ $(PETUUM_LIB) \
#		-lzmq -lgtest_main -lboost_system -lboost_thread -o $(TESTS_BIN)/$@

#consistency_controller_test_run: consistency_controller_test
#	GLOG_v=0 \
#	LD_LIBRARY_PATH=$(THIRD_PARTY_INSTALLED)/lib $(TESTS_BIN)/$<

#op_log_manager_test: $(BIN)/stats.o $(BIN)/table_partitioner.o \
#	$(PETUUM_CONSISTENCY_TESTS_DIR)/consistency/op_log_manager_test.cpp
#	$(CXX) $(INCFLAGS) $(TEST_INCFLAGS) $(CPPFLAGS) $^ $(PETUUM_LIB) \
#		-lgtest_main -o $(TESTS_BIN)/$@

#op_log_manager_test_run: op_log_manager_test
#	GLOG_v=0 \
#	LD_LIBRARY_PATH=$(THIRD_PARTY_INSTALLED)/lib $(TESTS_BIN)/$<
