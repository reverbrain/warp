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

		void zprocess(const std::string &root, const struct parsed_word &rec) {
			pack(root, rec.ending, rec.feature_mask);
		}

	private:
		std::ofstream m_out;

		void pack(const std::string &root, const std::string &ending, const parsed_word::feature_mask_t features) {
			msgpack::sbuffer buf;
			msgpack::packer<msgpack::sbuffer> pk(&buf);

			pk.pack_array(4);
			pk.pack(entry::serialization_version);
			pk.pack(root);
			pk.pack(ending);
			pk.pack(features);

			m_out.write(buf.data(), buf.size());
		}

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
	if (o.type != msgpack::type::ARRAY || o.via.array.size < 1)
		throw msgpack::type_error();

	object *p = o.via.array.ptr;
	const uint32_t size = o.via.array.size;
	uint16_t version = 0;
	p[0].convert(&version);
	switch (version) {
	case 1: {
		if (size != 4)
			throw msgpack::type_error();

		p[1].convert(&e.root);
		p[2].convert(&e.ending);
		p[3].convert(&e.features);
		break;
	}
	default:
		throw msgpack::type_error();
	}

	return e;
}

} // namespace msgpack

int main(int argc, char *argv[])
{
	namespace bpo = boost::program_options;

	bpo::options_description op("Parser options");

	std::string input, output;
	op.add_options()
		("help", "This help message")
		("input", bpo::value<std::string>(&input), "Input file")
		("output", bpo::value<std::string>(&output), "Output file")
		;

	bpo::variables_map vm;
	bpo::store(bpo::parse_command_line(argc, argv, op), vm);
	bpo::notify(vm);

	if (!vm.count("input") || !vm.count("output")) {
		std::cerr << "You must provide input and output files\n" << op << std::endl;
		return -1;
	}

	namespace iw = ioremap::warp;

	iw::packer pack(output);
	iw::zparser records;
	records.set_process(std::bind(&iw::packer::zprocess, &pack, std::placeholders::_1, std::placeholders::_2));
	records.parse_file(input);
}

