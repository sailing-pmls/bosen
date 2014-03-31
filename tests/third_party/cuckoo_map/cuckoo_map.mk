
CUCKOO_MAP_TESTS_DIR = $(TESTS)/third_party/cuckoo_map

$(TESTS_BIN)/cuckoo_map_test: $(CUCKOO_MAP_TESTS_DIR)/cuckoo_map_test.cpp
	$(CXX) $(CXXFLAGS) $(INCFLAGS) $^ $(TESTS_LDFLAGS) -o $@

cuckoo_map_test_run: $(TESTS_BIN)/cuckoo_map_test
	GLOG_v=3 GLOG_logtostderr=true $<
