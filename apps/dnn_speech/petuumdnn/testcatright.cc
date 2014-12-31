#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
int main(int argc, char **argv) {
	int num = 30;
	std::ostringstream ss;
	std::ofstream outfile("testout.txt",std::ios::out);
	std::ofstream outfile2("testout2.txt",std::ios::out);
	float term;
	for (int i = 1; i <= num; i++) {
		ss << i;
		std::string feaname = "Train.fea."+ss.str();
		//std::string labelname = "Train.label."+ss.str();
		std::ifstream infile(feaname.c_str(),std::ios::binary);
		infile.read((char *)&term, sizeof(float));
		while (!infile.eof()) {
			outfile << term << " ";
			infile.read((char *)&term, sizeof(float));
		}
		infile.close();
	}
	std::ifstream infile2("Train.fea",std::ios::binary);
		infile2.read((char *)&term, sizeof(float));
		while (!infile2.eof()) {
			outfile2 << term << " ";
			infile2.read((char *)&term, sizeof(float));
		}
	infile2.close();
	outfile.close();
	outfile2.close();
	return 0;
}