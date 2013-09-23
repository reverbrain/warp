#include <boost/program_options.hpp>

#include "feature.hpp"

static void parse(const std::string &input_file, const std::string &output_file)
{
	std::ifstream in(input_file.c_str());

	ioremap::warp::base_holder records;

	std::string line;
	std::string word;

	while (std::getline(in, line)) {
		if (line.substr(0, 5) == "@ID: ") {
			// skip next line - it contains original word
			if (!std::getline(in, line))
				break;

			continue;
		}

		records.parse_dict_string(line);
	}

	int ending_size = 8;
	int word_size = 24 + ending_size;

	records.dump_features(output_file, word_size, ending_size);
}

int main(int argc, char *argv[])
{
	namespace bpo = boost::program_options;

	bpo::options_description op("Parser options");

	std::string input, output;
	op.add_options()
		("help", "This help message")
		("input", bpo::value<std::string>(&input), "Input file")
		("output", bpo::value<std::string>(&output), "Output file")
		;

	bpo::variables_map vm;
	bpo::store(bpo::parse_command_line(argc, argv, op), vm);
	bpo::notify(vm);

	if (!vm.count("input") || !vm.count("output")) {
		std::cerr << "You must provide input and output files\n" << op << std::endl;
		return -1;
	}

	parse(input, output);
}
