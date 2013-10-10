#ifndef __WARP_FEATURE_HPP
#define __WARP_FEATURE_HPP

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <set>

#include <boost/locale.hpp>

#include "timer.hpp"

namespace ioremap { namespace warp {

static const float default_zero = 0.001;

namespace lb = boost::locale::boundary;

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

	token_entity try_parse(const std::string &token) {
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

			std::cout << token << ": " << position << std::endl;
		}

		return pos == t2p.end();
	}

	parser() : unique(0) {
		push({ "им", "род", "дат", "вин", "твор", "пр" });
		push({ std::string("ед"), "мн" });
		push({ std::string("неод"), "од" });
		//push({ std::string("полн"), "кр" });
		push({ "муж", "жен", "сред", "мж" });
		//push("устар", oldness);
		//push({ std::string("прич"), "деепр" });
		//push({ std::string("действ"), "страд" });
		push({ "имя", "отч", "фам" });
		push({ "S", "A", "V", "PART", "PR", "CONJ", "INTJ", "ADV", "PRDK", "SPRO", "COM", "APRO", "ANUM" });

		token_entity ent;
		ent = try_parse("ADV");
		insert("ADVPRO", ent.position);
		ent = try_parse("ANUM");
		insert("NUM", ent.position);

		push({ "наст", "прош", "буд" });
		push({ "1", "2", "3" });
		//push({ std::string("сов"), "несов" });
		//push({ "сосл", "пов", "изъяв" });
		//push("гео", location);
		//push("орг", organization);
		push({ std::string("срав"), "прев" });
		push("инф");

		push("притяж");
		ent = try_parse("притяж");
		insert("AOT_притяж", ent.position);

		push("жарг");

		push("obsclite");
		ent = try_parse("obsclite");
		insert("обсц", ent.position);

		//push({ "непрош", "пе", "-", "л", "нп", "reserved", "AOT_разг", "dsbl", "сокр", "парт", "вводн", "местн", "редк", "AOT_ФРАЗ", "AOT_безл", "зват", "разг", "AOT_фраз", "AOT_указат", "буфф" });
	}

};

struct parsed_word {
	std::string			ending;
	std::vector<token_entity>	ent;
};

struct record {
	std::vector<parsed_word>	forms;
};

class zparser {
	public:
		zparser() : m_total(0) {
			boost::locale::generator gen;
			m_loc = gen("en_US.UTF8");
		}

		std::vector<std::string> split(const std::string &sentence) {
			lb::ssegment_index wmap(lb::word, sentence.begin(), sentence.end(), m_loc);
			wmap.rule(lb::word_any);

			std::vector<std::string> ret;
			for (auto it = wmap.begin(), e = wmap.end(); it != e; ++it) {
				ret.push_back(it->str());
			}

			return ret;
		}

		void parse_dict_string(const std::string &token_str) {
			std::string token = boost::locale::to_lower(token_str, m_loc);

			lb::ssegment_index wmap(lb::word, token.begin(), token.end(), m_loc);
			wmap.rule(lb::word_any | lb::word_none);

			std::string root;
			parsed_word rec;

			std::vector<std::string> failed;

			for (auto it = wmap.begin(), e = wmap.end(); it != e; ++it) {
				// skip word prefixes
				if ((root.size() == 0) && (it->str() != "["))
					continue;

				if (it->str() == "[") {
					if (++it == e)
						break;
					root = it->str();

					if (++it == e)
						break;

					// something is broken, skip this line at all
					if (it->str() != "]")
						break;

					if (++it == e)
						break;

					// ending check
					// we assume here that ending can start with the word token
					if (it->rule() & lb::word_any) {
						do {
							// there is no ending in this word if next token is space (or anything else if that matters)
							// only not space tokens here mean (parts of) word ending, let's check it
							if (isspace(it->str()[0]))
								break;

							rec.ending += it->str();

							if (++it == e)
								break;
						} while (true);

						if (it == e)
							break;
					}
				}

				// skip this token if it is not word token
				if (!(it->rule() & lb::word_any))
					continue;

				token_entity ent = m_p.try_parse(it->str());
				if (ent.position == -1) {
					failed.push_back(it->str());
				} else {
					rec.ent.emplace_back(ent);
				}
			}

			if (!rec.ent.size())
				return;

			std::sort(rec.ent.begin(), rec.ent.end(), token_entity());

			if (0 && failed.size()) {
				std::cout << token << ": root: " << root <<
					", ending: " << rec.ending <<
					", failed: ";
				for (auto it = failed.begin(); it != failed.end(); ++it)
					std::cout << *it << " ";
				std::cout << std::endl;
			} else if (0) {
				std::cout << token << ": root: " << root <<
					", ending: " << rec.ending <<
					", features: " << rec.ent.size() <<
					", total unique: " << m_p.unique <<
					": ";
				for (const auto & a : rec.ent) {
					std::cout << a.position << " ";
				}
				std::cout << std::endl;
			}

			m_words[root].forms.emplace_back(rec);
			m_total++;

			std::string tmp = root + rec.ending;
			lb::ssegment_index cmap(lb::character, tmp.begin(), tmp.end(), m_loc);

			for (auto it = cmap.begin(), e = cmap.end(); it != e; ++it) {
				m_letters.insert(it->str());
			}
		}

		void parse_file(const std::string &input_file) {
			std::ifstream in(input_file.c_str());

			ioremap::warp::timer t;
			ioremap::warp::timer total;

			std::string line;
			std::string word;

			long lines = 0;
			long chunk = 100000;
			long duration;
			while (std::getline(in, line)) {
				if (++lines % chunk == 0) {
					duration = t.restart();
					std::cout << "Read and parsed " << lines << " lines, took: " << duration << " msecs, speed: " << chunk * 1000 / duration << " lines/sec" << std::endl;
				}

				if (line.substr(0, 5) == "@ID: ") {
					// skip next line - it contains original word
					if (!std::getline(in, line))
						break;

					continue;
				}

				parse_dict_string(line);
			}
			duration = total.restart();
			std::cout << "Read and parsed " << lines << " lines, took: " << duration << " msecs, speed: " << lines * 1000 / duration << " lines/sec" << std::endl;
		}

		const std::set<std::string> &letters(void) const {
			return m_letters;
		}

		const std::map<std::string, record> &words(void) const {
			return m_words;
		}

		int parser_features_num(void) const {
			return m_p.unique;
		}

		int total_features_num(void) const {
			return m_total;
		}

	private:
		std::locale m_loc;
		std::map<std::string, record> m_words;
		parser m_p;
		int m_total;

		std::set<std::string> m_letters;
};


}} // namespace ioremap::warp

#endif /* __WARP_FEATURE_HPP */
