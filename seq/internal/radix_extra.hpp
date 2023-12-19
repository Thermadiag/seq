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

#ifndef SEQ_RADIX_EXTRA_HPP
#define SEQ_RADIX_EXTRA_HPP



/** @file */

/** \addtogroup containers
 *  @{
 */

#include <tuple>
#include "radix_tree.hpp"

namespace seq
{

	namespace radix_detail
	{

		/// @brief Hash value type for strings of wide char types and sorted radix tree
		template<class CharT>
		struct WStringHash
		{
			static_assert(sizeof(CharT) == 2 || sizeof(CharT) == 4, "invalid wide char size");

			using wstring_view = basic_tstring_view<CharT>;
			using size_type = std::uint64_t;
			const CharT* data;
			std::uint64_t size;
			std::uint64_t hash_shift;

			WStringHash() {}
			WStringHash(const CharT* d, size_type s) noexcept
				:data(d), size(s), hash_shift(0)
			{
			}
			WStringHash(size_t sh, const CharT* d, size_type s) noexcept
				:data(d), size(s), hash_shift(sh) {
			}
			size_t get_shift() const noexcept {
				return static_cast<size_t>(hash_shift);
			}
			size_t get_size() const noexcept {
				return static_cast<size_t>(size * 8ULL);
			}

			SEQ_ALWAYS_INLINE std::uint64_t read_from_byte(size_t byte_pos) const noexcept
			{
				size_t char_offset = (byte_pos) / sizeof(CharT);
				size_t byte_offset = (byte_pos) % sizeof(CharT);
				std::uint64_t hash = 0;
				
				if (SEQ_LIKELY(size  >= char_offset + 8U / sizeof(CharT)))
					memcpy(&hash, data + char_offset, 8U);
				else if (char_offset < size )
					memcpy(&hash, data + char_offset, static_cast<size_t>((size  - char_offset) * sizeof(CharT)));

#if SEQ_BYTEORDER_ENDIAN != SEQ_BYTEORDER_BIG_ENDIAN
				if SEQ_CONSTEXPR (sizeof(CharT) == 2) 
				{
					unsigned short* p = reinterpret_cast<unsigned short*>(&hash);
					p[0] = byte_swap_16(p[0]);
					p[1] = byte_swap_16(p[1]);
					p[2] = byte_swap_16(p[2]);
					p[3] = byte_swap_16(p[3]);
				}
				else 
				{
					unsigned * p = reinterpret_cast<unsigned*>(&hash);
					p[0] = byte_swap_32(p[0]);
					p[1] = byte_swap_32(p[1]);
				}
				hash = byte_swap_64(hash);
#endif
				return hash << (byte_offset * 8U);
			}

			SEQ_ALWAYS_INLINE unsigned n_bits(size_t count) const noexcept
			{
				return n_bits(static_cast<size_t>(hash_shift), count);
			}
			SEQ_ALWAYS_INLINE unsigned n_bits(size_t start, size_t count) const noexcept
			{
				if (count == 0) return 0;

				size_t byte_offset = start / 8U;
				size_t bit_offset = (start) & 7U;
				std::uint64_t hash = read_from_byte(byte_offset);
				return static_cast<unsigned>((hash << bit_offset) >> (64U - count));

			}
			SEQ_ALWAYS_INLINE bool add_shift(size_type shift) const noexcept
			{
				const_cast<WStringHash*>(this)->hash_shift += shift;
				return (hash_shift) <= size * 8U;; //still valid
			}
			unsigned get() const noexcept
			{
				return n_bits(static_cast<size_t>(hash_shift), 32);
			}

			template<unsigned BitStep, class ExtractKey, class Hasher, class Iter>
			static size_t nb_common_bits(const Hasher& h, size_t start_bit, Iter start, Iter end) noexcept
			{

				size_t max_bits = ExtractKey{}(*start).size() * sizeof(CharT) * 8U > start_bit ? ExtractKey{}(*start).size() * sizeof(CharT) * 8U - start_bit : 0;
				for (Iter it = start; it != end; ++it) {
					if (ExtractKey{}(*it).size() * sizeof(CharT) * 8U > start_bit)
					{
						size_t bits = ExtractKey{}(*it).size() * sizeof(CharT) * 8U - start_bit;
						max_bits = std::max(max_bits, bits);
					}
				}

				wstring_view first = ExtractKey{}(*start);
				++start;
				size_t bits = max_bits;

				for (; start != end && bits > 0; ++start)
				{
					auto fi = h.hash(first);
					auto tmp = h.hash(ExtractKey{}(*start));

					fi.add_shift(start_bit);
					tmp.add_shift(start_bit);

					size_t common = 0;

					for (;;) {
						unsigned x = fi.get() ^ tmp.get();
						if (x == 0) {
							common += 32;
							bool l1 = fi.add_shift(32);
							bool l2 = tmp.add_shift(32);
							if (!l1 && !l2) {
								common = max_bits;
								break;
							}
						}
						else {
							common += 32U - bit_scan_reverse_32(x) - 1U;
							break;
						}
					}
					bits = std::min(bits, common);
					bits = (bits / BitStep) * BitStep;

				}

				return bits;
			}

			template<class Key>
			static SEQ_ALWAYS_INLINE bool check_prefix(const WStringHash& hash, const Key& val, size_t bits) noexcept
			{
				// get hash to compare to
				auto match = WStringHash(hash.get_shift(), val.data(), val.size());

				if (match.get_shift() >= match.get_size() && hash.get_shift() >= hash.get_size()) {
					hash.add_shift(bits);
					return true;
				}

				size_t words = bits / 32U;
				for (size_t i = 0; i < words; ++i)
				{
					if (hash.get() != match.get()) return false;
					hash.add_shift(32U);
					match.add_shift(32U);

				}
				if (size_t rem = bits & 31U) {
					if (hash.n_bits(rem) != match.n_bits(rem))
						return false;
					hash.add_shift(rem);
				}
				return true;
			}

		};



		template<class Tuple, int N = std::tuple_size<Tuple>::value >
		struct GetTupleSize
		{
			static constexpr int tuple_size = std::tuple_size<Tuple>::value;
			static constexpr int pos = tuple_size - N;

			static size_t get(size_t prev, const Tuple& t) noexcept
			{
				using type = typename std::tuple_element<pos, Tuple>::type;
				using key = typename ExtractKeyResultType< default_key<type>, type >::type;
				size_t s = SortedHasher< key>{}.hash(std::get<pos>(t)).get_size();
				return GetTupleSize<Tuple, N - 1>::get(s + prev, t);
			}
		};
		template<class Tuple>
		struct GetTupleSize<Tuple, 0>
		{
			static size_t get(size_t s, const Tuple& /*unused*/) noexcept {
				return s;
			}
		};


		template<class Tuple, int N = std::tuple_size<Tuple>::value >
		struct GetNBits
		{
			static constexpr int tuple_size = std::tuple_size<Tuple>::value;
			static constexpr int pos = tuple_size - N;

			static unsigned get(unsigned prev, unsigned prev_offset, const Tuple& t, size_t start, size_t count) noexcept
			{
				using type = typename std::tuple_element<pos, Tuple>::type;
				using key = typename ExtractKeyResultType< default_key<type>, type >::type;

				static_assert(SortedHasher< key>::variable_length == false, "composite keys only accept types of fixed size");

				auto hash = SortedHasher< key>{}.hash(std::get<pos>(t));
				size_t size = hash.get_size();

				if(N !=1 && start >= size)
					return GetNBits<Tuple, N - 1>::get(prev, prev_offset, t, start - size, count);
				
				// for the last element of the tuple, read_bits == count
				size_t read_bits = (N == 1) ? count : std::min(count, size - start);
				unsigned tmp = hash.n_bits(start, read_bits);
				
				prev = (prev << read_bits) | tmp;
				count -= read_bits;
				if (count == 0)
					return prev;

				return GetNBits<Tuple, N - 1>::get(prev, prev_offset, t, 0, count);
			}
		};
		template<class Tuple>
		struct GetNBits<Tuple, 0>
		{
			static unsigned get(unsigned prev, unsigned , const Tuple& , size_t , size_t ) noexcept
			{
				return prev;
			}
		};



		template<class Tuple, int N = std::tuple_size<Tuple>::value >
		struct GetTinyHash
		{
			static constexpr int tuple_size = std::tuple_size<Tuple>::value;
			static constexpr int pos = tuple_size - N;

			static unsigned char get(unsigned char prev, const Tuple& t) noexcept
			{
				using type = typename std::tuple_element<pos, Tuple>::type;
				using key = typename ExtractKeyResultType< default_key<type>, type >::type;
				unsigned char s = SortedHasher< key>::tiny_hash(SortedHasher< key>{}.hash(std::get<pos>(t)), std::get<pos>(t));
				return GetTinyHash<Tuple, N - 1>::get(s ^ prev, t);
			}
		};
		template<class Tuple>
		struct GetTinyHash<Tuple, 0>
		{
			static unsigned char get(unsigned char s, const Tuple& /*unused*/) noexcept {
				return s;
			}
		};



		/// @brief Hash value type for strings of wide char types and sorted radix tree
		template<class Tuple>
		struct TupleHash
		{
			using size_type = std::uint64_t;
			const Tuple tuple;
			size_type hash_shift;
			size_type size;

			TupleHash() {}
			TupleHash(const Tuple& t) noexcept
				:tuple(t),  hash_shift(0)
			{
				size = GetTupleSize<Tuple>::get(0, t);
			}
			TupleHash(size_t sh, const Tuple& t) noexcept
				:tuple(t), hash_shift(sh) 
			{
				size = GetTupleSize<Tuple>::get(0, t);
			}
			size_t get_shift() const noexcept {
				return static_cast<size_t>(hash_shift);
			}
			size_t get_size() const noexcept {
				return static_cast<size_t>(size);
			}
			
			SEQ_ALWAYS_INLINE unsigned n_bits(size_t count) const noexcept
			{
				return n_bits(static_cast<size_t>(hash_shift), count);
			}
			SEQ_ALWAYS_INLINE unsigned n_bits(size_t start, size_t count) const noexcept
			{
				if (count == 0) return 0;
				return GetNBits<Tuple>::get(0, 0, tuple, start, count);

			}
			SEQ_ALWAYS_INLINE bool add_shift(size_type shift) const noexcept
			{
				const_cast<TupleHash*>(this)->hash_shift += shift;
				return (hash_shift) <= size ; //still valid
			}
			unsigned get() const noexcept
			{
				return n_bits(static_cast<size_t>(hash_shift), 32);
			}

			template<unsigned BitStep, class ExtractKey, class Hasher, class Iter>
			static size_t nb_common_bits(const Hasher& h, size_t start_bit, Iter start, Iter end) noexcept
			{

				size_t max_bits = TupleHash(ExtractKey{}(*start)).get_size();
				for (Iter it = start; it != end; ++it) {
					size_t bits = TupleHash(ExtractKey{}(*start)).get_size();
					if (bits > start_bit)
					{
						bits -= start_bit;
						max_bits = std::max(max_bits, bits);
					}
				}

				Tuple first = ExtractKey{}(*start);
				++start;
				size_t bits = max_bits;

				for (; start != end && bits > 0; ++start)
				{
					auto fi = h.hash(first);
					auto tmp = h.hash(ExtractKey{}(*start));

					fi.add_shift(start_bit);
					tmp.add_shift(start_bit);

					size_t common = 0;

					for (;;) {
						unsigned x = fi.get() ^ tmp.get();
						if (x == 0) {
							common += 32;
							bool l1 = fi.add_shift(32);
							bool l2 = tmp.add_shift(32);
							if (!l1 && !l2) {
								common = max_bits;
								break;
							}
						}
						else {
							common += 32U - bit_scan_reverse_32(x) - 1U;
							break;
						}
					}
					bits = std::min(bits, common);
					bits = (bits / BitStep) * BitStep;

				}

				return bits;
			}

			template<class Key>
			static SEQ_ALWAYS_INLINE bool check_prefix(const TupleHash& hash, const Key& val, size_t bits) noexcept
			{
				// get hash to compare to
				auto match = TupleHash(hash.get_shift(), val);

				if (match.get_shift() >= match.get_size() && hash.get_shift() >= hash.get_size()) {
					hash.add_shift(bits);
					return true;
				}

				size_t words = bits / 32U;
				for (size_t i = 0; i < words; ++i)
				{
					if (hash.get() != match.get()) return false;
					hash.add_shift(32U);
					match.add_shift(32U);

				}
				if (size_t rem = bits & 31U) {
					if (hash.n_bits(rem) != match.n_bits(rem))
						return false;
					hash.add_shift(rem);
				}
				return true;
			}

		};




		template<class CharT>
		struct is_wstring : std::false_type {};

		template<class CharT, class Traits>
		struct is_wstring< tiny_string<CharT,Traits,view_allocator<CharT>,0> >
		{
			static constexpr bool value = sizeof(CharT) > 1;
		};

		template<class CharT, class Traits, class Allocator>
		struct is_wstring<std::basic_string<CharT, Traits, Allocator> >
		{
			static constexpr bool value = sizeof(CharT) > 1;
		};
#ifdef SEQ_HAS_CPP_17
		template<class CharT, class Traits>
		struct is_wstring<std::basic_string_view<CharT, Traits> >
		{
			static constexpr bool value = sizeof(CharT) > 1;
		};
#endif




		/// @brief Hash function for string types and sorted radix tree
		template<class CharT>
		struct SortedHasher<basic_tstring_view<CharT>, void > : BaseSortedHasher
		{
			using type = WStringHash<CharT>;
			using const_reference = const type&;
			static constexpr bool prefix_search = true;
			static constexpr bool variable_length = true;
			static constexpr size_t max_bits = static_cast<size_t>(-1);

			SEQ_ALWAYS_INLINE type hash(const basic_tstring_view<CharT>& k) const noexcept
			{
				return type(k.data(), k.size());
			}
			SEQ_ALWAYS_INLINE type hash_shift(size_t shift, const  basic_tstring_view<CharT>& k) const noexcept
			{
				return type(shift, k.data(), k.size());
			}
			template<class Hash>
			static std::uint8_t tiny_hash(const Hash&, const  basic_tstring_view<CharT>& v) noexcept
			{
				std::uint64_t h = 14695981039346656037ULL;
				const std::uint8_t* ptr = reinterpret_cast<const std::uint8_t*>(v.data());
				const std::uint8_t* end_string = ptr + v.size() * sizeof(CharT);
				const std::uint8_t* end = end_string - (sizeof(std::uint64_t) - 1);
				while (ptr < end) {
					auto k = read_64(ptr);
					h ^= k;
					ptr += sizeof(size_t);
				}
				if (ptr < end_string - (sizeof(std::uint32_t) - 1)) {
					auto k = read_32(ptr);
					h ^= k;
					ptr += sizeof(std::uint32_t);
				}
				if (ptr < end_string - (sizeof(std::uint16_t) - 1)) {
					auto k = read_16(ptr);
					h ^= k;
					ptr += sizeof(std::uint16_t);
				}
				if (ptr < end_string)
					h ^= *ptr;
				return static_cast<std::uint8_t>((h * 0xc4ceb9fe1a85ec53ull) >> 56U);
			}

		};




		/// @brief Hash function for composite types and sorted radix tree
		template<class... Args>
		struct SortedHasher<std::tuple<Args...>, void > : BaseSortedHasher
		{
			using tuple_type = std::tuple<Args...>;
			using type = TupleHash<tuple_type>;
			using const_reference = const type&;
			static constexpr bool prefix_search = true;
			static constexpr bool variable_length = false;// true;
			static constexpr size_t max_bits = static_cast<size_t>(-1);

			SEQ_ALWAYS_INLINE type hash(const tuple_type& k) const noexcept
			{
				return type(k);
			}
			SEQ_ALWAYS_INLINE type hash_shift(size_t shift, const tuple_type& k) const noexcept
			{
				return type(shift, k);
			}
			template<class Hash>
			static std::uint8_t tiny_hash(const Hash&, const tuple_type& v) noexcept
			{
				return GetTinyHash< tuple_type>::get(0, v);
			}

		};

	}// end radix_detail

	/// @brief Key extractor for wide string types
	/// @tparam T wide string type
	template<class T>
	struct default_key<T, typename std::enable_if<radix_detail::is_wstring<T>::value, void>::type >
	{
		template<class CharT>
		basic_tstring_view<CharT> operator()(const basic_tstring_view<CharT>& val) const noexcept {
			return val;
		}
		template<class CharT, class Traits, class Allocator>
		basic_tstring_view<CharT> operator()(const std::basic_string<CharT, Traits, Allocator>& val) const noexcept {
			return val;
		}
		template<class CharT>
		basic_tstring_view<CharT> operator()(const CharT* val) const noexcept {
			return basic_tstring_view<CharT>(val);
		}
#ifdef SEQ_HAS_CPP_17
		template<class CharT, class Traits>
		basic_tstring_view<CharT> operator()(const std::basic_string_view<CharT, Traits> & val) const noexcept {
			return val;
		}
#endif
	};
}

/** @}*/
//end containers

#endif
