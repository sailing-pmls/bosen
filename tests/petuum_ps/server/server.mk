
PETUUM_SERVER_TESTS_DIR = $(PETUUM_TESTS_DIR)/server

# ========================= Server Tests =========================

table_clock_test: $(PETUUM_SERVER_TESTS_DIR)/table_clock_test.cpp
	$(CXX) $(INCFLAGS) $(CPPFLAGS) $^ $(PETUUM_LIB) -lboost_system \
		-lboost_thread -lgtest_main \
		-o $(TESTS_BIN)/$@
	GLOG_v=0 GLOG_logtostderr=false \
	LD_LIBRARY_PATH=$$LD_LIBRARY_PATH:$(THIRD_PARTY_INSTALLED)/lib $(TESTS_BIN)/$@

distributed_server_test: $(TESTS_COMM_SRC_CPP) $(PETUUM_TESTS_DIR)/server/distributed_server_test.cpp
	$(CXX) $(INCFLAGS) $(CPPFLAGS) $^ $(PETUUM_LIB) -lboost_system \
		-lboost_thread -ltbb -lzmq -lgtest_main \
		-o $(TESTS_BIN)/$@

distributed_server_test_dummy_client: $(TESTS_COMM_SRC_CPP) $(PETUUM_TESTS_DIR)/server/dummy_client.cpp
	$(CXX) $(INCFLAGS) $(CPPFLAGS) $(CPPFLAGS) $^ \
	    $(TESTS_COMM_ST_LIBS) $(TESTS_COMM_DY_LIBS) \
	    -o $(TESTS_BIN)/$@

distributed_server_test_run: distributed_server_test distributed_server_test_dummy_client
	$(TESTS_BIN)/distributed_server_test_dummy_client &
	GLOG_v=0 GLOG_logtostderr=false \
	LD_LIBRARY_PATH=$(THIRD_PARTY_INSTALLED)/lib $(TESTS_BIN)/$<

