CPP=g++
INCFLGS += -I./

LIBS=  -lpthread
CFLAG = -g -O2 -Wall -Wno-strict-aliasing

MMT2BINSRC=mmt2bin.cpp mm_io.cpp
MMT2BINHEADER=mmt2bin.hpp

mmt2bin: $(MMT2BINSRC)  $(MMT2BINHEADER)
	$(CPP) $(MMT2BINSRC) $(INCFLGS) -o mmt2bin $(IOCFLAG) $(CFLAG) $(LIBS) 
