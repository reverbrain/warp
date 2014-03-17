/*
 * Copyright 2014+ Evgeniy Polyakov <zbr@ioremap.net>
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

#ifndef __WARP_SPELL_HPP
#define __WARP_SPELL_HPP

#include "warp/distance.hpp"
#include "warp/fuzzy.hpp"
#include "warp/ngram.hpp"
#include "warp/pack.hpp"
#include "warp/timer.hpp"

#include <msgpack.hpp>

namespace ioremap { namespace warp {

class spell {
	public:
		spell(int ngram, const std::vector<std::string> &path) : m_fuzzy(ngram) {
			timer tm;

			warp::unpacker(path, 2, std::bind(&spell::unpack_roots, this, std::placeholders::_1));

			printf("spell checker loaded: words: %zd, time: %lld ms\n",
					m_fe.size(), (unsigned long long)tm.elapsed());
		}

		std::vector<lstring> search(const std::string &text) {
			timer tm;

			lstring t = lconvert::from_utf8(boost::locale::to_lower(text, __fuzzy_locale));
			auto fsearch = m_fuzzy.search(t);

			printf("spell checker lookup: rough search: words: %zd, fuzzy-search-time: %lld ms\n",
					fsearch.size(), (unsigned long long)tm.elapsed());

			std::vector<lstring> ret;

			ret = search_roots(t, fsearch);

			printf("spell checker lookup: checked: words: %zd, total-search-time: %lld ms:\n",
					ret.size(), (unsigned long long)tm.restart());

			for (auto r = ret.begin(); r != ret.end(); ++r)
				std::cout << *r << std::endl;
			return ret;
		}

	private:
		std::map<lstring, std::vector<warp::feature_ending>> m_fe;
		fuzzy m_fuzzy;

		bool unpack_roots(const warp::entry &e) {
			lstring tmp = lconvert::from_utf8(e.root);
			m_fe[tmp] = e.fe;
			m_fuzzy.feed_word(tmp);
			return true;
		}

		bool unpack_everything(const warp::entry &e) {
			for (auto f = e.fe.begin(); f != e.fe.end(); ++f) {
				std::string word = e.root + f->ending;
				m_fuzzy.feed_text(word);
			}
			return true;
		}

		std::vector<lstring> search_roots(const lstring &t, const std::vector<ngram::ncount<lstring>> &fsearch) {
			timer tm;

			std::vector<lstring> ret;
			int min_dist = 1024;

			long total_endings = 0;
			long total_words = 0;

			ret.reserve(fsearch.size() / 4);
			for (auto w = fsearch.begin(); w != fsearch.end(); ++w) {
				if (w->word.size() >= t.size() + 3)
					continue;

				const auto & fe = m_fe.find(w->word);
				if (fe != m_fe.end()) {
					std::set<std::string> checked_endings;

					for (auto ending = fe->second.begin(); ending != fe->second.end(); ++ending) {
						auto tmp_end = checked_endings.find(ending->ending);
						if (tmp_end != checked_endings.end())
							continue;

						checked_endings.insert(ending->ending);
						lstring word = w->word + lconvert::from_utf8(ending->ending);

						int dist = distance::levenstein<lstring>(t, word);
						if (dist <= min_dist) {
							if (dist < min_dist)
								ret.clear();

							ret.emplace_back(word);
							min_dist = dist;
						}

						total_endings++;
					}
					total_words++;
				}
			}

			printf("spell checker lookup: checked endings: roots: %ld, endings: %ld, dist: %d, time: %lld ms:\n",
					total_words, total_endings,
					min_dist, (unsigned long long)tm.restart());

			return ret;
		}

		std::vector<lstring> search_everything(const lstring &t, const std::vector<ngram::ncount<lstring>> &fsearch) {
			std::vector<lstring> ret;
			int min_dist = 1024;

			ret.reserve(fsearch.size());

			for (auto w = fsearch.begin(); w != fsearch.end(); ++w) {
				int dist = distance::levenstein<lstring>(t, w->word);
				if (dist <= min_dist) {
					if (dist < min_dist)
						ret.clear();

					ret.emplace_back(w->word);
					min_dist = dist;
				}
			}

			return ret;
		}
};


}} // namespace ioremap::warp

#endif /* __WARP_SPELL_HPP */
