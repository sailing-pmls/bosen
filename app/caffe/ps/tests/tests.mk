TESTS_LDFLAGS = $(LDFLAGS) -lgtest_main

#include $(TESTS_MK)

include $(TESTS)/petuum_ps/util/util.mk
include $(TESTS)/petuum_ps/thread/thread.mk
include $(TESTS)/petuum_ps/independent/independent.mk
include $(TESTS)/petuum_ps/oplog/oplog.mk
include $(TESTS)/petuum_ps/storage/storage.mk
include $(TESTS)/ml/feature/feature.mk
include $(TESTS)/ml/util/util.mk
include $(TESTS)/ml/disk_stream/disk_stream.mk
include $(TESTS)/ml/ml.mk
include $(TESTS)/third_party/cuckoo_perf/cuckoo_perf.mk
include $(TESTS)/third_party/cuckoo_map/cuckoo_map.mk
include $(TESTS)/third_party/malloc_scalability/malloc_scalability.mk
include $(TESTS)/third_party/memcpy_scalability/memcpy_scalability.mk
