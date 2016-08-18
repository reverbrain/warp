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

#ifndef __IOREMAP_WARP_PACK_HPP
#define __IOREMAP_WARP_PACK_HPP

#include "warp/database.hpp"
#include "warp/utils.hpp"

#include <ribosome/error.hpp>

namespace ioremap { namespace warp {

class packer {
public:
	static ribosome::error_info write(dictionary::database &db, const dictionary::word_form &wf) {
		dictionary::document_for_index did;
		did.indexed_id = wf.indexed_id;
		std::string sdid = warp::serialize(did);

		rocksdb::WriteBatch batch;

		std::string key = db.options().word_form_prefix + wf.word;
		std::string wfs = warp::serialize(wf);
		batch.Merge(rocksdb::Slice(key), rocksdb::Slice(wfs));

		std::string wkey = db.options().word_form_indexed_prefix + std::to_string(wf.indexed_id);
		batch.Merge(rocksdb::Slice(wkey), rocksdb::Slice(wfs));

		std::set<std::string> ngram_strings;
		auto ngrams = warp::ngram<ribosome::lstring>::split(wf.lw, 2);
		for (auto &n: ngrams) {
			std::string ns = ribosome::lconvert::to_string(n);
			ngram_strings.insert(ns);
		}
		
		for (auto &n: ngram_strings) {
			std::string key = db.options().ngram_prefix + n;
			batch.Merge(rocksdb::Slice(key), rocksdb::Slice(sdid));
		}

		auto err = db.write(&batch);
		if (err)
			return err;

		return ribosome::error_info();
	}
};

}} // namespace ioremap::warp

#endif /* __IOREMAP_WARP_PACK_HPP */
