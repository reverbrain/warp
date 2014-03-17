#include "warp/spell.hpp"

#include <boost/program_options.hpp>

using namespace ioremap;

int main(int argc, char *argv[])
{
	namespace bpo = boost::program_options;

	bpo::options_description generic("Fuzzy search tool options");

	int num;
	std::string enc_dir;

	generic.add_options()
		("help", "This help message")
		("ngram", bpo::value<int>(&num)->default_value(3), "Number of symbols in each ngram")
		("msgpack", "Whether files are msgpack packed Zaliznyak dictionary files")
		("encoding-dir", bpo::value<std::string>(&enc_dir), "Load encodings from given wookie directory")
		;

	bpo::positional_options_description p;
	p.add("text", 1).add("files", -1);

	std::vector<std::string> files;
	std::string text;

	bpo::options_description hidden("Positional options");
	hidden.add_options()
		("text", bpo::value<std::string>(&text), "text to lookup")
		("files", bpo::value<std::vector<std::string>>(&files), "files to parse")
	;

	bpo::variables_map vm;

	try {
		bpo::options_description cmdline_options;
		cmdline_options.add(generic).add(hidden);

		bpo::store(bpo::command_line_parser(argc, argv).options(cmdline_options).positional(p).run(), vm);
		bpo::notify(vm);
	} catch (const std::exception &e) {
		std::cerr << "Invalid options: " << e.what() << "\n" << generic << std::endl;
		return -1;
	}

	if (!text.size()) {
		std::cerr << "No text to lookup\n" << generic << "\n" << hidden << std::endl;
		return -1;
	}

	if (!files.size()) {
		std::cerr << "There are no input files\n" << generic << "\n" << hidden << std::endl;
		return -1;
	}

	try {
		if (vm.count("msgpack")) {
			warp::spell sp(num, files);
			sp.search(text);
		} else {
			warp::fuzzy f(num);

			for (auto file = files.begin(); file != files.end(); ++file) {
				std::ifstream in(file->c_str());
				if (in.bad()) {
					std::cerr << "spell: could not feed file '" << *file << "': " << in.rdstate() << std::endl;
					continue;
				}

				std::ostringstream ss;

				ss << in.rdbuf();

				std::string text = ss.str();

				f.feed_text(text);
			}

			f.search(text);
		}
	} catch (const std::exception &e) {
		std::cerr << "Exception: " << e.what() << std::endl;
		return -1;
	}

	return 0;
}

