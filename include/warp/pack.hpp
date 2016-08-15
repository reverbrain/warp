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

#include "feature.hpp"

#include <atomic>
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <thread>
#include <vector>

#include <msgpack.hpp>

#include <boost/lexical_cast.hpp>

namespace ioremap { namespace warp {

class packer {
	public:
		packer(const std::string &output, int output_num) : m_output_base(output), m_output_num(output_num) {
		}

		packer(const packer &z) = delete;
		~packer() {
			std::ofstream out;

			int file_id = 0;
			size_t num = 0;
			size_t step = m_roots.size() / m_output_num + 1;

			for (auto root = m_roots.begin(); root != m_roots.end(); ++root) {
				if (!out.good() || !out.is_open()) {
					std::string name = m_output_base + "." + boost::lexical_cast<std::string>(file_id);
					out.open(name.c_str(), std::ios::binary | std::ios::trunc);
					if (!out.good()) {
						std::cerr << "Could not open output file '" << name << "': " << -errno << std::endl;
						return;
					}

					std::cout << "Using '" << name << "' output file" << std::endl;
				}

				bool ret = pack(out, root->second);
				if (!ret) {
					std::string name = m_output_base + boost::lexical_cast<std::string>(file_id);
					std::cerr << "Could not write to output file '" << name << "': " << -errno << std::endl;
					return;
				}

				if (++num >= step) {
					out.close();
					num = 0;
					++file_id;
				}
			}
		}

		bool zprocess(const struct parsed_word &rec) {
			auto r = m_roots.find(rec.lemma);
			if (r == m_roots.end()) {
				std::vector<parsed_word> vec;
				vec.push_back(rec);

				m_roots[rec.lemma] = vec;
			} else {
				r->second.emplace_back(rec);
			}
			return true;
		}

	private:
		std::map<std::string, std::vector<parsed_word>> m_roots;
		std::string m_output_base;
		int m_output_num;

		bool pack(std::ofstream &out, const std::vector<parsed_word> &words) {
			for (auto word = words.begin(); word != words.end(); ++word) {
				msgpack::sbuffer buf;
				msgpack::pack(&buf, *word);

				out.write(buf.data(), buf.size());
				if (!out.good())
					return false;
			}

			return true;
		}
};

class unpacker {
	public:
		typedef std::function<bool (int idx, const parsed_word &)> unpack_process;
 
		unpacker(const std::vector<std::string> &inputs, int thread_num, const unpack_process &process) : m_total(0) {
			timer t;

			std::vector<std::thread> pool;
			pool.reserve(thread_num);

			for (int i = 0; i < thread_num; ++i) {
				std::vector<std::string> in;

				for (size_t j = i; j < inputs.size(); j += thread_num)
					in.push_back(inputs[j]);

				pool.emplace_back(std::bind(&unpacker::unpack, this, i, in, process));
			}

			for (auto th = pool.begin(); th != pool.end(); ++th)
				th->join();

			std::cout << "Threads: " << thread_num <<
				", read objects: " << m_total <<
				", elapsed time: " << t.elapsed() << " msecs" <<
				", speed: " << m_total * 1000 / t.elapsed() << " objs/sec" <<
				std::endl;
		}

	private:
		std::atomic_long m_total;

		void unpack(int idx, const std::vector<std::string> &inputs, const unpack_process &process) {
			timer total, t;

			long num = 0;
			long chunk = 100000;
			long duration;

			for (auto it = inputs.begin(); it != inputs.end(); ++it) {
				try {
					msgpack::unpacker pac;

					std::ifstream in(*it, std::ios::binary);

					std::ostringstream ss;
					ss << "Opened file '" << *it << "'\n";
					std::cout << ss.str();

					while (true) {
						pac.reserve_buffer(1024 * 1024);
						size_t bytes = in.readsome(pac.buffer(), pac.buffer_capacity());

						if (!bytes)
							break;
						pac.buffer_consumed(bytes);

						msgpack::unpacked result;
						while (pac.next(&result)) {
							msgpack::object obj = result.get();

							parsed_word e;
							obj.convert<parsed_word>(&e);

							if (!process(idx, e))
								return;

							if ((++num % chunk) == 0) {
								duration = t.restart();
								std::cout << "Index: " << idx << ", read objects: " << num <<
									", elapsed time: " << total.elapsed() << " msecs" <<
									", speed: " << chunk * 1000 / duration << " objs/sec" <<
									std::endl;
							}

							++m_total;
						}
					}

				} catch (const std::exception &e) {
					std::cerr << "Exception: " << e.what() << std::endl;
				}
			}

			duration = total.elapsed();
			std::cout << "Index: " << idx << ", read objects: " << num <<
				", elapsed time: " << duration << " msecs" <<
				", speed: " << num * 1000 / duration << " objs/sec" <<
				std::endl;

		}
};

}} // namespace ioremap::warp

#endif /* __IOREMAP_WARP_PACK_HPP */
