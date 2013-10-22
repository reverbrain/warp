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

	ef() : features(0ULL), ending_len(0) {}
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

		std::vector<parsed_word::feature_mask> generate(const std::vector<std::string> &grams) {
			std::vector<parsed_word::feature_mask> ret;

			parser p;

			for (auto gram = grams.begin(); gram != grams.end(); ++gram) {
				lb::ssegment_index wmap(lb::word, gram->begin(), gram->end(), m_loc);
				wmap.rule(lb::word_any);

				parsed_word rec;
				for (auto it = wmap.begin(), e = wmap.end(); it != e; ++it) {
					token_entity ent = p.try_parse(it->str());
					if (ent.position != -1) {
						if (ent.position < (int)sizeof(parsed_word::feature_mask) * 8)
							rec.features |= (parsed_word::feature_mask)1 << ent.position;
						rec.ent.emplace_back(ent);
					}
				}

				std::cout << *gram << ": 0x" << std::hex << rec.features << std::dec << std::endl;
				ret.push_back(rec.features);
			}

			return ret;
		}

		std::vector<int> grammar(const std::vector<parsed_word::feature_mask> &gfeat, const std::vector<std::string> &words) {
			// this is actually substring search, but I do not care about more optimized algorithms for now,
			// since grammatics are supposed to be rather small

			std::vector<int> gram_positions;

			int gfeat_pos = 0;
			auto gram_start = words.begin();
			for (auto word = words.begin(); word != words.end();) {
				auto lres = m_word.lookup(word2ll(*word));

				std::cout << "word: " << *word << ", grammar position: " << gfeat_pos << ", grammar-start: " << *gram_start << ", match-to: " << std::hex;
				for (auto gw : lres.first)
					std::cout << "0x" << gw.features << " ";
				std::cout << std::dec << std::endl;

				ef eftmp = found(gfeat[gfeat_pos], lres.first);
				if (!eftmp.features) {
					// try next word if the first grammar entry doesn't match
					if (gfeat_pos == 0) {
						++word;
						++gram_start;
						std::cout << std::endl;
						continue;
					}

					gfeat_pos = 0;
					++gram_start;
					word = gram_start;
					std::cout << std::endl;
					continue;
				}

				++gfeat_pos;

				if (gfeat_pos != (int)gfeat.size()) {
					++word;
					continue;
				}

				// whole grammar has been found
				gram_positions.push_back(gram_start - words.begin());
				gfeat_pos = 0;

				++word;
				gram_start = word;
			}

			return gram_positions;
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
			ll.reserve(word.size());

			lb::ssegment_index cmap(lb::character, word.begin(), word.end(), m_loc);
			for (auto it = cmap.begin(), e = cmap.end(); it != e; ++it) {
				ll.emplace_back(it->str());
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

		int bits_set(parsed_word::feature_mask tmp) {
			int pos;
			int count = 0;
			while ((pos = ffsll(tmp)) != 0) {
				tmp >>= pos;
				count++;
			}

			return count;
		}

		ef found(const parsed_word::feature_mask &mask, const std::vector<ef> &features) {
			int request_max_count = bits_set(mask);
			int max_count = 0;

			ef max_ef;
			for (auto fres = features.begin(); fres != features.end(); ++fres) {
				int count = bits_set(mask & fres->features);

				if (count > max_count) {
					max_ef = *fres;
					max_count = count;
				}
			}

			std::cout << "requested mask: 0x" << std::hex << mask << ", requested bits: " << std::dec << request_max_count <<
				", best-match: 0x" << std::hex << max_ef.features << ", bits-intersection: " << std::dec << max_count << std::endl;

			if (max_count >= request_max_count)
				return max_ef;

			return ef();
		}

};

}} // namespace ioremap::warp

int main(int argc, char *argv[])
{
	namespace bpo = boost::program_options;

	bpo::options_description generic("Parser options");

	std::string input, output, msgin, gram;
	generic.add_options()
		("help", "This help message")
		("input", bpo::value<std::string>(&input), "Input Zaliznyak dictionary file")
		("output", bpo::value<std::string>(&output), "Output msgpack file")
		("msgpack-input", bpo::value<std::string>(&msgin), "Input msgpack file")
		("grammar", bpo::value<std::string>(&gram), "Grammar string suitable for ioremap::warp::parser, space separates single word descriptions")
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

		if (!vm.count("grammar")) {
			for (auto & w : words)
				l.lookup(w);
		} else {
			std::vector<std::string> tokens;
			std::istringstream iss(gram);
			std::copy(std::istream_iterator<std::string>(iss), std::istream_iterator<std::string>(), std::back_inserter<std::vector<std::string>>(tokens));

			std::vector<iw::parsed_word::feature_mask> vgram = l.generate(tokens);

			for (auto pos : l.grammar(vgram, words)) {
				for (size_t i = 0; i < vgram.size(); ++i) {
					std::cout << words[i + pos] << " ";
				}

				std::cout << std::endl;
			}
		}
	}

	return 0;
}

