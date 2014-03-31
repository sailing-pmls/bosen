WGET = wget --no-check-certificate
MAKE = make -j

THIRD_PARTY := $(shell readlink $(dir $(lastword $(MAKEFILE_LIST))) -f)

THIRD_PARTY_SRC = $(THIRD_PARTY)/src
THIRD_PARTY_INCLUDE = $(THIRD_PARTY)/include
THIRD_PARTY_LIB = $(THIRD_PARTY)/lib

all: boost zeromq

path:
	mkdir -p $(THIRD_PARTY_SRC) $(THIRD_PARTY_INCLUDE)

# ==================== boost ====================

BOOST_HOST = http://downloads.sourceforge.net/project/boost/boost/1.54.0
BOOST_SRC = $(THIRD_PARTY_SRC)/boost_1_54_0.tar.bz2
BOOST_INCLUDE = $(THIRD_PARTY_INCLUDE)/boost

boost: path $(BOOST_INCLUDE)

$(BOOST_INCLUDE): $(BOOST_SRC)
	tar jxf $< -C $(THIRD_PARTY_SRC)
	cd $(basename $(basename $<)); \
	./bootstrap.sh --with-libraries=system,thread --prefix=$(THIRD_PARTY); \
	./b2 install

$(BOOST_SRC):
	$(WGET) $(BOOST_HOST)/$(@F) -O $@


# ==================== zeromq ====================
# NOTE: need uuid-dev

ZMQ_SRC = $(THIRD_PARTY_SRC)/zeromq-3.2.4.tar.gz
ZMQ_LIB = $(THIRD_PARTY_LIB)/libzmq.so

zeromq: path $(ZMQ_LIB)

$(ZMQ_LIB): $(ZMQ_SRC)
	tar zxf $< -C $(THIRD_PARTY_SRC)
	cd $(basename $(basename $<)); \
	./configure --prefix=$(THIRD_PARTY); \
	$(MAKE) install

$(ZMQ_SRC):
	$(WGET) http://download.zeromq.org/$(@F) -O $@

