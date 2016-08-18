#include "warp/fuzzy.hpp"

#include <boost/program_options.hpp>

#include <iostream>

using namespace ioremap;


int main(int argc, char *argv[])
{
	namespace bpo = boost::program_options;

	bpo::options_description generic("Parser options");

	std::string rocksdb_path;
	std::string replace, around;
	float boundary;
	generic.add_options()
		("help", "This help message")
		("rocksdb", bpo::value<std::string>(&rocksdb_path)->required(), "Rocksdb database")
		("lang-model-replace", bpo::value<std::string>(&replace), "Error language model: letter replacement mapping file")
		("lang-model-around", bpo::value<std::string>(&around), "Error language model: letter keyboard invlid pressing mapping file")
		("boundary", bpo::value<float>(&boundary)->default_value(0.1), "Internal boundary to normalized frequencies")
		;

	bpo::options_description cmdline_options;
	cmdline_options.add(generic);

	bpo::variables_map vm;

	try {
		bpo::store(bpo::command_line_parser(argc, argv).options(cmdline_options).run(), vm);

		if (vm.count("help")) {
			std::cout << generic << std::endl;
			return 0;
		}

		bpo::notify(vm);
	} catch (const std::exception &e) {
		std::cerr << "Invalid options: " << e.what() << "\n" << generic << std::endl;
		return -1;
	}

	warp::checker ch;
	auto err = ch.open(rocksdb_path);
	if (err) {
		std::cerr << "Could not open database: " << err.message() << std::endl;
		return err.code();
	}

	ch.load_error_models(replace, around);

	std::vector<warp::dictionary::word_form> wfs;

	auto dump = [&] (const std::string &t) -> void {
		wfs.clear();
		err = ch.check(t, boundary, &wfs);
		if (err) {
			std::cerr << "Could not check word: " << t << ", error: " << err.message() << std::endl;
			exit(err.code());
		}

		for (auto &wf: wfs) {
			std::cout << t << " -> " << wf.word <<
				", freq: " << wf.freq <<
				", freq_norm: " << wf.freq_norm <<
				", documents: " << wf.documents <<
				", edit_distance: " << wf.edit_distance <<
				std::endl;
		}
	};

	std::vector<std::string> test({"падьезд", "прафисианал", "превет"});
	for (auto &t: test) {
		dump(t);
	}

	std::ifstream in("/dev/stdin");
	std::string w;
	while (std::getline(in, w)) {
		dump(w);
	}
	return 0;
}
