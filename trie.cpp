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

#include <boost/locale.hpp>

#include "trie.hpp"

namespace lb = boost::locale::boundary;

static void check_layers(int argc, char *argv[])
{
	ioremap::trie::layer<ioremap::trie::letter<int>> layer;
	int idx = 0;

	std::locale m_loc;
	boost::locale::generator gen;
	m_loc = gen("en_US.UTF8");

	for (int i = 0; i < argc; ++i) {
		std::string tmp = argv[i];

		lb::ssegment_index cmap(lb::character, tmp.begin(), tmp.end(), m_loc);

		if (i % 2 == 0) {
			for (auto it = cmap.begin(), e = cmap.end(); it != e; ++it) {
				ioremap::trie::letter<int> l(it->str(), idx);
				layer.insert(l);

				++idx;
			}
		}

		for (auto it = cmap.begin(), e = cmap.end(); it != e; ++it) {
			auto ans = layer.find(it->str());
			if (ans == NULL) {
				std::cerr << "Cound not find letter '" << it->str() << "' in '" << tmp << "'" << std::endl;
				continue;
			}

			std::cout << "letter: " << it->str() << ", found: " << ans->string() << ", data: " << ans->data() << std::endl;
		}
	}

	layer.dump();

	ioremap::trie::layer<ioremap::trie::letter<int>> tmp = layer.split("r");

	layer.dump();
	tmp.dump();
}

static void check_node(int argc, char *argv[])
{
	ioremap::trie::node<int> root;
	int idx = 0;

	std::locale m_loc;
	boost::locale::generator gen;
	m_loc = gen("en_US.UTF8");

	for (int i = 0; i < argc; ++i) {
		std::string tmp = argv[i];

		ioremap::trie::letters ll;

		lb::ssegment_index cmap(lb::character, tmp.begin(), tmp.end(), m_loc);
		for (auto it = cmap.begin(), e = cmap.end(); it != e; ++it) {
			ll.push_back(it->str());
		}

		root.add(ll, idx);
		std::cout << "added word: " << tmp << ", data: " << idx << std::endl;
		idx++;
	}

	for (int i = 0; i < argc; ++i) {
		std::string tmp = argv[i];

		ioremap::trie::letters lookup;

		std::string word;
		lb::ssegment_index cmap(lb::character, tmp.begin(), tmp.end(), m_loc);
		for (auto it = cmap.begin(), e = cmap.end(); it != e; ++it) {
			if (lookup.size() < 3) {
				lookup.push_back(it->str());
				word += it->str();
			}
		}

		auto data = root.lookup(lookup);
		for (auto d : data) {
			std::cout << "word: " << word << "/" << tmp << ", data: " << d << std::endl;
		}
	}
}

int main(int argc, char *argv[])
{
	check_layers(argc, argv);
	check_node(argc, argv);

	return 0;
}
