MALLOC_SCALABILITY_DIR = $(TESTS)/third_party/malloc_scalability

malloc_scalability: $(MALLOC_SCALABILITY_DIR)/malloc_scalability

$(MALLOC_SCALABILITY_DIR)/malloc_scalability: $(MALLOC_SCALABILITY_DIR)/malloc_scalability.cpp
	$(PETUUM_CXX) $(PETUUM_CXXFLAGS) $(PETUUM_INCFLAGS) \
	$(MALLOC_SCALABILITY_DIR)/malloc_scalability.cpp $(PETUUM_PS_LIB) \
	$(PETUUM_LDFLAGS) -o $@

run_malloc_scalability: malloc_scalability
	GLOG_logtostderr=true \
	GLOG_minloglevel=0 \
	$(MALLOC_SCALABILITY_DIR)/malloc_scalability \
	--num_threads 2 \
	--malloc_size 8192

clean_malloc_scalability:
	rm -rf $(MALLOC_SCALABILITY_DIR)/malloc_scalability

.PHONY: clean_malloc_scalability malloc_scalability run_malloc_scalability