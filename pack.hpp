#ifndef __IOREMAP_WARP_PACK_HPP
#define __IOREMAP_WARP_PACK_HPP

#include <fstream>
#include <iostream>
#include <string>
#include <sstream>

#include <msgpack.hpp>

#include "feature.hpp"

namespace ioremap { namespace warp {

struct entry {
	static const int serialization_version;

	std::string root;
	std::string ending;
	parsed_word::feature_mask_t features;

	entry() : features(0ULL) {}
};

const int entry::serialization_version = 1;

class packer {
	public:
		packer(const std::string &output) {
			m_out.exceptions(m_out.failbit);
			m_out.open(output.c_str(), std::ios::binary | std::ios::trunc);
		}

		packer(const packer &z) = delete;

		bool zprocess(const std::string &root, const struct parsed_word &rec) {
			return pack(root, rec.ending, rec.feature_mask);
		}

	private:
		std::ofstream m_out;

		bool pack(const std::string &root, const std::string &ending, const parsed_word::feature_mask_t features) {
			msgpack::sbuffer buf;
			msgpack::packer<msgpack::sbuffer> pk(&buf);

			pk.pack_array(4);
			pk.pack(entry::serialization_version);
			pk.pack(root);
			pk.pack(ending);
			pk.pack(features);

			m_out.write(buf.data(), buf.size());
			return true;
		}

};

class unpacker {
	public:
		unpacker(const std::string &input) {
			m_in.exceptions(m_in.failbit);
			m_in.open(input.c_str(), std::ios::binary);
		}

		typedef std::function<bool (const entry &)> unpack_process;
		void unpack(const unpack_process &process) {
			msgpack::unpacker pac;

			try {
				timer t, total;
				long num = 0;
				long chunk = 100000;
				long duration;

				while (true) {
					pac.reserve_buffer(1024 * 1024);
					size_t bytes = m_in.readsome(pac.buffer(), pac.buffer_capacity());

					if (!bytes)
						break;
					pac.buffer_consumed(bytes);

					msgpack::unpacked result;
					while (pac.next(&result)) {
						msgpack::object obj = result.get();

						entry e;
						obj.convert<entry>(&e);

						if (!process(e))
							return;

						if ((++num % chunk) == 0) {
							duration = t.restart();
							std::cout << "Read objects: " << num <<
								", elapsed time: " << total.elapsed() << " msecs" <<
								", speed: " << chunk * 1000 / duration << " objs/sec" <<
								std::endl;
						}
					}
				}

				duration = total.elapsed();
				std::cout << "Read objects: " << num <<
					", elapsed time: " << duration << " msecs" <<
					", speed: " << num * 1000 / duration << " objs/sec" <<
					std::endl;

			} catch (const std::exception &e) {
				std::cerr << "Exception: " << e.what() << std::endl;
			}
		}

	private:
		std::ifstream m_in;
};

}} // namespace ioremap::warp


namespace msgpack {

template <typename Stream>
inline msgpack::packer<Stream> &operator <<(msgpack::packer<Stream> &o, const ioremap::warp::entry &e)
{
	o.pack_array(4);
	o.pack(ioremap::warp::entry::serialization_version);
	o.pack(e.root);
	o.pack(e.ending);
	o.pack(e.features);

	return o;
}

inline ioremap::warp::entry &operator >>(msgpack::object o, ioremap::warp::entry &e)
{
	if (o.type != msgpack::type::ARRAY || o.via.array.size < 1) {
		std::ostringstream ss;
		ss << "entry msgpack: type: " << o.type <<
			", must be: " << msgpack::type::ARRAY <<
			", size: " << o.via.array.size;
		throw std::runtime_error(ss.str());
	}

	object *p = o.via.array.ptr;
	const uint32_t size = o.via.array.size;
	uint16_t version = 0;
	p[0].convert(&version);
	switch (version) {
	case 1: {
		if (size != 4) {
			std::ostringstream ss;
			ss << "entry msgpack: array size mismatch: read: " << size << ", must be: 4";
			throw std::runtime_error(ss.str());
		}

		p[1].convert(&e.root);
		p[2].convert(&e.ending);
		p[3].convert(&e.features);
		break;
	}
	default: {
		std::ostringstream ss;
		ss << "entry msgpack: version mismatch: read: " << version <<
			", must be: <= " << ioremap::warp::entry::serialization_version;
		throw msgpack::type_error();
	}
	}

	return e;
}

} // namespace msgpack


#endif /* __IOREMAP_WARP_PACK_HPP */
