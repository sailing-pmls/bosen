
TOOLS_DIR = $(APPS)/tools
TOOLS_SRC = $(wildcard $(TOOLS_DIR)/*.cpp)
TOOLS = $(TOOLS_SRC:$(TOOLS_DIR)/%.cpp=$(BIN)/%)

tools: $(TOOLS)

$(TOOLS): $(BIN)/%: $(TOOLS_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(INCFLAGS) $^ $(LDFLAGS) -o $@

tools_clean:
	rm -f $(TOOLS)

.PHONY: tools tools_clean
