TESTS_THREAD_DIR=$(TESTS)/petuum_ps/thread

value_oplog_meta_test: $(TESTS_THREAD_DIR)/value_oplog_meta_test.cpp
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) $(PETUUM_INCFLAGS) \
	$(TESTS_THREAD_DIR)/value_oplog_meta_test.cpp $(PETUUM_PS_LIB) $(PETUUM_LDFLAGS) \
	-lgtest_main -o $(TESTS_THREAD_DIR)/value_oplog_meta_test

run_value_oplog_meta_test: value_oplog_meta_test
	GLOG_logtostderr=true \
	$(TESTS_THREAD_DIR)/value_oplog_meta_test

clean_value_oplog_meta_test:
	rm -rf $(TESTS_THREAD_DIR)/value_oplog_meta_test
