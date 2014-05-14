/*
 * Copyright 2014+ Evgeniy Polyakov <zbr@ioremap.net>
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

#ifndef __WARP_DISTANCE_HPP
#define __WARP_DISTANCE_HPP

#include <algorithm>
#include <vector>

namespace ioremap { namespace warp { namespace distance {

template <typename S>
static int levenstein(const S &s, const S &t, int min_dist) {
	// degenerate cases
	if (s == t)
		return 0;
	if (s.size() == 0)
		return t.size();
	if (t.size() == 0)
		return s.size();

	// create two work vectors of integer distances
	std::vector<int> v0(t.size() + 1);
	std::vector<int> v1(t.size() + 1);

	// initialize v0 (the previous row of distances)
	// this row is A[0][i]: edit distance for an empty s
	// the distance is just the number of characters to delete from t
	for (size_t i = 0; i < v0.size(); ++i)
		v0[i] = i;

	for (size_t i = 0; i < s.size(); ++i) {
		// calculate v1 (current row distances) from the previous row v0

		// first element of v1 is A[i+1][0]
		//   edit distance is delete (i+1) chars from s to match empty t
		v1[0] = i + 1;

		// use formula to fill in the rest of the row
		for (size_t j = 0; j < t.size(); ++j) {
			int cost = (s[i] == t[j]) ? 0 : 1;
			v1[j + 1] = std::min(v1[j] + 1, v0[j + 1] + 1);
			v1[j + 1] = std::min(v1[j + 1], v0[j] + cost);
		}

		// copy v1 (current row) to v0 (previous row) for next iteration
		int dist = v1.size();
		for (size_t i = 0; i < v1.size(); ++i) {
			if (v1[i] < dist)
				dist = v1[i];

			v0[i] = v1[i];
		}

		if (dist > min_dist)
			return -1;
	}

	return v1[t.size()];
}

}}} // namespace ioremap::warp::distance

#endif /* __WARP_DISTANCE_HPP */
