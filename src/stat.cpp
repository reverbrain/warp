#include "warp/lstring.hpp"
#include "warp/spell.hpp"

#include <boost/program_options.hpp>

#include <msgpack.hpp>

using namespace ioremap;

namespace lb = boost::locale::boundary;

int main(int argc, char *argv[])
{
	namespace bpo = boost::program_options;

	bpo::options_description generic("Word stats gathering options");

	std::string stat;

	generic.add_options()
		("help", "This help message")
		("merge", "If specified word stats will be merged into new tree indexed by lemma forms instead of words")
		("stat", bpo::value<std::string>(&stat), "Msgpack statistics file (stats will be appended to what is present in the file)")
		;

	bpo::positional_options_description p;
	p.add("files", -1);

	std::vector<std::string> files;

	bpo::options_description hidden("Positional options");
	hidden.add_options()
		("files", bpo::value<std::vector<std::string>>(&files), "files to parse")
	;

	bpo::variables_map vm;

	try {
		bpo::options_description cmdline_options;
		cmdline_options.add(generic).add(hidden);

		bpo::store(bpo::command_line_parser(argc, argv).options(cmdline_options).positional(p).run(), vm);

		if (vm.count("help") != 0) {
			std::cerr << generic << std::endl;
			return -1;
		}

		bpo::notify(vm);
	} catch (const std::exception &e) {
		std::cerr << "Invalid options: " << e.what() << "\n" << generic << std::endl;
		return -1;
	}

	if (!files.size()) {
		std::cerr << "There are no input files\n" << generic << "\n" << hidden << std::endl;
		return -1;
	}

	if (!vm.count("stat")) {
		std::cerr << "There is no stat file\n" << generic << "\n" << hidden << std::endl;
		return -1;
	}

	std::map<std::string, int> counts;

	std::ifstream in(stat.c_str());
	if (in.good()) {
		try {
			std::ostringstream ss;
			ss << in.rdbuf();
			std::string stat_data = ss.str();

			msgpack::unpacked msg;
			msgpack::unpack(&msg, stat_data.data(), stat_data.size());
			msgpack::object obj = msg.get();

			obj.convert(&counts);
		} catch (const std::exception &e) {
			std::cerr << "Could not parse stat file content: " << e.what() << std::endl;
			return -1;
		}
	}
	in.close();


	if (vm.count("merge")) {
		warp::spell sp(3);
		sp.feed_dict(files);

		std::map<std::string, int> out;
		for (auto it = counts.begin(); it != counts.end(); ++it) {
			auto search = sp.search(it->first);

			std::string out_word;

			if (search.size() == 0) {
				out_word = it->first;
			} else {
				out_word = warp::lconvert::to_string(search[0]);
			}

			int out_count;

			auto out_lookup = out.find(out_word);
			if (out_lookup == out.end()) {
				out[out_word] = it->second;
				out_count = it->second;
			} else {
				out_lookup->second += it->second;
				out_count = out_lookup->second;
			}

			std::cout << "merge: " << it->first << " -> " << out_word << " : " << out_count << std::endl;
		}

		std::cout << "Merge completed: " << counts.size() << " -> " << out.size() << " words" << std::endl;

		counts = out;
	} else {
		size_t words = 0;

		for (auto file = files.begin(); file != files.end(); ++file) {
			std::ifstream in(file->c_str());
			if (in.bad()) {
				std::cerr << "spell: could not feed file '" << *file << "': " << in.rdstate() << std::endl;
				continue;
			}

			std::ostringstream ss;
			ss << in.rdbuf();

			std::string text = ss.str();

			lb::ssegment_index wmap(lb::word, text.begin(), text.end(), warp::__fuzzy_locale);
			wmap.rule(lb::word_any);

			for (auto it = wmap.begin(), e = wmap.end(); it != e; ++it) {
				std::string word = boost::locale::to_lower(it->str(), warp::__fuzzy_locale);

				auto wc = counts.find(word);
				if (wc != counts.end()) {
					wc->second++;
				} else {
					counts[word] = 1;
				}

				++words;
			}
		}

		std::cout << "total words processed: " << words << ", unique tokens: " << counts.size() << std::endl;
	}

	msgpack::sbuffer sbuf;
	msgpack::pack(&sbuf, counts);

	std::ofstream out(stat.c_str(), std::ios::trunc | std::ios::binary);
	out.write(sbuf.data(), sbuf.size());
	out.close();

	return 0;
}

