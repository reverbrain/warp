#include "warp/alphabet.hpp"
#include "warp/database.hpp"
#include "warp/pack.hpp"

#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/program_options.hpp>

#include <expat.h>

#include <condition_variable>
#include <fstream>
#include <iostream>
#include <thread>
#include <unordered_map>

#include <ribosome/error.hpp>
#include <ribosome/lstring.hpp>
#include <ribosome/split.hpp>
#include <ribosome/timer.hpp>
#include <ribosome/xml.hpp>

using namespace ioremap;

class wiki_parser : public ribosome::xml_parser {
public:
	wiki_parser(int n, const std::string &alphabet) : ribosome::xml_parser(n), m_alphabet(alphabet) {
		m_model.resize(n);
	}
	~wiki_parser() {
	}

	ribosome::error_info write(warp::dictionary::database &db, int boundary) {
		std::map<ribosome::lstring, warp::dictionary::word_form> model;
		for (auto &m: m_model) {
			for (auto &p: m) {
				auto it = model.find(p.first);
				if (it == model.end()) {
					model[p.first] = p.second;
				} else {
					it->second.freq += p.second.freq;
					it->second.documents += p.second.documents;
				}
			}
		}

		for (auto &p: model) {
			warp::dictionary::word_form &wf = p.second;
			if (wf.freq < boundary)
				continue;

			wf.indexed_id = db.metadata().get_sequence();
			auto err = warp::packer::write(db, wf);
			if (err)
				return err;
		}

		if (db.options().sync_metadata_timeout == 0) {
			auto err = db.sync_metadata(NULL);
			if (err) {
				return err;
			}
		}

		return ribosome::error_info();
	}

	void on_element(const ribosome::element &elm) override {
		if (elm.thread_num >= (int)m_model.size()) {
			ribosome::throw_error(-E2BIG, "invalid thread number: %d, must be less than number of workers: %ld",
					elm.thread_num, m_model.size());
			return;
		}

		if (elm.name != "title" && elm.name != "text")
			return;

		auto &model = m_model[elm.thread_num];

		ribosome::split spl;
		auto all_words = spl.convert_split_words(elm.chars.data(), elm.chars.size(), warp::drop_characters);

		for (auto &w: all_words) {
			ribosome::lstring lw = ribosome::lconvert::to_lower(w);

			if (!m_alphabet.ok(lw))
				continue;

			auto it = model.find(lw);
			if (it == model.end()) {
				warp::dictionary::word_form wf;
				wf.lw = lw;
				wf.word = ribosome::lconvert::to_string(lw);
				wf.freq = 1;
				wf.documents = 1;
				model.insert(std::make_pair<ribosome::lstring, warp::dictionary::word_form>(std::move(lw), std::move(wf)));
			} else {
				it->second.freq++;
				// do not increment document, since one page is one document
			}
		}
	}

private:
	std::vector<std::map<ribosome::lstring, warp::dictionary::word_form>> m_model;
	warp::alphabet m_alphabet;
};


int main(int argc, char *argv[])
{
	namespace bpo = boost::program_options;

	bpo::options_description generic("Parser options");

	std::string alphabet;
	int boundary, num_threads;
	std::string wiki, output;
	generic.add_options()
		("help", "This help message")
		("wiki", bpo::value<std::string>(&wiki)->required(), "Wikipedia dump file in packed with bzip2")
		("output", bpo::value<std::string>(&output)->required(), "Output rocksdb database")
		("alphabet", bpo::value<std::string>(&alphabet), "If present, output words will only consist of this alphabet")
		("boundary", bpo::value<int>(&boundary)->default_value(100),
		 	"Lower limit of word frequency, if it is less than limit, word will not be stored")
		("num-threads", bpo::value<int>(&num_threads)->default_value(7),
		 	"Number of text parser threads (wikipedia xml is being parsed by separate thread)")
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

	warp::dictionary::database db;
	auto err = db.open_read_write(output);
	if (err) {
		std::cerr << "could not open rocksdb database: " << err.message() << std::endl;
		return err.code();
	}

	namespace bio = boost::iostreams;
	try {
		ribosome::timer tm, momentum;

		std::ifstream file(wiki, std::ios::in | std::ios::binary);
		bio::filtering_streambuf<bio::input> bin;
		bin.push(bio::bzip2_decompressor());
		bin.push(file);

		std::istream in(&bin);

		wiki_parser parser(num_threads, alphabet);

		size_t total_size = 0;
		std::string tmp;
		tmp.resize(1024 * 1024);
		while (in) {
			in.read((char *)tmp.data(), tmp.size());

			if (in.gcount() < 0)
				break;

			auto err = parser.feed_data(tmp.data(), in.gcount());
			if (err) {
				std::cerr << "parser feed error: " << err.message() << std::endl;
				return err.code();
			}

			total_size += in.gcount();
			printf("\r %ld seconds: loaded: %ld, speed: %.2f MB/s, momentum speed: %.2f MB/s",
					tm.elapsed() / 1000, total_size,
					total_size * 1000.0 / (tm.elapsed() * 1024 * 1024.0),
					in.gcount() * 1000.0 / (momentum.restart() * 1024 * 1024.0));
		}
		printf("\r %ld seconds: loaded: %ld, speed: %.2f MB/s\n",
				tm.elapsed() / 1000, total_size,
				total_size * 1000.0 / (tm.elapsed() * 1024 * 1024.0));

		parser.write(db, boundary);

		return 0;
	} catch (const std::exception &e) {
		std::cerr << "exception: " << e.what() << std::endl;
		return -EINVAL;
	}

	std::cerr << generic << std::endl;
	return -EINVAL;
}

