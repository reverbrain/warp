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

#ifndef __WARP_FUZZY_HPP
#define __WARP_FUZZY_HPP

#include "warp/lstring.hpp"
#include "warp/ngram.hpp"
#include "warp/timer.hpp"

#include <algorithm>
#include <mutex>
#include <vector>

namespace ioremap { namespace warp {

struct ngram_data {
	lstring word;
	int ngram_pos;

	ngram_data() : ngram_pos(0) {}
};

class fuzzy {
	public:
		fuzzy(int num) : m_ngram(num) {}

		void feed_text(const std::string &text) {
			namespace lb = boost::locale::boundary;

			lb::ssegment_index wmap(lb::word, text.begin(), text.end(), __fuzzy_locale);
			wmap.rule(lb::word_any);

			for (auto it = wmap.begin(), e = wmap.end(); it != e; ++it) {
				lstring word = lconvert::from_utf8(boost::locale::to_lower(it->str(), __fuzzy_locale));
				feed_word(word);
			}
		}

		void feed_word(const lstring &word) {
			std::unique_lock<std::mutex> guard(m_lock);
			m_ngram.load(word);
		}

		std::vector<ngram::ncount<lstring>> search(const std::string &text) {
			lstring t = lconvert::from_utf8(boost::locale::to_lower(text, __fuzzy_locale));
			return search(t);
		}

		std::vector<ngram::ncount<lstring>> search(const lstring &text) {
			timer tm, total;

			auto ngrams = ngram::ngram<lstring>::split(text, m_ngram.n());

			std::map<lstring, int> word_count;
			int prev_pos = 0;

			for (auto it = ngrams.begin(); it != ngrams.end(); ++it) {
				auto tmp = m_ngram.lookup_word(*it);

				for (auto ndata = tmp.begin(); ndata != tmp.end(); ++ndata) {
					auto wc = word_count.find(ndata->word);
					if (wc == word_count.end()) {
						word_count[ndata->word] = ndata->pos;
					} else {
						if ((wc->second < ndata->pos) && (wc->second + m_ngram.n() >= ndata->pos)) {
							wc->second = ndata->pos;
						} else {
							word_count.erase(wc);
						}
					}
				}

				prev_pos++;
			}

			long lookup_time = tm.restart();

			std::vector<ngram::ncount<lstring>> counts;
			for (auto wc = word_count.begin(); wc != word_count.end(); ++wc) {
				ngram::ncount<lstring> nc;
				nc.word = wc->first;
				nc.count = 1.0 / (double)wc->first.size();

				if (nc.count >= 0.01)
					counts.emplace_back(nc);
			}

			long count_time = tm.restart();

			std::sort(counts.begin(), counts.end());

			long sort_time = tm.restart();

			std::cout << text << ": lookup: " << lookup_time << " ms, count: " << count_time << " ms, sort: " << sort_time << " ms, total: " << total.elapsed() << " ms" << std::endl;
#if 0
			std::cout << text << "\n";
			for (auto nc = counts.begin(); nc != counts.end(); ++nc) {
				std::cout << nc->word << ": " << nc->count << std::endl;
			}
#endif
			return counts;
		}

	private:
		ngram::ngram<lstring> m_ngram;
		std::mutex m_lock;
};

}} // namespace ioremap::warp

#endif /* __WARP_FUZZY_HPP */
