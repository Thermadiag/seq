#pragma once

#include <type_traits>
#include <algorithm>


#define __SEQ_INLINE_LOOKUP //SEQ_ALWAYS_INLINE


namespace seq
{
	namespace detail
	{

		template<class Key, bool Branchless = std::is_arithmetic<Key>::value >
		struct lower_bound_internal
		{
			template<class T, class SizeType, class U, class Le>
			static __SEQ_INLINE_LOOKUP SizeType apply(const T* ptr, SizeType size, const U& value, const Le& le)
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
				return low;

				/*
				//quaternary search, keep it just in case
				SizeType start = 0, end = size;
				while (end > start + end_of_probe) {
					SizeType size = end - start;
					SizeType midFirst = (start + size / 4); //mid of first and second block
					SizeType midSecond = (start + size / 2); //mid of first and second block
					SizeType midThird = (start + size * 3 / 4);


					if (le( ptr[midThird], value )) {
						start = midThird + 1;
					}
					else if (le(ptr[midSecond],value) ) {
						start = midSecond + 1;
						end = midThird;
					}
					else if (le( ptr[midFirst], value)) {
						start = midFirst + 1;
						end = midSecond;
					}
					else {
						end = midFirst;
					}

				}
				while (start < end && le(ptr[start], value))
					++start;
				return start;*/
			}

		};

		template<class Key>
		struct lower_bound_internal<Key, false>
		{
			template<class T, class SizeType, class U, class Le>
			static __SEQ_INLINE_LOOKUP SizeType apply(const T* ptr, SizeType size, const U& value, const Le& le)
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
				return static_cast<SizeType>(p - ptr);
			}

		};



		template<class Key, bool Branchless = std::is_arithmetic<Key>::value >
		struct upper_bound_internal
		{
			template<class T, class SizeType, class U, class Le>
			static __SEQ_INLINE_LOOKUP SizeType apply(const T* ptr, SizeType size, const U& value, const Le& le)
			{
				// This unrolled and branchless variant is faster than std::upper_bound for arithmetic types.
				// The end_of_probe value is selected so that the linear search part fits within a cache line.

				static constexpr SizeType end_of_probe = (sizeof(T) > 16U ? 8 : (sizeof(T) > 8U ? 16 : 32));
				SizeType low = 0;
				while (size > end_of_probe) {
					SizeType half = size / 2;
					low = le(value, ptr[low + half]) ? low : (low + size - half);
					size = half;

					half = size / 2;
					low = le(value, ptr[low + half]) ? low : (low + size - half);
					size = half;
				}

				size += low;
				for (; low < size; ++low) {
					if (le(value, ptr[low]))
						break;
				}
				return low;
			}

		};

		template<class Key>
		struct upper_bound_internal<Key, false>
		{
			template<class T, class SizeType, class U, class Le>
			static __SEQ_INLINE_LOOKUP SizeType apply(const T* ptr, SizeType size, const U& value, const Le& le)
			{
				return static_cast<SizeType>(std::upper_bound(ptr, ptr + size, value, le) - ptr);
			}

		};

	}//end detail
}//end seq
