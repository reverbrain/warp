#include <boost/program_options.hpp>

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

		void unpack(void) {
			msgpack::unpacker pac;

			try {
				while (true) {
					pac.reserve_buffer(4096);
					size_t bytes = m_in.readsome(pac.buffer(), pac.buffer_capacity());

					if (!bytes)
						break;
					pac.buffer_consumed(bytes);

					msgpack::unpacked result;
					while (pac.next(&result)) {
						msgpack::object obj = result.get();

						entry e;
						obj.convert<entry>(&e);
					}
				}
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
		ss << "entry msgpack: type: " << o.type << ", must be: " << msgpack::type::ARRAY << ", size: " << o.via.array.size;
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
		ss << "entry msgpack: version mismatch: read: " << version << ", must be: <= " << ioremap::warp::entry::serialization_version;
		throw msgpack::type_error();
	}
	}

	return e;
}

} // namespace msgpack

int main(int argc, char *argv[])
{
	namespace bpo = boost::program_options;

	bpo::options_description op("Parser options");

	std::string input, output, msgin;
	op.add_options()
		("help", "This help message")
		("input", bpo::value<std::string>(&input), "Input Zaliznyak dictionary file")
		("output", bpo::value<std::string>(&output), "Output msgpack file")
		("msgpack-input", bpo::value<std::string>(&msgin), "Input msgpack file")
		;

	bpo::variables_map vm;
	bpo::store(bpo::parse_command_line(argc, argv, op), vm);
	bpo::notify(vm);

	namespace iw = ioremap::warp;

	if (!(vm.count("input") && vm.count("output")) && !vm.count("msgpack-input")) {
		std::cerr << op;
		return -1;
	}

	if (vm.count("input") && vm.count("output")) {
		iw::packer pack(output);
		iw::zparser records;
		records.set_process(std::bind(&iw::packer::zprocess, &pack, std::placeholders::_1, std::placeholders::_2));
		records.parse_file(input);
	}

	if (vm.count("msgpack-input")) {
		iw::unpacker unpack(msgin);
		unpack.unpack();
	}

	return 0;
}

