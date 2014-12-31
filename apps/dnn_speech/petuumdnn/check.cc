#include <fstream>
#include <iostream>
#include <cmath>

int main(int argc, char **argv) {
	double term1, term2, max = -1;
	std::ifstream infile1("Transform.txt",std::ios::in);
	std::ifstream infile2("Train.fea.1",std::ios::in);
	std::ofstream outfile("check.result", std::ios::out);
	infile1 >> term1; infile2 >> term2;
	while (!infile1.eof()) {
		if (fabs(term1-term2) > max) max = fabs(term1-term2);
		infile1 >> term1; infile2 >> term2;
	}
	outfile << max;
	infile1.close();
	infile2.close();
	outfile.close();
	return 0;
}