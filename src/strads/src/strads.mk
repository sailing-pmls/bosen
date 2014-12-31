ST_DIR = $(SRC)/strads
ST_COMMON_DIR = $(SRC)/strads_common

ST_SRC = $(shell find $(ST_DIR) -type f -name "*.cpp")
ST_HEADERS = $(shell find $(ST_DIR) -type f -name "*.hpp")
ST_OBJ = $(ST_SRC:.cpp=.o)

ST_COMMON_SRC = $(shell find $(ST_COMMON_DIR) -type f -name "*.cpp")
ST_COMMON_HEADERS = $(shell find $(ST_COMMON_DIR) -type f -name "*.hpp")
ST_COMMON_OBJ = $(ST_COMMON_SRC:.cpp=.o)

# ================== client library ==================

ST_LIB = $(LIB)/libpetuum-strads.a

st_lib: $(ST_LIB)

$(ST_LIB): $(ST_OBJ) $(ST_COMMON_OBJ) path
	ar csrv $@ $(ST_OBJ) $(ST_COMMON_OBJ)

$(ST_OBJ): %.o: %.cpp $(ST_HEADERS) $(ST_COMMON_HEADERS)
	$(CXX) $(CXXFLAGS) $(INCFLAGS) -c $< -o $@

$(ST_SN_LIB): $(ST_SN_OBJ) $(ST_COMMON_OBJ) path
	ar csrv $@ $(ST_SN_OBJ) $(ST_COMMON_OBJ)

$(ST_SN_OBJ): %.o: %.cpp $(ST_SN_HEADERS) $(ST_COMMON_HEADERS)
	$(CXX) $(CXXFLAGS) $(INCFLAGS) -c $< -o $@

$(ST_COMMON_OBJ): %.o: %.cpp $(ST_COMMON_HEADERS)
	$(CXX) $(CXXFLAGS) $(INCFLAGS) -c $< -o $@

.PHONY: st_lib