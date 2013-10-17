#include <boost/locale.hpp>
#include <boost/program_options.hpp>

#include "pack.hpp"
#include "trie.hpp"

namespace lb = boost::locale::boundary;

namespace ioremap { namespace warp {

struct ef {
	parsed_word::feature_mask	features;
	int				ending_len;

	bool operator==(const struct ef &ef) {
		return features == ef.features && ending_len == ef.ending_len;
	}
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

		std::vector<ef> lookup(const std::string &word) {
			auto ll = word2ll(word);

			auto res = m_word.lookup(ll);

			std::string ex = "exact";
			if (res.second != (int)ll.size())
				ex = "not exact (" + boost::lexical_cast<std::string>(res.second) + "/" +
					boost::lexical_cast<std::string>(ll.size()) + ")";

			std::cout << "word: " << word << ", match: " << ex << ": ";
			for (auto v : res.first) {
				std::cout << v.ending_len << "," << std::hex << "0x" << v.features << " " << std::dec;
			}
			std::cout << std::endl;

			return res.first;
		}

	private:
		trie::node<ef> m_word;

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
			trie::letters root = word2ll(e.root);
			trie::letters ending = word2ll(e.ending);

			struct ef ef;
			ef.features = e.features;
			ef.ending_len = ending.size();

			ending.insert(ending.end(), root.begin(), root.end());
			m_word.add(ending, ef);

			return true;
		}
};

}} // namespace ioremap::warp

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

