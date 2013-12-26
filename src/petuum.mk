
PS_DIR = $(SRC)/petuum_ps

ps_all: ps_client \
        ps_server_int	\
        ps_server_float

.PHONY: ps_all

# ================== client library ==================

PS_CLIENT = $(LIB)/libps_client.a

PS_SERVER_INT_SRC = $(PS_DIR)/server/server_exec_int.cpp
PS_SERVER_FLOAT_SRC = $(PS_DIR)/server/server_exec_float.cpp
PS_SRC = $(filter-out $(PS_SERVER_INT_SRC) \
                      $(PS_SERVER_FLOAT_SRC), \
                      $(shell find $(PS_DIR) -type f -name *.cpp))
PS_OBJ = $(PS_SRC:.cpp=.o)

ps_client: $(NEED_MKDIR) $(PS_CLIENT)

$(PS_CLIENT): $(PS_OBJ)
	ar csrv $@ $^

# NOTE: Assuming foo.o depends on foo.cpp and foo.hpp
$(PS_OBJ): %.o: %.cpp %.hpp
	$(CXX) $(CXXFLAGS) $(INCFLAGS) -c $< -o $@

# ================ server executable =================

PS_SERVER_INT = $(BIN)/ps_server_int
PS_SERVER_FLOAT = $(BIN)/ps_server_float

PS_SRC_ALL = $(wildcard $(PS_DIR)/*/*)

ps_server_int: $(NEED_MKDIR) $(PS_SERVER_INT)
ps_server_float: $(NEED_MKDIR) $(PS_SERVER_FLOAT)

$(PS_SERVER_INT): $(PS_SERVER_INT_SRC) $(PS_OBJ) $(PS_SRC_ALL)
	$(CXX) $(CXXFLAGS) $(INCFLAGS) $< $(PS_OBJ) $(LDFLAGS) -o $@

$(PS_SERVER_FLOAT): $(PS_SERVER_FLOAT_SRC) $(PS_OBJ) $(PS_SRC_ALL)
	$(CXX) $(CXXFLAGS) $(INCFLAGS) $< $(PS_OBJ) $(LDFLAGS) -o $@

