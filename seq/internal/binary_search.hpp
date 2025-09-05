/**
 * MIT License
 *
 * Copyright (c) 2025 Victor Moncada <vtr.moncada@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


#ifndef SEQ_BINARY_SEARCH_HPP
#define SEQ_BINARY_SEARCH_HPP

#include "../type_traits.hpp"
#include <algorithm>

namespace seq
{


	template<bool Multi, class Key, class Iter, class SizeType, class U, class Less>
	std::pair<SizeType, bool> lower_bound(Iter ptr, SizeType size, const U& value, const Less& le)
	{
		using T = typename std::iterator_traits<Iter>::value_type;

		if constexpr (std::is_arithmetic_v<Key>) {
			static constexpr SizeType end_of_probe = (sizeof(T) > 16U ? 8 : (sizeof(T) > 8U ? 16 : 32));
			SizeType low = 0;
			while (size > end_of_probe) {
				SizeType half = size / 2;
				low = le(ptr[low + half], value) ? (low + size - half) : low;
				size = half;
				
				half = size / 2;
				low = le(ptr[low + half], value) ? (low + size - half) : low;
				size = half;

			}
			// Finish with linear probing
			size += low;
			while (low < size && le(ptr[low], value))
				++low;
			return { low, false };
		}
		else if constexpr (has_comparable<Less>::value) {
			SizeType s = 0;
			SizeType e = size;
			if constexpr (Multi) {
				bool exact_match = false;
				while (s != e) {
					const SizeType mid = (s + e) >> 1;
					const int c = le.compare(ptr[mid], value);
					if (c < 0) {
						s = mid + 1;
					}
					else {
						e = mid;
						if (c == 0) {
							// Need to return the first value whose key is not less than value,
							// which requires continuing the binary search if this is a
							// multi-container.
							exact_match = true;
						}
					}
				}
				return { s, exact_match };
			}
			else { // Not a multi-container.
				while (s != e) {
					const SizeType mid = (s + e) >> 1;
					const int c = le.compare(ptr[mid], value);
					if (c < 0) {
						s = mid + 1;
					}
					else if (c > 0) {
						e = mid;
					}
					else {
						return { mid, true };
					}
				}
				return { s, false };
			}
		}
		else {
			auto p = ptr;
			SizeType count = size;
			while (0 < count) {
				const SizeType half = count / 2;
				if (le(p[half], value)) {
					p += half + 1;
					count -= half + 1;
				}
				else
					count = half;
			}
			return { static_cast<SizeType>(p - ptr), false };
		}
	}

	template<bool Multi, class Key, class Iter, class SizeType, class U, class Le>
	SEQ_ALWAYS_INLINE SizeType upper_bound(Iter ptr, SizeType size, const U& value, const Le& le)
	{
		return lower_bound<Key, Multi>(ptr, size, value, [&le](const auto& a, const auto& b) { return !le(b, a); }).first;
	}

} // end seq

#endif
