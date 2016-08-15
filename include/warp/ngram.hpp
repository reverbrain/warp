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

#include <algorithm>
#include <fstream>
#include <map>
#include <string>
#include <sstream>
#include <vector>

#include <msgpack.hpp>

namespace ioremap { namespace warp {
template <typename S>
class ngram {
public:
	ngram() {}
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

	void load(const S &text) {
		for (ssize_t i = 0; i < (ssize_t)text.size() - m_n + 1; ++i) {
			S word = text.substr(i, m_n);

			auto it = m_map.find(word);
			if (it == m_map.end()) {
				m_map[word] = 1;
			} else {
				it->second++;
			}
		}
	}

	void reset_stats() {
		m_map.clear();
	}

	void sort(size_t num) {
		typedef std::pair<S, size_t> sp;
		std::vector<sp> tmp(m_map.begin(), m_map.end());

		std::sort(tmp.begin(), tmp.end(), [](const sp &p1, const sp &p2) { return p1.second < p2.second; });

		m_profile.clear();

		size_t pos = 0;
		for (auto it = tmp.rbegin(); it != tmp.rend(); ++it) {
			m_profile[it->first] = pos;

			++pos;
			if (pos == num)
				break;
		}
	}

	size_t score(const S &text) const {
		size_t score = 0;

		for (ssize_t i = 0; i < (ssize_t)text.size() - (ssize_t)m_n + 1; ++i) {
			S word = text.substr(i, m_n);

			size_t add;
			auto it = m_profile.find(word);
			if (it == m_profile.end()) {
				add = m_profile.size();
			} else {
				add = it->second;
			}

			score += add;
		}

		return score;
	}

	int n(void) const {
		return m_n;
	}

	MSGPACK_DEFINE(m_n, m_map, m_profile);

private:
	int m_n = 0;
	std::map<S, size_t> m_map;
	std::map<S, size_t> m_profile;
};

template <typename S>
class probability {
public:
	probability() {}
	probability(int ng) {
		for (int i = 2; i <= ng; ++i) {
			m_ngrams.emplace(std::pair<size_t, ngram<S>>(i, ngram<S>(i)));
		}
	}

	void reset_stats() {
		for (auto &ng: m_ngrams) {
			ng.second.reset_stats();
		}
	}

	bool load_text(const S &text) {
		for (auto &ng: m_ngrams) {
			ng.second.load(text);
		}

		return true;
	}

	void sort(size_t num) {
		for (auto &ng: m_ngrams) {
			ng.second.sort(num);
		}
	}

	size_t score(const S &text) const {
		size_t score = 0;

		for (auto &ng: m_ngrams) {
			score += ng.second.score(text) / ng.first;
		}

		return score;
	}

	MSGPACK_DEFINE(m_ngrams);

private:
	std::map<size_t, ngram<S>> m_ngrams;
};


template <typename S, typename D>
class detector {
public:
	detector() {}

	bool load_text(const S &text, const D &id) {
		auto it = m_probs.find(id);
		if (it != m_probs.end()) {
			return it->second.load_text(text);
		}

		probability<S> p(4);
		bool ret = p.load_text(text);
		if (ret)
			m_probs.emplace(std::pair<D, probability<S>>(id, p));
		return ret;
	}

	void sort(size_t num) {
		for (auto &p: m_probs) {
			p.second.sort(num);
		}
	}

	void reset_stats() {
		for (auto &p: m_probs) {
			p.second.reset_stats();
		}
	}

	D detect(const S &text) const {
		ssize_t min_score = -1;
		D name;

		for (auto &p: m_probs) {
			size_t score = p.second.score(text);
			if ((min_score == -1) || ((ssize_t)score < min_score)) {
				min_score = score;
				name = p.first;
			}
		}

		return name;
	}

	std::string save() const {
		std::stringstream buffer;
		msgpack::pack(buffer, *this);
		buffer.seekg(0);
		return buffer.str();
	}

	int save_file(const char *path) {
		std::ofstream output(path, std::ios::trunc);
		std::string content = save();

		output.write(content.data(), content.size());
		if (!output.good())
			return -1;

		return 0;
	}

	int load_file(const char *path) {
		std::ifstream input(path);
		std::ostringstream ss;
		ss << input.rdbuf();
		std::string load_content = ss.str();
		return load(load_content.data(), load_content.size());
	}

	int load(const char *data, size_t size) {
		msgpack::unpacked msg;
		try {
			msgpack::unpack(&msg, data, size);

			msg.get().convert(this);
		} catch (const std::exception &e) {
			return -EINVAL;
		}

		return 0;
	}

	MSGPACK_DEFINE(m_probs);

private:
	std::map<D, probability<S>> m_probs;
};

}} // namespace ioremap::warp

#endif /* __WARP_NGRAM_HPP */
