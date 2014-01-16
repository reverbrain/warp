/*
 * Copyright 2013+ Evgeniy Polyakov <zbr@ioremap.net>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
		return features == ef.features;
	}
	bool operator<(const struct ef &ef) const {
		return features < ef.features;
	}

	ef() : features(0ULL), ending_len(0) {}
};

struct grammar {
	parsed_word::feature_mask	features;
	parsed_word::feature_mask	negative;

	grammar() : features(0ULL), negative(0ULL) {}
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

		std::vector<grammar> generate(const std::vector<std::string> &grams) {
			std::vector<grammar> ret;

			parser p;

			for (auto gram = grams.begin(); gram != grams.end(); ++gram) {
				lb::ssegment_index wmap(lb::word, gram->begin(), gram->end(), m_loc);
				wmap.rule(lb::word_any | lb::word_none);

				grammar tmp;
				bool negative = false;
				for (auto it = wmap.begin(), e = wmap.end(); it != e; ++it) {
					if (it->rule() & lb::word_none) {
						if (it->str() == "-")
							negative = true;
						continue;
					}

					token_entity ent = p.try_parse(it->str());
					if (ent.position != -1) {
						if (ent.position < (int)sizeof(parsed_word::feature_mask) * 8) {
							if (negative)
								tmp.negative |= (parsed_word::feature_mask)1 << ent.position;
							else
								tmp.features |= (parsed_word::feature_mask)1 << ent.position;
						}
					}
				}

				std::cout << *gram << ": features: 0x" << std::hex << tmp.features << ", negative: " << tmp.negative << std::dec << std::endl;
				ret.push_back(tmp);
			}

			return ret;
		}

		std::vector<int> grammar_deduction(const std::vector<grammar> &gfeat, const std::vector<std::string> &words) {
			// this is actually substring search, but I do not care about more optimized
			// algorithms for now, since grammatics are supposed to be rather small

			std::vector<int> gram_positions;

			int gfeat_pos = 0;
			auto gram_start = words.begin();
			for (auto word = words.begin(); word != words.end();) {
				auto lres = m_word.lookup(word2ll(*word));

#if 0
				std::cout << "word: " << *word <<
					", grammar position: " << gfeat_pos <<
					", grammar-start: " << *gram_start <<
					", match-to: " << std::hex;
				for (auto gw : lres)
					std::cout << "0x" << gw.features << " ";
				std::cout << std::dec << std::endl;
#endif
				ef eftmp = found(gfeat[gfeat_pos], lres);
				if (!eftmp.features) {
					// try next word if the first grammar entry doesn't match
					if (gfeat_pos == 0) {
						++word;
						++gram_start;
						continue;
					}

					gfeat_pos = 0;
					++gram_start;
					word = gram_start;
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

			if (res.size()) {
				std::ostringstream ss;

				int count = ll.size() - res[0].ending_len;
				for (auto l = ll.rbegin(); l != ll.rend(); ++l) {
					ss << *l;

					if (--count == 0)
						break;
				}

				return ss.str();
			}

			return "";
		}

		std::map<std::string, std::vector<ef>> lookup_sentence(const std::string &sent) {
			std::map<std::string, std::vector<ef>> ewords;

			lb::ssegment_index wmap(lb::word, sent.begin(), sent.end(), m_loc);
			wmap.rule(lb::word_any);

			for (auto it = wmap.begin(), e = wmap.end(); it != e; ++it) {
				const auto & res = lookup(it->str());
				if (res.size())
					ewords[it->str()] = res;
			}

			return ewords;
		}

		std::vector<ef> lookup(const std::string &word) {
			auto ll = word2ll(word);

			auto res = m_word.lookup(ll);

#if 0
			std::cout << "word: " << word << ": ";
			for (auto v : res) {
				std::cout << v.ending_len <<
					"," << std::hex << "0x" << v.features <<
					" " << std::dec;
			}
			std::cout << std::endl;
#endif
			return res;
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

		ef found(const grammar &mask, const std::vector<ef> &features) {
			int request_max_count = bits_set(mask.features);
			int max_count = 0;

			ef max_ef;
			for (auto fres = features.begin(); fres != features.end(); ++fres) {
				if (mask.negative & fres->features)
					continue;

				int count = bits_set(mask.features & fres->features);

				if (count > max_count) {
					max_ef = *fres;
					max_count = count;
				}
			}
#if 0
			std::cout << "requested mask: 0x" << std::hex << mask.features <<
				", negative: 0x" << mask.negative <<
				", requested bits: " << std::dec << request_max_count <<
				", best-match: 0x" << std::hex << max_ef.features <<
				", bits-intersection: " << std::dec << max_count <<
				std::endl;
#endif
			if (max_count >= request_max_count)
				return max_ef;

			return ef();
		}

};

}} // namespace ioremap::warp

#endif /* __IOREMAP_WARP_LEX_HPP */
