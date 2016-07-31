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

#ifndef __WARP_NGRAM_HPP
#define __WARP_NGRAM_HPP

#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <sstream>
#include <vector>

namespace ioremap { namespace warp { namespace ngram {
template <typename S, typename D>
class ngram {
struct ngram_data {
	D data;
	int pos;

	ngram_data() : pos(0) {}
};

struct ngram_index_data {
	size_t data_index;
	int pos;

	bool operator<(const ngram_index_data &other) const {
		return data_index < other.data_index;
	}

	ngram_index_data() : data_index(0), pos(0) {}
};

struct ngram_meta {
	std::set<ngram_index_data> data;
	double count;

	ngram_meta() : count(1.0) {}
};

public:
	ngram(int n) : m_n(n) {}

	static std::vector<S> split(const S &text, size_t ngram) {
		std::vector<S> ret;

		if (text.size() >= ngram) {
			for (size_t i = 0; i < text.size() - ngram + 1; ++i) {
				S word = text.substr(i, ngram);
				ret.emplace_back(word);
			}
		}

		return ret;
	}

	void load(const S &text, const D &d) {
		std::vector<S> grams = ngram<S, D>::split(text, m_n);
		int position = 0;
		size_t index;

		auto it = m_data_index.find(d);
		if (it == m_data_index.end()) {
			index = m_data.size();
			m_data.push_back(d);

			m_data_index[d] = index;
		} else {
			index = it->second;
		}

		for (auto word = grams.begin(); word != grams.end(); ++word) {
			ngram_index_data data;
			data.data_index = index;
			data.pos = position++;

			auto it = m_map.find(*word);
			if (it == m_map.end()) {
				ngram_meta meta;

				meta.data.insert(data);
				m_map[*word] = meta;
			} else {
				it->second.count++;
				it->second.data.insert(data);
			}
		}
	}

	std::vector<ngram_data> lookup_word(const S &word) const {
		std::vector<ngram_data> ret;

		auto it = m_map.find(word);
		if (it != m_map.end()) {
			for (auto idx = it->second.data.begin(); idx != it->second.data.end(); ++idx) {
				ngram_data data;

				data.data = m_data[idx->data_index];
				data.pos = idx->pos;

				ret.emplace_back(data);
			}
		}

		return ret;
	}

	double lookup(const S &word) const {
		double count = 1.0;

		auto it = m_map.find(word);
		if (it != m_map.end())
			count += it->second.count;

		count /= 2.0 * m_map.size();
		return count;
	}

	size_t num(void) const {
		return m_map.size();
	}

	int n(void) const {
		return m_n;
	}

private:
	int m_n;
	std::map<S, ngram_meta> m_map;
	std::vector<D> m_data;
	std::map<D, size_t> m_data_index;
};

typedef ngram<std::string, std::string> byte_ngram;

template <typename S, typename D>
class probability {
public:
	probability() : m_small(2), m_large(3) {}

	bool load_text(const S &text, const D &mark) {
		m_small.load(text, mark);
		m_large.load(text, mark);
#if 0
		printf("%s: loaded: %zd bytes, 2-grams: %zd, 3-grams: %zd\n",
				filename, text.size(), m_n2.num(), m_n3.num());
#endif
		return true;
	}

	double detect(const S &text) const {
		double p = 1;
		for (size_t i = m_large.n(); i <= text.size(); ++i) {
			S s_large = text.substr(i - m_large.n(), m_large.n());
			S s_small = text.substr(i - m_large.n(), m_small.n());

			double p_large = (m_large.lookup(s_large));
			double p_small = (m_small.lookup(s_small));

			p *= p_large / p_small;
#if 1
			printf("s3: %s - %f, s2: %s - %f, diff: %f, p: %f\n",
				ribosome::lconvert::to_string(s_large).c_str(), p_large,
				ribosome::lconvert::to_string(s_small).c_str(), p_small,
				p_large/p_small, p);
#endif
		}

		if (p == 1)
			return 0;
		return p;
	}

private:
	ngram<S, D> m_small, m_large;
};


template <typename S, typename D>
class detector {
public:
	detector() {}

	bool load_text(const S &text, const D &id) {
		probability<S, D> p;
		bool ret = p.load_text(text, id);
		if (ret)
			n_prob[id] = p;

		return ret;
	}

	std::string detect(const S &text) const {
		double max_p = 0;
		std::string name = "";

		for (auto it = n_prob.begin(); it != n_prob.end(); ++it) {
			double p = it->second.detect(text);
			printf("word: %s, lang: %s, probability: %f\n",
					ribosome::lconvert::to_string(text).c_str(),
					it->first.c_str(), p);
			if (p > max_p) {
				name = it->first;
				max_p = p;
			}
		}

		return name;
	}

private:
	std::map<D, probability<S, D>> n_prob;
};

}}} // namespace ioremap::warp::ngram

#endif /* __WARP_NGRAM_HPP */
