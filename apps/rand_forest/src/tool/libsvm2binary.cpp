#include <iostream>
#include <ml/include/ml.hpp>
#include <petuum_ps_common/include/petuum_ps.hpp>
#include <fstream>
#include <gflags/gflags.h>
#include <glog/logging.h>

DEFINE_string(input_file, "", "Path to input file");
DEFINE_string(output_file, "", "Path to output file");
DEFINE_int32(num_data, 0, "number of examples in the input file");
DEFINE_int32(feature_dim, 0, "number of features in the input file");
DEFINE_bool(feature_one_based, false, "feature index start from 1?");
DEFINE_bool(label_one_based, false, "label index start from 1?");

int main(int argc, char* argv[]) {
	std::vector<petuum::ml::AbstractFeature<float>* > features;
	std::vector<int32_t> labels;
	int32_t num_data, feature_dim;
	bool feature_one_based, label_one_based;

	google::ParseCommandLineFlags(&argc, &argv, true);
	google::InitGoogleLogging(argv[0]);

	num_data = FLAGS_num_data;
	feature_dim = FLAGS_feature_dim;
	feature_one_based = FLAGS_feature_one_based;
	label_one_based = FLAGS_label_one_based;


	LOG(INFO) << "Start reading data from input file ...";
	// Read data
	petuum::ml::ReadDataLabelLibSVM(FLAGS_input_file, feature_dim, num_data, &features, &labels, 
			feature_one_based, label_one_based);
	LOG(INFO) << "Finish reading data.";
	LOG(INFO) << "Example size: " << features.size();
	LOG(INFO) << "Feature size: " << feature_dim;

	CHECK_EQ(features.size(), labels.size()) << "`features` dimension do not match wiel `labels` dimension";

	std::ofstream out(FLAGS_output_file, std::ios::out | std::ios::binary);


	LOG(INFO) << "Start writing binary data to output file ...";
	float feature_val;
	for (int i = 0; i < features.size(); i++) {
		// write label
		out.write(reinterpret_cast<char*> (&(labels[i])), sizeof(int32_t));	
		// write features
		for (int j = 0; j < feature_dim; j++) {
			feature_val = (*(features[i]))[j];
			out.write(reinterpret_cast<char*> (&feature_val), sizeof(float));
		}
	}
	out.close();
	LOG(INFO) << "Writing finish. Program shut down.";
	return 0;
}
