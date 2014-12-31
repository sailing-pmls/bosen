#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <stdlib.h>

int main(int argc, char ** argv) {
	std::ifstream infile(argv[1],std::ios::in);
	std::ofstream outfile(argv[2], std::ios::out);
	std::string line, num, control_line;
	int beginN, num_layers, num_epoch, stepsize, minibatch, num_smp, num_iter;
	int inputlayer = atoi(argv[3]), outputlayer = atoi(argv[4]);
	
	// Hidden layers
	std::getline(infile, line);
	beginN = line.find(":");
	num = line.substr(beginN+1);
	sscanf(num.c_str(), "%d", &num_layers);
	
	// Num of units in each layer
	int hidden[num_layers];
	std::getline(infile, line);
	beginN = line.find(":");
	num = line.substr(beginN+1);
	control_line = "%d";
	for (int i = 1; i < num_layers; i++) {
		control_line += " %d";
	}
	sscanf(num.c_str(), control_line.c_str(), hidden);
	
	// Num_epoch
	std::getline(infile, line);
	beginN = line.find(":");
	num = line.substr(beginN+1);
	sscanf(num.c_str(), "%d", &num_epoch);

	// Stepsize
	std::getline(infile, line);
	beginN = line.find(":");
	num = line.substr(beginN+1);
	sscanf(num.c_str(), "%d", &stepsize);	
	
	// Mini-batch size
	std::getline(infile, line);
	beginN = line.find(":");
	num = line.substr(beginN+1);
	sscanf(num.c_str(), "%d", &minibatch);
	
	// Number of evaluated samples
	std::getline(infile, line);
	beginN = line.find(":");
	num = line.substr(beginN+1);
	sscanf(num.c_str(), "%d", &num_smp);
	
	// Number of evaluated iterations
	std::getline(infile, line);
	beginN = line.find(":");
	num = line.substr(beginN+1);
	sscanf(num.c_str(), "%d", &num_iter);	
	
	// Now, composing the DNN config
	outfile << "num_layers: " << num_layers+2 << std::endl;
	outfile << "num_units_in_each_layer: " << inputlayer << " ";
	for (int i = 0;  i < num_layers; i++) {
		outfile << hidden[i] << " ";
	}
	outfile << outputlayer << std::endl;
	outfile << "num_epochs: " << num_epoch << std::endl;
	outfile << "stepsize: " << stepsize << std::endl;
	outfile << "mini_batch_size: " << minibatch << std::endl;
	outfile << "num_smp_evaluate: " << num_smp << std::endl;
	outfile << "num_iters_evaluate: " << num_iter << std::endl;

	infile.close();
	outfile.close();
	return 0;
}
