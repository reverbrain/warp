#include "warp/alphabet.hpp"
#include "warp/ngram.hpp"

#include <ribosome/dir.hpp>
#include <ribosome/html.hpp>
#include <ribosome/lstring.hpp>
#include <ribosome/split.hpp>

#include <boost/program_options.hpp>

#include <fstream>
#include <iostream>
#include <sstream>

using namespace ioremap;

int main(int argc, char *argv[])
{
	namespace bpo = boost::program_options;

	std::vector<std::string> learn;
	std::vector<std::string> check;
	std::vector<std::string> astrings;

	std::string save_path, load_path;

	bpo::options_description generic("Language detector test options");
	generic.add_options()
		("help", "this help message")
		("save", bpo::value<std::string>(&save_path), "save language statistics into given file")
		("load", bpo::value<std::string>(&load_path), "load language statistics from given file")
		("alphabet", bpo::value<std::vector<std::string>>(&astrings)->composing(),
			 "for any given language use only provided alphabet, for example: english:abcdefghijklmnopqrstuvwxyz")
		("learn", bpo::value<std::vector<std::string>>(&learn)->composing(),
		 	"files to learn language, format: --learn language:directory")
		("check", bpo::value<std::vector<std::string>>(&check)->composing(),
		 	"files to check language")
		;


	bpo::variables_map vm;
	bpo::options_description cmdline_options;
	cmdline_options.add(generic);

	try {
		bpo::store(bpo::command_line_parser(argc, argv).options(cmdline_options).run(), vm);

		if (vm.count("help")) {
			std::cout << cmdline_options << std::endl;
			return 0;
		}

		bpo::notify(vm);
	} catch (const std::exception &e) {
		std::cerr << "Invalid options: " << e.what() << "\n" << cmdline_options << std::endl;
		return -1;
	}

	warp::alphabets_checker alphabets;
	for (const auto &a: astrings) {
		size_t pos = a.find(':');
		if (pos != std::string::npos) {
			alphabets.add(a.substr(0, pos), a.substr(pos+1));
		}
	}

	auto prepare_dir = [] (const std::string &path) -> std::pair<std::string, std::string> {
		size_t sep_pos = path.find(':');
		if (sep_pos == std::string::npos) {
			std::cerr << path << ": could not find ':' separator, skipping" << std::endl;
			return std::make_pair<std::string, std::string>("", "");
		}

		std::string lang = path.substr(0, sep_pos);
		std::string dir = path.substr(sep_pos+1, path.size());

		return std::make_pair<std::string, std::string>(std::move(lang), std::move(dir));
	};

	warp::detector<std::string, std::string> det;

	if (load_path.size()) {
		int err = det.load_file(load_path.c_str());
		if (err) {
			std::cerr << "could not load statistics from file " << load_path << ": " << err << std::endl;
			return err;
		}
	}

	for (auto &l: learn) {
		auto p = prepare_dir(l);
		if (p.first.empty() || p.second.empty())
			continue;

		const auto &dir = p.second;
		const auto &lang = p.first;

		ribosome::iterate_directory(dir, [&](const char *path, const char *file) -> bool {
			(void) file;

			ribosome::html_parser html;
			html.feed_file(path);
			std::string text = html.text(" ");
			std::string lower = ribosome::lconvert::string_to_lower(text);

			ribosome::split spl;
			auto all_words = spl.convert_split_words(lower.c_str(), lower.size(), warp::drop_characters);

			for (auto &lw: all_words) {
				if (alphabets.ok(lang, lw)) {
					std::string word = ribosome::lconvert::to_string(lw);
					det.load_text(word, lang);
				}
			}

			std::cout << "loaded " << text.size() << " bytes from " << path << ", language: " << lang << std::endl;
			return true;
		});
	}

	det.sort();

	if (save_path.size()) {
		int err = det.save_file(save_path.c_str());
		if (err) {
			std::cerr << "Could not save statistics data into " << save_path << std::endl;
		} else {
			std::cout << "Successfully saved statistics data into " << save_path << std::endl;
			return -1;
		}
	}

	ribosome::split spl;
	for (auto &c: check) {
		auto p = prepare_dir(c);
		if (p.first.empty() || p.second.empty())
			continue;

		const auto &dir = p.second;
		const auto &lang = p.first;

		ribosome::iterate_directory(dir, [&](const char *path, const char *file) -> bool {
			(void) file;
			ribosome::html_parser html;
			html.feed_file(path);
			std::string text = html.text(" ");
			std::string lower = ribosome::lconvert::string_to_lower(text);

			long errors = 0;
			long total = 0;

			auto all_words = spl.convert_split_words(lower.data(), lower.size(), warp::drop_characters);
			for (auto &w: all_words) {
				std::string detected = det.detect(ribosome::lconvert::to_string(w));

				if (detected != lang) {
					std::cout << "detection: file: " << path <<
						", word: " << ribosome::lconvert::to_string(w) <<
						", word: " << w <<
						", language: " << detected << std::endl;
					errors++;
				} else {
					std::cout << "detection: file: " << path <<
						", word: " << ribosome::lconvert::to_string(w) <<
						", word: " << w <<
						", successfully detected language: " << detected << std::endl;
				}

				total++;
			}

			std::cout << "detection: file: " << path <<
				", words: " << total <<
				", errors: " << errors <<
				", error rate: " << (float)errors * 100.0/(float)total << "%" <<
				std::endl;

			return true;
		});
	}

	return 0;
}
