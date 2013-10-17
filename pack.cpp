#include <boost/locale.hpp>
#include <boost/program_options.hpp>

#include <fstream>
#include <iostream>
#include <string>
#include <sstream>

#include <msgpack.hpp>

#include "feature.hpp"
#include "trie.hpp"

namespace lb = boost::locale::boundary;

namespace ioremap { namespace warp {

struct entry {
	static const int serialization_version;

	std::string root;
	std::string ending;
	parsed_word::feature_mask_t features;

	entry() : features(0ULL) {}
};

const int entry::serialization_version = 1;

class packer {
	public:
		packer(const std::string &output) {
			m_out.exceptions(m_out.failbit);
			m_out.open(output.c_str(), std::ios::binary | std::ios::trunc);
		}

		packer(const packer &z) = delete;

		bool zprocess(const std::string &root, const struct parsed_word &rec) {
			return pack(root, rec.ending, rec.feature_mask);
		}

	private:
		std::ofstream m_out;

		bool pack(const std::string &root, const std::string &ending, const parsed_word::feature_mask_t features) {
			msgpack::sbuffer buf;
			msgpack::packer<msgpack::sbuffer> pk(&buf);

			pk.pack_array(4);
			pk.pack(entry::serialization_version);
			pk.pack(root);
			pk.pack(ending);
			pk.pack(features);

			m_out.write(buf.data(), buf.size());
			return true;
		}

};

class unpacker {
	public:
		unpacker(const std::string &input) {
			m_in.exceptions(m_in.failbit);
			m_in.open(input.c_str(), std::ios::binary);
		}

		typedef std::function<bool (const entry &)> unpack_process;
		void unpack(const unpack_process &process) {
			msgpack::unpacker pac;

			try {
				timer t, total;
				long num = 0;
				long chunk = 100000;
				long duration;

				while (true) {
					pac.reserve_buffer(1024 * 1024);
					size_t bytes = m_in.readsome(pac.buffer(), pac.buffer_capacity());

					if (!bytes)
						break;
					pac.buffer_consumed(bytes);

					msgpack::unpacked result;
					while (pac.next(&result)) {
						msgpack::object obj = result.get();

						entry e;
						obj.convert<entry>(&e);

						if (!process(e))
							return;

						if ((++num % chunk) == 0) {
							duration = t.restart();
							std::cout << "Read objects: " << num <<
								", elapsed time: " << total.elapsed() << " msecs" <<
								", speed: " << chunk * 1000 / duration << " objs/sec" <<
								std::endl;
						}
					}
				}

				duration = total.elapsed();
				std::cout << "Read objects: " << num <<
					", elapsed time: " << duration << " msecs" <<
					", speed: " << num * 1000 / duration << " objs/sec" <<
					std::endl;

			} catch (const std::exception &e) {
				std::cerr << "Exception: " << e.what() << std::endl;
			}
		}

	private:
		std::ifstream m_in;
};

class lex {
	public:
		lex() {
			boost::locale::generator gen;
			m_loc = gen("en_US.UTF8");
		}

		void load(const std::string &path) {
			unpacker unpack(path);
			unpack.unpack(std::bind(&lex::unpack_process, this, std::placeholders::_1));
		}

		std::vector<parsed_word::feature_mask_t> lookup(const std::string &word) {
			auto ll = word2ll(word);

			auto res = m_word.lookup(ll);

			std::string ex = "exact";
			if (res.second != (int)ll.size())
				ex = "not exact (" + boost::lexical_cast<std::string>(res.second) + "/" +
					boost::lexical_cast<std::string>(ll.size()) + ")";

			std::cout << "word: " << word << ", match: " << ex << ": ";
			for (auto v : res.first) {
				std::cout << std::hex << "0x" << v << " " << std::dec;
			}
			std::cout << std::endl;

			return res.first;
		}

	private:
		trie::node<parsed_word::feature_mask_t> m_word;

		std::locale m_loc;

		trie::letters word2ll(const std::string &word) {
			trie::letters ll;

			lb::ssegment_index cmap(lb::character, word.begin(), word.end(), m_loc);
			for (auto it = cmap.begin(), e = cmap.end(); it != e; ++it) {
				ll.emplace_back(std::move(it->str()));
			}

			std::reverse(ll.begin(), ll.end());

			return ll;
		}

		bool unpack_process(const entry &e) {
			trie::letters ll = word2ll(e.root + e.ending);
			m_word.add(ll, e.features);

			return true;
		}
};

}} // namespace ioremap::warp

namespace msgpack {

template <typename Stream>
inline msgpack::packer<Stream> &operator <<(msgpack::packer<Stream> &o, const ioremap::warp::entry &e)
{
	o.pack_array(4);
	o.pack(ioremap::warp::entry::serialization_version);
	o.pack(e.root);
	o.pack(e.ending);
	o.pack(e.features);

	return o;
}

inline ioremap::warp::entry &operator >>(msgpack::object o, ioremap::warp::entry &e)
{
	if (o.type != msgpack::type::ARRAY || o.via.array.size < 1) {
		std::ostringstream ss;
		ss << "entry msgpack: type: " << o.type <<
			", must be: " << msgpack::type::ARRAY <<
			", size: " << o.via.array.size;
		throw std::runtime_error(ss.str());
	}

	object *p = o.via.array.ptr;
	const uint32_t size = o.via.array.size;
	uint16_t version = 0;
	p[0].convert(&version);
	switch (version) {
	case 1: {
		if (size != 4) {
			std::ostringstream ss;
			ss << "entry msgpack: array size mismatch: read: " << size << ", must be: 4";
			throw std::runtime_error(ss.str());
		}

		p[1].convert(&e.root);
		p[2].convert(&e.ending);
		p[3].convert(&e.features);
		break;
	}
	default: {
		std::ostringstream ss;
		ss << "entry msgpack: version mismatch: read: " << version <<
			", must be: <= " << ioremap::warp::entry::serialization_version;
		throw msgpack::type_error();
	}
	}

	return e;
}

} // namespace msgpack

int main(int argc, char *argv[])
{
	namespace bpo = boost::program_options;

	bpo::options_description generic("Parser options");

	std::string input, output, msgin;
	generic.add_options()
		("help", "This help message")
		("input", bpo::value<std::string>(&input), "Input Zaliznyak dictionary file")
		("output", bpo::value<std::string>(&output), "Output msgpack file")
		("msgpack-input", bpo::value<std::string>(&msgin), "Input msgpack file")
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

		for (auto & w : words)
			l.lookup(w);
	}

	return 0;
}

