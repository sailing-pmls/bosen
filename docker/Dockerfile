FROM ubuntu:14.04
MAINTAINER Abutalib Aghayev <agayev@cs.cmu.edu>

RUN sudo apt-get -y update && sudo apt-get -y install g++ make autoconf git libtool uuid-dev openssh-server cmake libopenmpi-dev openmpi-bin libssl-dev libnuma-dev python-dev python-numpy python-scipy python-yaml protobuf-compiler subversion libxml2-dev libxslt-dev zlibc zlib1g zlib1g-dev libbz2-1.0 libbz2-dev

RUN git clone https://github.com/petuum/bosen.git
RUN cd /bosen && git clone https://github.com/petuum/third_party.git
RUN cd /bosen/third_party && make
RUN cd /bosen && cp defns.mk.template defns.mk && make
RUN cd /bosen/app/NMF && make

ENV GLOG_logtostderr 1
ENV GLOG_v -1
ENV GLOG_minloglevel 0

ENTRYPOINT ["/bosen/app/NMF/bin/nmf_main"]
CMD ["--help"]
