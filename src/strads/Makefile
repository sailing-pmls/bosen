CPP=g++
GCC=gcc
MCPP=mpic++
INCFLGS = -I./comm_zmq -I./ -Ithirdparty/include

LIBS= -Wl,-rpath,thirdparty/lib -Lthirdparty/lib -lm -lboost_thread -lboost_system -lpthread  -lzmq -lcrypto -lssl

#LIBS= -Wl,-rpath,thirdparty/lib -Wl,-rpath,/usr/local/lib -Lthirdparty/lib -lm -lboost_thread -lboost_system -lpthread  -lzmq -lcrypto -lssl

# -lboost_program_options -lboost_thread-mt

#-ltbb -lgsl -lgslcblas
CFLAG = -g -O2 -Wall -Wno-strict-aliasing -std=c++0x -Wno-sign-compare -Wno-unused-variable -Wno-unused-but-set-variable -Wno-sequence-point

COMMONSRC=main.cpp options.cpp utility.cpp iohandler.cpp scheduler-bg-common.cpp strads-queue.cpp getcorr.cpp ./comm_zmq/comm_handler.cpp ./comm_zmq/zmq_util.cpp usercom.cpp syscom.hpp binaryio.cpp
# mm_io.cpp mm_io.hpp
COMMONHEADERS=options.hpp basectx.hpp threadctx.hpp  utility.hpp iohandler.hpp scheduler-bg.hpp strads-queue.hpp getcorr.hpp ./comm_zmq/comm_handler.hpp ./comm_zmq/zmq_util.hpp sched-delta.hpp dpartitioner.hpp binaryio.hpp

OOCBIGLASSOINC = -I./app/oocbiglasso
OOCBIGLASSOSRC=./app/oocbiglasso/biglasso.cpp ./app/oocbiglasso/scheduler-bg-app.cpp ./sched-delta.cpp ./dpartitioner.cpp ./syscom.cpp ./ooc.cpp ./app/oocbiglasso/lassoio.cpp
OOCBIGLASSOHEADERS=./app/oocbiglasso/usercom.hpp ./app/oocbiglasso/userdefined.hpp ./app/oocbiglasso/interthreadcom.hpp

SKELETONINC = -I./app/skeleton
SKELETONSRC=./app/skeleton/skeleton.cpp ./app/skeleton/scheduler-bg-app.cpp ./sched-delta.cpp ./dpartitioner.cpp ./syscom.cpp ./ooc.cpp
SKELETONHEADERS=./app/skeleton/usercom.hpp ./app/skeleton/userdefined.hpp

all: thirdparty_lib schedlasso noschedlasso

thirdparty_lib:
	cd thirdparty; \
	make -f strads_3rd.mk

schedlasso: $(OOCBIGLASSOSRC) $(COMMONHEADERS) $(OOCBIGLASSOHEADERS)
	$(MCPP) $(INCFLGS) $(OOCBIGLASSOINC) $(OOCBIGLASSOSRC) $(COMMONSRC) $(LIBS) -o dist-schedlasso $(CFLAG) -DRCU_SCHEDULER -DINTERFERENCE_CHECKING -DDELTA -DOBJ_USING_RESIDUAL -DDELTA_UPDATE_OPTIMIZATION -DBINARY_IO_FORMAT
#-DPRINT_SUMMARY_OOCINFO
#-DSHOW_VM_PEAK

noschedlasso: $(OOCBIGLASSOSRC) $(COMMONHEADERS) $(OOCBIGLASSOHEADERS)
	$(MCPP) $(INCFLGS) $(OOCBIGLASSOINC) $(OOCBIGLASSOSRC) $(COMMONSRC) $(LIBS) -o dist-noschedlasso $(CFLAG) -DRCU_SCHEDULER -DBINARY_IO_FORMAT 

clean:
	rm -f dist-schedlasso
	rm -f dist-noschedlasso
	rm -f *~	
	rm -rf \#*
	rm -rf ./app/skeleton/\#*
	rm -f  ./app/skeleton/~	
	rm -rf ./app/biglasso/\#*
	rm -f  ./app/biglasso/~
