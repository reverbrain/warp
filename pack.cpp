#include <boost/program_options.hpp>

#include "lex.hpp"

int main(int argc, char *argv[])
{
	namespace bpo = boost::program_options;

	bpo::options_description generic("Parser options");

	std::string input, output, msgin, gram;
	generic.add_options()
		("help", "This help message")
		("input", bpo::value<std::string>(&input), "Input Zaliznyak dictionary file")
		("output", bpo::value<std::string>(&output), "Output msgpack file")
		("msgpack-input", bpo::value<std::string>(&msgin), "Input msgpack file")
		("grammar", bpo::value<std::string>(&gram), "Grammar string suitable for ioremap::warp::parser, space separates single word descriptions")
		;

	bpo::positional_options_description p;
	p.add("words", -1);

	std::vector<std::string> words;
	bpo::options_description hidden("Hidden options");
	hidden.add_options()
		("words", bpo::value<std::vector<std::string>>(&words), "lookup words")
	;

	bpo::options_description cmdline_options;
	cmdline_options.add(generic).add(hidden);

	bpo::variables_map vm;
	bpo::store(bpo::command_line_parser(argc, argv).options(cmdline_options).positional(p).run(), vm);
	bpo::notify(vm);

	namespace iw = ioremap::warp;

	if (!(vm.count("input") && vm.count("output")) && !vm.count("msgpack-input")) {
		std::cerr << generic;
		return -1;
	}

	if (vm.count("input") && vm.count("output")) {
		iw::packer pack(output);
		iw::zparser records;
		records.set_process(std::bind(&iw::packer::zprocess, &pack, std::placeholders::_1, std::placeholders::_2));
		records.parse_file(input);
	}

	if (vm.count("msgpack-input")) {
		iw::lex l;
		l.load(msgin);

		if (!vm.count("grammar")) {
			for (auto & w : words)
				l.lookup(w);
		} else {
			std::vector<std::string> tokens;
			std::istringstream iss(gram);
			std::copy(std::istream_iterator<std::string>(iss), std::istream_iterator<std::string>(), std::back_inserter<std::vector<std::string>>(tokens));

			std::vector<iw::grammar> vgram = l.generate(tokens);

			for (auto pos : l.grammar_deduction(vgram, words)) {
				for (size_t i = 0; i < vgram.size(); ++i) {
					std::cout << words[i + pos] << " ";
				}

				std::cout << std::endl;
			}
		}
	}

	return 0;
}


