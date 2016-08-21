#pragma once

#include "warp/utils.hpp"
#include "warp/ngram.hpp"

#pragma GCC diagnostic push 
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <rocksdb/cache.h>
#include <rocksdb/db.h>
#include <rocksdb/filter_policy.h>
#include <rocksdb/merge_operator.h>
#include <rocksdb/options.h>
#include <rocksdb/slice.h>
#include <rocksdb/status.h>
#include <rocksdb/table.h>
#include <rocksdb/utilities/transaction_db.h>
#pragma GCC diagnostic pop

#include <msgpack.hpp>

#include <ribosome/error.hpp>
#include <ribosome/expiration.hpp>
#include <ribosome/lstring.hpp>

#include <memory>
#include <string>

namespace ioremap { namespace warp {

namespace dictionary {
struct word_form {
	std::string		word;
	uint64_t		indexed_id = 0ULL;
	int			freq = 0;
	int			documents = 0;

	ribosome::lstring	lw;
	float			freq_norm = 0;
	int			edit_distance = 0; // used when sorting 

	MSGPACK_DEFINE(word, indexed_id, freq, documents);

	bool operator<(const word_form &other) const {
		return word < other.word;
	}
};

class metadata {
public:
	metadata() : m_dirty(false), m_seq(0) {}

	bool dirty() const {
		return m_dirty;
	}
	void clear_dirty() {
		m_dirty = false;
	}

	long get_sequence() {
		m_dirty = true;
		return m_seq++;
	}

	enum {
		serialize_version_2 = 2,
	};

	template <typename Stream>
	void msgpack_pack(msgpack::packer<Stream> &o) const {
		o.pack_array(metadata::serialize_version_2);
		o.pack((int)metadata::serialize_version_2);
		o.pack(m_seq.load());
	}

	void msgpack_unpack(msgpack::object o) {
		if (o.type != msgpack::type::ARRAY) {
			std::ostringstream ss;
			ss << "could not unpack metadata, object type is " << o.type <<
				", must be array (" << msgpack::type::ARRAY << ")";
			throw std::runtime_error(ss.str());
		}

		int version;
		long seq;

		msgpack::object *p = o.via.array.ptr;
		p[0].convert(&version);

		if (version != (int)o.via.array.size) {
			std::ostringstream ss;
			ss << "could not unpack document, invalid version: " << version << ", array size: " << o.via.array.size;
			throw std::runtime_error(ss.str());
		}

		switch (version) {
		case metadata::serialize_version_2:
			p[1].convert(&seq);
			m_seq.store(seq);
			break;
		default: {
			std::ostringstream ss;
			ss << "could not unpack metadata, invalid version " << version;
			throw std::runtime_error(ss.str());
		}
		}
	}

private:
	bool m_dirty;
	std::atomic_long m_seq;
};

struct document_for_index {
	uint64_t indexed_id;
	MSGPACK_DEFINE(indexed_id);

	bool operator<(const document_for_index &other) const {
		return indexed_id < other.indexed_id;
	}
};

struct disk_index {
	typedef document_for_index value_type;
	typedef document_for_index& reference;
	typedef document_for_index* pointer;

	std::vector<document_for_index> ids;

	MSGPACK_DEFINE(ids);
};


class merge_operator : public rocksdb::MergeOperator {
public:
	virtual const char* Name() const override {
		return "dictionary::merge_operator";
	}

	bool merge_ngram_index(const rocksdb::Slice& key, const rocksdb::Slice* old_value,
			const std::deque<std::string>& operand_list,
			std::string* new_value,
			rocksdb::Logger *logger) const {

		struct disk_index index;
		ribosome::error_info err;
		std::set<document_for_index> unique_index;

		if (old_value) {
			err = deserialize(index, old_value->data(), old_value->size());
			if (err) {
				rocksdb::Error(logger, "merge: key: %s, index deserialize failed: %s [%d]",
						key.ToString().c_str(), err.message().c_str(), err.code());
				return false;
			}

			unique_index.insert(index.ids.begin(), index.ids.end());
		}

		for (const auto& value : operand_list) {
			document_for_index did;
			err = deserialize(did, value.data(), value.size());
			if (err) {
				rocksdb::Error(logger, "merge: key: %s, document deserialize failed: %s [%d]",
						key.ToString().c_str(), err.message().c_str(), err.code());
				return false;
			}

			unique_index.emplace(did);
		}

		index.ids.clear();
		index.ids.insert(index.ids.end(), unique_index.begin(), unique_index.end());
		*new_value = serialize(index);

		return true;
	}


	bool merge_word_forms(const rocksdb::Slice& key, const rocksdb::Slice* old_value,
			const std::deque<std::string>& operand_list,
			std::string* new_value,
			rocksdb::Logger *logger) const {

		word_form wf;
		ribosome::error_info err;

		if (old_value) {
			err = warp::deserialize(wf, old_value->data(), old_value->size());
			if (err) {
				rocksdb::Error(logger, "merge: key: %s, index deserialize failed: %s [%d]",
						key.ToString().c_str(), err.message().c_str(), err.code());
				return false;
			}
		}

		for (const auto& value : operand_list) {
			word_form merge_form;

			err = warp::deserialize(merge_form, value.data(), value.size());
			if (err) {
				rocksdb::Error(logger, "merge: key: %s, document deserialize failed: %s [%d]",
						key.ToString().c_str(), err.message().c_str(), err.code());
				return false;
			}

			wf.freq += merge_form.freq;
			wf.documents += merge_form.documents;

			if (wf.word.empty())
				wf.word = merge_form.word;
		}

		*new_value = warp::serialize(wf);
		return true;
	}

	virtual bool FullMerge(const rocksdb::Slice& key, const rocksdb::Slice* old_value,
			const std::deque<std::string>& operand_list,
			std::string* new_value,
			rocksdb::Logger *logger) const override {
		if (key.starts_with(rocksdb::Slice("wf.")) || key.starts_with(rocksdb::Slice("wf_indexed."))) {
			return merge_word_forms(key, old_value, operand_list, new_value, logger);
		} else if (key.starts_with(rocksdb::Slice("ngram."))) {
			return merge_ngram_index(key, old_value, operand_list, new_value, logger);
		}

		return false;
	}

	virtual bool PartialMerge(const rocksdb::Slice& key,
			const rocksdb::Slice& left_operand, const rocksdb::Slice& right_operand,
			std::string* new_value,
			rocksdb::Logger* logger) const {
		(void) key;
		(void) left_operand;
		(void) right_operand;
		(void) new_value;
		(void) logger;

		return false;
	}
};

class database {
public:
	struct options {
		int bits_per_key = 10; // bloom filter parameter

		long lru_cache_size = 100 * 1024 * 1024; // 100 MB of uncompressed data cache

		long sync_metadata_timeout = 60000; // 60 seconds

		std::string word_form_prefix;
		std::string word_form_indexed_prefix;
		std::string ngram_prefix;
		std::string transform_prefix;
		std::string metadata_key;

		options():
			word_form_prefix("wf."),
			word_form_indexed_prefix("wf_indexed."),
			ngram_prefix("ngram."),
			transform_prefix("transform."),
			metadata_key("dictionary.meta.")
		{
		}
	};

	~database() {
		if (!m_ro) {
			m_expiration_timer.stop();
			sync_metadata(NULL);
		}
	}

	const database::options &options() const {
		return m_opts;
	}
	dictionary::metadata &metadata() {
		return m_meta;
	}

	void compact() {
		if (m_db) {
			struct rocksdb::CompactRangeOptions opts;
			opts.change_level = true;
			opts.target_level = 0;
			m_db->CompactRange(opts, NULL, NULL);
		}
	}

	ribosome::error_info open_read_only(const std::string &path) {
		return open(path, true);
	}
	ribosome::error_info open_read_write(const std::string &path) {
		return open(path, false);
	}

	ribosome::error_info open(const std::string &path, bool ro) {
		if (m_db) {
			return ribosome::create_error(-EINVAL, "database is already opened");
		}

		rocksdb::Options dbo;
		dbo.OptimizeForPointLookup(4);
		dbo.max_open_files = 1000;
		//dbo.disableDataSync = true;

		dbo.compression = rocksdb::kZlibCompression;

		dbo.create_if_missing = true;
		dbo.create_missing_column_families = true;

		dbo.merge_operator.reset(new merge_operator);

		rocksdb::BlockBasedTableOptions table_options;
		table_options.block_cache = rocksdb::NewLRUCache(m_opts.lru_cache_size); // 100MB of uncompresseed data cache
		table_options.filter_policy.reset(rocksdb::NewBloomFilterPolicy(m_opts.bits_per_key, true));
		dbo.table_factory.reset(rocksdb::NewBlockBasedTableFactory(table_options));

		rocksdb::DB *db;
		rocksdb::Status s;

		if (ro) {
			s = rocksdb::DB::OpenForReadOnly(dbo, path, &db);
		} else {
			s = rocksdb::DB::Open(dbo, path, &db);
		}
		if (!s.ok()) {
			return ribosome::create_error(-s.code(), "failed to open rocksdb database: '%s', read-only: %d, error: %s",
					path.c_str(), ro, s.ToString().c_str());
		}
		m_db.reset(db);
		m_ro = ro;

		std::string meta;
		s = m_db->Get(rocksdb::ReadOptions(), rocksdb::Slice(m_opts.metadata_key), &meta);
		if (!s.ok() && !s.IsNotFound()) {
			return ribosome::create_error(-s.code(), "could not read key: %s, error: %s",
					m_opts.metadata_key.c_str(), s.ToString().c_str());
		}

		if (s.ok()) {
			auto err = deserialize(m_meta, meta.data(), meta.size());
			if (err)
				return ribosome::create_error(err.code(), "metadata deserialization failed, key: %s, error: %s",
					m_opts.metadata_key.c_str(), err.message().c_str());
		}

		if (m_opts.sync_metadata_timeout > 0 && !ro) {
			sync_metadata_callback();
		}

		return ribosome::error_info(); 
	}

	ribosome::error_info sync_metadata(rocksdb::WriteBatch *batch) {
		if (m_ro) {
			return ribosome::create_error(-EROFS, "read-only database");
		}

		if (!m_db) {
			return ribosome::create_error(-EINVAL, "database is not opened");
		}

		if (!m_meta.dirty())
			return ribosome::error_info();

		std::string meta_serialized = serialize(m_meta);

		rocksdb::Status s;
		if (batch) {
			batch->Put(rocksdb::Slice(m_opts.metadata_key), rocksdb::Slice(meta_serialized));
		} else {
			s = m_db->Put(rocksdb::WriteOptions(), rocksdb::Slice(m_opts.metadata_key), rocksdb::Slice(meta_serialized));
		}

		if (!s.ok()) {
			return ribosome::create_error(-s.code(), "could not write metadata key: %s, error: %s",
					m_opts.metadata_key.c_str(), s.ToString().c_str());
		}

		m_meta.clear_dirty();
		return ribosome::error_info();
	}


	ribosome::error_info read(const std::string &key, std::string *ret) {
		if (!m_db) {
			return ribosome::create_error(-EINVAL, "database is not opened");
		}

		auto s = m_db->Get(rocksdb::ReadOptions(), rocksdb::Slice(key), ret);
		if (!s.ok()) {
			return ribosome::create_error(-s.code(), "could not read key: %s, error: %s", key.c_str(), s.ToString().c_str());
		}
		return ribosome::error_info();
	}

	ribosome::error_info read(const std::string &key, word_form *wf) {
		std::string ret;
		auto err = read(key, &ret);
		if (err)
			return err;

		err = warp::deserialize(*wf, ret.data(), ret.size());
		if (err) {
			return ribosome::create_error(err.code(), "could not deserialize word form: %s, error: %s",
					key.c_str(), err.message().c_str());
		}

		return ribosome::error_info();
	}

	ribosome::error_info write(rocksdb::WriteBatch *batch) {
		if (!m_db) {
			return ribosome::create_error(-EINVAL, "database is not opened");
		}

		if (m_ro) {
			return ribosome::create_error(-EROFS, "read-only database");
		}

		auto wo = rocksdb::WriteOptions();

		auto s = m_db->Write(wo, batch);
		if (!s.ok()) {
			return ribosome::create_error(-s.code(), "could not write batch: %s", s.ToString().c_str());
		}

		return ribosome::error_info();
	}

	ribosome::error_info write(const std::string &key, const word_form &wf) {
		auto wo = rocksdb::WriteOptions();

		std::string wfs = warp::serialize(wf);
		auto s = m_db->Put(wo, rocksdb::Slice(key), wfs);
		if (!s.ok()) {
			return ribosome::create_error(-s.code(), "could not write key: %s, size: %ld, error: %s",
					key.c_str(), wfs.size(), s.ToString().c_str());
		}

		return ribosome::error_info();
	}

private:
	bool m_ro = true;
	std::unique_ptr<rocksdb::DB> m_db;
	struct options m_opts;
	dictionary::metadata m_meta;

	ribosome::expiration m_expiration_timer;

	void sync_metadata_callback() {
		sync_metadata(NULL);

		auto expires_at = std::chrono::system_clock::now() + std::chrono::milliseconds(m_opts.sync_metadata_timeout);
		m_expiration_timer.insert(expires_at, std::bind(&database::sync_metadata_callback, this));
	}
};

} // namespace dictionary


}} // namespace ioremap::warp
