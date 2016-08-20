#pragma once

#include "warp/fuzzy.hpp"

namespace ioremap { namespace warp {

struct language_model {
	std::string language;

	struct error_model {
		std::string replace_path;
		std::string around_path;
	} error;

	std::string lang_model_path;
};

class language_checker {
public:
	ribosome::error_info load_language_model(const language_model &m) {
		std::unique_ptr<warp::checker> ch(new warp::checker());

		auto err = ch->open(m.lang_model_path.c_str());
		if (err) {
			return ribosome::create_error(err.code(), "could not open lang model path: %s, error: %s",
					m.lang_model_path.c_str(), err.message().c_str());
		}

		err = ch->load_error_models(m.error.replace_path, m.error.around_path);
		if (err) {
			return ribosome::create_error(err.code(), "could not load error models, replace: %s, around: %s: error: %s",
					m.error.replace_path.c_str(), m.error.around_path.c_str(), err.message().c_str());
		}

		std::lock_guard<std::mutex> guard(m_lock);
		m_checkers.emplace(std::pair<std::string, std::unique_ptr<warp::checker>>(m.language, std::move(ch)));

		return ribosome::error_info();
	}

	ribosome::error_info load_langdetect_stats(const std::string &path) {
		int err = m_det.load_file(path.c_str());
		if (err) {
			return ribosome::create_error(err, "could not load language detector stats from file %s", path.c_str());
		}

		m_language_stats_path = path;
		return ribosome::error_info();
	}

	ribosome::error_info check(const check_control &ctl, std::vector<dictionary::word_form> *ret) {
		std::string lang;
		if (ctl.lw.size() && ctl.word.size()) {
			lang = std::move(language(ctl.word, ctl.lw));
		} else if (ctl.lw.size()) {
			lang = std::move(language(ctl.lw));
		} else if (ctl.word.size()) {
			lang = std::move(language(ctl.word));
		} else {
			return ribosome::create_error(-ENOENT, "no word has been provided");
		}

		return check(lang, ctl, ret);
	}

	ribosome::error_info check(const std::string &lang, const check_control &ctl, std::vector<dictionary::word_form> *ret) {
		std::unique_lock<std::mutex> guard(m_stats_lock);
		auto ch = m_checkers.find(lang);
		if (ch == m_checkers.end()) {
			return ribosome::create_error(-ENOENT, "there is no language detector for lang '%s', word: '%s'",
					lang.c_str(), ctl.word.c_str());
		}
		guard.unlock();

		return ch->second->check(ctl, ret);
	}

	ribosome::error_info detector_save(const std::string &text, const std::string &lang) {
		m_det.load_text(text, lang);

		std::lock_guard<std::mutex> guard(m_stats_lock);

		m_det.sort();

		int err = m_det.save_file(m_language_stats_path.c_str());
		if (err) {
			return ribosome::create_error(err, "could not save language detector stats to file %s",
					m_language_stats_path.c_str());
		}

		return ribosome::error_info();
	}

	std::string language(const std::string &word) {
		check_control ctl;
		ctl.word = word;

		return language(ctl);
	}

	std::string language(const ribosome::lstring &lw) {
		check_control ctl;
		ctl.word = ribosome::lconvert::to_string(lw);
		ctl.lw = lw;

		return language(ctl);
	}

	std::string language(const std::string &word, const ribosome::lstring &lw) {
		check_control ctl;
		ctl.word = word;
		ctl.lw = lw;

		return language(ctl);
	}


private:
	std::mutex m_lock;
	std::map<std::string, std::unique_ptr<warp::checker>> m_checkers;

	std::mutex m_stats_lock;
	std::string m_language_stats_path;
	detector<std::string, std::string> m_det;

	std::string language(check_control &ctl) {
		ctl.level = check_control::level_0;

		std::vector<dictionary::word_form> tmp;

		std::unique_lock<std::mutex> guard(m_stats_lock);
		for (const auto &p: m_checkers) {
			const auto &ch = p.second;
			const auto &lang = p.first;

			auto err = ch->check(ctl, &tmp);
			if (!err) {
				return lang;
			}
		}
		guard.unlock();

		return m_det.detect(ctl.word);
	}
};

}} // namespace ioremap::warp
