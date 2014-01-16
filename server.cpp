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

#include "lex.hpp"

#include <swarm/http_request.hpp>
#include <swarm/urlfetcher/url_fetcher.hpp>
#include <thevoid/server.hpp>
#include <thevoid/stream.hpp>

#include <thevoid/rapidjson/document.h>
#include <thevoid/rapidjson/stringbuffer.h>
#include <thevoid/rapidjson/prettywriter.h>

using namespace ioremap;

template <typename T>
struct on_grammar : public thevoid::simple_request_stream<T>, public std::enable_shared_from_this<on_grammar<T>>
{
	virtual void on_request(const swarm::http_request &req, const boost::asio::const_buffer &buffer) {
		(void) req;

		rapidjson::Document doc;
		const char *ptr = boost::asio::buffer_cast<const char*>(buffer);
		doc.Parse<0>(ptr);

		if (doc.HasParseError()) {
			this->logger().log(swarm::SWARM_LOG_ERROR, "grammar::request: failed to parse document: %s", doc.GetParseError());
			this->send_reply(swarm::http_response::bad_request);
			return;
		}

		if (!doc.HasMember("data")) {
			this->logger().log(swarm::SWARM_LOG_ERROR, "grammar::request: no 'data' string in the document");
			this->send_reply(swarm::http_response::bad_request);
			return;
		}

		std::string data = doc["data"].GetString();

		std::map<std::string, std::vector<warp::ef>> ewords = this->server()->lex().lookup_sentence(data);

		rapidjson::Document reply;
		reply.SetObject();

		for (auto it = ewords.begin(); it != ewords.end(); ++it) {
			rapidjson::Value features(rapidjson::kArrayType);

			for (auto ef = it->second.begin(); ef != it->second.end(); ++ef) {
				rapidjson::Value obj(rapidjson::kObjectType);

				obj.AddMember("features", ef->features, reply.GetAllocator());
				obj.AddMember("ending-length", ef->ending_len, reply.GetAllocator());

				features.PushBack(obj, reply.GetAllocator());
			}

			reply.AddMember(it->first.c_str(), reply.GetAllocator(), features, reply.GetAllocator());
		}

		rapidjson::StringBuffer sbuf;
		rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(sbuf);

		reply.Accept(writer);
		sbuf.Put('\n');

		swarm::url_fetcher::response http_reply;
		http_reply.set_code(swarm::url_fetcher::response::ok);
		http_reply.headers().set_content_length(sbuf.Size());
		http_reply.headers().set_content_type("text/json");

		std::string sbuf_data = sbuf.GetString();

		this->send_reply(std::move(http_reply), std::move(sbuf_data));
	}
};

class http_server : public thevoid::server<http_server>
{
public:
	virtual bool initialize(const rapidjson::Value &config) {
		if (!config.HasMember("msgpack-input")) {
			this->logger().log(swarm::SWARM_LOG_ERROR, "initialize: no msgpack-input option");
			return false;
		}

		std::string path = config["msgpack-input"].GetString();
		m_lex.load(path);

		this->logger().log(swarm::SWARM_LOG_ERROR, "grammar::request: data from %s has been loaded", path.c_str());

		on<on_grammar<http_server>>(
			options::exact_match("/grammar"),
			options::methods("POST")
		);
	
		return true;
	}

	warp::lex &lex(void) {
		return m_lex;
	}

private:
	warp::lex m_lex;
};

int main(int argc, char *argv[])
{
	return ioremap::thevoid::run_server<http_server>(argc, argv);
}
