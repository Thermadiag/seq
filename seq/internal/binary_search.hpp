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
