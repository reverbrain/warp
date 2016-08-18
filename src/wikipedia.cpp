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

using namespace ioremap;

typedef std::map<std::string, std::vector<std::string>> attributes_map_t;
struct element {
	std::vector<XML_Char> chars;
	attributes_map_t attrs;
	std::string name;
	int thread_num;
};

class expat_parser
{
public:
	expat_parser(int n = 1) {
		m_parser = XML_ParserCreate(NULL);
		if (!m_parser) {
			ribosome::throw_error(-ENOMEM, "could not create XML parser");
		}

		XML_SetUserData(m_parser, this);
		XML_SetElementHandler(m_parser, expat_start, expat_end);
		XML_SetCharacterDataHandler(m_parser, expat_characters);

		m_current.reset(new element);
		for (int i = 0; i < n; ++i) {
			m_pool.emplace_back(std::thread(std::bind(&expat_parser::callback, this, i)));
		}
	}

	~expat_parser() {
		m_need_exit = true;
		m_pool_wait.notify_all();
		for (auto &t: m_pool) {
			t.join();
		}

		XML_ParserFree(m_parser);
	}

	ribosome::error_info feed_data(const char *data, size_t size) {
		int err = XML_Parse(m_parser, data, size, size == 0);
		if (err == XML_STATUS_ERROR) {
			return ribosome::create_error(err, "could not parse chunk: size: %ld, last: %d, line: %ld, error: %s",
					size, size == 0,
					XML_GetCurrentLineNumber(m_parser),
					XML_ErrorString(XML_GetErrorCode(m_parser)));
		}

		return ribosome::error_info();
	}

private:
	XML_Parser m_parser;
	std::unique_ptr<element> m_current;

	bool m_need_exit = false;
	std::mutex m_lock;
	std::deque<std::unique_ptr<element>> m_elements;
	std::condition_variable m_pool_wait, m_parser_wait;
	std::vector<std::thread> m_pool;

	static void expat_start(void *data, const char *el, const char **attr) {
		expat_parser *p = (expat_parser *)data;

		p->m_current->name.assign(el);

		for (size_t i = 0; attr[i]; i += 2) {
			std::string aname(attr[i]);
			std::string aval(attr[i+1]);

			auto it = p->m_current->attrs.find(aname);
			if (it == p->m_current->attrs.end()) {
				p->m_current->attrs[aname] = std::vector<std::string>({aval});
			} else {
				it->second.emplace_back(aval);
			}
		}
	}
	static void expat_end(void *data, const char *el) {
		expat_parser *p = (expat_parser *)data;

		(void)el;

		p->element_is_ready();
	}

	static void expat_characters(void *data, const XML_Char *s, int len) {
		expat_parser *p = (expat_parser *)data;

		p->m_current->chars.insert(p->m_current->chars.end(), s, s + len);
	}

	void element_is_ready() {
		std::unique_lock<std::mutex> guard(m_lock);
		m_elements.emplace_back(std::move(m_current));
		m_current.reset(new element);

		if (m_elements.size() > m_pool.size() * 2) {
			m_parser_wait.wait(guard, [&] {return m_elements.size() < m_pool.size();});
		}

		guard.unlock();
		m_pool_wait.notify_one();
	}

	void callback(int num) {
		while (!m_need_exit) {
			std::unique_lock<std::mutex> guard(m_lock);
			m_pool_wait.wait_for(guard, std::chrono::milliseconds(100), [&] {return !m_elements.empty();});

			while (!m_elements.empty()) {
				std::unique_ptr<element> elm = std::move(m_elements.front());
				m_elements.pop_front();
				guard.unlock();
				m_parser_wait.notify_one();

				elm->thread_num = num;
				on_element(*elm);

				guard.lock();
			}
		}
	}

protected:
	virtual void on_element(const element &elm) {
		for (auto &p: elm.attrs) {
			std::cout << elm.name << "::" << p.first << "=";
			for (auto &aval: p.second) {
				std::cout << aval << " ";
			}
			std::cout << std::endl;
		}

		ribosome::lstring ls = ribosome::lconvert::from_utf8(elm.chars.data(), elm.chars.size());
		std::cout << "chars (" << ls.size() << ") : " << ribosome::lconvert::to_string(ls) << std::endl;
	}
};

class wiki_parser : public expat_parser {
public:
	wiki_parser(int n, const std::string &alphabet) : expat_parser(n), m_alphabet(alphabet) {
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

	void on_element(const element &elm) override {
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

