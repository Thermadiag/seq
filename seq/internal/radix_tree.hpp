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

#ifndef SEQ_RADIX_TREE_HPP
#define SEQ_RADIX_TREE_HPP

#include "../bits.hpp"
#include "../tagged_pointer.hpp"
#include "../hash.hpp"
#include "../flat_map.hpp"
#include "../type_traits.hpp"
#include "simd.hpp"

namespace seq
{
	struct default_key
	{
		template<class U>
		SEQ_ALWAYS_INLINE const U& operator()(const U& v) const
		{
			return v;
		}
	};

	/// @brief Default (dummy) less functor for radix hash tree
	struct default_less
	{
		template<class A, class B>
		bool operator()(const A&, const B&) const noexcept
		{
			return false;
		}
	};

	namespace radix_detail
	{

		/// @brief Find result type of ExtractKey::operator() on T object
		template<class ExtractKey, class T>
		struct ExtractKeyResultType
		{
			using rtype = decltype(ExtractKey{}(std::declval<const T&>()));
			using type = typename std::decay<rtype>::type;
		};

		template<class T, class = void>
		struct has_data_pointer : std::false_type
		{
			using type = char;
		};
		template<class T>
		struct has_data_pointer<T, typename std::void_t<decltype(std::declval<T&>().data())>>
		{
			using pointer = decltype(std::declval<T&>().data());
			using ctype = typename std::remove_pointer<pointer>::type;
			using type = typename std::remove_const<ctype>::type;
			static constexpr bool value = std::is_pointer<pointer>::value;
		};
		template<class T, class = void>
		struct has_size : std::false_type
		{
			using type = char;
		};
		template<class T>
		struct has_size<T, typename std::void_t<decltype(std::declval<T&>().size())>>
		{
			using type = decltype(std::declval<T&>().size());
			static constexpr bool value = std::is_integral<type>::value;
		};
		template<class T>
		struct is_native_type : std::bool_constant<std::is_pointer<T>::value || std::is_arithmetic<T>::value>
		{
		};
		template<class T>
		struct has_native_data_pointer
		{
			using data = has_data_pointer<T>;
			using type = typename data::type;
			static constexpr bool value = (data::value && has_size<T>::value && is_native_type<type>::value);
		};

		static SEQ_ALWAYS_INLINE auto byte_swap(std::uint8_t v) noexcept
		{
			return v;
		}
		static SEQ_ALWAYS_INLINE auto byte_swap(std::uint16_t v) noexcept
		{
			return byte_swap_16(v);
		}
		static SEQ_ALWAYS_INLINE auto byte_swap(std::uint32_t v) noexcept
		{
			return byte_swap_32(v);
		}
		static SEQ_ALWAYS_INLINE auto byte_swap(std::uint64_t v) noexcept
		{
			return byte_swap_64(v);
		}
		template<class T>
		static SEQ_ALWAYS_INLINE auto byte_swap(const T* v) noexcept
		{
			if constexpr (sizeof(T*) == 8)
				return byte_swap_64(reinterpret_cast<std::uintptr_t>(v));
			else
				return byte_swap_32(reinterpret_cast<std::uint32_t>(v));
		}

		//
		// Convert arithmetic types and pointers to unsigned integers
		// while keeping ordering.
		//

		template<class T>
		static SEQ_ALWAYS_INLINE auto to_uint(T k) noexcept
		{
			using type = typename std::make_unsigned<T>::type;
			type val = static_cast<type>(k);
			if constexpr (std::is_signed<T>::value)
				// wrap around to keep an unsigned value in the same order as the signed one
				val += (static_cast<type>(1) << (static_cast<type>(sizeof(T) * 8 - 1)));
			return val;
		}
		template<class T>
		static SEQ_ALWAYS_INLINE auto to_uint(const T* p) noexcept
		{
			// Convert pointer to uintptr_t
			return reinterpret_cast<std::uintptr_t>(p);
		}
		template<class T>
		static SEQ_ALWAYS_INLINE auto to_uint(T* p) noexcept
		{
			// Convert pointer to uintptr_t
			return reinterpret_cast<std::uintptr_t>(p);
		}
		static SEQ_ALWAYS_INLINE auto to_uint(wchar_t k) noexcept
		{
			// Convert wchar_t to its unsigned representation.
			// That's because wchar_t string comparison works
			// on the unsigned character.
			// Other character types (char8_t, char16_t, char32_t)
			// are already unsigned.
			return static_cast<std::make_unsigned<wchar_t>::type>(k);
		}
		static SEQ_ALWAYS_INLINE bool to_uint(bool k) noexcept
		{
			return k;
		}
		static SEQ_ALWAYS_INLINE std::uint32_t to_uint(float k) noexcept
		{
			union
			{
				float d;
				std::uint32_t u;
			} u = { k };
			// Flip all except top if top bit is set.
			u.u ^= ((static_cast<unsigned>((static_cast<int>(u.u)) >> 31)) >> 1);
			// Flip top bit.
			return (u.u ^ (1u << 31u));
		}
		static SEQ_ALWAYS_INLINE std::uint64_t to_uint(double k) noexcept
		{
			// Interpret double as 64-bit unsigned.
			union
			{
				double d;
				std::uint64_t u;
			} u = { k };
			// Flip all except top if top bit is set.
			u.u ^= ((static_cast<std::uint64_t>((static_cast<std::int64_t>(u.u)) >> 63ll)) >> 1ll);
			// Flip top bit.
			return (u.u ^ (1ull << 63ull));
		}

		//
		// Radix hasher classes
		//

		template<class T>
		struct make_unsigned_or_bool : std::make_unsigned<T>
		{
			// make_unsigned that also work on bool type
			using type = typename std::conditional<std::is_same<T, bool>::value, bool, typename std::make_unsigned<T>::type>::type;
		};
		template<class T>
		struct make_unsigned_from_float
		{
			// Unsigned type with the same size as float type
			using type = typename std::conditional<std::is_same<T, float>::value, uint32_t, uint64_t>::type;
		};

		/// @brief Default invalid hash class for the radix tree
		template<class Key, class = void>
		struct RadixHasher
		{
			static constexpr size_t max_bits = 0; // invalid value
		};

		/// @brief Integral hash class
		template<class Integral>
		struct RadixHasher<Integral, typename std::enable_if<std::is_integral<Integral>::value, void>::type>
		{
			using less_type = default_less;
			using integral_type = typename make_unsigned_or_bool<Integral>::type;
			static constexpr integral_type integral_bits = sizeof(integral_type) * 8U;
			static constexpr bool variable_length = false;
			static constexpr bool prefix_search = true;
			static constexpr bool has_fast_check_prefix = true; // provides a check_prefix() function
			static constexpr size_t max_bits = sizeof(Integral) * 8U;
			static constexpr size_t bit_step = 2;
			static constexpr bool is_transparent = true;

			integral_type value = 0;

			SEQ_ALWAYS_INLINE auto n_bits(size_t start, size_t count) const noexcept
			{
				if constexpr (sizeof(integral_type) < 4u) {
					// Special trick for 16 and 8 bits integers
					unsigned v = static_cast<unsigned>(value) << (32u - sizeof(integral_type) * 8u);
					return (v << static_cast<unsigned>(start)) >> (32u - static_cast<unsigned>(count));
				}
				else
					return static_cast<unsigned>((value << static_cast<integral_type>(start)) >> (integral_bits - static_cast<integral_type>(count)));
			}

			SEQ_ALWAYS_INLINE std::uint8_t tiny_hash() const noexcept { return static_cast<std::uint8_t>(hash_finalize(static_cast<size_t>(value))); }

			// Retuns the total number of bits
			SEQ_ALWAYS_INLINE constexpr size_t get_size() const noexcept { return static_cast<size_t>(integral_bits); }
			template<class U>
			SEQ_ALWAYS_INLINE auto hash(const U& k) const noexcept
			{
				return RadixHasher<Integral>{ to_uint(static_cast<Integral>(k)) };
			}

			template<class U>
			static SEQ_ALWAYS_INLINE bool check_prefix(RadixHasher<U> hash, RadixHasher<U> match, size_t start, size_t bits) noexcept
			{
				// Faster check_prefix() version
				if constexpr (sizeof(U) < 4u)
					return hash.n_bits(start, bits) == match.n_bits(start, bits);
				else {
					auto shift = (RadixHasher<U>::max_bits - bits);
					return ((hash.value << start) >> shift) == ((match.value << start) >> shift);
				}
			}
			template<class U, class V>
			static SEQ_ALWAYS_INLINE bool less(const U& l, const V& r) noexcept
			{
				return static_cast<Integral>(l) < static_cast<Integral>(r);
			}
			template<class U, class V>
			SEQ_ALWAYS_INLINE bool equal(const U& l, const V& r) const noexcept
			{
				return static_cast<Integral>(l) == static_cast<Integral>(r);
			}
		};

		/// @brief Hash class for float and double
		template<class T>
		struct RadixHasher<T, typename std::enable_if<std::is_floating_point<T>::value, void>::type> : RadixHasher<typename make_unsigned_from_float<T>::type>
		{
			static_assert(std::numeric_limits<T>::is_iec559, "");
			using unsigned_type = typename make_unsigned_from_float<T>::type;

			SEQ_ALWAYS_INLINE RadixHasher(unsigned_type v = 0) noexcept
			  : RadixHasher<unsigned_type>{ v }
			{
			}
			template<class U>
			SEQ_ALWAYS_INLINE auto hash(U k) const noexcept
			{
				return RadixHasher<T>{ to_uint(static_cast<T>(k)) };
			}
			template<class U, class V>
			static SEQ_ALWAYS_INLINE bool less(const U& l, const V& r) noexcept
			{
				return static_cast<T>(l) < static_cast<T>(r);
			}
			template<class U, class V>
			SEQ_ALWAYS_INLINE bool equal(const U& l, const V& r) const noexcept
			{
				return static_cast<T>(l) == static_cast<T>(r);
			}
		};

		/// @brief Hash class for pointers
		template<class T>
		struct RadixHasher<T, typename std::enable_if<std::is_pointer<T>::value, void>::type> : RadixHasher<uintptr_t>
		{
			SEQ_ALWAYS_INLINE RadixHasher(uintptr_t v = 0) noexcept
			  : RadixHasher<uintptr_t>{ v }
			{
			}
			template<class U>
			SEQ_ALWAYS_INLINE auto hash(const U& k) const noexcept
			{
				return RadixHasher<T>{ to_uint(static_cast<T>(k)) };
			}
			template<class U, class V>
			static SEQ_ALWAYS_INLINE bool less(const U& l, const V& r) noexcept
			{
				return static_cast<T>(l) < static_cast<T>(r);
			}
			template<class U, class V>
			SEQ_ALWAYS_INLINE bool equal(const U& l, const V& r) const noexcept
			{
				return static_cast<T>(l) == static_cast<T>(r);
			}
		};

		/// @brief Hash class for any kind of array of primitive type (std::string, std::wstring, std::vector, std::array...)
		template<class T>
		struct RadixHasher<T, typename std::enable_if<has_native_data_pointer<T>::value, void>::type>
		{
#if SEQ_BYTEORDER_ENDIAN != SEQ_BYTEORDER_BIG_ENDIAN
#define swap_b(v) byte_swap(v)
#else
#define swap_b(v) v
#endif
			static constexpr bool variable_length = true;
			static constexpr bool prefix_search = true;
			static constexpr bool has_fast_check_prefix = false;
			static constexpr size_t max_bits = static_cast<size_t>(-1); // unlimited
			static constexpr size_t bit_step = 4;
			using type = typename has_native_data_pointer<T>::type;
			using less_type = default_less;
			const type* data{ nullptr };
			size_t size{ 0 };

			SEQ_ALWAYS_INLINE auto get_size() const noexcept -> size_t { return static_cast<size_t>(size * sizeof(type) * 8ULL); }

			SEQ_ALWAYS_INLINE auto n_bits(size_t shift, size_t count) const noexcept -> unsigned
			{
				if SEQ_UNLIKELY (count == 0)
					return 0;

				size_t byte_offset = shift >> 3u;
				size_t bit_offset = (shift) & 7U;

				if constexpr (sizeof(type) == 1) {
					// One byte: string
					uint64_t hash = 0;
					if (size >= byte_offset + 8u)
						memcpy(&hash, data + byte_offset, 8u);
					else if (byte_offset < size)
						memcpy(&hash, data + byte_offset, static_cast<unsigned>(size - byte_offset));
					return static_cast<unsigned>((swap_b(hash) << bit_offset) >> (64u - count));
				}
				else {
					uint64_t hash = 0;
					size_t char_offset = byte_offset / sizeof(type);
					byte_offset = (byte_offset) % sizeof(type);

					if (size >= char_offset + 8U / sizeof(type))
						memcpy(&hash, data + char_offset, 8U);
					else if (char_offset < size)
						memcpy(&hash, data + char_offset, static_cast<size_t>((size - char_offset) * sizeof(type)));

					// That's easier in big endian as all the bytes
					// are already in the right order...
					using unsigned_type = typename RadixHasher<type>::integral_type;
					type* p = reinterpret_cast<type*>(&hash);
					unsigned_type* up = reinterpret_cast<unsigned_type*>(&hash);
					static_assert(!std::is_const<T>::value, " const type!");
					if constexpr (sizeof(type) == 2) {
						up[0] = swap_b(to_uint(p[0]));
						up[1] = swap_b(to_uint(p[1]));
						up[2] = swap_b(to_uint(p[2]));
						up[3] = swap_b(to_uint(p[3]));
						hash = swap_b(hash);
					}
					else if constexpr (sizeof(type) == 4) {
						up[0] = swap_b(to_uint(p[0]));
						up[1] = swap_b(to_uint(p[1]));
						hash = swap_b(hash);
					}
					else {
						// no byte swap here, just plain old bits manipulation
						up[0] = to_uint(p[0]);
						if (shift > 32 && (char_offset + 1) < size) {
							auto hash2 = to_uint(data[char_offset + 1]);
							hash = (hash << (byte_offset << 3u)) | (hash2 >> (64u - (byte_offset << 3u)));
							return static_cast<unsigned>((hash << bit_offset) >> (64u - count));
						}
					}
					return static_cast<unsigned>((hash << ((byte_offset << 3u) + bit_offset)) >> (64u - count));
				}
			}

			SEQ_ALWAYS_INLINE std::uint8_t tiny_hash() const noexcept { return static_cast<std::uint8_t>(hash_bytes_komihash(data, size * sizeof(type))); }

			SEQ_ALWAYS_INLINE auto hash(const type* p) const noexcept { return RadixHasher<T>{ p, std::char_traits<type>::length(p) }; }
			template<class U>
			SEQ_ALWAYS_INLINE auto hash(const U& k) const noexcept
			{
				return RadixHasher<T>{ static_cast<const type*>(k.data()), k.size() };
			}

			static SEQ_ALWAYS_INLINE bool less(const type* v1, size_t l1, const type* v2, size_t l2) noexcept
			{
				size_t l = l1 < l2 ? l1 : l2;
				if constexpr (sizeof(type) == 1) {
					// Comparison for one byte string
					using unsigned_char = typename std::make_unsigned<type>::type;
					if (l && *v1 != *v2)
						return (static_cast<unsigned_char>(*v1) < static_cast<unsigned_char>(*v2));
					const type* end = v1 + l;
					while ((v1 < end - (8 - 1))) {
						uint64_t r1 = read_64(v1);
						uint64_t r2 = read_64(v2);
						if (r1 != r2)
							return swap_b(r1) < swap_b(r2);
						v1 += 8;
						v2 += 8;
					}
					if ((v1 < (end - 3))) {
						uint32_t r1 = read_32(v1);
						uint32_t r2 = read_32(v2);
						if (r1 != r2)
							return swap_b(r1) < swap_b(r2);
						v1 += 4;
						v2 += 4;
					}
					if ((v1 < (end - 1))) {
						uint16_t r1 = read_16(v1);
						uint16_t r2 = read_16(v2);
						if (r1 != r2)
							return swap_b(r1) < swap_b(r2);
						v1 += 2;
						v2 += 2;
					}
					if (v1 != end && *v1 != *v2)
						return static_cast<unsigned_char>(*v1) < static_cast<unsigned_char>(*v2);
				}
				else {
					// For multi byte character string,
					// use standard lexicographic comparison
					if (l && *v1 != *v2)
						return (*v1) < (*v2);
					for (size_t i = 1; i < l; ++i)
						if (v1[i] != v2[i])
							return v1[i] < v2[i];
				}
				return l1 < l2;
			}
			template<class L, class R>
			static SEQ_ALWAYS_INLINE bool less(const L& l, const R& r) noexcept
			{
				auto lh = RadixHasher<T>{}.hash(l);
				auto rh = RadixHasher<T>{}.hash(r);
				return less(lh.data, lh.size, rh.data, rh.size);
			}

			static SEQ_ALWAYS_INLINE bool equal(const type* v1, size_t l1, const type* v2, size_t l2) noexcept
			{
				if (l1 != l2)
					return false;

				if constexpr (sizeof(type) == 1) {
					// using unsigned_char = typename std::make_unsigned<type>::type;
					if (l1 && *v1 != *v2)
						return false;
					const type* end = v1 + l1;
					while ((v1 < end - (8 - 1))) {
						if (read_64(v1) != read_64(v2))
							return false;
						v1 += 8;
						v2 += 8;
					}
					if ((v1 < (end - 3))) {
						if (read_32(v1) != read_32(v2))
							return false;
						v1 += 4;
						v2 += 4;
					}
					if ((v1 < (end - 1))) {
						if (read_16(v1) != read_16(v2))
							return false;
						v1 += 2;
						v2 += 2;
					}
					if (v1 != end && *v1 != *v2)
						return false;
				}
				else {
					if (l1 && *v1 != *v2)
						return false;
					for (size_t i = 1; i < l1; ++i)
						if (v1[i] != v2[i])
							return false;
				}
				return true;
			}
			template<class L, class R>
			SEQ_ALWAYS_INLINE bool equal(const L& l, const R& r) const noexcept
			{
				auto lh = RadixHasher<T>{}.hash(l);
				auto rh = RadixHasher<T>{}.hash(r);
				return equal(lh.data, lh.size, rh.data, rh.size);
			}
		};

		template<class Tuple, size_t N = std::tuple_size<Tuple>::value>
		struct TupleInfo
		{
			static constexpr size_t tuple_size = std::tuple_size<Tuple>::value;
			static constexpr size_t pos = tuple_size - N;
			using type = typename std::tuple_element<pos, Tuple>::type;
			static constexpr size_t size = sizeof(type) + TupleInfo<Tuple, N - 1>::size;
			static_assert(!RadixHasher<type>::variable_length, "tuple key only works with fixed size types");

			static SEQ_ALWAYS_INLINE void build_hash(char* dst, const Tuple& t) noexcept
			{
				// compute tiny hash
				auto s = swap_b(RadixHasher<type>{}.hash(std::get<pos>(t)).value);
				memcpy(dst, &s, sizeof(s));
				return TupleInfo<Tuple, N - 1>::build_hash(dst + sizeof(type), t);
			}
			static SEQ_ALWAYS_INLINE uint8_t build_tiny_hash(uint8_t h, const char* src) noexcept
			{
				if constexpr (sizeof(type) == 1)
					h ^= *reinterpret_cast<const uint8_t*>(src);
				else if constexpr (sizeof(type) == 2)
					h ^= static_cast<uint8_t>(*reinterpret_cast<const uint16_t*>(src) * 14313749767032793493ULL);
				else if constexpr (sizeof(type) == 4)
					h ^= static_cast<uint8_t>(*reinterpret_cast<const uint32_t*>(src) * 14313749767032793493ULL);
				else if constexpr (sizeof(type) == 8)
					h ^= static_cast<uint8_t>(*reinterpret_cast<const uint64_t*>(src) * 14313749767032793493ULL);
				return TupleInfo<Tuple, N - 1>::build_tiny_hash(h, src + sizeof(type));
			}
		};
		template<class Tuple>
		struct TupleInfo<Tuple, 0>
		{
			static constexpr size_t size = 0;
			static SEQ_ALWAYS_INLINE void build_hash(char*, const Tuple&) {}
			static SEQ_ALWAYS_INLINE uint8_t build_tiny_hash(uint8_t h, const char*) noexcept { return h; }
		};

		/// @brief Hash class for tuple
		template<class T>
		struct RadixHasher<T, typename std::enable_if<is_tuple<T>::value, void>::type>
		{
			using less_type = default_less;
			static constexpr bool variable_length = false;
			static constexpr bool prefix_search = true;
			static constexpr bool has_fast_check_prefix = false;
			static constexpr size_t size = TupleInfo<T>::size;
			static constexpr size_t max_bits = size * 8;
			static constexpr size_t bit_step = 2;
			static constexpr bool is_transparent = true;

			char data[size];

			template<class U>
			auto as() const noexcept
			{
				return RadixHasher<U>{ swap_b(*reinterpret_cast<const U*>(data)) };
			}

			RadixHasher() {}
			SEQ_ALWAYS_INLINE RadixHasher(const T& val) { TupleInfo<T>::build_hash(data, val); }

			SEQ_ALWAYS_INLINE constexpr auto get_size() const noexcept -> size_t { return max_bits; }

			SEQ_ALWAYS_INLINE auto n_bits(size_t shift, size_t count) const noexcept -> unsigned
			{
				if constexpr (size == 2)
					return as<uint16_t>().n_bits(shift, count);
				else if constexpr (size == 4)
					return as<uint32_t>().n_bits(shift, count);
				else if constexpr (size == 8)
					return as<uint64_t>().n_bits(shift, count);
				else {
					if SEQ_UNLIKELY (count == 0)
						return 0;
					size_t byte_offset = shift >> 3u;
					size_t bit_offset = (shift) & 7U;
					uint64_t hash = 0;
					if (size >= byte_offset + 8u)
						memcpy(&hash, data + byte_offset, 8u);
					else if (byte_offset < size)
						memcpy(&hash, data + byte_offset, static_cast<unsigned>(size - byte_offset));
					return static_cast<unsigned>((swap_b(hash) << bit_offset) >> (64u - count));
				}
			}

			SEQ_ALWAYS_INLINE std::uint8_t tiny_hash() const noexcept { return TupleInfo<T>::build_tiny_hash(0, data); }

			SEQ_ALWAYS_INLINE auto hash(const T& v) const noexcept { return RadixHasher<T>(v); }

			template<class U, class V>
			static SEQ_ALWAYS_INLINE bool less(const U& l, const V& r) noexcept
			{
				return l < r;
			}
			template<class U, class V>
			static SEQ_ALWAYS_INLINE bool equal(const U& l, const V& r) noexcept
			{
				return l == r;
			}
		};

#undef swap_b

		/// @brief Hasher using a hash function
		template<class T, class Hash, class Less, class Equal>
		struct RadixHasherUnordered
		  : RadixHasher<size_t>
		  , Hash
		  , Equal
		{
			using less_type = Less;
			using this_type = RadixHasherUnordered<T, Hash, Less, Equal>;
			static constexpr bool is_transparent = hash_is_transparent<Hash>::value;
			static constexpr bool prefix_search = false;

			SEQ_ALWAYS_INLINE RadixHasherUnordered(size_t val = 0, const Hash& h = Hash(), const Equal& eq = Equal()) noexcept
			  : RadixHasher<size_t>{ val }
			  , Hash(h)
			  , Equal(eq)
			{
			}

			SEQ_ALWAYS_INLINE std::uint8_t tiny_hash() const noexcept { return static_cast<std::uint8_t>(this->value >> ((sizeof(size_t) - 8) * 8)); }
			template<class U>
			SEQ_ALWAYS_INLINE auto hash(const U& k) const noexcept
			{
				return this_type(hash_value(static_cast<const Hash&>(*this), k), *this, *this);
			}
			template<class L, class R>
			static SEQ_ALWAYS_INLINE bool less(const L& l, const R& r) noexcept
			{
				return Less{}(l, r);
			}
			template<class L, class R>
			SEQ_ALWAYS_INLINE bool equal(const L& l, const R& r) const noexcept
			{
				return Equal::operator()(l, r);
			}
		};

	}

	namespace radix_detail
	{
		/// @brief Policy used when inserting new key
		struct EmplacePolicy
		{
			template<class Vector, class K, class... Args>
			static std::pair<size_t, bool> emplace_vector(Vector& v, K&& key, Args&&... args)
			{
				return v.emplace(std::forward<K>(key), std::forward<Args>(args)...);
			}
			template<class Vector, class K, class... Args>
			static unsigned emplace_vector_no_check(Vector& v, K&& key, Args&&... args)
			{
				return static_cast<unsigned>(v.emplace_no_check(std::forward<K>(key), std::forward<Args>(args)...).first);
			}
			template<class T, class K, class... Args>
			static T* emplace(T* dst, K&& key, Args&&... args)
			{
				return new (dst) T(std::forward<K>(key), std::forward<Args>(args)...);
			}
		};
		/// @brief Policy used when inserting new key using try_emplace
		struct TryEmplacePolicy
		{
			template<class Vector, class K, class... Args>
			static std::pair<size_t, bool> emplace_vector(Vector& v, K&& key, Args&&... args)
			{
				return v.emplace(
				  typename Vector::value_type(std::piecewise_construct, std::forward_as_tuple(std::forward<K>(key)), std::forward_as_tuple(std::forward<Args>(args)...)));
			}
			template<class Vector, class K, class... Args>
			static unsigned emplace_vector_no_check(Vector& v, K&& key, Args&&... args)
			{
				return static_cast<unsigned>(
				  v.emplace_no_check(
				     typename Vector::value_type(std::piecewise_construct, std::forward_as_tuple(std::forward<K>(key)), std::forward_as_tuple(std::forward<Args>(args)...)))
				    .first);
			}
			template<class T, class K, class... Args>
			static T* emplace(T* dst, K&& key, Args&&... args)
			{
				return new (dst) T(std::piecewise_construct, std::forward_as_tuple(std::forward<K>(key)), std::forward_as_tuple(std::forward<Args>(args)...));
			}
		};

		template<class ExtractKey, class Hasher, class T, class SizeType, class K>
		static inline SizeType compute_lower_bound(const T* vals, SizeType size, const K& key)
		{
			return lower_bound<false, T>(vals, size, key, [](const auto& l, const auto& r) { return Hasher::less(ExtractKey{}(l), ExtractKey{}(r)); }).first;
		}

		/// @brief Copy count elements from src to dst while destroying elements in src
		/// In case of exception, destroy all elements in in src and dst.
		template<class U>
		static inline void copy_destroy(U* dst, U* src, unsigned count)
		{
			if constexpr (is_relocatable<U>::value)
				memcpy(static_cast<void*>(dst), static_cast<const void*>(src), sizeof(U) * count);
			else {
				unsigned i = 0;
				try {
					for (; i < count; ++i) {
						new (dst + i) U(std::move(src[i]));
						src[i].~U();
					}
				}
				catch (...) {
					// destroy created elements
					for (unsigned j = 0; j < i; ++j)
						dst[j].~U();
					// finish destroying src
					for (; i < count; ++i) {
						src[i].~U();
					}
					throw;
				}
			}
		}

		/// @brief Insert element at src position while moving elements to the right at dst.
		/// Basic exception guarantee.
		template<class U, class Policy, class... Args>
		static void SEQ_ALWAYS_INLINE insert_move_right(U* dst, U* src, unsigned count, Policy, Args&&... args)
		{
			// Move src to the right
			// In case of exception, values are in undefined state, but no new value created (basic exception guarantee)

			if constexpr (is_relocatable<U>::value) {
				if (count)
					memmove(static_cast<void*>(dst), static_cast<void*>(src), count * sizeof(U));
				try {
					Policy::emplace(src, std::forward<Args>(args)...);
				}
				catch (...) {
					// move back to the left
					memmove(static_cast<void*>(src), static_cast<const void*>(dst), count * sizeof(U));
					throw;
				}
			}
			else {
				try {
					if (count) {
						new (dst + count - 1) U();
						std::move_backward(src, src + count, dst + count);
						src->~U();
						Policy::emplace(src, std::forward<Args>(args)...);
					}
					else
						Policy::emplace(src, std::forward<Args>(args)...);
				}
				catch (...) {
					// destroy created element
					if (count)
						(dst + count - 1)->~U();
					throw;
				}
			}
		}

		/// @brief Erase element at pos in src.
		/// Basic exception guarantee
		template<class U>
		static void erase_pos(U* src, unsigned pos, unsigned count)
		{
			if constexpr (is_relocatable<U>::value) {
				src[pos].~U();
				memmove(static_cast<void*>(src + pos), static_cast<const void*>(src + pos + 1), (count - pos - 1) * sizeof(U));
			}
			else {
				// might throw, in which case the count remains the same.
				std::move(src + pos + 1, src + count, src + pos);
				src[count - 1].~U();
			}
		}

		/// @brief Movemask function of 8 bytes word
		static SEQ_ALWAYS_INLINE std::uint64_t movemask8(std::uint64_t word)
		{
			std::uint64_t tmp = (word & 0x7F7F7F7F7F7F7F7FULL) + 0x7F7F7F7F7F7F7F7FULL;
			return ~(tmp | word | 0x7F7F7F7F7F7F7F7FULL);
		}
#ifdef __SSE3__
		static SEQ_ALWAYS_INLINE unsigned movemask32(const unsigned char* hashs, unsigned char th) noexcept
		{
			auto left = static_cast<unsigned short>(_mm_movemask_epi8(_mm_cmpeq_epi8(_mm_loadu_si128(reinterpret_cast<const __m128i*>(hashs)), _mm_set1_epi8(static_cast<char>(th)))));
			auto right =
			  static_cast<unsigned short>(_mm_movemask_epi8(_mm_cmpeq_epi8(_mm_loadu_si128(reinterpret_cast<const __m128i*>(hashs + 16)), _mm_set1_epi8(static_cast<char>(th)))));
			return static_cast<unsigned>(left) | (static_cast<unsigned>(right) << 16);
		}
#endif

		/// @brief Swiss table like find using AVX2, SSE3, or 8 byte movemask.
		template<bool UseLowerBound, class ExtractKey, class Equal, class Less, class T, class U>
		static SEQ_ALWAYS_INLINE unsigned find_value(const Equal& eq, const T* values, const unsigned char* ths, unsigned size, unsigned char th, unsigned* insert_pos, const U& val)
		{
			if constexpr (UseLowerBound) {
				*insert_pos = size;
				if (Less{}(ExtractKey{}(values[size - 1]), val))
					return static_cast<unsigned>(-1);
			}
#ifdef __SSE3__
			if (cpu_features().HAS_SSE3) {
				unsigned size32 = size & ~31u;
				for (unsigned i = 0; i < size32; i += 32) {
					if (unsigned found = movemask32(ths + i, th)) {
						do {
							unsigned pos = bit_scan_forward_32(found);
							if (eq(ExtractKey{}(values[i + pos]), val))
								return i + pos;
							found = found & ~(1U << pos);
						} while (found);
					}
				}
				if (unsigned rem = size - size32) {
					if (unsigned found = movemask32(ths + size32, th) & ((1U << rem) - 1U)) {
						do {
							unsigned pos = bit_scan_forward_32(found);
							if (eq(ExtractKey{}(values[size32 + pos]), val))
								return size32 + pos;
							found = found & ~(1U << pos);
						} while (found);
					}
				}
			}
			else
#endif
			{
				unsigned count = size & ~7U;
				uint64_t _th;
				memset(&_th, th, sizeof(_th));
				for (unsigned i = 0; i < count; i += 8) {
					uint64_t found = movemask8((read_64(ths + i) ^ _th));
					while (found) {
						unsigned pos = bit_scan_forward_64(found) >> 3;
						if (eq(ExtractKey{}(values[i + pos]), val))
							return i + pos;
						reinterpret_cast<unsigned char*>(&found)[pos] = 0;
					}
				}
				if (unsigned rem = size - count) {
					uint64_t found = movemask8((read_64(ths + count) ^ _th)) & ((1ULL << (rem) * 8ULL) - 1ULL);
					while (found) {
						unsigned pos = bit_scan_forward_64(found) >> 3;
						if (eq(ExtractKey{}(values[count + pos]), val))
							return count + pos;
						reinterpret_cast<unsigned char*>(&found)[pos] = 0;
					}
				}
			}

			if constexpr (UseLowerBound)
				*insert_pos = compute_lower_bound<ExtractKey, Less>(values, size, val);

			return static_cast<unsigned>(-1);
		}

		/// @brief Insertion sort algorithm based on values and maintaining hash values order
		template<class T, class ExtractKey, class Less>
		void insertion_sort(T* values, std::uint8_t* hashs, int begin, int end, Less comp)
		{
			for (int cur = begin + 1; cur != end; ++cur) {
				int sift = cur;
				int sift_1 = cur - 1;

				// Compare first so we can avoid 2 moves for an element already positioned correctly.
				if (comp(ExtractKey{}(values[sift]), ExtractKey{}(values[sift_1]))) {
					T tmp = std::move(values[sift]);
					std::uint8_t h = hashs[sift];
					do {
						values[sift] = std::move(values[sift_1]);
						hashs[sift] = hashs[sift_1];
						--sift;
					} while (sift != begin && comp(ExtractKey{}(tmp), ExtractKey{}(values[--sift_1])));

					values[sift] = std::move(tmp);
					hashs[sift] = h;
				}
			}
		}

		/// @brief Leaf node of a radix tree, can be sorted
		template<class T, bool Sorted = true, bool HasMaxCapacity = true>
		struct LeafNode
		{
			using value_type = T;

			static constexpr bool is_sorted = Sorted;
			// header size on 64 bits
			static constexpr unsigned header_size = sizeof(std::uint64_t);
			// minimum capacity, depends on sizeof(T) to allow an AVX (32 bytes) load
			static constexpr unsigned min_capacity = sizeof(T) == 1 ? 32 : sizeof(T) <= 3 ? 16 : sizeof(T) <= 8 ? 8 : sizeof(T) <= 16 ? 4 : 2;
			// maximum capacity (and size), lower for sorted elements
			static constexpr unsigned max_capacity = Sorted ? 64 : 96;

			// returns size of header and tiny hash values
			static unsigned hash_for_size(unsigned /*size*/, unsigned capacity) noexcept { return header_size + capacity; }
			// returns the capacity for a given size
			static unsigned capacity_for_size(unsigned size) noexcept
			{
				// return (size / min_capacity) * min_capacity + (size % min_capacity ? min_capacity : 0);
				if (size <= min_capacity)
					return min_capacity;
				unsigned bits = bit_scan_reverse_32(size);
				unsigned cap = 1U << bits;
				if (cap < size) {
					cap *= 2;
				}
				if (HasMaxCapacity && cap > max_capacity)
					cap = max_capacity;
				return cap;
			}

			SEQ_ALWAYS_INLINE bool full() const noexcept { return count() >= max_capacity; }
			SEQ_ALWAYS_INLINE std::uint32_t* toint32() noexcept { return reinterpret_cast<std::uint32_t*>(this); }
			SEQ_ALWAYS_INLINE const std::uint32_t* toint32() const noexcept { return reinterpret_cast<const std::uint32_t*>(this); }
			SEQ_ALWAYS_INLINE std::uint32_t* size() noexcept { return toint32(); }
			SEQ_ALWAYS_INLINE const std::uint32_t* size() const noexcept { return toint32(); }
			SEQ_ALWAYS_INLINE std::uint32_t* capacity() noexcept { return toint32() + 1; }
			SEQ_ALWAYS_INLINE const std::uint32_t* capacity() const noexcept { return toint32() + 1; }
			SEQ_ALWAYS_INLINE const std::uint8_t* hashs() const noexcept { return reinterpret_cast<const std::uint8_t*>(toint32() + 2); }
			SEQ_ALWAYS_INLINE std::uint8_t* hashs() noexcept { return reinterpret_cast<std::uint8_t*>(toint32() + 2); }
			SEQ_ALWAYS_INLINE unsigned count() const noexcept { return *size(); }
			SEQ_ALWAYS_INLINE T* values() noexcept { return reinterpret_cast<T*>(hashs() + *capacity()); }
			SEQ_ALWAYS_INLINE const T* values() const noexcept { return reinterpret_cast<const T*>(hashs() + *capacity()); }
			SEQ_ALWAYS_INLINE const T& back() const noexcept { return values()[count() - 1]; }
			SEQ_ALWAYS_INLINE std::uint8_t get_tiny_hash(unsigned pos) const { return hashs()[pos]; }

			// Returns lower bound for sorted leaf only
			template<class ExtractKey, class Less, class Equal, class K>
			SEQ_ALWAYS_INLINE unsigned lower_bound(size_t start_bit, unsigned, const K& key) const
			{
				return compute_lower_bound<ExtractKey, Less>(/* start_bit,*/ values(), count(), key);
			}
			// Check if given value already exists, and return insertion position for sorted leaf only
			template<bool EnsureSorted, class ExtractKey, class Equal, class Less, class U>
			SEQ_ALWAYS_INLINE std::pair<const T*, unsigned> find_insert(const Equal& eq, size_t /*start_bit*/, unsigned th, const U& val) const
			{
				using key_type = typename ExtractKeyResultType<ExtractKey, T>::type;
				if constexpr (Sorted && EnsureSorted && std::is_arithmetic<key_type>::value) {
					// For arithmetic keys, checking bounds is cheap and helps a lot for ordered insertions
					if (Less{}(ExtractKey{}(values()[count() - 1]), (val)))
						return std::pair<const T*, unsigned>(nullptr, count());
					if (Less{}((val), ExtractKey{}(values()[0])))
						return std::pair<const T*, unsigned>(nullptr, 0);
				}
				unsigned insert_pos = static_cast<unsigned>(-1);
				unsigned pos = find_value<(Sorted && EnsureSorted), ExtractKey, Equal, Less>(eq, values(), hashs(), count(), static_cast<std::uint8_t>(th), &insert_pos, val);
				return std::pair<const T*, unsigned>(pos == static_cast<unsigned>(-1) ? nullptr : values() + pos, insert_pos);
			}
			// Returns value index, -1 if not found
			template<class ExtractKey, class Equal, class Less, class K>
			SEQ_ALWAYS_INLINE unsigned find(const Equal& eq, size_t /*start_bit*/, std::uint8_t th, const K& key) const
			{
				return find_value<false, ExtractKey, Equal, Less>(eq, values(), hashs(), count(), th, nullptr, key);
			}
			template<class ExtractKey, class Equal, class K>
			SEQ_ALWAYS_INLINE unsigned find(const Equal& eq, std::uint8_t th, const K& key) const
			{
				return find_value<false, ExtractKey, Equal, default_less>(eq, values(), hashs(), count(), th, nullptr, key);
			}

			// Sort leaf
			template<class ExtractKey, class Less>
			void sort(Less le)
			{
				insertion_sort<T, ExtractKey>(values(), hashs(), 0, static_cast<int>(count()), le);
			}

			// Reallocate leaf on insertion
			template<class NodeAllocator, class Policy, class... Args>
			std::pair<LeafNode*, unsigned> switch_buffer(NodeAllocator& al, unsigned old_size, unsigned pos, std::uint8_t th, Policy, Args&&... args)
			{
				unsigned new_capacity = capacity_for_size(old_size + 1);
				// might throw, fine
				LeafNode* n = al.allocate(hash_for_size(old_size + 1, new_capacity), new_capacity);
				*n->size() = this->count();
				*n->capacity() = new_capacity;
				try {
					// might throw
					Policy::emplace((n->values() + pos), std::forward<Args>(args)...);
					n->hashs()[pos] = th;
					try {
						if (Sorted && old_size != pos) {
							copy_destroy(n->hashs(), this->hashs(), pos);
							copy_destroy(n->hashs() + pos + 1, this->hashs() + pos, (old_size - pos));

							// both calls might throw
							copy_destroy(n->values(), this->values(), pos);
							copy_destroy(n->values() + pos + 1, this->values() + pos, (old_size - pos));
						}
						else {
							copy_destroy(n->hashs(), this->hashs(), old_size);
							// both calls might throw
							copy_destroy(n->values(), this->values(), old_size);
						}
					}
					catch (...) {
						n->values()[pos].~T();
						throw;
					}
				}
				catch (...) {
					al.deallocate(n, hash_for_size(old_size + 1, new_capacity), new_capacity);
					throw;
				}

				al.deallocate(this, hash_for_size(old_size, old_size), old_size);

				return std::pair<LeafNode*, unsigned>(n, pos);
			}
			// Insert new value. Does NOT check for already existing element.
			template<class ExtractKey, class Less, class NodeAllocator, class Policy, class K, class... Args>
			SEQ_ALWAYS_INLINE std::pair<LeafNode*, unsigned> insert(NodeAllocator& al, size_t start_bit, unsigned pos, std::uint8_t th, Policy p, K&& key, Args&&... args)
			{
				const unsigned size = count();

				if constexpr (Sorted) {
					if (pos > size)
						pos = compute_lower_bound<ExtractKey, Less>(/* start_bit,*/ values(), count(), key);
				}
				else
					pos = size;

				if SEQ_UNLIKELY (*capacity() == size) {
					// might throw, fine
					auto res = switch_buffer(al, size, pos, th, p, std::forward<K>(key), std::forward<Args>(args)...);
					(*res.first->size())++;
					return res;
				}

				if (Sorted && pos != size) {
					insert_move_right(hashs() + pos + 1, hashs() + pos, (size - pos), EmplacePolicy{}, th);
					insert_move_right(values() + pos + 1, values() + pos, (size - pos), p, std::forward<K>(key), std::forward<Args>(args)...);
				}
				else {
					Policy::emplace((values() + size), std::forward<K>(key), std::forward<Args>(args)...);
					hashs()[size] = th;
				}

				(*this->size())++;
				return std::pair<LeafNode*, unsigned>(this, pos);
			}

			template<class NodeAllocator>
			LeafNode* erase(NodeAllocator& al, unsigned pos)
			{
				erase_pos(values(), pos, *size());
				erase_pos(hashs(), pos, *size());
				(*size())--;

				if (*size() == 0) {
					unsigned cap = capacity_for_size(1);
					al.deallocate(this, hash_for_size(1, cap), cap);
					return nullptr;
				}

				unsigned cap = capacity_for_size(*size());
				if (cap != *capacity()) {
					// might throw, fine
					LeafNode* n = al.allocate(hash_for_size(*size(), cap), cap);
					*n->size() = *this->size();
					*n->capacity() = cap;

					try {
						copy_destroy(n->values(), values(), *size());
						copy_destroy(n->hashs(), this->hashs(), *size());
					}
					catch (...) {
						al.deallocate(n, hash_for_size(*size(), cap), cap);
						throw;
					}

					al.deallocate(this, hash_for_size(*size() + 1, *capacity()), *capacity());

					return n;
				}
				return this;
			}

			// Create node with one value
			template<class NodeAllocator, class Policy, class... Args>
			static std::pair<LeafNode*, T*> make(NodeAllocator& alloc, unsigned th, Policy, Args&&... args)
			{
				const unsigned capacity = capacity_for_size(1);
				const unsigned hash_count = hash_for_size(1, capacity);
				LeafNode* tmp = alloc.allocate(hash_count, capacity);
				*tmp->size() = 1;
				*tmp->capacity() = capacity;
				T* p = nullptr;
				try {
					p = Policy::emplace(tmp->values(), std::forward<Args>(args)...);
					tmp->hashs()[0] = static_cast<std::uint8_t>(th);
				}
				catch (...) {
					alloc.deallocate(tmp, hash_count, capacity_for_size(1));
					throw;
				}

				return std::pair<LeafNode*, T*>(tmp, p);
			}

			// Destroy node
			template<class NodeAllocator>
			static void destroy(NodeAllocator& alloc, LeafNode* node)
			{
				unsigned size = node->count();

				// destroy values
				if constexpr (!std::is_trivially_destructible<T>::value) {
					T* values = node->values();
					for (unsigned i = 0; i < size; ++i)
						values[i].~T();
				}

				// deallocate
				unsigned cap = *node->capacity();
				alloc.deallocate(node, hash_for_size(size, cap), cap);
			}
		};

		/// @brief Directory child, uses a tagged_pointer to tell if the child is a directory, a leaf or a vector.
		template<class Dir, class Node, class Vector>
		struct ChildPtr : public tagged_pointer<void, CustomAlignment, 8>
		{
			using base = tagged_pointer<void, CustomAlignment, 8>;
			SEQ_ALWAYS_INLINE ChildPtr()
			  : base()
			{
			}
			SEQ_ALWAYS_INLINE ChildPtr(void* ptr) noexcept
			  : base(ptr)
			{
			}
			SEQ_ALWAYS_INLINE ChildPtr(void* ptr, std::uintptr_t t) noexcept
			  : base(ptr, t)
			{
			}
			SEQ_ALWAYS_INLINE const Dir* to_dir() const noexcept { return static_cast<const Dir*>(this->ptr()); }
			SEQ_ALWAYS_INLINE const Node* to_node() const noexcept { return static_cast<const Node*>(this->ptr()); }
			SEQ_ALWAYS_INLINE const Vector* to_vector() const noexcept { return static_cast<const Vector*>(this->ptr()); }
			SEQ_ALWAYS_INLINE Dir* to_dir() noexcept { return static_cast<Dir*>(this->ptr()); }
			SEQ_ALWAYS_INLINE Node* to_node() noexcept { return static_cast<Node*>(this->ptr()); }
			SEQ_ALWAYS_INLINE Vector* to_vector() noexcept { return static_cast<Vector*>(this->ptr()); }
			SEQ_ALWAYS_INLINE unsigned size() const { return this->tag() == Dir::IsLeaf ? to_node()->count() : to_vector()->size(); }
			SEQ_ALWAYS_INLINE const typename Node::value_type& front() const { return this->tag() == Dir::IsLeaf ? *to_node()->values() : to_vector()->front(); }
		};

		/// @brief Directory within a radix tree.
		/// A directory stores a power of 2 number of children that could be either directories, leaves or vectors.
		template<class T, class Node, class Vector, class Hasher>
		struct Directory
		{
			using value_type = T;
			using node = Node;
			using child_ptr = ChildPtr<Directory, Node, Vector>;
			using vector_type = Vector;
			template<class Allocator, class U>
			using RebindAlloc = typename std::allocator_traits<Allocator>::template rebind_alloc<U>;

			static constexpr std::uint8_t IsNull = 0;
			static constexpr std::uint8_t IsDir = 1;
			static constexpr std::uint8_t IsLeaf = 2;
			static constexpr std::uint8_t IsVector = 3;
			static constexpr unsigned alloc_size = sizeof(uint64_t);
			static constexpr uint64_t invalid = static_cast<uint64_t>(-1);

			unsigned hash_len = 0;		      // hash len in bits, size = 1 << hash_len
			unsigned dir_count = 0;		      // number of directories inside
			unsigned child_count = 0;	      // total number of children
			unsigned parent_pos = 0;	      // position within parent directory
			uint64_t first_valid_child = invalid; // position of the first valid child, if possible a leaf node.
			uint64_t prefix_len = 0;	      // prefix len in bits.
			Directory* parent = nullptr;	      // parent directory (null for root leaf)

			/// @brief Returns a child of this directory.
			/// This is used to get the prefix bits of this directory.
			const T& any_child() const noexcept
			{
				if (first_valid_child == invalid)
					const_cast<Directory*>(this)->compute_first_valid();
				SEQ_ASSERT_DEBUG(first_valid_child < size(), "");
				child_ptr ch = const_child((unsigned)first_valid_child);
				while (ch.tag() == IsDir) {
					auto dir = ch.to_dir();
					if (dir->first_valid_child == invalid)
						dir->compute_first_valid();
					ch = dir->const_child((unsigned)dir->first_valid_child);
				}

				if (ch.tag() == IsLeaf)
					return ch.to_node()->back();
				return ch.to_vector()->back();
			}
			/// @brief Recompute the first valid position within the directory, and if possible a leaf node
			void compute_first_valid() noexcept
			{
				first_valid_child = invalid;
				for (unsigned i = 0; i < size(); ++i) {
					if (const_child(i).tag() == IsLeaf) {
						first_valid_child = i;
						break;
					}
					else if (const_child(i).tag() && first_valid_child == invalid)
						first_valid_child = i;
				}
			}
			SEQ_ALWAYS_INLINE void invalidate_first_valid() noexcept { first_valid_child = invalid; }

			template<class Fun>
			void for_each_leaf(Fun f)
			{
				for (unsigned i = 0; i < size(); ++i) {
					auto tag = child(i).tag();
					if (tag == IsDir) {
						child(i).to_dir()->for_each_leaf(f);
						continue;
					}
					if (tag != 0) {
						f(this, i);
					}
				}
			}

			/// @brief Returns the full size of the directory
			SEQ_ALWAYS_INLINE unsigned size() const noexcept { return (1U << hash_len); }
			/// @brief Returns the children pointer
			SEQ_ALWAYS_INLINE child_ptr* children() noexcept { return (reinterpret_cast<child_ptr*>(this + 1)); }
			SEQ_ALWAYS_INLINE const child_ptr* children() const noexcept { return (reinterpret_cast<const child_ptr*>(this + 1)); }
			/// @brief Returns child at given position
			SEQ_ALWAYS_INLINE child_ptr& child(unsigned pos) noexcept { return children()[pos]; }
			SEQ_ALWAYS_INLINE child_ptr const_child(unsigned pos) const noexcept { return children()[pos]; }
			/// @brief Allocate, initialize and return a directory with given bit length (log2(size))
			template<class NodeAllocator>
			static Directory* make(NodeAllocator& alloc, unsigned hash_len)
			{
				Directory* dir = alloc.allocate_dir(hash_len);
				dir->hash_len = hash_len;
				dir->first_valid_child = Directory::invalid;
				return dir;
			}
			/// @brief Destroy and deallocate directory recursively
			template<class NodeAllocator>
			static void destroy(NodeAllocator& alloc, Directory* dir, bool recurse = true)
			{
				// recursively destroy and deallocate
				if (recurse) {
					unsigned size = dir->size();
					for (unsigned i = 0; i < size; ++i) {
						child_ptr child = dir->const_child(i);
						if (child.full()) {
							if (child.tag() == IsDir)
								destroy(alloc, static_cast<Directory*>(child.ptr()));
							else if (child.tag() == IsLeaf)
								node::destroy(alloc, static_cast<node*>(child.ptr()));
							else if (child.tag() == IsVector)
								alloc.destroy_vector(static_cast<Vector*>(child.ptr()));
						}
					}
				}

				alloc.deallocate_dir(dir);
			}
		};

		/// @brief Class handling allocation/deallocation for leaves and directories
		template<class Allocator, class T, class Directory, class Node>
		class NodeAllocator : private Allocator
		{
			template<class Al, class U>
			using RebindAlloc = typename std::allocator_traits<Al>::template rebind_alloc<U>;
			using node = Node;
			using directory = Directory;
			using child_ptr = typename directory::child_ptr;

			// Allocate by quantum of 8 bytes
			static constexpr size_t alloc_size = sizeof(std::uint64_t);

		public:
			using allocator_type = Allocator;
			using vector_type = typename directory::vector_type;

			size_t size;	 // tree size
			directory* root; // root directory

			NodeAllocator(const Allocator& al)
			  : Allocator(al)
			  , size(0)
			  , root(nullptr)
			{
			}
			~NodeAllocator() {}

			Allocator& get_allocator() noexcept { return *this; }
			const Allocator& get_allocator() const noexcept { return *this; }

			/// @brief Allocate and construct a vector
			template<class Hash>
			vector_type* make_vector(const Hash& h)
			{
				RebindAlloc<Allocator, vector_type> al = get_allocator();
				vector_type* res = al.allocate(1);
				try {
					construct_ptr(res, get_allocator(), h);
				}
				catch (...) {
					al.deallocate(res, 1);
					throw;
				}
				return res;
			}
			/// @brief Destroy and deallocate a vector
			void destroy_vector(vector_type* val)
			{
				RebindAlloc<Allocator, vector_type> al = get_allocator();
				destroy_ptr(val);
				al.deallocate(val, 1);
			}

			/// @brief Allocate a leaf node for given capacity
			node* allocate(unsigned hash_size, unsigned capacity)
			{

				RebindAlloc<Allocator, std::uint64_t> al = get_allocator();
				size_t bytes = hash_size + sizeof(T) * capacity;
				size_t to_alloc = bytes / alloc_size + (bytes % alloc_size ? 1 : 0);

#ifndef SEQ_DEBUG
				return reinterpret_cast<node*>(al.allocate(to_alloc));
#else
				std::uint64_t* tmp = al.allocate(to_alloc + 1);
				*tmp = to_alloc;
				return reinterpret_cast<node*>(tmp + 1);
#endif
			}
			/// @brief Deallocate a leaf node with given capacity
			void deallocate(node* node, unsigned hash_size, unsigned capacity)
			{

				RebindAlloc<Allocator, std::uint64_t> al = get_allocator();
				size_t bytes = hash_size + sizeof(T) * capacity;
				size_t to_dealloc = bytes / alloc_size + (bytes % alloc_size ? 1 : 0);

#ifndef SEQ_DEBUG
				al.deallocate(reinterpret_cast<std::uint64_t*>(node), to_dealloc);
#else
				std::uint64_t* tmp = reinterpret_cast<std::uint64_t*>(node) - 1;
				SEQ_ASSERT_DEBUG(*tmp == to_dealloc, "mismatch between allocation and deallocation size");
				if (*tmp != to_dealloc)
					throw std::runtime_error("");
				al.deallocate(tmp, to_dealloc + 1);
#endif
			}

			/// @brief Allocate a directory for given bit length
			directory* allocate_dir(size_t hash_len)
			{

				RebindAlloc<Allocator, std::uint64_t> al = get_allocator();
				size_t dir_size = 1ULL << hash_len;
				size_t bytes = sizeof(directory) + sizeof(child_ptr) * dir_size;
				size_t to_alloc = bytes / alloc_size + (bytes % alloc_size ? 1 : 0);

				directory* dir = reinterpret_cast<directory*>(al.allocate(to_alloc));
				memset(static_cast<void*>(dir), 0, to_alloc * sizeof(std::uint64_t));

				//++dir_count;
				return dir;
			}
			/// @brief Deallocate directory
			void deallocate_dir(directory* dir)
			{
				RebindAlloc<Allocator, std::uint64_t> al = get_allocator();
				size_t bytes = sizeof(directory) + sizeof(child_ptr) * dir->size();
				size_t to_dealloc = bytes / alloc_size + (bytes % alloc_size ? 1 : 0);
				//--dir_count;
				al.deallocate(reinterpret_cast<std::uint64_t*>(dir), to_dealloc);
			}
		};

		/// @brief Iterator class for radix trees
		template<class T, class Data, class Dir, class VectorType>
		class RadixConstIter
		{
		public:
			using child_ptr = typename Dir::child_ptr;
			using node = typename Dir::node;

			Data* data;
			Dir* dir;
			size_t bit_pos;	   // bit position of the directory
			unsigned child;	   // node position in directory
			unsigned node_pos; // position within node

			SEQ_ALWAYS_INLINE const node* to_node() const noexcept { return static_cast<const node*>(dir->const_child(child).ptr()); }
			SEQ_ALWAYS_INLINE const VectorType* to_vector() const noexcept { return static_cast<const VectorType*>(dir->const_child(child).ptr()); }

			/// @brief Returns the bit position of given directory
			static size_t get_bit_pos(const Dir* dir) noexcept
			{
				size_t bit_pos = dir->prefix_len;
				while (dir->parent) {
					bit_pos += dir->parent->hash_len + dir->parent->prefix_len;
					dir = dir->parent;
				}
				return bit_pos;
			}

		public:
			using iterator_category = std::bidirectional_iterator_tag;
			using value_type = T;
			using difference_type = std::ptrdiff_t;
			using const_pointer = const value_type*;
			using const_reference = const value_type&;
			using pointer = value_type*;
			using reference = value_type&;

			struct PosInDir
			{
				Dir* dir;
				unsigned child;
				size_t bit_pos;
			};

			static PosInDir find_next(const Dir* current, unsigned current_pos, size_t bit_pos) noexcept
			{
				// go right on the directory
				size_t dir_size = current->size();
				for (; current_pos != dir_size; ++current_pos) {
					if (current->const_child(current_pos))
						break;
				}
				child_ptr found = current_pos != dir_size ? current->const_child(current_pos) : child_ptr();

				// found a valid non dir node: return
				if (found && found.tag() != Dir::IsDir)
					return PosInDir{ const_cast<Dir*>(current), current_pos, bit_pos };

				if (found.tag() == Dir::IsDir) {

					// we found a directory: investigate it
					auto tmp = find_next(found.to_dir(), 0, bit_pos + current->hash_len + found.to_dir()->prefix_len);
					if (tmp.dir)
						return tmp;
				}

				// we reach the end of this directory: try in parent
				if (current->parent && current)
					return find_next(current->parent, current->parent_pos + 1, bit_pos - current->prefix_len - current->parent->hash_len);

				return { nullptr, 0, 0 }; // end of iteration
			}

			static PosInDir find_prev(const Dir* current, unsigned current_pos, size_t bit_pos) noexcept
			{
				// go left on the directory
				size_t dir_size = current->size();
				if (current_pos == dir_size)
					--current_pos;
				for (; current_pos != static_cast<unsigned>(-1); --current_pos) {
					if (current->const_child(current_pos))
						break;
				}
				child_ptr found = current_pos != static_cast<unsigned>(-1) ? current->const_child(current_pos) : child_ptr();

				// found a valid non dir node: return
				if (found && found.tag() != Dir::IsDir)
					return { const_cast<Dir*>(current), current_pos, bit_pos };

				if (found.tag() == Dir::IsDir) {

					// we found a directory: investigate it
					auto tmp = find_prev(found.to_dir(), found.to_dir()->size(), bit_pos + current->hash_len + found.to_dir()->prefix_len);
					if (tmp.dir)
						return tmp; // we found a node in this directory
				}

				// we reach the end of this directory: try in parent
				if (current->parent)
					return find_prev(current->parent, current->parent_pos - 1, bit_pos - current->prefix_len - current->parent->hash_len);

				return { nullptr, 0, 0 }; // end of iteration
			}

			auto next() noexcept -> RadixConstIter&
			{
				auto tmp = find_next(dir, child + 1, bit_pos);
				if SEQ_UNLIKELY (!tmp.dir) {
					dir = nullptr;
					child = 0;
					node_pos = 0;
					return *this; // end of iteration
				}
				SEQ_ASSERT_DEBUG(!node::is_sorted || tmp.bit_pos == get_bit_pos(tmp.dir), "");
				// we found a valid node
				dir = tmp.dir;
				child = tmp.child;
				node_pos = 0;
				bit_pos = tmp.bit_pos;

				return *this;
			}
			auto prev() noexcept -> RadixConstIter&
			{
				auto tmp = find_prev(dir, child - 1, bit_pos);
				// It is not allowed to decrement begin iterator
				SEQ_ASSERT_DEBUG(tmp.dir, "");
				SEQ_ASSERT_DEBUG(!node::is_sorted || tmp.bit_pos == get_bit_pos(tmp.dir), "");

				// we found a valid node
				dir = tmp.dir;
				child = tmp.child;
				child_ptr c = dir->children()[child];
				node_pos = c.tag() == Dir::IsLeaf ? c.to_node()->count() - 1 : static_cast<unsigned>(c.to_vector()->size()) - 1;
				bit_pos = tmp.bit_pos;

				return *this;
			}

			SEQ_ALWAYS_INLINE RadixConstIter(const Data* dt, const Dir* d, unsigned c, unsigned np, size_t bp) noexcept
			  : data(const_cast<Data*>(dt))
			  , dir(const_cast<Dir*>(d))
			  , bit_pos(bp)
			  , child(c)
			  , node_pos(np)
			{
			}
			SEQ_ALWAYS_INLINE RadixConstIter(const Data* dt = nullptr) noexcept // end
			  : data(const_cast<Data*>(dt))
			  , dir(nullptr)
			  , bit_pos(0)
			  , child(0)
			  , node_pos(0)
			{
			}
			RadixConstIter(const RadixConstIter&) noexcept = default;
			RadixConstIter& operator=(const RadixConstIter&) noexcept = default;

			bool is_vector() const noexcept { return dir->const_child(child).tag() == Dir::IsVector; }

			SEQ_ALWAYS_INLINE auto operator*() const noexcept -> reference
			{
				SEQ_ASSERT_DEBUG(dir, "dereferencing null iterator");
				return const_cast<value_type&>(dir->const_child(child).tag() == Dir::IsVector ? to_vector()->at(node_pos) : to_node()->values()[node_pos]);
			}
			SEQ_ALWAYS_INLINE auto operator->() const noexcept -> pointer { return std::pointer_traits<pointer>::pointer_to(**this); }
			SEQ_ALWAYS_INLINE auto operator++() noexcept -> RadixConstIter&
			{
				SEQ_ASSERT_DEBUG(data, "");
				++node_pos;
				if (node_pos != (dir->const_child(child).tag() == Dir::IsVector ? to_vector()->size() : to_node()->count()))
					return *this;

				return next();
			}
			SEQ_ALWAYS_INLINE auto operator++(int) noexcept -> RadixConstIter
			{
				RadixConstIter _Tmp = *this;
				++(*this);
				return _Tmp;
			}
			SEQ_ALWAYS_INLINE auto operator--() noexcept -> RadixConstIter&
			{
				SEQ_ASSERT_DEBUG(data, "");
				if (!dir) {
					// end iterator: got to last element
					auto tmp = find_prev(data->base.root, data->base.root->size(), 0);
					dir = tmp.dir;
					child = tmp.child;
					node_pos = dir->const_child(child).tag() == Dir::IsVector ? static_cast<unsigned>(to_vector()->size()) - 1 : to_node()->count() - 1;
					bit_pos = tmp.bit_pos;
					return *this;
				}
				--node_pos;
				if (node_pos == static_cast<unsigned>(-1))
					return prev();
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator--(int) noexcept -> RadixConstIter
			{
				RadixConstIter _Tmp = *this;
				--(*this);
				return _Tmp;
			}
			SEQ_ALWAYS_INLINE bool operator==(const RadixConstIter& other) const noexcept
			{
				SEQ_ASSERT_DEBUG(data == other.data || !data || !other.data, "comparing iterators from different radix trees");
				return dir == other.dir && child == other.child && node_pos == other.node_pos;
			}
			SEQ_ALWAYS_INLINE bool operator!=(const RadixConstIter& other) const noexcept
			{
				SEQ_ASSERT_DEBUG(data == other.data || !data || !other.data, "comparing iterators from different radix trees");
				return dir != other.dir || child != other.child || node_pos != other.node_pos;
			}
		};

		inline void check_vector_size(size_t size)
		{
			// For vector nodes, the size is limited to (unsigned)-1 since the highest values is reserved for the radix iterator
			if (size == std::numeric_limits<unsigned>::max() - 1)
				throw std::out_of_range("Vector node size is limited to 32 bits");
		}

		/// @brief Less functor used by vector nodes
		template<class Hasher, class ExtractKey>
		struct VectorLess
		{
			using is_transparent = int;
			template<class U, class V>
			bool operator()(const U& a, const V& b) const noexcept(noexcept(Hasher::less(ExtractKey{}(a), ExtractKey{}(b))))
			{
				return Hasher::less(ExtractKey{}(a), ExtractKey{}(b));
			}
			template<class U, class V>
			static bool less(const U& a, const V& b) noexcept(noexcept(Hasher::less(ExtractKey{}(a), ExtractKey{}(b))))
			{
				return Hasher::less(ExtractKey{}(a), ExtractKey{}(b));
			}
		};

		/// @brief Equal functor used by vector nodes
		template<class Hasher, class ExtractKey>
		struct VectorEqual : Hasher
		{
			using is_transparent = int;
			VectorEqual(const Hasher& h = Hasher())
			  : Hasher(h)
			{
			}
			template<class U, class V>
			bool operator()(const U& a, const V& b) const noexcept(noexcept(Hasher::equal(ExtractKey{}(a), ExtractKey{}(b))))
			{
				return this->Hasher::equal(ExtractKey{}(a), ExtractKey{}(b));
			}
		};

		/// @brief Sorted vector node based on seq::flat_set
		template<class T, class Hasher, class ExtractKey, class Allocator, bool sorted = true>
		struct VectorNode
		{
			using key_type = typename ExtractKeyResultType<ExtractKey, T>::type;
			using value_type = T;
			using Less = VectorLess<Hasher, ExtractKey>;

			flat_set<T, Less, Allocator> set;

			VectorNode(const Allocator& al, const Hasher&)
			  : set(al)
			{
			}

			SEQ_ALWAYS_INLINE size_t size() const noexcept { return set.size(); }
			SEQ_ALWAYS_INLINE const T& front() const noexcept { return set.pos(0); }
			SEQ_ALWAYS_INLINE const T& back() const noexcept { return set.pos(set.size() - 1); }
			template<class... Args>
			SEQ_ALWAYS_INLINE std::pair<size_t, bool> emplace(Args&&... args)
			{
				check_vector_size(size());
				return set.emplace_pos(std::forward<Args>(args)...);
			}
			template<class... Args>
			SEQ_ALWAYS_INLINE std::pair<size_t, bool> emplace_no_check(Args&&... args)
			{
				check_vector_size(size());
				return set.emplace_pos(std::forward<Args>(args)...);
			}
			SEQ_ALWAYS_INLINE void erase(size_t pos) { set.erase_pos(pos); }
			template<class K>
			SEQ_ALWAYS_INLINE size_t find(const K& key) const
			{
				return set.find_pos(key);
			}
			template<class K>
			SEQ_ALWAYS_INLINE size_t lower_bound(const K& key) const
			{
				return set.lower_bound_pos(key);
			}
			template<class K>
			SEQ_ALWAYS_INLINE size_t upper_bound(const K& key) const
			{
				return set.upper_bound_pos(key);
			}
			SEQ_ALWAYS_INLINE const T& at(size_t pos) const { return set.pos(pos); }
		};

		/// @brief Unsorted vector node based on seq::dvector
		template<class T, class Hasher, class ExtractKey, class Allocator>
		struct VectorNode<T, Hasher, ExtractKey, Allocator, false>
		{
			using key_type = typename ExtractKeyResultType<ExtractKey, T>::type;
			using value_type = T;
			using Equal = VectorEqual<Hasher, ExtractKey>;

			seq::devector<T, Allocator> vector;
			Equal equal;

			VectorNode(const Allocator& al, const Hasher& h)
			  : vector(al)
			  , equal(h)
			{
			}

			SEQ_ALWAYS_INLINE size_t size() const noexcept { return vector.size(); }
			SEQ_ALWAYS_INLINE const T& front() const noexcept { return vector.front(); }
			SEQ_ALWAYS_INLINE const T& back() const noexcept { return vector.back(); }
			template<class K, class... Args>
			SEQ_ALWAYS_INLINE std::pair<size_t, bool> emplace(K&& key, Args&&... args)
			{
				check_vector_size(size());
				size_t found = find(key);
				if (found == size()) {
					vector.emplace_back(std::forward<K>(key), std::forward<Args>(args)...);
					return std::pair<size_t, bool>(vector.size() - 1, true);
				}
				return std::pair<size_t, bool>(found, false);
			}
			template<class... Args>
			SEQ_ALWAYS_INLINE std::pair<size_t, bool> emplace_no_check(Args&&... args)
			{
				check_vector_size(size());
				vector.emplace_back(std::forward<Args>(args)...);
				return std::pair<size_t, bool>(vector.size() - 1, true);
			}
			SEQ_ALWAYS_INLINE void erase(size_t pos) { vector.erase(vector.begin() + pos); }
			template<class K>
			SEQ_ALWAYS_INLINE size_t find(const K& key) const
			{
				size_t count = vector.size();
				for (size_t i = 0; i < count; ++i)
					if (equal(vector[i], key))
						return i;
				return vector.size();
			}
			template<class... Args>
			SEQ_ALWAYS_INLINE size_t lower_bound(Args&&... args) const noexcept
			{
				return vector.size();
			}
			template<class... Args>
			SEQ_ALWAYS_INLINE size_t upper_bound(Args&&... args) const noexcept
			{
				return vector.size();
			}
			SEQ_ALWAYS_INLINE const T& at(size_t pos) const noexcept { return vector[pos]; }
		};

		/// @brief Root of a radix tree
		template<class Allocator, class T, class Directory, class Node>
		struct RootTree : public NodeAllocator<Allocator, T, Directory, Node>
		{
			using base_type = NodeAllocator<Allocator, T, Directory, Node>;
			using directory = Directory;
			using node = Node;

			RootTree(const Allocator& al)
			  : base_type(al)
			{
			}
		};

		/// @brief Radix tree container using Variable Arity Radix Tree
		/// @tparam T value type
		/// @tparam Allocator allocator type
		/// @tparam NodeType leaf node type
		/// @tparam Hash Hasher type, either Hasher or SortedHasher
		/// @tparam Extract extract key from value type
		template<class T, class Hash, class ExtractKey = default_key, class Allocator = std::allocator<T>, class NodeType = LeafNode<T>, unsigned MaxDepth = 16>
		struct RadixTree
		  : public Hash
		  , public Allocator
		{
			static constexpr unsigned start_arity = Hash::bit_step;
			static constexpr bool prefix_search = Hash::prefix_search;
			static constexpr bool variable_length = Hash::variable_length;
			static constexpr bool is_sorted = NodeType::is_sorted;
			static constexpr size_t minus_one_size_t = static_cast<size_t>(-1);

			using less_type = typename Hash::less_type;
			using node = NodeType;
			using key_type = typename ExtractKeyResultType<ExtractKey, T>::type;
			using vector_type = VectorNode<T, Hash, ExtractKey, Allocator, (is_sorted || !std::is_same_v<less_type, default_less>)>;
			using directory = Directory<T, NodeType, vector_type, Hash>;
			using child_ptr = typename directory::child_ptr;
			using root_type = RootTree<Allocator, T, directory, NodeType>;
			using this_type = RadixTree<T, Hash, ExtractKey, Allocator, NodeType, MaxDepth>;

			template<class U>
			using RebindAlloc = typename std::allocator_traits<Allocator>::template rebind_alloc<U>;

			/// @brief Less structure operating on keys
			using Less = VectorLess<Hash, ExtractKey>;
			/// @brief Equal structure operating on keys
			using Equal = VectorEqual<Hash, ExtractKey>;

			/// @brief Internal data
			struct PrivateData
			{
				root_type base;
				PrivateData(const Allocator& al, unsigned start_len = start_arity)
				  : base(al)
				{
					base.root = directory::make(base, start_len);
				}
				~PrivateData() { directory::destroy(base, base.root); }
			};

			/// @brief Returns an empty directory used to initialize a radix tree
			static directory* get_null_dir() noexcept
			{
				struct null_dir
				{
					directory dir;
					typename directory::child_ptr child;
				};
				static null_dir inst;
				return &inst.dir;
			}

			/// @brief Destroy internal data and reset root
			void destroy_data()
			{
				if (!d_data)
					return;
				RebindAlloc<PrivateData> al = get_allocator();
				destroy_ptr(d_data);
				al.deallocate(d_data, 1);
				d_data = nullptr;
				d_root = get_null_dir();
			}

			/// @brief Allocate and construct internal data with given arity
			void make_data(unsigned start_len = start_arity)
			{
				if (d_data)
					return;
				RebindAlloc<PrivateData> al = get_allocator();
				PrivateData* d = al.allocate(1);
				try {
					construct_ptr(d, get_allocator(), start_len);
				}
				catch (...) {
					al.deallocate(d, 1);
					throw;
				}
				d_data = d;
				d_root = d->base.root;
			}

		public:
			using value_type = T;
			using hash_type = Hash;
			using const_hash_ref = const Hash&;
			using extract_key_type = ExtractKey;
			using const_iterator = RadixConstIter<T, PrivateData, directory, vector_type>;
			using iterator = const_iterator;

			PrivateData* d_data;
			directory* d_root;

			// Constructors

			RadixTree(const Allocator& al = Allocator())
			  : Allocator(al)
			  , d_data(nullptr)
			  , d_root(get_null_dir())
			{
			}

			RadixTree(const Hash& h, const Allocator& al = Allocator())
			  : Hash(h)
			  , Allocator(al)
			  , d_data(nullptr)
			  , d_root(get_null_dir())
			{
			}

			RadixTree(const RadixTree& other)
			  : Hash(other)
			  , Allocator(copy_allocator(other.get_allocator()))
			  , d_data(nullptr)
			  , d_root(get_null_dir())
			{
				if (other.size())
					insert(other.begin(), other.end(), false);
			}
			RadixTree(const RadixTree& other, const Allocator& al)
			  : Hash(other)
			  , Allocator(al)
			  , d_data(nullptr)
			  , d_root(get_null_dir())
			{
				if (other.size())
					insert(other.begin(), other.end(), false);
			}

			RadixTree(RadixTree&& other) noexcept(std::is_nothrow_move_constructible<Allocator>::value && std::is_nothrow_copy_constructible<Hash>::value)
			  : Hash(other)
			  , Allocator(std::move(other.get_allocator()))
			  , d_data(nullptr)
			  , d_root(get_null_dir())
			{
				swap(other, false);
			}
			RadixTree(RadixTree&& other, const Allocator& alloc)
			  : Hash(other)
			  , Allocator(alloc)
			  , d_data(nullptr)
			  , d_root(get_null_dir())
			{
				if (alloc == other.get_allocator())
					swap(other, false);
				else if (other.size())
					insert(make_move_iterator(other.begin()), make_move_iterator(other.end()), false);
			}

			template<class Iter>
			RadixTree(Iter first, Iter last, const Allocator& alloc = Allocator())
			  : Allocator(alloc)
			  , d_data(nullptr)
			  , d_root(get_null_dir())
			{
				insert(first, last);
			}

			/// Destructor
			~RadixTree() { clear(); }

			// Assignment operators/functions

			auto operator=(RadixTree&& other) noexcept(noexcept(std::declval<RadixTree&>().swap(std::declval<RadixTree&>()))) -> RadixTree&
			{
				swap(other);
				return *this;
			}

			auto operator=(const RadixTree& other) -> RadixTree&
			{
				if (std::addressof(other) != this) {
					clear();
					assign_allocator<Allocator>(*this, other);
					insert(other.begin(), other.end(), false);
				}
				return *this;
			}

			auto operator=(const std::initializer_list<T>& init) -> RadixTree&
			{
				clear();
				insert(init.begin(), init.end());
				return *this;
			}

			template<class Iter>
			void assign(Iter start, Iter end)
			{
				clear();
				insert(start, end);
			}

			/// @brief Equivalent to std::set::merge()
			void merge(RadixTree& other)
			{
				SEQ_ASSERT_DEBUG(get_allocator() == other.get_allocator(), "");

				auto it = other.begin();
				while (it != other.end()) {
					if (this->insert(std::move(const_cast<value_type&>(*it))).second)
						it = other.erase(it);
					else
						++it;
				}
			}

			// Iterators

			SEQ_ALWAYS_INLINE const_iterator begin() const noexcept
			{
				if (size() == 0)
					return end();
				// Find the first valid value
				auto tmp = const_iterator::find_next(d_data->base.root, 0, 0);
				return const_iterator(d_data, tmp.dir, tmp.child, 0, tmp.bit_pos);
			}
			SEQ_ALWAYS_INLINE const_iterator end() const noexcept { return const_iterator(d_data); }
			SEQ_ALWAYS_INLINE const_iterator cbegin() const noexcept { return begin(); }
			SEQ_ALWAYS_INLINE const_iterator cend() const noexcept { return end(); }

			// Alloctor

			SEQ_ALWAYS_INLINE Allocator& get_allocator() noexcept { return static_cast<Allocator&>(*this); }
			SEQ_ALWAYS_INLINE const Allocator& get_allocator() const noexcept { return static_cast<const Allocator&>(*this); }

			// Size functions

			SEQ_ALWAYS_INLINE bool empty() const noexcept { return !d_data || d_data->base.size == 0; }
			SEQ_ALWAYS_INLINE size_t size() const noexcept { return d_data ? d_data->base.size : 0; }
			SEQ_ALWAYS_INLINE size_t max_size() const noexcept { return std::numeric_limits<size_t>::max(); }

			/// @brief Destroy and deallocate all values, directories and nodes
			void clear() { destroy_data(); }

			/// @brief Swap 2 radix trees
			void swap(RadixTree& other, bool swap_alloc = true) noexcept(noexcept(swap_allocator(std::declval<Allocator&>(), std::declval<Allocator&>())))
			{
				std::swap(d_data, other.d_data);
				std::swap(d_root, other.d_root);
				if (swap_alloc)
					swap_allocator<Allocator>(*this, other);
			}

			/// @brief Reserve capcity ahead, only works for unsorted trees
			void reserve(size_t capacity)
			{
				if (node::is_sorted)
					return;

				// Create new radix tree
				RadixTree other(static_cast<Hash&>(*this), get_allocator());

				// Update its root size
				capacity = (capacity / (node::max_capacity));
				unsigned bits = bit_scan_reverse_64(capacity) + 1;
				bits = std::min(bits, 26U); // maximum 26 bits for a directory
				other.make_data(bits);

				// Move all values inside the new tree
				for (auto it = begin(); it != end(); ++it)
					other.emplace(std::move(*it));

				// swap
				this->swap(other);
				other.clear();
			}

			void shrink_to_fit()
			{
				// Create new radix tree and reinsert all values inside
				RadixTree other(static_cast<Hash&>(*this), get_allocator());
				other.reserve(this->size());
				other.insert(std::make_move_iterator(begin()), std::make_move_iterator(end()), false);
				other.swap(*this);
			}

			/// @brief Sort all leaves
			void sort_leaves()
			{
				if (!node::is_sorted)
					return;
				if (size() == 0)
					return;

				d_data->base.root->for_each_leaf([](directory* dir, unsigned pos) {
					auto child = dir->child(pos);
					if (child.tag() == directory::IsLeaf) {
						child.to_node()->template sort<extract_key_type>(Less{});
					}
				});
			}

			/// @brief Hash key and return the hash value
			template<class U>
			SEQ_ALWAYS_INLINE hash_type hash_key(const U& val) const
			{
				return this->hash(ExtractKey{}(val));
			}
			SEQ_ALWAYS_INLINE const hash_type& hash_key(const hash_type& val) const { return val; }

			auto get_prefix_first_bits(directory* dir, unsigned count, size_t bit_pos) noexcept -> unsigned
			{
				// Returns the count first bits starting at given directory based on any of its children
				if (bit_pos == minus_one_size_t)
					bit_pos = iterator::get_bit_pos(dir);
				return hash_key(dir->any_child()).n_bits(bit_pos, count);
			}

			directory* make_intermediate(directory* parent, unsigned hash_len, unsigned parent_pos)
			{
				directory* intermediate = directory::make(d_data->base, hash_len);
				intermediate->parent = parent;
				intermediate->parent_pos = parent_pos;
				parent->children()[parent_pos] = child_ptr(intermediate, directory::IsDir);
				parent->dir_count++;
				parent->child_count++;
				return intermediate;
			}

			/// @brief If dir is full and only contains directories, make it grow and replace its children by its grandkids
			directory* merge_dir(directory* dir, size_t bit_pos = minus_one_size_t)
			{
				directory* parent_dir = dir->parent;
				unsigned parent_pos = dir->parent_pos;

				unsigned size = dir->size();

				// new directory hash len
				unsigned new_hash_len = start_arity + dir->hash_len;

				if SEQ_UNLIKELY (new_hash_len >= 32)
					// we are above maximum allowed size for a directory
					return nullptr;

				// save internal value in order to reset it later to the new directory

				// might throw, fine
				directory* new_dir = directory::make(d_data->base, new_hash_len);
				// copy prefix length
				new_dir->prefix_len = dir->prefix_len;

				try {
					for (unsigned i = 0; i < size; ++i) {
						directory* child = static_cast<directory*>(dir->children()[i].ptr());
						unsigned child_count = child->size();

						if (prefix_search && child->prefix_len >= start_arity) {
							// keep this directory and remove max_hash_len to the prefix.
							unsigned loc = (i << start_arity) | (get_prefix_first_bits(child, start_arity, bit_pos));
							child->prefix_len -= start_arity;
							new_dir->child(loc) = dir->const_child(i);
							new_dir->child_count++;
							new_dir->dir_count++;
							child->parent = new_dir;
							child->parent_pos = loc;
							dir->children()[i] = child_ptr();
							continue;
						}

						if (child->hash_len != start_arity) {
							// if child has more than min_hash_len bits
							unsigned rem_bits = child->hash_len - start_arity;
							unsigned mask = ((1U << rem_bits) - 1U);

							for (unsigned j = 0; j < child_count; ++j) {
								unsigned loc = (i << start_arity) | (j >> rem_bits); // take high bits of j
								directory* intermediate = static_cast<directory*>(new_dir->children()[loc].ptr());
								if (!intermediate)
									// this part might throw which is a problem, we are in an intermediate state.
									intermediate = make_intermediate(new_dir, rem_bits, loc);

								// take low bits of j
								if ((intermediate->children()[j & mask] = child->children()[j])) // take high bits of j
								{
									intermediate->child_count++;
									if (intermediate->first_valid_child == static_cast<uint64_t>(-1))
										intermediate->first_valid_child = j & mask;
								}
								if (child->children()[j].tag() == directory::IsDir) {
									intermediate->dir_count++;
									directory* d = child->const_child(j).to_dir();
									d->parent = intermediate;
									d->parent_pos = j & mask;
								}
								else if (child->children()[j].tag() != 0)
									intermediate->first_valid_child = j & mask;

								// set children to null in case of exception
								child->children()[j] = child_ptr();
							}
						}
						else {
							for (unsigned j = 0; j < child_count; ++j) {
								// compute location
								unsigned loc = j | (i << child->hash_len);

								if ((new_dir->children()[loc] = child->children()[j]))
									new_dir->child_count++;

								// update directory count
								if (child->children()[j].tag() == directory::IsDir) {
									new_dir->dir_count++;
									directory* d = child->const_child(j).to_dir(); // static_cast<directory*>(child->children()[j].ptr());
									d->parent = new_dir;
									d->parent_pos = loc;
								}
								// set children to null in case of exception
								child->children()[j] = child_ptr();
							}
						}

						dir->children()[i] = child_ptr();
						directory::destroy(d_data->base, child, false);
					}
				}
				catch (...) {
					// to keep the basic exception guarantee, the simplest solution is just to clear the tree
					directory::destroy(d_data->base, new_dir, true);
					clear();

					throw;
				}

				// destroy old directory
				directory::destroy(d_data->base, dir, false);

				// keep merging if possible
				while (new_dir->dir_count == new_dir->size()) {
					if (parent_dir) {
						parent_dir->children()[parent_pos] = child_ptr(new_dir, directory::IsDir);
						new_dir->parent = parent_dir;
						new_dir->parent_pos = parent_pos;
						if (parent_dir->first_valid_child == parent_pos)
							parent_dir->invalidate_first_valid();
					}
					directory* d = merge_dir(new_dir);
					if (!d)
						break;
					new_dir = d;
				}

				if (parent_dir) {
					parent_dir->children()[parent_pos] = child_ptr(new_dir, directory::IsDir);
					new_dir->parent = parent_dir;
					new_dir->parent_pos = parent_pos;
					if (parent_dir->first_valid_child == parent_pos)
						parent_dir->invalidate_first_valid();

					// try to merge up
					if (parent_dir->dir_count == (1ULL << parent_dir->hash_len))
						merge_dir(parent_dir);
				}
				else
					d_root = d_data->base.root = new_dir;

				return new_dir;
			}

			/// @brief Returns the number of common bits from start_pos in the range [first,last)
			template<unsigned BitStep, class Iter>
			auto nb_common_bits(size_t start_bit, Iter start, Iter end) noexcept -> size_t
			{
				// Compute number of common bits from start_bits for given range
				// The result is rounded down to the previous BitStep multiplier

				size_t max_bits = hash_type::max_bits;

				if constexpr (variable_length) {
					// Compute maximum possible common bits
					auto hstart = hash_key((*start));
					max_bits = hstart.get_size() > start_bit ? hstart.get_size() - start_bit : 0;
					Iter it = start;
					++it;
					for (; it != end; ++it) {
						auto size = hash_key((*it)).get_size();
						if (size > start_bit) {
							size_t bits = size - start_bit;
							max_bits = std::max(max_bits, bits);
						}
					}
				}

				auto first = hash_key((*start));

				++start;
				size_t bits = max_bits;
				size_t i = 1;
				for (; start != end && bits > 0; ++start, ++i) {
					auto tmp = hash_key(*start);
					size_t start = start_bit;
					size_t common = 0;

					for (;;) {
						if (unsigned x = first.n_bits(start, 32u) ^ tmp.n_bits(start, 32u)) {
							common += 32U - bit_scan_reverse_32(x) - 1U;
							break;
						}
						common += 32u;
						start += 32u;
						if (start > first.get_size() && start > tmp.get_size()) {
							common = max_bits;
							break;
						}
					}
					bits = std::min(bits, common);
					bits = (bits / BitStep) * BitStep;
				}

				return bits;
			}

			template<class Iter>
			auto compute_common_bits(size_t start_pos, Iter first, Iter last) noexcept -> size_t
			{
				// Returns the number of common bits from start_pos in the range [first,last)
				return nb_common_bits<start_arity>(start_pos, first, last);
			}

			/// @brief Returns the number of common bits from start_pos between first and second
			template<class K1, class K2>
			auto compute_common_bits_2(size_t start_bits, const K1& first, const K2& second) noexcept -> size_t
			{
				// Returns the number of common bits from start_pos between first and second
				struct Iter
				{
					const hash_type* vals;
					bool operator!=(const Iter& o) const noexcept { return vals != o.vals; }
					Iter& operator++() noexcept
					{
						++vals;
						return *this;
					}
					const hash_type& operator*() const noexcept { return (*vals); }
				};
				const hash_type vals[2] = { hash_key(first), hash_key(second) };
				return compute_common_bits(start_bits, Iter{ vals }, Iter{ vals + 2 });
			}

			/// @brief Move all elements within given leaf into a newly created vector, and insert given new value
			template<class Policy, class K, class... Args>
			const_iterator insert_in_vector(directory* dir, size_t bit_pos, node* child, unsigned pos, Policy, K&& key, Args&&... args)
			{
				// turn node into a vector and move values
				vector_type* vec = d_data->base.make_vector(*this);
				unsigned position = 0;
				try {
					T* vals = child->values();
					unsigned count = child->count();
					for (unsigned i = 0; i < count; ++i)
						vec->emplace_no_check(std::move(vals[i]));
					position = Policy::emplace_vector_no_check(*vec, std::forward<K>(key), std::forward<Args>(args)...);
				}
				catch (...) {
					d_data->base.destroy_vector(vec);
					throw;
				}

				// destroy old child
				node::destroy(d_data->base, child);

				dir->children()[pos] = child_ptr(vec, directory::IsVector);
				++d_data->base.size;

				return const_iterator(d_data, dir, pos, position, bit_pos);
			}

			/// @brief Returns directory depth
			size_t get_depth(directory* dir)
			{
				size_t depth = 0;
				while (dir->parent) {
					++depth;
					dir = dir->parent;
				}
				return depth;
			}

			/// @brief Rehash leaf and move its elements to deeper leaves. Then, insert new value.
			template<bool EnsureSorted, class Policy, class K, class... Args>
			const_iterator rehash_node_and_insert(directory* dir, size_t hash_bits, const_hash_ref hash, std::uint8_t th, Policy policy, K&& key, Args&&... args)
			{
				static constexpr bool Sort = EnsureSorted && node::is_sorted;

				// get position in directory
				unsigned pos = hash.n_bits(hash_bits, dir->hash_len);
				// get leaf, which cannot be null at this point
				node* child = dir->const_child(pos).to_node();

				SEQ_ASSERT_DEBUG(child != nullptr, "");

				// increment bit position and save it
				hash_bits += dir->hash_len;
				size_t prev_hash_bits = hash_bits;

				if ((!variable_length && hash_bits > Hash::max_bits - start_arity) || (variable_length && get_depth(dir) > MaxDepth))
					// We exhausted all available bits, no choice but to turn this leaf into a vector
					return insert_in_vector(dir, hash_bits, child, pos, policy, std::forward<K>(key), std::forward<Args>(args)...);

				// create new child directory, might throw, fine
				directory* child_dir = directory::make(d_data->base, start_arity);

				node* n = nullptr;
				try {
					// Rehash node and insert its values inside the new directory
					T* vals = child->values();
					unsigned count = child->count();

					if constexpr (prefix_search) {
						// Find common prefix and increment bit position accordingly
						if (size_t common_bits = compute_common_bits(hash_bits, vals, vals + count)) {
							child_dir->prefix_len = common_bits;
							hash_bits += common_bits;
						}
					}

					for (unsigned i = 0; i < count; ++i) {

						// Compute position in directory.
						// This actually recompute the hash value (potentially costly).
						unsigned new_pos = hash_key(vals[i]).n_bits(hash_bits, start_arity);

						// get the tiny hash
						std::uint8_t cth = child->get_tiny_hash(i);

						if (!child_dir->const_child(new_pos).full()) {
							// create node. If this throw, the new directory is destroyed (basic exception guarantee)
							n = node::make(d_data->base, cth, EmplacePolicy{}, std::move(vals[i])).first;
							child_dir->child(new_pos) = child_ptr(n, directory::IsLeaf);
							child_dir->child_count++;
							child_dir->first_valid_child = new_pos;
						}
						else {
							// move value in leaf.
							// if Sort is false, insert the value at the end of the leaf (only for sorted nodes)
							n = static_cast<node*>(child_dir->const_child(new_pos).ptr());
							auto p = n->template insert<extract_key_type, Less>(
							  d_data->base, hash_bits, Sort ? static_cast<unsigned>(-1) : n->count(), cth, EmplacePolicy{}, std::move(vals[i]));
							child_dir->child(new_pos) = child_ptr(n = p.first, directory::IsLeaf);
						}
					}
				}
				catch (...) {
					// In case of exception, just destroyed the newly created directory.
					// Some values might have been moved to it but, hey, this is basic exception guarantee only
					directory::destroy(d_data->base, child_dir);
					throw;
				}

				// destroy child node unused anymore
				node::destroy(d_data->base, child);

				// update parent dir
				child_dir->parent = dir;
				child_dir->parent_pos = pos;
				dir->child(pos) = child_ptr(child_dir, directory::IsDir);
				dir->dir_count++;

				// now, check if the current directory contains only directories, and merge it if possible
				if (dir->dir_count == dir->size()) {

					if (merge_dir(dir, prev_hash_bits)) {
						// directory merging succeded, now insert the new value starting from the root
						return this->insert_hash_with_tiny<EnsureSorted>(d_data->base.root, 0, hash, th, policy, std::forward<K>(key), std::forward<Args>(args)...).first;
					}
				}

				// update first valid position
				if (dir->first_valid_child == pos)
					dir->invalidate_first_valid();

				// insert new value starting from dir
				return this->insert_hash_with_tiny<EnsureSorted>(dir, prev_hash_bits - dir->hash_len, hash_key((key)), th, policy, std::forward<K>(key), std::forward<Args>(args)...)
				  .first;
			}

			/// @brief Insert value in an empty position within dir
			template<class Policy, class K, class... Args>
			std::pair<const_iterator, bool> insert_null_node(directory* dir, size_t bit_pos, unsigned pos, unsigned th, Policy policy, K&& key, Args&&... args)
			{
				// child is empty: create a new leaf, might throw (fine)
				auto p = node::make(d_data->base, th, policy, std::forward<K>(key), std::forward<Args>(args)...);
				dir->child(pos) = child_ptr(p.first, directory::IsLeaf);
				dir->child_count++;
				dir->first_valid_child = pos;
				++d_data->base.size;

				return std::pair<const_iterator, bool>(const_iterator(d_data, dir, pos, 0, bit_pos), true);
			}

			/// @brief Insert new value in a vector node
			template<class Policy, class K, class... Args>
			std::pair<const_iterator, bool> insert_in_vector_node(directory* dir, size_t bit_pos, unsigned pos, unsigned, Policy, K&& key, Args&&... args)
			{
				// child is a vector
				vector_type* child = static_cast<vector_type*>(dir->children()[pos].ptr());
				auto found = Policy::emplace_vector(*child, std::forward<K>(key), std::forward<Args>(args)...);
				if (!found.second)
					return std::pair<const_iterator, bool>(const_iterator(d_data, dir, pos, static_cast<unsigned>(found.first), bit_pos), false);
				++d_data->base.size;
				return std::pair<const_iterator, bool>(const_iterator(d_data, dir, pos, static_cast<unsigned>(found.first), bit_pos), true);
			}

			/// @brief The value to insert does not follow a directory prefox -> create intermediate directory
			template<class U>
			directory* check_prefix_create_intermediate(directory* dir, directory* d, size_t& hash_bits, unsigned pos, const hash_type&, const U& value)
			{
				// compute the number of common bits starting at hash_bits
				const T& any_child = d->any_child();
				std::uint64_t common = compute_common_bits_2(hash_bits, value, any_child);

				// create intermediate directory with a new prefix length

				// might throw, fine
				directory* new_dir = directory::make(d_data->base, start_arity);
				d->parent->child(pos) = child_ptr(new_dir, directory::IsDir);
				new_dir->parent = d->parent;
				new_dir->parent_pos = pos;
				new_dir->child_count = new_dir->dir_count = 1;
				new_dir->prefix_len = static_cast<size_t>(common);

				hash_bits += static_cast<size_t>(common);
				unsigned new_pos = hash_key(any_child).n_bits(hash_bits, start_arity);
				new_dir->child(new_pos) = child_ptr(d, directory::IsDir);
				new_dir->first_valid_child = new_pos;
				d->parent = new_dir;
				d->parent_pos = new_pos;

				// detect nasty bug where prefix_len wrap around
				SEQ_ASSERT_DEBUG(d->prefix_len >= (start_arity + common), "");
				d->prefix_len -= start_arity + static_cast<size_t>(common);
				hash_bits -= dir->hash_len;

				return new_dir;
			}

			static SEQ_ALWAYS_INLINE bool generic_check_prefix(const hash_type& hash, const hash_type& match, size_t start, size_t bits) noexcept
			{
				// Check equal prefix for hash and match.
				// Only check from start for given bits length.

				if (bits == 0)
					return true;

				if constexpr (hash_type::has_fast_check_prefix) {
					// faster version for arithmetic types
					return hash_type::check_prefix(hash, match, start, bits);
				}
				// strings, arrays, vectors...
				if constexpr (variable_length) {
					if (start >= match.get_size() && start >= hash.get_size()) {
						return true;
					}
				}
				while (bits > 32u) {
					if (hash.n_bits(start, 32u) != match.n_bits(start, 32u))
						return false;
					start += 32u;
					bits -= 32u;
				}
				return hash.n_bits(start, bits) == match.n_bits(start, bits);
			}
			SEQ_ALWAYS_INLINE bool check_prefix(const hash_type& hash, size_t start_bit, const directory* d) const noexcept
			{
				// Check if given hash value share the same prefix as d
				return generic_check_prefix(hash, hash_key(d->any_child()), start_bit, d->prefix_len);
			}
			SEQ_ALWAYS_INLINE bool check_prefix(const hash_type& hash, const hash_type& h2, size_t start_bit, size_t bit_len) const noexcept
			{
				return generic_check_prefix(hash, h2, start_bit, bit_len);
			}
			SEQ_ALWAYS_INLINE bool check_prefix(const hash_type& hash, const key_type& k, size_t start_bit, size_t bit_len) const noexcept
			{
				return generic_check_prefix(hash, hash_key(k), start_bit, bit_len);
			}

			template<class U>
			SEQ_ALWAYS_INLINE auto check_prefix_insert(directory* dir, directory* d, size_t& hash_bits, unsigned pos, const hash_type& hash, const U& value) noexcept -> directory*
			{
				// Check if the value to insert follows the directory prefix.
				// If not, create an intermediate directory with a new prefix.
				// Update the bit position if the prefix matches.

				// increment bit position
				hash_bits += dir->hash_len;

				// check prefix
				if (check_prefix(hash, hash_bits, d)) {
					// the value follows the directory prefix: update the bit position
					hash_bits -= dir->hash_len;
					hash_bits += d->prefix_len;
					return d;
				}
				return check_prefix_create_intermediate(dir, d, hash_bits, pos, hash, value);
			}

			/// @brief Insert in leaf node
			template<bool EnsureSorted, class Policy, class K, class... Args>
			SEQ_ALWAYS_INLINE std::pair<const_iterator, bool>
			insert_in_leaf(directory* dir, node* child, size_t hash_bits, unsigned pos, const_hash_ref hash, std::uint8_t th, Policy policy, K&& key, Args&&... args)
			{
				// find key in node
				auto found = child->template find_insert<EnsureSorted, extract_key_type, Equal, Less>(Equal{}, hash_bits, th, ExtractKey{}(key));

				if (found.first)
					// key already exists!
					return std::pair<const_iterator, bool>(const_iterator(d_data, dir, pos, static_cast<unsigned>(found.first - child->values()), hash_bits), false);

				// check if the node is full and needs to be rehashed
				if SEQ_UNLIKELY (child->full())
					return std::pair<const_iterator, bool>(
					  rehash_node_and_insert<EnsureSorted>(dir, hash_bits, hash, th, policy, std::forward<K>(key), std::forward<Args>(args)...), true);

				// add to leaf
				auto p = child->template insert<extract_key_type, Less>(
				  d_data->base, hash_bits, EnsureSorted ? found.second : child->count(), th, policy, std::forward<K>(key), std::forward<Args>(args)...);

				// update first_valid_child
				dir->first_valid_child = pos;

				// increment size
				++d_data->base.size;

				// update child at pos
				dir->child(pos) = child_ptr(p.first, directory::IsLeaf);

				return std::pair<const_iterator, bool>(const_iterator(d_data, dir, pos, p.second, hash_bits), true);
			}

			/// @brief Main key insertion process, starting from dir at hash_bits position.
			template<bool EnsureSorted, class Policy, class K, class... Args>
			SEQ_ALWAYS_INLINE std::pair<const_iterator, bool>
			insert_hash_with_tiny(directory* dir, size_t hash_bits, const_hash_ref hash, std::uint8_t th, Policy p, K&& key, Args&&... args)
			{
				static constexpr bool Sort = EnsureSorted && node::is_sorted;

				// compute position within directory
				unsigned pos = hash.n_bits(hash_bits, dir->hash_len);

				// move forward inside the tree
				while (dir->const_child(pos).tag() == directory::IsDir) {
					// get child directory
					directory* d = static_cast<directory*>(dir->const_child(pos).ptr());

					// if this directory has a prefix, check that the value to insert follows it
					if (prefix_search && d->prefix_len)
						// check prefix and create intermediate directory if necessary
						d = check_prefix_insert(dir, d, hash_bits, pos, hash, key);

					// update bit position, current directory and new position within current directory
					hash_bits += dir->hash_len;
					dir = d;
					pos = hash.n_bits(hash_bits, dir->hash_len);
				}

				if (dir->const_child(pos).tag() == directory::IsNull)
					// child is empty: create a new node
					return insert_null_node(dir, hash_bits, pos, th, p, std::forward<K>(key), std::forward<Args>(args)...);

				else if (dir->const_child(pos).tag() == directory::IsVector)
					// insert in vector node
					return insert_in_vector_node(dir, hash_bits, pos, th, p, std::forward<K>(key), std::forward<Args>(args)...);

				// child is a leaf
				node* child = dir->const_child(pos).to_node();
				return this->template insert_in_leaf<Sort>(dir, child, hash_bits, pos, hash, th, p, std::forward<K>(key), std::forward<Args>(args)...);
			}

			/// @brief Insert new value based on its hash value
			template<bool EnsureSorted, class Policy, class K, class... Args>
			SEQ_ALWAYS_INLINE std::pair<const_iterator, bool> emplace_hash(const_hash_ref hash, Policy p, K&& key, Args&&... args)
			{
				if SEQ_UNLIKELY (!d_data)
					// initialize root
					make_data();

				// insert
				return insert_hash_with_tiny<EnsureSorted>(d_data->base.root, 0, hash, hash.tiny_hash(), p, std::forward<K>(key), std::forward<Args>(args)...);
			}

			/// @brief Construct emplace and insert
			template<class K, class... Args>
			SEQ_ALWAYS_INLINE std::pair<const_iterator, bool> emplace(K&& key, Args&&... args)
			{
				return this->template emplace_hash<true>(hash_key((key)), EmplacePolicy{}, std::forward<K>(key), std::forward<Args>(args)...);
			}

			/// @brief Insert new value based on its hash value and a hint
			template<bool EnsureSorted, class Policy, class K, class... Args>
			SEQ_ALWAYS_INLINE std::pair<const_iterator, bool> emplace_hash_hint(const_iterator hint, const_hash_ref h, Policy p, K&& key, Args&&... args)
			{
				SEQ_ASSERT_DEBUG(hint.data == d_data || !hint.data, "");

				// Insert value and try to reuse the previous position information.
				// This makes sorted insertion (ascending or desending order) way faster.

				// We must ensure that the previous position is valid,
				// and that the key prefix is the same as the directory one.
				// In addition, we only do that for fixed length keys
				// to avoid potential cache misses in check_prefix().

				if SEQ_UNLIKELY (!d_data)
					// initialize root
					make_data();

				// compute tiny hash
				std::uint8_t th = h.tiny_hash();
				if (variable_length || !is_sorted || hint == end() || hint.is_vector() || !check_prefix(h, hash_key(hint.dir->any_child()), 0, hint.bit_pos)) {
					// Reset position to insert from the root directory
					hint.dir = d_data->base.root;
					hint.bit_pos = 0;
				}

				// insert
				return insert_hash_with_tiny<EnsureSorted>(hint.dir, hint.bit_pos, h, th, p, std::forward<K>(key), std::forward<Args>(args)...);
			}

			template<class K, class... Args>
			SEQ_ALWAYS_INLINE const_iterator emplace_hint(const_iterator hint, K&& key, Args&&... args)
			{
				return this->template emplace_hash_hint<true>(hint, hash_key((key)), EmplacePolicy{}, std::forward<K>(key), std::forward<Args>(args)...).first;
			}

			template<class K, class... Args>
			SEQ_ALWAYS_INLINE auto try_emplace(K&& key, Args&&... args) -> std::pair<iterator, bool>
			{
				return this->template emplace_hash<true>(hash_key((key)), TryEmplacePolicy{}, std::forward<K>(key), std::forward<Args>(args)...);
			}

			template<class K, class... Args>
			SEQ_ALWAYS_INLINE auto try_emplace_hint(const_iterator hint, K&& key, Args&&... args) -> std::pair<iterator, bool>
			{
				return this->template emplace_hash_hint<true>(hint, hash_key((key)), TryEmplacePolicy{}, std::forward<K>(key), std::forward<Args>(args)...);
			}

			/// @brief Range insertion.
			/// For sorted radix tree, insert all values in an unsorted way within leaves, and sort all leaves independantly.
			template<class Iter>
			void insert(Iter start, Iter end, bool _sort_leaves = true)
			{
				if (start == end)
					return;

				const_iterator it = this->end();
				for (; start != end; ++start) {
					it = this->template emplace_hash_hint<false>(it, hash_key((*start)), EmplacePolicy{}, *start).first;
				}
				if (is_sorted && _sort_leaves)
					sort_leaves();
			}

			/// @brief Erase range
			/// Compute the distance N between first and last, then erase N elements starting from first
			const_iterator erase(const_iterator first, const_iterator last)
			{
				if (first == begin() && last == end()) {
					clear();
					return end();
				}
				size_t count = std::distance(first, last);
				for (size_t i = 0; i < count; ++i)
					first = erase(first);
				return first;
			}

			size_t remove_directory(directory* d)
			{
				// Remove the directory from the tree
				// and returns the total number fo removed elements.
				// The directory itself and its values are not destroyed
				// and can be used afterward.
				//
				// This leave the tree in a valid state.

				if (!d->parent) {
					// Root dir
					// Create a new root of half the size, might throw, fine
					auto* new_root = directory::make(d_data->base, d->hash_len - start_arity);
					d_data->base.root = d_root = new_root;
				}

				// The remaining does not throw

				auto* p = d->parent;

				// Detach directory
				if (d->parent) {
					d->parent->child(d->parent_pos) = child_ptr();
					d->parent->child_count--;
					d->parent->dir_count--;
					d->parent->invalidate_first_valid();
					d->parent = nullptr;

					// Destroy empty directories until root directory
					while (p->parent && p->child_count == 0) {
						directory* parent = p->parent;
						unsigned parent_pos = p->parent_pos;

						directory::destroy(d_data->base, p, false);
						parent->children()[parent_pos] = child_ptr();
						parent->child_count--;
						parent->dir_count--;
						if (parent->first_valid_child == parent_pos)
							parent->invalidate_first_valid();
						p = parent;
					}
				}

				// Update container size
				size_t count = 0;
				d->for_each_leaf([&count](directory* dir, unsigned pos) {
					auto tag = dir->child(pos).tag();
					if (tag == directory::IsLeaf)
						count += dir->child(pos).to_node()->count();
					else
						count += dir->child(pos).to_vector()->size();
				});

				this->d_data->base.size -= count;
				return count;
			}

			const_iterator rebalance(directory* d, const_iterator it)
			{
				const_iterator res = end();

				// Move iterator value to tmp storage
				alignas(T) char data[sizeof(T)];
				T* value = it == end() ? nullptr : (T*)data;
				if (value) {
					new (value) T(std::move(*it));
				}

				// Check if 'it' is inside d.
				// If not, remove it from the tree.
				if (it != end()) {
					auto dir = it.dir;
					while (dir) {
						if (dir == d)
							break;
						dir = dir->parent;
					}
					if (!dir) {
						// NOT inside d
						erase(it, false);
					}
				}

				try {
					remove_directory(d);

					// Reinsert all values inside directory into the tree
					d->for_each_leaf([this, &it](directory* dir, unsigned pos) {
						auto tag = dir->child(pos).tag();
						if (tag == directory::IsLeaf) {
							// Standard leaf
							auto node = dir->child(pos).to_node();
							for (unsigned i = 0; i < node->count(); ++i) {
								// Insert all values except the moved one
								if (it.dir != dir || it.child != pos || it.node_pos != i)
									this->insert_hash_with_tiny<true>(
									  d_data->base.root, 0, hash_key(node->values()[i]), node->hashs()[i], EmplacePolicy{}, std::move(node->values()[i]));
							}
						}
						else {
							// Vector node
							auto* vec = dir->child(pos).to_vector();
							for (unsigned i = 0; i < vec->size(); ++i) {
								if (it.dir != dir || it.child != pos || it.node_pos != i)
									this->emplace(std::move(vec->at(i)));
							}
						}
					});

					if (value) {
						res = this->emplace(std::move(*value)).first;
						value->~T();
					}
				}
				catch (...) {
					// Destroy the directory and recompute ends
					// to leave the tree in a valid state
					directory::destroy(d_data->base, d);
					if (value)
						value->~T();
					throw;
				}

				directory::destroy(d_data->base, d);
				return res;
			}

			/// @brief Erase a single element
			const_iterator erase(const_iterator it, bool with_rebalance = true)
			{
				SEQ_ASSERT_DEBUG(it != end(), "");

				// Get next position
				const_iterator next = it;
				++next;

				directory* d = it.dir;
				unsigned dpos = it.child;

				if (d->child(dpos).tag() == directory::IsVector) {

					// erase inside vector leaf
					vector_type* v = d->child(dpos).to_vector();
					v->erase(it.node_pos);

					if (v->size() == 0) {
						// destroy empty vector
						destroy_ptr(v);
						d->children()[dpos] = child_ptr();
						d->child_count--;
					}
					else {
						--d_data->base.size;
						if (it.node_pos == v->size())
							return next;
						return it;
					}
				}
				else {
					// erase in leaf node
					node* n = d->child(dpos).to_node();
					n = n->erase(d_data->base, it.node_pos);
					d->children()[dpos] = child_ptr(n, n ? d->child(dpos).tag() : 0);
					if (!n)
						d->child_count--;
					else {
						--d_data->base.size;
						if (it.node_pos == n->count())
							return next;
						return it;
					}
				}

				--d_data->base.size;

				// If we reach that point, the directory might be empty

				// Invalidate first valid position
				if (d->first_valid_child == dpos)
					d->invalidate_first_valid();

				if (d->child_count) {
					// Rebalance
					if (is_sorted && with_rebalance && d->hash_len != start_arity && d->child_count < (1ull << (d->hash_len - start_arity)))
						return rebalance(d, next);
					return next;
				}

				// Destroy empty directories until root directory
				while (d->parent && d->child_count == 0) {
					directory* parent = d->parent;
					unsigned parent_pos = d->parent_pos;

					directory::destroy(d_data->base, d, false);
					parent->children()[parent_pos] = child_ptr();
					parent->child_count--;
					parent->dir_count--;
					if (parent->first_valid_child == parent_pos)
						parent->invalidate_first_valid();
					d = parent;
				}

				// Rebalance
				if (is_sorted && with_rebalance && d->hash_len != start_arity && d->child_count < (1ull << (d->hash_len - start_arity)))
					return rebalance(d, next);
				if (size() == 0)
					return end();
				return next;
			}

			template<class K>
			size_t erase(const K& key)
			{
				auto it = find(key);
				if (it == end())
					return 0;
				erase(it);
				return 1;
			}

			/// @brief Check if given hash value share the same prefix as d
			//SEQ_ALWAYS_INLINE bool check_prefix(const hash_type& hash, const directory* d) const noexcept { return check_prefix(hash, hash_key(d->any_child()), d->prefix_len); }

			/// @brief Find key in vector node
			template<class U>
			const_iterator find_in_vector(const directory* d, size_t bit_pos, unsigned pos, const vector_type* vec, const U& key) const
			{
				size_t found = vec->find(key);
				if (found == vec->size())
					return end();
				return const_iterator(d_data, d, pos, static_cast<unsigned>(found), bit_pos);
			}

			template<class U>
			const T* find_in_vector_ptr(const directory*, size_t, unsigned, const vector_type* vec, const U& key) const
			{
				size_t found = vec->find(key);
				if (found == vec->size())
					return nullptr;
				return &vec->at(found);
			}

			/// @brief Find key based on its hash value
			/// Make this function template just for hash table allowing heterogeneous lookup
			template<class U>
			SEQ_ALWAYS_INLINE const_iterator find_hash(const_hash_ref hash, const U& key) const
			{
				// start directory
				const directory* d = d_root;

				// tiny hash
				unsigned th = hash.tiny_hash();

				// start position within start directory
				size_t bit_pos = 0;
				unsigned pos = hash.n_bits(bit_pos, d->hash_len);

				// walk through the tree as long as we keep finding directories
				while (std::uintptr_t tag = d->children()[pos].tag()) {
					switch (tag) {
						case directory::IsDir:
							bit_pos += (d->hash_len);
							d = d->children()[pos].to_dir();

							// check directory prefix
							if (prefix_search && d->prefix_len) {
								if (!check_prefix(hash, bit_pos, d))//TEST
									return cend();
								else
									bit_pos += d->prefix_len;
							}

							// compute new position within current directory
							pos = hash.n_bits(bit_pos, d->hash_len);
							continue;

						case directory::IsVector:
							return find_in_vector(d, bit_pos, pos, d->children()[pos].to_vector(), key);

						case directory::IsLeaf:
							th = d->children()[pos].to_node()->template find<extract_key_type, Equal, Less>(Equal{}, bit_pos, static_cast<std::uint8_t>(th), key);
							if (th != static_cast<unsigned>(-1))
								return const_iterator(d_data, d, pos, th, bit_pos);
							return cend();
					}
				}
				// not found: the node is null or the key is not inside
				return cend();
			}

			template<class U>
			SEQ_ALWAYS_INLINE const T* find_ptr_hash(const_hash_ref hash, const U& key) const
			{
				// start directory
				const directory* d = d_root;
				// tiny hash
				unsigned th = hash.tiny_hash();
				// start position within start directory
				size_t bit_pos = 0;
				unsigned pos = hash.n_bits(0, d->hash_len);

				// walk through the tree as long as we keep finding directories
				while (std::uintptr_t tag = d->children()[pos].tag()) {
					switch (tag) {
						case directory::IsDir:
							bit_pos += (d->hash_len);
							d = d->children()[pos].to_dir();

							// check directory prefix
							if (prefix_search && d->prefix_len) {
								if (!check_prefix(hash, bit_pos, d)) //TEST: bit_pos
									return nullptr;
								else
									bit_pos += d->prefix_len;
							}

							// compute new position within current directory
							pos = hash.n_bits(bit_pos, d->hash_len);
							continue;

						case directory::IsVector:
							return find_in_vector_ptr(d, bit_pos, pos, d->children()[pos].to_vector(), key);

						case directory::IsLeaf:
							th = d->children()[pos].to_node()->template find<extract_key_type, Equal, Less>(Equal{}, bit_pos, static_cast<std::uint8_t>(th), key);
							if (th != static_cast<unsigned>(-1))
								return d->children()[pos].to_node()->values() + th;
							return nullptr;
					}
				}
				// not found: the node is null or the key is not inside
				return nullptr;
			}

			/// @brief Returns iterator pointing to given key, or end() if not found
			template<class U>
			SEQ_ALWAYS_INLINE const_iterator find(const U& k) const
			{
				return find_hash(hash_key(k), k);
			}
			template<class U>
			SEQ_ALWAYS_INLINE const T* find_ptr(const U& k) const
			{
				return find_ptr_hash(hash_key(k), k);
			}

			const_iterator lower_bound_in_vector(const directory* d, unsigned pos, size_t bit_pos, const key_type& key) const
			{
				const vector_type* v = d->children()[pos].to_vector();
				unsigned p = static_cast<unsigned>(v->lower_bound(key));
				if (p != v->size())
					return const_iterator(d_data, d, pos, p, bit_pos);
				return ++const_iterator(d_data, d, pos, p - 1, bit_pos);
			}

			/// @brief Find key based on its hash value
			SEQ_ALWAYS_INLINE const_iterator lower_bound_hash(const_hash_ref hash, const key_type& key) const
			{
				// start directory
				const directory* d = d_root;
				// tiny hash
				unsigned th = hash.tiny_hash();
				// start position within start directory
				size_t bit_pos = 0;
				unsigned pos = hash.n_bits(bit_pos, d->hash_len);

				// walk through the tree as long as we keep finding directories
				while (d->children()[pos].tag() == directory::IsDir) {
					bit_pos += (d->hash_len);

					// compute new directory
					d = d->children()[pos].to_dir();

					if constexpr (prefix_search) {
						// check directory prefix
						if (d->prefix_len) {
							if (!check_prefix(hash, bit_pos, d)) {//TEST: bit_pos
								bool less = Less{}(ExtractKey{}(d->any_child()), key);
								auto tmp = const_iterator::find_next(
								  less ? d->parent : d, less ? d->parent_pos + 1 : 0, less ? bit_pos - d->parent->hash_len : bit_pos + d->prefix_len);

								// Note: could be end() iterator
								return const_iterator(d_data, tmp.dir, tmp.child, 0, tmp.bit_pos);
							}
							else
								bit_pos += d->prefix_len;
						}
					}
					// compute new position within current directory
					pos = hash.n_bits(bit_pos, d->hash_len);
				}

				if (d->children()[pos].tag() == 0) {
					// return next item
					auto tmp = const_iterator::find_next(d, pos + 1, bit_pos);

					// Note: could be end() iterator
					return const_iterator(d_data, tmp.dir, tmp.child, 0, tmp.bit_pos);
				}

				// at this point, the position within current directory hold either a standard leaf or a vector

				if SEQ_UNLIKELY (d->children()[pos].tag() == directory::IsVector)
					return lower_bound_in_vector(d, pos, bit_pos, key);

				const node* n = d->children()[pos].to_node();
				//TEST
				auto & back = n->back();
				unsigned p = n->template lower_bound<ExtractKey, Less, Equal>(bit_pos, th, key);
				if (p != n->count())
					return const_iterator(d_data, d, pos, p, bit_pos);
				return ++const_iterator(d_data, d, pos, p - 1, bit_pos);
			}
			template<class U>
			SEQ_ALWAYS_INLINE const_iterator lower_bound(const U& k) const
			{
				return lower_bound_hash(hash_key(k), k);
			}

			SEQ_ALWAYS_INLINE const_iterator upper_bound_hash(const_hash_ref hash, const key_type& key) const
			{
				const_iterator it = lower_bound_hash(hash, key);
				if (it != end() && ExtractKey{}(*it) == key)
					++it;
				return it;
			}
			template<class U>
			SEQ_ALWAYS_INLINE const_iterator upper_bound(const U& k) const
			{
				return upper_bound_hash(hash_key(k), k);
			}

			/// @brief Find key based on its hash value
			SEQ_ALWAYS_INLINE const_iterator prefix_hash(const_hash_ref hash, const key_type& key) const
			{
				static_assert(prefix_search, "prefix function is only available for variable length keys!");

				auto it = lower_bound_hash(hash, key);
				if (it != end() && ExtractKey{}(*it).find(key.data(), 0, key.size()) == 0)
					return it;
				return end();
			}
			template<class U>
			SEQ_ALWAYS_INLINE const_iterator prefix(const U& k) const
			{
				return prefix_hash(hash_key(k), k);
			}

			class const_prefix_iterator
			{
				const_iterator it;
				key_type prefix;

			public:
				using iterator_category = std::forward_iterator_tag;
				using value_type = T;
				using difference_type = std::ptrdiff_t;
				using const_pointer = const value_type*;
				using const_reference = const value_type&;
				using pointer = value_type*;
				using reference = value_type&;

				SEQ_ALWAYS_INLINE const_prefix_iterator(const const_iterator& i = const_iterator(), const key_type& pr = key_type()) noexcept
				  : it(i)
				  , prefix(pr)
				{
				}
				const_prefix_iterator(const const_prefix_iterator&) = default;
				const_prefix_iterator& operator=(const const_prefix_iterator&) = default;

				SEQ_ALWAYS_INLINE auto operator*() const noexcept -> reference { return *it; }
				SEQ_ALWAYS_INLINE auto operator->() const noexcept -> pointer { return std::pointer_traits<pointer>::pointer_to(**this); }
				SEQ_ALWAYS_INLINE auto operator++() noexcept -> const_prefix_iterator&
				{
					++it;
					if (it != const_iterator(it.data) && ExtractKey{}(*it).find(prefix.data(), 0, prefix.size()) != 0)
						it = const_iterator(it.data); // end
					return *this;
				}
				SEQ_ALWAYS_INLINE auto operator++(int) noexcept -> const_prefix_iterator
				{
					const_prefix_iterator _Tmp = *this;
					++(*this);
					return _Tmp;
				}
				SEQ_ALWAYS_INLINE bool operator==(const const_prefix_iterator& other) const noexcept { return it == other.it; }
				SEQ_ALWAYS_INLINE bool operator!=(const const_prefix_iterator& other) const noexcept { return it != other.it; }
			};

			template<class U>
			SEQ_ALWAYS_INLINE auto prefix_range(const U& k) const -> std::pair<const_prefix_iterator, const_prefix_iterator>
			{
				auto it = prefix(k);
				return std::make_pair(const_prefix_iterator(it, extract_key_type{}(k)), const_prefix_iterator(end()));
			}
		};
	}
}

#endif
