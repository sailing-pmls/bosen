#include <iostream>
#include <fstream>
#include <string>
#include <stdlib.h>

int main(int argc, char **argv) {
	std::ifstream infile(argv[1], std::ios::in);
	std::ofstream outfile(argv[2], std::ios::out);
	std::ofstream outfile2(argv[3], std::ios::out);
	std::string line, num;
	int t, t2, t3, t4;
	bool flag = false;
	std::getline(infile, line);
	while (!infile.eof()) {
		t = line.find("<NumComponents>");
		if ( t != std::string::npos ) {
			std::string head = line.substr(0, t+16);
			t2 = line.find("<Components>");
			std::string tail = line.substr(t2);
			outfile << head << " " << 0 << " " << tail << std::endl;
		} else {
			t2 = line.find("</FixedAffineComponent>");
			if ( t2 != std::string::npos ) {
				outfile << line << std::endl;
				break;
			}
			outfile << line << std::endl;
		}
		if (flag == true) {
			t3 = line.find("[");
			t4 = line.find("]");
			int beg = 0, len = -1;
			if (t3 != std::string::npos) beg = t3+1;
			if (t4 != std::string::npos) len = t4 - t3 - 1;
			if (len != -1) outfile2 << line.substr(beg, len) << std::endl; else outfile2 << line.substr(beg) << std::endl;
		}
		if ( line.find("<FixedAffineComponent>") != std::string::npos ) flag = true;
		std::getline(infile, line);
	}
	infile.close();
	outfile2.close();
	outfile.close();
	return 0;
}