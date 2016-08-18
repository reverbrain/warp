#pragma once

#include <vector>

#include <unistd.h>

namespace ioremap { namespace warp {

template <typename S>
S longest_substring(const S &s1, const S &s2) {
	std::vector<std::vector<int>> a;

	a.resize(s1.size());
	for (auto &e: a) {
		e.resize(s2.size());
	}

	int prefix_start = 0;
	int prefix_len = 0;

	for (int i = 0; i < (int)s1.size(); ++i) {
		for (int j = 0; j < (int)s2.size(); ++j) {
			if (s1[i] != s2[j]) {
				a[i][j] = 0;
				continue;
			}

			if ((i == 0) || (j == 0)) {
				a[i][j] = 1;
			} else {
				a[i][j] = a[i - 1][j - 1] + 1;
			}

			if (a[i][j] > prefix_len) {
				prefix_len = a[i][j];
				prefix_start = i - prefix_len + 1;
			}
		}
	}

	return s1.substr(prefix_start, prefix_len);
}

}} // namespace ioremap::warp
