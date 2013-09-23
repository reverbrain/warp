#include <iostream>
#include <stdexcept>
#include <sstream>

#include <boost/program_options.hpp>

#include <fann.h>
#include <floatfann.h>

#include "feature.hpp"

namespace ioremap { namespace warp {

class check {
	public:
		check(const std::string &input) {
			m_ann = fann_create_from_file(input.c_str());
			if (!m_ann) {
				struct fann_error ferr;
				std::ostringstream ss;
				ss << "Failed to create ANN from file '" << input << "': error: " << fann_get_errstr(&ferr);
				throw std::runtime_error(ss.str());
			}
		}

		void run(const std::string &input) {
		}

		~check() {
			fann_destroy(m_ann);
		}

	private:
		struct fann *m_ann;
};

}}	

int main(int argc, char *argv[])
{
	namespace bpo = boost::program_options;
	std::string input;

	bpo::options_description op("ANN options");
	op.add_options()
		("help", "This help message")
		("input", bpo::value<std::string>(&input), "Input FANN file")
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

	//calc_out = fann_run(ann, input);

	//printf("xor test (%f,%f) -> %f\n", input[0], input[1], calc_out[0]);
}
