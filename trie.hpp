#ifndef __IOREMAP_TRIE_HPP
#define __IOREMAP_TRIE_HPP

#include <algorithm>
#include <vector>

namespace ioremap { namespace trie {

template <typename C>
class letter {
	public:
		letter(const std::string &l, const C &c) : m_letter(l), m_container(c) {}
		letter(const C &c) : m_container(c) {}
		letter() {}

		const std::string &string() const {
			return m_letter;
		}

		C &data(void) {
			return m_container;
		}

		const C &data(void) const {
			return m_container;
		}

		bool empty(void) const {
			return m_letter.size() == 0;
		}

		bool operator== (const letter &l) {
			return (l.string() == m_letter) && (l.data() == m_container);
		}

		bool operator() (const letter &a, const letter &b) {
			return a.string() < b.string();
		}

		bool operator< (const std::string &str) const {
			return m_letter < str;
		}

	private:
		std::string m_letter;
		C m_container;
};

template <typename L>
class layer {
	public:
		layer() {}
		layer(const std::vector<L> &l) : m_layer(l) {}

		void insert(const L &l) {
			m_layer.push_back(l);
			std::sort(m_layer.begin(), m_layer.end(), l);
		}

		void push_back(const L &l) {
			m_layer.push_back(l);
		}

		void emplace_back(const L &&l) {
			m_layer.emplace_back(l);
		}

		void reverse(void) {
			std::reverse(m_layer.begin(), m_layer.end());
		}

		L *find(const std::string &str) {
			auto it = std::lower_bound(m_layer.begin(), m_layer.end(), str);
			if (it == m_layer.end())
				return NULL;
			if (it->string() != str)
				return NULL;

			return &(*it);
		}

		layer<L> split(const std::string &str) {
			auto it = std::lower_bound(m_layer.begin(), m_layer.end(), str);
			if (it == m_layer.end())
				return layer<L>();
			if (it->string() != str)
				return layer<L>();

			std::vector<L> tmp;
			size_t rest = it - m_layer.begin();
			tmp.reserve(m_layer.size() - rest);

			for (; it != m_layer.end(); ++it)
				tmp.push_back(*it);

			layer<L> ret(tmp);
			m_layer.resize(rest);

			return ret;
		}

		L &operator [](size_t index) {
			return m_layer.at(index);
		}

		size_t size(void) const {
			return m_layer.size();
		}

		void dump(void) {
			for (auto & l : m_layer) {
				std::cout << l.string();
			}
			std::cout << std::endl;
		}

	private:
		std::vector<L> m_layer;
};

template<class X> using letter_layer = layer<letter<X>>;
typedef std::vector<std::string> letters;

template <typename D>
class node {
	public:
		node() {}
		node(const D &d) {
			m_data.push_back(d);
		}

		void add(const letters &ll, const D &d) {
			raw_add(ll, 0, d);
		}

		std::vector<D> lookup(const letters &ll) {
			std::set<D> tmp = raw_lookup(ll, 0);
			std::vector<D> ret;
			ret.reserve(tmp.size());

			for (auto & t : tmp)
				ret.push_back(t);

			return ret;
		}

	private:
		std::set<D> m_data;
		letter_layer<node<D>> m_children;

		void append_data(const D &d) {
			m_data.insert(d);
		}


		std::set<D> &data(void) {
			return m_data;
		}

		void add_and_data_append(const letters &ll, int pos, const D &d, node<D> &n) {
			// put data not only at the end of the word (last node in trie),
			// but into every node on every level

			if (pos + 1 == ll.size()) {
				n.append_data(d);
			} else {
				n.raw_add(ll, pos + 1, d);
			}
		}

		void raw_add(const letters &ll, int pos, const D &d) {
			const auto & l = ll[pos];

			auto el = m_children.find(l);
			if (el == NULL) {
				node<D> n;

				add_and_data_append(ll, pos, d, n);

				letter<node<D>> tmp(l, n);
				m_children.insert(tmp);

				return;
			}

			add_and_data_append(ll, pos, d, el->data());
		}

		std::set<D> rest(bool self) {
			std::set<D> ret;

			if (self)
				ret = m_data;

			for (int i = 0; i < m_children.size(); ++i) {
				for (auto & t : m_children[i].data().rest(true))
					ret.insert(t);
			}

			return ret;
		}

		std::set<D> raw_lookup(const letters &ll, int pos) {
			const auto & l = ll[pos];
			auto el = m_children.find(l);
			if (el == NULL) {
				if (pos <= 1)
					return std::set<D>();

				return rest(false);
			}

			if (pos + 1 == ll.size()) {
				auto tmp = el->data().data();
				if (tmp.size() == 0)
					return el->data().rest(true);

				return tmp;
			}

			return el->data().raw_lookup(ll, pos + 1);
		}
};

}} // namespace ioremap::trie

#endif /* __IOREMAP_TRIE_HPP */
