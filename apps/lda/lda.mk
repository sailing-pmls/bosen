
LDA_DIR = $(APPS)/lda
LDA_SRC = $(wildcard $(LDA_DIR)/*.cpp)
LDA_HDR = $(wildcard $(LDA_DIR)/*.hpp)
LDA_OBJ = $(LDA_SRC:%.cpp=%.o)
LDA = $(BIN)/lda_main

lda: $(LDA)

$(LDA): $(LDA_OBJ) $(PS_CLIENT)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

$(LDA_OBJ): %.o: %.cpp $(LDA_HDR)
	$(CXX) $(CXXFLAGS) -I$(LDA_DIR) $(INCFLAGS) -c $< -o $@

lda_clean:
	rm -f $(LDA_OBJ) $(LDA)

.PHONY: lda lda_clean
