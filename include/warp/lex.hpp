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

#include "warp/spell.hpp"

#include <boost/locale.hpp>
#include <boost/algorithm/string.hpp>

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

struct word_features {
	std::string word;
	std::vector<ef> fvec;

	word_features(const std::string &word, const std::vector<ef> &fvec) : word(word), fvec(fvec) {}
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

		void load(int ngram, const std::vector<std::string> &path) {
			m_spell.reset(new spell(ngram, path));
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

		std::vector<grammar> generate(const std::string &gram_string) {
			std::vector<std::string> ret;

			boost::split(ret, gram_string, boost::is_any_of("\t "));
			return generate(ret);
		}

		std::vector<int> grammar_deduction_sentence(const std::vector<grammar> &gfeat, const std::string &sent) {
			lb::ssegment_index wmap(lb::word, sent.begin(), sent.end(), m_loc);
			wmap.rule(lb::word_any);

			std::vector<word_features> wfeat;
			for (auto it = wmap.begin(), e = wmap.end(); it != e; ++it) {
				wfeat.emplace_back(it->str(), lookup(it->str()));
			}

			return grammar_deduction(gfeat, wfeat);
		}

		std::vector<int> grammar_deduction(const std::vector<grammar> &gfeat, const std::vector<word_features> &wfeat) {
			// this is actually substring search, but I do not care about more optimized
			// algorithms for now, since grammatics are supposed to be rather small

			std::vector<int> gram_positions;

			int gfeat_pos = 0;
			auto gram_start = wfeat.begin();
			for (auto it = wfeat.begin(); it != wfeat.end();) {
				ef eftmp = found(gfeat[gfeat_pos], it->fvec);
#ifdef WARP_STDOUT_DEBUG
				std::cout << "grammar_deduction: word: " << it->word <<
					", grammar position: " << gfeat_pos <<
					", grammar-start: " << gram_start->word <<
					", match-to: " << std::hex;
				for (auto gw : it->fvec)
					std::cout << "0x" << gw.features << " ";
				std::cout << ", found-features: " << eftmp.features << std::dec << std::endl;
#endif
				if (!eftmp.features) {
					// try next word if the first grammar entry doesn't match
					if (gfeat_pos == 0) {
						++it;
						++gram_start;
						continue;
					}

					gfeat_pos = 0;
					++gram_start;
					it = gram_start;
					continue;
				}

				++gfeat_pos;

				if (gfeat_pos != (int)gfeat.size()) {
					++it;
					continue;
				}

				// whole grammar has been found
				gram_positions.push_back(gram_start - wfeat.begin());
				gfeat_pos = 0;

				++it;
				gram_start = it;
			}

			return gram_positions;
		}

		std::vector<int> grammar_deduction(const std::vector<grammar> &gfeat, const std::vector<std::string> &words) {
			std::vector<word_features> wfeat;

			wfeat.reserve(words.size());
			for (auto w = words.begin(); w != words.end(); ++w) {
				wfeat.emplace_back(*w, lookup(*w));
			}

			return grammar_deduction(gfeat, wfeat);
		}

		std::string root(const std::string &word) {
			auto ret = m_spell->search(word);
			if (ret.size())
				return lconvert::to_string(ret[0]);
			return word;
		}

		std::vector<word_features> lookup_sentence(const std::string &sent) {
			std::vector<word_features> wf;

			lb::ssegment_index wmap(lb::word, sent.begin(), sent.end(), m_loc);
			wmap.rule(lb::word_any);

			for (auto it = wmap.begin(), e = wmap.end(); it != e; ++it) {
				const auto & res = lookup(it->str());
				wf.emplace_back(it->str(), res);
			}

			return wf;
		}

		std::vector<std::string> normalize_sentence(const std::string &sent) {
			lb::ssegment_index wmap(lb::word, sent.begin(), sent.end(), m_loc);
			wmap.rule(lb::word_any);

			std::vector<std::string> roots;
			for (auto it = wmap.begin(), e = wmap.end(); it != e; ++it) {
				roots.emplace_back(root(it->str()));
			}

			return roots;
		}

		std::vector<ef> lookup(const std::string &word) {
			return std::vector<ef>();
		}

	private:
		std::locale m_loc;
		std::auto_ptr<warp::spell> m_spell;

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

#ifdef WARP_STDOUT_DEBUG
			std::cout << "found: requested mask: 0x" << std::hex << mask.features <<
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
