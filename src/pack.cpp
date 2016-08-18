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

#include "warp/database.hpp"
#include "warp/feature.hpp"
#include "warp/ngram.hpp"
#include "warp/pack.hpp"
#include "warp/stem.hpp"
#include "warp/utils.hpp"

#include <boost/program_options.hpp>

using namespace ioremap;

int main(int argc, char *argv[])
{
	namespace bpo = boost::program_options;

	bpo::options_description generic("Parser options");

	std::string skip, pass;
	std::string input, rocksdb_path;
	generic.add_options()
		("help", "This help message")
		("input", bpo::value<std::string>(&input)->required(), "Input Zaliznyak dictionary file")
		("rocksdb", bpo::value<std::string>(&rocksdb_path)->required(), "Output rocksdb path")
		("skip", bpo::value<std::string>(&skip),
		 	"Comma-separated features which will force word to be skipped from indexing if present")
		("pass", bpo::value<std::string>(&pass),
		 	"Comma-separated features which will force word to be skipped from indexing, if feature is not present")
		;

	bpo::options_description cmdline_options;
	cmdline_options.add(generic);

	try {
		bpo::variables_map vm;
		bpo::store(bpo::command_line_parser(argc, argv).options(cmdline_options).run(), vm);

		if (vm.count("help")) {
			std::cout << generic << std::endl;
			return 0;
		}

		bpo::notify(vm);
	} catch (const std::exception &e) {
		std::cerr << "Invalid options: " << e.what() << "\n" << generic << std::endl;
		return -1;
	}

	ribosome::error_info err;

	warp::dictionary::database db;
	err = db.open(rocksdb_path, false);
	if (err) {
		std::cerr << "Could not open database: " << err.message() << std::endl;
		return err.code();
	}

	warp::stemmer stem;

	warp::zparser records([&] (struct warp::parsed_word &word) -> ribosome::error_info {
		ribosome::error_info err;
		if (word.features.empty())
			return err;

		warp::dictionary::word_form wf;
		wf.word = ribosome::lconvert::to_string(word.lemma);
		wf.lw = word.lemma;
		wf.freq = word.features.size();
		wf.documents = 1;
		wf.indexed_id = db.metadata().get_sequence();

		return warp::packer::write(db, wf);
	}, skip, pass);

	err = records.parse_file(input);
	if (err) {
		std::cerr << "Could not parse input file " << input << ": " << err.message() << std::endl;
		return err.code();
	}

	return 0;
}


