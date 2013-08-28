#include <boost/locale.hpp>
#include <boost/program_options.hpp>

#include <fstream>
#include <iostream>
#include <string>

namespace ioremap { namespace wookie { namespace lemmer {

enum wcase {
	ime = 0,
	rod,
	dat,
	vin,
	tvo,
	pre
};

enum number {
	single = 0,
	multiple
};

enum alive {
	nonalive = 0,
	live
};

enum fullness {
	full = 0,
	brief
};

enum family {
	male = 0,
	female,
	median
};

enum oldness {
	normal = 0,
	old,
};

enum word_type {
	S = 0,
	V,
	A,
	PART,
	PR,
	INTJ,
	CONJ,
	ADV,
};

struct wordbase {
	std::string	ending;
};

struct S : public wordbase {
	wcase		cs;
	family		fam;
	number		num;
	alive		live;
	fullness	full;
};

struct A : public wordbase {
	wcase		cs;
	family		fam;
	number		num;
	fullness	full;
};

struct PART {
};

struct PR {
};

struct CONJ {
};

struct INTJ {
};

struct ADV {
};

enum participle_type {
	normal_verb = 0,
	participle,
	departiciple,
};

enum voice {
	active = 0,
	passive
};

enum word_time {
	present = 0,
	past,
	future,
};

struct V : public wordbase {
	participle_type	participle;
	voice		v;
	family		fam;
	wcase		cs;
	number		num;
};

template <typename T>
struct record {
	record() {
	}

	std::string	root;

	std::vector<T>	entries;
};

static void parse(const std::string &input_file, const std::string &output_file)
{
	std::ifstream in(input_file.c_str());
	std::ofstream out(output_file.c_str(), std::ios::trunc | std::ios::binary);

	std::string line;
	while (std::getline(in, line)) {
		if (line.substr(0, 5) == "@ID: ") {
			char type = *line.rbegin();
			std::cout << "Type: " << type << std::endl;
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
