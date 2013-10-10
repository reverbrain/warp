#include <boost/program_options.hpp>

#include "feature.hpp"
#include "timer.hpp"

static void parse(const std::string &input_file, const std::string &output_file)
{
	std::ifstream in(input_file.c_str());

	ioremap::warp::base_holder records;
	ioremap::warp::timer t;
	ioremap::warp::timer total;

	std::string line;
	std::string word;

	long lines = 0;
	long chunk = 100000;
	long duration;
	while (std::getline(in, line)) {
		if (++lines % chunk == 0) {
			duration = t.restart();
			std::cout << "Read and parsed " << lines << " lines, took: " << duration << " msecs, speed: " << chunk * 1000 / duration << " lines/sec" << std::endl;
		}

		if (line.substr(0, 5) == "@ID: ") {
			// skip next line - it contains original word
			if (!std::getline(in, line))
				break;

			continue;
		}

		records.parse_dict_string(line);
	}
	duration = total.restart();
	std::cout << "Read and parsed " << lines << " lines, took: " << duration << " msecs, speed: " << lines * 1000 / duration << " lines/sec" << std::endl;
	return;

	int ending_size = 5;
	int word_size = 5 + ending_size;

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
