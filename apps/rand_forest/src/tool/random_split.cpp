#include <iostream>
#include <fstream>
#include <sstream>
#include <random>
#include <string>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <thread>
#include <iterator>

DEFINE_string(filename, "", "Path to the input dataset");
DEFINE_double(valid_percent, 1.0, "The percentage of data will be included in train and test");
DEFINE_double(train_percent, 0.8, "The percentage of data will be included in training set");
DEFINE_string(train_filename, "", "Path to the output of training set");
DEFINE_string(test_filename, "", "Path to the output of testing set");

int main(int argc, char* argv[]) {
	google::ParseCommandLineFlags(&argc, &argv, true);
	google::InitGoogleLogging(argv[0]);

	CHECK(FLAGS_valid_percent > 0 && FLAGS_valid_percent <= 1);
	CHECK(FLAGS_train_percent > 0 && FLAGS_train_percent <= 1);

	LOG(INFO) << "Starting Reading Data ...";
	std::ifstream file(FLAGS_filename, std::ifstream::binary);

	CHECK(file != nullptr) << "Cannot open file!";

	std::string file_str((std::istreambuf_iterator<char>(file)),
			std::istreambuf_iterator<char>());
	std::istringstream data_stream(file_str);	
	LOG(INFO) << "Finish Reading Data ...";

	/* Count # of rows */
	long long row = 0, total_row;
	for (std::string line; std::getline(data_stream, line); row++);
	LOG(INFO) << "There are " << row << " lines in the input file.";
	total_row = row;

	/* Load data again */
	data_stream.clear();
	data_stream.str(file_str);

	std::vector<std::string> train_vec;
	std::vector<std::string> test_vec;
	long long valid_row = total_row * FLAGS_valid_percent, valid_row_left = valid_row;
	long long train_row = valid_row * FLAGS_train_percent, train_row_left = train_row;
	long long test_row = valid_row - train_row;

	std::random_device rd;
	std::mt19937 mt(rd());
	std::uniform_real_distribution<float> dist(0, 1);
	for (std::string line; std::getline(data_stream, line);) {
		/* choose a row */
		if (dist(mt) < (float)valid_row_left / row) {
			if (dist(mt) < (float)train_row_left / valid_row_left) {
				train_vec.push_back(line);
				train_row_left--;
			} else {
				test_vec.push_back(line);
			}
			valid_row_left--;
		}
		row--;
	}

	CHECK_EQ(train_vec.size(), train_row);
	CHECK_EQ(test_vec.size(), test_row);

	std::stringstream s;
	std::ofstream train_out(FLAGS_train_filename.c_str()), test_out(FLAGS_test_filename.c_str());

	CHECK(train_out != nullptr) << "Cannot open train.out";
	CHECK(test_out != nullptr) << "Cannot open test.out";

	std::copy(train_vec.begin(), train_vec.end(), std::ostream_iterator<std::string>(s, "\n"));
	LOG(INFO) << "Training set size: " << train_vec.size();
	train_out << s.str();
	s.clear();
	s.str("");
	std::copy(test_vec.begin(), test_vec.end(), std::ostream_iterator<std::string>(s, "\n"));
	LOG(INFO) << "Testing set: " << test_vec.size();
	test_out << s.str();

	train_out.close();
	test_out.close();
	return 0;
}
