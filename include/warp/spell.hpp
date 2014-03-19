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
		spell(int ngram) : m_fuzzy(ngram) {}

		void feed_dict(const std::vector<std::string> &path) {
			timer tm;

			warp::unpacker(path, 2, std::bind(&spell::unpack_everything, this, std::placeholders::_1));

			printf("spell checker loaded: words: %ld, roots: %ld, time: %lld ms\n",
					m_words.load(), m_roots.load(), (unsigned long long)tm.elapsed());
		}

		void feed_word(const std::string &word) {
			m_fuzzy.feed_word(lconvert::from_utf8(word), lconvert::from_utf8(word));
			m_roots += 1;
			m_words += 1;
		}

		std::vector<lstring> search(const std::string &text) {
			timer tm;

			lstring t = lconvert::from_utf8(boost::locale::to_lower(text, __fuzzy_locale));
			auto fsearch = m_fuzzy.search(t);

			printf("spell checker lookup: rough search: words: %zd, fuzzy-search-time: %lld ms\n",
					fsearch.size(), (unsigned long long)tm.elapsed());

			std::vector<lstring> ret;

			ret = search_everything(t, fsearch);

			printf("spell checker lookup: checked: words: %zd, total-search-time: %lld ms:\n",
					ret.size(), (unsigned long long)tm.restart());

			for (auto r = ret.begin(); r != ret.end(); ++r)
				std::cout << *r << std::endl;
			return ret;
		}

	private:
		std::map<lstring, std::vector<warp::feature_ending>> m_fe;
		fuzzy<lstring> m_fuzzy;
		std::atomic_long m_roots, m_words;

		bool unpack_everything(const warp::entry &e) {
			for (auto f = e.fe.begin(); f != e.fe.end(); ++f) {
				std::string word = e.root + f->ending;
				m_fuzzy.feed_word(lconvert::from_utf8(word), lconvert::from_utf8(e.root));
			}

			m_roots += 1;
			m_words += e.fe.size();
			return true;
		}

		std::vector<lstring> search_everything(const lstring &t, const std::vector<lstring> &fsearch) {
			std::vector<lstring> ret;
			int min_dist = 1024;

			ret.reserve(fsearch.size());

			for (auto w = fsearch.begin(); w != fsearch.end(); ++w) {
				if (w->size() > t.size() + 2)
					continue;

				if (t.size() > w->size())
					continue;

				int dist = distance::levenstein<lstring>(t, *w);
				if (dist <= min_dist) {
					if (dist < min_dist)
						ret.clear();

					ret.emplace_back(*w);
					min_dist = dist;
				}
			}

			return ret;
		}
};


}} // namespace ioremap::warp

#endif /* __WARP_SPELL_HPP */
