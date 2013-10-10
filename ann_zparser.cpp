#include <boost/program_options.hpp>

#include "feature.hpp"
#include "timer.hpp"
#include "ann.hpp"

static void parse(const std::string &input_file, const std::string &output_file)
{
	ioremap::warp::zparser records;
	records.parse_file(input_file);

	int ending_size = 5;
	int word_size = 5 + ending_size;

	ioremap::warp::ann ann;
	ann.dump_features(records, output_file, word_size, ending_size);
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
