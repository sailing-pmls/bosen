#include<fstream>
#include<iostream>
#include<string>
int main(int argc, char **argv) {
	std::ifstream infile(argv[1],std::ios::in);
	std::string datafile = std::string(argv[1])+".Bin";
	std::ofstream outfile(datafile.c_str(),std::ios::binary|std::ios::out);
	float term;
	infile >> term;
	while (!infile.eof()) {
		outfile.write((char *)&term, sizeof(float));		
		infile >> term;
	}
	infile.close();
	outfile.close();
	return 0;
}