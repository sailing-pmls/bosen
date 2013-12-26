
HELLOWORLD_DIR = $(APPS)/helloworld
HELLOWORLD_SRC = $(wildcard $(HELLOWORLD_DIR)/*.cpp)
HELLOWORLD = $(HELLOWORLD_SRC:$(HELLOWORLD_DIR)/%.cpp=$(BIN)/%)

helloworld: $(HELLOWORLD)

$(HELLOWORLD): $(BIN)/%: $(HELLOWORLD_DIR)/%.cpp $(PS_CLIENT)
	$(CXX) $(CXXFLAGS) $(INCFLAGS) $^ $(LDFLAGS) -o $@

helloworld_clean:
	rm -f $(HELLOWORLD)

.PHONY: helloworld helloworld_clean

