#include <fann.h>

#include <fstream>
#include <boost/program_options.hpp>

int main(int argc, char *argv[])
{
	namespace bpo = boost::program_options;
	int num_neurons_hidden = 128;
	int num_input = 32;
	int num_output = 64;
	int num_layers = 3;
	const float desired_error = (const float) 0.001;
	int max_epochs = 200;
	int epochs_between_reports = 1;
	int total;
	std::string input;

	bpo::options_description op("ANN options");
	op.add_options()
		("help", "This help message")
		("input", bpo::value<std::string>(&input), "Input file")
		("hidden", bpo::value<int>(&num_neurons_hidden), "Number of neurons in the hidden layer")
		;

	bpo::variables_map vm;
	try {
		bpo::store(bpo::parse_command_line(argc, argv, op), vm);
		bpo::notify(vm);
	} catch (const std::exception &e) {
		std::cerr << e.what() << std::endl << op << std::endl;
		return -1;
	}

	if (!vm.count("input")) {
		std::cerr << "You must provide input file\n" << op << std::endl;
		return -1;
	}

	std::ifstream in_tmp(input.c_str());

	std::string line;
	std::getline(in_tmp, line);

	int num = sscanf(line.c_str(), "%d %d %d", &total, &num_input, &num_output);
	if (num != 3) {
		std::cerr << "Invalid input file: first line must be '$total_examples_num $input_features_num $output_features_num'" << std::endl;
		return -1;
	}

	int err;
	struct fann *ann = fann_create_standard(num_layers, num_input, num_neurons_hidden, num_output);

	fann_set_activation_function_hidden(ann, FANN_SIGMOID_SYMMETRIC);
	fann_set_activation_function_output(ann, FANN_SIGMOID_SYMMETRIC);

	std::cout << "Total examples: " << total <<
		", total layers: " << num_layers <<
		", input neurons: " << num_input <<
		", hidden neurons: " << num_neurons_hidden <<
		", output neurons: " << num_output <<
		std::endl;

	fann_train_on_file(ann, input.c_str(), max_epochs, epochs_between_reports, desired_error);

	std::string output = input + ".net." + boost::lexical_cast<std::string>(getpid());
	err = fann_save(ann, output.c_str());
	if (err) {
		struct fann_error ferr;
		std::cerr << "Save to '" << output << "': error: " << fann_get_errstr(&ferr) << std::endl;
	}

	fann_destroy(ann);

	return 0;
}
