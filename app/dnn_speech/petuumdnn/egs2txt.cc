#include "base/kaldi-common.h"
#include "util/common-utils.h"
#include "hmm/transition-model.h"
#include "nnet2/train-nnet.h"
#include "nnet2/am-nnet.h"
#include "nnet2/nnet-example-functions.h"
#include "nnet2/nnet-example.h"
#include "matrix/kaldi-matrix.h"
#include <fstream>

int main(int argc, char *argv[]) {
  try {
    using namespace kaldi;
    using namespace kaldi::nnet2;
    typedef kaldi::int32 int32;
    typedef kaldi::int64 int64;
    int dim = atoi(argv[3]);
    float **ldapara;
	ldapara = new float *[dim+1];
	for (int i = 0; i < dim+1; i++) {
		ldapara[i] = new float [dim];
	}

	std::string labelfile = "Train.label";
	std::string feafile = "Train.fea";
	std::ofstream out1(labelfile.c_str(),std::ios::app);
	std::ofstream out2(feafile.c_str(),std::ios::app);
    
    std::string examples_rspecifier = argv[1];
    int64 num_examples = 0;
	SequentialNnetExampleReader example_reader(examples_rspecifier);

	for (; !example_reader.Done(); example_reader.Next(), num_examples++) {
kaldi::nnet2::NnetExample value = example_reader.Value();
	out1 << value.labels.size() << " ";
	for (int i = 0; i < value.labels.size(); i++) {
		out1 << value.labels[i].first << " " << value.labels[i].second << " ";
	}	
	out1 << '\n';
	kaldi::Matrix<kaldi::BaseFloat> temp_mat(value.input_frames.NumRows(), value.input_frames.NumCols(), kaldi::kUndefined);
	value.input_frames.CopyToMat(&temp_mat);

	for (kaldi::MatrixIndexT i = 0; i < value.input_frames.NumRows(); i++) {
		for (kaldi::MatrixIndexT j = 0; j < value.input_frames.NumCols(); j++)	{
			out2 << temp_mat(i, j) << " ";
			}
		}
		out2 << std::endl;
	}
	out1.close();
	out2.close();
    KALDI_LOG << "Finished, processed " << num_examples;
    return (num_examples == 0 ? 1 : 0);
  } catch(const std::exception &e) {
    std::cerr << e.what() << '\n';
    return -1;
  }
}

