#include "warp/alphabet.hpp"
#include "warp/database.hpp"
#include "warp/pack.hpp"

#include <boost/program_options.hpp>

#include <ribosome/lstring.hpp>
#include <ribosome/html.hpp>
#include <ribosome/split.hpp>
#include <ribosome/timer.hpp>

using namespace ioremap;

class html_parser {
public:
	html_parser(const std::string &alphabet) : m_alphabet(alphabet) {
	}
	~html_parser() {
	}

	void feed_file(const char *path) {
		m_html.feed_file(path);

		std::string text = m_html.text(" ");
		std::string lower = ribosome::lconvert::string_to_lower(text);

		ribosome::split spl;
		auto all_words = spl.convert_split_words(lower.c_str(), lower.size(), warp::drop_characters);

		for (auto &lw: all_words) {
			if (!m_alphabet.ok(lw)) {
				continue;
			}

			auto it = m_model.find(lw);
			if (it == m_model.end()) {
				warp::dictionary::word_form wf;
				wf.lw = lw;
				wf.word = ribosome::lconvert::to_string(lw);
				wf.freq = 1;
				wf.documents = 1;
				m_model.insert(std::make_pair<ribosome::lstring, warp::dictionary::word_form>(std::move(lw), std::move(wf)));
			} else {
				it->second.freq++;
				// do not increment document, since one page is one document
			}
		}
	}

	ribosome::error_info write(warp::dictionary::database &db, int boundary) {
		for (auto &p: m_model) {
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

private:
	std::map<ribosome::lstring, warp::dictionary::word_form> m_model;
	ribosome::html_parser m_html;
	warp::alphabet m_alphabet;
};


int main(int argc, char *argv[])
{
	namespace bpo = boost::program_options;

	bpo::options_description generic("Parser options");

	std::string alphabet;
	int boundary;
	std::string output;
	generic.add_options()
		("help", "This help message")
		("output", bpo::value<std::string>(&output)->required(), "Output rocksdb database")
		("alphabet", bpo::value<std::string>(&alphabet), "If present, output words will only consist of this alphabet")
		("boundary", bpo::value<int>(&boundary)->default_value(100),
		 	"Lower limit of word frequency, if it is less than limit, word will not be stored")
		;

	std::vector<std::string> inputs;
	bpo::options_description hidden("Hidden options");
	hidden.add_options()
		("input", bpo::value<std::vector<std::string>>(&inputs)->required()->composing(),
		 	"Input html/text file to parse and collect word statistics")
		;

	bpo::positional_options_description p;
	p.add("input", -1);

	bpo::options_description cmdline_options;
	cmdline_options.add(generic).add(hidden);

	bpo::variables_map vm;

	try {
		bpo::store(bpo::command_line_parser(argc, argv).options(cmdline_options).positional(p).run(), vm);

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

	html_parser html(alphabet);

	for (const auto &f: inputs) {
		ribosome::timer tm;

		html.feed_file(f.c_str());
		printf("%s: %.2f seconds\n", f.c_str(), tm.elapsed() / 1000.0);

		html.write(db, boundary);
	}

	return 0;
}

