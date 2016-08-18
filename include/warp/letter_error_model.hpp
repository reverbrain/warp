#ifndef __FUZZY_LETTER_ERROR_MODEL_HPP
#define __FUZZY_LETTER_ERROR_MODEL_HPP

#include <ribosome/error.hpp>
#include <ribosome/lstring.hpp>

#include <fstream>
#include <map>
#include <set>
#include <string>

namespace ioremap { namespace warp { namespace error_model {

class letter_error_model {
public:
	letter_error_model() {}
	letter_error_model(letter_error_model &&emod) {
		m_around.swap(emod.m_around);
		m_replace.swap(emod.m_replace);
	}

	ribosome::error_info load_transform_around(const std::string &path) {
		m_around = load_map(path);
		if (m_around.empty()) {
			return ribosome::create_error(-errno, "could not load around map from file %s", path.c_str());
		}
		return ribosome::error_info();
	}

	ribosome::error_info load_transform_replace(const std::string &path) {
		m_replace = load_map(path);
		if (m_replace.empty()) {
			return ribosome::create_error(-errno, "could not load replace map from file %s", path.c_str());
		}
		return ribosome::error_info();
	}

	ribosome::lstring transform(const ribosome::letter &src, int pos) const {
		std::set<ribosome::letter> ret;
		ret.insert(src);

		auto it = m_replace.find(src);
		if (it != m_replace.end()) {
			ret.insert(it->second.begin(), it->second.end());
		}

		if (pos != 0) {
			it = m_around.find(src);
			if (it != m_around.end()) {
				ret.insert(it->second.begin(), it->second.end());
			}
#if 0
			std::cout << "transform: " <<
				"pos: " << pos <<
				", letter: " << src.str() <<
				" -> " << ribosome::lconvert::to_string(ribosome::lstring(ret.begin(), ret.end())) <<
				std::endl;
#endif
		}

		return ribosome::lstring(ret.begin(), ret.end());
	}

private:
	typedef std::map<ribosome::letter, ribosome::lstring> tmap;

	tmap m_around, m_replace;

	tmap load_map(const std::string &path) {
		std::ifstream in(path);

		tmap ret;
		std::string line;
		while (std::getline(in, line)) {
			size_t pos = line.find(' ');
			if (pos != std::string::npos) {
				ribosome::letter f = s2l(line.c_str(), pos);
				ret[f] = ribosome::lconvert::from_utf8(line.substr(pos + 1));
			}
		}

		return ret;
	}

	ribosome::letter s2l(const char *ptr, size_t size)
	{
		ribosome::lstring s = ribosome::lconvert::from_utf8(ptr, size);
		return s[0];
	}
};

}}} // namespace ioremap::warp::error_model


#endif // __FUZZY_LETTER_ERROR_MODEL_HPP
