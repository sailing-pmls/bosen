#include<fstream>
#include<iostream>
#include<string>
#include<stdlib.h>
int main(int argc, char **argv) {
	std::ifstream infile(argv[1],std::ios::binary|std::ios::in);
	std::string datafile = std::string(argv[1])+".ASC";
	std::ofstream outfile(datafile.c_str(),std::ios::out);
	float term; int dim = atoi(argv[2]);
	int count = 0;
	infile.read((char *)&term, sizeof(float));
	while (!infile.eof()) {
		count++;
		outfile << term << " ";
		if (count == dim) {
			outfile << std::endl;
			count = 0;
		}
		infile.read((char *)&term, sizeof(float));
	}
	infile.close();
	outfile.close();
	return 0;
}