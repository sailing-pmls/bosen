# Boost is too heavy to host on github..
THIRD_PARTY_HOST = http://www.cs.cmu.edu/~jinlianw/third_party
BOOST_HOST = http://downloads.sourceforge.net/project/boost/boost/1.54.0

third_party: gflags \
             glog \
             zeromq \
             boost \
             gperftools \
             libconfig

.PHONY: third_party

# ===================== gflags ===================

GFLAGS_SRC = $(THIRD_PARTY_SRC)/gflags-2.0.tar.gz
GFLAGS_LIB = $(THIRD_PARTY_LIB)/libgflags.so

gflags: $(GFLAGS_LIB)

$(GFLAGS_LIB): $(GFLAGS_SRC)
	tar zxf $< -C $(THIRD_PARTY_SRC)
	cd $(basename $(basename $<)); \
	./configure --prefix=$(THIRD_PARTY); \
	make install

$(GFLAGS_SRC):
	wget $(THIRD_PARTY_HOST)/$(@F) -O $@

# ===================== glog =====================

GLOG_SRC = $(THIRD_PARTY_SRC)/glog-0.3.3.tar.gz
GLOG_LIB = $(THIRD_PARTY_LIB)/libglog.so

glog: $(GLOG_LIB)

$(GLOG_LIB): $(GLOG_SRC)
	tar zxf $< -C $(THIRD_PARTY_SRC)
	cd $(basename $(basename $<)); \
	./configure --prefix=$(THIRD_PARTY); \
	make install

$(GLOG_SRC):
	wget $(THIRD_PARTY_HOST)/$(@F) -O $@

# ==================== zeromq ====================

ZMQ_SRC = $(THIRD_PARTY_SRC)/zeromq-3.2.3.tar.gz
ZMQ_LIB = $(THIRD_PARTY_LIB)/libzmq.so

zeromq: $(ZMQ_LIB)

$(ZMQ_LIB): $(ZMQ_SRC)
	tar zxf $< -C $(THIRD_PARTY_SRC)
	cd $(basename $(basename $<)); \
	./configure --prefix=$(THIRD_PARTY); \
	make install

$(ZMQ_SRC):
	wget $(THIRD_PARTY_HOST)/$(@F) -O $@
	wget $(THIRD_PARTY_HOST)/zmq.hpp -P $(THIRD_PARTY_INCLUDE)

# ==================== boost ====================

BOOST_SRC = $(THIRD_PARTY_SRC)/boost_1_54_0.tar.bz2
BOOST_INCLUDE = $(THIRD_PARTY_INCLUDE)/boost

boost: $(BOOST_INCLUDE)

$(BOOST_INCLUDE): $(BOOST_SRC)
	tar jxf $< -C $(THIRD_PARTY_SRC)
	cd $(basename $(basename $<)); \
	./bootstrap.sh \
		--with-libraries=system,thread \
		--prefix=$(THIRD_PARTY); \
	./b2 install

$(BOOST_SRC):
	wget $(BOOST_HOST)/$(@F) -O $@

# ================== gperftools =================

GPERFTOOLS_SRC = $(THIRD_PARTY_SRC)/gperftools-2.1.tar.gz
GPERFTOOLS_LIB = $(THIRD_PARTY_LIB)/libtcmalloc.so

gperftools: $(GPERFTOOLS_LIB)

$(GPERFTOOLS_LIB): $(GPERFTOOLS_SRC)
	tar zxf $< -C $(THIRD_PARTY_SRC)
	cd $(basename $(basename $<)); \
	./configure --prefix=$(THIRD_PARTY) --enable-frame-pointers; \
	make install

$(GPERFTOOLS_SRC):
	wget $(THIRD_PARTY_HOST)/$(@F) -O $@

# ==================== libconfig ===================

LIBCONFIG_SRC = $(THIRD_PARTY_SRC)/libconfig-1.4.9.tar.gz
LIBCONFIG_LIB = $(THIRD_PARTY_LIB)/libconfig++.so

libconfig: $(LIBCONFIG_LIB)

$(LIBCONFIG_LIB): $(LIBCONFIG_SRC)
	tar zxf $< -C $(THIRD_PARTY_SRC)
	cd $(basename $(basename $<)); \
	./configure --prefix=$(THIRD_PARTY); \
	make install

$(LIBCONFIG_SRC):
	wget $(THIRD_PARTY_HOST)/$(@F) -O $@

