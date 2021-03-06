/*
 * Copyright 2013+ Evgeniy Polyakov <zbr@ioremap.net>
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

#include "warp/error_check.hpp"
#include "warp/json.hpp"
#include "warp/jsonvalue.hpp"
#include "warp/language_model.hpp"
#include "warp/stem.hpp"
#include "warp/thevoid_stream.hpp"

#include <swarm/logger.hpp>

#include <thevoid/server.hpp>

#include <ribosome/error.hpp>
#include <ribosome/html.hpp>
#include <ribosome/lstring.hpp>
#include <ribosome/split.hpp>

#include <boost/algorithm/string/trim_all.hpp>

#define WLOG(level, a...) BH_LOG(logger(), level, ##a)
#define WLOG_ERROR(a...) WLOG(SWARM_LOG_ERROR, ##a)
#define WLOG_WARNING(a...) WLOG(SWARM_LOG_WARNING, ##a)
#define WLOG_INFO(a...) WLOG(SWARM_LOG_INFO, ##a)
#define WLOG_NOTICE(a...) WLOG(SWARM_LOG_NOTICE, ##a)
#define WLOG_DEBUG(a...) WLOG(SWARM_LOG_DEBUG, ##a)

using namespace ioremap;

static std::string clear_symbols = "~`1234567890-=!@#$%^&*()_+[]\\{}|';\":/.,?><\n\r\t ";
static std::string clear_symbols_without_numbers = "~`-=!@#$%^&*()_+[]\\{}|';\":/.,?><\n\r\t ";

static std::string clear_text(const std::string &text)
{
	return boost::algorithm::trim_fill_copy_if(text, " ", boost::is_any_of(clear_symbols));
};

class http_server : public thevoid::server<http_server>
{
public:
	virtual bool initialize(const rapidjson::Value &config) {
		if (!lang_init(config)) {
			return false;
		}

		on<on_lang>(
			options::exact_match("/tokenize"),
			options::methods("POST")
		);
		on<on_lang>(
			options::exact_match("/convert"),
			options::methods("POST")
		);

		on<on_add_language>(
			options::prefix_match("/add_language/"),
			options::methods("POST")
		);

		on<warp::on_error_check<http_server>>(
			options::exact_match("/error_check"),
			options::methods("POST")
		);

		return true;
	}

	struct on_add_language : public thevoid::simple_request_stream_error<http_server> {
		virtual void on_request(const thevoid::http_request &http_req, const boost::asio::const_buffer &buffer) {
			const auto &pc = http_req.url().path_components();
			if (pc.size() != 2) {
				send_error(swarm::http_response::bad_request, -EINVAL,
						"there are %ld path components in %s, must be 2",
							pc.size(), http_req.url().path().c_str());
				return;
			}
			const std::string &lang = pc[1];

			const char *ptr = boost::asio::buffer_cast<const char*>(buffer);
			if (!ptr) {
				send_error(swarm::http_response::bad_request, -EINVAL, "document is empty");
				return;
			}
			size_t size = boost::asio::buffer_size(buffer);

			ribosome::html_parser html;
			html.feed_text(ptr, size);

			std::string nohtml_request = html.text(" ");
			auto lower_request = ribosome::lconvert::string_to_lower(nohtml_request);
			std::string clear_request = clear_text(lower_request);

			auto err = server()->detector_save(clear_request, lang);
			if (err) {
				send_error(swarm::http_response::internal_server_error, err.code(),
						"could not save statistics data: %s", err.message().c_str());
				return;
			}

			this->send_reply(swarm::http_response::ok);
		}
	};

	struct on_lang : public thevoid::simple_request_stream_error<http_server> {
		virtual void on_request(const thevoid::http_request &http_req, const boost::asio::const_buffer &buffer) {
			if (http_req.url().query().has_item("stem")) {
				m_want_stemming = true;
			}
			if (http_req.url().query().has_item("urls")) {
				m_want_urls = true;
			}
			if (http_req.url().path().find("/tokenize") == 0) {
				m_tokenize = true;
			}

			rapidjson::Document doc;
			const char *ptr = boost::asio::buffer_cast<const char*>(buffer);
			if (!ptr) {
				send_error(swarm::http_response::bad_request, -EINVAL, "document is empty");
				return;
			}

			std::string buf;
			buf.assign(ptr, boost::asio::buffer_size(buffer));

			doc.Parse<0>(buf.c_str());
			if (doc.HasParseError()) {
				send_error(swarm::http_response::bad_request, -EINVAL, "document parsing error: %s, offset: %ld",
						doc.GetParseError(), doc.GetErrorOffset());
				return;
			}

			warp::JsonValue reply;
			auto &alloc = reply.GetAllocator();

			const auto &req = warp::get_object(doc, "request");
			if (!req.IsObject()) {
				send_error(swarm::http_response::bad_request, -ENOENT, "'request' must be object");
				return;
			}

			for (auto member_it = req.MemberBegin(), member_end = req.MemberEnd();
					member_it != member_end; ++member_it) {
				if (!member_it->value.IsString()) {
					continue;
				}

				rapidjson::Value member(rapidjson::kObjectType);

				ribosome::html_parser html;
				html.feed_text(member_it->value.GetString());

				if (m_want_urls) {
					rapidjson::Value uarray(rapidjson::kArrayType);
					for (const auto &url: html.urls()) {
						rapidjson::Value uv(url.c_str(), url.size(), alloc);
						uarray.PushBack(uv, alloc);
					}

					member.AddMember("urls", uarray, alloc);
				}

				std::string nohtml_request = html.text(" ");
				ribosome::lstring lt = ribosome::lconvert::from_utf8(nohtml_request);
				auto lower_request = ribosome::lconvert::to_lower(lt);

				ribosome::split spl;
				auto all_words = spl.convert_split_words(lower_request, clear_symbols_without_numbers);

				if (m_tokenize) {
					tokenize(member, alloc, all_words);
				} else {
					convert(member, alloc, all_words);
				}

				// we can not use AddMember(member_it->name, member, alloc) here,
				// since member_it->name is const_iterator and for some weird reason
				// AddMember() only accepts reference to non-const object, although
				// it is not modified internally.
				reply.AddMember(member_it->name.GetString(), alloc, member, alloc);
			}

			std::string reply_data = reply.ToString();

			thevoid::http_response http_reply;
			http_reply.set_code(swarm::http_response::ok);
			http_reply.headers().set_content_length(reply_data.size());
			http_reply.headers().set_content_type("text/json");

			this->send_reply(std::move(http_reply), std::move(reply_data));
		}

	private:
		bool m_tokenize = false;
		bool m_want_stemming = false;
		bool m_want_urls = false;

		void tokenize(rapidjson::Value &member, rapidjson::Document::AllocatorType &alloc,
				const std::vector<ribosome::lstring> &all_words) {
			rapidjson::Value tokens(rapidjson::kArrayType);

			std::map<std::string, std::vector<size_t>> words;
			size_t pos = 0;
			for (auto &w: all_words) {
				auto word = ribosome::lconvert::to_string(w);
				auto it = words.find(word);
				if (it == words.end()) {
					words[word] = std::vector<size_t>({pos});
				} else {
					it->second.push_back(pos);
				}

				++pos;
			}

			for (auto &p: words) {
				const auto &word = p.first;
				const auto &positions = p.second;

				std::string lang = server()->language(word);

				rapidjson::Value lv(lang.c_str(), lang.size(), alloc);
				rapidjson::Value wv(word.c_str(), word.size(), alloc);

				rapidjson::Value tok(rapidjson::kObjectType);
				tok.AddMember("word", wv, alloc);
				tok.AddMember("language", lv, alloc);

				rapidjson::Value pv(rapidjson::kArrayType);
				for (auto pos: positions) {
					pv.PushBack(pos, alloc);
				}
				tok.AddMember("positions", pv, alloc);

				if (m_want_stemming) {
					std::string stemmed = server()->stemmer().stem(word, lang, "");
					rapidjson::Value sv(stemmed.c_str(), stemmed.size(), alloc);
					tok.AddMember("stem", sv, alloc);
				}

				tokens.PushBack(tok, alloc);
			}

			member.AddMember("tokens", tokens, alloc);
		}

		void convert(rapidjson::Value &member, rapidjson::Document::AllocatorType &alloc,
				const std::vector<ribosome::lstring> &all_words) {
			std::vector<std::string> words, stems;

			for (auto &w: all_words) {
				auto word = ribosome::lconvert::to_string(w);
				words.push_back(word);

				std::string lang = server()->language(word);

				if (m_want_stemming) {
					stems.push_back(server()->stemmer().stem(word, lang, ""));
				}
			}

			auto join = [] (const std::vector<std::string> &words) -> std::string {
				std::ostringstream ss;
				for (size_t i = 0; i < words.size(); ++i) {
					ss << words[i];
					if (i != words.size() - 1)
						ss << " ";
				}
				return ss.str();
			};

			std::string words_joined = join(words);

			rapidjson::Value cv(words_joined.c_str(), words_joined.size(), alloc);
			member.AddMember("text", cv, alloc);

			if (m_want_stemming) {
				std::string stems_joined = join(stems);
				rapidjson::Value sv(stems_joined.c_str(), stems_joined.size(), alloc);
				member.AddMember("stem", sv, alloc);
			}
		}

	};

	warp::stemmer &stemmer() {
		return m_stemmer;
	}

	std::string language(const std::string &word) {
		return m_lch.language(word);
	}
	std::string language(const ribosome::lstring &lw) {
		return m_lch.language(lw);
	}
	std::string language(const std::string &word, const ribosome::lstring &lw) {
		return m_lch.language(word, lw);
	}

	ribosome::error_info detector_save(const std::string &text, const std::string &lang) {
		return m_lch.detector_save(text, lang);
	}

	ribosome::error_info check(const warp::check_control &ctl, std::vector<warp::dictionary::word_form> *ret) {
		return m_lch.check(ctl, ret);
	}
	ribosome::error_info check(const std::string &lang, const warp::check_control &ctl, std::vector<warp::dictionary::word_form> *ret) {
		return m_lch.check(lang, ctl, ret);
	}

private:
	warp::stemmer m_stemmer;
	warp::language_checker m_lch;

	bool lang_init(const rapidjson::Value &config) {
		const char *lang_stats = warp::get_string(config, "language_detector_stats");
		if (!lang_stats) {
			WLOG_ERROR("\"application.language_detector_stats\" option must be a string");
			return false;
		}

		auto err = m_lch.load_langdetect_stats(lang_stats);
		if (err) {
			WLOG_ERROR("could not load language detector: %s [%d]", err.message().c_str(), err.code());
			return false;
		}

		auto &lm = warp::get_object(config, "language_models");
		if (!lm.IsObject()) {
			WLOG_ERROR("\"application.language_models\" must be object");
			return false;
		}

		for (auto member_it = lm.MemberBegin(), member_end = lm.MemberEnd(); member_it != member_end; ++member_it) {
			if (!member_it->value.IsObject()) {
				WLOG_ERROR("\"application.language_models\" entry must be an object");
				return false;
			}

			warp::language_model lm;
			lm.language = member_it->name.GetString();

			if (!parse_model(member_it->value, &lm)) {
				return false;
			}

			auto err = m_lch.load_language_model(lm);
			if (err) {
				WLOG_ERROR("could not load language model, lang: %s, error: %s [%d]",
						lm.language.c_str(), err.message().c_str(), err.code());
				return false;
			}
		}

		return true;
	}

	bool parse_model(const rapidjson::Value &config, warp::language_model *lm) {
		const char *path = warp::get_string(config, "rocksdb_path");
		if (!path) {
			WLOG_ERROR("\"rocksdb_path\" must be a string in language model");
			return false;
		}

		lm->lang_model_path.assign(path);

		auto &em = warp::get_object(config, "error_model");
		if (em.IsObject()) {
			const char *replace = warp::get_string(em, "replace");
			const char *around = warp::get_string(em, "around");

			if (replace)
				lm->error.replace_path.assign(replace);
			if (around)
				lm->error.replace_path.assign(around);
		}

		return true;
	}

};

int main(int argc, char *argv[])
{
	return ioremap::thevoid::run_server<http_server>(argc, argv);
}
