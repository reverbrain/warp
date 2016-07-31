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

#include "warp/json.hpp"
#include "warp/jsonvalue.hpp"
#include "warp/ngram.hpp"
#include "warp/stem.hpp"

#include <swarm/logger.hpp>

#include <thevoid/server.hpp>
#include <thevoid/stream.hpp>

#include <thevoid/rapidjson/document.h>
#include <thevoid/rapidjson/stringbuffer.h>
#include <thevoid/rapidjson/prettywriter.h>

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

static std::string prepare_text(const std::string &text)
{
	std::string trimmed = boost::algorithm::trim_fill_copy_if(text, " ",
		boost::is_any_of("1234567890-=!@#$%^&*()_+[]\\{}|';\":/.,?><\n\r\t "));

	auto ret = ribosome::lconvert::from_utf8(trimmed);
	//return ribosome::lconvert::to_lower(ret);
	return ribosome::lconvert::to_string(ribosome::lconvert::to_lower(ret));
};

class http_server : public thevoid::server<http_server>
{
public:
	virtual bool initialize(const rapidjson::Value &config) {
		const char *lang_stats = warp::get_string(config, "language_detector_stats");
		if (!lang_stats) {
			WLOG_ERROR("initialize: 'language_detector_stats' option must be a string");
			return false;
		}

		int err = m_det.load_file(lang_stats);
		if (err) {
			WLOG_ERROR("initialize: could not load language detector stats from '%s': %d", lang_stats, err);
			return false;
		}

		on<on_lang>(
			options::exact_match("/lang_detect"),
			options::methods("POST")
		);

		on<on_lang>(
			options::exact_match("/stem"),
			options::methods("POST")
		);

		return true;
	}

	struct on_lang : public thevoid::simple_request_stream<http_server> {
		void send_error(int status, int error, const char *fmt, ...) {
			va_list args;
			va_start(args, fmt);

			char buffer[1024];
			int sz = vsnprintf(buffer, sizeof(buffer), fmt, args);

			WLOG_ERROR("%s: %d", buffer, error);

			warp::JsonValue val;
			rapidjson::Value ev(rapidjson::kObjectType);


			rapidjson::Value esv(buffer, sz, val.GetAllocator());
			ev.AddMember("message", esv, val.GetAllocator());
			ev.AddMember("code", error, val.GetAllocator());
			val.AddMember("error", ev, val.GetAllocator());

			va_end(args);

			std::string data = val.ToString();

			thevoid::http_response http_reply;
			http_reply.set_code(status);
			http_reply.headers().set_content_length(data.size());
			http_reply.headers().set_content_type("text/json");

			this->send_reply(std::move(http_reply), std::move(data));
		}

		virtual void on_request(const thevoid::http_request &http_req, const boost::asio::const_buffer &buffer) {
			bool want_stemming = false;

			if (http_req.url().path().find("/stem") == 0) {
				want_stemming = true;
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

			try {
				warp::JsonValue reply;

				auto request = warp::get_string(doc, "request");
				if (!request) {
					send_error(swarm::http_response::bad_request, -EINVAL,
							"'request' member must be string");
					return;
				}
				ribosome::html_parser html;
				html.feed_text(request);
				std::string nohtml_request = html.text(" ");
				std::string clear_request = prepare_text(nohtml_request);

				rapidjson::Value tokens(rapidjson::kArrayType);

				ribosome::split spl;
				auto all_words = spl.convert_split_words(clear_request.data(), clear_request.size());
				std::set<std::string> words;
				for (auto &w: all_words) {
					words.emplace(ribosome::lconvert::to_string(w));
				}

				for (auto &word: words) {
					std::string lang = server()->detector().detect(word);

					rapidjson::Value lv(lang.c_str(), lang.size(), reply.GetAllocator());
					rapidjson::Value wv(word.c_str(), word.size(), reply.GetAllocator());

					rapidjson::Value tok(rapidjson::kObjectType);
					tok.AddMember("word", wv, reply.GetAllocator());
					tok.AddMember("language", lv, reply.GetAllocator());

					if (want_stemming) {
						std::string stemmed = server()->stemmer().stem(word, lang, "");
						rapidjson::Value sv(stemmed.c_str(), stemmed.size(), reply.GetAllocator());
						tok.AddMember("stem", sv, reply.GetAllocator());
					}

					tokens.PushBack(tok, reply.GetAllocator());
				}

				reply.AddMember("tokens", tokens, reply.GetAllocator());
				std::string reply_data = reply.ToString();

				thevoid::http_response http_reply;
				http_reply.set_code(swarm::http_response::ok);
				http_reply.headers().set_content_length(reply_data.size());
				http_reply.headers().set_content_type("text/json");

				this->send_reply(std::move(http_reply), std::move(reply_data));
			} catch (const std::exception &e) {
				send_error(swarm::http_response::bad_request, -EINVAL, "caught error during processing: %s",
						e.what());
				return;
			}
		}
	};

	warp::stemmer &stemmer() {
		return m_stemmer;
	}

	warp::ngram::detector<std::string, std::string> &detector() {
		return m_det;
	}

private:
	warp::ngram::detector<std::string, std::string> m_det;
	warp::stemmer m_stemmer;
};

int main(int argc, char *argv[])
{
	return ioremap::thevoid::run_server<http_server>(argc, argv);
}
