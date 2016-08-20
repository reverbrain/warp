#pragma once

#include "warp/json.hpp"
#include "warp/jsonvalue.hpp"
#include "warp/language_model.hpp"
#include "warp/thevoid_stream.hpp"

#include <ribosome/split.hpp>

namespace ioremap { namespace warp {

template <typename Server>
class on_error_check : public thevoid::simple_request_stream_error<Server> {
public:
	using thevoid::simple_request_stream_error<Server>::send_error;
	using thevoid::simple_request_stream_error<Server>::server;

	virtual void on_request(const thevoid::http_request &http_req, const boost::asio::const_buffer &buffer) {
		int level = warp::check_control::level_2;
		if (http_req.url().query().has_item("level")) {
			auto opt = http_req.url().query().item_value("level");
			level = atoi((*opt).c_str());
		}
		int max_num = 3;
		if (http_req.url().query().has_item("max_num")) {
			auto opt = http_req.url().query().item_value("max_num");
			max_num = atoi((*opt).c_str());
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

			ribosome::lstring text = ribosome::lconvert::from_utf8(member_it->value.GetString(),
					member_it->value.GetStringLength());

			auto lower_request = ribosome::lconvert::to_lower(text);
			rapidjson::Value tokens(rapidjson::kArrayType);

			ribosome::split spl;
			auto all_words = spl.convert_split_words(lower_request, "");
			for (auto &w: all_words) {
				std::string word = ribosome::lconvert::to_string(w);
				std::string lang = server()->language(word, w);

				struct check_control ctl;
				ctl.word = word;
				ctl.lw = w;
				ctl.level = level;
				ctl.max_num = max_num;

				std::vector<warp::dictionary::word_form> forms;

				auto err = server()->check(lang, ctl, &forms);
				if (err) {
					warp::dictionary::word_form orig;
					orig.word = word;
					orig.lw = w;

					forms.emplace_back(orig);
				}

				rapidjson::Value token(rapidjson::kObjectType);
				rapidjson::Value wv(word.c_str(), word.size(), alloc);
				token.AddMember("word", wv, alloc);
				rapidjson::Value lv(lang.c_str(), lang.size(), alloc);
				token.AddMember("language", lv, alloc);

				rapidjson::Value wfs(rapidjson::kArrayType);
				for (auto &wf: forms) {
					rapidjson::Value fv(rapidjson::kObjectType);

					rapidjson::Value wv(wf.word.c_str(), wf.word.size(), alloc);
					fv.AddMember("word", wv, alloc);
					fv.AddMember("freq", wf.freq, alloc);
					fv.AddMember("similarity", wf.freq_norm, alloc);

					wfs.PushBack(fv, alloc);
				}

				token.AddMember("forms", wfs, alloc);
				tokens.PushBack(token, alloc);
			}

			reply.AddMember(member_it->name.GetString(), alloc, tokens, alloc);
		}

		std::string reply_data = reply.ToString();

		thevoid::http_response http_reply;
		http_reply.set_code(swarm::http_response::ok);
		http_reply.headers().set_content_length(reply_data.size());
		http_reply.headers().set_content_type("text/json");

		this->send_reply(std::move(http_reply), std::move(reply_data));
	}

private:
};


}} // namespace ioremap::warp
