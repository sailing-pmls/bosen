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
    std::string rspecifier = argv[1];
    std::ofstream outfile("Transform.txt",std::ios::out);
    
    SequentialBaseFloatMatrixReader mat_reader(rspecifier);
    
    int32 num_done = 0;
    int64 num_rows_done = 0;
    
    for (; !mat_reader.Done(); mat_reader.Next()) {
      std::string key = mat_reader.Key();
      Matrix<double> mat(mat_reader.Value());
	  //outfile << mat.NumRows() << " " << mat.NumCols() << std::endl;
	  for (int i = 0; i < mat.NumRows(); i++) {
		for (int j = 0; j < mat.NumCols(); j++) {
			outfile << mat(i,j) << " ";
		}
	  }
	  outfile << std::endl;
	  num_done++;
    }
    
    KALDI_LOG << "Summed rows " << num_done << " matrices, "
              << num_rows_done << " rows in total.";
    
    return (num_done != 0 ? 0 : 1);
  } catch(const std::exception &e) {
    std::cerr << e.what();
    return -1;
  }
}
