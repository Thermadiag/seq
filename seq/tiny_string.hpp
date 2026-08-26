/**
 * MIT License
 *
 * Copyright (c) 2026 Victor Moncada <vtr.moncada@gmail.com>
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
#include <string_view>

#include "hash.hpp"
#include "type_traits.hpp"
#include "internal/utils.hpp"

#ifdef min
#undef min
#undef max
#endif

#define SEQ_STR_INLINE_STRONG SEQ_ALWAYS_INLINE

namespace seq
{

	// forward declaration
	template<class Char, class Allocator, size_t MaxStaticSize>
	class tiny_string;

	/// @brief Base string typedef, similar to std::string. Equivalent to tiny_string<0, std::allocator<char>>.
	using tstring = tiny_string<char, std::allocator<char>, 0>;
	using wtstring = tiny_string<wchar_t, std::allocator<wchar_t>, 0>;

	namespace detail
	{
		/*-************************************
		 *  Common functions
		 **************************************/

		// Function missing on msvc and gcc/mingw
		template<class Char>
		auto string_memrchr(const Char* s, Char c, size_t n) noexcept -> const Char*
		{
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

			while (static_cast<size_t>(pInLimit - pIn) >= stepsize) {
				size_t const diff = read_size_t(pMatch) ^ read_size_t(pIn);
				if (diff == 0u) {
					pIn += stepsize;
					pMatch += stepsize;
					continue;
				}
				return static_cast<size_t>(pIn - pStart);
			}

			if ((stepsize == 8) && static_cast<size_t>(pInLimit - pIn) >= sizeof(uint32_t) && (read_32(pMatch) == read_32(pIn))) {
				pIn += 4;
				pMatch += 4;
			}
			if (static_cast<size_t>(pInLimit - pIn) >= sizeof(uint16_t) && (read_16(pMatch) == read_16(pIn))) {
				pIn += 2;
				pMatch += 2;
			}
			if ((pIn < pInLimit) && (*pMatch == *pIn)) {
				pIn++;
			}
			return static_cast<size_t>(pIn - pStart);
		}

		template<class T>
		SEQ_STR_INLINE_STRONG T swap_if_le(T v) noexcept
		{
#if SEQ_BYTEORDER_ENDIAN != SEQ_BYTEORDER_BIG_ENDIAN
			if constexpr (sizeof(T) == 8)
				return byte_swap_64(v);
			else if constexpr (sizeof(T) == 4)
				return byte_swap_32(v);
			else if constexpr (sizeof(T) == 2)
				return byte_swap_16(v);
			else
				return v;
#else
			return v;
#endif
		}

		template<class Traits, class Char>
		SEQ_STR_INLINE_STRONG int traits_string_compare(const Char* v1, size_t l1, const Char* v2, size_t l2) noexcept
		{
			using unsigned_char = typename std::make_unsigned<Char>::type;

			size_t l = l1 < l2 ? l1 : l2;
			if (l && !Traits::eq(*v1, *v2))
				return Traits::lt(*v1, *v2) ? -1 : 1;

#ifndef SEQ_NO_FAST_BSWAP
			if constexpr (sizeof(Char) == 1 && std::is_same_v<Traits, std::char_traits<Char>>) {
				const Char* end = v1 + l;
				while ((end - v1) >= 8) {
					std::uint64_t r1 = read_64(v1);
					std::uint64_t r2 = read_64(v2);
					if (r1 != r2)
						return swap_if_le(r1) < swap_if_le(r2) ? -1 : 1;
					v1 += 8;
					v2 += 8;
				}
				if ((end - v1) >= 4) {
					std::uint32_t r1 = read_32(v1);
					std::uint32_t r2 = read_32(v2);
					if (r1 != r2)
						return swap_if_le(r1) < swap_if_le(r2) ? -1 : 1;
					v1 += 4;
					v2 += 4;
				}
				if ((end - v1) >= 2) {
					std::uint16_t r1 = read_16(v1);
					std::uint16_t r2 = read_16(v2);
					if (r1 != r2)
						return swap_if_le(r1) < swap_if_le(r2) ? -1 : 1;
					v1 += 2;
					v2 += 2;
				}
				if (v1 != end) {
					if (*v1 != *v2) {
						return static_cast<unsigned_char>(*v1) < static_cast<unsigned_char>(*v2) ? -1 : 1;
					}
				}
				return (l1 < l2 ? -1 : (l1 > l2));
			}
			else
#endif
			{
				const int r = Traits::compare(v1, v2, l);
				return r != 0 ? r : (l1 < l2 ? -1 : (l1 > l2));
			}
		}

		template<class Traits, class Char>
		SEQ_STR_INLINE_STRONG bool traits_string_inf(const Char* v1, size_t l1, const Char* v2, size_t l2) noexcept
		{
			return traits_string_compare<Traits>(v1, l1, v2, l2) < 0;
		}
		template<class Traits, class Char>
		SEQ_STR_INLINE_STRONG bool traits_string_inf(const std::pair<const Char*, size_t>& p1, const std::pair<const Char*, size_t>& p2) noexcept
		{
			return traits_string_compare<Traits>(p1.first, p1.second, p2.first, p2.second) < 0;
		}

		template<class Traits, class Char>
		bool traits_string_sup(const Char* v1, size_t l1, const Char* v2, size_t l2) noexcept
		{
			return traits_string_inf<Traits>(v2, l2, v1, l1);
		}
		template<class Traits, class Pair>
		bool traits_string_sup(const Pair& p1, const Pair& p2) noexcept
		{
			return traits_string_inf<Traits>(p2.first, p2.second, p1.first, p1.second);
		}

		template<class Traits, class Char>
		bool traits_string_inf_equal(const Char* v1, size_t l1, const Char* v2, size_t l2) noexcept
		{
			return !traits_string_inf<Traits>(v2, l2, v1, l1);
		}
		template<class Traits, class Pair>
		bool traits_string_inf_equal(const Pair& p1, const Pair& p2) noexcept
		{
			return traits_string_inf_equal<Traits>(p1.first, p1.second, p2.first, p2.second);
		}

		template<class Traits, class Char>
		bool traits_string_sup_equal(const Char* v1, size_t l1, const Char* v2, size_t l2) noexcept
		{
			return traits_string_inf_equal<Traits>(v2, l2, v1, l1);
		}
		template<class Traits, class Pair>
		bool traits_string_sup_equal(const Pair& p1, const Pair& p2) noexcept
		{
			return traits_string_sup_equal<Traits>(p1.first, p1.second, p2.first, p2.second);
		}

		template<class Traits, class Char>
		bool traits_string_equal(const Char* v1, size_t l1, const Char* v2, size_t l2) noexcept
		{
			return (l1 == l2) && traits_string_compare<Traits>(v1, l1, v2, l2) == 0;
		}
		template<class Traits, class Pair>
		bool traits_string_equal(const Pair& p1, const Pair& p2) noexcept
		{
			return traits_string_equal<Traits>(p1.first, p1.second, p2.first, p2.second);
		}

		template<class Traits, class Char>
		auto traits_string_find_first_of(const Char* data, size_t pos, size_t size, const Char* s, size_t n, size_t npos) noexcept -> size_t
		{
			return std::basic_string_view<Char, Traits>(data, size).find_first_of(s, pos, n);

			// Possible implementation:
			/*
			if (pos >= size)
				return npos;
			const Char* end = data + size;
			if (sizeof(Char) > 1 || size * n < 256U) {
				const Char* send = s + n;
				for (const Char* p = data + pos; p != end; ++p)
					for (const Char* m = s; m != send; ++m)
						if (*m == *p)
							return static_cast<size_t>(p - data);
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
						return static_cast<size_t>(p - data);
				}
			}
			return npos;*/
		}

		template<class Traits, class Char>
		auto traits_string_find_last_of(const Char* data, size_t pos, size_t size, const Char* s, size_t n, size_t npos) noexcept -> size_t
		{
			return std::basic_string_view<Char, Traits>(data, size).find_last_of(s, pos, n);

			// Possible implementation:
			/* if (size == 0)
				return npos;
			if (pos >= size)
				pos = size - 1;
			const Char* p = data;
			if (sizeof(Char) > 1 || size * n < 256U) {
				const Char* send = s + n;
				for (const Char* in = p + pos; in >= p; --in) {
					for (const Char* m = s; m != send; ++m)
						if (*m == *in)
							return static_cast<size_t>(in - p);
				}
			}
			else {
				unsigned char buff[256 / CHAR_BIT];
				memset(buff, 0, sizeof(buff));
				for (size_t i = 0; i < n; ++i) {
					unsigned char v = static_cast<unsigned char>(s[i]);
					buff[v / CHAR_BIT] |= 1U << (v % CHAR_BIT);
				}
				for (const Char* in = p + pos; in >= p; --in) {
					unsigned char v = static_cast<unsigned char>(*in);
					if ((buff[v / CHAR_BIT] >> (v % CHAR_BIT)) & 1U)
						return static_cast<size_t>(in - p);
				}
			}
			return npos;*/
		}

		template<class Traits, class Char>
		auto traits_string_find_first_not_of(const Char* data, size_t pos, size_t size, const Char* s, size_t n, size_t npos) noexcept -> size_t
		{
			return std::basic_string_view<Char, Traits>(data, size).find_first_not_of(s, pos, n);

			// Possible implementation:
			/* const Char* end = data + size;
			const Char* send = s + n;
			for (const Char* p = data + pos; p != end; ++p) {
				const Char* m = s;
				for (; m != send; ++m)
					if (*m == *p)
						break;
				if (m == send)
					return static_cast<size_t>(p - data);
			}
			return npos;*/
		}

		template<class Traits, class Char>
		auto traits_string_find_last_not_of(const Char* data, size_t pos, size_t size, const Char* s, size_t n, size_t npos) noexcept -> size_t
		{
			return std::basic_string_view<Char, Traits>(data, size).find_last_not_of(s, pos, n);

			// Possible implementation:
			/* if (size == 0)
				return npos;
			if (pos >= size)
				pos = size - 1;
			const Char* p = data;
			const Char* send = s + n;
			for (const Char* in = p + pos; in >= p; --in) {
				const Char* m = s;
				for (; m != send; ++m)
					if (*m == *in)
						break;
				if (m == send)
					return static_cast<size_t>(in - p);
			}
			return npos;*/
		}

		template<class Traits, class Char>
		auto traits_string_find(const Char* data, size_t pos, size_t size, const Char* s, size_t n, size_t npos) noexcept -> size_t
		{
			if (pos > size)
				return npos;
			if (n == 0)
				return pos;
			if (n > size - pos)
				return npos;

			const Char* in = data + pos;
			const Char* end = in + (size - pos - n) + 1;
			Char c = *s;
			for (;;) {
				in = Traits::find(in, static_cast<size_t>(end - in), c);
				if (!in)
					return npos;

				// start searching, count_approximate_common_bytes returns (usually) an underestimation of the common bytes, except if equal
				size_t common = detail::count_approximate_common_bytes(in + 1, s + 1, in + n);
				if (common == (n - 1) * sizeof(Char))
					return static_cast<size_t>(in - data);
				++in;
			}
			return npos;
		}

		template<class Traits, class Char>
		auto traits_string_rfind(const Char* data, size_t pos, size_t size, const Char* s, size_t n, size_t npos) noexcept -> size_t
		{
			return std::basic_string_view<Char, Traits>(data, size).rfind(s, pos, n);

			// Possible implementation:
			/*if (n > size || pos < n || n == 0)
				return npos;
			if (pos > size)
				pos = size;
			const Char* beg = data;
			const Char* in = std::min(beg + pos, data + size - n);
			Char c = *s;
			for (;;) {
				in = string_memrchr(beg, c, static_cast<size_t>(in - beg + 1));
				if (!in)
					return npos;
				// start searching
				size_t common = detail::count_approximate_common_bytes(in + 1, s + 1, in + n);
				if (common == (n - 1) * sizeof(Char))
					return static_cast<size_t>(in - data);
				--in;
			}
			return npos;*/
		}

		template<class Char, size_t MaxSSO>
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
			static constexpr bool last_character_overlapp = sizeof(SSOStorage<Char, sso_capacity>) == sizeof(NoneSSOStorage<Char>);

#else
			static constexpr size_t min_capacity_for_BE = SSOStorage<Char, sizeof(NoneSSOStorage<Char>) / sizeof(Char) + 1U>::sso_capacity;
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

			union StorageUnion
			{
				SSOStorage<Char, sso_capacity> sso;
				NoneSSOStorage<Char> non_sso;
			} d_union;

			SEQ_STR_INLINE_STRONG StringHolder() noexcept { reset(); }
			template<class Union>
			SEQ_STR_INLINE_STRONG StringHolder(const Union& u) noexcept
			{
				memcpy(&d_union, &u, sizeof(d_union));
				// avoid a reset operation
			}

			SEQ_STR_INLINE_STRONG unsigned_char last_sso_char() const noexcept { return static_cast<unsigned_char>(d_union.sso.data[sso_capacity - 1]); }
			SEQ_STR_INLINE_STRONG unsigned_char& last_sso_char() noexcept { return reinterpret_cast<unsigned_char*>(d_union.sso.data)[sso_capacity - 1]; }

			SEQ_STR_INLINE_STRONG bool is_sso() const noexcept { return (last_sso_char() & check_sso_mask) != non_sso_mask; }
			static SEQ_STR_INLINE_STRONG bool is_sso(std::uintptr_t size) noexcept { return size < sso_capacity; }
			SEQ_STR_INLINE_STRONG std::uintptr_t sizeSSO() const noexcept { return (last_sso_char() == 0 ? sso_capacity - 1U : last_sso_char() & (sso_mask - 1U)); }
			SEQ_STR_INLINE_STRONG std::uintptr_t sizeNonSSO() const noexcept { return (last_character_overlapp ? (d_union.non_sso.size & (non_sso_flag - 1ULL)) : d_union.non_sso.size); }
			SEQ_STR_INLINE_STRONG std::uintptr_t size(unsigned_char last) const noexcept
			{
				if ((last & check_sso_mask) != non_sso_mask)
					return last == 0 ? (sso_capacity - 1U) : (last & (sso_mask - 1U));
				if constexpr (last_character_overlapp)
					return (d_union.non_sso.size & (non_sso_flag - 1ULL));
				else
					return d_union.non_sso.size;
			}
			SEQ_STR_INLINE_STRONG std::uintptr_t size() const noexcept { return size(last_sso_char()); }
			SEQ_STR_INLINE_STRONG const Char* data() const noexcept { return is_sso() ? d_union.sso.data : (d_union.non_sso.data + char_offset); }
			SEQ_STR_INLINE_STRONG Char* data() noexcept { return is_sso() ? d_union.sso.data : (d_union.non_sso.data + char_offset); }

			SEQ_STR_INLINE_STRONG std::pair<const Char*, size_t> as_pair(const unsigned_char last) const noexcept
			{
				if ((last & check_sso_mask) != non_sso_mask)
					return { d_union.sso.data, (last == 0 ? sso_capacity - 1U : last & (sso_mask - 1U)) };
				return { (d_union.non_sso.data + char_offset), static_cast<size_t>((last_character_overlapp ? (d_union.non_sso.size & (non_sso_flag - 1ULL)) : d_union.non_sso.size)) };
			}
			SEQ_STR_INLINE_STRONG std::pair<const Char*, size_t> as_pair() const noexcept { return as_pair(last_sso_char()); }

			SEQ_STR_INLINE_STRONG void setSizeNonSSO(std::uintptr_t size) noexcept
			{
				if constexpr (last_character_overlapp)
					// last character overlapp with size member: set last 2 bits to '01'
					d_union.non_sso.size = size | non_sso_flag;
				else {
					// no overlapp: just store the size
					d_union.non_sso.size = size;
					// set last character to '01....'
					last_sso_char() = non_sso_mask;
				}
			}
			SEQ_STR_INLINE_STRONG bool setSize(std::uintptr_t size) noexcept
			{
				if (size == sso_capacity - 1) {
					last_sso_char() = 0;
					return true;
				}
				else if (size < sso_capacity - 1) {
					last_sso_char() = static_cast<unsigned_char>(sso_mask | size);
					return true;
				}
				else {
					setSizeNonSSO(size);
					return false;
				}
			}

			SEQ_STR_INLINE_STRONG void setSizeKeepSSOFlag(std::uintptr_t size) noexcept
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
				d_union.sso.data[0] = Char{ 0 };
			}

			SEQ_STR_INLINE_STRONG auto capacity_internal() const noexcept -> size_t
			{
				// returns the capacity
				return is_sso() ? sso_capacity :
						// read_size_t(d_data.d_union.non_sso.data);
					 *reinterpret_cast<const size_t*>(d_union.non_sso.data);
			}
		};

		template<class Char, size_t MaxStaticSize, class Allocator>
		struct string_internal
		  : StringHolder<Char, MaxStaticSize + 1U>
		  , private Allocator
		{
			using base = StringHolder<Char, MaxStaticSize + 1U>;

			static constexpr size_t max_sso_capacity = base::sso_capacity;
			static constexpr size_t max_sso_size = base::sso_capacity - 1U;
			static constexpr size_t max_allowed_sso_capacity = base::max_allowed_sso_capacity;
			static constexpr size_t max_capacity = base::max_capacity;

			static constexpr size_t per_size_t = sizeof(size_t) / sizeof(Char);

			SEQ_STR_INLINE_STRONG string_internal() noexcept(std::is_nothrow_default_constructible_v<Allocator>)
			  : base()
			  , Allocator()
			{
			}
			SEQ_STR_INLINE_STRONG string_internal(const Allocator& al) noexcept(std::is_nothrow_copy_constructible_v<Allocator>)
			  : base()
			  , Allocator(al)
			{
			}
			SEQ_STR_INLINE_STRONG string_internal(Allocator&& al) noexcept(std::is_nothrow_move_constructible_v<Allocator>)
			  : base()
			  , Allocator(std::move(al))
			{
			}
			SEQ_STR_INLINE_STRONG string_internal(Allocator&& al, const string_internal& other) noexcept(std::is_nothrow_move_constructible_v<Allocator>)
			  : base(other.d_union)
			  , Allocator(std::move(al))
			{
			}

			SEQ_STR_INLINE_STRONG constexpr size_t max_size() const
			{
				using word_allocator = typename std::allocator_traits<Allocator>::template rebind_alloc<size_t>;
				using word_traits = std::allocator_traits<word_allocator>;

				word_allocator alloc(get_allocator());
				constexpr size_t max_words = std::numeric_limits<size_t>::max() / per_size_t;
				const size_t words = std::min(static_cast<size_t>(word_traits::max_size(alloc)), max_words);
				const size_t n = words * per_size_t;
				const size_t available = n > per_size_t + 1 ? n - per_size_t - 1 : 0;
				return std::max(max_sso_size, std::min(max_capacity, available));
			}

			auto allocate(size_t n) -> Char*
			{
				// Allocate with size_t alignment
				if (n > std::numeric_limits<size_t>::max() - (per_size_t - 1))
					throw std::length_error("tiny_string allocation too large");

				n = (n + per_size_t - 1) / per_size_t;
				return (Char*)allocate_from<size_t>(get_allocator(), n);
			}
			void deallocate(Char* p, size_t n)
			{
				n = (n + per_size_t - 1) / per_size_t;
				deallocate_from(get_allocator(), (size_t*)p, n);
			}
			SEQ_STR_INLINE_STRONG auto get_allocator() noexcept -> Allocator& { return *this; }
			SEQ_STR_INLINE_STRONG auto get_allocator() const noexcept -> const Allocator& { return *this; }
			SEQ_STR_INLINE_STRONG void swap(string_internal& other) noexcept
			{
				auto tmp = this->d_union;
				this->d_union = other.d_union;
				other.d_union = tmp;
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
	/// Indeed, gcc std::string implementation is not relocatable as it stores a pointer to its internal data for small strings. In addition, most std::string implementations have a size of 32
	/// bytes (at least msvc and gcc ones), which was a lot for my compressed container. Therefore, I started working on a string implementation with the following specificities: -
	/// Relocatable, -	Small footprint (16 bytes on 64 bits machine if possible), -	Customizable Small String Optimization (SSO), -	Higly optimized for small strings (fast copy/move
	/// assignment and fast comparison operators).
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
	template<class Char, class Allocator = std::allocator<Char>, size_t MaxStaticSize = 0>
	class tiny_string
	{
		using Traits = std::char_traits<Char>;
		using internal_data = detail::string_internal<Char, MaxStaticSize, Allocator>;
		using this_type = tiny_string<Char, Allocator, MaxStaticSize>;

		static constexpr size_t sso_max_capacity = internal_data::max_sso_capacity;
		static constexpr size_t sso_max_size = internal_data::max_sso_size;
		static constexpr size_t max_allowed_sso_capacity = internal_data::max_allowed_sso_capacity;
		static constexpr size_t first_allocated_capacity = (1ULL << (static_bit_scan_reverse<sso_max_capacity>::value + 1U)); // TODO: check value
		static constexpr size_t char_offset = sizeof(size_t) / sizeof(Char);

		static_assert(sizeof(Char) == alignof(Char), "invalid Char type alignment");
		static_assert((sizeof(size_t) % sizeof(Char)) == 0, "invalid Char type size");
		static_assert(sizeof(Char) <= alignof(size_t), "invalid Char type size");
		static_assert(MaxStaticSize < max_allowed_sso_capacity, "tiny_string maximum static size is limited to 126 elements");
		static_assert(is_character_type_v<Char>, "invalid character type");
		static_assert(std::is_trivially_copyable_v<Char>);

		internal_data d_data;

		template<class C, class Al, size_t S>
		friend class tiny_string;

		// is it a small string
		SEQ_STR_INLINE_STRONG bool is_sso() const noexcept { return d_data.is_sso(); }
		SEQ_STR_INLINE_STRONG bool is_sso(size_t len) const noexcept { return d_data.is_sso(len); }

		SEQ_STR_INLINE_STRONG auto capacity_internal() const noexcept -> size_t
		{
			// returns the capacity
			return d_data.capacity_internal();
		}
		auto capacity_for_length(size_t len) const -> size_t
		{
			// returns the capacity for given length

			if (len > max_size()) // We need the null character
				throw std::length_error("tiny_string too long");

			if (is_sso(len))
				return sso_max_capacity;

			if (len < first_allocated_capacity)
				return first_allocated_capacity;

			const unsigned bits = bit_scan_reverse_64(len);
			if ((sizeof(size_t) == 4 && bits >= 31) || (sizeof(size_t) == 8 && bits >= 63))
				return max_size() + 1;

			return static_cast<size_t>(1ULL << (1ULL + bits));

			// return is_sso(len) ? sso_max_capacity : (len < first_allocated_capacity ? first_allocated_capacity : (1ULL << (1ULL + (bit_scan_reverse_64(len)))));
		}

		SEQ_STR_INLINE_STRONG Char* resize_grow(size_t len, bool exact_size = false)
		{
			// Resize for growing, keep old data
			Char* p;

			if (is_sso()) {
				if (len > sso_max_size) {
					internal_resize(len, true, exact_size);
					p = d_data.d_union.non_sso.data + char_offset;
				}
				else {
					d_data.setSize(len);
					p = d_data.d_union.sso.data;
				}
			}
			else {
				if (len > capacity())
					internal_resize(len, true, exact_size);
				else
					d_data.setSizeNonSSO(len);

				p = d_data.d_union.non_sso.data + char_offset;
			}

			p[len] = Char{ 0 };
			return p;
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

				// from non sso to sso
				Char* ptr = d_data.d_union.non_sso.data;
				size_t cap = capacity_for_length(old_size);
				if (keep_old)
					memcpy(d_data.d_union.sso.data, ptr + char_offset, len * sizeof(Char));
				d_data.deallocate(ptr, cap + char_offset);
			}
			// non sso new len, might throw
			else {
				// from sso to non sso

				size_t capacity = exact_size ? len + 1 : capacity_for_length(len);
				Char* ptr = d_data.allocate(capacity + char_offset);
				// write capacity
				write_size_t(ptr, capacity);
				// write previous content
				if (keep_old)
					memcpy(ptr + char_offset, data(), size() * sizeof(Char));
				// deallocate previous if necessary
				if (!sso)
					d_data.deallocate(d_data.d_union.non_sso.data, capacity_internal() + char_offset);
				d_data.d_union.non_sso.data = ptr;
				// null terminate
				d_data.d_union.non_sso.data[len + char_offset] = 0;
			}
			d_data.setSize(len);
		}

		void erase_internal(size_t first, size_t last)
		{
			SEQ_ASSERT_DEBUG(first <= last, "erase: invalid positions");
			SEQ_ASSERT_DEBUG(last <= size(), "erase: invalid last position");
			if (first == last)
				return;
			size_t space_before = first;
			size_t space_after = size() - last;
			size_t s = size();
			Char* p = data();
			if (space_before < space_after) {
				std::copy_backward(p, p + first, p + last);
				resize_front(s - (last - first));
			}
			else {
				internal_copy(p + last, p + s, p + first);
				resize(s - (last - first));
			}
		}

		template<class Iter>
		SEQ_ALWAYS_INLINE bool alias(Iter first, Iter last) const
		{
			// Check for aliasing
			if constexpr (!is_random_access_v<Iter>)
				return false;
			else if constexpr (!std::is_same_v<typename std::iterator_traits<Iter>::value_type, Char>)
				return false;
			else if constexpr (std::is_pointer_v<Iter> && std::is_same_v<std::remove_cv_t<std::remove_pointer_t<Iter>>, Char>)
				return (std::uintptr_t)first >= (std::uintptr_t)begin() && (std::uintptr_t)first <= (std::uintptr_t)end();
			else {
				if (first == last)
					return false;
				// Assumption: a source iterator range either refers entirely to one
				// independent container or begins within this string's storage.
				using reference = decltype(*first);
				if constexpr (!std::is_lvalue_reference_v<reference>)
					return true; // conservatively assume aliasing
				else {
					const auto p = reinterpret_cast<std::uintptr_t>(std::addressof(*first));
					return p >= (std::uintptr_t)begin() && p <= (std::uintptr_t)end();
				}
			}
		}

		template<class It>
		static constexpr bool nothrow_iter() noexcept
		{
			// Check if iterator type does not throw for a call to internal_copy()
			return std::is_nothrow_copy_constructible_v<It> && noexcept(std::declval<Char&>() = *std::declval<It&>()) && noexcept(std::declval<It&>() != std::declval<It&>()) &&
			       noexcept(*std::declval<It&>()) && noexcept(++std::declval<It&>()) && noexcept(std::declval<It&>()++);
		}

		template<class Iter>
		tiny_string& replace_internal(size_t pos, size_t len, Iter first, Iter last)
		{
			// Check this bounds
			if (pos > size())
				throw std::out_of_range("tiny_string::replace position out of range");

			size_t available = size() - pos;
			if (len > available)
				len = available;

			if (alias(first, last))
				goto forward;

			if constexpr (is_random_access_v<Iter> && nothrow_iter<Iter>()) {

				const auto insize = std::distance(first, last);
				if (insize < 0)
					throw std::length_error("tiny_string::replace: invalid iterator range");

				const size_t input_size = static_cast<size_t>(insize);
				const size_t retained = size() - len;

				if (input_size > max_size() - retained)
					throw std::length_error("tiny_string too long");

				const size_t new_size = retained + input_size;

				if (new_size <= capacity()) {
					// do everything inplace
					if (input_size != len)
						memmove(data() + pos + input_size, data() + pos + len, (size() - (pos + len)) * sizeof(Char));

					// copy input
					internal_copy(first, last, data() + pos);

					data()[new_size] = 0;
					d_data.setSizeKeepSSOFlag(new_size);
					SEQ_ASSERT_DEBUG(check_invariant(), "");
					return *this;
				}

				// otherwise, create a new string and fill it (might throw)
				tiny_string other(new_size, 0, get_allocator());
				Char* op = other.data();
				// first part
				memcpy(op, data(), pos * sizeof(Char));
				// middle part
				internal_copy(first, last, op + pos);
				// last part
				memcpy(op + pos + input_size, data() + pos + len, (size() - (pos + len)) * sizeof(Char));

				std::swap(d_data.d_union, other.d_data.d_union);
				SEQ_ASSERT_DEBUG(check_invariant(), "");
				return *this;
			}

		forward: {
			// otherwise, create a new string and fill it
			tiny_string other(get_allocator());
			// first part
			other.append(data(), pos);
			// middle part
			other.append(first, last);
			// last part
			other.append(data() + pos + len, size() - (pos + len));
			std::swap(d_data.d_union, other.d_data.d_union);
		}
			SEQ_ASSERT_DEBUG(check_invariant(), "");
			return *this;
		}

		auto initialize(size_t size) -> Char*
		{
			if (size > max_size())
				throw std::length_error("tiny_string::initialize: invalid size");

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

		template<class String>
		std::basic_string_view<Char, Traits> compute_substring(const String& str, size_t pos, size_t len) const
		{
			if (pos > str.size())
				throw std::out_of_range("tiny_string substring position");

			const size_t available = str.size() - pos;
			if (len > available)
				len = available;
			return { str.data() + pos, len };
		}

		template<class It>
		SEQ_STR_INLINE_STRONG void internal_copy(It first, It last, Char* dst)
		{
			// Similar to std::copy, but make sure only a known subset of iterator interface is used
			// to deduce noexcept specifier.
			// Also tries to use memmove when possible.

			if constexpr (std::is_same_v<It, Char*> || std::is_same_v<It, const Char*> || std::is_same_v<It, typename std::basic_string<Char>::iterator> ||
				      std::is_same_v<It, typename std::basic_string<Char>::const_iterator> || std::is_same_v<It, typename std::basic_string_view<Char>::iterator> ||
				      std::is_same_v<It, typename std::basic_string_view<Char>::const_iterator>) {
				const auto n = static_cast<size_t>(last - first);
				if (n != 0)
					memmove(dst, std::addressof(*first), n * sizeof(Char));
			}
			else {
				for (; first != last; ++first, ++dst)
					*dst = *first;
			}
		}

		bool check_invariant() noexcept { return data()[size()] == 0; }
		auto to_string_view() const noexcept { return std::basic_string_view<Char, Traits>(data(), size()); }

		using al_traits = std::allocator_traits<Allocator>;
		using word_allocator = typename al_traits::template rebind_alloc<std::size_t>;
		using word_traits = std::allocator_traits<word_allocator>;

		static_assert(std::is_same_v<typename al_traits::size_type, std::size_t>);
		static_assert(std::is_same_v<typename word_traits::size_type, std::size_t>);

	public:
		static_assert(std::is_same_v<typename al_traits::pointer, Char*>, "tiny_string does not support fancy pointers");

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
		using size_type = std::size_t; // We require size_t as size_type instead of allocator size_type
		using difference_type = std::ptrdiff_t;
		using allocator_type = Allocator;

		// Maximum string length to use SSO
		static constexpr size_type max_allowed_static_size = max_allowed_sso_capacity - 1;
		static constexpr size_type max_static_size = sso_max_capacity - 1;
		static constexpr size_type npos = static_cast<size_type>(-1);

		/// @brief Default constructor
		tiny_string() noexcept(std::is_nothrow_default_constructible_v<internal_data>) { SEQ_ASSERT_DEBUG(check_invariant(), ""); }
		/// @brief Construct from allocator object
		explicit tiny_string(const Allocator& al) noexcept(std::is_nothrow_copy_constructible_v<Allocator>)
		  : d_data(al)
		{
			SEQ_ASSERT_DEBUG(check_invariant(), "");
		}
		/// @brief Construct from a null-terminated string and an allocator object
		tiny_string(const Char* data, const Allocator& al = Allocator())
		  : d_data(al)
		{
			if (data) {
				const size_type len = Traits::length(data);
				memcpy(initialize(len), data, len * sizeof(Char));
			}
			SEQ_ASSERT_DEBUG(check_invariant(), "");
		}
		/// @brief Construct from a string, its size and an allocator object
		tiny_string(const Char* data, size_type n, const Allocator& al = Allocator())
		  : d_data(al)
		{
			if (n != 0) {
				if (!data)
					throw std::invalid_argument("tiny_string: null source with nonzero length");
				memcpy(initialize(n), data, n * sizeof(Char));
			}
			SEQ_ASSERT_DEBUG(check_invariant(), "");
		}
		/// @brief Construct by filling with n copy of character c and an allocator object
		tiny_string(size_type n, Char c, const Allocator& al = Allocator())
		  : d_data(al)
		{
			std::fill_n(initialize(n), n, c);
			SEQ_ASSERT_DEBUG(check_invariant(), "");
		}
		/// @brief Construct from a std::string and an allocator object
		template<class String, std::enable_if_t<is_string_class_for_v<String, Char>, int> = 0>
		tiny_string(const String& str, const Allocator& al = Allocator())
		  : d_data(al)
		{
			if (str.size() > 0)
				memcpy(initialize(str.size()), str.data(), str.size() * sizeof(Char));
			SEQ_ASSERT_DEBUG(check_invariant(), "");
		}

		/// @brief Construct by copying a sub-part of other
		template<class String, std::enable_if_t<is_string_class_for_v<String, Char>, int> = 0>
		tiny_string(const String& other, size_type pos, size_type len, const Allocator& al = Allocator())
		  : tiny_string(compute_substring(other, pos, len), al)
		{
			SEQ_ASSERT_DEBUG(check_invariant(), "");
		}
		template<class String, std::enable_if_t<is_string_class_for_v<String, Char>, int> = 0>
		tiny_string(const String& other, size_type pos, const Allocator& al = Allocator())
		  : tiny_string(compute_substring(other, pos, npos), al)
		{
			SEQ_ASSERT_DEBUG(check_invariant(), "");
		}

		/// @brief Copy constructor
		tiny_string(const tiny_string& other)
		  : tiny_string(other, copy_allocator(other.d_data.get_allocator()))
		{
			SEQ_ASSERT_DEBUG(check_invariant(), "");
		}
		/// @brief Copy constructor with custom allocator
		tiny_string(const tiny_string& other, const Allocator& al)
		  : d_data(al)
		{
			if (other.is_sso())
				// for SSO and read only string
				memcpy(&d_data.d_union, &other.d_data.d_union, sizeof(d_data.d_union));
			else {
				size_type size = other.size();
				memcpy(initialize(size), other.d_data.d_union.non_sso.data + char_offset, sizeof(Char) * (size));
			}
			SEQ_ASSERT_DEBUG(check_invariant(), "");
		}

		/// @brief Move constructor
		SEQ_STR_INLINE_STRONG tiny_string(tiny_string&& other) noexcept(std::is_nothrow_move_constructible_v<Allocator>)
		  : d_data(std::move(other.d_data.get_allocator()), other.d_data)
		{
			other.d_data.reset();
			SEQ_ASSERT_DEBUG(check_invariant(), "");
			SEQ_ASSERT_DEBUG(other.check_invariant(), "");
		}
		/// @brief Move constructor with custom allocator
		tiny_string(tiny_string&& other, const Allocator& al) noexcept(std::allocator_traits<Allocator>::is_always_equal::value && std::is_nothrow_copy_constructible_v<Allocator>)
		  : d_data(al)
		{
			using traits = std::allocator_traits<Allocator>;

			if constexpr (traits::is_always_equal::value) {
				memcpy(&d_data.d_union, &other.d_data.d_union, sizeof(d_data.d_union));
				other.d_data.reset();
			}
			else if (al == other.get_allocator()) {
				memcpy(&d_data.d_union, &other.d_data.d_union, sizeof(d_data.d_union));
				other.d_data.reset();
			}
			else {
				const size_type n = other.size();
				memcpy(initialize(n), other.data(), n * sizeof(Char));
			}
			SEQ_ASSERT_DEBUG(check_invariant(), "");
			SEQ_ASSERT_DEBUG(other.check_invariant(), "");
		}
		/// @brief Construct by copying the range [first,last)
		template<class Iter, std::enable_if_t<is_iterator_v<Iter>, int> = 0>
		tiny_string(Iter first, Iter last, const Allocator& al = Allocator())
		  : d_data(al)
		{
			assign(first, last);
		}
		/// @brief Construct from initializer list
		tiny_string(std::initializer_list<Char> il, const Allocator& al = Allocator())
		  : d_data(al)
		{
			assign(il);
		}

		~tiny_string() noexcept
		{
			if (!is_sso())
				d_data.deallocate(d_data.d_union.non_sso.data, capacity_internal() + char_offset);
		}

		/// @brief Returns the internal character storage
		SEQ_STR_INLINE_STRONG auto data() noexcept -> Char* { return d_data.data(); }
		/// @brief Returns the internal character storage
		SEQ_STR_INLINE_STRONG auto data() const noexcept -> const Char* { return d_data.data(); }
		/// @brief Returns the internal character storage
		SEQ_STR_INLINE_STRONG auto c_str() const noexcept -> const Char* { return data(); }
		/// @brief Returns the string size (without the trailing null character)
		SEQ_STR_INLINE_STRONG auto size() const noexcept -> size_type { return static_cast<size_type>(d_data.size()); }
		/// @brief Returns the string size (without the trailing null character)
		SEQ_STR_INLINE_STRONG auto length() const noexcept -> size_type { return size(); }
		/// @brief Returns the string maximum size
		SEQ_STR_INLINE_STRONG auto max_size() const -> size_type { return d_data.max_size(); }
		/// @brief Returns the string current capacity
		SEQ_STR_INLINE_STRONG auto capacity() const noexcept -> size_type { return static_cast<size_type>(capacity_internal() - 1ULL); }
		/// @brief Returns true if the string is empty, false otherwise
		SEQ_STR_INLINE_STRONG auto empty() const noexcept -> bool { return size() == 0; }
		/// @brief Returns the string allocator
		SEQ_STR_INLINE_STRONG auto get_allocator() const -> allocator_type { return d_data.get_allocator(); }

		SEQ_STR_INLINE_STRONG std::pair<const Char*, size_t> as_pair() const noexcept { return d_data.as_pair(); }

		/// @brief Assign the range [first,last) to this string
		template<class Iter, std::enable_if_t<is_iterator_v<Iter>, int> = 0>
		auto assign(Iter first, Iter last) -> tiny_string&
		{
			if constexpr (!nothrow_iter<Iter>()) {
				tiny_string tmp(get_allocator());
				for (; first != last; ++first)
					tmp.push_back(*first);
				std::swap(d_data.d_union, tmp.d_data.d_union);
			}
			else {

				if (alias(first, last)) {
					// Aliasing: insert in new string and move
					tiny_string tmp(first, last, get_allocator());
					std::swap(d_data.d_union, tmp.d_data.d_union);
				}
				else {
					if constexpr (!is_random_access_v<Iter>) {
						// Assign range for non random access iterators, keep allocated memory if any
						tiny_string tmp(get_allocator());
						for (; first != last; ++first)
							tmp.push_back(*first);
						*this = std::move(tmp);
					}
					else {
						const auto distance = std::distance(first, last);
						if (distance < 0)
							throw std::length_error("tiny_string::assign: invalid iterator range");

						// Assign range for random access iterators
						internal_resize(static_cast<size_type>(distance), false, true);
						internal_copy(first, last, data());
					}
				}
			}
			SEQ_ASSERT_DEBUG(check_invariant(), "");
			return *this;
		}
		/// @brief Assign the content of other to this string
		template<class String, std::enable_if_t<is_string_class_for_v<String, Char>, int> = 0>
		auto assign(const String& other) -> tiny_string&
		{
			const auto* d = other.data();
			return assign(d, d + other.size());
		}

		/// @brief Assign a sub-part of other to this string
		template<class String, std::enable_if_t<is_string_class_for_v<String, Char>, int> = 0>
		auto assign(const String& str, size_type subpos, size_type sublen) -> tiny_string&
		{
			return assign(compute_substring(str, subpos, sublen));
		}
		/// @brief Assign a null-terminated buffer to this string
		auto assign(const Char* s) -> tiny_string&
		{
			if (!s)
				clear();
			else
				assign(s, Traits::length(s));
			return *this;
		}
		/// @brief Assign a buffer to this string
		auto assign(const Char* s, size_type n) -> tiny_string&
		{
			if (s && n)
				assign(s, s + n);
			else
				clear();
			return *this;
		}
		/// @brief Reset the string by n copies of c
		auto assign(size_type n, Char c) -> tiny_string&
		{
			internal_resize(n, false, true);
			std::fill_n(this->data(), n, c);
			SEQ_ASSERT_DEBUG(check_invariant(), "");
			return *this;
		}
		/// @brief Assign to this string the initializer list il
		auto assign(std::initializer_list<Char> il) -> tiny_string& { return assign(il.begin(), il.end()); }

		/// @brief Resize the string.
		/// For wide string, the allocated memory will be of exactly n+1 bytes.
		void resize(size_type n) { resize(n, 0); }
		/// @brief Resize the string.
		/// For wide string, the allocated memory will be of exactly n+1 bytes.
		/// If n is bigger than size(), pad with (n-size()) copies of c.
		void resize(size_type n, Char c)
		{
			size_type old_size = size();
			if (old_size == n)
				return;
			internal_resize(n, true);
			if (n > old_size)
				std::fill_n(data() + old_size, n - old_size, c);
			SEQ_ASSERT_DEBUG(check_invariant(), "");
		}

		/// @brief Resize the string from the front.
		/// For wide string, the allocated memory will be of exactly n+1 bytes.
		void resize_front(size_type n) { resize_front(n, 0); }
		/// @brief Resize the string from the front.
		/// For wide string, the allocated memory will be of exactly n+1 bytes.
		/// If n is bigger than size(), pad at the front with (n-size()) copies of c.
		void resize_front(size_type n, Char c)
		{
			size_type old_size = size();
			if (old_size == n)
				return;

			if (n <= capacity()) {
				// no alloc/dealloc required
				Char* p = data();
				if (n > old_size) {
					memmove(p + n - old_size, p, old_size * sizeof(Char));
					std::fill_n(p, n - old_size, c);
				}
				else {
					memmove(p, p + old_size - n, n * sizeof(Char));
				}
				d_data.setSizeKeepSSOFlag(n);
				data()[n] = 0;
				SEQ_ASSERT_DEBUG(check_invariant(), "");
				return;
			}

			tiny_string other(n, 0, get_allocator());
			if (n > old_size) {
				std::fill_n(other.data(), n - old_size, c);
				memcpy(other.data() + n - old_size, data(), old_size * sizeof(Char));
			}
			else {
				memcpy(other.data(), data() + old_size - n, n * sizeof(Char));
			}
			swap(other);
			SEQ_ASSERT_DEBUG(check_invariant(), "");
		}

		/// @brief Clear the string without deallocating its memory
		void clear() noexcept
		{
			d_data.setSizeKeepSSOFlag(0);
			data()[0] = Char{ 0 };
		}

		void shrink_to_fit()
		{
			size_type s = size();
			size_type cp = capacity_internal();
			if (!is_sso() && cp != s + 1) {
				Char* p = d_data.d_union.non_sso.data;
				if (is_sso(s)) {
					memcpy(d_data.d_union.sso.data, p + char_offset, (s + 1) * sizeof(Char));
				}
				else {
					Char* _new = d_data.allocate(s + char_offset + 1);
					write_size_t(_new, s + 1);
					memcpy(_new + char_offset, p + char_offset, (s + 1) * sizeof(Char));
					d_data.d_union.non_sso.data = _new;
				}
				d_data.deallocate(p, cp + char_offset);
				d_data.setSize(s);
			}
			SEQ_ASSERT_DEBUG(check_invariant(), "");
		}

		void reserve(size_type new_capacity)
		{
			if (new_capacity > capacity()) {
				size_type s = size();
				Char* p = resize_grow(new_capacity, true);
				d_data.setSizeKeepSSOFlag(s);
				p[s] = 0;
				SEQ_ASSERT_DEBUG(check_invariant(), "");
			}
		}

		/// @brief Returns an iterator to the first element of the container.
		auto begin() noexcept -> iterator { return data(); }
		/// @brief Returns an iterator to the first element of the container.
		auto begin() const noexcept -> const_iterator { return data(); }
		/// @brief Returns an iterator to the first element of the container.
		auto cbegin() const noexcept -> const_iterator { return data(); }

		/// @brief Returns an iterator to the element following the last element of the container.
		auto end() noexcept -> iterator { return data() + size(); }
		/// @brief Returns an iterator to the element following the last element of the container.
		auto end() const noexcept -> const_iterator { return data() + size(); }
		/// @brief Returns an iterator to the element following the last element of the container.
		auto cend() const noexcept -> const_iterator { return end(); }

		/// @brief Returns a reverse iterator to the first element of the reversed list.
		auto rbegin() noexcept -> reverse_iterator { return reverse_iterator(end()); }
		/// @brief Returns a reverse iterator to the first element of the reversed list.
		auto rbegin() const noexcept -> const_reverse_iterator { return const_reverse_iterator(end()); }
		/// @brief Returns a reverse iterator to the first element of the reversed list.
		auto crbegin() const noexcept -> const_reverse_iterator { return rbegin(); }

		/// @brief Returns a reverse iterator to the element following the last element of the reversed list.
		auto rend() noexcept -> reverse_iterator { return reverse_iterator(begin()); }
		/// @brief Returns a reverse iterator to the element following the last element of the reversed list.
		auto rend() const noexcept -> const_reverse_iterator { return const_reverse_iterator(begin()); }
		/// @brief Returns a reverse iterator to the element following the last element of the reversed list.
		auto crend() const noexcept -> const_reverse_iterator { return rend(); }

		/// @brief Returns the character at pos, throw std::out_of_range for invalid position.
		SEQ_STR_INLINE_STRONG const_reference at(size_type pos) const
		{
			if (pos >= size())
				throw std::out_of_range("");
			return data()[pos];
		}
		/// @brief Returns the character at pos, throw std::out_of_range for invalid position.
		SEQ_STR_INLINE_STRONG reference at(size_type pos)
		{
			if (pos >= size())
				throw std::out_of_range("");
			return data()[pos];
		}

		/// @brief Returns the character at pos
		SEQ_STR_INLINE_STRONG const_reference operator[](size_type pos) const noexcept
		{
			SEQ_ASSERT_DEBUG(pos <= size(), "invalid position");
			return data()[pos];
		}
		/// @brief Returns the character at pos
		SEQ_STR_INLINE_STRONG reference operator[](size_type pos) noexcept
		{
			SEQ_ASSERT_DEBUG(pos <= size(), "invalid position");
			return data()[pos];
		}

		/// @brief Returns the last character of the string
		SEQ_STR_INLINE_STRONG const_reference back() const noexcept
		{
			SEQ_ASSERT_DEBUG(size() > 0, "empty container");
			return data()[size() - 1];
		}
		/// @brief Returns the last character of the string
		SEQ_STR_INLINE_STRONG reference back() noexcept
		{
			SEQ_ASSERT_DEBUG(size() > 0, "empty container");
			return data()[size() - 1];
		}
		/// @brief Returns the first character of the string
		SEQ_STR_INLINE_STRONG const_reference front() const noexcept
		{
			SEQ_ASSERT_DEBUG(size() > 0, "empty container");
			return data()[0];
		}
		/// @brief Returns the first character of the string
		SEQ_STR_INLINE_STRONG reference front() noexcept
		{
			SEQ_ASSERT_DEBUG(size() > 0, "empty container");
			return data()[0];
		}

		/// @brief Append character to the back of the string
		SEQ_STR_INLINE_STRONG void push_back(Char c)
		{
			size_type s = size();
			Char* p = resize_grow(s + 1);
			p[s] = c;
			// p[s + 1] = 0; // This is done in resize_grow
			SEQ_ASSERT_DEBUG(check_invariant(), "");
		}
		/// @brief Removes the last character of the string
		SEQ_STR_INLINE_STRONG void pop_back()
		{
			SEQ_ASSERT_DEBUG(size() > 0, "pop_back on an empty string!");

			if SEQ_LIKELY (!is_sso()) {
				size_type s = d_data.sizeNonSSO() - 1;
				d_data.setSizeNonSSO(s);
				d_data.d_union.non_sso.data[s + char_offset] = 0;
			}
			else {
				d_data.setSize(d_data.sizeSSO() - 1);
				d_data.d_union.sso.data[d_data.sizeSSO()] = 0;
			}
			SEQ_ASSERT_DEBUG(check_invariant(), "");
		}

		/// @brief Append the content of str to this string
		template<class String, std::enable_if_t<is_string_class_for_v<String, Char>, int> = 0>
		auto append(const String& str) -> tiny_string&
		{
			const auto* d = str.data();
			return append(d, d + str.size());
		}

		/// @brief Append the sub-part of str to this string
		template<class String, std::enable_if_t<is_string_class_for_v<String, Char>, int> = 0>
		auto append(const String& str, size_type subpos, size_type sublen = npos) -> tiny_string&
		{
			return append(compute_substring(str, subpos, sublen));
		}

		/// @brief Append a null-terminated buffer to this string
		auto append(const Char* s) -> tiny_string&
		{
			if (s)
				append(s, Traits::length(s));
			return *this;
		}

		/// @brief Append buffer content to this string
		auto append(const Char* s, size_type n) -> tiny_string&
		{
			if (n)
				return append(s, s + n);
			return *this;
		}

		/// @brief Append a character
		auto append(Char c) -> tiny_string&
		{
			push_back(c);
			return *this;
		}

		/// @brief Append n copies of character c to the string
		auto append(size_type n, Char c) -> tiny_string&
		{
			if SEQ_UNLIKELY (n == 0)
				return *this;

			const size_type old_size = size();
			if (n > max_size() - old_size)
				throw std::length_error("tiny_string::append too long");

			const size_type new_size = old_size + n;
			Char* d = resize_grow(new_size);
			std::fill_n(d + old_size, n, c);
			// d[new_size] = 0; // This is done in resize_grow
			SEQ_ASSERT_DEBUG(check_invariant(), "");
			return *this;
		}
		/// @brief Append the content of the range [il.begin(),il.end()) to this string
		auto append(std::initializer_list<Char> il) -> tiny_string& { return append(il.begin(), il.end()); }

		/// @brief Append the content of the range [first,last) to this string
		template<class Iter, std::enable_if_t<is_iterator_v<Iter>, int> = 0>
		auto append(Iter first, Iter last) -> tiny_string&
		{
			if (first == last)
				return *this;

			if constexpr (!nothrow_iter<Iter>() || !is_random_access_v<Iter>) {
				tiny_string tmp(*this, get_allocator());
				while (first != last) {
					tmp.push_back(*first);
					++first;
				}
				std::swap(d_data.d_union, tmp.d_data.d_union);
			}
			else {

				if (alias(first, last)) {
					tiny_string tmp(first, last, get_allocator());
					return append(tmp);
				}

				const auto distance = std::distance(first, last);
				if (distance < 0)
					throw std::length_error("tiny_string::append: invalid iterator range");

				const size_type n = static_cast<size_type>(distance);
				const size_type old_size = size();
				if (n > max_size() - old_size)
					throw std::length_error("tiny_string::append too long");

				const size_type new_size = old_size + n;
				Char* d = resize_grow(new_size);
				internal_copy(first, last, d + old_size);
			}
			SEQ_ASSERT_DEBUG(check_invariant(), "");
			return *this;
		}

		/// @brief Inserts the content of str at position pos
		template<class String, std::enable_if_t<is_string_class_for_v<String, Char>, int> = 0>
		auto insert(size_type pos, const String& str) -> tiny_string&
		{
			if (pos > size())
				throw std::out_of_range("tiny_string::insert position");
			insert(begin() + pos, str.begin(), str.end());
			return *this;
		}

		/// @brief Inserts a sub-part of str at position pos
		template<class String, std::enable_if_t<is_string_class_for_v<String, Char>, int> = 0>
		auto insert(size_type pos, const String& str, size_type subpos, size_type sublen = npos) -> tiny_string&
		{
			if (pos > size())
				throw std::out_of_range("tiny_string::insert position");
			insert(begin() + pos, compute_substring(str, subpos, sublen));
			return *this;
		}
		/// @brief Inserts a null-terminated buffer at position pos
		auto insert(size_type pos, const Char* s) -> tiny_string&
		{
			if (pos > size())
				throw std::out_of_range("tiny_string::insert position");
			if (s)
				insert(begin() + pos, s, s + Traits::length(s));
			return *this;
		}
		/// @brief Inserts buffer at position pos
		auto insert(size_type pos, const Char* s, size_type n) -> tiny_string&
		{
			if (pos > size())
				throw std::out_of_range("tiny_string::insert position");
			if (n != 0) {
				if (!s)
					throw std::invalid_argument("tiny_string::insert: null pointer");

				insert(begin() + pos, s, s + n);
			}
			return *this;
		}
		/// @brief Inserts n copies of character c at position pos
		auto insert(size_type pos, size_type n, Char c) -> tiny_string&
		{
			if (pos > size())
				throw std::out_of_range("tiny_string::insert position");
			insert(begin() + pos, cvalue_iterator<Char>(0, c), cvalue_iterator<Char>(n));
			return *this;
		}

		template<class String, std::enable_if_t<is_string_class_for_v<String, Char>, int> = 0>
		auto insert(const_iterator p, const String& str) -> iterator
		{
			return insert(p, str.data(), str.data() + str.size());
		}
		template<class String, std::enable_if_t<is_string_class_for_v<String, Char>, int> = 0>
		auto insert(const_iterator p, const String& str, size_type subpos, size_type sublen = npos) -> iterator
		{
			return insert(p, compute_substring(str, subpos, sublen));
		}
		auto insert(const_iterator p, const Char* s) -> iterator
		{
			if (s)
				return insert(p, s, s + Traits::length(s));
			return const_cast<iterator>(p);
		}
		auto insert(const_iterator p, const Char* s, size_type n) -> iterator
		{
			if (n)
				return insert(p, s, s + n);
			return const_cast<iterator>(p);
		}
		/// @brief Inserts n copies of character c at position p
		auto insert(const_iterator p, size_type n, Char c) -> iterator { return insert(p, cvalue_iterator<Char>(0, c), cvalue_iterator<Char>(n)); }
		/// @brief Inserts character c at position p
		auto insert(const_iterator p, Char c) -> iterator { return insert(p, &c, (&c) + 1); }
		/// @brief Inserts the content of the range [il.begin(),il.end()) at position p
		auto insert(const_iterator p, std::initializer_list<Char> il) -> iterator { return insert(p, il.begin(), il.end()); }
		/// @brief Inserts the content of the range [first,last) at position p
		template<class Iter, std::enable_if_t<is_iterator_v<Iter>, int> = 0>
		auto insert(const_iterator loc, Iter first, Iter last) -> iterator
		{
			SEQ_ASSERT_DEBUG(loc >= begin() && loc <= end(), "tiny_string::insert invalid iterator");

			size_type pos = static_cast<size_type>(loc - begin());

			if constexpr (!nothrow_iter<Iter>() || !is_random_access_v<Iter>) {
				tiny_string tmp(cbegin(), loc, get_allocator());
				tmp.append(first, last);
				tmp.append(loc, cend());
				std::swap(d_data.d_union, tmp.d_data.d_union);
			}
			else {

				if (alias(first, last)) {
					tiny_string tmp(first, last, get_allocator());
					return insert(loc, tmp.begin(), tmp.end());
				}

				const auto distance = std::distance(first, last);
				if (distance < 0)
					throw std::length_error("tiny_string::insert: invalid iterator range");

				if (static_cast<size_type>(distance) > max_size() - size())
					throw std::length_error("tiny_string::append too long");

				if (pos < size() / 2) {
					size_type to_insert = static_cast<size_type>(distance);
					// Might throw, fine
					resize_front(size() + to_insert);
					iterator beg = begin();
					// Might throw, fine
					Char* p = data();
					std::for_each(p + to_insert, p + to_insert + pos, [&](Char v) {
						*beg = v;
						++beg;
					});
					std::for_each(p + pos, p + pos + to_insert, [&](Char& v) { v = *first++; });
				}
				else {
					// Might throw, fine
					size_type to_insert = static_cast<size_type>(distance);
					resize(size() + to_insert);
					Char* p = data();
					size_type s = size();
					std::copy_backward(p + pos, p + (s - to_insert), p + s);
					internal_copy(first, last, p + pos);
				}
			}
			SEQ_ASSERT_DEBUG(check_invariant(), "");
			return begin() + pos;
		}

		/// @brief Removes up to sublen character starting from subpos
		auto erase(size_type subpos, size_type sublen = npos) -> tiny_string&
		{
			if (subpos > size())
				throw std::out_of_range("tiny_string::erase position");

			const size_type available = size() - subpos;
			if (sublen > available)
				sublen = available;

			erase_internal(subpos, subpos + sublen);
			SEQ_ASSERT_DEBUG(check_invariant(), "");
			return *this;
		}
		/// @brief Remove character at position p
		auto erase(const_iterator p) -> iterator
		{
			size_type f = static_cast<size_type>(p - begin());
			erase(f, 1);
			return begin() + f;
		}
		/// @brief Removes the range [first,last)
		auto erase(const_iterator first, const_iterator last) -> iterator
		{
			if (first > last)
				throw std::invalid_argument("tiny_string::erase: invalid iterator range");
			size_type f = static_cast<size_type>(first - begin());
			erase(f, static_cast<size_type>(last - first));
			return begin() + f;
		}

		/// @brief Replace up to len character starting from pos by str
		template<class String, std::enable_if_t<is_string_class_for_v<String, Char>, int> = 0>
		auto replace(size_type pos, size_type len, const String& str) -> tiny_string&
		{
			return replace(pos, len, str.begin(), str.end());
		}

		/// @brief Replace characters in the range [i1,i2) by str
		template<class String, std::enable_if_t<is_string_class_for_v<String, Char>, int> = 0>
		auto replace(const_iterator i1, const_iterator i2, const String& str) -> tiny_string&
		{
			return replace(static_cast<size_type>(i1 - cbegin()), static_cast<size_type>(i2 - i1), str.begin(), str.end());
		}

		/// @brief Replace up to len character starting from pos by a sub-part of str
		template<class String, std::enable_if_t<is_string_class_for_v<String, Char>, int> = 0>
		auto replace(size_type pos, size_type len, const String& str, size_type subpos, size_type sublen) -> tiny_string&
		{
			return replace(pos, len, compute_substring(str, subpos, sublen));
		}

		/// @brief Replace up to len character starting from pos by s
		auto replace(size_type pos, size_type len, const Char* s) -> tiny_string&
		{
			if (!s)
				return *this;
			return replace(pos, len, s, s + Traits::length(s));
		}
		/// @brief Replace characters in the range [i1,i2) by null-terminated string s
		auto replace(const_iterator i1, const_iterator i2, const Char* s) -> tiny_string&
		{
			if (!s)
				return *this;
			return replace(i1, i2, s, Traits::length(s));
		}
		/// @brief Replace up to len character starting from pos by buffer s of size n
		auto replace(size_type pos, size_type len, const Char* s, size_type n) -> tiny_string&
		{
			if (n == 0)
				return erase(pos, len);

			if (!s)
				throw std::invalid_argument("tiny_string::replace: null pointer passed");
			return replace(pos, len, s, s + n);
		}
		/// @brief Replace characters in the range [i1,i2) by buffer s of size n
		auto replace(const_iterator i1, const_iterator i2, const Char* s, size_type n) -> tiny_string& { return replace(i1, i2, s, s + n); }

		/// @brief Replace up to len character starting from pos by n copies of c
		auto replace(size_type pos, size_type len, size_type n, Char c) -> tiny_string& { return replace(pos, len, cvalue_iterator<Char>(0, c), cvalue_iterator<Char>(n)); }
		/// @brief Replace characters in the range [i1,i2) by n copies of c
		auto replace(const_iterator i1, const_iterator i2, size_type n, Char c) -> tiny_string& { return replace(i1, i2, cvalue_iterator<Char>(0, c), cvalue_iterator<Char>(n)); }
		/// @brief Replace characters in the range [i1,i2) by the range [first,last)
		template<class It, std::enable_if_t<is_iterator_v<It>, int> = 0>
		auto replace(const_iterator i1, const_iterator i2, It first, It last) -> tiny_string&
		{
			if (i1 > i2)
				throw std::invalid_argument("tiny_string::replace: invalid iterator range");
			return replace(static_cast<size_type>(i1 - cbegin()), static_cast<size_type>(i2 - i1), first, last);
		}
		/// @brief Replace characters in the range [i1,i2) by the range [il.begin(),il.end())
		auto replace(const_iterator i1, const_iterator i2, std::initializer_list<Char> il) -> tiny_string& { return replace(i1, i2, il.begin(), il.end()); }

		template<class It, std::enable_if_t<is_iterator_v<It>, int> = 0>
		auto replace(size_type pos, size_type len, It first, It last) -> tiny_string&
		{
			return replace_internal(pos, len, first, last);
		}

		/// @brief Replace any sub-string _from of size n1 by the string _to of size n2, starting to position start
		auto replace(const Char* _from, size_type n1, const Char* _to, size_type n2, size_type start = 0) -> size_type
		{
			if (n1 == 0)
				return 0;

			if (alias(_from, _from + n1) || alias(_to, _to + n2)) {
				tiny_string sfrom(_from, n1, get_allocator());
				tiny_string sto(_to, n2, get_allocator());
				return replace(sfrom.data(), sfrom.size(), sto.data(), sto.size(), start);
			}

			size_type res = 0;
			size_type start_pos = start;
			while ((start_pos = find(_from, start_pos, n1)) != npos) {
				replace(start_pos, n1, _to, n2);
				start_pos += n2; // Handles case where 'to' is a substring of 'from'
				++res;
			}
			return res;
		}
		/// @brief Replace any sub-string _from by the string _to, starting to position start
		auto replace(const Char* _from, const Char* _to, size_type start = 0) -> size_type { return replace(_from, Traits::length(_from), _to, Traits::length(_to), start); }
		/// @brief Replace any sub-string _from by the string _to, starting to position start
		template<class String1, class String2, std::enable_if_t<(is_string_class_for_v<String1, Char> && is_string_class_for_v<String2, Char>), int> = 0>
		auto replace(const String1& _from, const String2& _to, size_type start = 0) -> size_type
		{
			return replace(_from.data(), _from.size(), _to.data(), _to.size(), start);
		}
		/// @brief Replace any sub-string _from by the string _to, starting to position start
		auto replace(const std::basic_string_view<Char, Traits>& _from, const std::basic_string_view<Char, Traits>& _to, size_type start = 0) -> size_type
		{
			return replace(_from.data(), _from.size(), _to.data(), _to.size(), start);
		}

		/// @brief Returns the count of non-overlapping occurrences of 'str' starting from position start
		auto count(const Char* str, size_type n, size_type start = 0) const noexcept -> size_type
		{
			if (length() == 0 || n == 0)
				return 0;
			size_type count = 0;
			for (size_type offset = find(str, start, n); offset != npos; offset = find(str, offset + n, n))
				++count;
			return count;
		}
		/// @brief Returns the count of non-overlapping occurrences of 'str' starting from position start
		auto count(const Char* str, size_type start = 0) const noexcept -> size_type
		{
			if (str)
				return count(str, Traits::length(str), start);
			return 0;
		}
		/// @brief Returns the count of non-overlapping occurrences of 'str' starting from position start
		template<class String, std::enable_if_t<is_string_class_for_v<String, Char>, int> = 0>
		auto count(const String& str, size_type start = 0) const -> size_type
		{
			return count(str.data(), str.size(), start);
		}
		/// @brief Returns the count of non-overlapping occurrences of 'str' starting from position start
		auto count(Char c, size_type start = 0) const noexcept -> size_type
		{
			if (length() == 0)
				return 0;
			size_type count = 0;
			for (size_type offset = find(c, start); offset != npos; offset = find(c, offset + 1))
				++count;
			return count;
		}

		/// @brief Copies a substring [pos, pos+len) to character string pointed to by s.
		/// Returns the number of copied characters.
		auto copy(Char* s, size_type len, size_type pos = 0) const -> size_type
		{
			if (pos > size())
				throw std::out_of_range("tiny_string::copy out of range");
			if (len == npos || len > (size() - pos))
				len = size() - pos;

			if (len != 0) {
				if (!s)
					throw std::invalid_argument("tiny_string::copy: null destination");
				memcpy(s, data() + pos, len * sizeof(Char));
			}
			return len;
		}

		// @brief Returns a sub-part of this string
		auto substr(size_type pos, size_type len = npos) const { return tiny_string(compute_substring(*this, pos, len)); }

		// @brief Returns a sub-part of this string as a basic_string_view object
		auto substr_view(size_type pos, size_type len = npos) const { return compute_substring(*this, pos, len); }

		template<class String, std::enable_if_t<is_string_class_for_v<String, Char>, int> = 0>
		auto find(const String& str, size_type pos = 0) const -> size_type
		{
			return find(str.data(), pos, str.size());
		}
		auto find(const Char* s, size_type pos = 0) const noexcept -> size_type
		{
			if (!s)
				return npos;
			return find(s, pos, Traits::length(s));
		}
		auto find(const Char* s, size_type pos, size_type n) const noexcept -> size_type { return detail::traits_string_find<Traits>(data(), pos, size(), s, n, npos); }
		auto find(Char c, size_type pos = 0) const noexcept -> size_type
		{
			if (pos >= size())
				return npos;
			const Char* p = Traits::find(data() + pos, size() - pos, c);
			return p == NULL ? npos : p - begin();
		}

		template<class String, std::enable_if_t<is_string_class_for_v<String, Char>, int> = 0>
		auto rfind(const String& str, size_type pos = npos) const -> size_type
		{
			return rfind(str.data(), pos, str.size());
		}
		auto rfind(const Char* s, size_type pos = npos) const noexcept -> size_type
		{
			if (!s)
				return npos;
			return rfind(s, pos, Traits::length(s));
		}
		auto rfind(const Char* s, size_type pos, size_type n) const noexcept -> size_type { return detail::traits_string_rfind<Traits>(data(), pos, size(), s, n, npos); }
		auto rfind(Char c, size_type pos = npos) const noexcept -> size_type { return std::basic_string_view<Char, Traits>(data(), size()).rfind(c, pos); }

		auto find_first_of(const Char* s, size_type pos, size_type n) const noexcept -> size_type
		{
			if (!s)
				return npos;
			return detail::traits_string_find_first_of<Traits>(data(), pos, size(), s, n, npos);
		}
		template<class String, std::enable_if_t<is_string_class_for_v<String, Char>, int> = 0>
		auto find_first_of(const String& str, size_type pos = 0) const -> size_type
		{
			return find_first_of(str.data(), pos, str.size());
		}
		auto find_first_of(const Char* s, size_type pos = 0) const noexcept -> size_type
		{
			if (!s)
				return npos;
			return find_first_of(s, pos, Traits::length(s));
		}
		auto find_first_of(Char c, size_type pos = 0) const noexcept -> size_type { return find(c, pos); }

		template<class String, std::enable_if_t<is_string_class_for_v<String, Char>, int> = 0>
		auto find_last_of(const String& str, size_type pos = npos) const -> size_type
		{
			return find_last_of(str.data(), pos, str.size());
		}
		auto find_last_of(const Char* s, size_type pos = npos) const noexcept -> size_type
		{
			if (!s)
				return npos;
			return find_last_of(s, pos, Traits::length(s));
		}
		auto find_last_of(const Char* s, size_type pos, size_type n) const noexcept -> size_type
		{
			if (!s)
				return npos;
			return detail::traits_string_find_last_of<Traits>(data(), pos, size(), s, n, npos);
		}
		auto find_last_of(Char c, size_type pos = npos) const noexcept -> size_type { return rfind(c, pos); }

		template<class String, std::enable_if_t<is_string_class_for_v<String, Char>, int> = 0>
		auto find_first_not_of(const String& str, size_type pos = 0) const -> size_type
		{
			return find_first_not_of(str.data(), pos, str.size());
		}
		auto find_first_not_of(const Char* s, size_type pos = 0) const noexcept -> size_type { return find_first_not_of(s, pos, s ? Traits::length(s) : 0); }
		auto find_first_not_of(const Char* s, size_type pos, size_type n) const noexcept -> size_type
		{
			if (pos >= size())
				return npos;
			if (!s)
				return n == 0 ? pos : npos;
			return detail::traits_string_find_first_not_of<Traits>(data(), pos, size(), s, n, npos);
		}
		auto find_first_not_of(Char c, size_type pos = 0) const noexcept -> size_type { return to_string_view().find_first_not_of(c, pos); }

		template<class String, std::enable_if_t<is_string_class_for_v<String, Char>, int> = 0>
		auto find_last_not_of(const String& str, size_type pos = npos) const -> size_type
		{
			return find_last_not_of(str.data(), pos, str.size());
		}
		auto find_last_not_of(const Char* s, size_type pos = npos) const noexcept -> size_type { return find_last_not_of(s, pos, s ? Traits::length(s) : 0); }
		auto find_last_not_of(const Char* s, size_type pos, size_type n) const noexcept -> size_type
		{
			if (!s) {
				if (n != 0 || empty())
					return npos;
				return std::min(pos, size() - 1);
			}
			return detail::traits_string_find_last_not_of<Traits>(data(), pos, size(), s, n, npos);
		}
		auto find_last_not_of(Char c, size_type pos = npos) const noexcept -> size_type { return to_string_view().find_last_not_of(c, pos); }

		template<class String, std::enable_if_t<is_string_class_for_v<String, Char>, int> = 0>
		SEQ_STR_INLINE_STRONG auto compare(const String& str) const -> int
		{
			return detail::traits_string_compare<Traits>(data(), size(), str.data(), str.size());
		}

		template<class String, std::enable_if_t<is_string_class_for_v<String, Char>, int> = 0>
		auto compare(size_type pos, size_type len, const String& str) const -> int
		{
			return compare(pos, len, str.data(), str.size());
		}

		template<class String, std::enable_if_t<is_string_class_for_v<String, Char>, int> = 0>
		auto compare(size_type pos, size_type len, const String& str, size_type subpos, size_type sublen) const -> int
		{
			return compare(pos, len, compute_substring(str, subpos, sublen));
		}
		auto compare(const Char* s) const noexcept -> int { return compare(0, size(), s); }
		auto compare(size_type pos, size_type len, const Char* s) const -> int
		{
			if (!s)
				return compare(pos, len, std::basic_string_view<Char, Traits>{});
			return compare(pos, len, s, Traits::length(s));
		}
		auto compare(size_type pos, size_type len, const Char* _s, size_type n) const -> int
		{
			auto v = compute_substring(*this, pos, len);
			return detail::traits_string_compare<Traits>(v.data(), v.size(), _s, n);
		}

		/// @brief Swap the content of this string with other
		SEQ_ALWAYS_INLINE void swap(tiny_string& other) noexcept(std::allocator_traits<Allocator>::propagate_on_container_swap::value ? std::is_nothrow_swappable_v<Allocator> : true)
		{
			using traits = std::allocator_traits<Allocator>;

			if constexpr (!traits::propagate_on_container_swap::value) {
				SEQ_ASSERT_DEBUG(get_allocator() == other.get_allocator(), "swap requires equal non-propagating allocators");
			}
			else {
				using std::swap;
				if (this == std::addressof(other))
					return;
				swap(d_data.get_allocator(), other.d_data.get_allocator());
			}

			d_data.swap(other.d_data);
			SEQ_ASSERT_DEBUG(check_invariant(), "");
			SEQ_ASSERT_DEBUG(other.check_invariant(), "");
		}

		/// @brief Copy assignment.
		///
		/// Provides the strong exception guarantee when allocator propagation does
		/// not require a potentially throwing commit. Otherwise provides the basic
		/// exception guarantee.
		SEQ_ALWAYS_INLINE auto operator=(const tiny_string& other) -> tiny_string&
		{
			if (this != std::addressof(other)) {
				using traits = std::allocator_traits<Allocator>;

				if constexpr (traits::propagate_on_container_copy_assignment::value) {

					tiny_string tmp(other, other.get_allocator());

					// Release storage using the current allocator before changing it.
					if (!is_sso())
						d_data.deallocate(d_data.d_union.non_sso.data, capacity_internal() + char_offset);
					try {
						d_data.get_allocator() = other.get_allocator();
						memcpy(&d_data.d_union, &tmp.d_data.d_union, sizeof(d_data.d_union));
						tmp.d_data.reset();
						SEQ_ASSERT_DEBUG(check_invariant(), "");
						return *this;
					}
					catch (...) {
						d_data.reset();
						throw;
					}
				}
				else {
					if (other.is_sso()) {
						if (!is_sso())
							d_data.deallocate(d_data.d_union.non_sso.data, capacity_internal() + char_offset);
						memcpy(&d_data.d_union, &other.d_data.d_union, sizeof(d_data.d_union));
					}
					else {
						internal_resize(other.size(), false, false);
						internal_copy(other.begin(), other.end(), data());
					}
				}
			}
			SEQ_ASSERT_DEBUG(check_invariant(), "");
			return *this;
		}

		/// @brief Move assignment.
		///
		/// Provides the strong exception guarantee except when the allocator
		/// propagates and its move assignment throws. In that case, the destination
		/// remains valid but its previous value may have been lost.
		SEQ_ALWAYS_INLINE auto operator=(tiny_string&& other) noexcept(std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value
										 ? std::is_nothrow_move_assignable_v<Allocator>
										 : std::allocator_traits<Allocator>::is_always_equal::value) -> tiny_string&
		{

			if (this == std::addressof(other))
				return *this;

			using traits = std::allocator_traits<Allocator>;

			if constexpr (traits::propagate_on_container_move_assignment::value) {

				// Deallocate and reset
				if (!is_sso())
					d_data.deallocate(d_data.d_union.non_sso.data, capacity_internal() + char_offset);

				if constexpr (std::is_nothrow_move_assignable_v<Allocator>) {
					d_data.get_allocator() = std::move(other.d_data.get_allocator()); // No-op for std::allocator
					memcpy(&d_data.d_union, &other.d_data.d_union, sizeof(d_data.d_union));
					other.d_data.reset();
				}
				else {
					// Since moving allocator might throw, we need to reset this before
					d_data.reset();

					// Move allocator, last line that might throw
					d_data.get_allocator() = std::move(other.d_data.get_allocator());

					// Steal data and reset other
					memcpy(&d_data.d_union, &other.d_data.d_union, sizeof(d_data.d_union));
					other.d_data.reset();
				}
			}
			else if constexpr (traits::is_always_equal::value) {
				// Deallocate this (fast way)
				if (!is_sso())
					d_data.deallocate(d_data.d_union.non_sso.data, capacity_internal() + char_offset);
				// Steal data
				memcpy(&d_data.d_union, &other.d_data.d_union, sizeof(d_data.d_union));
				other.d_data.reset();
			}
			else {
				if (get_allocator() == other.get_allocator()) {
					// Deallocate this (fast way)
					if (!is_sso())
						d_data.deallocate(d_data.d_union.non_sso.data, capacity_internal() + char_offset);
					// Steal data
					memcpy(&d_data.d_union, &other.d_data.d_union, sizeof(d_data.d_union));
					other.d_data.reset();
				}
				else
					// Different allocator: assign an leave the source unchanged.
					// Might throw!
					assign(other.data(), other.size());
			}

			SEQ_ASSERT_DEBUG(check_invariant(), "");
			SEQ_ASSERT_DEBUG(other.check_invariant(), "");
			return *this;
		}

		template<class String, std::enable_if_t<is_string_class_for_v<String, Char>, int> = 0>
		SEQ_ALWAYS_INLINE auto operator=(const String& other) -> tiny_string&
		{
			return assign(other);
		}

		SEQ_ALWAYS_INLINE auto operator=(const Char* other) -> tiny_string& { return assign(other); }
		SEQ_ALWAYS_INLINE auto operator=(Char c) -> tiny_string& { return assign(&c, 1); }
		SEQ_ALWAYS_INLINE auto operator=(std::initializer_list<Char> il) -> tiny_string& { return assign(il); }

		template<class String, std::enable_if_t<is_string_class_for_v<String, Char>, int> = 0>
		SEQ_ALWAYS_INLINE auto operator+=(const String& str) -> tiny_string&
		{
			return append(str);
		}
		SEQ_ALWAYS_INLINE auto operator+=(const Char* s) -> tiny_string& { return append(s); }
		SEQ_ALWAYS_INLINE auto operator+=(Char c) -> tiny_string&
		{
			push_back(c);
			return *this;
		}
		SEQ_ALWAYS_INLINE auto operator+=(std::initializer_list<Char> il) -> tiny_string& { return append(il); }

		/// @brief Implicit conversion to std::basic_string
		template<class Al>
		SEQ_ALWAYS_INLINE operator std::basic_string<Char, Traits, Al>() const
		{
			return std::basic_string<Char, Traits, Al>(data(), size());
		}

		/// @brief Implicit conversion to std::basic_string_view
		operator std::basic_string_view<Char, Traits>() const noexcept { return to_string_view(); }
	};

	/// @brief Detect tiny_string
	template<class T>
	struct is_tiny_string : std::false_type
	{
	};
	template<class Char, class Al, size_t S>
	struct is_tiny_string<tiny_string<Char, Al, S>> : std::true_type
	{
	};
	template<class T>
	constexpr bool is_tiny_string_v = is_tiny_string<T>::value;

	/// @brief Returns the string data (const char*) for given string object
	template<class String, std::enable_if_t<is_string_class_v<String>, int> = 0>
	SEQ_STR_INLINE_STRONG auto string_data(const String& str)
	{
		return str.data();
	}
	template<class Char>
	SEQ_STR_INLINE_STRONG auto string_data(const Char* str) -> const Char*
	{
		return str;
	}

	template<class String, std::enable_if_t<is_string_class_v<String>, int> = 0>
	SEQ_STR_INLINE_STRONG auto string_pair(const String& str)
	{
		using Char = typename String::value_type;
		return std::pair<const Char*, size_t>(str.data(), str.size());
	}
	template<class Char>
	SEQ_STR_INLINE_STRONG auto string_pair(const Char* str)
	{
		return std::pair<const Char*, size_t>(str, str ? std::char_traits<Char>::length(str) : 0);
	}
	template<class Char, class Al, size_t S>
	SEQ_STR_INLINE_STRONG auto string_pair(const tiny_string<Char, Al, S>& str)
	{
		return str.as_pair();
	}

	/// @brief Returns the string size for given string object
	template<class String, std::enable_if_t<is_string_class_v<String>, int> = 0>
	SEQ_STR_INLINE_STRONG auto string_size(const String& str) -> size_t
	{
		return (size_t)str.size();
	}
	template<class Char>
	SEQ_STR_INLINE_STRONG auto string_size(const Char* str) -> size_t
	{
		return str ? std::char_traits<Char>::length(str) : 0;
	}

	/**********************************
	 * Comparison operators
	 * ********************************/

	template<class Char, class A, size_t S, class String, std::enable_if_t<is_generic_string_for_v<String, Char>, int> = 0>
	auto operator==(const tiny_string<Char, A, S>& lhs, const String& rhs) -> bool
	{
		using Traits = std::char_traits<Char>;
		return detail::traits_string_equal<Traits>(lhs.data(), lhs.size(), string_data(rhs), string_size(rhs));
	}
	template<class Char, class A, size_t S, class String, std::enable_if_t<is_generic_string_for_v<String, Char> && !is_tiny_string_v<String>, int> = 0>
	auto operator==(const String& lhs, const tiny_string<Char, A, S>& rhs) -> bool
	{
		using Traits = std::char_traits<Char>;
		return detail::traits_string_equal<Traits>(string_data(lhs), string_size(lhs), rhs.data(), rhs.size());
	}

	template<class Char, class A, size_t S, class String, std::enable_if_t<is_generic_string_for_v<String, Char>, int> = 0>
	auto operator!=(const tiny_string<Char, A, S>& lhs, const String& rhs) -> bool
	{
		return !(lhs == rhs);
	}
	template<class Char, class A, size_t S, class String, std::enable_if_t<is_generic_string_for_v<String, Char> && !is_tiny_string_v<String>, int> = 0>
	auto operator!=(const String& lhs, const tiny_string<Char, A, S>& rhs) -> bool
	{
		return !(lhs == rhs);
	}

	template<class Char, class A, size_t S, class String, std::enable_if_t<is_generic_string_for_v<String, Char>, int> = 0>
	SEQ_STR_INLINE_STRONG auto operator<(const tiny_string<Char, A, S>& lhs, const String& rhs) -> bool
	{
		using Traits = std::char_traits<Char>;
		return detail::traits_string_inf<Traits>(string_pair(lhs), string_pair(rhs));
	}
	template<class Char, class A, size_t S, class String, std::enable_if_t<is_generic_string_for_v<String, Char> && !is_tiny_string_v<String>, int> = 0>
	SEQ_STR_INLINE_STRONG auto operator<(const String& lhs, const tiny_string<Char, A, S>& rhs) -> bool
	{
		using Traits = std::char_traits<Char>;
		return detail::traits_string_inf<Traits>(string_pair(lhs), string_pair(rhs));
	}

	template<class Char, class A, size_t S, class String, std::enable_if_t<is_generic_string_for_v<String, Char>, int> = 0>
	auto operator<=(const tiny_string<Char, A, S>& lhs, const String& rhs) -> bool
	{
		using Traits = std::char_traits<Char>;
		return detail::traits_string_inf_equal<Traits>(lhs.data(), lhs.size(), string_data(rhs), string_size(rhs));
	}
	template<class Char, class A, size_t S, class String, std::enable_if_t<is_generic_string_for_v<String, Char> && !is_tiny_string_v<String>, int> = 0>
	auto operator<=(const String& lhs, const tiny_string<Char, A, S>& rhs) -> bool
	{
		using Traits = std::char_traits<Char>;
		return detail::traits_string_inf_equal<Traits>(string_data(lhs), string_size(lhs), rhs.data(), rhs.size());
	}

	template<class Char, class A, size_t S, class String, std::enable_if_t<is_generic_string_for_v<String, Char>, int> = 0>
	auto operator>(const tiny_string<Char, A, S>& lhs, const String& rhs) -> bool
	{
		using Traits = std::char_traits<Char>;
		return detail::traits_string_sup<Traits>(lhs.data(), lhs.size(), string_data(rhs), string_size(rhs));
	}
	template<class Char, class A, size_t S, class String, std::enable_if_t<is_generic_string_for_v<String, Char> && !is_tiny_string_v<String>, int> = 0>
	auto operator>(const String& lhs, const tiny_string<Char, A, S>& rhs) -> bool
	{
		using Traits = std::char_traits<Char>;
		return detail::traits_string_sup<Traits>(string_data(lhs), string_size(lhs), rhs.data(), rhs.size());
	}

	template<class Char, class A, size_t S, class String, std::enable_if_t<is_generic_string_for_v<String, Char>, int> = 0>
	auto operator>=(const tiny_string<Char, A, S>& lhs, const String& rhs) -> bool
	{
		using Traits = std::char_traits<Char>;
		return detail::traits_string_sup_equal<Traits>(lhs.data(), lhs.size(), string_data(rhs), string_size(rhs));
	}
	template<class Char, class A, size_t S, class String, std::enable_if_t<is_generic_string_for_v<String, Char> && !is_tiny_string_v<String>, int> = 0>
	auto operator>=(const String& lhs, const tiny_string<Char, A, S>& rhs) -> bool
	{
		using Traits = std::char_traits<Char>;
		return detail::traits_string_sup_equal<Traits>(string_data(lhs), string_size(lhs), rhs.data(), rhs.size());
	}

	////////////////////////////////////////////////////////////////
	// Concatenation operators
	////////////////////////////////////////////////////////////////

	template<class Char, class A, size_t S, class String, std::enable_if_t<is_generic_string_for_v<String, Char>, int> = 0>
	auto operator+(const tiny_string<Char, A, S>& lhs, const String& rhs)
	{
		tiny_string<Char, A, S> ret(lhs);
		ret.append(string_data(rhs), string_size(rhs));
		return ret;
	}
	template<class Char, class A, size_t S>
	auto operator+(const tiny_string<Char, A, S>& lhs, Char c)
	{
		tiny_string<Char, A, S> ret(lhs);
		ret.append(c);
		return ret;
	}

	template<class Char, class A, size_t S, class String, std::enable_if_t<is_generic_string_for_v<String, Char> && !is_tiny_string_v<String>, int> = 0>
	auto operator+(const String& lhs, const tiny_string<Char, A, S>& rhs)
	{
		using traits = std::allocator_traits<A>;
		tiny_string<Char, A, S> ret(lhs, traits::select_on_container_copy_construction(rhs.get_allocator()));
		ret.append(string_data(rhs), string_size(rhs));
		return ret;
	}
	template<class Char, class A, size_t S>
	auto operator+(Char c, const tiny_string<Char, A, S>& rhs)
	{
		using traits = std::allocator_traits<A>;
		tiny_string<Char, A, S> ret(traits::select_on_container_copy_construction(rhs.get_allocator()));
		ret.append(c);
		ret.append(string_data(rhs), string_size(rhs));
		return ret;
	}

	//////
	// String related type traits
	//////

	/// @brief Detect basic_string
	template<class T>
	struct is_basic_string : std::false_type
	{
	};
	template<class Char, class Traits, class Al>
	struct is_basic_string<std::basic_string<Char, Traits, Al>> : std::true_type
	{
	};

	/// @brief Detect basic_string
	template<class T>
	struct is_basic_string_view : std::false_type
	{
	};
	template<class Char, class Traits>
	struct is_basic_string_view<std::basic_string_view<Char, Traits>> : std::true_type
	{
	};

	/// @brief Detect tiny_string, std::basic_string, but not std::basic_string_view
	template<class T>
	struct is_allocated_string : std::false_type
	{
	};
	template<class Char, class Al, size_t S>
	struct is_allocated_string<tiny_string<Char, Al, S>> : std::true_type
	{
	};
	template<class Char, class Traits, class Allocator>
	struct is_allocated_string<std::basic_string<Char, Traits, Allocator>> : std::true_type
	{
	};

	/// @brief Detect all possible string types (std::string, tstring, std::string_view, const char*, char*
	template<class T, class = void>
	struct is_generic_char_string : std::false_type
	{
	};
	template<class T>
	struct is_generic_char_string<T, typename std::enable_if<is_generic_string_v<T>, void>::type>
	{
		static constexpr bool value = std::is_same_v<char, character_type_t<T>>;
	};

	/// @brief Detect generic string view: std::string_view, char*, const char*
	template<class T>
	struct is_generic_string_view
	{
		static constexpr bool value = is_character_pointer<T>::value;
	};
	template<class Char, class Traits>
	struct is_generic_string_view<std::basic_string_view<Char, Traits>> : std::true_type
	{
	};

	// Specialization of is_relocatable

	template<class Char, class Al, size_t S>
	struct is_relocatable<tiny_string<Char, Al, S>> : is_relocatable<Al>
	{
	};

	/**********************************
	 * Reading/writing from/to streams
	 * ********************************/

	template<class Elem, class Traits, size_t Size, class Alloc>
	auto operator>>(std::basic_istream<Elem, Traits>& iss, tiny_string<Elem, Alloc, Size>& str) ->
	  typename std::enable_if<std::is_same_v<Traits, std::char_traits<Elem>>, std::basic_istream<Elem, Traits>>::type&
	{ // extract a string
		typedef std::ctype<Elem> c_type;
		typedef std::basic_istream<Elem, Traits> stream_type;
		typedef tiny_string<Elem, Alloc, Size> string_type;
		typedef typename string_type::size_type size_type;

		std::ios_base::iostate state = std::ios_base::goodbit;
		bool changed = false;
		const typename stream_type::sentry ok(iss);

		if (ok) { // state okay, extract characters
			const c_type& ctype_fac = std::use_facet<c_type>(iss.getloc());
			str.clear();

			try {
				size_type size = 0 < iss.width() && static_cast<size_type>(iss.width()) < str.max_size() ? static_cast<size_type>(iss.width()) : str.max_size();
				typename Traits::int_type _Meta = iss.rdbuf()->sgetc();

				for (; 0 < size; --size, _Meta = iss.rdbuf()->snextc())
					if (Traits::eq_int_type(Traits::eof(), _Meta)) { // end of file, quit
						state |= std::ios_base::eofbit;
						break;
					}
					else if (ctype_fac.is(c_type::space, Traits::to_char_type(_Meta)))
						break; // whitespace, quit
					else {	       // add character to string
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

	template<class Elem, class Traits, size_t Size, class Alloc, std::enable_if_t<std::is_same_v<Traits, std::char_traits<Elem>>, int> = 0>
	auto operator<<(std::basic_ostream<Elem, Traits>& oss, const tiny_string<Elem, Alloc, Size>& str) -> std::basic_ostream<Elem, Traits>&
	{ // insert a string
		typedef std::basic_ostream<Elem, Traits> myos;
		typedef tiny_string<Elem, Alloc, Size> mystr;
		typedef typename mystr::size_type mysizt;

		std::ios_base::iostate state = std::ios_base::goodbit;
		mysizt size = str.size();
		mysizt pad = oss.width() <= 0 || static_cast<mysizt>(oss.width()) <= size ? 0 : static_cast<mysizt>(oss.width()) - size;
		const typename myos::sentry sok(oss);

		if (!sok)
			state |= std::ios_base::badbit;
		else { // state okay, insert characters
			try {
				if ((oss.flags() & std::ios_base::adjustfield) != std::ios_base::left)
					for (; 0 < pad; --pad) // pad on left
						if (Traits::eq_int_type(Traits::eof(),
									oss.rdbuf()->sputc(oss.fill()))) { // insertion failed, quit
							state |= std::ios_base::badbit;
							break;
						}

				if (state == std::ios_base::goodbit && oss.rdbuf()->sputn(str.c_str(), static_cast<std::streamsize>(size)) != static_cast<std::streamsize>(size))
					state |= std::ios_base::badbit;
				else
					for (; 0 < pad; --pad) // pad on right
						if (Traits::eq_int_type(Traits::eof(),
									oss.rdbuf()->sputc(oss.fill()))) { // insertion failed, quit
							state |= std::ios_base::badbit;
							break;
						}
				oss.width(0);
			}
			catch (...) {
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
	template<class Char, class Allocator, size_t Size>
	class hasher<seq::tiny_string<Char, Allocator, Size>>
	{
		using Traits = std::char_traits<Char>;

	public:
		using is_transparent = std::true_type;
		using is_avalanching = std::true_type;
		SEQ_STR_INLINE_STRONG auto operator()(const seq::tiny_string<Char, Allocator, Size>& str) const noexcept -> size_t
		{
			return seq::hash_bytes_komihash((str.data()), str.size() * sizeof(Char));
		}
		template<size_t S, class Al>
		SEQ_STR_INLINE_STRONG auto operator()(const seq::tiny_string<Char, Al, S>& str) const noexcept -> size_t
		{
			return seq::hash_bytes_komihash((str.data()), str.size() * sizeof(Char));
		}
		template<class Al>
		SEQ_STR_INLINE_STRONG auto operator()(const std::basic_string<Char, Traits, Al>& str) const noexcept -> size_t
		{
			return seq::hash_bytes_komihash((str.data()), str.size() * sizeof(Char));
		}
		SEQ_STR_INLINE_STRONG auto operator()(const Char* str) const noexcept -> size_t
		{
			if (!str)
				return 0;
			return seq::hash_bytes_komihash((str), Traits::length(str) * sizeof(Char));
		}

		SEQ_STR_INLINE_STRONG auto operator()(const std::basic_string_view<Char, Traits>& str) const noexcept -> size_t
		{
			return seq::hash_bytes_komihash((str.data()), str.size() * sizeof(Char));
		}
	};

	/// @brief Specialization of seq::hasher for basic_string
	/// Uses seq hash function hash_bytes_komihash()
	///
	template<class Char, class Allocator>
	class hasher<std::basic_string<Char, std::char_traits<Char>, Allocator>>
	{
		using Traits = std::char_traits<Char>;

	public:
		using is_transparent = std::true_type;
		using is_avalanching = std::true_type;

		template<class A, size_t S>
		SEQ_STR_INLINE_STRONG auto operator()(const seq::tiny_string<Char, A, S>& str) const noexcept -> size_t
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
			if (!str)
				return 0;
			return seq::hash_bytes_komihash((str), Traits::length(str) * sizeof(Char));
		}
		SEQ_STR_INLINE_STRONG auto operator()(const std::basic_string_view<Char, Traits>& str) const noexcept -> size_t
		{
			return seq::hash_bytes_komihash((str.data()), str.size() * sizeof(Char));
		}
	};

	/// @brief Specialization of seq::hasher for basic_string_view
	/// Uses seq hash function hash_bytes_komihash()
	///
	template<class Char, class Traits>
	class hasher<std::basic_string_view<Char, Traits>> : public hasher<std::basic_string<Char, Traits, std::allocator<Char>>>
	{
	};

	// Overload std::swap for tiny_string.
	template<class Char, class Allocator, size_t Size>
	SEQ_STR_INLINE_STRONG void swap(tiny_string<Char, Allocator, Size>& a, tiny_string<Char, Allocator, Size>& b) noexcept(noexcept(a.swap(b)))
	{
		a.swap(b);
	}
} // end namespace seq

namespace std
{

	/// @brief Specialization of std::hash for tiny_string
	/// This specialization uses a (relatively) strong hash function: murmurhash2
	///
	template<class Char, class Allocator, size_t Size>
	class hash<seq::tiny_string<Char, Allocator, Size>>
	{
		using Traits = std::char_traits<Char>;

	public:
		using is_transparent = std::true_type;
		using is_avalanching = std::true_type;
		SEQ_STR_INLINE_STRONG auto operator()(const seq::tiny_string<Char, Allocator, Size>& str) const noexcept -> size_t
		{
			return seq::hash_bytes_komihash(str.data(), str.size() * sizeof(Char));
		}
		template<size_t S, class Al>
		SEQ_STR_INLINE_STRONG auto operator()(const seq::tiny_string<Char, Al, S>& str) const noexcept -> size_t
		{
			return seq::hash_bytes_komihash(str.data(), str.size() * sizeof(Char));
		}
		template<class Al>
		SEQ_STR_INLINE_STRONG auto operator()(const std::basic_string<Char, Traits, Al>& str) const noexcept -> size_t
		{
			return seq::hash_bytes_komihash(str.data(), str.size() * sizeof(Char));
		}
		SEQ_STR_INLINE_STRONG auto operator()(const Char* str) const noexcept -> size_t
		{
			if (!str)
				return 0;
			return seq::hash_bytes_komihash(str, Traits::length(str) * sizeof(Char));
		}

		SEQ_STR_INLINE_STRONG auto operator()(const std::basic_string_view<Char, Traits>& str) const noexcept -> size_t
		{
			return seq::hash_bytes_komihash(str.data(), str.size() * sizeof(Char));
		}
	};

} // end namespace std

#endif
