TESTS_OPLOG_DIR=$(TESTS)/petuum_ps/oplog

oplog_benchmark: $(TESTS_OPLOG_DIR)/oplog_benchmark.cpp
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) $(PETUUM_INCFLAGS) \
	$(TESTS_OPLOG_DIR)/oplog_benchmark.cpp $(PETUUM_PS_LIB) $(PETUUM_LDFLAGS) \
	-o $(TESTS_OPLOG_DIR)/oplog_benchmark

run_oplog_benchmark: oplog_benchmark
	GLOG_logtostderr=true \
	$(TESTS_OPLOG_DIR)/oplog_benchmark

clean_oplog_benchmark:
	rm -rf oplog_benchmark

append_only_oplog_benchmark: $(TESTS_OPLOG_DIR)/append_only_oplog_benchmark.cpp
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) $(PETUUM_INCFLAGS) \
	$(TESTS_OPLOG_DIR)/append_only_oplog_benchmark.cpp $(PETUUM_PS_LIB) $(PETUUM_LDFLAGS) \
	-o $(TESTS_OPLOG_DIR)/append_only_oplog_benchmark

run_append_only_oplog_benchmark: append_only_oplog_benchmark
	GLOG_logtostderr=true \
	$(TESTS_OPLOG_DIR)/append_only_oplog_benchmark \
	--row_oplog_type 0 \
	--row_capacity 1000 \
	--oplog_dense_serialized \
	--append_only_oplog_type DenseBatchInc \
	--append_only_buff_capacity 10485760 \
	--per_thread_append_only_buff_pool_size 3 \
	--max_row_id 1000 \
	--num_iters 5 \
	--apply_freq -1

clean_append_only_oplog_benchmark:
	rm -rf append_only_oplog_benchmark

.PHONY: oplog_benchmark run_oplog_benchmark clean_oplog_benchmark \
	append_only_oplog_benchmark run_append_only_oplog_benchmark \
	clean_append_only_oplog_benchmark
