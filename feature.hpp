#ifndef __WARP_FEATURE_HPP
#define __WARP_FEATURE_HPP

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <set>

#include <boost/locale.hpp>

namespace ioremap { namespace warp {

static const float default_zero = 0.001;

namespace lb = boost::locale::boundary;

enum token_class {
	none = 0,
	wcase,
	family,
	number,
	alive,
	fullness,
	participle,
	voice,
	name,
	type,
	oldness,
	time,
	completeness,
	inclination,
	face,
	comparison,
	location,
	organization,
	infinitive,
	possessive,
	jargon,
	profanity,

	weird,
};

struct token_entity {
	token_entity() : type(none), position(-1) {}
	token_class	type;
	int		position;

	bool operator() (const token_entity &a, const token_entity &b) {
		return a.position < b.position;
	}
};

struct parser {
	std::map<std::string, token_entity> t2p;
	int unique;

	void push(const std::vector<std::string> &tokens, token_class type) {
		for (auto it = tokens.begin(); it != tokens.end(); ++it) {
			if (insert(*it, unique, type))
				++unique;
		}
	}

	void push(const std::string &bool_token, token_class type) {
		if (insert(bool_token, unique, type))
			++unique;
	}

	token_entity try_parse(const std::string &token) {
		token_entity tok;

		auto it = t2p.find(token);
		if (it != t2p.end())
			tok = it->second;

		return tok;
	}

	bool insert(const std::string &token, int position, token_class type) {
		token_entity t;

		auto pos = t2p.find(token);
		if (pos == t2p.end()) {
			t.type = type;
			t.position = position;

			t2p[token] = t;

			std::cout << token << ": " << type << "." << position << std::endl;
		}

		return pos == t2p.end();
	}

	parser() : unique(0) {
		push({ "им", "род", "дат", "вин", "твор", "пр" }, wcase);
		push({ std::string("ед"), "мн" }, number);
		push({ std::string("неод"), "од" }, alive);
		//push({ std::string("полн"), "кр" }, fullness);
		push({ "муж", "жен", "сред", "мж" }, family);
		//push("устар", oldness);
		//push({ std::string("прич"), "деепр" }, participle);
		//push({ std::string("действ"), "страд" }, voice);
		push({ "имя", "отч", "фам" }, name);
		push({ "S", "A", "V", "PART", "PR", "CONJ", "INTJ", "ADV", "PRDK", "SPRO", "COM", "APRO", "ANUM" }, type);

		token_entity ent;
		ent = try_parse("ADV");
		insert("ADVPRO", ent.position, ent.type);
		ent = try_parse("ANUM");
		insert("NUM", ent.position, ent.type);

		push({ "наст", "прош", "буд" }, time);
		push({ "1", "2", "3" }, face);
		//push({ std::string("сов"), "несов" }, completeness);
		//push({ "сосл", "пов", "изъяв" }, inclination);
		//push("гео", location);
		//push("орг", organization);
		push({ std::string("срав"), "прев" }, comparison);
		push("инф", infinitive);

		push("притяж", possessive);
		ent = try_parse("притяж");
		insert("AOT_притяж", ent.position, ent.type);

		push("жарг", jargon);

		push("obsclite", profanity);
		ent = try_parse("obsclite");
		insert("обсц", ent.position, ent.type);

		//push({ "непрош", "пе", "-", "л", "нп", "reserved", "AOT_разг", "dsbl", "сокр", "парт", "вводн", "местн", "редк", "AOT_ФРАЗ", "AOT_безл", "зват", "разг", "AOT_фраз", "AOT_указат", "буфф" }, weird);
	}

};

struct parsed_word {
	std::string			ending;
	std::vector<token_entity>	ent;
};

struct record {
	std::vector<parsed_word>	forms;
};

class base_holder {
	public:
		base_holder() : m_total(0) {
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
				if (ent.type == none) {
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
					std::cout << a.type << "." << a.position << " ";
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

		template <typename T>
		std::vector<T> convert_word(const std::vector<std::string> &lv, const std::string &word, int max) {
			lb::ssegment_index wmap(lb::character, word.begin(), word.end(), m_loc);

			std::vector<T> out_vec;
			out_vec.resize(max * lv.size(), default_zero);

			std::vector<int> positions;

			for (auto it = wmap.begin(), e = wmap.end(); it != e; ++it) {
				auto pos = std::lower_bound(lv.begin(), lv.end(), it->str());
				if (*pos == it->str()) {
					positions.push_back(pos - lv.begin());
				}
			}

			int letter = 0;
			for (auto pos = positions.rbegin(); pos != positions.rend(); ++pos) {
				out_vec[*pos + letter * lv.size()] = 1;
				if (--max == 0)
					break;
				++letter;
			}

			return out_vec;
		}

		void output_string(std::ofstream &out, const std::vector<std::string> &lv, const std::string &word, int max) {
			std::vector<float> f = convert_word<float>(lv, word, max);

			for (auto it = f.begin(); it != f.end(); ++it)
				out << *it << " ";
		}

		std::vector<std::string> load_letters(const std::string &path) {
			std::ifstream in(path.c_str());

			if (!in.good()) {
				std::ostringstream ss;
				ss << "Failed to open letters file '" << path << "'";
				throw std::runtime_error(ss.str());
			}

			std::string line;
			while (std::getline(in, line)) {
				m_letters.insert(line);
			}

			std::vector<std::string> lv;
			std::copy(m_letters.begin(), m_letters.end(), std::back_inserter(lv));

			return lv;
		}

		void dump_letters(const std::string &output) {
			std::ofstream out_letter_map(output, std::ios::trunc | std::ios::binary);
			for (auto it = m_letters.begin(); it != m_letters.end(); ++it) {
				out_letter_map << *it << std::endl;
			}
		}

		void dump_features(const std::string &output, int word_size, int ending_size) {
			std::vector<std::string> lv;
			std::copy(m_letters.begin(), m_letters.end(), std::back_inserter(lv));

			std::ofstream out(output.c_str(), std::ios::trunc | std::ios::binary);

			dump_letters(output + ".letters");

			int word_num = 0;
			int features = 0;
			out << m_total << " " << word_size * m_letters.size() << " " << m_p.unique + ending_size * m_letters.size() << std::endl;
			for (auto root = m_words.begin(); root != m_words.end(); ++root) {
				for (auto rec = root->second.forms.begin(); rec != root->second.forms.end(); ++rec) {
					std::string word = root->first + rec->ending;

					output_string(out, lv, word, word_size);
					out << "\n";

					output_string(out, lv, rec->ending, ending_size);

					size_t pos = 0;
					for (int i = 0; i < m_p.unique; ++i) {
						float tmp = default_zero;

						if ((pos < rec->ent.size()) && (i == rec->ent[pos].position)) {
							tmp = 1.0;
							++pos;
						}

						out << tmp << " ";
					}
					out << "\n";
					features++;
				}

				if (++word_num % (m_words.size() / 100) == 0)
					std::cout << "Dumped " << word_num << "/" << m_words.size() << " words, number of features: " << features << std::endl;
			}

			std::cout << "Dumped " << word_num << "/" << m_words.size() << " words, number of features: " << features << std::endl;
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
