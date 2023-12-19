#pragma once

#include "../type_traits.hpp"
#include <algorithm>


#define __SEQ_INLINE_LOOKUP 


namespace seq
{
	namespace detail
	{
		template <class T, class = void>
		struct has_comparable : std::false_type {};

		template <class T>
		struct has_comparable<T,
			typename make_void<typename T::comparable>::type>
			: std::true_type {};



		template<class Key, bool Multi, bool Comparable, bool Branchless = std::is_arithmetic<Key>::value >
		struct lower_bound_internal
		{
			template<class T, class SizeType, class U, class Le>
			static __SEQ_INLINE_LOOKUP std::pair<SizeType,bool> apply(const T* ptr, SizeType size, const U& value, const Le& le)
			{
				// This unrolled and branchless variant is faster than std::lower_bound for arithmetic types.
				// The end_of_probe value is selected so that the linear search part fits within a cache line.
				// Inspired by https://academy.realm.io/posts/how-we-beat-cpp-stl-binary-search/

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

				size += low;
				while (low < size && le(ptr[low], value))
					++low;
				return { low,false };
			}

		};

		template<class Key, bool Multi>
		struct lower_bound_internal<Key,Multi, false, false>
		{
			template<class T, class SizeType, class U, class Le>
			static __SEQ_INLINE_LOOKUP std::pair<SizeType, bool> apply(const T* ptr, SizeType size, const U& value, const Le& le)
			{
				const T* p = ptr;
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
				return { static_cast<SizeType>(p - ptr),false };
			}

		};


		template<class Key, bool Multi>
		struct lower_bound_internal<Key, Multi, true, false>
		{
			template<class T, class SizeType, class U, class Le>
			static __SEQ_INLINE_LOOKUP std::pair<SizeType, bool> apply(const T* ptr, SizeType size, const U& k, const Le& le)
			{
				SizeType s = 0;
				SizeType e = size;
				if (Multi) {
					bool exact_match = false;
					while (s != e) {
						const SizeType mid = (s + e) >> 1;
						const int c = le.compare(ptr[mid], k);
						if (c < 0) {
							s = mid + 1;
						}
						else {
							e = mid;
							if (c == 0) {
								// Need to return the first value whose key is not less than k,
								// which requires continuing the binary search if this is a
								// multi-container.
								exact_match = true;
							}
						}
					}
					return { s, exact_match };
				}
				else {  // Not a multi-container.
					while (s != e) {
						const SizeType mid = (s + e) >> 1;
						const int c = le.compare(ptr[mid], k);
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
					return { s,false };
				}
			}

		};



		template<bool Multi, class Key, class T, class SizeType, class U, class Le>
		__SEQ_INLINE_LOOKUP std::pair<SizeType, bool> lower_bound(const T* ptr, SizeType size, const U& k, const Le& le)
		{
			return lower_bound_internal<Key, Multi, has_comparable<Le>::value>::apply(ptr, size, k, le);
		}

		template<bool Multi, class Key, class T, class SizeType, class U, class Le>
		__SEQ_INLINE_LOOKUP SizeType upper_bound(const T* ptr, SizeType size, const U& k, const Le& le)
		{
			return lower_bound_internal<Key, Multi, false>::apply(ptr, size, k, [&le](const auto& a, const auto& b) {return !le(b, a); }).first;
		}

	}//end detail
}//end seq
