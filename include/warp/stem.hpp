#pragma once

#include "libstemmer.h"

#include <ribosome/error.hpp>

#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace ioremap { namespace warp {
class raw_stemmer {
public:
	raw_stemmer(const char *lang, const char *enc) {
		m_stem = sb_stemmer_new(lang, enc);
		if (!m_stem) {
			const char *sname = "eng";
			m_stem = sb_stemmer_new(sname, enc);
			if (!m_stem)
				ribosome::throw_error(-ENOMEM, "could not create stemmer '%s'", sname);
		}
	}

	~raw_stemmer() {
		sb_stemmer_delete(m_stem);
	}

	std::string convert(const char *word, size_t size) {
		const sb_symbol *sb;
		std::string ret;

		int len, prev_len;
		len = prev_len = size;

		std::lock_guard<std::mutex> guard(m_lock);

		do {
			sb = sb_stemmer_stem(m_stem, (const sb_symbol *)word, len);
			if (!sb)
				return ret;

			len = sb_stemmer_length(m_stem);
			if (len == prev_len)
				break;

			prev_len = len;
		} while (len > 0);

		ret.assign((char *)sb, len);
		return ret;
	}

private:
	std::mutex m_lock;
	struct sb_stemmer *m_stem;
};

typedef std::shared_ptr<raw_stemmer> stemmer_t;

class stemmer {
public:
	std::string stem(const std::string &word, const std::string &lang, const std::string &enc) {
		std::unique_lock<std::mutex> guard(m_lock);
		auto it = m_stemmers.find(lang);
		if (it == m_stemmers.end()) {
			const char *cenc = NULL;
			if (enc.size())
				cenc = enc.c_str();

			stemmer_t st = std::make_shared<raw_stemmer>(lang.c_str(), cenc);
			auto p = m_stemmers.emplace(std::pair<std::string, stemmer_t>(lang, st));
			it = p.first;
		}

		guard.unlock();
		return it->second->convert(word.data(), word.size());
	}
private:
	std::mutex m_lock;
	std::map<std::string, stemmer_t> m_stemmers;
};

}} // ioremap::warp
