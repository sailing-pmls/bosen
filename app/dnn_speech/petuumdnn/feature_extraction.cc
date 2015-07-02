#include "base/kaldi-common.h"
#include "util/common-utils.h"
#include "hmm/transition-model.h"
#include "nnet2/train-nnet.h"
#include "nnet2/am-nnet.h"
#include "nnet2/nnet-example-functions.h"
#include "nnet2/nnet-example.h"
#include "matrix/kaldi-matrix.h"
#include <fstream>

void load_lda(char *filename, int dim, float **ldapara) {
	std::ifstream infile(filename, std::ios::in);
	for (int i = 0; i < dim+1; i++) {
		for (int j = 0; j < dim; j++) {
			infile >> ldapara[i][j];
		}
	}
	infile.close();

}

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

	std::string labelfile = "Train.label."+string(argv[4]);
	std::string feafile = "Train.fea."+string(argv[4]);
	std::ofstream out1(labelfile.c_str(),std::ios::out);
//	std::ofstream out2(feafile.c_str(),std::ios::binary|std::ios::out);
	std::ofstream out2(feafile.c_str(),std::ios::out);
    
    std::string examples_rspecifier = argv[1];
    int64 num_examples = 0;
    load_lda(argv[2],dim,ldapara);
	SequentialNnetExampleReader example_reader(examples_rspecifier);

	for (; !example_reader.Done(); example_reader.Next(), num_examples++) {
kaldi::nnet2::NnetExample value = example_reader.Value();
//	out1.write((char *)&value.labels[0].first, sizeof(int32));
	out1 << value.labels[0].first << '\n';
//input_frames.Write(os, binary); // can be read as regular Matrix.
//	outfile << "Matrix Row = " << value.input_frames.NumRows() << "   Matrixx Column = " << value.input_frames.NumCols() << "\n";
	kaldi::Matrix<kaldi::BaseFloat> temp_mat(value.input_frames.NumRows(), value.input_frames.NumCols(), kaldi::kUndefined);
	value.input_frames.CopyToMat(&temp_mat);
	float tmp[dim];
	for (int i =0; i < dim; i++) tmp[i] = ldapara[dim][i];
	for (kaldi::MatrixIndexT i = 0; i < value.input_frames.NumRows(); i++) {
//		outfile << "\n";
		for (kaldi::MatrixIndexT j = 0; j < value.input_frames.NumCols(); j++)	{
//			outfile << temp_mat(i, j) << " ";
			for (int k = 0; k < dim; k++) {
				tmp[k] += temp_mat(i,j)*ldapara[k][i*value.input_frames.NumCols()+j]; 
			}
		}
//			out2.write((char *)&temp_mat(i,j), sizeof(float));
	}
//	for (int k = 0; k < dim; k++) out2.write((char *) &tmp[k], sizeof(float));
	for (int k = 0; k < dim; k++) out2 << tmp[k] << " ";
	out2 << std::endl;
	}
	for (int i = 0; i < dim+1; i++) delete ldapara[i];
	delete [] ldapara;
	out1.close();
	out2.close();
    KALDI_LOG << "Finished, processed " << num_examples;
    return (num_examples == 0 ? 1 : 0);
  } catch(const std::exception &e) {
    std::cerr << e.what() << '\n';
    return -1;
  }
}

