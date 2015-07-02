FEATURE_TESTS_DIR = $(TESTS)/ml/feature
FEATURE_SRC_DIR=$(SRC)/ml/feature

feature_test_run_all: sparse_feature_test_run dense_feature_test_run

$(TESTS_BIN)/sparse_feature_test: $(FEATURE_TESTS_DIR)/sparse_feature_test.cpp \
	$(FEATURE_SRC_DIR)/sparse_feature.hpp
	$(CXX) $(CXXFLAGS) $(INCFLAGS) $^ $(TESTS_LDFLAGS) -o $@

sparse_feature_test_run: $(TESTS_BIN)/sparse_feature_test
	$<

$(TESTS_BIN)/dense_feature_test: $(FEATURE_TESTS_DIR)/dense_feature_test.cpp \
	$(FEATURE_SRC_DIR)/dense_feature.hpp
	$(CXX) $(CXXFLAGS) $(INCFLAGS) $^ $(TESTS_LDFLAGS) -o $@

dense_feature_test_run: $(TESTS_BIN)/dense_feature_test
	$<

$(TESTS_BIN)/dense_decay_feature_test: \
	$(FEATURE_TESTS_DIR)/dense_decay_feature_test.cpp \
	$(FEATURE_SRC_DIR)/dense_decay_feature.hpp
	$(CXX) $(CXXFLAGS) $(INCFLAGS) $^ $(TESTS_LDFLAGS) -o $@

dense_decay_feature_test_run: $(TESTS_BIN)/dense_decay_feature_test
	$<
