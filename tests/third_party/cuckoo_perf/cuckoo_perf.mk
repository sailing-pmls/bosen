
CUCKOO_PERF_DIR = $(TESTS)/third_party/cuckoo_perf
CUCKOO_PERF_SRC = $(wildcard $(CUCKOO_PERF_DIR)/*.cc)
CUCKOO_PERF = $(CUCKOO_PERF_SRC:$(CUCKOO_PERF_DIR)/%.cc=$(TESTS_BIN)/%)

cuckoo_perf: $(CUCKOO_PERF)

# This require TBB. After TBB is removed from third party this might not work.
$(CUCKOO_PERF): $(TESTS_BIN)/%: $(CUCKOO_PERF_DIR)/%.cc
	$(CXX) $(CXXFLAGS) $(INCFLAGS) $< $(TESTS_LDFLAGS) -o $@

cuckoo_read_throughput: $(TESTS_BIN)/cuckoo_read_throughput
	$< --load 95

tbb_read_throughput: $(TESTS_BIN)/tbb_read_throughput
	$< --load 95

cuckoo_insert_throughput: $(TESTS_BIN)/cuckoo_insert_throughput
	env HEAPCHECK=normal $< --begin-load 60

tbb_insert_throughput: $(TESTS_BIN)/tbb_insert_throughput
	$< --begin-load 90
