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

#include "warp/lex.hpp"

#include <swarm/logger.hpp>

#include <thevoid/server.hpp>
#include <thevoid/stream.hpp>

#include <thevoid/rapidjson/document.h>
#include <thevoid/rapidjson/stringbuffer.h>
#include <thevoid/rapidjson/prettywriter.h>

#define WLOG(level, a...) BH_LOG(logger(), level, ##a)
#define WLOG_ERROR(a...) WLOG(SWARM_LOG_ERROR, ##a)
#define WLOG_WARNING(a...) WLOG(SWARM_LOG_WARNING, ##a)
#define WLOG_INFO(a...) WLOG(SWARM_LOG_INFO, ##a)
#define WLOG_NOTICE(a...) WLOG(SWARM_LOG_NOTICE, ##a)
#define WLOG_DEBUG(a...) WLOG(SWARM_LOG_DEBUG, ##a)

using namespace ioremap;

class http_server : public thevoid::server<http_server>
{
public:
	virtual bool initialize(const rapidjson::Value &config) {
		if (!config.HasMember("msgpack-input")) {
			WLOG_ERROR("initialize: no msgpack-input option");
			return false;
		}

		std::vector<std::string> path;

		const auto & input = config["msgpack-input"];
		if (input.IsArray()) {
			for (rapidjson::Value::ConstValueIterator it = input.Begin(); it != input.End(); ++it) {
				path.push_back(it->GetString());
			}
		} else {
			path.push_back(input.GetString());
		}

		m_lex.load(3, path);

		WLOG_INFO("grammar::request: data from %s (and other files) has been loaded", path[0].c_str());

		on<on_grammar>(
			options::exact_match("/grammar"),
			options::methods("POST")
		);
	
		return true;
	}

	warp::lex &lex(void) {
		return m_lex;
	}

	struct on_grammar : public thevoid::simple_request_stream<http_server> {
		virtual void on_request(const thevoid::http_request &http_req, const boost::asio::const_buffer &buffer) {
			(void) http_req;

			rapidjson::Document doc;
			const char *ptr = boost::asio::buffer_cast<const char*>(buffer);
			if (!ptr) {
				WLOG_ERROR("grammar::request: empty request\n");
				this->send_reply(swarm::http_response::bad_request);
				return;
			}

			std::string buf;
			buf.assign(ptr, boost::asio::buffer_size(buffer));

			doc.Parse<0>(buf.c_str());
			if (doc.HasParseError()) {
				WLOG_ERROR("grammar::request: failed to parse document: %s",
						doc.GetParseError());
				this->send_reply(swarm::http_response::bad_request);
				return;
			}

			rapidjson::Document reply;
			reply.SetObject();

			try {
				rapidjson::Value reply_array(rapidjson::kArrayType);

				if (!doc.HasMember("request")) {
					WLOG_ERROR("grammar::request: no 'request' field in the document");
					this->send_reply(swarm::http_response::bad_request);
					return;
				}

				const rapidjson::Value &req = doc["request"];
				if (req.IsArray()) {
					for (rapidjson::Value::ConstValueIterator it = req.Begin(); it != req.End(); ++it) {
						rapidjson::Value element(rapidjson::kObjectType);
						parse_single_element(*it, element, reply.GetAllocator());

						reply_array.PushBack(element, reply.GetAllocator());
					}
				} else {
					rapidjson::Value element(rapidjson::kObjectType);
					parse_single_element(req, element, reply.GetAllocator());

					reply_array.PushBack(element, reply.GetAllocator());
				}

				reply.AddMember("reply", reply_array, reply.GetAllocator());

				rapidjson::StringBuffer sbuf;
				rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(sbuf);

				reply.Accept(writer);
				sbuf.Put('\n');

				thevoid::http_response http_reply;
				http_reply.set_code(swarm::http_response::ok);
				http_reply.headers().set_content_length(sbuf.Size());
				http_reply.headers().set_content_type("text/json");

				std::string sbuf_data = sbuf.GetString();

				WLOG_DEBUG("grammar::request: completed processing, reply-size: %zd", sbuf.Size());

				this->send_reply(std::move(http_reply), std::move(sbuf_data));
			} catch (const std::exception &e) {
				WLOG_ERROR("grammar::request: caught exception during processing: %s", e.what());
				this->send_reply(swarm::http_response::bad_request);
				return;
			}
		}

		bool parse_single_element(const rapidjson::Value &val, rapidjson::Value &reply,
				rapidjson::Document::AllocatorType &allocator) {
			if (!val.HasMember("data")) {
				WLOG_ERROR("grammar::parse_single_element: no 'data' string in the document");
				return false;
			}

			std::string data = val["data"].GetString();
			bool normalize = val.HasMember("normalize");
			WLOG_NOTICE("grammar::parse_single_element: length: %ld, data: '%s', normalize: %d",
					data.size(), data.c_str(), normalize);

			if (normalize) {
				std::vector<std::string> roots = this->server()->lex().normalize_sentence(data);
				std::ostringstream ss;

				for (auto it = roots.begin(); it != roots.end(); ++it)
					ss << *it << " ";

				std::string text = ss.str();
				rapidjson::Value norm(text.c_str(), text.size(), allocator);
				reply.AddMember("normalize", norm, allocator);
			} else {
				rapidjson::Value data_obj(rapidjson::kObjectType);

				std::vector<warp::word_features> ewords = this->server()->lex().lookup_sentence(data);
				for (auto it = ewords.begin(); it != ewords.end(); ++it) {
					rapidjson::Value features(rapidjson::kArrayType);

					for (auto ef = it->fvec.begin(); ef != it->fvec.end(); ++ef) {
						rapidjson::Value obj(rapidjson::kObjectType);

						obj.AddMember("features", ef->features, allocator);
						obj.AddMember("ending-length", ef->ending_len, allocator);

						features.PushBack(obj, allocator);
					}

					data_obj.AddMember(it->word.c_str(), allocator, features, allocator);
				}
				reply.AddMember("lemmas", data_obj, allocator);

				if (val.HasMember("grammar")) {
					rapidjson::Value grammar_obj(rapidjson::kObjectType);
					std::string grammar = val["grammar"].GetString();

					std::vector<warp::grammar> grams = this->server()->lex().generate(grammar);
					std::vector<int> starts = this->server()->lex().grammar_deduction_sentence(grams, data);

					rapidjson::Value jstarts(rapidjson::kArrayType);
					rapidjson::Value jstrings(rapidjson::kArrayType);

					for (auto s = starts.begin(); s != starts.end(); ++s) {
						jstarts.PushBack(*s, allocator);

						std::ostringstream out;
						for (size_t i = 0; i < grams.size(); ++i) {
							out << ewords[i + *s].word;
							if (i != grams.size() - 1)
								out << " ";
						}

						rapidjson::Value tmp;
						tmp.SetString(out.str().c_str(), allocator);

						jstrings.PushBack(tmp, allocator);
					}

					grammar_obj.AddMember("starts", jstarts, allocator);
					grammar_obj.AddMember("texts", jstrings, allocator);

					reply.AddMember("grammar", grammar_obj, allocator);
				}
			}

			return true;
		}
	};


private:
	warp::lex m_lex;
};

int main(int argc, char *argv[])
{
	return ioremap::thevoid::run_server<http_server>(argc, argv);
}
