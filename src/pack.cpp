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

#include "warp/pack.hpp"

#include <boost/program_options.hpp>

int main(int argc, char *argv[])
{
	namespace bpo = boost::program_options;

	bpo::options_description generic("Parser options");

	int output_num;
	std::string input, output, msgin, gram;
	generic.add_options()
		("help", "This help message")
		("input", bpo::value<std::string>(&input)->required(), "Input Zaliznyak dictionary file")
		("output", bpo::value<std::string>(&output)->required(), "Output msgpack file")
		("output-num", bpo::value<int>(&output_num)->default_value(1), "Number of output msgpack files")
		;

	bpo::positional_options_description p;
	p.add("words", -1);

	std::vector<std::string> words;
	bpo::options_description hidden("Hidden options");
	hidden.add_options()
		("words", bpo::value<std::vector<std::string>>(&words), "lookup words")
	;

	bpo::options_description cmdline_options;
	cmdline_options.add(generic).add(hidden);

	bpo::variables_map vm;

	try {
		bpo::store(bpo::command_line_parser(argc, argv).options(cmdline_options).positional(p).run(), vm);

		if (vm.count("help")) {
			std::cout << generic << std::endl;
			return 0;
		}

		bpo::notify(vm);
	} catch (const std::exception &e) {
		std::cerr << "Invalid options: " << e.what() << "\n" << generic << std::endl;
		return -1;
	}

	namespace iw = ioremap::warp;

	iw::packer pack(output, output_num);
	iw::zparser records;
	records.set_process(std::bind(&iw::packer::zprocess, &pack, std::placeholders::_1));
	records.parse_file(input);

	return 0;
}


