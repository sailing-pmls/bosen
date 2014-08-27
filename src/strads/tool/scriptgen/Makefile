CPP=g++
GCC=gcc
MCPP=mpic++
INCFLGS = -I./comm_zmq
INCFLGS += -I./

CFLAG = -g -O2 -Wall -Wno-strict-aliasing -std=c++0x

COMMONSRC=cfgen.cpp 

biglasso: $(COMMONSRC)
#	$(CPP) $(INCFLGS) $(COMMONSRC) $(LIBS) -o cfgen $(CFLAG) 
	$(CPP) $(INCFLGS) $(COMMONSRC)  -o cfgen $(CFLAG) 

clean:
	rm -f cfgen