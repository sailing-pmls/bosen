TESTS_INDEPENDENT_DIR=$(TESTS)/petuum_ps/independent

inheritance: $(TESTS_INDEPENDENT_DIR)/inheritance.cpp
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) $(PETUUM_INCFLAGS) \
	$(TESTS_INDEPENDENT_DIR)/inheritance.cpp $(PETUUM_PS_LIB) $(PETUUM_LDFLAGS) \
	-o $(TESTS_INDEPENDENT_DIR)/inheritance

run_inheritance: inheritance
	$(TESTS_INDEPENDENT_DIR)/inheritance

clean_inheritance:
	rm -rf $(TESTS_INDEPENDENT_DIR)/inheritance

map_vec: $(TESTS_INDEPENDENT_DIR)/map_vec.cpp
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) $(PETUUM_INCFLAGS) \
	$(TESTS_INDEPENDENT_DIR)/map_vec.cpp $(PETUUM_PS_LIB) $(PETUUM_LDFLAGS) \
	-o $(TESTS_INDEPENDENT_DIR)/map_vec

run_map_vec: map_vec
	$(TESTS_INDEPENDENT_DIR)/map_vec
clean_map_vec:
	rm -rf $(TESTS_INDEPENDENT_DIR)/map_vec

map_size_test: $(TESTS_INDEPENDENT_DIR)/map_size_test.cpp
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) $(PETUUM_INCFLAGS) \
	$(TESTS_INDEPENDENT_DIR)/map_size_test.cpp $(PETUUM_PS_LIB) $(PETUUM_LDFLAGS) \
	-o $(TESTS_INDEPENDENT_DIR)/map_size_test

run_map_size_test: map_size_test
	$(TESTS_INDEPENDENT_DIR)/map_size_test

clean_map_size_test:
	rm -rf $(TESTS_INDEPENDENT_DIR)/map_size_test

bitset_cmp: $(TESTS_INDEPENDENT_DIR)/bitset_cmp.cpp
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) $(PETUUM_INCFLAGS) \
	$(TESTS_INDEPENDENT_DIR)/bitset_cmp.cpp $(PETUUM_PS_LIB) $(PETUUM_LDFLAGS) \
	-o $(TESTS_INDEPENDENT_DIR)/bitset_cmp

run_bitset_cmp: bitset_cmp
	$(TESTS_INDEPENDENT_DIR)/bitset_cmp

clean_bitset_cmp:
	rm -rf $(TESTS_INDEPENDENT_DIR)/bitset_cmp

shared_lock_test: $(TESTS_INDEPENDENT_DIR)/shared_lock_test.cpp
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) $(PETUUM_INCFLAGS) \
	$(TESTS_INDEPENDENT_DIR)/shared_lock_test.cpp $(PETUUM_PS_LIB) $(PETUUM_LDFLAGS) \
	-o $(TESTS_INDEPENDENT_DIR)/shared_lock_test

run_shared_lock_test: shared_lock_test
	GLOG_logtostderr=true \
	GLOG_v=-1 \
	GLOG_minloglevel=0 \
	$(TESTS_INDEPENDENT_DIR)/shared_lock_test

clean_shared_lock_test:
	rm -rf $(TESTS_INDEPENDENT_DIR)/shared_lock_test

std_lock_test: $(TESTS_INDEPENDENT_DIR)/std_lock_test.cpp
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) $(PETUUM_INCFLAGS) \
	$(TESTS_INDEPENDENT_DIR)/std_lock_test.cpp $(PETUUM_PS_LIB) $(PETUUM_LDFLAGS) \
	-o $(TESTS_INDEPENDENT_DIR)/std_lock_test

run_std_lock_test: std_lock_test
	GLOG_logtostderr=true \
	GLOG_v=-1 \
	GLOG_minloglevel=0 \
	$(TESTS_INDEPENDENT_DIR)/std_lock_test

clean_std_lock_test:
	rm -rf $(TESTS_INDEPENDENT_DIR)/std_lock_test

spin_lock_test: $(TESTS_INDEPENDENT_DIR)/spin_lock_test.cpp
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) $(PETUUM_INCFLAGS) \
	$(TESTS_INDEPENDENT_DIR)/spin_lock_test.cpp $(PETUUM_PS_LIB) $(PETUUM_LDFLAGS) \
	-o $(TESTS_INDEPENDENT_DIR)/spin_lock_test

run_spin_lock_test: spin_lock_test
	GLOG_logtostderr=true \
	GLOG_v=-1 \
	GLOG_minloglevel=0 \
	$(TESTS_INDEPENDENT_DIR)/spin_lock_test

clean_spin_lock_test:
	rm -rf $(TESTS_INDEPENDENT_DIR)/spin_lock_test

core_dump_test: $(TESTS_INDEPENDENT_DIR)/core_dump.cpp
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) $(PETUUM_INCFLAGS) \
	$(TESTS_INDEPENDENT_DIR)/core_dump.cpp $(PETUUM_PS_LIB) $(PETUUM_LDFLAGS) \
	-o $(TESTS_INDEPENDENT_DIR)/core_dump_test

run_core_dump_test: core_dump_test
	GLOG_logtostderr=true \
	GLOG_v=-1 \
	GLOG_minloglevel=0 \
	$(TESTS_INDEPENDENT_DIR)/core_dump_test

clean_core_dump_test:
	rm -rf $(TESTS_INDEPENDENT_DIR)/core_dump_test