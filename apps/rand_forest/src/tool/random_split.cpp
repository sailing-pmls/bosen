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

int main(int argc, char* argv[]) {
	google::ParseCommandLineFlags(&argc, &argv, true);
	google::InitGoogleLogging(argv[0]);

	CHECK(FLAGS_valid_percent > 0 && FLAGS_valid_percent <= 1);
	CHECK(FLAGS_train_percent > 0 && FLAGS_train_percent <= 1);

	LOG(INFO) << "Starting Reading Data ...";
	std::ifstream file(FLAGS_filename, std::ifstream::binary);

	std::string file_str((std::istreambuf_iterator<char>(file)),
			std::istreambuf_iterator<char>());
	std::istringstream data_stream(file_str);	

	/* Count # of rows */
	int row = 0, total_row;
	for (std::string line; std::getline(data_stream, line); row++);
	LOG(INFO) << "Line " << row << std::endl;
	total_row = row;

	/* Load data again */
	data_stream.clear();
	data_stream.str(file_str);

	std::vector<std::string> train_vec;
	std::vector<std::string> test_vec;
	int valid_row = total_row * FLAGS_valid_percent, valid_row_left = valid_row;
	int train_row = valid_row * FLAGS_train_percent, train_row_left = train_row;
	int test_row = valid_row - train_row;

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
	std::copy(train_vec.begin(), train_vec.end(), std::ostream_iterator<std::string>(s, " "));
	LOG(INFO) << "Training set: " << s.str();
	s.clear();
	s.str("");
	std::copy(test_vec.begin(), test_vec.end(), std::ostream_iterator<std::string>(s, " "));
	LOG(INFO) << "Testing set: " << s.str();
	return 0;
}
