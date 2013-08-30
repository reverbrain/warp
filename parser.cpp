#include <boost/locale.hpp>
#include <boost/program_options.hpp>

#include <fstream>
#include <iostream>
#include <string>

namespace ioremap { namespace wookie { namespace lemmer {

struct base_token {
	base_token() : value(0) {}

	bool set(const std::vector<std::string> &tokens, const std::string &token) {
		for (auto it = tokens.begin(); it != tokens.end(); ++it) {
			if (*it == token) {
				value = it - tokens.begin();
				return true;
			}
		}

		return false;
	}

	virtual bool try_parse(const std::string &) = 0;

	int value;
};

struct wcase : public base_token {
	bool try_parse(const std::string &token) {
		static std::vector<std::string> wcase_tokens = { "им", "род", "дат", "вин", "твор", "пр" };
		return set(wcase_tokens, token);
	}
};

struct number : public base_token {
	bool try_parse(const std::string &token) {
		static std::vector<std::string> number_tokens = { "ед", "мн" };
		return set(number_tokens, token);
	}
};

struct alive : public base_token {
	bool try_parse(const std::string &token) {
		static std::vector<std::string> alive_tokens = { "неод", "од" };
		return set(alive_tokens, token);
	}
};

struct fullness : public base_token {
	bool try_parse(const std::string &token) {
		static std::vector<std::string> fullness_tokens = { "полн", "кр" };
		return set(fullness_tokens, token);
	}
};

struct family : public base_token {
	bool try_parse(const std::string &token) {
		static std::vector<std::string> family_tokens = { "муж", "жен", "сред", "мж" };
		return set(family_tokens, token);
	}
};

struct oldness : public base_token {
	bool try_parse(const std::string &token) {
		static std::vector<std::string> oldness_tokens = { "XXXXXXX", "устар" };
		return set(oldness_tokens, token);
	}
};

struct participle : public base_token {
	bool try_parse(const std::string &token) {
		static std::vector<std::string> participle_tokens = { "ЪЪЪЪЪЪ", "прич", "деепр" };
		return set(participle_tokens, token);
	}
};

struct voice : public base_token {
	bool try_parse(const std::string &token) {
		static std::vector<std::string> voice_tokens = { "действ", "страд" };
		return set(voice_tokens, token);
	}
};

struct name : public base_token {
	bool try_parse(const std::string &token) {
		static std::vector<std::string> name_tokens = { "name-token", "имя", "отч", "фам" };
		return set(name_tokens, token);
	}
};

struct type : public base_token {
	bool try_parse(const std::string &token) {
		static std::vector<std::string> type_tokens1 = { "S", "A", "V", "PART", "PR", "CONJ", "INTJ", "ADV", "PRDK", "SPRO", "COM", "APRO", "ANUM" };
		if (set(type_tokens1, token))
			return true;
		static std::vector<std::string> type_tokens2 = { "S", "A", "V", "PART", "PR", "CONJ", "INTJ", "ADVPRO", "PRDK", "SPRO", "COM", "APRO", "NUM" };
		if (set(type_tokens2, token))
			return true;

		return false;
	}
};

struct time : public base_token {
	bool try_parse(const std::string &token) {
		static std::vector<std::string> time_tokens = { "наст", "прош", "буд" };
		return set(time_tokens, token);
	}
};

struct face : public base_token {
	bool try_parse(const std::string &token) {
		static std::vector<std::string> face_tokens = { "1", "2", "3" };
		return set(face_tokens, token);
	}
};

struct completeness : public base_token {
	bool try_parse(const std::string &token) {
		static std::vector<std::string> complete_tokens = { "несов", "сов" };
		return set(complete_tokens, token);
	}
};

struct inclination : public base_token {
	bool try_parse(const std::string &token) {
		static std::vector<std::string> incline_tokens = { "сосл", "пов", "изъяв" };
		return set(incline_tokens, token);
	}
};

struct location : public base_token {
	bool try_parse(const std::string &token) {
		static std::vector<std::string> loc_tokens = { "asdasd", "гео" };
		return set(loc_tokens, token);
	}
};

struct organization : public base_token {
	bool try_parse(const std::string &token) {
		static std::vector<std::string> org_tokens = { "asdasd", "орг" };
		return set(org_tokens, token);
	}
};

struct comparison : public base_token {
	bool try_parse(const std::string &token) {
		static std::vector<std::string> comp_tokens = { "asdasd", "срав", "прев" };
		return set(comp_tokens, token);
	}
};

struct infinitive : public base_token {
	bool try_parse(const std::string &token) {
		static std::vector<std::string> inf_tokens = { "asdasd", "инф" };
		return set(inf_tokens, token);
	}
};

struct possessive : public base_token {
	bool try_parse(const std::string &token) {
		// positions for the same entities have to match
		static std::vector<std::string> poss_tokens1 = { "asdasd", "притяж" };
		if (set(poss_tokens1, token))
			return true;
		static std::vector<std::string> poss_tokens2 = { "asdasd", "AOT_притяж" };
		if (set(poss_tokens2, token))
			return true;

		return false;
	}
};

struct jargon : public base_token {
	bool try_parse(const std::string &token) {
		static std::vector<std::string> jar_tokens = { "asdasd", "жарг" };
		return set(jar_tokens, token);
	}
};

struct profanity : public base_token {
	bool try_parse(const std::string &token) {
		static std::vector<std::string> profan_tokens = { "asdasd", "obsclite", "обсц" };
		return set(profan_tokens, token);
	}
};

struct weird : public base_token {
	bool try_parse(const std::string &token) {
		static std::vector<std::string> weird_tokens = { "непрош", "пе", "-", "л", "нп", "reserved", "AOT_разг", "dsbl", "сокр",
			"парт", "вводн", "местн", "редк", "AOT_ФРАЗ", "AOT_безл", "зват", "разг", "AOT_фраз", "AOT_указат", "буфф" };
		return set(weird_tokens, token);
	}
};

struct parsed_word {
	std::string	ending;

	wcase		cs;
	family		fam;
	number		num;
	alive		live;
	fullness	full;
	participle	partic;
	voice		v;
	name		nam;
	type		t;
	oldness		old;
	time		tm;
	completeness	comp;
	inclination	inc;
	face		fc;
	comparison	cmp;
	location	loc;
	organization	org;
	infinitive	inf;
	possessive	poss;
	jargon		jar;
	profanity	profan;

	weird		skip;

	bool parse(const std::string &token) {
		if (cs.try_parse(token))
			return true;
		if (fam.try_parse(token))
			return true;
		if (num.try_parse(token))
			return true;
		if (live.try_parse(token))
			return true;
		if (full.try_parse(token))
			return true;
		if (partic.try_parse(token))
			return true;
		if (v.try_parse(token))
			return true;
		if (nam.try_parse(token))
			return true;
		if (t.try_parse(token))
			return true;
		if (old.try_parse(token))
			return true;
		if (tm.try_parse(token))
			return true;
		if (comp.try_parse(token))
			return true;
		if (inc.try_parse(token))
			return true;
		if (fc.try_parse(token))
			return true;
		if (loc.try_parse(token))
			return true;
		if (cmp.try_parse(token))
			return true;
		if (org.try_parse(token))
			return true;
		if (inf.try_parse(token))
			return true;
		if (poss.try_parse(token))
			return true;
		if (jar.try_parse(token))
			return true;
		if (profan.try_parse(token))
			return true;

		if (skip.try_parse(token))
			return true;

		return false;
	}
};

struct record {
	std::vector<parsed_word>	forms;
};

struct base_holder {
	std::locale m_loc;
	std::map<std::string, record> words;

	base_holder() {
		boost::locale::generator gen;
		m_loc = gen("en_US.UTF8");
	}

	void parse_string(const std::string &token_str) {
		namespace lb = boost::locale::boundary;

		lb::ssegment_index wmap(lb::word, token_str.begin(), token_str.end(), m_loc);
		wmap.rule(lb::word_any | lb::word_none);

		std::string root;
		parsed_word rec;

		bool have_data = false;
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

			if (!rec.parse(it->str()))
				failed.push_back(it->str());

			have_data = true;
		}

		if (!have_data)
			return;

		if (failed.size()) {
			std::cout << token_str << ": root: " << root <<
				", ending: " << rec.ending <<
				", failed: ";
			for (auto it = failed.begin(); it != failed.end(); ++it)
				std::cout << *it << " ";
			std::cout << std::endl;
		} else if (0) {
			std::cout << token_str << ": root: " << root <<
				", ending: " << rec.ending <<
				std::endl;
		}

		words[root].forms.emplace_back(rec);
	}
};

static void parse(const std::string &input_file, const std::string &output_file)
{
	std::ifstream in(input_file.c_str());
	std::ofstream out(output_file.c_str(), std::ios::trunc | std::ios::binary);

	base_holder records;

	std::string line;
	std::string word;

	while (std::getline(in, line)) {
		if (line.substr(0, 5) == "@ID: ") {
			// skip next line - it contains original word
			if (!std::getline(in, line))
				break;

			continue;
		}

		records.parse_string(line);
	}
}

}}}

using namespace ioremap::wookie::lemmer;

int main(int argc, char *argv[])
{
	namespace bpo = boost::program_options;

	bpo::options_description op("Parser options");

	std::string input, output;
	op.add_options()
		("help", "This help message")
		("input", bpo::value<std::string>(&input), "Input file")
		("output", bpo::value<std::string>(&output), "Output file")
		;

	bpo::variables_map vm;
	bpo::store(bpo::parse_command_line(argc, argv, op), vm);
	bpo::notify(vm);

	if (!vm.count("input") || !vm.count("output")) {
		std::cerr << "You must provide input and output files\n" << op << std::endl;
		return -1;
	}

	parse(input, output);
}
