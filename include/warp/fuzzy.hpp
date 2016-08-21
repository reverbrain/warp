#ifndef __FUZZY_FUZZY_HPP
#define __FUZZY_FUZZY_HPP

#include "warp/database.hpp"
#include "warp/ngram.hpp"
#include "warp/norvig.hpp"
#include "warp/substring.hpp"

#include <ribosome/distance.hpp>
#include <ribosome/error.hpp>
#include <ribosome/lstring.hpp>
#include <ribosome/timer.hpp>

#include <msgpack.hpp>

namespace ioremap { namespace warp {

struct check_control {
	std::string word;
	ribosome::lstring lw;
	int max_num = 10;

	enum {
		level_0 = 0,
		level_1,
		level_2,
		level_3,
	};

	int level = level_3;
};


class checker {
public:
	ribosome::error_info open(const std::string &path) {
		return m_db.open_read_only(path);
	}

	ribosome::error_info load_error_models(const std::string &replace_path, const std::string &around_path) {
		ribosome::error_info err;

		if (around_path.size()) {
			err = m_model.load_error_model_around(around_path);
			if (err)
				return err;
		}

		if (replace_path.size()) {
			err = m_model.load_error_model_replace(replace_path);
			if (err)
				return err;
		}

		return err;
	}

	ribosome::error_info check(const check_control &ctl, std::vector<dictionary::word_form> *ret) {
		ribosome::error_info err;
		if (ctl.word.empty())
			return err;

		dictionary::word_form wf;
		read_word(ctl.word, &wf);
		if (wf.word.size()) {
			ret->push_back(wf);
			return err;
		}
		if (ctl.level == check_control::level_0) {
			return err;
		}

		read_transform(ctl.word, &wf);
		if (wf.word.size()) {
			ret->push_back(wf);
			return err;
		}

		if (ctl.level == check_control::level_1) {
			return err;
		}

		std::vector<dictionary::word_form> tmp;
		err = norvig_check(ctl.word, ctl.lw, &tmp);
		if (err) {
			return err;
		}

		if (tmp.empty() && (ctl.level >= check_control::level_3)) {
			err = ngram_check(ctl.word, ctl.lw, &tmp);
			if (err) {
				return err;
			}
		}

		*ret = sort(ctl.lw, tmp, ctl.max_num);
		return err;
	}

	ribosome::error_info check(const std::string &word, std::vector<dictionary::word_form> *ret) {
		struct check_control ctl;

		ctl.word = word;
		ctl.lw = ribosome::lconvert::from_utf8(word);
		ctl.lw = ribosome::lconvert::to_lower(ctl.lw);

		return check(ctl, ret);
	}

	ribosome::error_info check(const std::string &word, const ribosome::lstring &lw, std::vector<dictionary::word_form> *ret) {
		struct check_control ctl;

		ctl.word = word;
		ctl.lw = lw;

		return check(ctl, ret);
	}

private:
	dictionary::database m_db;
	norvig::lang_model m_model;
	int m_ngram = 2;

	ribosome::error_info read_word(const std::string &word, dictionary::word_form *wf) {
		std::string key = m_db.options().word_form_prefix + word;
		auto err = m_db.read(key, wf);
		if (err) {
			return err;
		}

		wf->lw = ribosome::lconvert::from_utf8(wf->word);
		return ribosome::error_info();
	}

	ribosome::error_info read_transform(const std::string &word, dictionary::word_form *wf) {
		std::string key = m_db.options().transform_prefix + word;
		auto err = m_db.read(key, wf);
		if (err) {
			return err;
		}

		wf->lw = ribosome::lconvert::from_utf8(wf->word);
		return ribosome::error_info();
	}

	ribosome::error_info read_word_by_id(const dictionary::document_for_index &did, dictionary::word_form *wf) {
		std::string wkey = m_db.options().word_form_indexed_prefix + std::to_string(did.indexed_id);
		std::string word_serialized;
		auto err = m_db.read(wkey, &word_serialized);
		if (err) {
			return ribosome::create_error(err.code(),
				"could not read word form: indexed_id: %ld, error: %s",
					did.indexed_id, err.message().c_str());
		}

		err = deserialize(*wf, word_serialized.data(), word_serialized.size());
		if (err) {
			return ribosome::create_error(-ENOENT,
				"could not deserialize word form: indexed_id: %ld, error: %s",
					did.indexed_id, err.message().c_str());
		}

		wf->lw = ribosome::lconvert::from_utf8(wf->word);

		return ribosome::error_info();
	}

	ribosome::error_info read_ids(const std::vector<uint64_t> &idc, std::set<dictionary::word_form> *ret) {
		dictionary::document_for_index did;
		dictionary::word_form wf;

		for (auto id: idc) {
			did.indexed_id = id;
			auto err = read_word_by_id(did, &wf);
			if (err) {
				return err;
			}

			ret->insert(wf);
		}

		return ribosome::error_info();
	}

	ribosome::error_info norvig_check(const std::string &word, const ribosome::lstring &lw, std::vector<dictionary::word_form> *ret) {
		(void) word;

		std::set<dictionary::word_form> wfs;
		ribosome::error_info err;
		dictionary::word_form wf;

		std::set<ribosome::lstring> e1 = m_model.edits1(lw);
		for (const auto &w1: e1) {
			std::string w = ribosome::lconvert::to_string(w1);
			err = read_word(w, &wf);
			if (wf.word.size()) {
				wf.edit_distance = 1;
				wfs.insert(wf);
			}
		}

		for (const auto &w1: e1) {
			for (const auto &w2: m_model.edits1(w1)) {
				std::string w = ribosome::lconvert::to_string(w2);
				err = read_word(w, &wf);
				if (wf.word.size()) {
					wf.edit_distance = 2;
					wfs.insert(wf);
				}
			}
		}

		ret->insert(ret->end(), wfs.begin(), wfs.end());
		return ribosome::error_info();
	}

	ribosome::error_info ngram_check(const std::string &word, const ribosome::lstring &lw, std::vector<dictionary::word_form> *ret) {
		auto ngrams = ngram<ribosome::lstring>::split(lw, m_ngram);

		std::map<uint64_t, size_t> idc;

		for (auto &n: ngrams) {
			std::string ns = ribosome::lconvert::to_string(n);
			std::string key = m_db.options().ngram_prefix + ns;

			std::string nlist;
			auto err = m_db.read(key, &nlist);
			if (err) {
				continue;
			}

			dictionary::disk_index did;
			err = deserialize(did, nlist.data(), nlist.size());
			if (err) {
				return ribosome::create_error(err.code(),
					"could not deserialize index key: word: %s, ngram: %s, key: %s, error: %s",
						word.c_str(), ns.c_str(), key.c_str(), err.message().c_str());
			}

			for (auto &i: did.ids) {
				auto it = idc.find(i.indexed_id);
				if (it == idc.end()) {
					idc[i.indexed_id] = 1;
				} else {
					it->second++;
				}
			}
		}

		std::vector<uint64_t> good_ids;
		good_ids.reserve(idc.size());

		for (auto &p: idc) {
			if (p.second > 2 && lw.size() > 4)
				good_ids.push_back(p.first);
		}

		std::set<dictionary::word_form> wfs;
		auto err = read_ids(good_ids, &wfs);
		if (err)
			return err;

		ret->insert(ret->end(), wfs.begin(), wfs.end());
		return ribosome::error_info();
	}

	std::vector<dictionary::word_form> sort(const ribosome::lstring &lw, const std::vector<dictionary::word_form> &words, int max_num) {
		std::vector<dictionary::word_form> ret;
		ret.reserve(words.size());

		long sum_freq = 0;
		int min_dist = lw.size() / 2;
		for (auto &wf: words) {
			int edit_distance = ribosome::distance::levenstein(lw, wf.lw, min_dist);
			if (edit_distance < 0) {
				continue;
			}
			if (edit_distance < min_dist) {
				min_dist = edit_distance;
			}

			sum_freq += wf.freq;

			dictionary::word_form tmp = wf;
			tmp.edit_distance = edit_distance;
			ret.emplace_back(tmp);
		}

		for (auto &wf: ret) {
			float f = (float)wf.freq / (float)sum_freq;
			float d = (float)wf.edit_distance / (float)wf.lw.size();
			ribosome::lstring sub = longest_substring(lw, wf.lw);
			size_t diff = lw.size() - sub.size();
			if (diff == 0) {
				wf.freq_norm = f / d;
			} else {
				wf.freq_norm = f / d / (diff * 10.0);
			}
		}

		std::sort(ret.begin(), ret.end(), [&] (const dictionary::word_form &wf1, const dictionary::word_form &wf2) {
					return wf1.freq_norm >= wf2.freq_norm;
				});

		if ((int)ret.size() > max_num)
			ret.resize(max_num);
		return ret;
	}
};

}} // namespace ioremap::warp

#endif // __FUZZY_FUZZY_HPP
