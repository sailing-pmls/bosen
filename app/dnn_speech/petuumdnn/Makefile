
all:
EXTRA_CXXFLAGS = -Wno-sign-compare
include ../kaldi.mk

LDFLAGS += $(CUDA_LDFLAGS)
LDLIBS += $(CUDA_LDLIBS)

BINFILES = feature_extraction ali_count parse_head set_num_layers

OBJFILES =

# Add this dependency to force cuda-compiled.o to be rebuilt when we reconfigure.
cuda-compiled.o: ../kaldi.mk


TESTFILES =

ADDLIBS = ../nnet2/kaldi-nnet2.a ../nnet/kaldi-nnet.a ../gmm/kaldi-gmm.a \
         ../decoder/kaldi-decoder.a ../lat/kaldi-lat.a ../hmm/kaldi-hmm.a  \
         ../transform/kaldi-transform.a ../tree/kaldi-tree.a ../thread/kaldi-thread.a \
         ../cudamatrix/kaldi-cudamatrix.a ../matrix/kaldi-matrix.a \
         ../util/kaldi-util.a ../base/kaldi-base.a 

include ../makefiles/default_rules.mk
