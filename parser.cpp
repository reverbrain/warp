#include <boost/locale.hpp>
#include <boost/program_options.hpp>

#include <fstream>
#include <iostream>
#include <string>

namespace lb = boost::locale::boundary;

namespace ioremap { namespace wookie { namespace lemmer {

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
};

struct parser {
	std::map<std::string, token_entity> t2p;
	int unique;

	void push(const std::vector<std::string> &tokens, token_class type) {
		for (auto it = tokens.begin(); it != tokens.end(); ++it) {
			if (insert(*it, it - tokens.begin() + unique, type))
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
		}

		return pos == t2p.end();
	}

	parser() : unique(0) {
		push({ "им", "род", "дат", "вин", "твор", "пр" }, wcase);
		push({ std::string("ед"), "мн" }, number);
		push({ std::string("неод"), "од" }, alive);
		push({ std::string("полн"), "кр" }, fullness);
		push({ "муж", "жен", "сред", "мж" }, family);
		push("устар", oldness);
		push({ std::string("прич"), "деепр" }, participle);
		push({ std::string("действ"), "страд" }, voice);
		push({ "имя", "отч", "фам" }, name);
		push({ "S", "A", "V", "PART", "PR", "CONJ", "INTJ", "ADV", "PRDK", "SPRO", "COM", "APRO", "ANUM" }, type);

		token_entity ent;
		ent = try_parse("ADV");
		insert("ADVPRO", ent.position, ent.type);
		ent = try_parse("ANUM");
		insert("NUM", ent.position, ent.type);

		push({ "наст", "прош", "буд" }, time);
		push({ "1", "2", "3" }, face);
		push({ std::string("сов"), "несов" }, completeness);
		push({ "сосл", "пов", "изъяв" }, inclination);
		push("гео", location);
		push("орг", organization);
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
	std::string	ending;
	token_entity	ent;
};

struct record {
	std::vector<parsed_word>	forms;
};

struct base_holder {
	std::locale m_loc;
	std::map<std::string, record> words;
	parser p;
	int total;

	std::set<std::string> letters;

	base_holder() : total(0) {
		boost::locale::generator gen;
		m_loc = gen("en_US.UTF8");
	}

	void parse_string(const std::string &token_str) {
		std::string token = boost::locale::to_lower(token_str, m_loc);

		lb::ssegment_index wmap(lb::word, token.begin(), token.end(), m_loc);
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

			rec.ent = p.try_parse(it->str());
			if (rec.ent.type == none)
				failed.push_back(it->str());
			else
				have_data = true;
		}

		if (!have_data)
			return;

		if (0 && failed.size()) {
			std::cout << token << ": root: " << root <<
				", ending: " << rec.ending <<
				", failed: ";
			for (auto it = failed.begin(); it != failed.end(); ++it)
				std::cout << *it << " ";
			std::cout << std::endl;
		}

		words[root].forms.emplace_back(rec);
		total++;

		std::string tmp = root + rec.ending;
		lb::ssegment_index cmap(lb::character, tmp.begin(), tmp.end(), m_loc);

		for (auto it = cmap.begin(), e = cmap.end(); it != e; ++it) {
			letters.insert(it->str());
		}
	}
};

static void output_string(std::ofstream &out, const std::vector<std::string> &letters, const std::string &word, std::locale &loc, int max)
{
	lb::ssegment_index wmap(lb::character, word.begin(), word.end(), loc);

	std::vector<double> out_vec(max, 0.001);
	int offset = 0;

	for (auto it = wmap.begin(), e = wmap.end(); it != e; ++it) {
		auto pos = std::lower_bound(letters.begin(), letters.end(), it->str());
		if (*pos == it->str()) {
			double val = pos - letters.begin() + 1;
			val = val / (letters.size() * 2.0);

			out_vec[offset++] = val;
			if (offset > max)
				break;
		}
	}

	std::reverse(out_vec.begin(), out_vec.end());
	for (auto it = out_vec.begin(); it != out_vec.end(); ++it)
		out << *it << " ";
}

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

	std::vector<std::string> letters;
	std::ofstream out_letter_map(output_file + ".letters", std::ios::trunc | std::ios::binary);
	for (auto it = records.letters.begin(); it != records.letters.end(); ++it) {
		out_letter_map << *it << std::endl;
		letters.push_back(*it);
	}

	int ending_size = 8;
	int word_size = 24 + ending_size;

	out << records.total << " " << word_size << " " << records.p.unique + ending_size << std::endl;
	for (auto root = records.words.begin(); root != records.words.end(); ++root) {
		for (auto rec = root->second.forms.begin(); rec != root->second.forms.end(); ++rec) {
			std::string word = root->first + rec->ending;

			output_string(out, letters, word, records.m_loc, word_size);
			out << "\n";

			for (int i = 0; i < records.p.unique; ++i) {
				double tmp = 0.001;
				if (i == rec->ent.position)
					tmp = 1.0;

				out << tmp << " ";
			}

			output_string(out, letters, rec->ending, records.m_loc, ending_size);
			out << "\n";
		}
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
