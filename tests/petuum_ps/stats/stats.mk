
TESTS_STATS_DY_LIBS = -Wl,-Bdynamic -pthread
TESTS_STATS_ST_LIBS = -Wl,-Bstatic -lboost_system -lglog -lgflags -luuid \
	-ltcmalloc -lunwind -lrt -lnsl -lyaml-cpp

TESTS_STATS_SRC = $(SRC)/petuum_ps/stats
TESTS_STATS_TESTS = $(TESTS_DIR)/petuum_ps/stats

TESTS_STATS_INCFLAGS = -I$(SRC)

TESTS_STATS_FLAGS = -DPETUUM_stats

tests_stats_helloworld: mk_tests_bin $(TESTS_STATS_SRC)/stats.hpp \
	$(TESTS_STATS_SRC)/stats.cpp $(TESTS_STATS_TESTS)/helloworld.cpp
	$(CXX) $(INCFLAGS) $(TESTS_STATS_INCFLAGS) $(CPPFLAGS) $(TESTS_STATS_FLAGS) \
	$(TESTS_STATS_TESTS)/helloworld.cpp $(TESTS_STATS_SRC)/stats.cpp \
	$(TESTS_STATS_ST_LIBS) $(TESTS_STATS_DY_LIBS) -o $(TESTS_BIN)/$@
