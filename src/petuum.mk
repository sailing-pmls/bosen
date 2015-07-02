PS_DIR = $(SRC)/petuum_ps
PS_SRC = $(shell find $(PS_DIR) -type f -name "*.cpp")
PS_HEADERS = $(shell find $(PS_DIR) -type f -name "*.hpp")
PS_OBJ = $(PS_SRC:.cpp=.o)
IO_DIR = $(SRC)/io

PS_COMMON_DIR = $(SRC)/petuum_ps_common
PS_COMMON_SRC = $(shell find $(PS_COMMON_DIR) -type f -name "*.cpp")
PS_COMMON_HEADERS = $(shell find $(PS_COMMON_DIR) -type f -name "*.hpp")
PS_COMMON_OBJ = $(PS_COMMON_SRC:.cpp=.o)

ML_DIR = $(SRC)/ml
ML_SRC = $(shell find $(ML_DIR) $(IO_DIR) -type f -name "*.cpp")
ML_HEADERS = $(shell find $(ML_DIR) -type f -name "*.hpp")
ML_OBJ = $(ML_SRC:.cpp=.o)

# ================== library ==================

# PS Library
PS_LIB = $(LIB)/libpetuum-ps.a

$(PS_LIB): $(PS_OBJ) $(PS_COMMON_OBJ) path
	ar csrv $@ $(PS_OBJ) $(PS_COMMON_OBJ)

$(PS_OBJ): %.o: %.cpp $(PS_HEADERS) $(PS_COMMON_HEADERS)
	$(CXX) $(CXXFLAGS) $(INCFLAGS) -c $< -o $@

$(PS_COMMON_OBJ): %.o: %.cpp $(PS_COMMON_HEADERS)
	$(CXX) $(CXXFLAGS) $(INCFLAGS) -c $< -o $@

ps_lib: $(PS_LIB)


# ML Library
ML_LIB = $(LIB)/libpetuum-ml.a

$(ML_LIB): $(ML_OBJ) path
	ar csrv $@ $(ML_OBJ)

$(ML_OBJ): %.o: %.cpp $(ML_HEADERS)
	$(CXX) $(CXXFLAGS) $(INCFLAGS) $(HDFS_INCFLAGS) $(HAS_HDFS) $(HDFS_LDFLAGS) -c $< -o $@

ml_lib: $(ML_LIB)


clean_src:
	rm -rf $(IO_OBJ)

.PHONY: ps_lib ml_lib
