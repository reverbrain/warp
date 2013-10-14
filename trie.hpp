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

		const L &operator [](size_t index) const {
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

template <typename D>
class node {
	public:
		node() {}
		node(const D &d) {
			m_data.push_back(d);
		}

		void add(const letter_layer<D> &ll, const D &d) {
			raw_add(ll, 0, d);
		}

		std::pair<std::vector<D>, int> lookup(const letter_layer<D> &ll) {
			return raw_lookup(ll, 0);
		}

	private:
		std::vector<D> m_data;
		letter_layer<node<D>> m_children;

		void append_data(const D &d) {
			m_data.push_back(d);
		}


		std::vector<D> &data(void) {
			return m_data;
		}

		void raw_add(const letter_layer<D> &ll, int pos, const D &d) {
			auto & l = ll[pos];

			auto el = m_children.find(l.string());
			if (el == NULL) {
				node<D> n;

				n.append_data(d);
				if (pos + 1 == ll.size()) {
				} else {
					n.raw_add(ll, pos + 1, d);
				}

				letter<node<D>> tmp(l.string(), n);
				m_children.insert(tmp);

				return;
			}

			el->data().append_data(d);
			if (pos + 1 < ll.size()) {
				el->data().raw_add(ll, pos + 1, d);
			} else {
			}
		}

		std::pair<std::vector<D>, int> raw_lookup(const letter_layer<D> &ll, int pos) {
			auto & l = ll[pos];
			auto el = m_children.find(l.string());
			if (el == NULL) {
				return std::make_pair(m_data, pos);
			}

			if (pos + 1 == ll.size())
				return std::make_pair(el->data().data(), pos + 1);

			return el->data().raw_lookup(ll, pos + 1);
		}

};

}} // namespace ioremap::trie

#endif /* __IOREMAP_TRIE_HPP */
