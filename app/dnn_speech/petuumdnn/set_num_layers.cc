#include <iostream>
#include <fstream>
#include <string>
#include <stdlib.h>

int main(int argc, char **argv) {
	std::ifstream infile(argv[1], std::ios::in);
	std::ifstream infile2(argv[3], std::ios::in);
	std::ofstream outfile(argv[2], std::ios::out);
	std::string line, num;
	int num_layers, t, t2;
	std::getline(infile2, line);
	t = line.find(":");
	num = line.substr(t+1);	
	sscanf(num.c_str(), "%d", &num_layers);
	std::getline(infile, line);
	while (!infile.eof()) {
		t = line.find("<NumComponents>");
		if ( t != std::string::npos ) {
			std::string head = line.substr(0, t+16);
			t2 = line.find("<Components>");
			std::string tail = line.substr(t2);
			outfile << head << " " << num_layers*2 << " " << tail << std::endl;
		} else {
			t2 = line.find("</FixedAffineComponent>");
			if ( t2 != std::string::npos ) {
				outfile << line << std::endl;
				break;
			}
			outfile << line << std::endl;
		}
		std::getline(infile, line);
	}
	infile.close();
	infile2.close();
	outfile.close();
	return 0;
}