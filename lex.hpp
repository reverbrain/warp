#ifndef __IOREMAP_WARP_LEX_HPP
#define __IOREMAP_WARP_LEX_HPP

#include <boost/locale.hpp>

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

		lex(const std::locale &loc) : m_loc(loc) {}

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

		std::vector<int> grammar(const std::vector<parsed_word::feature_mask> &gfeat,
				const std::vector<std::string> &words) {
			// this is actually substring search, but I do not care about more optimized
			// algorithms for now, since grammatics are supposed to be rather small

			std::vector<int> gram_positions;

			int gfeat_pos = 0;
			auto gram_start = words.begin();
			for (auto word = words.begin(); word != words.end();) {
				auto lres = m_word.lookup(word2ll(*word));

				std::cout << "word: " << *word <<
					", grammar position: " << gfeat_pos <<
					", grammar-start: " << *gram_start <<
					", match-to: " << std::hex;
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

		std::string root(const std::string &word) {
			auto ll = word2ll(word);

			auto res = m_word.lookup(ll);

			if (res.first.size()) {
				std::ostringstream ss;

				int count = ll.size() - res.first[0].ending_len;
				for (auto l = ll.rbegin(); l != ll.rend(); ++l) {
					ss << *l;

					if (--count == 0)
						break;
				}

				return ss.str();
			}

			return word;
		}

		std::vector<ef> lookup(const std::string &word) {
			auto ll = word2ll(word);

			auto res = m_word.lookup(ll);

			std::string ex = "exact";
			if (res.second != (int)ll.size())
				ex = "not exact (" + boost::lexical_cast<std::string>(res.second) +
					"/" + boost::lexical_cast<std::string>(ll.size()) + ")";

			std::cout << "word: " << word << ", match: " << ex << ": ";
			for (auto v : res.first) {
				std::cout << v.ending_len <<
					"," << std::hex << "0x" << v.features <<
					" " << std::dec;
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

			std::cout << "requested mask: 0x" << std::hex << mask <<
				", requested bits: " << std::dec << request_max_count <<
				", best-match: 0x" << std::hex << max_ef.features <<
				", bits-intersection: " << std::dec << max_count <<
				std::endl;

			if (max_count >= request_max_count)
				return max_ef;

			return ef();
		}

};

}} // namespace ioremap::warp

#endif /* __IOREMAP_WARP_LEX_HPP */
