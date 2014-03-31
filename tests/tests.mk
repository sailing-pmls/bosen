
#TESTS_MK = $(shell find $(TESTS) -mindepth 2 -type f -name *.mk)

TESTS_LDFLAGS = $(LDFLAGS) -lgtest_main

#include $(TESTS_MK)

include $(TESTS)/petuum_ps/storage/storage.mk
include $(TESTS)/petuum_ps/consistency/consistency.mk
include $(TESTS)/petuum_ps/util/util.mk
include $(TESTS)/third_party/cuckoo_perf/cuckoo_perf.mk
include $(TESTS)/third_party/cuckoo_map/cuckoo_map.mk
include $(TESTS)/petuum_ps/thread/thread.mk
