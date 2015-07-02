MEMCPY_SCALABILITY_DIR = $(TESTS)/third_party/memcpy_scalability

memcpy_scalability: $(MEMCPY_SCALABILITY_DIR)/memcpy_scalability

$(MEMCPY_SCALABILITY_DIR)/memcpy_scalability: $(MEMCPY_SCALABILITY_DIR)/memcpy_scalability.cpp
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) $(PETUUM_INCFLAGS) \
	$(MEMCPY_SCALABILITY_DIR)/memcpy_scalability.cpp $(PETUUM_PS_LIB) \
	$(PETUUM_LDFLAGS) -o $@

run_memcpy_scalability: memcpy_scalability
	GLOG_logtostderr=true \
	GLOG_minloglevel=0 \
	$(MEMCPY_SCALABILITY_DIR)/memcpy_scalability \
	--num_threads 1 \
	--malloc_size 8192

clean_memcpy_scalability:
	rm -rf $(MEMCPY_SCALABILITY_DIR)/memcpy_scalability

.PHONY: clean_memcpy_scalability memcpy_scalability run_memcpy_scalability