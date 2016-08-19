#pragma once

#include <map>
#include <string>
#include <unordered_map>

#include <ribosome/lstring.hpp>

namespace ioremap { namespace warp {

static const std::string drop_characters = "`~1234567890-=!@#$%^&*()_+[]\\{}|';\":/.,?><\n\r\t";

class alphabet {
public:
	alphabet(const std::string &a) {
		ribosome::lstring lw = ribosome::lconvert::from_utf8(a);
		for (auto ch: lw) {
			m_alphabet[ch] = 1;
		}
	}

	bool ok(const ribosome::lstring &lw) const {
		if (m_alphabet.empty())
			return true;

		for (auto ch: lw) {
			auto it = m_alphabet.find(ch);
			if (it == m_alphabet.end()) {
				return false;
			}
		}

		return true;
	}

private:
	std::unordered_map<ribosome::letter, int, ribosome::letter_hash> m_alphabet;
};

class alphabets_checker {
public:
	void add(const std::string &lang, const std::string &a) {
		m_alphabets.emplace(std::pair<std::string, alphabet>(lang, alphabet(a)));
	}

	bool ok(const std::string &lang, const ribosome::lstring &lw) {
		auto it = m_alphabets.find(lang);
		if (it == m_alphabets.end())
			return true;

		return it->second.ok(lw);
	}
private:
	std::map<std::string, alphabet> m_alphabets;
};


}} // namespace ioremap::warp
