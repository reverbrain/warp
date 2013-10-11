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

		ioremap::trie::letter_layer<int> ll;

		lb::ssegment_index cmap(lb::character, tmp.begin(), tmp.end(), m_loc);
		for (auto it = cmap.begin(), e = cmap.end(); it != e; ++it) {
			ioremap::trie::letter<int> l(it->str(), -1);
			ll.push_back(l);
		}

		root.add(ll, idx);
		std::cout << "added word: " << tmp << ", data: " << idx << std::endl;
		idx++;
	}

	for (int i = 0; i < argc; ++i) {
		std::string tmp = argv[i];

		ioremap::trie::letter_layer<int> lookup;
		ioremap::trie::letter_layer<int> ll;

		lb::ssegment_index cmap(lb::character, tmp.begin(), tmp.end(), m_loc);
		for (auto it = cmap.begin(), e = cmap.end(); it != e; ++it) {
			ioremap::trie::letter<int> l(it->str(), -1);

			if (lookup.size() < 1)
				lookup.push_back(l);
			ll.push_back(l);
		}

		auto data = root.lookup(lookup, 0);
		for (auto d : data.first) {
			std::cout << "word: " << tmp << ", pos: " << data.second << ", exact: " << (data.second + 1 == (int)ll.size()) << ", data: " << d << std::endl;
		}
	}
}

int main(int argc, char *argv[])
{
	check_layers(argc, argv);
	check_node(argc, argv);

	return 0;
}
