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

template <typename D>
class fuzzy {
	public:
		fuzzy(int num) : m_ngram(num) {}

		void feed_word(const lstring &word, const D &d) {
			std::unique_lock<std::mutex> guard(m_lock);
			m_ngram.load(word, d);
		}

		std::vector<D> search(const std::string &text) {
			lstring t = lconvert::from_utf8(boost::locale::to_lower(text, __fuzzy_locale));
			return search(t);
		}

		std::vector<D> search(const lstring &text) {
			timer tm, total;

			auto ngrams = ngram::ngram<lstring, D>::split(text, m_ngram.n());

			std::map<D, int> word_count;

			for (auto it = ngrams.begin(); it != ngrams.end(); ++it) {
				auto tmp = m_ngram.lookup_word(*it);

				for (auto ndata = tmp.begin(); ndata != tmp.end(); ++ndata) {

					auto wc = word_count.find(ndata->data);
					if (wc == word_count.end()) {
						word_count[ndata->data] = 1;
					} else {
						wc->second++;
					}
				}
				std::cout << std::endl;
			}

			long lookup_time = tm.restart();

			std::vector<D> counts;

			for (auto wc = word_count.begin(); wc != word_count.end(); ++wc) {
				double tmp = (double)wc->second / (double)wc->first.size();
				if (tmp > 0.01) {
					counts.emplace_back(wc->first);
				}
			}

			long count_time = tm.restart();

			std::cout << text << ": counts: " << counts.size() << ", lookup: " << lookup_time << " ms, count: " << count_time << " ms, total: " << total.elapsed() << " ms" << std::endl;
#if 0
			std::cout << text << "\n";
			for (auto nc = counts.begin(); nc != counts.end(); ++nc) {
			}
#endif
			return counts;
		}

	private:
		ngram::ngram<lstring, D> m_ngram;
		std::mutex m_lock;
};

}} // namespace ioremap::warp

#endif /* __WARP_FUZZY_HPP */
