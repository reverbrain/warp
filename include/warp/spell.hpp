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

	struct lemma_freq {
		std::string lemma;
		int count, distance;

		lemma_freq() : count(0), distance(0) {}
	};

	struct lemma_ctl {
		std::vector<lemma_freq> freq;

		// we have multiple lemmas referred by the same wordform
		std::string get_max_word() {
			int max_count = 0;
			std::string lemma;
			for (auto it = freq.begin(); it != freq.end(); ++it) {
				//std::cout << "get-max-word: " << it->lemma << ": forms: " << it->count << std::endl;
				if (it->count > max_count) {
					max_count = it->count;
					lemma = it->lemma;
				}
			}

			return lemma;
		}

		bool has_lemma(const std::string &word) {
			for (auto it = freq.begin(); it != freq.end(); ++it) {
				if (it->lemma == word)
					return true;
			}

			return false;
		}
	};

	typedef std::shared_ptr<lemma_ctl> shared_lemma;


class spell {
	public:
		spell(int ngram) : m_thread_num(1) {
			for (int i = 0; i < m_thread_num; ++i) {
				m_search.emplace_back(lemma_search(ngram));
			}
		}

		void feed_word(const std::string &word) {
			m_search[0].feed_word(word);
		}

		void feed_dict(const std::vector<std::string> &path) {
			timer tm;

			warp::unpacker(path, m_thread_num, std::bind(&spell::unpack_everything, this,
						std::placeholders::_1, std::placeholders::_2));

			long words = 0, lemmas = 0;
			for (int i = 0; i < m_thread_num; ++i) {
				const auto & search = m_search[i];
				words += search.m_words;
				lemmas += search.m_lemmas;
			}

			printf("spell checker loaded: words: %ld, lemmas: %ld, time: %lld ms\n",
					words, lemmas, (unsigned long long)tm.elapsed());
		}

		std::vector<std::string> search(const std::string &text) {
			timer tm;
			std::vector<lemma_freq> ret;

			int min_dist = 2;
			for (int i = 0; i < m_thread_num; ++i) {
				auto tmp = m_search[i].search(text, min_dist);
				ret.insert(ret.end(), tmp.begin(), tmp.end());
			}

			std::vector<std::string> ret_str;
			ret_str.reserve(ret.size());

			for (auto it = ret.begin(); it != ret.end(); ++it) {
				std::cout << text << ": " << it->lemma << " : count: " << it->count << ", distance: " << it->distance << std::endl;
				ret_str.emplace_back(it->lemma);
			}

			printf("search: %s, found: %zd, total search time: %lld ms\n",
					text.c_str(), ret_str.size(), (unsigned long long)tm.elapsed());

			return ret_str;
		}

	private:
		int m_thread_num;

		struct lemma_search {
			long m_words, m_lemmas;
			fuzzy<shared_lemma> m_fuzzy;
			std::map<std::string, shared_lemma> m_form2lemma;

			lemma_search(int ngram) : m_words(0), m_lemmas(0), m_fuzzy(ngram) {
			}

			std::map<std::string, shared_lemma>::iterator feed_word(const std::string &word) {
				shared_lemma ctl = std::make_shared<lemma_ctl>();

				lemma_freq fr;
				fr.lemma = word;
				fr.count = 1;

				ctl->freq.emplace_back(fr);

				m_fuzzy.feed_word(lconvert::from_utf8(word), ctl);
				auto it = m_form2lemma.insert(std::pair<std::string, shared_lemma>(word, ctl));

				m_lemmas += 1;

				return it.first;
			}

			std::vector<lemma_freq> search(const std::string &text, int &min_dist) {
				timer tm;
				std::vector<shared_lemma> ret;

				auto precise = m_form2lemma.find(text);
				if (precise != m_form2lemma.end()) {
					min_dist = 0;

					printf("spell checker lookup: '%s': precise search: elements: %zd, total words: %zd, search-time: %lld ms\n",
							text.c_str(), precise->second->freq.size(), m_form2lemma.size(), (unsigned long long)tm.elapsed());
					return precise->second->freq;
				}

				lstring t = lconvert::from_utf8(boost::locale::to_lower(text, __fuzzy_locale));
				auto fsearch = m_fuzzy.search(t);

				printf("spell checker lookup: rough search: words: %zd, min-dist: %d, fuzzy-search-time: %lld ms\n",
						fsearch.size(), min_dist, (unsigned long long)tm.elapsed());

				auto freq = search_everything(t, fsearch, min_dist);

				printf("spell checker lookup: checked: words: %zd, total-search-time: %lld ms:\n",
						freq.size(), (unsigned long long)tm.restart());

				return freq;
			}

			std::vector<lemma_freq> search_everything(const lstring &t, const std::vector<shared_lemma> &fsearch, int &min_dist) {
				std::vector<lemma_freq> ret;

				ret.reserve(fsearch.size());

				for (auto it = fsearch.begin(); it != fsearch.end(); ++it) {
					for (auto w = (*it)->freq.begin(); w != (*it)->freq.end(); ++w) {
						lstring word = lconvert::from_utf8(w->lemma);

						if (word.size() > t.size() + 2)
							continue;

						if (t.size() > word.size() + 2)
							continue;

						int dist = distance::levenstein<lstring>(t, word, min_dist);
						if (dist < 0)
							continue;

						if (dist < min_dist)
							ret.clear();

						w->distance = dist;
						ret.push_back(*w);
						min_dist = dist;
					}
				}

				return ret;
			}
		};

		std::vector<lemma_search> m_search;

		bool unpack_everything(int idx, const warp::parsed_word &e) {
			auto & search = m_search[idx];

			auto w = search.m_form2lemma.find(e.word);

			// our word was already placed into the map
			if (w != search.m_form2lemma.end()) {
				bool has_lemma = false;
				// our word exists in the map, let's check whether lemma_ctl, it refers to, contains our lemma
				for (auto fr = w->second->freq.begin(); fr != w->second->freq.end(); ++fr) {
					if (fr->lemma == e.lemma) {
						// our lemma has been found in the array: our word was found in the map, our lemma was found in its lemma_ctl,
						// this is just a double in the dictionary
						has_lemma = true;
						break;
					}
				}

				if (!has_lemma) {
					// this is a new meaning of the word, since it has different lemma (not ones we already put into lemma ctl)
					// lets create new entry in lemma_ctl array

					lemma_freq fr;
					fr.lemma = e.lemma;
					fr.count = 1;

					w->second->freq.emplace_back(fr);
				}
			} else {
				// our word was never placed into the map, let's check its lemma and add it if it does not yet exist

				auto it = search.m_form2lemma.find(e.lemma);
				if (it == search.m_form2lemma.end()) {
					it = search.feed_word(e.lemma);
				}

				auto sh = it->second;

				for (auto fr = sh->freq.begin(); fr != sh->freq.end(); ++fr) {
					if (fr->lemma == e.lemma) {
						fr->count++;
						break;
					}
				}

				search.m_form2lemma[e.word] = sh;
			}

			search.m_words += 1;
			return true;
		}
};


}} // namespace ioremap::warp

#endif /* __WARP_SPELL_HPP */
