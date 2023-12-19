/**
 * MIT License
 *
 * Copyright (c) 2022 Victor Moncada <vtr.moncada@gmail.com>
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

#ifndef SEQ_TINY_STRING_HPP
#define SEQ_TINY_STRING_HPP



 /** @file */



#include <vector>
#include <string>
#include <algorithm>
#include <iterator>
#include <climits>
#include <cstdint>




#include "hash.hpp"
#include "type_traits.hpp"
#include "utils.hpp"


#ifdef SEQ_HAS_CPP_17
#include <string_view>
#endif


#define SEQ_STR_INLINE inline
#define SEQ_STR_INLINE_STRONG SEQ_ALWAYS_INLINE

namespace seq
{

	/// @brief Allocator type for tiny string view
	template<class Char>
	struct view_allocator
	{
		typedef Char value_type;
		typedef Char* pointer;
		typedef const Char* const_pointer;
		typedef Char& reference;
		typedef const Char& const_reference;
		using size_type = size_t;
		using difference_type = ptrdiff_t;
		using propagate_on_container_move_assignment = std::true_type;
		using is_always_equal = std::true_type;

		template <class Other>
		struct rebind {
			using other = view_allocator<Other>;
		};

		constexpr view_allocator() noexcept {}
		constexpr view_allocator(const view_allocator&) noexcept = default;
		template <class Other>
		constexpr view_allocator(const view_allocator<Other>&) noexcept {}
		~view_allocator() = default;
		view_allocator& operator=(const view_allocator&)noexcept = default;

		void deallocate(Char*, const size_t) {}
		Char* allocate(const size_t) { return nullptr; }
		Char* allocate(const size_t, const void*) { return nullptr; }
		size_t max_size() const noexcept { return static_cast<size_t>(-1) / sizeof(Char); }
	};

	// forward declaration
	template<class Char, class Traits, class Allocator, size_t MaxStaticSize>
	class tiny_string;


	template<class Char>
	using basic_tstring_view = tiny_string<Char, std::char_traits<Char>, view_allocator<Char>, 0>;

	/// @brief Base string view typedef, similar to std::string_view.
	using tstring_view = basic_tstring_view<char>;
	using wtstring_view = basic_tstring_view<wchar_t>;

	/// @brief Base string typedef, similar to std::string. Equivalent to tiny_string<0, std::allocator<char>>.
	using tstring = tiny_string<char, std::char_traits<char>, std::allocator<char>, 0 >;
	using wtstring = tiny_string<wchar_t, std::char_traits<wchar_t>, std::allocator<wchar_t>, 0 >;





	namespace detail
	{
		/*-************************************
		*  Common functions
		**************************************/


		//Function missing on msvc and gcc/mingw
		template<class Char>
		inline auto string_memrchr(const Char* s, Char c, size_t n) noexcept -> const Char* {
			const Char* p = s;
			for (p += n; n > 0; n--) {
				if (*--p == c) {
					return (p);
				}
			}

			return nullptr;
		}

		inline auto count_approximate_common_bytes(const void* _pIn, const void* _pMatch, const void* _pInLimit) noexcept -> size_t
		{
			static constexpr size_t stepsize = sizeof(size_t);
			const char* pIn = static_cast<const char*>(_pIn);
			const char* pMatch = static_cast<const char*>(_pMatch);
			const char* pInLimit = static_cast<const char*>(_pInLimit);
			const char* pStart = pIn;

			while (SEQ_LIKELY(pIn < pInLimit - (stepsize - 1))) {
				size_t const diff = read_size_t(pMatch) ^ read_size_t(pIn);
				if (diff == 0u) { pIn += stepsize; pMatch += stepsize; continue; }
				return static_cast<size_t>(pIn - pStart);
			}

			if ((stepsize == 8) && (pIn < (pInLimit - 3)) && (read_32(pMatch) == read_32(pIn))) { pIn += 4; pMatch += 4; }
			if ((pIn < (pInLimit - 1)) && (read_16(pMatch) == read_16(pIn))) { pIn += 2; pMatch += 2; }
			if ((pIn < pInLimit) && (*pMatch == *pIn)) { pIn++; }
			return static_cast<size_t>(pIn - pStart);
		}


		
		template<class Traits, class Char>
		SEQ_STR_INLINE_STRONG int traits_string_compare(const Char* v1, size_t l1, const Char* v2, size_t l2) noexcept
		{
			using unsigned_char = typename std::make_unsigned<Char>::type;

			size_t l = l1 < l2 ? l1 : l2;
			if (l && *v1 != *v2)
				return (static_cast<unsigned_char>(*v1) < static_cast<unsigned_char>(*v2)) ? -1 : 1;

#ifndef SEQ_NO_FAST_BSWAP
			if SEQ_CONSTEXPR(sizeof(Char) == 1)
			{
				const Char* end = v1 + l;
				while ((v1 < end - (8 - 1))) {
					std::uint64_t r1 = read_64(v1);
					std::uint64_t r2 = read_64(v2);
					if (r1 != r2) {
#if SEQ_BYTEORDER_ENDIAN != SEQ_BYTEORDER_BIG_ENDIAN
						r1 = byte_swap_64(r1);
						r2 = byte_swap_64(r2);
#endif
						return r1 < r2 ? -1 : 1;
					}
					v1 += 8; v2 += 8;
				}
				if ((v1 < (end - 3))) {
					std::uint32_t r1 = read_32(v1);
					std::uint32_t r2 = read_32(v2);
					if (r1 != r2) {
#if SEQ_BYTEORDER_ENDIAN != SEQ_BYTEORDER_BIG_ENDIAN
						r1 = byte_swap_32(r1);
						r2 = byte_swap_32(r2);
#endif
						return r1 < r2 ? -1 : 1;
					}
					v1 += 4; v2 += 4;
				}
				if ((v1 < (end - 1))) {
					std::uint16_t r1 = read_16(v1);
					std::uint16_t r2 = read_16(v2);
					if (r1 != r2) {
#if SEQ_BYTEORDER_ENDIAN != SEQ_BYTEORDER_BIG_ENDIAN
						r1 = byte_swap_16(r1);
						r2 = byte_swap_16(r2);
#endif
						return r1 < r2 ? -1 : 1;
					}
					v1 += 2; v2 += 2;
				}
				if (v1 != end) {
					if (*v1 != *v2) { return static_cast<unsigned_char>(*v1) < static_cast<unsigned_char>(*v2) ? -1 : 1; }
				}
				return (l1 < l2 ? -1 : (l1 > l2 ));
			}
#endif


			const int r = Traits::compare(v1, v2, l);
			return r != 0 ? r : (l1 < l2 ? -1 : (l1 > l2));
		}
		template<class Traits, class Pair>
		SEQ_STR_INLINE_STRONG int traits_string_compare(const Pair& p1, const Pair& p2) noexcept
		{
			return traits_string_compare<Traits>(p1.first, p1.second, p2.first, p2.second);
		}
		


		template<class Traits, class Char>
		SEQ_STR_INLINE_STRONG bool traits_string_inf(const Char* v1, size_t l1, const Char* v2, size_t l2) noexcept
		{
			return traits_string_compare<Traits>(v1, l1, v2, l2) < 0;
		}
		template<class Traits, class Pair>
		SEQ_STR_INLINE_STRONG bool traits_string_inf(const Pair & p1, const Pair & p2) noexcept
		{
			return traits_string_compare<Traits>(p1.first, p1.second, p2.first, p2.second) < 0;
		}
		
		template<class Traits, class Char>
		SEQ_STR_INLINE bool traits_string_sup(const Char* v1, size_t l1, const Char* v2, size_t l2) noexcept
		{
			return traits_string_inf<Traits>(v2, l2, v1, l1);
		}
		template<class Traits, class Pair>
		SEQ_STR_INLINE bool traits_string_sup(const Pair& p1, const Pair& p2) noexcept
		{
			return traits_string_inf<Traits>(p2.first, p2.second, p1.first, p1.second);
		}


		template<class Traits, class Char>
		SEQ_STR_INLINE bool traits_string_inf_equal(const Char* v1, size_t l1, const Char* v2, size_t l2) noexcept
		{
			return !traits_string_inf<Traits>(v2, l2, v1, l1);
		}
		template<class Traits, class Pair>
		SEQ_STR_INLINE bool traits_string_inf_equal(const Pair& p1, const Pair& p2) noexcept
		{
			return traits_string_inf_equal<Traits>(p1.first, p1.second, p2.first, p2.second);
		}

		template<class Traits, class Char>
		SEQ_STR_INLINE bool traits_string_sup_equal(const Char* v1, size_t l1, const Char* v2, size_t l2) noexcept
		{
			return traits_string_inf_equal<Traits>(v2, l2, v1, l1);
		}
		template<class Traits, class Pair>
		SEQ_STR_INLINE bool traits_string_sup_equal(const Pair& p1, const Pair& p2) noexcept
		{
			return traits_string_sup_equal<Traits>(p1.first, p1.second, p2.first, p2.second);
		}


		template<class Traits, class Char>
		SEQ_STR_INLINE bool traits_string_equal(const Char* v1, size_t l1, const Char* v2, size_t l2) noexcept
		{
			return (l1 == l2) && traits_string_compare<Traits>(v1, l1, v2, l2) == 0;
		}
		template<class Traits, class Pair>
		SEQ_STR_INLINE bool traits_string_equal(const Pair& p1, const Pair& p2) noexcept
		{
			return traits_string_equal<Traits>(p1.first, p1.second, p2.first, p2.second);
		}

		template<class Traits, class Char>
		inline auto traits_string_find_first_of(const Char* data, size_t pos, size_t size, const Char* s, size_t n, size_t npos) noexcept -> size_t
		{
			if (pos >= size)
				return npos;
			const Char* end = data + size;
			if (sizeof(Char) > 1 || size * n < 256U)
			{
				const Char* send = s + n;
				for (const Char* p = data + pos; p != end; ++p)
					for (const Char* m = s; m != send; ++m)
						if (*m == *p) return  static_cast<size_t>(p - data);
			}
			else {
				unsigned char buff[256 / CHAR_BIT];
				memset(buff, 0, sizeof(buff));
				for (size_t i = 0; i < n; ++i) {
					unsigned char v = static_cast<unsigned char>(s[i]);
					buff[v / CHAR_BIT] |= 1U << (v % CHAR_BIT);
				}
				for (const Char* p = data + pos; p != end; ++p) {
					unsigned char v = static_cast<unsigned char>(*p);
					if ((buff[v / CHAR_BIT] >> (v % CHAR_BIT)) & 1U)
						return  static_cast<size_t>(p - data);
				}

			}
			return npos;
		}

		template<class Traits, class Char>
		inline auto traits_string_find_last_of(const Char* data, size_t pos, size_t size, const Char* s, size_t n, size_t npos) noexcept -> size_t
		{
			if (size == 0) return npos;
			if (pos >= size) pos = size - 1;
			const Char* p = data;
			if (sizeof(Char) > 1 || size * n < 256U)
			{
				const Char* send = s + n;
				for (const Char* in = p + pos; in >= p; --in) {
					for (const Char* m = s; m != send; ++m)
						if (*m == *in) return  static_cast<size_t>(in - p);
				}
			}
			else
			{
				unsigned char buff[256 / CHAR_BIT];
				memset(buff, 0, sizeof(buff));
				for (size_t i = 0; i < n; ++i) {
					unsigned char v = static_cast<unsigned char>(s[i]);
					buff[v / CHAR_BIT] |= 1U << (v % CHAR_BIT);
				}
				for (const Char* in = p + pos; in >= p; --in) {
					unsigned char v = static_cast<unsigned char>(*in);
					if ((buff[v / CHAR_BIT] >> (v % CHAR_BIT)) & 1U)
						return  static_cast<size_t>(in - p);
				}
			}
			return npos;
		}

		template<class Traits, class Char>
		inline auto traits_string_find_first_not_of(const Char* data, size_t pos, size_t size, const Char* s, size_t n, size_t npos) noexcept -> size_t
		{
			const Char* end = data + size;
			const Char* send = s + n;
			for (const Char* p = data + pos; p != end; ++p) {
				const Char* m = s;
				for (; m != send; ++m)
					if (*m == *p) break;
				if (m == send)
					return static_cast<size_t>(p - data);
			}
			return npos;
		}

		template<class Traits, class Char>
		inline auto traits_string_find_last_not_of(const Char* data, size_t pos, size_t size, const Char* s, size_t n, size_t npos) noexcept -> size_t
		{
			if (size == 0) return npos;
			if (pos >= size) pos = size - 1;
			const Char* p = data;
			const Char* send = s + n;
			for (const Char* in = p + pos; in >= p; --in) {
				const Char* m = s;
				for (; m != send; ++m)
					if (*m == *in) break;
				if (m == send)
					return static_cast<size_t>(in - p);
			}
			return npos;
		}

		template<class Traits, class Char>
		inline auto traits_string_find(const Char* data, size_t pos, size_t size, const Char* s, size_t n, size_t npos) noexcept -> size_t
		{
			if (n > size || pos + n > size )
				return npos;
			if (n == 0)
				return 0;

			const Char* in = data + pos;
			const Char* end = in + (size - pos - n) + 1;
			Char c = *s;
			for (;;) {
				in = Traits::find(in, static_cast<size_t>(end - in), c);
				if (!in) return npos;

				//start searching, count_approximate_common_bytes returns (usually) an underestimation of the common bytes, except if equal
				size_t common = detail::count_approximate_common_bytes(in + 1, s + 1, in + n);
				if (common == (n - 1) * sizeof(Char))
					return  static_cast<size_t>(in - data);
				//in += common +1;
				++in;
			}
			return npos;
		}

		template<class Traits, class Char>
		inline auto traits_string_rfind(const Char* data, size_t pos, size_t size, const Char* s, size_t n, size_t npos) noexcept -> size_t
		{
			if (n > size || pos < n || n == 0) return npos;
			if (pos > size) pos = size;
			const Char* beg = data;
			const Char* in = std::min(beg + pos, data + size - n);
			Char c = *s;
			for (;;) {
				in = string_memrchr(beg, c, static_cast<size_t>(in - beg + 1));
				if (!in) return npos;
				//start searching
				size_t common = detail::count_approximate_common_bytes(in + 1, s + 1, in + n);
				if (common == (n - 1) * sizeof(Char))
					return  static_cast<size_t>(in - data);
				--in;
			}
			return npos;
		}







		template<class Char, size_t MaxSSO >
		struct SSOStorage
		{
			static constexpr size_t sizeof_word = sizeof(std::uintptr_t);
			static constexpr size_t sso_size_bytes = sizeof(Char) * MaxSSO;
			static constexpr size_t sso_struct_bytes = ((sso_size_bytes / sizeof_word + (sso_size_bytes % sizeof_word ? 1 : 0)) * sizeof_word);
			static constexpr size_t struct_bytes = sso_struct_bytes < sizeof_word * 2 ? sizeof_word * 2 : sso_struct_bytes;
			static constexpr size_t sso_capacity = struct_bytes / sizeof(Char);
			Char data[sso_capacity];
		};

		template<class Char>
		struct NoneSSOStorage
		{
			Char* data;
			std::uintptr_t size;
		};


		template<class Char, size_t MaxSSOCapacity>
		struct StringHolder
		{

			/// All information regarding sso or not and sso size is stored in the last sso character
			/// If 0 : SSO with maximum size
			/// If last character bit is set to 1: SSO, and size is set in the lower bits
			/// If last character bit is set to 0 and next bit is 1: non SS0
			/// 
			using unsigned_char = typename std::make_unsigned<Char>::type;

			// Compute maximum allowed SSO capacity
			static constexpr size_t _max_allowed_sso_capacity = (1ULL << (sizeof(unsigned_char) * 8ULL - 1ULL)) - 1ULL;
			static constexpr size_t max_allowed_sso_capacity = (_max_allowed_sso_capacity / sizeof(std::uintptr_t)) * sizeof(std::uintptr_t);
			static constexpr size_t max_sso_capacity = MaxSSOCapacity > max_allowed_sso_capacity ? max_allowed_sso_capacity : MaxSSOCapacity;

#if SEQ_BYTEORDER_ENDIAN == SEQ_BYTEORDER_LITTLE_ENDIAN
			static constexpr std::uintptr_t sso_capacity = SSOStorage<Char, max_sso_capacity>::sso_capacity;
			// Last SSO character overlapp with the size member of non SSO struct
			static constexpr bool last_character_overlapp = sizeof(SSOStorage<Char, sso_capacity>) == sizeof(NoneSSOStorage< Char>);

#else
			static constexpr size_t min_capacity_for_BE = SSOStorage<Char, sizeof(NoneSSOStorage< Char>) / sizeof(Char) + 1U>::sso_capacity;
			static constexpr std::uintptr_t _sso_capacity = SSOStorage<Char, max_sso_capacity>::sso_capacity;
			static constexpr std::uintptr_t sso_capacity = _sso_capacity < min_capacity_for_BE ? min_capacity_for_BE : _sso_capacity;
			static constexpr bool last_character_overlapp = false;

#endif


			// masks to extract last bit(s) of last character
			static constexpr unsigned_char sso_mask = sizeof(unsigned_char) == 8U ? (1ULL << 63ULL) : (1ULL << (sizeof(unsigned_char) * 8ULL - 1ULL));
			static constexpr unsigned_char non_sso_mask = sso_mask >> 1ULL;
			static constexpr unsigned_char check_sso_mask = sso_mask | non_sso_mask;
			static constexpr std::uintptr_t max_capacity = last_character_overlapp ? (std::numeric_limits<size_t>::max() >> 2ULL) : std::numeric_limits<size_t>::max();

			static constexpr std::uintptr_t non_sso_flag = (1ULL << (sizeof(std::uintptr_t) * 8U - 2U));
			static constexpr size_t char_offset = sizeof(size_t) / sizeof(Char);

			union StorageUnion {
				SSOStorage<Char, sso_capacity> sso;
				NoneSSOStorage< Char> non_sso;
			} d_union;

			StringHolder() noexcept {
				reset();
			}
			template<class Union>
			StringHolder(const Union & u) noexcept 
			//:d_union(u){
			{
				memcpy(&d_union, &u, sizeof(d_union));
				//avoid a reset operation
			}

			SEQ_STR_INLINE_STRONG unsigned_char last_sso_char() const noexcept {
				return static_cast<unsigned_char>(d_union.sso.data[sso_capacity - 1]);
			}
			SEQ_STR_INLINE_STRONG unsigned_char& last_sso_char() noexcept {
				return reinterpret_cast<unsigned_char*>(d_union.sso.data)[sso_capacity - 1];
			}

			SEQ_STR_INLINE_STRONG bool is_sso() const noexcept
			{
				return (last_sso_char() & check_sso_mask) != non_sso_mask ;
			}
			static SEQ_STR_INLINE bool is_sso(std::uintptr_t size) noexcept
			{
				return size < sso_capacity;
			}
			SEQ_STR_INLINE std::uintptr_t sizeSSO() const noexcept
			{
				return (last_sso_char() == 0 ? sso_capacity - 1U : last_sso_char() & (sso_mask - 1U));
			}
			SEQ_STR_INLINE std::uintptr_t sizeNonSSO() const noexcept
			{
				return (last_character_overlapp ? (d_union.non_sso.size & (non_sso_flag - 1ULL)) : d_union.non_sso.size);
			}
			SEQ_STR_INLINE_STRONG std::uintptr_t size(unsigned_char last) const noexcept
			{
				if ((last & check_sso_mask) != non_sso_mask) return last == 0 ? (sso_capacity - 1U) : (last & (sso_mask - 1U));
				if (last_character_overlapp) return (d_union.non_sso.size & (non_sso_flag - 1ULL));
				return d_union.non_sso.size;
			}
			SEQ_STR_INLINE_STRONG std::uintptr_t size() const noexcept
			{
				return size(last_sso_char());
				//return is_sso() ?
				//	(last_sso_char() == 0 ? sso_capacity - 1U : last_sso_char() & (sso_mask - 1U)) :
				//	(last_character_overlapp ? (d_union.non_sso.size & (non_sso_flag - 1ULL)) : d_union.non_sso.size);
			}
			SEQ_STR_INLINE_STRONG const Char* data() const noexcept
			{
				return is_sso() ? d_union.sso.data : (d_union.non_sso.data + char_offset);
			}
			SEQ_STR_INLINE_STRONG Char* data() noexcept
			{
				return is_sso() ? d_union.sso.data : (d_union.non_sso.data + char_offset);
			}

			SEQ_STR_INLINE_STRONG std::pair<const Char*, size_t> as_pair(const unsigned_char last) const noexcept {
				if ((last & check_sso_mask) != non_sso_mask)
					return{ d_union.sso.data , (last == 0 ? sso_capacity - 1U : last & (sso_mask - 1U)) };
				return { (d_union.non_sso.data + char_offset), static_cast<size_t>((last_character_overlapp ? (d_union.non_sso.size & (non_sso_flag - 1ULL)) : d_union.non_sso.size)) };
			}
			SEQ_STR_INLINE_STRONG std::pair<const Char*, size_t> as_pair() const noexcept {
				return as_pair(last_sso_char());
			}

			SEQ_STR_INLINE void setSizeNonSSO(std::uintptr_t size) noexcept
			{
				if (last_character_overlapp)
					// last character overlapp with size member: set last 2 bits to '01'
					d_union.non_sso.size = size | non_sso_flag;
				else {
					// no overlapp: just store the size
					d_union.non_sso.size = size;
					// set last character to '01....'
					last_sso_char() = non_sso_mask;
				}
			}
			SEQ_STR_INLINE bool setSize(std::uintptr_t size) noexcept
			{
				if (size == sso_capacity - 1) {
					last_sso_char() = 0;
					return true;
				}
				else if (size < sso_capacity - 1) {
					last_sso_char() = static_cast<unsigned_char>(sso_mask | size);
					return true;
				}
				else
				{
					setSizeNonSSO(size);
					return false;
				}
			}

			SEQ_STR_INLINE void setSizeKeepSSOFlag(std::uintptr_t size) noexcept
			{
				if (!is_sso())
					setSizeNonSSO(size);
				else if (size == sso_capacity - 1) 
					last_sso_char() = 0;
				else  
					last_sso_char() = static_cast<unsigned_char>(sso_mask | size);
			}

			SEQ_STR_INLINE_STRONG void reset() noexcept
			{
				// Initialize with SSO string of size 0
				last_sso_char() = static_cast<unsigned_char>(sso_mask);
			}
		};



		template<class Char, size_t MaxStaticSize, class Allocator>
		struct string_internal : StringHolder<Char, MaxStaticSize  + 1U>, private Allocator
		{
			using base = StringHolder<Char, MaxStaticSize + 1U>;

			static constexpr size_t max_sso_capacity = base::sso_capacity;
			static constexpr size_t max_sso_size = base::sso_capacity - 1U;
			static constexpr size_t max_allowed_sso_capacity = base::max_allowed_sso_capacity;
			static constexpr size_t max_capacity = base::max_capacity;

			string_internal() noexcept(std::is_nothrow_default_constructible<Allocator>::value) 
				:base(), Allocator() { }
			string_internal(const Allocator& al) noexcept(std::is_nothrow_copy_constructible<Allocator>::value) 
				:base(), Allocator(al) { }
			string_internal(Allocator&& al)noexcept(std::is_nothrow_move_constructible<Allocator>::value) 
				:base(), Allocator(std::move(al)) { }
			string_internal(Allocator&& al, const string_internal & other)noexcept(std::is_nothrow_move_constructible<Allocator>::value)
				:base(other.d_union), Allocator(std::move(al)) { }
			auto allocate(size_t n) -> Char* {
				return this->Allocator::allocate(n);
			}
			void deallocate(Char* p, size_t n) {
				this->Allocator::deallocate(p, n);
			}
			auto get_allocator() noexcept -> Allocator& { return *this; }
			auto get_allocator() const noexcept -> const Allocator& { return *this; }
			
			SEQ_STR_INLINE_STRONG void swap(string_internal& other) noexcept (noexcept(swap_allocator(std::declval<Allocator&>(), std::declval<Allocator&>())))
			{
				if SEQ_CONSTEXPR(!std::is_same<std::allocator<Char>, Allocator>::value)
					swap_allocator(get_allocator(), other.get_allocator());
				std::swap(this->d_union, other.d_union);
			}

		};


		template<class Char, size_t MaxStaticSize>
		struct string_internal<Char, MaxStaticSize, view_allocator<Char> >
		{
			// internal storage for tiny_string and view_allocator (tstring_view objects)
			const Char* data;
			size_t size;

			string_internal(const Char* d = nullptr, size_t s = 0) noexcept
				:data(d), size(s) {}

			auto allocate(size_t n) -> Char* {
				throw std::bad_alloc();
				return NULL;
			}
			void deallocate(Char*  /*unused*/, size_t  /*unused*/) noexcept {}
			auto get_allocator() const noexcept -> view_allocator<Char> { return view_allocator<Char>(); }
			
			SEQ_STR_INLINE_STRONG void swap(string_internal& other) noexcept
			{
				std::swap(data, other.data);
				std::swap(size, other.size);
			}

			SEQ_STR_INLINE_STRONG std::pair<const Char*, size_t> as_pair() const noexcept {
				return { data,size };
			}
		};




	}



	/// @brief String class with a similar interface and requirements than std::string
	/// @tparam MaxStaticSize maximum static size for Small String Optimization
	/// @tparam Allocator allocator type
	/// 
	/// 
	/// seq::tiny_string is a string class similar to std::string but aiming at greater performances when used in containers.
	/// It provides a customizable Small String Optimization (SSO) much like boost::small_vector, where the maximum static size
	/// before an allocation is triggered is defined at compile time.
	/// 
	/// seq::tiny_string contains some preallocated elements in-place, which can avoid the use of dynamic storage allocation when 
	/// the actual number of elements is below that preallocated threshold (MaxStaticSize parameter).
	/// 
	/// seq::tiny_string only supports characters of type char, not wchar_t.
	/// 
	/// 
	/// Motivation
	/// ----------
	/// 
	/// Why another string class? I started writing tiny_string to provide a small and relocatable string class that can be used within seq::cvector.
	/// Indeed, gcc std::string implementation is not relocatable as it stores a pointer to its internal data for small strings. In addition, most std::string implementations have a size of 32 bytes 
	/// (at least msvc and gcc ones), which was a lot for my compressed container. Therefore, I started working on a string implementation with the following specificities:
	/// -	Relocatable,
	/// -	Small footprint (16 bytes on 64 bits machine if possible),
	/// -	Customizable Small String Optimization (SSO),
	/// -	Higly optimized for small strings (fast copy/move assignment and fast comparison operators).
	/// 
	/// It turns out that such string implementation makes all flat containers (like std::vector or std::deque) faster (at least for small strings) as it greatly reduces cache misses.
	/// The Customizable Small String Optimization is also very convenient to avoid unnecessary memory allocations for different workloads.
	/// 
	/// 
	/// Size and bookkeeping
	/// --------------------
	/// 
	/// By default, a tiny_string contains enough room to store a 15 bytes string, therefore a length of 14 bytes for null terminated strings.
	/// For small strings (below the preallocated threshold), tiny_string only store one additional byte for bookkeeping: 7 bits for string length
	/// and 1 bit to tell if the string is allocated in-place or on the heap. It means that the default tiny_string size is 16 bytes, which is half
	/// the size of std::string on gcc and msvc. This small footprint is what makes tiny_string very fast on flat containers like std::vector ot std::deque,
	/// while node based container (like std::map) are less impacted. Note that this tiny size is only reach when using an empty allocator class like std::allocator<char>. 
	/// 
	/// When the tiny_string grows beyong the preallocated threshold, memory is allocated on the heap based on provided allocator, and the bookkeeping part is divided as follow:
	///		-	still 1 bit to tell is the memory is heap allocated or not,
	///		-	1 bit to tell if the string capacity is exactly equal to the string size + 1 (as tiny_string is always null terminated),
	///		-	sizeof(size_t)*8 - 2 bits to store the string size. Therefore, the maximum size of tiny_string is slightly lower than std::string.
	///		-	a pointer (4 to 8 bytes) to the actual memory chunk.
	/// 
	/// seq::tiny_string does not store the memory capacity, and always use a grow factor of 2. The capacity is always deduced from the string length 
	/// using compiler intrinsics (like _BitScanReverse on msvc). In some cases (like copy construction), the allocated capacity is the same as the string length,
	/// in which case a 1 bit flag is set to track this information.
	/// 
	/// The global typedef seq::tstring is provided for convenience, and is equivalent to seq::tiny_string<0,std::allocator<char>>.
	/// 
	/// Static size
	/// -----------
	/// 
	/// The maximum preallocated space is specified as a template parameter (MaxStaticSize).
	/// By default, this value is set to 0, meaning that the tiny_string only uses 2 word of either 32 or 64 bits depending on the architecture.
	/// Therefore, the maximum in-place capacity is either 7 or 15 bytes.
	/// 
	/// The maximum preallocated space can be increased up to 126 bytes. To have a tiny_string of 32 bytes like std::string on gcc and msvc, you could use, for instance, tiny_string<28>.
	/// In this case, the maximum string size (excluding null-terminated character) to use SSO would be 30 bytes (!).
	/// 
	/// 
	/// Relocatable type
	/// ----------------
	/// 
	/// seq::tiny_string is relocatable, meaning that it does not store pointer to internal data.
	/// Relocatable types can be used more efficiently in containers that are aware of this property. For instance, seq::devector,
	/// seq::tiered_vector and seq::flat_map are faster when working with relocatable types, as the process to move one object from a memory layout
	/// about to be destroyed to a new one can be accomplished through a simple memcpy.
	/// 
	/// Msvc implementation of std::string is also relocatable, while gcc implementation is not as it stores a pointer to its internal data
	/// for small strings.
	/// 
	/// Within the seq library, a relocatable type must statify seq::is_relocatable<type>::value == true.
	/// 
	/// Note that tiny_string is only relocatable if the allocator itself is relocatable (which is the case for the default std::allocator<char>).
	/// 
	/// Interface
	/// ---------
	/// 
	/// seq::tiny_string provides the same interface as std::string.
	/// Functions working on other strings like find(), compare()... are overloaded to also work on std::string.
	/// The comparison operators are also overloaded to work with std::string.
	/// 
	/// seq::tiny_string provides the following additional members for convenience:
	///		-	join(): merge several strings with a common separator
	///		-	split(): split string based on separator
	///		-	replace(): replace a string by another one
	///		-	convert(): convert the string to another type using streams
	///		-	sprintf(): format string
	/// 
	/// seq::tiny_string also works with std::istream/std::ostream exactly like std::string.
	/// A specialization of std::hash is provided for tiny_string types which relies on murmurhash2. This specialization is transparent and
	/// supports hashing std::string, tiny_string and const char*.
	/// 
	/// seq::tiny_string provides the same invalidation rules as std::string as well as the same exception guarantees.
	/// 
	/// The main difference compared to std::string is memory deallocation. As tiny_string does not store the capacity internally,
	/// its capacity is deduced from the size and must be the closest greater or equal power of 2 (except for a few situations where the capacity is excatly the length +1).
	/// Therefore, tiny_string must release memory when its size decreases due, for instance, to calls to tiny_string::pop_back().
	/// Likewise, shrink_to_fit() and reserve() are no-ops.
	/// 
	/// 
	/// String view
	/// -----------
	/// 
	/// seq::tiny_string is specialized for seq::view_allocator in order to provide a std::string_view like class.
	/// It is provided for compilers that do not support (yet) std::string_view, and provides a similar interface.
	/// 
	/// The global typedef seq::tstring_view is provided  for convenience, and is equivalent to seq::tiny_string<0,seq::view_allocator>.
	/// tstring_view is also hashable and can be compared to other tiny_string types as well as std::string.
	/// 
	/// 
	/// Performances
	/// ------------
	/// 
	/// All tiny_string members have been optimized to match or outperform (for small strings) most std::string implementations. They have been benchmarked against
	/// gcc (10.0.1) and msvc (14.20) for members compare(), find(), rfind(), find_first_of(), find_last_of(), find_first_not_of() and find_last_not_of().
	/// Comparison operators <=> are usually faster than std::string.
	/// 
	/// However, tiny_string is usually slower for back insertion with push_back(). The pop_back() member is also slower than msvc and gcc implementations.
	/// 
	/// tiny_string is usually faster when used inside flat containers simply because its size is smaller than std::string (32 bytes on gcc and msvc).
	/// The following table shows the performances of tiny_string against std::string for sorting a vector of 1M random short string (size = 14, where both tiny_string
	/// and std::string use SSO) and 1M random longer strings (size = 200, both use heap allocation). Tested with gcc 10.1.0 (-O3) for msys2 on Windows 10,
	/// using Intel(R) Core(TM) i7-10850H at 2.70GHz.
	/// 
	/// String class       | sort small (std::less) | sort small (tstring::less) | sort wide (std::less) | sort wide (tstring::less) |
	/// -------------------|------------------------|----------------------------|-----------------------|---------------------------|
	/// std::string        |          160 ms        |          122 ms            |       382 ms          |         311 ms            |
	/// seq::tiny_string   |          112 ms        |          112 ms            |       306 ms          |         306 ms            |
	/// 
	/// This benchmark is available in file 'seq/benchs/bench_tiny_string.hpp'.
	/// Note that tiny_string always uses its own comparison function. We can see that the comparison function of tiny_string is faster than the default one used
	/// by std::string. Even when using tiny_string comparator, std::string remains slightly slower due to the tinier memory footprint of tiny_string.
	/// 
	/// The difference is wider on msvc (14.20):
	/// 
	/// String class       | sort small (std::less) | sort small (tstring::less) | sort wide (std::less) | sort wide (tstring::less) |
	/// -------------------|------------------------|----------------------------|-----------------------|---------------------------|
	/// std::string        |          281 ms        |          264 ms            |       454 ms          |         441 ms            |
	/// seq::tiny_string   |          176 ms        |          176 ms            |       390 ms          |         390 ms            |
	///
	/// 
	/// 
	template<class Char, class Traits = std::char_traits<Char>, class Allocator = std::allocator<Char>, size_t MaxStaticSize = 0>
	class tiny_string
	{
		static_assert(sizeof(Char) == alignof(Char),"invalid Char type alignment");
		static_assert((sizeof(size_t) % sizeof(Char)) == 0, "invalid Char type size");
		static_assert(sizeof(Char) <= alignof(size_t), "invalid Char type size");


		using internal_data = detail::string_internal<Char, MaxStaticSize, Allocator>;
		using this_type = tiny_string<Char, Traits, Allocator, MaxStaticSize>;

		static constexpr size_t sso_max_capacity = internal_data::max_sso_capacity;
		static constexpr size_t sso_max_size = internal_data::max_sso_size;
		static constexpr size_t max_allowed_sso_capacity = internal_data::max_allowed_sso_capacity;
		static constexpr size_t first_allocated_capacity = (1ULL << (static_bit_scan_reverse< sso_max_capacity>::value + 1U));//TODO: check value
		static constexpr size_t char_offset = sizeof(size_t) / sizeof(Char);

		internal_data d_data;

		template<class C, class T, class Al, size_t S>
		friend class tiny_string;

		//is it a small string
		SEQ_STR_INLINE bool is_sso() const noexcept {return d_data.is_sso();}
		SEQ_STR_INLINE bool is_sso(size_t len) const noexcept { return d_data.is_sso(len); }

		
		SEQ_STR_INLINE auto capacity_internal() const noexcept -> size_t {
			// returns the capacity
			return is_sso() ?
				sso_max_capacity :
				//read_size_t(d_data.d_union.non_sso.data);
				*reinterpret_cast<const size_t*>(d_data.d_union.non_sso.data);
		}
		SEQ_STR_INLINE auto capacity_for_length(size_t len) const noexcept -> size_t {
			//returns the capacity for given length
			return is_sso(len) ?
				sso_max_capacity :
				(len < first_allocated_capacity ?
					first_allocated_capacity :
					(1ULL << (1ULL + (bit_scan_reverse_64(len)))));
		}

		SEQ_STR_INLINE_STRONG Char* resize_grow(size_t len, bool exact_size = false)
		{
			// Resize for growing, keep old data
			if (SEQ_UNLIKELY(is_sso())) {
				if (SEQ_UNLIKELY(len > sso_max_size)) {
					internal_resize(len, true, exact_size);
					return d_data.d_union.non_sso.data + char_offset;
				}
				else {
					d_data.setSize(len);
					return d_data.d_union.sso.data;
				}
			}
			else {
				if (SEQ_UNLIKELY(len > *reinterpret_cast<const size_t*>(d_data.d_union.non_sso.data) - 1))
					internal_resize(len, true, exact_size);
				else
					d_data.setSizeNonSSO(len);
				return d_data.d_union.non_sso.data + char_offset;
			}
		}

		void internal_resize(size_t len, bool keep_old, bool exact_size = false)
		{
			// Resize string without initializing memory.
			// Take care of switch from sso to non sso (and opposite).
			// Keep old data if keep_old is true.
			// Allocate the exact amount of memory if exact_size is true.
			// Always null terminate new string.

			const size_t old_size = size();
			const bool sso = is_sso();
			if (len == old_size)
				return;

			// Avoid reallocating if we have enough capacity
			if (sso) {
				if (len <= sso_max_size) {
					// Just set the size and last char to 0
					d_data.setSize(len);
					d_data.d_union.sso.data[len] = 0;
					return;
				}
			}
			else {
				if (len <= *reinterpret_cast<const size_t*>(d_data.d_union.non_sso.data) - 1) {
					// Just set the size and last char to 0
					d_data.setSizeNonSSO(len);
					d_data.d_union.non_sso.data[len + char_offset] = 0;
					return;
				}
			}

			if (is_sso(len)) {

				//from non sso to sso
				Char* ptr = d_data.d_union.non_sso.data;
				if (keep_old)
					memcpy(d_data.d_union.sso.data, ptr + char_offset , len * sizeof(Char));
				d_data.deallocate(ptr, capacity_internal() + char_offset);
			}
			//non sso new len, might throw
			else {
				//from sso to non sso

				size_t capacity = exact_size ? len + 1 : capacity_for_length(len);
				Char* ptr = d_data.allocate(capacity + char_offset);
				// write capacity
				write_size_t(ptr, capacity);
				// write previous content
				if (keep_old)
					memcpy(ptr + char_offset, data(), size() * sizeof(Char));
				// deallocate previous if necessary
				if(!sso)
					d_data.deallocate(d_data.d_union.non_sso.data, capacity_internal() + char_offset);
				d_data.d_union.non_sso.data = ptr;
				// null terminate
				d_data.d_union.non_sso.data[len + char_offset] = 0;
			}
			d_data.setSize(len);
		}

		template<class Iter, class Cat>
		void assign_cat(Iter first, Iter last, Cat /*unused*/)
		{
			// assign range for non random access iterators
			tiny_string tmp;
			while (first != last) {
				tmp.push_back(*first);
				++first;
			}
			*this = std::move(tmp);
		}
		template<class Iter>
		void assign_cat(Iter first, Iter last, std::random_access_iterator_tag /*unused*/)
		{
			// assign range for random access iterators
			internal_resize(static_cast<size_t>(last - first), false, true);
			std::copy(first, last, data());
		}

		template<class Iter, class Cat>
		void insert_cat(size_t pos, Iter first, Iter last, Cat /*unused*/)
		{
			// insert range for non random access iterators
			SEQ_ASSERT_DEBUG(pos <= size(), "invalid insert position");
			if (first == last)
				return;

			// push back, might throw
			size_type prev_size = size();
			for (; first != last; ++first)
				push_back(*first);
			// Might throw, fine
			std::rotate(begin() + pos, begin() + prev_size, end());
		}

		template<class Iter>
		void insert_cat(size_t pos, Iter first, Iter last, std::random_access_iterator_tag /*unused*/)
		{
			// insert range for random access iterators
			SEQ_ASSERT_DEBUG(pos <= size(), "invalid insert position");
			if (first == last)
				return;

			if (pos < size() / 2) {
				size_type to_insert = static_cast<size_t>(last - first);
				// Might throw, fine
				resize_front(size() + to_insert);
				iterator beg = begin();
				// Might throw, fine
				Char* p = data();
				std::for_each(p + to_insert, p + to_insert + pos, [&](Char v) {*beg = v; ++beg; });
				std::for_each(p + pos, p + pos + to_insert, [&](Char& v) {v = *first++; });
			}
			else {
				// Might throw, fine
				size_type to_insert = static_cast<size_t>(last - first);
				resize(size() + to_insert);
				Char* p = data();
				size_t s = size();
				std::copy_backward(p + pos, p + (s - to_insert), p + s);
				std::copy(first, last, p + pos);
			}
		}

		void erase_internal(size_t first, size_t last)
		{
			SEQ_ASSERT_DEBUG(first <= last, "erase: invalid positions");
			SEQ_ASSERT_DEBUG(last <= size(), "erase: invalid last position");
			if (first == last) return;
			size_type space_before = first;
			size_type space_after = size() - last;
			size_type s = size();
			Char* p = data();
			if (space_before < space_after) {
				std::copy_backward(p, p + first, p + last);
				resize_front(s - (last - first));
			}
			else {
				std::copy(p + last, p + s, p + first);
				resize(s - (last - first));
			}
		}


		template <class Iter>
		void replace_cat(size_t pos, size_t len, Iter first, Iter last, std::random_access_iterator_tag /*unused*/)
		{
			size_t input_size = (last - first);
			size_t new_size = size() - len + input_size;
			if (new_size <= capacity) {
				//do everything inplace
				if (input_size != len)
					memmove(data() + pos + input_size, data() + pos + len, size() - (pos + len));

				// copy input
				std::copy(first, last, data() + pos);

				data()[new_size] = 0;
				d_data.setSizeKeepSSOFlag(new_size);
				return;
			}

			//otherwise, create a new string and fill it (might throw)
			tiny_string other(new_size, 0);
			Char* op = other.data();
			//first part
			memcpy(op, data(), pos * sizeof(Char));
			//middle part
			std::copy(first, last, op + pos);
			//last part
			memcpy(op + pos + input_size, data() + pos + len, (size() - (pos + len)) * sizeof(Char));

			swap(other);
		}
		template <class Iter, class Cat>
		void replace_cat(size_t pos, size_t len, Iter first, Iter last, Cat c)
		{
			//otherwise, create a new string and fill it
			tiny_string other;
			//first part
			other.append(data(), pos);
			//middle part
			other.append(first, last);
			//last part
			other.append(data() + pos + len, size() - (pos + len));
			swap(other);
		}
		template <class Iter>
		void replace_internal(size_t pos, size_t len, Iter first, Iter last)
		{
			// replace portion of the string
			replace_cat(pos, len, first, last, typename std::iterator_traits<Iter>::iterator_category());
		}

		auto initialize(size_t size) -> Char*
		{
			// intialize the string for given size
			if (!d_data.setSize(size)) {
				// non sso
				try {
					d_data.d_union.non_sso.data = d_data.allocate(size + char_offset + 1);
					write_size_t(d_data.d_union.non_sso.data, size + 1);
					d_data.d_union.non_sso.data[size + char_offset] = 0;
					return d_data.d_union.non_sso.data + char_offset;
				}
				catch (...) {
					d_data.reset();
					throw;
				}
			}
			else
				d_data.d_union.sso.data[size] = 0;
			return d_data.d_union.sso.data;
		}


	public:
		static_assert(MaxStaticSize < max_allowed_sso_capacity, "tiny_string maximum static size is limited to 126 elements");

		// Maximum string length to use SSO
		static constexpr size_t max_allowed_static_size = max_allowed_sso_capacity - 1;
		static constexpr size_t max_static_size = sso_max_capacity - 1;
		static constexpr size_t npos = static_cast<size_t>(-1);

		using traits_type = Traits;
		using value_type = Char;
		using reference = Char&;
		using pointer = Char*;
		using const_reference = const Char&;
		using const_pointer = const Char*;
		using iterator = Char*;
		using const_iterator = const Char*;
		using reverse_iterator = std::reverse_iterator<iterator>;
		using const_reverse_iterator = std::reverse_iterator<const_iterator>;
		using size_type = size_t;
		using difference_type = std::intptr_t;
		using allocator_type = Allocator;



		/// @brief Default constructor
		tiny_string() noexcept(std::is_nothrow_default_constructible< internal_data>::value)
		{
		}
		/// @brief Construct from allocator object
		explicit tiny_string(const Allocator& al) noexcept(std::is_nothrow_copy_constructible< Allocator>::value)
			:d_data(al)
		{
		}
		/// @brief Construct from a null-terminated string and an allocator object
		tiny_string(const Char* data, const Allocator& al = Allocator())
			:d_data(al)
		{
			size_t len = Traits::length(data);
			memcpy(initialize(len), data, len * sizeof(Char));
		}
		/// @brief Construct from a string, its size and an allocator object
		tiny_string(const Char* data, size_t n, const Allocator& al = Allocator())
			:d_data(al)
		{
			memcpy(initialize(n), data, n * sizeof(Char));
		}
		/// @brief Construct by filling with n copy of character c and an allocator object
		tiny_string(size_t n, Char c, const Allocator& al = Allocator())
			:d_data(al)
		{
			std::fill_n(initialize(n), n, c);
		}
		/// @brief Construct from a std::string and an allocator object
		template<class Al>
		tiny_string(const std::basic_string<Char, Traits, Al>& str, const Allocator& al = Allocator())
			: d_data(al)
		{
			memcpy(initialize(str.size()), str.data(), str.size() * sizeof(Char));
		}


#ifdef SEQ_HAS_CPP_17
		template<class Tr>
		tiny_string(const std::basic_string_view<Char, Tr>& str, const Allocator& al = Allocator())
			:d_data(al)
		{
			memcpy(initialize(str.size()), str.data(), str.size() * sizeof(Char));
		}
#endif



		/// @brief Copy constructor
		tiny_string(const tiny_string& other)
			:d_data(copy_allocator(other.d_data.get_allocator()))
		{
			if (other.is_sso())
				// for SSO and read only string 
				memcpy(&d_data.d_union, &other.d_data.d_union, sizeof(d_data.d_union));
			else {
				size_t size = other.size();
				memcpy(initialize(size), other.d_data.d_union.non_sso.data + char_offset, sizeof(Char) * (size ));
			}
		}
		/// @brief Copy constructor with custom allocator
		tiny_string(const tiny_string& other, const Allocator& al)
			:d_data(al)
		{
			if (other.is_sso())
				// for SSO and read only string 
				memcpy(&d_data.d_union, &other.d_data.d_union, sizeof(d_data.d_union));
			else {
				size_t size = other.size();
				memcpy(initialize(size), other.d_data.d_union.non_sso.data + char_offset, sizeof(Char) * (size ));
			}
		}
		/// @brief Construct by copying the content of another tiny_string
		template<size_t OtherMaxSS, class OtherAlloc>
		tiny_string(const tiny_string<Char, Traits, OtherAlloc, OtherMaxSS>& other)
			:d_data()
		{
			memcpy(initialize(other.size()), other.data(), other.size() * sizeof(Char));
		}
		/// @brief Construct by copying a sub-part of other
		tiny_string(const tiny_string& other, size_t pos, size_t len, const Allocator& al = Allocator())
			:d_data(al)
		{
			if (len == npos || pos + len > other.size())
				len = other.size() - pos;
			memcpy(initialize(len), other.data() + pos, len * sizeof(Char));
		}
		/// @brief Construct by copying a sub-part of other
		tiny_string(const tiny_string& other, size_t pos, const Allocator& al = Allocator())
			:d_data(al)
		{
			size_t len = other.size() - pos;
			memcpy(initialize(len), other.data() + pos, len * sizeof(Char));
		}
		/// @brief Move constructor
		SEQ_STR_INLINE_STRONG tiny_string(tiny_string&& other)
			noexcept(std::is_nothrow_move_constructible<Allocator>::value)
			:d_data(std::move(other.d_data.get_allocator()), other.d_data)
		{
			//memcpy(&d_data.d_union, &other.d_data.d_union, sizeof(d_data.d_union));
			other.d_data.reset();
		}
		/// @brief Move constructor with custom allocator
		tiny_string(tiny_string&& other, const Allocator& al) noexcept
			:d_data(al)
		{
			memcpy(&d_data.d_union, &other.d_data.d_union, sizeof(d_data.d_union));
			if (!is_sso()) {
				if (!std::allocator_traits<Allocator>::is_always_equal::value && al != other.get_allocator()) {
					size_t size = other.size();
					memcpy(initialize(size), other.data(), size * sizeof(Char));
				}
				else {
					other.d_data.reset();
				}
			}
		}
		/// @brief Construct by copying the range [first,last)
		template<class Iter>
		tiny_string(Iter first, Iter last, const Allocator& al = Allocator())
			:d_data(al)
		{
			assign_cat(first, last, typename std::iterator_traits<Iter>::iterator_category());
		}
		/// @brief Construct from initializer list
		tiny_string(std::initializer_list<Char> il, const Allocator& al = Allocator())
			:d_data(al)
		{
			assign(il);
		}

		~tiny_string()
		{
			if (!is_sso()) {
				d_data.deallocate(d_data.d_union.non_sso.data, capacity_internal() + char_offset);
			}
		}

		SEQ_STR_INLINE_STRONG std::pair<const Char*, size_t> as_pair() const noexcept {return d_data.as_pair();}
		/// @brief Returns the internal character storage 
		SEQ_STR_INLINE_STRONG auto data() noexcept -> Char* { return d_data.data(); }
		/// @brief Returns the internal character storage 
		SEQ_STR_INLINE_STRONG auto data() const noexcept -> const Char* { return d_data.data(); }
		/// @brief Returns the internal character storage 
		SEQ_STR_INLINE_STRONG auto c_str() const noexcept -> const Char* { return data(); }
		/// @brief Returns the string size (without the trailing null character)
		SEQ_STR_INLINE_STRONG auto size() const noexcept -> size_t { return d_data.size(); }
		/// @brief Returns the string size (without the trailing null character)
		SEQ_STR_INLINE_STRONG auto length() const noexcept -> size_t { return size(); }
		/// @brief Returns the string maximum size
		SEQ_STR_INLINE_STRONG auto max_size() const noexcept -> size_t { return internal_data::max_capacity - 1; }
		/// @brief Returns the string current capacity
		SEQ_STR_INLINE_STRONG auto capacity() const noexcept -> size_t { return capacity_internal() - 1ULL; }
		/// @brief Returns true if the string is empty, false otherwise
		SEQ_STR_INLINE_STRONG auto empty() const noexcept -> bool { return size() == 0; }
		/// @brief Returns the string allocator
		SEQ_STR_INLINE_STRONG auto get_allocator() const noexcept -> const allocator_type& { return d_data.get_allocator(); }
		SEQ_STR_INLINE_STRONG auto get_allocator() noexcept -> allocator_type& { return d_data.get_allocator(); }


		/// @brief Assign the range [first,last) to this string
		template<class Iter>
		auto assign(Iter first, Iter last) -> tiny_string&
		{
			assign_cat(first, last, typename std::iterator_traits<Iter>::iterator_category());
			return *this;
		}
		/// @brief Assign the content of other to this string
		auto assign(const tiny_string& other) -> tiny_string&
		{
			if (is_sso() && other.is_sso())
				memcpy(&d_data.d_union, &other.d_data.d_union, sizeof(d_data.d_union));
			else {
				if SEQ_CONSTEXPR(assign_alloc<Allocator>::value)
				{
					if (get_allocator() != other.get_allocator()) {
						clear();
					}
				}
				assign_allocator(get_allocator(), other.get_allocator());

				internal_resize(other.size(), false, true);
				memcpy(this->data(), other.c_str(), other.size() * sizeof(Char));
			}
			return *this;
		}
		/// @brief Assign the content of other to this string
		template<class OtherAlloc, size_t OtherMaxStaticSize>
		auto assign(const tiny_string<Char, Traits, OtherAlloc, OtherMaxStaticSize>& other) -> tiny_string&
		{
			internal_resize(other.size(), false, true);
			memcpy(this->data(), other.c_str(), other.size() * sizeof(Char));
			return *this;
		}
		/// @brief Assign the content of other to this string
		template<class Al>
		auto assign(const std::basic_string<Char, Traits, Al>& other) -> tiny_string&
		{
			internal_resize(other.size(), false, true);
			memcpy(this->data(), other.c_str(), other.size() * sizeof(Char));
			return *this;
		}

#ifdef SEQ_HAS_CPP_17
		/// @brief Assign the content of other to this string
		auto assign(const std::basic_string_view<Char, Traits>& other) -> tiny_string&
		{
			internal_resize(other.size(), false, true);
			memcpy(this->data(), other.data(), other.size() * sizeof(Char));
			return *this;
		}
#endif

		/// @brief Assign a sub-part of other to this string
		auto assign(const tiny_string& str, size_t subpos, size_t sublen) -> tiny_string&
		{
			if (sublen == npos || subpos + sublen > str.size())
				sublen = str.size() - subpos;
			internal_resize(sublen, false, true);
			memcpy(this->data(), str.data() + subpos, sublen * sizeof(Char));
			return *this;
		}
		/// @brief Assign a null-terminated buffer to this string
		auto assign(const Char* s) -> tiny_string&
		{
			size_t len = Traits::length(s);
			internal_resize(len, false, true);
			memcpy(this->data(), s, len * sizeof(Char));
			return *this;
		}
		/// @brief Assign a buffer to this string
		auto assign(const Char* s, size_t n) -> tiny_string&
		{
			internal_resize(n, false, true);
			memcpy(this->data(), s, n * sizeof(Char));
			return *this;
		}
		/// @brief Reset the string by n copies of c
		auto assign(size_t n, Char c) -> tiny_string&
		{
			internal_resize(n, false, true);
			std::fill_n(this->data(), n, c);
			return *this;
		}
		/// @brief Assign to this string the initializer list il
		auto assign(std::initializer_list<Char> il) -> tiny_string&
		{
			return assign(il.begin(), il.end());
		}
		/// @brief Move the content of other to this string. Equivalent to tiny_string::operator=.
		SEQ_STR_INLINE_STRONG auto assign(tiny_string&& other) noexcept(noexcept(std::declval<internal_data&>().swap(std::declval<internal_data&>()))) -> tiny_string&
		{
			if SEQ_CONSTEXPR( !noexcept(move_allocator(std::declval<Allocator&>(), std::declval<Allocator&>())))
				// Use internal_data.swap() for small static size or if moving allocator is NOT noexcept
				d_data.swap(other.d_data);
			else if (SEQ_LIKELY(this != std::addressof(other)))
			{
				if (!is_sso())
					d_data.deallocate(d_data.d_union.non_sso.data, capacity_internal() + char_offset);
				memcpy(&d_data.d_union, &other.d_data.d_union, sizeof(d_data.d_union));
				move_allocator(d_data.get_allocator(), other.d_data.get_allocator());
				other.d_data.reset();
			}
			return *this;
		}

		/// @brief Resize the string.
		/// For wide string, the allocated memory will be of exactly n+1 bytes.
		void resize(size_t n)
		{
			resize(n, 0);
		}
		/// @brief Resize the string.
		/// For wide string, the allocated memory will be of exactly n+1 bytes.
		/// If n is bigger than size(), pad with (n-size()) copies of c.
		void resize(size_t n, Char c)
		{
			size_t old_size = size();
			if (old_size == n)
				return;
			internal_resize(n, true);
			if (n > old_size)
				std::fill_n(data() + old_size, n - old_size, c);
		}

		/// @brief Resize the string from the front.
		/// For wide string, the allocated memory will be of exactly n+1 bytes.
		void resize_front(size_t n)
		{
			resize_front(n, 0);
		}
		/// @brief Resize the string from the front.
		/// For wide string, the allocated memory will be of exactly n+1 bytes.
		/// If n is bigger than size(), pad at the front with (n-size()) copies of c.
		void resize_front(size_t n, Char c)
		{
			size_t old_size = size();
			if (old_size == n)
				return;

			if (n <= capacity()) {
				//no alloc/dealloc required
				Char* p = data();
				if (n > old_size) {
					memmove(p + n - old_size, p, old_size * sizeof(Char));
					std::fill_n(p, n - old_size, c);
				}
				else {
					memmove(p, p + old_size - n, n * sizeof(Char));
				}
				d_data.setSizeKeepSSOFlag( n);
				data()[n] = 0;
				return;
			}

			tiny_string other(n, 0);
			if (n > old_size) {
				std::fill_n(other.data(), n - old_size, c);
				memcpy(other.data() + n - old_size, data(), old_size * sizeof(Char));
			}
			else {
				memcpy(other.data(), data() + old_size - n, n * sizeof(Char));
			}
			swap(other);
		}

		/// @brief Swap the content of this string with other
		SEQ_STR_INLINE_STRONG void swap(tiny_string& other) noexcept(noexcept(std::declval<internal_data&>().swap(std::declval<internal_data&>())))
		{
			// Do not check for same address as this is counter productive
			d_data.swap(other.d_data);
		}

		/// @brief Clear the string and deallocate memory for wide strings
		void clear() noexcept
		{
			if (!is_sso()) 
				d_data.deallocate(d_data.d_union.non_sso.data, capacity_internal() + char_offset);
			d_data.reset();
		}
		/// @brief 
		void shrink_to_fit() 
		{ 
			size_t s = size();
			if (!is_sso() && capacity_internal() != s+1) 
			{
				Char* p = d_data.d_union.non_sso.data;
				if (is_sso(s)) {
					memcpy(d_data.d_union.sso.data, p + char_offset, (s + 1)*sizeof(Char));
					d_data.deallocate(p, capacity_internal() + char_offset);
				}
				else{
					Char* _new = d_data.allocate(s + char_offset + 1);
					write_size_t(_new, s + 1);
					memcpy(_new + char_offset, p + char_offset, (s + 1) * sizeof(Char));
					d_data.deallocate(p, capacity_internal() + char_offset);
					d_data.d_union.non_sso.data = _new;
				}
				d_data.setSize(s);
			}
		} 
		/// @brief 
		void reserve(size_t new_capacity)
		{
			size_t s = size();
			Char* p = resize_grow(new_capacity,true);
			d_data.setSizeKeepSSOFlag(s);
			p[s] = 0;
		}

		/// @brief Returns an iterator to the first element of the container.
		SEQ_STR_INLINE auto begin() noexcept -> iterator { return data(); }
		/// @brief Returns an iterator to the first element of the container.
		SEQ_STR_INLINE auto begin() const noexcept -> const_iterator { return data(); }
		/// @brief Returns an iterator to the first element of the container.
		SEQ_STR_INLINE auto cbegin() const noexcept -> const_iterator { return data(); }

		/// @brief Returns an iterator to the element following the last element of the container.
		SEQ_STR_INLINE auto end() noexcept -> iterator { return data() + size(); }
		/// @brief Returns an iterator to the element following the last element of the container.
		SEQ_STR_INLINE auto end() const noexcept -> const_iterator { return data() + size(); }
		/// @brief Returns an iterator to the element following the last element of the container.
		SEQ_STR_INLINE auto cend() const noexcept -> const_iterator { return end(); }

		/// @brief Returns a reverse iterator to the first element of the reversed list.
		SEQ_STR_INLINE auto rbegin() noexcept -> reverse_iterator { return reverse_iterator(end()); }
		/// @brief Returns a reverse iterator to the first element of the reversed list.
		SEQ_STR_INLINE auto rbegin() const noexcept -> const_reverse_iterator { return const_reverse_iterator(end()); }
		/// @brief Returns a reverse iterator to the first element of the reversed list.
		SEQ_STR_INLINE auto crbegin() const noexcept -> const_reverse_iterator { return rbegin(); }

		/// @brief Returns a reverse iterator to the element following the last element of the reversed list.
		SEQ_STR_INLINE auto rend() noexcept -> reverse_iterator { return reverse_iterator(begin()); }
		/// @brief Returns a reverse iterator to the element following the last element of the reversed list.
		SEQ_STR_INLINE auto rend() const noexcept -> const_reverse_iterator { return const_reverse_iterator(begin()); }
		/// @brief Returns a reverse iterator to the element following the last element of the reversed list.
		SEQ_STR_INLINE auto crend() const noexcept -> const_reverse_iterator { return rend(); }

		/// @brief Returns the character at pos, throw std::out_of_range for invalid position.
		SEQ_STR_INLINE Char at(size_t pos) const
		{
			if (pos >= size()) throw std::out_of_range("");
			return data()[pos];
		}
		/// @brief Returns the character at pos, throw std::out_of_range for invalid position.
		SEQ_STR_INLINE Char& at(size_t pos)
		{
			if (pos >= size()) throw std::out_of_range("");
			return data()[pos];
		}

		/// @brief Returns the character at pos
		SEQ_STR_INLINE Char operator[](size_t pos) const noexcept
		{
			SEQ_ASSERT_DEBUG(pos < size(), "invalid position");
			return data()[pos];
		}
		/// @brief Returns the character at pos
		SEQ_STR_INLINE Char& operator[](size_t pos) noexcept
		{
			SEQ_ASSERT_DEBUG(pos < size(), "invalid position");
			return data()[pos];
		}

		/// @brief Returns the last character of the string
		SEQ_STR_INLINE Char back() const noexcept {
			SEQ_ASSERT_DEBUG(size() > 0, "empty container");
			return data()[size() - 1];
		}
		/// @brief Returns the last character of the string
		SEQ_STR_INLINE Char& back() noexcept {
			SEQ_ASSERT_DEBUG(size() > 0, "empty container");
			return data()[size() - 1];
		}
		/// @brief Returns the first character of the string
		SEQ_STR_INLINE Char front() const noexcept {
			SEQ_ASSERT_DEBUG(size() > 0, "empty container");
			return data()[0];
		}
		/// @brief Returns the first character of the string
		SEQ_STR_INLINE Char& front() noexcept {
			SEQ_ASSERT_DEBUG(size() > 0, "empty container");
			return data()[0];
		}

		/// @brief Append character to the back of the string
		SEQ_STR_INLINE_STRONG void push_back(Char c)
		{
			size_t s = size();
			Char* p = resize_grow(s + 1);
			p[s] = c;
			p[s+1] = 0;
		}
		/// @brief Removes the last character of the string
		SEQ_STR_INLINE_STRONG void pop_back()
		{
			SEQ_ASSERT_DEBUG(size() > 0, "pop_back on an empty string!");

			if (SEQ_LIKELY(!is_sso())) {
				size_t s = d_data.sizeNonSSO() - 1;
				d_data.setSizeNonSSO(s);
				d_data.d_union.non_sso.data[s + char_offset] = 0;
			}
			else {
				d_data.setSize(d_data.sizeSSO() - 1);
				d_data.d_union.sso.data[d_data.sizeSSO() ] = 0;
			}
		}

		/// @brief Append the content of str to this string
		template<class Al, size_t Ss>
		auto append(const tiny_string<Char, Traits, Al, Ss>& str) -> tiny_string&
		{
			return append(str.data(), str.size());
		}

		/// @brief Append the content of str to this string
		template<class Al>
		auto append(const std::basic_string<Char, Traits, Al>& str) -> tiny_string&
		{
			return append(str.data(), str.size());
		}

#ifdef SEQ_HAS_CPP_17
		/// @brief Append the content of str to this string
		auto append(const std::basic_string_view<Char, Traits>& str) -> tiny_string&
		{
			return append(str.data(), str.size());
		}
#endif

		/// @brief Append the sub-part of str to this string
		template<size_t Ss, class Al>
		auto append(const tiny_string<Char, Traits, Al, Ss>& str, size_t subpos, size_t sublen = npos) -> tiny_string&
		{
			if (sublen == npos || subpos + sublen > str.size())
				sublen = str.size() - subpos;
			return append(str.data() + subpos, sublen);
		}
		/// @brief Append the sub-part of str to this string
		template<class Al>
		auto append(const std::basic_string<Char, Traits, Al>& str, size_t subpos, size_t sublen = npos) -> tiny_string&
		{
			if (sublen == npos || subpos + sublen > str.size())
				sublen = str.size() - subpos;
			return append(str.data() + subpos, sublen);
		}

#ifdef SEQ_HAS_CPP_17
		/// @brief Append the sub-part of str to this string
		auto append(const std::basic_string_view<Char, Traits>& str, size_t subpos, size_t sublen = npos) -> tiny_string&
		{
			if (sublen == npos || subpos + sublen > str.size())
				sublen = str.size() - subpos;
			return append(str.data() + subpos, sublen);
		}
#endif


		/// @brief Append a null-terminated buffer to this string
		auto append(const Char* s) -> tiny_string&
		{
			return append(s, Traits::length(s));
		}

		/// @brief Append buffer content to this string
		auto append(const Char* s, size_t n) -> tiny_string&
		{
			if (SEQ_UNLIKELY(n == 0)) return *this;
			size_t old_size = size();
			size_t new_size = old_size + n;
			Char* d = resize_grow(new_size);
			memcpy(d + old_size, s, n * sizeof(Char));
			d[new_size] = 0;
			return *this;
		}
		/// @brief Append n copies of character c to the string
		auto append(size_t n, Char c) -> tiny_string&
		{
			if (SEQ_UNLIKELY(n == 0)) return *this;
			size_t old_size = size();
			size_t new_size = old_size + n;
			Char* d = resize_grow(new_size);
			std::fill_n(d + old_size, n, c);
			d[new_size] = 0;
			return *this;
		}
		/// @brief Append the content of the range [first,last) to this string
		template <class Iter>
		auto append(Iter first, Iter last) -> tiny_string&
		{
			if (first == last) return *this;
			if (std::is_same<typename std::iterator_traits<Iter>::iterator_category, std::random_access_iterator_tag>::value) {
				size_t n = std::distance(first, last);
				size_t old_size = size();
				size_t new_size = old_size + n;
				Char* d = resize_grow(new_size);
				std::copy(first, last, d + old_size);
				d[new_size] = 0;
			}
			else {
				while (first != last) {
					push_back(*first);
					++first;
				}
			}
			return *this;
		}
		/// @brief Append the content of the range [il.begin(),il.end()) to this string
		auto append(std::initializer_list<Char> il)  -> tiny_string&
		{
			return append(il.begin(), il.end());
		}



		/// @brief Inserts the content of str at position pos
		template<size_t S, class AL>
		auto insert(size_t pos, const tiny_string<Char, Traits, AL, S>& str) -> tiny_string&
		{
			insert(begin() + pos, str.begin(), str.end());
			return *this;
		}
		/// @brief Inserts a sub-part of str at position pos
		template<class Al>
		auto insert(size_t pos, const std::basic_string<Char, Traits, Al>& str) -> tiny_string&
		{
			insert(begin() + pos, str.begin(), str.end());
			return *this;
		}

#ifdef SEQ_HAS_CPP_17
		/// @brief Inserts a sub-part of str at position pos
		auto insert(size_t pos, const std::basic_string_view<Char, Traits>& str) -> tiny_string&
		{
			insert(begin() + pos, str.begin(), str.end());
			return *this;
		}
#endif

		/// @brief Inserts a sub-part of str at position pos
		auto insert(size_t pos, const tiny_string& str, size_t subpos, size_t sublen = npos)  -> tiny_string&
		{
			if (sublen == npos || subpos + sublen > str.size())
				sublen = str.size() - subpos;
			insert(begin() + pos, str.begin() + subpos, str.begin() + subpos + sublen);
			return *this;
		}
		/// @brief Inserts a null-terminated buffer at position pos
		auto insert(size_t pos, const Char* s)  -> tiny_string&
		{
			//avoid multiple push_back
			size_t len = Traits::length(s);
			insert(begin() + pos, s, s + len);
			return *this;
		}
		/// @brief Inserts buffer at position pos
		auto insert(size_t pos, const Char* s, size_t n) -> tiny_string&
		{
			insert(begin() + pos, s, s + n);
			return *this;
		}
		/// @brief Inserts n copies of character c at position pos
		auto insert(size_t pos, size_t n, Char c)  -> tiny_string&
		{
			insert(begin() + pos, cvalue_iterator<Char>(0, c), cvalue_iterator<Char>(n));
			return *this;
		}
		/// @brief Inserts n copies of character c at position p
		auto insert(const_iterator p, size_t n, Char c) -> iterator
		{
			return insert(p, cvalue_iterator<Char>(0, c), cvalue_iterator<Char>(n));
		}
		/// @brief Inserts character c at position p
		auto insert(const_iterator p, Char c) -> iterator
		{
			return insert(p, &c, (&c) + 1);
		}
		/// @brief Inserts the content of the range [first,last) at position p
		template <class InputIterator>
		auto insert(const_iterator p, InputIterator first, InputIterator last) -> iterator
		{
			size_t pos = static_cast<size_t>(p - cbegin());
			insert_cat(pos, first, last, typename std::iterator_traits<InputIterator>::iterator_category());
			return begin() + pos;
		}
		/// @brief Inserts the content of the range [il.begin(),il.end()) at position p
		auto insert(const_iterator p, std::initializer_list<Char> il) -> iterator
		{
			size_t pos = p - cbegin();
			insert_cat(pos, il.begin(), il.end(), std::random_access_iterator_tag());
			return begin() + pos;
		}

		/// @brief Removes up to sublen character starting from subpos
		auto erase(size_t subpos, size_t sublen = npos) -> tiny_string&
		{
			if (sublen == npos || subpos + sublen > size())
				sublen = size() - subpos;
			erase_internal(subpos, subpos + sublen);
			return *this;
		}
		/// @brief Remove character at position p
		auto erase(const_iterator p) -> iterator
		{
			size_type f = static_cast<size_t>(p - begin());
			erase_internal(f, f + 1);
			return begin() + f;
		}
		/// @brief Removes the range [first,last)
		auto erase(const_iterator first, const_iterator last) -> iterator
		{
			size_type f = static_cast<size_t>(first - begin());
			erase_internal(f, static_cast<size_t>(last - begin()));
			return begin() + f;
		}


		/// @brief Replace up to len character starting from pos by str
		template< class Al>
		auto replace(size_t pos, size_t len, const std::basic_string<Char, Traits, Al>& str) -> tiny_string&
		{
			if (len == npos || pos + len > size())
				len = size() - pos;
			replace_internal(pos, len, str.begin(), str.end());
			return *this;
		}

#ifdef SEQ_HAS_CPP_17
		/// @brief Replace up to len character starting from pos by str
		auto replace(size_t pos, size_t len, const std::basic_string_view<Char, Traits>& str) -> tiny_string&
		{
			if (len == npos || pos + len > size())
				len = size() - pos;
			replace_internal(pos, len, str.begin(), str.end());
			return *this;
		}
#endif


		/// @brief Replace characters in the range [i1,i2) by str
		template<class Al>
		auto replace(const_iterator i1, const_iterator i2, const std::basic_string<Char, Traits, Al>& str) -> tiny_string&
		{
			replace_internal(i1 - cbegin(), i2 - i1, str.begin(), str.end());
			return *this;
		}

#ifdef SEQ_HAS_CPP_17
		/// @brief Replace characters in the range [i1,i2) by str
		auto replace(const_iterator i1, const_iterator i2, const std::basic_string_view<Char, Traits>& str) -> tiny_string&
		{
			replace_internal(i1 - cbegin(), i2 - i1, str.begin(), str.end());
			return *this;
		}
#endif


		/// @brief Replace up to len character starting from pos by str
		template<class Al, size_t S>
		auto replace(size_t pos, size_t len, const tiny_string<Char, Traits, Al, S>& str) -> tiny_string&
		{
			if (len == npos || pos + len > size())
				len = size() - pos;
			replace_internal(pos, len, str.begin(), str.end());
			return *this;
		}
		/// @brief Replace characters in the range [i1,i2) by str
		template<class Al, size_t S>
		auto replace(const_iterator i1, const_iterator i2, const tiny_string<Char, Traits, Al, S>& str) -> tiny_string&
		{
			replace_internal(i1 - cbegin(), i2 - i1, str.begin(), str.end());
			return *this;
		}


		/// @brief Replace up to len character starting from pos by a sub-part of str
		template<class Al, size_t S>
		auto replace(size_t pos, size_t len, const tiny_string<Char, Traits, Al, S>& str,
			size_t subpos, size_t sublen)  -> tiny_string&
		{
			if (len == npos || pos + len > size())
				len = size() - pos;
			if (sublen == npos || subpos + sublen > str.size())
				sublen = size() - subpos;
			replace_internal(pos, len, str.begin() + subpos, str.begin() + subpos + sublen);
			return *this;
		}
		/// @brief Replace up to len character starting from pos by a sub-part of str
		template<class Al>
		auto replace(size_t pos, size_t len, const std::basic_string<Char, Traits, Al>& str,
			size_t subpos, size_t sublen) -> tiny_string&
		{
			if (len == npos || pos + len > size())
				len = size() - pos;
			if (sublen == npos || subpos + sublen > str.size())
				sublen = size() - subpos;
			replace_internal(pos, len, str.begin() + subpos, str.begin() + subpos + sublen);
			return *this;
		}

#ifdef SEQ_HAS_CPP_17
		/// @brief Replace up to len character starting from pos by a sub-part of str
		auto replace(size_t pos, size_t len, const std::basic_string_view<Char, Traits>& str,
			size_t subpos, size_t sublen) -> tiny_string&
		{
			if (len == npos || pos + len > size())
				len = size() - pos;
			if (sublen == npos || subpos + sublen > str.size())
				sublen = size() - subpos;
			replace_internal(pos, len, str.begin() + subpos, str.begin() + subpos + sublen);
			return *this;
		}
#endif

		/// @brief Replace up to len character starting from pos by s
		auto replace(size_t pos, size_t len, const Char* s) -> tiny_string&
		{
			size_t l = Traits::length(s);
			replace_internal(pos, len, s, s + l);
			return *this;
		}
		/// @brief Replace characters in the range [i1,i2) by null-terminated string s
		auto replace(const_iterator i1, const_iterator i2, const Char* s) -> tiny_string&
		{
			size_t l = Traits::length(s);
			replace_internal(i1 - cbegin(), i2 - i1, s, s + l);
			return *this;
		}
		/// @brief Replace up to len character starting from pos by buffer s of size n
		auto replace(size_t pos, size_t len, const Char* s, size_t n)  -> tiny_string&
		{
			replace_internal(pos, len, s, s + n);
			return *this;
		}
		/// @brief Replace characters in the range [i1,i2) by buffer s of size n
		auto replace(const_iterator i1, const_iterator i2, const Char* s, size_t n) -> tiny_string&
		{
			replace_internal(i1 - cbegin(), i2 - i1, s, s + n);
			return *this;
		}

		/// @brief Replace up to len character starting from pos by n copies of c
		auto replace(size_t pos, size_t len, size_t n, Char c)  -> tiny_string&
		{
			replace_internal(pos, len, cvalue_iterator<Char>(0, c), cvalue_iterator<Char>(n));
			return *this;
		}
		/// @brief Replace characters in the range [i1,i2) by n copies of c
		auto replace(const_iterator i1, const_iterator i2, size_t n, Char c) -> tiny_string&
		{
			replace_internal(i1 - cbegin(), i2 - i1, cvalue_iterator<Char>(0, c), cvalue_iterator<Char>(n));
			return *this;
		}
		/// @brief Replace characters in the range [i1,i2) by the range [first,last)
		template <class InputIterator>
		auto replace(const_iterator i1, const_iterator i2,
			InputIterator first, InputIterator last)  -> tiny_string&
		{
			replace_internal(i1 - cbegin(), i2 - i1, first, last);
			return *this;
		}
		/// @brief Replace characters in the range [i1,i2) by the range [il.begin(),il.end())
		auto replace(const_iterator i1, const_iterator i2, std::initializer_list<Char> il)  -> tiny_string&
		{
			return replace(i1, i2, il.begin(), il.end());
		}


		/// @brief Replace any sub-string _from of size n1 by the string _to of size n2, starting to position start
		auto replace(const Char* _from, size_t n1, const Char* _to, size_t n2, size_t start = 0) -> size_t
		{
			size_t res = 0;
			size_t start_pos = start;
			while ((start_pos = find(_from, start_pos, n1)) != std::string::npos) {
				replace(start_pos, n1, _to, n2);
				start_pos += n2; // Handles case where 'to' is a substring of 'from'
				++res;
			}
			return res;
		}
		/// @brief Replace any sub-string _from by the string _to, starting to position start
		auto replace(const Char* _from, const Char* _to, size_t start = 0) -> size_t
		{
			return replace(_from, Traits::length(_from), _to, Traits::length(_to), start);
		}
		/// @brief Replace any sub-string _from by the string _to, starting to position start
		template<size_t S1, class Al1, size_t S2, class Al2>
		auto replace(const tiny_string<Char, Traits, Al1, S1>& _from, const tiny_string<Char, Traits, Al2, S2>& _to, size_t start = 0) -> size_t
		{
			return replace(_from.data(), _from.size(), _to.data(), _to.size(), start);
		}
		/// @brief Replace any sub-string _from by the string _to, starting to position start
		template<class Al1, class Al2>
		auto replace(const std::basic_string<Char, Traits, Al1>& _from, const std::basic_string<Char, Traits, Al2>& _to, size_t start = 0) -> size_t
		{
			return replace(_from.data(), _from.size(), _to.data(), _to.size(), start);
		}

#ifdef SEQ_HAS_CPP_17
		/// @brief Replace any sub-string _from by the string _to, starting to position start
		auto replace(const std::basic_string_view<Char, Traits>& _from, const std::basic_string_view<Char, Traits>& _to, size_t start = 0) -> size_t
		{
			return replace(_from.data(), _from.size(), _to.data(), _to.size(), start);
		}
#endif


		/// @brief Returns the count of non-overlapping occurrences of 'str' starting from position start
		auto count(const Char* str, size_t n, size_t start = 0) const noexcept  -> size_t
		{
			if (length() == 0) return 0;
			size_t count = 0;
			for (size_t offset = find(str, start, n); offset != npos; offset = find(str, offset + n, n))
				++count;
			return count;
		}
		/// @brief Returns the count of non-overlapping occurrences of 'str' starting from position start
		auto count(const Char* str, size_t start = 0) const noexcept  -> size_t
		{
			return count(str, Traits::length(str), start);
		}
		/// @brief Returns the count of non-overlapping occurrences of 'str' starting from position start
		template<class Al, size_t S>
		auto count(const tiny_string<Char, Traits, Al, S>& str, size_t start = 0) const noexcept -> size_t
		{
			return count(str.data(), str.size(), start);
		}
		/// @brief Returns the count of non-overlapping occurrences of 'str' starting from position start
		template<class Al>
		auto count(const std::basic_string<Char, Traits, Al>& str, size_t start = 0) const noexcept -> size_t
		{
			return count(str.data(), str.size(), start);
		}

#ifdef SEQ_HAS_CPP_17
		/// @brief Returns the count of non-overlapping occurrences of 'str' starting from position start
		auto count(const std::basic_string_view<Char, Traits>& str, size_t start = 0) const noexcept -> size_t
		{
			return count(str.data(), str.size(), start);
		}
#endif


		/// @brief Returns the count of non-overlapping occurrences of 'str' starting from position start
		auto count(Char c, size_t start = 0) const noexcept  -> size_t
		{
			if (length() == 0) return 0;
			size_t count = 0;
			for (size_t offset = find(c, start); offset != npos; offset = find(c, offset + 1))
				++count;
			return count;
		}


		/// @brief Copies a substring [pos, pos+len) to character string pointed to by s.
		/// Returns the number of copied characters.
		auto copy(Char* s, size_t len, size_t pos = 0) const -> size_t
		{
			if (pos > size()) throw std::out_of_range("tiny_string::copy out of range");
			if (len == npos || pos + len > size())
				len = size() - pos;
			memcpy(s, data() + pos, len * sizeof(Char));
			return len;
		}

		// @brief Returns a sub-part of this string as a tstring_view object
		auto substr(size_t pos, size_t len = npos) const -> tiny_string<Char, Traits, view_allocator<Char>, 0>
		{
			if (pos > size()) throw std::out_of_range("tiny_string::substr out of range");
			if (len == npos || pos + len > size())
				len = size() - pos;
			return tiny_string<Char, Traits, view_allocator<Char>, 0>(begin() + pos, len);
		}

		template<class Al, size_t S>
		auto find(const tiny_string<Char, Traits, Al, S>& str, size_t pos = 0) const noexcept -> size_t
		{
			return find(str.data(), pos, str.size());
		}
		template<class Al>
		auto find(const std::basic_string<Char, Traits, Al>& str, size_t pos = 0) const noexcept -> size_t
		{
			return find(str.data(), pos, str.size());
		}

#ifdef SEQ_HAS_CPP_17
		auto find(const std::basic_string_view<Char, Traits>& str, size_t pos = 0) const noexcept -> size_t
		{
			return find(str.data(), pos, str.size());
		}
#endif

		auto find(const Char* s, size_t pos = 0) const noexcept-> size_t
		{
			return find(s, pos, Traits::length(s));
		}
		auto find(const Char* s, size_t pos, size_type n) const noexcept -> size_t
		{
			return detail::traits_string_find<Traits>(data(), pos, size(), s, n, npos);
		}
		auto find(Char c, size_t pos = 0) const noexcept -> size_t
		{
			const Char* p = Traits::find(data() + pos, size() - pos, c);
			return p == NULL ? npos : p - begin();
		}

		template<class Al, size_t S>
		auto rfind(const tiny_string<Char, Traits, Al, S>& str, size_t pos = npos) const noexcept -> size_t
		{
			return rfind(str.data(), pos, str.size());
		}
		template<class Al>
		auto rfind(const std::basic_string<Char, Traits, Al>& str, size_t pos = npos) const noexcept -> size_t
		{
			return rfind(str.data(), pos, str.size());
		}

#ifdef SEQ_HAS_CPP_17
		size_t rfind(const std::basic_string_view<Char, Traits>& str, size_t pos = npos) const noexcept
		{
			return rfind(str.data(), pos, str.size());
		}
#endif

		auto rfind(const Char* s, size_t pos = npos) const noexcept -> size_t
		{
			return rfind(s, pos, Traits::length(s));
		}
		auto rfind(const Char* s, size_t pos, size_type n) const noexcept -> size_t
		{
			return detail::traits_string_rfind<Traits>(data(), pos, size(), s, n, npos);
		}
		auto rfind(Char c, size_t pos = npos) const noexcept -> size_t
		{
			if (pos >= size()) pos = size() - 1;
			const Char* p = detail::string_memrchr(data(), c, pos + 1);
			return p == NULL ? npos : p - data();
		}


		auto find_first_of(const Char* s, size_t pos, size_t n) const noexcept -> size_t
		{
			return detail::traits_string_find_first_of<Traits>(data(), pos, size(), s, n, npos);
		}
		template<class Al, size_t S>
		auto find_first_of(const tiny_string<Char, Traits, Al, S>& str, size_t pos = 0) const noexcept -> size_t
		{
			return find_first_of(str.data(), pos, str.size());
		}
		auto find_first_of(const Char* s, size_t pos = 0) const noexcept -> size_t
		{
			return find_first_of(s, pos, Traits::length(s));
		}
		auto find_first_of(Char c, size_t pos = 0) const noexcept -> size_t
		{
			return find(c, pos);
		}

		template<class Al, size_t S>
		auto find_last_of(const tiny_string<Char, Traits, Al, S>& str, size_t pos = npos) const noexcept -> size_t
		{
			return find_last_of(str.data(), pos, str.size());
		}
		auto find_last_of(const Char* s, size_t pos = npos) const noexcept -> size_t
		{
			return find_last_of(s, pos, Traits::length(s));
		}
		auto find_last_of(const Char* s, size_t pos, size_t n = npos) const noexcept -> size_t
		{
			return detail::traits_string_find_last_of<Traits>(data(), pos, size(), s, n, npos);
		}
		auto find_last_of(Char c, size_t pos = npos) const noexcept -> size_t
		{
			return rfind(c, pos);
		}

		template<class Al, size_t S>
		auto find_first_not_of(const tiny_string<Char, Traits, Al, S>& str, size_t pos = 0) const noexcept -> size_t
		{
			return find_first_not_of(str.data(), pos, str.size());
		}
		auto find_first_not_of(const Char* s, size_t pos = 0) const noexcept -> size_t
		{
			return find_first_not_of(s, pos, Traits::length(s));
		}
		auto find_first_not_of(const Char* s, size_t pos, size_t n) const noexcept -> size_t
		{
			return detail::traits_string_find_first_not_of<Traits>(data(), pos, size(), s, n, npos);
		}
		auto find_first_not_of(Char c, size_t pos = 0) const noexcept -> size_t
		{
			const Char* e = end();
			for (const Char* p = data() + pos; p != e; ++p)
				if (*p != c) return p - data();
			return npos;
		}

		template<class Al, size_t S>
		auto find_last_not_of(const tiny_string<Char, Traits, Al, S>& str, size_t pos = npos) const noexcept -> size_t
		{
			return find_last_not_of(str.data(), pos, str.size());
		}
		auto find_last_not_of(const Char* s, size_t pos = npos) const noexcept -> size_t
		{
			return find_last_not_of(s, pos, Traits::length(s));
		}
		auto find_last_not_of(const Char* s, size_t pos, size_t n = npos) const noexcept -> size_t
		{
			return detail::traits_string_find_last_not_of<Traits>(data(), pos, size(), s, n, npos);
		}
		auto find_last_not_of(Char c, size_t pos = npos) const noexcept -> size_t
		{
			if (size() == 0) return npos;
			if (pos >= size()) pos = size() - 1;
			const Char* p = data();
			for (const Char* in = p + pos; in >= p; --in)
				if (*in != c)
					return in - p;
			return npos;
		}

		template<class Al, size_t S>
		SEQ_STR_INLINE_STRONG auto compare(const tiny_string<Char, Traits, Al, S>& str) const noexcept -> int
		{
			return detail::traits_string_compare<Traits>(as_pair(),str.as_pair());
		}
		template<class Al, size_t S>
		auto compare(size_t pos, size_t len, const tiny_string<Char, Traits, Al, S>& str) const noexcept -> int
		{
			return compare(pos, len, str.data(), str.size());
		}
		template<class Al>
		auto compare(size_t pos, size_t len, const std::basic_string<Char, Traits, Al>& str) const noexcept -> int
		{
			return compare(pos, len, str.data(), str.size());
		}

#ifdef SEQ_HAS_CPP_17
		auto compare(size_t pos, size_t len, const std::basic_string_view<Char, Traits>& str) const noexcept -> int
		{
			return compare(pos, len, str.data(), str.size());
		}
#endif
		template<class Al, size_t S>
		auto compare(size_t pos, size_t len, const tiny_string<Char, Traits, Al, S>& str, size_t subpos, size_t sublen) const noexcept -> int
		{
			if (sublen == npos || subpos + sublen > str.size())
				sublen = str.size() - subpos;
			return compare(pos, len, str.data() + subpos, sublen);
		}
		auto compare(const Char* s) const noexcept -> int
		{
			return compare(0, size(), s);
		}
		auto compare(size_t pos, size_t len, const Char* s) const noexcept -> int
		{
			return compare(pos, len, s, Traits::length(s));
		}
		auto compare(size_t pos, size_t len, const Char* _s, size_t n) const noexcept -> int
		{
			if (len == npos )//|| pos + len > size())
				len = size() - pos;
			return detail::traits_string_compare<Traits>(data() + pos, len, _s, n);
		}


		auto operator=(const tiny_string& other) -> tiny_string&
		{
			if (this != std::addressof(other))
				assign(other);
			return *this;
		}
		template<class Al, size_t S>
		auto operator=(const tiny_string<Char, Traits, Al, S>& other) -> tiny_string&
		{
			return assign(other);
		}
		SEQ_STR_INLINE_STRONG auto operator=(tiny_string&& other) noexcept(noexcept(std::declval<tiny_string&>().assign(std::move(std::declval<tiny_string&>())))) -> tiny_string&
		{
			return assign(std::move(other));
		}
		auto operator=(const Char* other) -> tiny_string&
		{
			return assign(other);
		}
		auto operator=(Char c) -> tiny_string&
		{
			return assign(&c, 1);
		}
		template<class Al>
		auto operator=(const std::basic_string<Char, Traits, Al>& other) -> tiny_string&
		{
			return assign(other);
		}

#ifdef SEQ_HAS_CPP_17
		auto operator=(const std::basic_string_view<Char, Traits>& other) -> tiny_string&
		{
			return assign(other);
		}
#endif

		auto operator=(std::initializer_list<Char> il) -> tiny_string&
		{
			return assign(il);
		}

		template<class Al, size_t S>
		auto operator+= (const tiny_string<Char, Traits, Al, S>& str) -> tiny_string& { return append(str); }
		template<class Al>
		auto operator+= (const std::basic_string<Char, Traits, Al>& str) -> tiny_string& { return append(str); }
		auto operator+= (const Char* s) -> tiny_string& { return append(s); }
		auto operator+= (Char c) -> tiny_string& { push_back(c); return *this; }
		auto operator+= (std::initializer_list<Char> il) -> tiny_string& { return append(il); }

#ifdef SEQ_HAS_CPP_17
		auto operator+= (const std::basic_string_view<Char, Traits>& str) -> tiny_string& { return append(str); }
#endif

		/// @brief Implicit conversion to std::string
		template< class Al>
		operator std::basic_string<Char, Traits, Al>() const {
			return std::basic_string<Char, Traits, Al>(data(), size());
		}
	};





	/// @brief Specialization of tiny_string for string views.
	/// You should use the global typedef tstring_view equivalent to tiny_string<0,view_allocator>.
	/// Provides a similar interface to std::string_view.
	/// See tiny_string documentation for more details.
	template<class Char, class Traits>
	struct tiny_string<Char, Traits, view_allocator<Char>, 0>
	{
		using internal_data = detail::string_internal<Char, 0, view_allocator<Char>>;
		using this_type = tiny_string<Char, Traits, view_allocator<Char>, 0>;

		static constexpr size_t sso_max_capacity = ((sizeof(internal_data) - alignof(Char)) / sizeof(Char));

		internal_data d_data;

		//is it a small string
		SEQ_STR_INLINE bool is_sso() const noexcept { return false; }
		SEQ_STR_INLINE bool is_sso(size_t  /*unused*/) const noexcept { return false; }
		auto size_internal() const noexcept -> size_t { return d_data.size; }


	public:
		static constexpr size_t max_static_size = sso_max_capacity - 1;
		static constexpr size_t npos = static_cast<size_t>(-1);

		using traits_type = std::char_traits<Char>;
		using value_type = Char;
		using reference = Char&;
		using pointer = Char*;
		using const_reference = const Char&;
		using const_pointer = const Char*;
		using iterator = const Char*;
		using const_iterator = const Char*;
		using reverse_iterator = std::reverse_iterator<iterator>;
		using const_reverse_iterator = std::reverse_iterator<const_iterator>;
		using size_type = size_t;
		using difference_type = std::intptr_t;
		using allocator_type = view_allocator<Char>;


		auto data() const noexcept -> const Char* { return d_data.data; }
		auto c_str() const noexcept -> const Char* { return data(); }
		auto size() const noexcept -> size_t { return d_data.size; }
		auto length() const noexcept -> size_t { return size(); }
		static auto max_size() noexcept -> size_t { return std::numeric_limits<size_t>::max(); }
		auto empty() const noexcept -> bool { return size() == 0; }
		auto get_allocator() const noexcept -> allocator_type { return d_data.get_allocator(); }

		tiny_string() noexcept
			:d_data(nullptr, 0) {}

		tiny_string(const Char* data) noexcept
			:d_data(data, Traits::length(data)) {}

		tiny_string(const Char* data, size_t n) noexcept
			:d_data(data, n) {}

		template<class Al>
		tiny_string(const std::basic_string<Char, Traits, Al>& str) noexcept
			: d_data(str.data(), str.size()) {}

#ifdef SEQ_HAS_CPP_17
		tiny_string(const std::basic_string_view<Char, Traits>& str) noexcept
			: d_data(str.data(), str.size()) {}
#endif

		template<class Al, size_t S>
		tiny_string(const tiny_string<Char, Traits, Al, S>& other) noexcept
			: d_data(other.data(), other.size()) {}

		tiny_string(const tiny_string& other) noexcept
			: d_data(other.d_data) {}

		template<class Iter>
		tiny_string(Iter first, Iter last)
			: d_data(&(*first), last - first) {}

		auto operator=(const tiny_string& other) noexcept -> tiny_string&
		{
			d_data = other.d_data;
			return *this;
		}
		template<class A, size_t S>
		auto operator=(const tiny_string<Char, Traits, A, S>& other) noexcept -> tiny_string&
		{
			d_data = internal_data(other.data(), other.size());
			return *this;
		}
		template<class Al>
		auto operator=(const std::basic_string<Char, Traits, Al>& other) noexcept -> tiny_string&
		{
			d_data = internal_data(other.data(), other.size());
			return *this;
		}
		auto operator=(const Char* other) noexcept -> tiny_string&
		{
			d_data = internal_data(other, Traits::length(other));
			return *this;
		}
		template<size_t S>
		auto operator=(const Char other[S]) noexcept -> tiny_string&
		{
			d_data = internal_data(other, Traits::length(other));
			return *this;
		}
#ifdef SEQ_HAS_CPP_17
		auto operator=(const std::basic_string_view<Char, Traits>& other) noexcept -> tiny_string&
		{
			d_data = internal_data(other.data(), other.size());
			return *this;
		}
#endif

		SEQ_STR_INLINE_STRONG void swap(tiny_string& other) noexcept
		{
			d_data.swap(other.d_data);
		}
		SEQ_STR_INLINE_STRONG std::pair<const Char*, size_t> as_pair() const noexcept { return d_data.as_pair(); }
		auto begin() const noexcept -> const_iterator { return data(); }
		auto cbegin() const noexcept -> const_iterator { return data(); }
		auto end() const noexcept -> const_iterator { return d_data.data + d_data.size; }
		auto cend() const noexcept -> const_iterator { return d_data.data + d_data.size; }
		auto rbegin() const noexcept -> const_reverse_iterator { return const_reverse_iterator(end()); }
		auto rend() const noexcept -> const_reverse_iterator { return const_reverse_iterator(begin()); }
		auto crbegin() const noexcept -> const_reverse_iterator { return rbegin(); }
		auto crend() const noexcept -> const_reverse_iterator { return rend(); }

		auto at(size_t pos) const -> Char
		{
			if (pos >= size()) { throw std::out_of_range(""); }
			return data()[pos];
		}
		auto operator[](size_t pos) const noexcept -> Char
		{
			return data()[pos];
		}

		auto back() const noexcept -> Char { return data()[size() - 1]; }
		auto front() const noexcept -> Char { return data()[0]; }

		/**
		* Convenient function, returns the count of non-overlapping occurrences of 'str'
		*/
		auto count(const Char* str, size_t n, size_t start = 0) const noexcept -> size_t
		{
			if (length() == 0) return 0;
			size_t count = 0;
			for (size_t offset = find(str, start, n); offset != npos; offset = find(str, offset + n, n))
				++count;
			return count;
		}
		auto count(const Char* str, size_t start = 0) const noexcept -> size_t
		{
			return count(str, Traits::length(str), start);
		}
		auto count(const tiny_string& str, size_t start = 0) const noexcept -> size_t
		{
			return count(str.data(), str.size(), start);
		}
		template<class Al>
		auto count(const std::basic_string<Char, Traits, Al>& str, size_t start = 0) const noexcept -> size_t
		{
			return count(str.data(), str.size(), start);
		}

#ifdef SEQ_HAS_CPP_17
		auto count(const std::basic_string_view<Char, Traits>& str, size_t start = 0) const noexcept -> size_t
		{
			return count(str.data(), str.size(), start);
		}
#endif

		auto count(Char c, size_t start = 0) const noexcept -> size_t
		{
			if (length() == 0) return 0;
			size_t count = 0;
			for (size_t offset = find(c, start); offset != npos; offset = find(c, offset + 1))
				++count;
			return count;
		}


		auto copy(Char* s, size_t len, size_t pos = 0) const -> size_t
		{
			if (pos > size()) { throw std::out_of_range("tiny_string::copy out of range"); }
			if (len == npos || pos + len > size()) {
				len = size() - pos;
			}
			memcpy(s, data() + pos, len * sizeof(Char));
			return len;
		}

		auto substr(size_t pos = 0, size_t len = npos) const -> tiny_string
		{
			if (pos > size()) { throw std::out_of_range("tiny_string::substr out of range"); }
			if (len == npos || pos + len > size()) {
				len = size() - pos;
			}
			return { begin() + pos, len };
		}

		template<class Al, size_t S>
		auto find(const tiny_string<Char, Traits, Al, S>& str, size_t pos = 0) const noexcept -> size_t
		{
			return find(str.data(), pos, str.size());
		}
		template<class Al>
		auto find(const std::basic_string<Char, Traits, Al>& str, size_t pos = 0) const noexcept -> size_t
		{
			return find(str.data(), pos, str.size());
		}

#ifdef SEQ_HAS_CPP_17
		auto find(const std::basic_string_view<Char, Traits>& str, size_t pos = 0) const noexcept -> size_t
		{
			return find(str.data(), pos, str.size());
		}
#endif

		auto find(const Char* s, size_t pos = 0) const noexcept -> size_t
		{
			return find(s, pos, Traits::length(s));
		}
		auto find(const Char* s, size_t pos, size_type n) const noexcept -> size_t
		{
			return detail::traits_string_find<Traits>(data(), pos, size(), s, n, npos);
		}
		auto find(Char c, size_t pos = 0) const noexcept -> size_t
		{
			const Char* p = Traits::find(data() + pos, size() - pos, c);
			return p == nullptr ? npos : static_cast<size_t>(p - begin());
		}

		template<class A, size_t S>
		auto rfind(const tiny_string<Char, Traits, A, S>& str, size_t pos = npos) const noexcept -> size_t
		{
			return rfind(str.data(), pos, str.size());
		}
		template<class Al>
		auto rfind(const std::basic_string<Char, Traits, Al>& str, size_t pos = npos) const noexcept -> size_t
		{
			return rfind(str.data(), pos, str.size());
		}

#ifdef SEQ_HAS_CPP_17
		auto rfind(const std::basic_string_view<Char, Traits>& str, size_t pos = npos) const noexcept -> size_t
		{
			return rfind(str.data(), pos, str.size());
		}
#endif

		auto rfind(const Char* s, size_t pos = npos) const noexcept -> size_t
		{
			return rfind(s, pos, Traits::length(s));
		}
		auto rfind(const Char* s, size_t pos, size_type n) const noexcept -> size_t
		{
			return detail::traits_string_rfind(data(), pos, size(), s, n, npos);
		}
		auto rfind(Char c, size_t pos = npos) const noexcept -> size_t
		{
			if (pos >= size())
				pos = size() - 1;
			const Char* p = detail::string_memrchr(data(), c, pos + 1);
			return p == nullptr ? npos : static_cast<size_t>(p - data());
		}



		auto find_first_of(const Char* s, size_t pos, size_t n) const noexcept -> size_t
		{
			return detail::traits_string_find_first_of<Traits>(data(), pos, size(), s, n, npos);
		}
		template<class A, size_t S>
		auto find_first_of(const tiny_string< Char, Traits, A, S>& str, size_t pos = 0) const noexcept -> size_t
		{
			return find_first_of(str.data(), pos, str.size());
		}
		auto find_first_of(const Char* s, size_t pos = 0) const noexcept -> size_t
		{
			return find_first_of(s, pos, Traits::length(s));
		}
		auto find_first_of(Char c, size_t pos = 0) const noexcept -> size_t
		{
			return find(c, pos);
		}

		template<class A, size_t S>
		auto find_last_of(const tiny_string<Char, Traits, A, S>& str, size_t pos = npos) const noexcept -> size_t
		{
			return find_last_of(str.data(), pos, str.size());
		}
		auto find_last_of(const Char* s, size_t pos = npos) const noexcept -> size_t
		{
			return find_last_of(s, pos, Traits::length(s));
		}
		auto find_last_of(const Char* s, size_t pos, size_t n) const noexcept -> size_t
		{
			return detail::traits_string_find_last_of(data(), pos, size(), s, n, npos);
		}
		auto find_last_of(Char c, size_t pos = npos) const noexcept -> size_t
		{
			return rfind(c, pos);
		}

		template<class A, size_t S>
		auto find_first_not_of(const tiny_string<Char, Traits, A, S>& str, size_t pos = 0) const noexcept -> size_t
		{
			return find_first_not_of(str.data(), pos, str.size());
		}
		auto find_first_not_of(const Char* s, size_t pos = 0) const noexcept -> size_t
		{
			return find_first_not_of(s, pos, Traits::length(s));
		}
		auto find_first_not_of(const Char* s, size_t pos, size_t n) const noexcept -> size_t
		{
			return detail::traits_string_find_first_not_of<Traits>(data(), pos, size(), s, n, npos);
		}
		auto find_first_not_of(Char c, size_t pos = 0) const noexcept -> size_t
		{
			const Char* e = end();
			for (const Char* p = data() + pos; p != e; ++p) {
				if (*p != c) { return static_cast<size_t>(p - data()); }
			}
			return npos;
		}

		template<class A, size_t S>
		auto find_last_not_of(const tiny_string<Char, Traits, A, S>& str, size_t pos = npos) const noexcept -> size_t
		{
			return find_last_not_of(str.data(), pos, str.size());
		}
		auto find_last_not_of(const Char* s, size_t pos = npos) const noexcept -> size_t
		{
			return find_last_not_of(s, pos, Traits::length(s));
		}
		auto find_last_not_of(const Char* s, size_t pos, size_t n) const noexcept -> size_t
		{
			return detail::traits_string_find_last_not_of(data(), pos, size(), s, n, npos);
		}
		auto find_last_not_of(Char c, size_t pos = npos) const noexcept -> size_t
		{
			if (size() == 0) { return npos; }
			if (pos >= size()) { pos = size() - 1; }
			const Char* p = data();
			for (const Char* in = p + pos; in >= p; --in) {
				if (*in != c) {
					return static_cast<size_t>(in - p);
				}
			}
			return npos;
		}

		template<class A, size_t S>
		auto compare(const tiny_string<Char, Traits, A, S>& str) const noexcept -> int
		{
			return detail::traits_string_compare<Traits>(as_pair(), str.as_pair());
		}
		template<class Al>
		auto compare(size_t pos, size_t len, const std::basic_string<Char, Traits, Al>& str) const noexcept -> int
		{
			return compare(pos, len, str.data(), str.size());
		}

#ifdef SEQ_HAS_CPP_17
		int compare(size_t pos, size_t len, const std::basic_string_view<Char, Traits>& str) const noexcept
		{
			return compare(pos, len, str.data(), str.size());
		}
#endif
		template<class A, size_t S>
		auto compare(size_t pos, size_t len, const tiny_string<Char, Traits, A, S>& str, size_t subpos, size_t sublen) const noexcept -> int
		{
			if (sublen == npos || subpos + sublen > str.size()) {
				sublen = str.size() - subpos;
			}
			return compare(pos, len, str.data() + subpos, sublen);
		}
		auto compare(const Char* s) const noexcept -> int
		{
			return compare(0, size(), s);
		}
		auto compare(size_t pos, size_t len, const Char* s) const noexcept -> int
		{
			return compare(pos, len, s, Traits::length(s));
		}
		auto compare(size_t pos, size_t len, const Char* _s, size_t n) const noexcept -> int
		{

			if (len == npos )//|| pos + len > size())
				len = size() - pos;
			return detail::traits_string_compare<Traits>(data() + pos, len, _s, n);
		}

		/// @brief Implicit conversion to std::string
		template<class Al>
		operator std::basic_string<Char, Traits, Al>() const {
			return std::basic_string<Char, Traits, Al>(data(), size());
		}
	};











	/**********************************
	* Comparison operators
	* ********************************/

	template<class Char, class Traits, class A1, class A2, size_t S1, size_t S2>
	SEQ_STR_INLINE auto operator== (const tiny_string<Char, Traits, A1, S1>& lhs, const tiny_string<Char, Traits, A2, S2>& rhs) noexcept -> bool {
		return detail::traits_string_equal<Traits>(lhs.as_pair(), rhs.as_pair());//lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}
	template<class Char, class Traits, class Al, size_t S>
	SEQ_STR_INLINE auto operator== (const Char* lhs, const tiny_string<Char, Traits, Al, S>& rhs) noexcept -> bool {
		return detail::traits_string_equal<Traits>(lhs, Traits::length(lhs), rhs.data(), rhs.size());
	}
	template<class Char, class Traits, class Al, size_t S>
	SEQ_STR_INLINE auto operator== (const tiny_string<Char, Traits, Al, S>& lhs, const Char* rhs) noexcept -> bool {
		return detail::traits_string_equal<Traits>(lhs.data(), lhs.size(), rhs, Traits::length(rhs));
	}
	template<class Char, class Traits, class Al, class Al2, size_t S>
	SEQ_STR_INLINE auto operator== (const std::basic_string<Char, Traits, Al>& lhs, const tiny_string<Char, Traits, Al2, S>& rhs)noexcept -> bool {
		return detail::traits_string_equal<Traits>(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}
	template<class Char, class Traits, class Al, class Al2, size_t S>
	SEQ_STR_INLINE auto operator== (const tiny_string<Char, Traits, Al2, S>& lhs, const std::basic_string<Char, Traits, Al>& rhs) noexcept -> bool {
		return detail::traits_string_equal<Traits>(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}

#ifdef SEQ_HAS_CPP_17
	template<class Char, class Traits, class Al, size_t S>
	SEQ_STR_INLINE bool operator== (const std::basic_string_view<Char, Traits>& lhs, const tiny_string<Char, Traits, Al, S>& rhs)noexcept {
		return detail::traits_string_equal<Traits>(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}
	template<class Char, class Traits, class Al, size_t S>
	SEQ_STR_INLINE bool operator== (const tiny_string<Char, Traits, Al, S>& lhs, const std::basic_string_view<Char, Traits>& rhs) noexcept {
		return detail::traits_string_equal<Traits>(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}
#endif



	template<class Char, class Traits, class A1, class A2, size_t S1, size_t S2>
	SEQ_STR_INLINE auto operator!= (const tiny_string<Char, Traits, A1, S1>& lhs, const tiny_string<Char, Traits, A2, S2>& rhs) noexcept -> bool {
		return !(lhs == rhs);
	}
	template<class Char, class Traits, class Al, size_t S>
	SEQ_STR_INLINE auto operator!= (const Char* lhs, const tiny_string<Char, Traits, Al, S>& rhs) noexcept -> bool {
		return !(lhs == rhs);
	}
	template<class Char, class Traits, class Al, size_t S>
	SEQ_STR_INLINE auto operator!= (const tiny_string<Char, Traits, Al, S>& lhs, const Char* rhs) noexcept -> bool {
		return !(lhs == rhs);
	}
	template<class Char, class Traits, class Al, class Al2, size_t S>
	SEQ_STR_INLINE auto operator!= (const std::basic_string<Char, Traits, Al>& lhs, const tiny_string<Char, Traits, Al2, S>& rhs)noexcept -> bool {
		return !(lhs == rhs);
	}
	template<class Char, class Traits, class Al, class Al2, size_t S>
	SEQ_STR_INLINE auto operator!= (const tiny_string<Char, Traits, Al2, S>& lhs, const std::basic_string<Char, Traits, Al>& rhs) noexcept -> bool {
		return !(lhs == rhs);
	}

#ifdef SEQ_HAS_CPP_17
	template<class Char, class Traits, class Al, size_t S>
	SEQ_STR_INLINE bool operator!= (const std::basic_string_view<Char, Traits>& lhs, const tiny_string<Char, Traits, Al, S>& rhs)noexcept {
		return !(lhs == rhs);
	}
	template<class Char, class Traits, class Al, size_t S>
	SEQ_STR_INLINE bool operator!= (const tiny_string<Char, Traits, Al, S>& lhs, const std::basic_string_view<Char, Traits>& rhs) noexcept {
		return !(lhs == rhs);
	}
#endif



	template<class Char, class Traits, class A1, class A2, size_t S1, size_t S2>
	SEQ_STR_INLINE_STRONG auto operator< (const tiny_string<Char, Traits, A1, S1>& lhs, const tiny_string<Char, Traits, A2, S2>& rhs) noexcept -> bool {
		return detail::traits_string_inf<Traits>(lhs.as_pair(), rhs.as_pair());//lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}
	template<class Char, class Traits, class Al, size_t S>
	SEQ_STR_INLINE_STRONG auto operator< (const Char* lhs, const tiny_string<Char, Traits, Al, S>& rhs) noexcept -> bool {
		return detail::traits_string_inf<Traits>(lhs, Traits::length(lhs), rhs.data(), rhs.size());
	}
	template<class Char, class Traits, class Al, size_t S>
	SEQ_STR_INLINE_STRONG auto operator< (const tiny_string<Char, Traits, Al, S>& lhs, const Char* rhs) noexcept -> bool {
		return detail::traits_string_inf<Traits>(lhs.data(), lhs.size(), rhs, Traits::length(rhs));
	}
	template<class Char, class Traits, class Al, class Al2, size_t S>
	SEQ_STR_INLINE_STRONG auto operator< (const std::basic_string<Char, Traits, Al>& lhs, const tiny_string<Char, Traits, Al2, S>& rhs)noexcept -> bool {
		return detail::traits_string_inf<Traits>(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}
	template<class Char, class Traits, class Al, class Al2, size_t S>
	SEQ_STR_INLINE_STRONG auto operator< (const tiny_string<Char, Traits, Al2, S>& lhs, const std::basic_string<Char, Traits, Al>& rhs) noexcept -> bool {
		return detail::traits_string_inf<Traits>(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}

#ifdef SEQ_HAS_CPP_17
	template<class Char, class Traits, class Al, size_t S>
	SEQ_STR_INLINE_STRONG bool operator< (const std::basic_string_view<Char, Traits>& lhs, const tiny_string<Char, Traits, Al, S>& rhs)noexcept {
		return detail::traits_string_inf<Traits>(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}
	template<class Char, class Traits, class Al, size_t S>
	SEQ_STR_INLINE_STRONG bool operator< (const tiny_string<Char, Traits, Al, S>& lhs, const std::basic_string_view<Char, Traits>& rhs) noexcept {
		return detail::traits_string_inf<Traits>(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}
#endif



	template<class Char, class Traits, class A1, class A2, size_t S1, size_t S2>
	SEQ_STR_INLINE auto operator<= (const tiny_string<Char, Traits, A1, S1>& lhs, const tiny_string<Char, Traits, A2, S2>& rhs) noexcept -> bool {
		return detail::traits_string_inf_equal<Traits>(lhs.as_pair(),rhs.as_pair());// lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}
	template<class Char, class Traits, class Al, size_t S>
	SEQ_STR_INLINE auto operator<= (const Char* lhs, const tiny_string<Char, Traits, Al, S>& rhs) noexcept -> bool {
		return detail::traits_string_inf_equal<Traits>(lhs, Traits::length(lhs), rhs.data(), rhs.size());
	}
	template<class Char, class Traits, class Al, size_t S>
	SEQ_STR_INLINE auto operator<= (const tiny_string<Char, Traits, Al, S>& lhs, const Char* rhs) noexcept -> bool {
		return detail::traits_string_inf_equal<Traits>(lhs.data(), lhs.size(), rhs, Traits::length(rhs));
	}
	template<class Char, class Traits, class Al, class Al2, size_t S>
	SEQ_STR_INLINE auto operator<= (const std::basic_string<Char, Traits, Al>& lhs, const tiny_string<Char, Traits, Al2, S>& rhs)noexcept -> bool {
		return detail::traits_string_inf_equal<Traits>(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}
	template<class Char, class Traits, class Al, class Al2, size_t S>
	SEQ_STR_INLINE auto operator<= (const tiny_string<Char, Traits, Al2, S>& lhs, const std::basic_string<Char, Traits, Al>& rhs) noexcept -> bool {
		return detail::traits_string_inf_equal<Traits>(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}

#ifdef SEQ_HAS_CPP_17
	template<class Char, class Traits, class Al, size_t S>
	SEQ_STR_INLINE bool operator<= (const std::basic_string_view<Char, Traits>& lhs, const tiny_string<Char, Traits, Al, S>& rhs)noexcept {
		return detail::traits_string_inf_equal<Traits>(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}
	template<class Char, class Traits, class Al, size_t S>
	SEQ_STR_INLINE bool operator<= (const tiny_string<Char, Traits, Al, S>& lhs, const std::basic_string_view<Char, Traits>& rhs) noexcept {
		return detail::traits_string_inf_equal<Traits>(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}
#endif



	template<class Char, class Traits, class A1, class A2, size_t S1, size_t S2>
	SEQ_STR_INLINE auto operator> (const tiny_string<Char, Traits, A1, S1>& lhs, const tiny_string<Char, Traits, A2, S2>& rhs) noexcept -> bool {
		return detail::traits_string_sup<Traits>(lhs.as_pair(),rhs.as_pair());// lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}
	template<class Char, class Traits, class Al, size_t S>
	SEQ_STR_INLINE auto operator> (const Char* lhs, const tiny_string<Char, Traits, Al, S>& rhs) noexcept -> bool {
		return detail::traits_string_sup<Traits>(lhs, Traits::length(lhs), rhs.data(), rhs.size());
	}
	template<class Char, class Traits, class Al, size_t S>
	SEQ_STR_INLINE auto operator> (const tiny_string<Char, Traits, Al, S>& lhs, const Char* rhs) noexcept -> bool {
		return detail::traits_string_sup<Traits>(lhs.data(), lhs.size(), rhs, Traits::length(rhs));
	}
	template<class Char, class Traits, class Al, class Al2, size_t S>
	SEQ_STR_INLINE auto operator> (const std::basic_string<Char, Traits, Al>& lhs, const tiny_string<Char, Traits, Al2, S>& rhs)noexcept -> bool {
		return detail::traits_string_sup<Traits>(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}
	template<class Char, class Traits, class Al, class Al2, size_t S>
	SEQ_STR_INLINE auto operator> (const tiny_string<Char, Traits, Al2, S>& lhs, const std::basic_string<Char, Traits, Al>& rhs) noexcept -> bool {
		return detail::traits_string_sup<Traits>(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}

#ifdef SEQ_HAS_CPP_17
	template<class Char, class Traits, class Al, size_t S>
	SEQ_STR_INLINE bool operator> (const std::basic_string_view<Char, Traits>& lhs, const tiny_string<Char, Traits, Al, S>& rhs)noexcept {
		return detail::traits_string_sup<Traits>(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}
	template<class Char, class Traits, class Al, size_t S>
	SEQ_STR_INLINE bool operator> (const tiny_string<Char, Traits, Al, S>& lhs, const std::basic_string_view<Char, Traits>& rhs) noexcept {
		return detail::traits_string_sup<Traits>(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}
#endif


	template<class Char, class Traits, class A1, class A2, size_t S1, size_t S2>
	SEQ_STR_INLINE auto operator>= (const tiny_string<Char, Traits, A1, S1>& lhs, const tiny_string<Char, Traits, A2, S2>& rhs) noexcept -> bool {
		return detail::traits_string_sup_equal<Traits>(lhs.as_pair(),rhs.as_pair());// lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}
	template<class Char, class Traits, class Al, size_t S>
	SEQ_STR_INLINE auto operator>= (const Char* lhs, const tiny_string<Char, Traits, Al, S>& rhs) noexcept -> bool {
		return detail::traits_string_sup_equal<Traits>(lhs, Traits::length(lhs), rhs.data(), rhs.size());
	}
	template<class Char, class Traits, class Al, size_t S>
	SEQ_STR_INLINE auto operator>= (const tiny_string<Char, Traits, Al, S>& lhs, const Char* rhs) noexcept -> bool {
		return detail::traits_string_sup_equal<Traits>(lhs.data(), lhs.size(), rhs, Traits::length(rhs));
	}
	template<class Char, class Traits, class Al, class Al2, size_t S>
	SEQ_STR_INLINE auto operator>= (const std::basic_string<Char, Traits, Al>& lhs, const tiny_string<Char, Traits, Al2, S>& rhs)noexcept -> bool {
		return detail::traits_string_sup_equal<Traits>(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}
	template<class Char, class Traits, class Al, class Al2, size_t S>
	SEQ_STR_INLINE auto operator>= (const tiny_string<Char, Traits, Al2, S>& lhs, const std::basic_string<Char, Traits, Al>& rhs) noexcept -> bool {
		return detail::traits_string_sup_equal<Traits>(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}

#ifdef SEQ_HAS_CPP_17
	template<class Char, class Traits, class Al, size_t S>
	SEQ_STR_INLINE bool operator>= (const std::basic_string_view<Char, Traits>& lhs, const tiny_string<Char, Traits, Al, S>& rhs)noexcept {
		return detail::traits_string_sup_equal<Traits>(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}
	template<class Char, class Traits, class Al, size_t S>
	SEQ_STR_INLINE bool operator>= (const tiny_string<Char, Traits, Al, S>& lhs, const std::basic_string_view<Char, Traits>& rhs) noexcept {
		return detail::traits_string_sup_equal<Traits>(lhs.data(), lhs.size(), rhs.data(), rhs.size());
	}
#endif



	////////////////////////////////////////////////////////////////
	// Concatenation operators
	////////////////////////////////////////////////////////////////

	namespace detail
	{
		template<class T>
		struct GetStaticSize
		{
			static constexpr size_t value = 0;
		};
		template<class Char, class Traits, class Al, size_t S>
		struct GetStaticSize<tiny_string<Char, Traits, Al, S> >
		{
			static constexpr size_t value = S;
		};
		template<class S1, class S2>
		struct FindStaticSize
		{
			static constexpr size_t value = GetStaticSize<S1>::value > GetStaticSize<S2>::value ? GetStaticSize<S1>::value : GetStaticSize<S2>::value;
		};
		template<class Al1, class Al2>
		struct FindAllocator
		{
			using allocator_type = Al1;
			static auto select(const Al1& al1, const Al2& /*unused*/) -> Al1 { return al1; }
		};
		template<class Char>
		struct FindAllocator<view_allocator<Char>, view_allocator<Char> >
		{
			using allocator_type = std::allocator<Char>;
			static auto select(const view_allocator<Char>& /*unused*/, const view_allocator<Char>& /*unused*/) -> std::allocator<Char> { return {}; }
		};
		template<class Al1, class Char>
		struct FindAllocator<Al1, view_allocator<Char> >
		{
			using allocator_type = Al1;
			static auto select(const Al1& al1, const view_allocator<Char>& /*unused*/) -> Al1 { return al1; }
		};
		template<class Char, class Al2>
		struct FindAllocator< view_allocator<Char>, Al2>
		{
			using allocator_type = Al2;
			static auto select(const view_allocator<Char>&  /*unused*/, const Al2& al2) -> Al2 { return al2; }
		};
		template<class Char, class Traits, class S1, class S2, class Al = void>
		struct FindReturnType
		{
			using type = tiny_string<Char, Traits, typename FindAllocator<Al, Al>::allocator_type, FindStaticSize<S1, S2>::value >;
		};
		template<class Char, class Traits, class S1, class S2>
		struct FindReturnType<Char, Traits, S1, S2, void>
		{
			using type = tiny_string<Char, Traits, typename FindAllocator<typename S1::allocator_type, typename S2::allocator_type>::allocator_type, FindStaticSize<S1, S2>::value >;
		};
	} //end namespace detail

	template<class Char, class Traits, class Al, size_t Size, class Al2, size_t Size2>
	SEQ_STR_INLINE auto operator+ (const tiny_string<Char, Traits, Al, Size>& lhs, const tiny_string<Char, Traits, Al2, Size2>& rhs) -> typename detail::FindReturnType<Char, Traits, tiny_string<Char, Traits, Al, Size>, tiny_string<Char, Traits, Al2, Size2> >::type
	{
		using find_alloc = detail::FindAllocator<Al, Al2>;
		using ret_type = typename detail::FindReturnType<Char, Traits, tiny_string<Char, Traits, Al, Size>, tiny_string<Char, Traits, Al2, Size2> >::type;
		ret_type ret(lhs.size() + rhs.size(), static_cast<Char>(0),
			std::allocator_traits<typename ret_type::allocator_type>::select_on_container_copy_construction(find_alloc::select(lhs.get_allocator(), rhs.get_allocator())));
		memcpy(ret.data(), lhs.data(), lhs.size() * sizeof(Char));
		memcpy(ret.data() + lhs.size(), rhs.data(), rhs.size() * sizeof(Char));
		return ret;
	}

	template<class Char, class Traits, size_t Size, class Al, class Al2>
	SEQ_STR_INLINE auto operator+ (const tiny_string<Char, Traits, Al, Size>& lhs, const std::basic_string<Char, Traits, Al2>& rhs) -> typename detail::FindReturnType<Char, Traits, tiny_string<Char, Traits, Al, Size>, tiny_string<Char, Traits, Al, Size> >::type
	{
		return lhs + tstring_view(rhs.data(), rhs.size());
	}
#ifdef SEQ_HAS_CPP_17
	template<class Char, class Traits, size_t Size, class Al>
	SEQ_STR_INLINE auto operator+ (const tiny_string<Char, Traits, Al, Size>& lhs, const std::basic_string_view<Char, Traits>& rhs) -> typename detail::FindReturnType<Char, Traits, tiny_string<Char, Traits, Al, Size>, tiny_string<Char, Traits, Al, Size> >::type
	{
		return lhs + tstring_view(rhs.data(), rhs.size());
	}
#endif

	template<class Char, class Traits, size_t Size, class Al>
	SEQ_STR_INLINE auto operator+ (const tiny_string<Char, Traits, Al, Size>& lhs, const Char* rhs) -> typename detail::FindReturnType<Char, Traits, tiny_string<Char, Traits, Al, Size>, tiny_string<Char, Traits, Al, Size> >::type
	{
		return lhs + tstring_view(rhs);
	}
	template<class Char, class Traits, size_t Size, class Al>
	SEQ_STR_INLINE auto operator+ (const tiny_string<Char, Traits, Al, Size>& lhs, char rhs) -> typename detail::FindReturnType<Char, Traits, tiny_string<Char, Traits, Al, Size>, tiny_string<Char, Traits, Al, Size> >::type
	{
		return lhs + tstring_view(&rhs, 1);
	}

	template<class Char, class Traits, size_t Size, class Al, class Al2>
	SEQ_STR_INLINE auto operator+ (const std::basic_string<Char, Traits, Al2>& lhs, const tiny_string<Char, Traits, Al, Size>& rhs) -> typename detail::FindReturnType<Char, Traits, tiny_string<Char, Traits, Al, Size>, tiny_string<Char, Traits, Al, Size> >::type
	{
		return tstring_view(lhs.data(), lhs.size()) + rhs;
	}
#ifdef SEQ_HAS_CPP_17
	template<class Char, class Traits, size_t Size, class Al>
	SEQ_STR_INLINE auto operator+ (const std::basic_string_view<Char, Traits>& lhs, const tiny_string<Char, Traits, Al, Size>& rhs) -> typename detail::FindReturnType<Char, Traits, tiny_string<Char, Traits, Al, Size>, tiny_string<Char, Traits, Al, Size>  >::type
	{
		return tstring_view(lhs.data(), lhs.size()) + rhs;
	}
#endif


	template<class Char, class Traits, size_t Size, class Al>
	SEQ_STR_INLINE auto operator+ (const Char* lhs, const tiny_string<Char, Traits, Al, Size>& rhs) -> typename detail::FindReturnType<Char, Traits, tiny_string<Char, Traits, Al, Size>, tiny_string<Char, Traits, Al, Size>  >::type
	{
		return tstring_view(lhs) + rhs;
	}
	template<class Char, class Traits, size_t Size, class Al>
	SEQ_STR_INLINE auto operator+ (Char lhs, const tiny_string<Char, Traits, Al, Size>& rhs) -> typename detail::FindReturnType<Char, Traits, tiny_string<Char, Traits, Al, Size>, tiny_string<Char, Traits, Al, Size>  >::type
	{
		return tstring_view(&lhs, 1) + rhs;
	}

	/// @brief Returns the string data (const char*) for given string object
	template<class Char, class Traits, class Al>
	inline auto string_data(const std::basic_string<Char, Traits, Al>& str) -> const Char* { return str.data(); }
	template<class Char, class Traits, class Al, size_t S>
	inline auto string_data(const tiny_string<Char, Traits, Al, S>& str) -> const Char* { return str.data(); }
	template<class Char>
	inline auto string_data(const Char* str) -> const Char* { return str; }
#ifdef SEQ_HAS_CPP_17
	template<class Char, class Traits>
	inline auto string_data(const std::basic_string_view<Char, Traits>& str) -> const Char* { return str.data(); }
#endif

	/// @brief Returns the string size for given string object
	template<class Char, class Traits, class Al>
	inline auto string_size(const std::basic_string<Char, Traits, Al>& str) -> size_t { return str.size(); }
	template<class Char, class Traits, class Al, size_t S>
	inline auto string_size(const tiny_string<Char, Traits, Al, S>& str) -> size_t { return str.size(); }
	template<class Char>
	inline auto string_size(const Char* str) -> size_t { return std::char_traits<Char>::length(str); }
#ifdef SEQ_HAS_CPP_17
	template<class Char, class Traits>
	inline auto string_size(const std::basic_string_view<Char, Traits>& str) -> size_t { return str.size(); }
#endif




	/// @brief Detect tiny_string
	template<class T>
	struct is_tiny_string : std::false_type {};
	template<class Char, class Traits, class Al, size_t S>
	struct is_tiny_string<tiny_string<Char, Traits, Al, S> > : std::true_type {};

	/// @brief Detect basic_string
	template<class T>
	struct is_basic_string : std::false_type {};
	template<class Char, class Traits, class Al>
	struct is_basic_string<std::basic_string<Char, Traits, Al> > : std::true_type {};

	/// @brief Detect basic_string
	template<class T>
	struct is_basic_string_view : std::false_type {};
#ifdef SEQ_HAS_CPP_17
	template<class Char, class Traits>
	struct is_basic_string_view<std::basic_string_view<Char, Traits> > : std::true_type {};
#endif

	/// @brief Detect tiny_string, std::string, but not tstring_view
	template<class T>
	struct is_allocated_string : std::false_type {};
	template<class Char, class Traits, class Al, size_t S>
	struct is_allocated_string<tiny_string<Char, Traits, Al, S>>
	{
		static constexpr bool value = !std::is_same<Al, view_allocator<Char> >::value;
	};
	template<class Char, class Traits, class Allocator >
	struct is_allocated_string<std::basic_string<Char, Traits, Allocator>> : std::true_type {};


	template< class C >
	struct is_char : std::integral_constant<bool, std::is_same<C, char>::value || std::is_same<C, char16_t>::value || std::is_same<C, char32_t>::value || std::is_same<C, wchar_t>::value> {};

	/// @brief Detect all possible string types (std::string, tstring, tstring_view, std::string_view, const char*, char*
	template<class T>
	struct is_generic_string2 : std::false_type {};
	template<class T>
	struct is_generic_string2<T*> : is_char<T> {};
	template<class T>
	struct is_generic_string2<const T*> : is_char<T> {};
	template<class Char, class Traits, class Allocator >
	struct is_generic_string2<std::basic_string<Char, Traits, Allocator> > : std::true_type {};
	template<class Char, class Traits, class Al, size_t S>
	struct is_generic_string2<tiny_string<Char, Traits, Al, S>> : std::true_type {};
#ifdef SEQ_HAS_CPP_17
	template<class Char, class Traits>
	struct is_generic_string2<std::basic_string_view<Char, Traits> > : std::true_type {};
#endif

	/// @brief Detect all possible string types (std::string, tstring, tstring_view, std::string_view, const char*, char*
	template<class T>
	struct is_generic_char_string : std::false_type {};
	template<>
	struct is_generic_char_string<char*> : std::true_type {};
	template<>
	struct is_generic_char_string<const char*> : std::true_type {};
	template< class Traits, class Allocator >
	struct is_generic_char_string<std::basic_string<char, Traits, Allocator> > : std::true_type {};
	template< class Traits, class Al, size_t S>
	struct is_generic_char_string<tiny_string<char, Traits, Al, S>> : std::true_type {};
#ifdef SEQ_HAS_CPP_17
	template< class Traits>
	struct is_generic_char_string<std::basic_string_view<char, Traits> > : std::true_type {};
#endif

	/// @brief Detect tstring_view or std::string_view
	template<class T>
	struct is_string_view : std::false_type {};
	template<class Char, class Traits>
	struct is_string_view<tiny_string<Char, Traits, view_allocator<Char>, 0> > : std::true_type {};
#ifdef SEQ_HAS_CPP_17
	template<class Char, class Traits>
	struct is_string_view<std::basic_string_view<Char, Traits> > : std::true_type {};
#endif


	/// @brief Detect generic string view: tstring_view, std::string_view, char*, const char*
	template<class T>
	struct is_generic_string_view : std::false_type {};
	template<class Char, class Traits>
	struct is_generic_string_view<tiny_string<Char, Traits, view_allocator<Char>, 0> > : std::true_type {};
	template<class Char>
	struct is_generic_string_view<Char*> : is_char<Char> {};
	template<class Char>
	struct is_generic_string_view<const Char*> : is_char<Char> {};
#ifdef SEQ_HAS_CPP_17
	template<class Char, class Traits>
	struct is_generic_string_view<std::basic_string_view<Char, Traits> > : std::true_type {};
#endif



	// Specialization of is_relocatable

	template<class Char>
	struct is_relocatable<view_allocator<Char> > : std::true_type {};

	template<class Char, class Traits, class Al, size_t S>
	struct is_relocatable<tiny_string<Char, Traits, Al, S> > : is_relocatable<Al> {};





	/**********************************
	* Reading/writing from/to streams
	* ********************************/

	template<class Elem, class Traits, size_t Size, class Alloc>
	inline auto operator>>(std::basic_istream<Elem, Traits>& iss, tiny_string<Elem, Traits, Alloc, Size>& str)
		-> typename std::enable_if<!std::is_same<Alloc, view_allocator<Elem> >::value, std::basic_istream<Elem, Traits> >::type&
	{	// extract a string
		typedef std::ctype<Elem> c_type;
		typedef std::basic_istream<Elem, Traits> stream_type;
		typedef tiny_string<Elem, Traits, Alloc, Size> string_type;
		typedef typename string_type::size_type size_type;

		std::ios_base::iostate state = std::ios_base::goodbit;
		bool changed = false;
		const typename stream_type::sentry ok(iss);

		if (ok)
		{	// state okay, extract characters
			const c_type& ctype_fac = std::use_facet< c_type >(iss.getloc());
			str.clear();

			try {
				size_type size = 0 < iss.width()
					&& static_cast<size_type>(iss.width()) < str.max_size()
					? static_cast<size_type>(iss.width()) : str.max_size();
				typename Traits::int_type _Meta = iss.rdbuf()->sgetc();

				for (; 0 < size; --size, _Meta = iss.rdbuf()->snextc())
					if (Traits::eq_int_type(Traits::eof(), _Meta))
					{	// end of file, quit
						state |= std::ios_base::eofbit;
						break;
					}
					else if (ctype_fac.is(c_type::space,
						Traits::to_char_type(_Meta)))
						break;	// whitespace, quit
					else
					{	// add character to string
						str.append(1, Traits::to_char_type(_Meta));
						changed = true;
					}

			}
			catch (...) {
				iss.setstate(std::ios_base::badbit);
			}
		}

		iss.width(0);
		if (!changed)
			state |= std::ios_base::failbit;
		iss.setstate(state);
		return (iss);
	}

	template<class Elem, class Traits, size_t Size, class Alloc>
	inline	auto operator<<(std::basic_ostream<Elem, Traits>& oss, const tiny_string<Elem, Traits, Alloc, Size>& str) -> std::basic_ostream<Elem, Traits>&
	{	// insert a string
		typedef std::basic_ostream<Elem, Traits> myos;
		typedef tiny_string<Elem, Traits, Alloc, Size> mystr;
		typedef typename mystr::size_type mysizt;

		std::ios_base::iostate state = std::ios_base::goodbit;
		mysizt size = str.size();
		mysizt pad = oss.width() <= 0 || static_cast<mysizt>(oss.width()) <= size
			? 0 : static_cast<mysizt>(oss.width()) - size;
		const typename myos::sentry sok(oss);

		if (!sok)
			state |= std::ios_base::badbit;
		else
		{	// state okay, insert characters
			try {
				if ((oss.flags() & std::ios_base::adjustfield) != std::ios_base::left)
					for (; 0 < pad; --pad)	// pad on left
						if (Traits::eq_int_type(Traits::eof(),
							oss.rdbuf()->sputc(oss.fill())))
						{	// insertion failed, quit
							state |= std::ios_base::badbit;
							break;
						}

				if (state == std::ios_base::goodbit
					&& oss.rdbuf()->sputn(str.c_str(), static_cast<std::streamsize>(size))
					!= static_cast<std::streamsize>(size))
					state |= std::ios_base::badbit;
				else
					for (; 0 < pad; --pad)	// pad on right
						if (Traits::eq_int_type(Traits::eof(),
							oss.rdbuf()->sputc(oss.fill())))
						{	// insertion failed, quit
							state |= std::ios_base::badbit;
							break;
						}
				oss.width(0);
			}
			catch (...)
			{
				oss.setstate(std::ios_base::badbit);
				throw;
			}
		}

		oss.setstate(state);
		return (oss);
	}



	/// @brief Specialization of hasher for tiny_string
	/// Uses seq hash function hash_bytes_komihash()
	/// 
	template<class Char, class Traits, class Allocator, size_t Size>
	class hasher< seq::tiny_string<Char, Traits, Allocator, Size> >
	{
	public:
		using is_transparent = int;
		using is_avalanching = int;
		SEQ_STR_INLINE_STRONG auto operator()(const seq::tiny_string<Char, Traits, Allocator, Size>& str) const noexcept -> size_t
		{
			return seq::hash_bytes_komihash((str.data()), str.size() * sizeof(Char));
		}
		template<size_t S, class Al>
		SEQ_STR_INLINE_STRONG auto operator()(const seq::tiny_string<Char, Traits, Al, S>& str) const noexcept -> size_t
		{
			return seq::hash_bytes_komihash((str.data()), str.size() * sizeof(Char));
		}
		template< class Al>
		SEQ_STR_INLINE_STRONG auto operator()(const std::basic_string<Char, Traits, Al>& str) const noexcept -> size_t
		{
			return seq::hash_bytes_komihash((str.data()), str.size() * sizeof(Char));
		}
		SEQ_STR_INLINE_STRONG auto operator()(const Char* str) const noexcept -> size_t
		{
			return seq::hash_bytes_komihash((str), Traits::length(str) * sizeof(Char));
		}

#ifdef SEQ_HAS_CPP_17
		SEQ_STR_INLINE_STRONG auto operator()(const std::basic_string_view<Char, Traits>& str) const noexcept -> size_t
		{
			return seq::hash_bytes_komihash((str.data()), str.size() * sizeof(Char));
		}
#endif
	};

	/// @brief Specialization of seq::hasher for basic_string
	/// Uses seq hash function hash_bytes_komihash()
	/// 
	template<class Char, class Traits, class Allocator>
	class hasher< std::basic_string<Char, Traits, Allocator> >
	{
	public:
		using is_transparent = int;
		using is_avalanching = int;

		template<class A, size_t S >
		SEQ_STR_INLINE_STRONG auto operator()(const seq::tiny_string<Char, Traits, A, S>& str) const noexcept -> size_t
		{
			return seq::hash_bytes_komihash((str.data()), str.size() * sizeof(Char));
		}
		template<class A>
		SEQ_STR_INLINE_STRONG auto operator()(const std::basic_string<Char, Traits, A>& str) const noexcept -> size_t
		{
			return seq::hash_bytes_komihash((str.data()), str.size() * sizeof(Char));
		}
		SEQ_STR_INLINE_STRONG auto operator()(const Char* str) const noexcept -> size_t
		{
			return seq::hash_bytes_komihash((str), Traits::length(str) * sizeof(Char));
		}

#ifdef SEQ_HAS_CPP_17
		SEQ_STR_INLINE_STRONG auto operator()(const std::basic_string_view<Char, Traits>& str) const noexcept -> size_t
		{
			return seq::hash_bytes_komihash((str.data()), str.size() * sizeof(Char));
		}
#endif
	};

#ifdef SEQ_HAS_CPP_17
	/// @brief Specialization of seq::hasher for basic_string_view
	/// Uses seq hash function hash_bytes_komihash()
	/// 
	template<class Char, class Traits>
	class hasher< std::basic_string_view<Char, Traits> > : public hasher<std::basic_string<Char, Traits, std::allocator<Char>> > {};
#endif
}//end namespace seq




namespace std
{
	// Overload std::swap for tiny_string. Illegal considering C++ standard, but works on all tested compilers and more efficient than the standard std::swap.
	template<class Char, class Traits, class Allocator, size_t Size>
	SEQ_STR_INLINE_STRONG void swap(seq::tiny_string<Char, Traits, Allocator, Size>& a, seq::tiny_string<Char, Traits, Allocator, Size>& b)
	{
		a.swap(b);
	}

	/// @brief Specialization of std::hash for tiny_string
	/// This specialization uses a (relatively) strong hash function: murmurhash2
	/// 
	template<class Char, class Traits, class Allocator, size_t Size>
	class hash< seq::tiny_string<Char, Traits, Allocator, Size> >
	{
	public:
		using is_transparent = int;
		using is_avalanching = int;
		SEQ_STR_INLINE_STRONG auto operator()(const seq::tiny_string<Char, Traits, Allocator, Size>& str) const noexcept -> size_t
		{
			return seq::hash_bytes_komihash(str.data(), str.size() * sizeof(Char));
		}
		template<size_t S, class Al>
		SEQ_STR_INLINE_STRONG auto operator()(const seq::tiny_string<Char, Traits, Al, S>& str) const noexcept -> size_t
		{
			return seq::hash_bytes_komihash(str.data(), str.size() * sizeof(Char));
		}
		template< class Al>
		SEQ_STR_INLINE_STRONG auto operator()(const std::basic_string<Char, Traits, Al>& str) const noexcept -> size_t
		{
			return seq::hash_bytes_komihash(str.data(), str.size() * sizeof(Char));
		}
		SEQ_STR_INLINE_STRONG auto operator()(const Char* str) const noexcept -> size_t
		{
			return seq::hash_bytes_komihash(str, Traits::length(str) * sizeof(Char));
		}

#ifdef SEQ_HAS_CPP_17
		SEQ_STR_INLINE_STRONG auto operator()(const std::basic_string_view<Char, Traits>& str) const noexcept -> size_t
		{
			return seq::hash_bytes_komihash(str.data(), str.size() * sizeof(Char));
		}
#endif
	};

} //end namespace std


/*
#include "memory.hpp"

namespace seq
{
	namespace detail
	{

		template<class Char, unsigned MaxStaticSize>
		class string_allocator
		{
			static constexpr unsigned MaxSSize = tiny_string<Char, std::char_traits< Char>, std::allocator<Char>, MaxStaticSize>::max_static_size;
			static constexpr size_t start_size = (1ull << (seq::static_bit_scan_reverse<MaxSSize>::value + 1));
			using alloc_strategy = pow_object_allocation<1024, start_size>;

			struct Char64
			{
				Char data[64];
			};
			using string_pool_type = parallel_object_pool<Char, std::allocator<Char> ,0, block_object_allocation<128, 8> >;// , 0, alloc_strategy > ;

			

		public:
			static SEQ_STR_INLINE_STRONG string_pool_type& get_string_pool() {
				static string_pool_type pool;
				return pool;
			}

			using value_type = Char;
			typedef Char* pointer;
			typedef const Char* const_pointer;
			typedef Char& reference;
			typedef const Char& const_reference;
			using size_type = size_t;
			using difference_type = ptrdiff_t;

			using propagate_on_container_move_assignment = std::false_type;
			using is_always_equal = std::true_type;

			template <class _Other>
			struct rebind {
				using other = string_allocator< _Other, MaxStaticSize >;
			};

			constexpr string_allocator() noexcept {}
			constexpr string_allocator(const string_allocator&) noexcept = default;
			template<class O, unsigned S>
			string_allocator(const string_allocator<O, S>&) noexcept {}
			~string_allocator() noexcept = default;
			string_allocator& operator=(const string_allocator&) noexcept = default;
			template<class O, unsigned S>
			string_allocator& operator=(const string_allocator<O, S>&) noexcept {
				return *this;
			};
			bool operator==(const string_allocator&) const noexcept { return true; }
			bool operator!=(const string_allocator&) const noexcept { return false; }
			void deallocate(Char* ptr, const size_t count) {
				//count = count / sizeof(Char64) + (count % sizeof(Char64) ? 1 : 0);
				get_string_pool().deallocate(ptr, count);
			}
			Char* allocate( size_t count) {
				//count = count / sizeof(Char64) + (count % sizeof(Char64) ? 1 : 0);
				return (Char*)get_string_pool().allocate(count);
			}
			Char* allocate(size_t count, const void*) {
				//count = count / sizeof(Char64) + (count % sizeof(Char64) ? 1 : 0);
				return (Char*)get_string_pool().allocate(count);
			}
			size_t max_size() const noexcept {
				return static_cast<size_t>(-1) ;
			}
		};
	}

	template<class Char, class Traits = std::char_traits<Char>, unsigned MaxStaticSize = 0>
	using ftiny_string = tiny_string<Char, Traits, detail::string_allocator<Char, MaxStaticSize>, MaxStaticSize>;
	using ftstring = ftiny_string<char, std::char_traits<char> >;
	using fwtstring = ftiny_string<wchar_t, std::char_traits<wchar_t> >;
}
*/



#endif
