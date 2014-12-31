runda: ./ccdmf.proto
	protoc -I./ --cpp_out=./ ./ccdmf.proto
	cp ccdmf.pb.cc ../ccdmf.pb.cpp
	cp ccdmf.pb.h ../ccdmf.pb.hpp

clean:
	rm -rf ccdmf.pb.cc
	rm -rf ccdmf.pb.h

.PHONY: clean
