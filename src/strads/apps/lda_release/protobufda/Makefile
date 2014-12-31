runda: ./lda.proto
	protoc -I./ --cpp_out=./ ./lda.proto
	cp lda.pb.cc ../lda.pb.cpp
	cp lda.pb.h ../lda.pb.hpp

clean:
	rm -rf $(LASSO_BIN)/lassoll


.PHONY: lassoll clean
