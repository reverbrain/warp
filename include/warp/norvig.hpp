#ifndef __FUZZY_NORVIG_HPP
#define __FUZZY_NORVIG_HPP

#include "warp/letter_error_model.hpp"

#include <ribosome/lstring.hpp>

#include <fstream>
#include <map>

namespace ioremap { namespace warp { namespace norvig {

template <class T>
class nt_iterator: public std::iterator<std::input_iterator_tag, T>
{
public:
	typedef typename nt_iterator<T>::pointer pointer;
	typedef typename nt_iterator<T>::value_type value_type;

	nt_iterator() {}
	nt_iterator(pointer p): m_ptr(p), m_finished(p == NULL) {}
	nt_iterator(pointer p, size_t size): m_ptr(p), m_size(size), m_finished(p == NULL) {}
	nt_iterator(const nt_iterator<T>& rhs): m_ptr(rhs.m_ptr), m_finished(rhs.m_finished) {}

	nt_iterator<T>& operator++() {
		++m_ptr;
		--m_size;
		if (*m_ptr == NULL || m_size == 0)
			m_finished = true;

		return *this;
	}

	nt_iterator<T> operator++(int) {
		nt_iterator n(*this);
		operator++();
		return n;
	}

	bool operator==(const nt_iterator<T>& rhs) {
		if (m_finished && rhs.m_finished)
			return true;

		if (!m_finished && !rhs.m_finished)
			return m_ptr == rhs.m_ptr;

		return false;
	}

	bool operator!=(const nt_iterator<T>& rhs) {
		return !(operator==(rhs));
	}

	value_type operator*() {
		return *m_ptr;
	}

private:
	pointer m_ptr;
	size_t m_size = LONG_MAX;
	bool m_finished = true;
};

class lang_model {
public:
	lang_model() {}

	lang_model(lang_model &&other) : m_emod(std::move(other.m_emod)) {}

	ribosome::error_info load_error_model_around(const std::string &path) {
		return m_emod.load_transform_around(path);
	}
	ribosome::error_info load_error_model_replace(const std::string &path) {
		return m_emod.load_transform_replace(path);
	}

	std::set<ribosome::lstring> edits1(const ribosome::lstring &vs) {
		std::set<ribosome::lstring> ret;

		std::vector<std::pair<ribosome::lstring, ribosome::lstring>> splits;
		splits.reserve(vs.size());
		for (size_t i = 0; i <= vs.size(); ++i) {
			ribosome::lstring l(vs.begin(), vs.begin() + i);
			ribosome::lstring r(vs.begin() + i, vs.end());
			splits.emplace_back(std::make_pair(std::move(l), std::move(r)));
		}

		// deletes
		for (auto &p: splits) {
			if (p.second.size() > 0) {
				ribosome::lstring tmp;
				tmp.reserve(p.first.size() + p.second.size() - 1);
				const ribosome::lstring &a = p.first;
				const ribosome::lstring &b = p.second;
				tmp.insert(tmp.begin(), a.begin(), a.end());
				tmp.insert(tmp.end(), b.begin() + 1, b.end());
				ret.emplace(tmp);
			}
		}

		// transposes
		for (auto &p: splits) {
			if (p.second.size() > 1) {
				ribosome::lstring tmp;
				tmp.reserve(p.first.size() + p.second.size());
				const ribosome::lstring &a = p.first;
				const ribosome::lstring &b = p.second;
				tmp.insert(tmp.begin(), a.begin(), a.end());
				tmp.push_back(b[1]);
				tmp.push_back(b[0]);
				tmp.insert(tmp.end(), b.begin() + 2, b.end());
				ret.emplace(tmp);
			}
		}

		// replaces
		for (auto &p: splits) {
			if (p.second.size() > 0 && p.first.size() > 0) {

				const ribosome::lstring &a = p.first;
				const ribosome::lstring &b = p.second;

				for (auto &l: m_emod.transform(b.front(), a.size())) {
					ribosome::lstring tmp;
					tmp.reserve(a.size() + b.size());
					tmp.insert(tmp.begin(), a.begin(), a.end());
					tmp.push_back(l);
					tmp.insert(tmp.end(), b.begin() + 1, b.end());
					ret.emplace(tmp);
				}
			}
		}

		// inserts
		for (auto &p: splits) {
			if (p.first.size() > 0) {
				const ribosome::lstring &a = p.first;
				const ribosome::lstring &b = p.second;

				for (auto &l: m_emod.transform(a.back(), a.size())) {
					ribosome::lstring tmp;
					tmp.reserve(a.size() + b.size() + 1);
					tmp.insert(tmp.begin(), a.begin(), a.end());
					tmp.push_back(l);
					tmp.insert(tmp.end(), b.begin(), b.end());
					ret.emplace(tmp);
				}
			}
		}

		return ret;
	}

	std::set<ribosome::lstring> edits2(const ribosome::lstring &vs) {
		std::set<ribosome::lstring> ret;

		std::set<ribosome::lstring> e1 = edits1(vs);

		for (auto &w: e1) {
			std::set<ribosome::lstring> tmp = edits1(w);
			ret.insert(tmp.begin(), tmp.end());
		}

		return ret;
	}

private:
	warp::error_model::letter_error_model m_emod;
};

}}} // namespace ioremap::fuzzy::norvig

#endif // __FUZZY_NORVIG_HPP
