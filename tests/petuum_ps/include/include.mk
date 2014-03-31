
PETUUM_STORAGE_TESTS_DIR = $(TESTS_DIR)/petuum_ps

# ========================= Include Tests =========================
# To allow dummy test classes to be included as "tests/petuum_ps/..."
TEST_INCFLAGS = -I$(PROJ_DIR)

table_tests_run_all: table_test_run table_group_test_run

table_test: $(PETUUM_STORAGE_TESTS_DIR)/include/table_test.cpp
	$(CXX) $(INCFLAGS) $(TEST_INCFLAGS) $(CPPFLAGS) \
		$(SRC)/petuum_ps/comm_handler/*.cpp $^ $(PETUUM_LIB) \
		-lzmq -lgtest_main -lboost_system -lboost_thread -o $(TESTS_BIN)/$@

table_test_run: table_test
	GLOG_v=2 \
	LD_LIBRARY_PATH=$(THIRD_PARTY_INSTALLED)/lib $(TESTS_BIN)/$<

table_group_test_run: $(PETUUM_STORAGE_TESTS_DIR)/include/table_group_test.cpp
	$(CXX) $(INCFLAGS) $(TEST_INCFLAGS) $(CPPFLAGS) \
		$(SRC)/petuum_ps/comm_handler/*.cpp $^ $(PETUUM_LIB) \
		-lzmq -lgtest_main -lboost_system -lboost_thread -o $(TESTS_BIN)/$@
	GLOG_v=0 \
	LD_LIBRARY_PATH=$(THIRD_PARTY_INSTALLED)/lib $(TESTS_BIN)/$@
