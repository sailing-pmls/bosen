# Requires PETUUM_ROOT to be defined

# Modify system dependent parameters for each environment:
JAVA_HOME = /usr/lib/jvm/java-7-openjdk-amd64
HADOOP_HOME = /usr/local/hadoop/hadoop-2.6.0
HAS_HDFS = # Leave empty to build without hadoop.
#HAS_HDFS = -DHAS_HADOOP # Uncomment this line to enable hadoop
ifdef HAS_HDFS
  $(info Hadoop is enabled.)
  # Given $HADOOP_HOME, HDFS_LDFLAGS and HDFS_INCFLAGS usually doesn't
  # need changing, unless your hadoop is not installed in a standard way.
  HDFS_LDFLAGS=-Wl,-rpath,${HADOOP_HOME}/lib/native/ \
               -Wl,-rpath,${HADOOP_HOME}/lib/ \
               -Wl,-rpath,${JAVA_HOME}/jre/lib/amd64/server/ \
               -L${HADOOP_HOME}/lib/native/ \
               -L${JAVA_HOME}/jre/lib/amd64/server/ \
               -lhdfs -ljvm
  HDFS_INCFLAGS = -I${HADOOP_HOME}/include
else
  $(info Hadoop is disabled)
	HDFS_LDFLAGS =
  HDFS_INCFLAGS =
endif

PETUUM_SRC = $(PETUUM_ROOT)/src
PETUUM_LIB = $(PETUUM_ROOT)/lib
PETUUM_THIRD_PARTY = $(PETUUM_ROOT)/../../../third_party
PETUUM_THIRD_PARTY_SRC = $(PETUUM_THIRD_PARTY)/src
PETUUM_THIRD_PARTY_INCLUDE = $(PETUUM_THIRD_PARTY)/include
PETUUM_THIRD_PARTY_LIB = $(PETUUM_THIRD_PARTY)/lib
PETUUM_THIRD_PARTY_BIN = $(PETUUM_THIRD_PARTY)/bin

PETUUM_CXX = g++
PETUUM_CXXFLAGS = -O3 \
           -std=c++11 \
           -Wall \
					 -Wno-sign-compare \
           -fno-builtin-malloc \
           -fno-builtin-calloc \
           -fno-builtin-realloc \
           -fno-builtin-free \
           -fno-omit-frame-pointer

PETUUM_INCFLAGS = -I$(PETUUM_SRC) -I$(PETUUM_THIRD_PARTY_INCLUDE)
PETUUM_INCFLAGS += $(HDFS_INCFLAGS)
PETUUM_LDFLAGS = -Wl,-rpath,$(PETUUM_THIRD_PARTY_LIB) \
          -L$(PETUUM_THIRD_PARTY_LIB) \
          -pthread -lrt -lnsl \
          -lzmq \
          -lboost_thread \
          -lboost_system \
          -lglog \
          -lgflags \
          -ltcmalloc \
          -lconfig++ \
          -lsnappy \
          -lboost_system \
          -lboost_thread \
	  -lyaml-cpp \
	  -lleveldb
PETUUM_LDFLAGS += $(HDFS_LDFLAGS)

PETUUM_PS_LIB = $(PETUUM_LIB)/libpetuum-ps.a
PETUUM_PS_SN_LIB = $(PETUUM_LIB)/libpetuum-ps-sn.a
PETUUM_ML_LIB = $(PETUUM_LIB)/libpetuum-ml.a
