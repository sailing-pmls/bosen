
PS_DIR = $(SRC)/petuum_ps

PS_SRC = $(shell find $(PS_DIR) -type f -name *.cpp)
PS_HEADERS = $(shell find $(PS_DIR) -type f -name *.hpp)
PS_OBJ = $(PS_SRC:.cpp=.o)

# ================== client library ==================

PS_LIB = $(LIB)/libpetuum-ps.a

ps_lib: $(PS_LIB)

$(PS_LIB): $(PS_OBJ) path
	ar csrv $@ $(PS_OBJ)

$(PS_OBJ): %.o: %.cpp $(PS_HEADERS)
	$(CXX) $(CXXFLAGS) $(INCFLAGS) -c $< -o $@

.PHONY: ps_lib
