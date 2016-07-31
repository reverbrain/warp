#include <warp/ngram.hpp>

#include <ribosome/dir.hpp>
#include <ribosome/html.hpp>
#include <ribosome/lstring.hpp>
#include <ribosome/split.hpp>

#include <boost/algorithm/string/trim_all.hpp>
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

	std::string save_path, load_path;

	bpo::options_description generic("Language detector test options");
	generic.add_options()
		("help", "this help message")
		("save", bpo::value<std::string>(&save_path), "save language statistics into given file")
		("load", bpo::value<std::string>(&load_path), "load language statistics from given file")
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

	ribosome::html_parser html;

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

	auto prepare_text = [] (const std::string &text) -> std::string {
		std::string trimmed = boost::algorithm::trim_fill_copy_if(text, " ",
			boost::is_any_of("1234567890-=!@#$%^&*()_+[]\\{}|';\":/.,?><\n\r\t "));

		auto ret = ribosome::lconvert::from_utf8(trimmed);
		//return ribosome::lconvert::to_lower(ret);
		return ribosome::lconvert::to_string(ribosome::lconvert::to_lower(ret));
	};


	warp::ngram::detector<std::string, std::string> det;

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

			html.feed_file(path);
			std::string text = html.text(" ");
			auto res = prepare_text(text);

#if 0
			std::string fn = std::string(path) + ".out";
			std::ofstream out(fn);
			out << res;
#else
			det.load_text(res, lang);
#endif
			std::cout << "loaded " << text.size() << " bytes from " << path << ", language: " << lang << std::endl;
			return true;
		});
	}

	det.sort(3000);

	if (save_path.size()) {
		int err = det.save_file(save_path.c_str());
		if (err) {
			std::cout << "Successfully saved statistics data into " << save_path << std::endl;
		} else {
			std::cerr << "Could not save statistics data into " << save_path << std::endl;
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
			html.feed_file(path);
			std::string text = html.text(" ");
			auto res = prepare_text(text);
#if 1
			//std::string converted = ribosome::lconvert::to_string(res);

			long errors = 0;
			long total = 0;

			//auto v = spl.convert_split_words(converted.data(), converted.size());
			auto v = spl.convert_split_words(res.data(), res.size());
			for (auto &w: v) {
				//std::string detected = det.detect(w);
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
#else
			std::string detected = det.detect(res);

			std::cout << "detection: file: " << path <<
				", language: " << detected << std::endl;
#endif
			return true;
		});
	}

	return 0;
}
