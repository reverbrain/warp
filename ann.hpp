#ifndef __WARP_ANN_HPP
#define __WARP_ANN_HPP

namespace ioremap { namespace warp {

static const float default_zero = 0.001;

class ann {
	public:
		ann() {
			boost::locale::generator gen;
			m_loc = gen("en_US.UTF8");
		}

		~ann() {
			std::cout << "Total endings: " << m_endings.size() << std::endl;
			for (auto & e : m_endings) {
				std::cout << e.first;
				for (auto f : e.second) {
					std::cout << " " << std::hex << f;
				}

				std::cout << std::endl;
			}
		}

		bool process(const std::string &root, const ioremap::warp::parsed_word &rec) {
			m_endings[rec.ending].insert(rec.features);

			m_words[root].forms.emplace_back(rec);

			std::string tmp = root + rec.ending;
			lb::ssegment_index cmap(lb::character, tmp.begin(), tmp.end(), m_loc);

			for (auto it = cmap.begin(), e = cmap.end(); it != e; ++it) {
				m_letters.insert(it->str());
			}

			return true;
		}

		template <typename T>
		std::vector<T> convert_word(const std::vector<std::string> &lv, const std::string &word, int max) {
			lb::ssegment_index wmap(lb::character, word.begin(), word.end(), m_loc);

			std::vector<T> out_vec;
			out_vec.resize(max * lv.size(), default_zero);

			std::vector<int> positions;

			for (auto it = wmap.begin(), e = wmap.end(); it != e; ++it) {
				auto pos = std::lower_bound(lv.begin(), lv.end(), it->str());
				if (*pos == it->str()) {
					positions.push_back(pos - lv.begin());
				}
			}

			int letter = 0;
			for (auto pos = positions.rbegin(); pos != positions.rend(); ++pos) {
				out_vec[*pos + letter * lv.size()] = 1;
				if (--max == 0)
					break;
				++letter;
			}

			return out_vec;
		}

		void output_string(std::ofstream &out, const std::vector<std::string> &lv, const std::string &word, int max) {
			std::vector<float> f = convert_word<float>(lv, word, max);

			for (auto it = f.begin(); it != f.end(); ++it)
				out << *it << " ";
		}

		std::vector<std::string> load_letters(const std::string &path) {
			std::ifstream in(path.c_str());

			if (!in.good()) {
				std::ostringstream ss;
				ss << "Failed to open letters file '" << path << "'";
				throw std::runtime_error(ss.str());
			}

			std::vector<std::string> lv;
			std::string line;
			while (std::getline(in, line)) {
				lv.push_back(line);
			}

			return lv;
		}

		void dump_letters(const std::string &output) {
			std::ofstream out_letter_map(output, std::ios::trunc | std::ios::binary);
			for (auto it = m_letters.begin(); it != m_letters.end(); ++it) {
				out_letter_map << *it << std::endl;
			}
		}

		void dump_features(const ioremap::warp::zparser &records, const std::string &output, int word_size, int ending_size) {
			std::vector<std::string> lv;
			std::copy(m_letters.begin(), m_letters.end(), std::back_inserter(lv));

			std::ofstream out(output.c_str(), std::ios::trunc | std::ios::binary);

			dump_letters(output + ".letters");

			int word_num = 0;
			int features = 0;
			int words_total = m_words.size();
			int words_chunk = words_total / 100;
			out << records.total_features_num() << " " << word_size * lv.size() << " " << records.parser_features_num() + ending_size * lv.size() << std::endl;
			for (auto root = m_words.begin(); root != m_words.end(); ++root) {
				for (auto rec = root->second.forms.begin(); rec != root->second.forms.end(); ++rec) {
					std::string word = root->first + rec->ending;

					output_string(out, lv, word, word_size);
					out << "\n";

					output_string(out, lv, rec->ending, ending_size);

					size_t pos = 0;
					for (int i = 0; i < records.parser_features_num(); ++i) {
						float tmp = default_zero;

						if ((pos < rec->ent.size()) && (i == rec->ent[pos].position)) {
							tmp = 1.0;
							++pos;
						}

						out << tmp << " ";
					}
					out << "\n";
					features++;
				}

				if (++word_num % words_chunk == 0)
					std::cout << "Dumped " << word_num << "/" << words_total << " words, number of features: " << features << std::endl;
			}

			std::cout << "Dumped " << word_num << "/" << words_total << " words, number of features: " << features << std::endl;
		}

	private:
		std::locale m_loc;

		std::map<std::string, ioremap::warp::record> m_words;
		std::set<std::string> m_letters;
		std::map<std::string, std::set<parsed_word::feature_mask>> m_endings;

};

}} // namespace ioremap::warp

#endif /* __WARP_ANN_HPP */
