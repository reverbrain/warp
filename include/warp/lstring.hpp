/*
 * Copyright 2014+ Evgeniy Polyakov <zbr@ioremap.net>
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

#ifndef __WARP_LSTRING_HPP
#define __WARP_LSTRING_HPP

#include <fstream>
#include <sstream>
#include <string>

#include <boost/locale.hpp>
#include <boost/locale/util.hpp>

namespace ioremap { namespace warp {

static const boost::locale::generator __fuzzy_locale_generator;
static const std::locale __fuzzy_locale(__fuzzy_locale_generator("en_US.UTF8"));
static const auto __fuzzy_utf8_converter = boost::locale::util::create_utf8_converter();

template <typename T>
struct letter {
	T l;

	letter() : l(0) {}
	letter(const T &_l) : l(_l) {}
	letter(const letter &other) {
		l = other.l;
	}

	std::string str() const {
		char tmp[8];
		memset(tmp, 0, sizeof(tmp));
		__fuzzy_utf8_converter->from_unicode(l, tmp, tmp + 8);

		return tmp;
	}

	bool operator==(const letter &other) const {
		return l == other.l;
	}
};

inline std::ostream &operator <<(std::ostream &out, const letter<unsigned int> &l)
{
	out << l.str();
	return out;
}

template <typename T>
struct letter_traits {
	typedef letter<T> char_type;
	typedef letter<T> int_type;
	typedef std::streampos pos_type;
	typedef std::streamoff off_type;
	typedef std::mbstate_t state_type;

	static void assign(char_type &c1, const char_type &c2) {
		c1 = c2;
	}

	static bool eq(const char_type &c1, const char_type &c2) {
		return c1.l == c2.l;
	}

	static bool lt(const char_type &c1, const char_type &c2) {
		return c1.l < c2.l;
	}

	static int compare(const char_type *s1, const char_type *s2, std::size_t n) {
		for (std::size_t i = 0; i < n; ++i) {
			if (eq(s1[i], char_type())) {
				if (eq(s2[i], char_type())) {
					return 0;
				}

				return -1;
			}

			if (lt(s1[i], s2[i]))
				return -1;
			else if (lt(s2[i], s1[i]))
				return 1;
		}

		return 0;
	}

	static std::size_t length(const char_type* s) {
		std::size_t i = 0;

		while (!eq(s[i], char_type()))
			++i;

		return i;
	}

	static const char_type *find(const char_type *s, std::size_t n, const char_type& a) {
		for (std::size_t i = 0; i < n; ++i)
			if (eq(s[i], a))
				return s + i;
		return 0;
	}

	static char_type *move(char_type *s1, const char_type *s2, std::size_t n) {
		return static_cast<char_type *>(memmove(s1, s2, n * sizeof(char_type)));
	}

	static char_type *copy(char_type *s1, const char_type *s2, std::size_t n) {
		std::copy(s2, s2 + n, s1);
		return s1;
	}

	static char_type *assign(char_type *s, std::size_t n, char_type a) {
		std::fill_n(s, n, a);
		return s;
	}

	static char_type to_char_type(const int_type &c) {
		return static_cast<char_type>(c);
	}

	static int_type to_int_type(const char_type &c) {
		return static_cast<int_type>(c);
	}

	static bool eq_int_type(const int_type &c1, const int_type &c2) {
		return c1.l == c2.l;
	}

	static int_type eof() {
		return static_cast<int_type>(~0U);
	}

	static int_type not_eof(const int_type &c) {
		return !eq_int_type(c, eof()) ? c : to_int_type(char_type());
	}
};

typedef std::basic_string<letter<unsigned int>, letter_traits<unsigned int>> lstring;

inline std::ostream &operator <<(std::ostream &out, const lstring &ls)
{
	for (auto it = ls.begin(); it != ls.end(); ++it) {
		out << *it;
	}
	return out;
}


class lconvert {
	public:
		static lstring from_utf8(const char *text, size_t size) {
			namespace lb = boost::locale::boundary;
			std::string::const_iterator begin(text);
			std::string::const_iterator end(text + size);

			lb::ssegment_index wmap(lb::character, begin, end, __fuzzy_locale);
			wmap.rule(lb::character_any);

			lstring ret;

			for (auto it = wmap.begin(), e = wmap.end(); it != e; ++it) {
				std::string str = it->str();
				const char *ptr = str.c_str();
				auto code = __fuzzy_utf8_converter->to_unicode(ptr, ptr + str.size());

				letter<unsigned int> l(code);
				ret.append(&l, 1);
			}

			return ret;
		}

		static lstring from_utf8(const std::string &text) {
			return from_utf8(text.c_str(), text.size());
		}

		static std::string to_string(const lstring &l) {
			std::ostringstream ss;
			ss << l;
			return ss.str();
		}
};

}} // ioremap::warp

#endif /* __WARP_LSTRING_HPP */
