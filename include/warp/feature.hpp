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

#ifndef __WARP_FEATURE_HPP
#define __WARP_FEATURE_HPP

#include <algorithm>
#include <atomic>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <set>

#include <boost/algorithm/string.hpp>

#include <ribosome/lstring.hpp>
#include <ribosome/timer.hpp>

#include <msgpack.hpp>

namespace ioremap { namespace warp {

struct token_entity {
	token_entity() : position(-1) {}
	int		position;

	bool operator() (const token_entity &a, const token_entity &b) {
		return a.position < b.position;
	}
};

struct parser {
	std::map<std::string, token_entity> t2p;
	int unique;

	void push(const std::vector<std::string> &tokens) {
		for (auto it = tokens.begin(); it != tokens.end(); ++it) {
			if (insert(*it, unique))
				++unique;
		}
	}

	void push(const std::string &bool_token) {
		if (insert(bool_token, unique))
			++unique;
	}

	void push(const std::initializer_list<std::string> &l) {
		for (auto it = l.begin(); it != l.end(); ++it) {
			if (insert(*it, unique))
				++unique;
		}
	}

	token_entity try_parse(const std::string &token) const {
		token_entity tok;

		auto it = t2p.find(token);
		if (it != t2p.end())
			tok = it->second;

		return tok;
	}

	bool insert(const std::string &token, int position) {
		token_entity t;

		auto pos = t2p.find(token);
		if (pos == t2p.end()) {
			t.position = position;

			t2p[token] = t;
			//std::cout << token << ": " << position << std::endl;
			return true;
		}

		return false;
	}

	parser() : unique(0) {
		push({ "им", "род", "дат", "вин", "твор", "пр" });
		push({ "ед", "мн" });
		push({ "неод", "од" });
		push({ "полн", "кр" });
		push({ "муж", "жен", "сред", "мж" });
		push("устар");
		push({ "прич", "деепр" });
		push({ "действ", "страд" });
		push({ "имя", "отч", "фам" });
		push({ "S", "A", "V", "PART", "PR", "CONJ", "INTJ", "ADV", "PRDK", "SPRO", "COM", "APRO", "ANUM" });

		token_entity ent;
		ent = try_parse("ADV");
		insert("ADVPRO", ent.position);
		ent = try_parse("ANUM");
		insert("NUM", ent.position);

		push({ "наст", "прош", "буд" });
		push({ "1", "2", "3" });
		push({ "сов", "несов" });
		push({ "сосл", "пов", "изъяв" });
		push("гео");
		push("орг");
		push({ "срав", "прев" });
		push("инф");

		push("притяж");
		ent = try_parse("притяж");
		insert("AOT_притяж", ent.position);

		push("жарг");

		push("obsclite");
		ent = try_parse("obsclite");
		insert("обсц", ent.position);

		push("weired");
		ent = try_parse("weired");
		for (auto w : { "непрош", "пе", "-", "л", "нп", "reserved", "AOT_разг", "dsbl", "сокр", "парт", "вводн", "местн",
				"редк", "AOT_ФРАЗ", "AOT_безл", "зват", "разг", "AOT_фраз", "AOT_указат", "буфф" }) {
			insert(w, ent.position);
		}
	}

};

struct feature {
	uint64_t mask;
	std::string string_ending;
	ribosome::lstring ending;

	MSGPACK_DEFINE(mask, string_ending);

	bool operator<(const feature &other) const {
		if (mask < other.mask)
			return true;
		if (mask > other.mask)
			return false;
		return ending < other.ending;
	}
};

struct parsed_word {
	ribosome::lstring lemma;
	ribosome::lstring stem;
	uint64_t indexed_id = 0;
	int root_len = 0;

	std::vector<feature> features;

	parsed_word() {
	}

	void reset() {
		features.clear();
		root_len = 0;
		lemma = ribosome::lstring();
	}

	void merge(const std::vector<feature> &other) {
		std::set<feature> f(features.begin(), features.end());
		f.insert(other.begin(), other.end());

		features.clear();
		features.insert(features.end(), f.begin(), f.end());
	}

	enum {
		serialize_version_6 = 6,
	};

	template <typename Stream>
	void msgpack_pack(msgpack::packer<Stream> &o) const {
		o.pack_array(serialize_version_6);
		o.pack((int)serialize_version_6);
		o.pack(ribosome::lconvert::to_string(lemma));
		o.pack(ribosome::lconvert::to_string(stem));
		o.pack(indexed_id);
		o.pack(root_len);
		o.pack(features);
	}

	void msgpack_unpack(msgpack::object o) {
		if (o.type != msgpack::type::ARRAY || o.via.array.size < 1) {
			std::ostringstream ss;
			ss << "parsed_word msgpack: type: " << o.type <<
				", must be: " << msgpack::type::ARRAY <<
				", size: " << o.via.array.size;
			throw std::runtime_error(ss.str());
		}

		msgpack::object *p = o.via.array.ptr;
		const uint32_t size = o.via.array.size;
		uint16_t version = 0;
		p[0].convert(&version);

		if (version != size) {
			std::ostringstream ss;
			ss << "parsed_word msgpack: invalid version: " << version <<
				", must be equal to array size: " << size;
			throw std::runtime_error(ss.str());
		}

		std::string tmp;

		switch (version) {
		case serialize_version_6:
			p[1].convert(&tmp);
			lemma = ribosome::lconvert::from_utf8(tmp);
			p[2].convert(&tmp);
			stem = ribosome::lconvert::from_utf8(tmp);
			p[3].convert(&indexed_id);
			p[4].convert(&root_len);
			p[5].convert(&features);
			break;
		default: {
			std::ostringstream ss;
			ss << "parsed_word msgpack: invalid version " << version;
			throw msgpack::type_error();
		}
		}
	}

};

class zparser {
public:
	// return false if you want to stop further processing
	typedef std::function<ribosome::error_info (struct parsed_word &rec)> zparser_process;
	zparser(zparser_process process, const std::string &skip, const std::string &pass) : m_process(process) {
		if (skip.size())
			m_skip_mask = parse_features(skip, NULL);
		if (pass.size())
			m_pass_mask = parse_features(pass, NULL);
	}

	uint64_t parse_features(const std::string &elm_string, std::vector<std::string> *failed) const {
		std::vector<std::string> elements;
		boost::split(elements, elm_string, boost::is_any_of(","));

		uint64_t feature_mask = 0;
		for (auto &elm: elements) {
			token_entity ent = m_p.try_parse(elm);
			if (ent.position == -1) {
				if (failed)
					failed->push_back(elm);
			} else {
				if (ent.position < (int)sizeof(feature::mask) * 8)
					feature_mask |= (uint64_t)1 << ent.position;
			}
		}

		return feature_mask;
	}

	ribosome::error_info parse_dict_string(const std::string &token) {
		ribosome::error_info err;

		size_t space_pos = token.find(' ');
		if (space_pos == std::string::npos) {
			return err;
		}

		if (space_pos <= 1) {
			return err;
		}

		std::string mixed_word = token.substr(1, space_pos - 1);
		ribosome::lstring lw = ribosome::lconvert::from_utf8(mixed_word);

		size_t root_end = lw.find(']');
		if (root_end == ribosome::lstring::npos) {
			return err;
		}

		size_t ending_len = lw.size() - (root_end + 1);
		lw.erase(root_end, 1);

		std::string elm_string = token.substr(space_pos + 1);

		struct feature f;
		f.mask = parse_features(elm_string, NULL);
		if (f.mask == 0ULL)
			return err;
		if (f.mask & m_skip_mask)
			return err;
		if (!(f.mask & m_pass_mask))
			return err;

		//std::cout << "parse: " << ribosome::lconvert::to_string(lw) << ": " << elm_string << ", feaures: " << std::hex << f.mask << std::endl;
		f.ending = lw.substr(lw.size() - ending_len);
		f.string_ending = ribosome::lconvert::to_string(f.ending);

		m_current.root_len = root_end;
		m_current.features.emplace_back(f);
		return err;
	}

	ribosome::error_info parse_file(const std::string &input_file) {
		ribosome::error_info err;

		std::ifstream in(input_file.c_str());

		ribosome::timer t;
		ribosome::timer file_time;

		std::string line;

		long total = 0;
		long lemmas = 0;
		long lines = 0;
		long chunk = 100000;
		long duration;
		bool read_lemma = false;

		while (std::getline(in, line)) {
			if (++lines % chunk == 0) {
				duration = t.restart();
				std::cout << "Read and parsed lines: " << lines <<
					", total words/features found: " << total <<
					", lemmas: " << lemmas <<
					", elapsed time: " << file_time.elapsed() << " msecs" <<
					", speed: " << chunk * 1000 / duration << " lines/sec" <<
					std::endl;
			}

			if (line[0] == '@') {
				read_lemma = false;
				continue;
			}

			if (!read_lemma) {
				err = m_process(m_current);
				m_current.reset();

				if (err) {
					return err;
				}

				m_current.reset();

				// next line contains lemma word
				auto l = ribosome::lconvert::from_utf8(line);
				read_lemma = true;

				m_current.lemma = ribosome::lconvert::to_lower(l);
				lemmas++;
				continue;
			}

			err = parse_dict_string(line);
			if (err) {
				return err;
			}
			total++;
		}
		if (m_current.features.size()) {
			err = m_process(m_current);
			if (err) {
				return err;
			}
		}

		duration = file_time.elapsed();
		std::cout << "Read and parsed " << lines << " lines"
			", lemmas: " << lemmas <<
			", elapsed: " << duration <<
			" msecs, speed: " << lines * 1000 / duration << " lines/sec" << std::endl;

		return err;
	}

private:
	parser m_p;
	zparser_process m_process;

	uint64_t m_skip_mask = 0;
	uint64_t m_pass_mask = ~0ULL;

	parsed_word m_current;
};


}} // namespace ioremap::warp

#endif /* __WARP_FEATURE_HPP */
