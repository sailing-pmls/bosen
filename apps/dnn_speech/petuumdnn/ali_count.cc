#include<fstream>
#include<iostream>
#include<vector>
#include<stdlib.h>

int main(int argc, char **argv) {
	std::vector<int> count;
	std::ifstream infile(argv[1],std::ios::in);
	std::ofstream outfile(argv[2], std::ios::out);
	std::ofstream outfile2(argv[3], std::ios::out);
	int term, sumNum = 0, sumAli = atoi(argv[4]), input_dim = atoi(argv[5]);
	count.resize(sumAli, 0);
	infile >> term;
	while (!infile.eof()) {
		count[term]++;
		sumNum++;
		infile >> term;
	}
	infile.close();
	outfile<<"</Components> </Nnet> [";
	for (int i = 0; i < count.size(); i++) {
		outfile << " " << double(count[i])/sumNum;
	}
	outfile << " ]" << std::endl;
	outfile2 << sumNum << " " << input_dim << " " << sumAli << std::endl;
	outfile.close();
	outfile2.close();
	return 0;
}
    
