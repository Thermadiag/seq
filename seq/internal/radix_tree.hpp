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

#ifndef SEQ_RADIX_TREE_HPP
#define SEQ_RADIX_TREE_HPP

#include "../bits.hpp"
#include "../tagged_pointer.hpp"
#include "../hash.hpp"
#include "../flat_map.hpp"
#include "simd.hpp"


namespace seq
{
	namespace radix_detail
	{
		/// @brief Find result type of ExtractKey::operator() on T object
		template<class ExtractKey, class T>
		struct ExtractKeyResultType
		{
			using rtype = decltype(ExtractKey{}(std::declval<T>()));
			using type = typename std::decay<rtype>::type;
		};
		template<class ExtractKey, class T>
		struct ExtractCVKeyResultType
		{
			using type = decltype(ExtractKey{}(std::declval<T>()));
		};
	}

	/// @brief Default (dummy) less functor for radix hash tree
	struct default_less
	{
		template<class A, class B>
		bool operator()(const A &, const B&) const noexcept {
			return false;
		}
	};

	/// @brief Default key extractor for radix tree
	template<class T, class = void>
	struct default_key
	{
		const T& operator()( const T& val) const noexcept {
			return val;
		}
		// Define for heterogeneous lookup
		template<class Key>
		T operator()(const Key& val) const noexcept {
			return static_cast<T>(val);
		}
	};

	/// @brief Default key extractor for string keys and radix tree
	template<class T>
	struct default_key<T, typename std::enable_if<is_generic_char_string<T>::value,void>::type >
	{
		template<class Traits, size_t S, class A>
		tstring_view operator()(const tiny_string<char,Traits, A,S>& val) const noexcept {return val;}
		template<class Traits, class A>
		tstring_view operator()(const std::basic_string<char,Traits,A>& val) const noexcept {return val;}
		tstring_view operator()(const char* val) const noexcept {return tstring_view(val);}
#ifdef SEQ_HAS_CPP_17
		template<class Traits>
		tstring_view operator()(const std::basic_string_view<char,Traits>& val) const noexcept {return val;}
#endif
	};


	namespace radix_detail
	{
		/// @brief Policy used when inserting new key
		struct EmplacePolicy
		{
			template<class Vector, class K, class... Args >
			static std::pair<size_t,bool> emplace_vector(Vector& v, K&& key, Args&&... args)
			{
				return v.emplace(std::forward<K>(key), std::forward<Args>(args)...);
			}
			template<class Vector, class K, class... Args >
			static unsigned emplace_vector_no_check(Vector& v, K&& key, Args&&... args)
			{
				return static_cast<unsigned>(v.emplace_no_check(std::forward<K>(key), std::forward<Args>(args)...).first);
			}
			template<class T, class K, class... Args >
			static T* emplace(T * dst, K&& key, Args&&... args)
			{
				return new (dst) T(std::forward<K>(key), std::forward<Args>(args)...);
			}
		};
		/// @brief Policy used when inserting new key using try_emplace
		struct TryEmplacePolicy
		{
			template<class Vector, class K, class... Args >
			static std::pair<size_t, bool> emplace_vector(Vector& v, K&& key, Args&&... args)
			{
				return v.emplace(typename Vector::value_type(std::piecewise_construct,
					std::forward_as_tuple(std::forward<K>(key)),
					std::forward_as_tuple(std::forward<Args>(args)...)));
			}
			template<class Vector, class K, class... Args >
			static unsigned emplace_vector_no_check(Vector& v, K&& key, Args&&... args)
			{
				return static_cast<unsigned>(v.emplace_no_check(typename Vector::value_type(std::piecewise_construct,
					std::forward_as_tuple(std::forward<K>(key)),
					std::forward_as_tuple(std::forward<Args>(args)...))).first);
			}
			template<class T, class K, class... Args >
			static T* emplace(T* dst, K&& key, Args&&... args)
			{
				return new (dst) T(std::piecewise_construct,
					std::forward_as_tuple(std::forward<K>(key)),
					std::forward_as_tuple(std::forward<Args>(args)...));
			}
		};



		/// @brief Less functor for lower bound search in a leaf node
		template<class ExtractKey, class Le>
		struct Less
		{
			template<class A, class B>
			bool operator()(const A& a, const B& b) const noexcept {
				return Le{}(ExtractKey{}(a) , ExtractKey{}(b));
			}
		};

		/// @brief Lower bound function for leaf nodes
		template<class T, class ExtractKey, class Le>
		struct LowerBound
		{
			template<class SizeType, class K>
			static SizeType apply(size_t , const T* vals, SizeType size, const K& key) noexcept{
				return seq::detail::lower_bound<false,T>(vals, size, key, Less<ExtractKey, Le>{}).first;
				//return seq::detail::lower_bound_internal<T>::apply(vals, size, key, Less<ExtractKey, Le>{});
			}
		};
		
		/// @brief Hash function for sorted radix tree
		template<class Key, class = void>
		struct SortedHasher
		{};

		/// @brief Hash value type for intergal types and sorted radix tree
		template<class Integral>
		struct IntegralHash
		{
			static constexpr Integral integral_bits = sizeof(Integral) * 8U;
			Integral hash; // Hash value
			std::uint8_t hash_shift; // Current shift
			IntegralHash() {}
			IntegralHash(Integral h) noexcept :hash(h), hash_shift(0){}
			IntegralHash(Integral h, size_t sh) noexcept :hash(h), hash_shift(static_cast<std::uint8_t>(sh)) {}
			
			SEQ_ALWAYS_INLINE bool add_shift(size_t shift) const noexcept {
				const_cast<IntegralHash*>(this)->hash_shift += static_cast<std::uint8_t>(shift);
				return hash_shift <= sizeof(Integral) * 8U; //still valid
			}
			SEQ_ALWAYS_INLINE unsigned get() const noexcept {return n_bits(32U);}
			SEQ_ALWAYS_INLINE size_t get_shift() const noexcept {return hash_shift;}
			SEQ_ALWAYS_INLINE unsigned n_bits(size_t count) const noexcept {return n_bits(hash_shift, count);}
			SEQ_ALWAYS_INLINE unsigned n_bits(size_t start, size_t count) const noexcept {
				if (count == 0) return 0;
				Integral res = static_cast<Integral>(hash << static_cast<Integral>(start));
				res >>= (integral_bits - static_cast<Integral>(count));
				return static_cast<unsigned>(res);
			}
			SEQ_ALWAYS_INLINE size_t get_size() const noexcept {return integral_bits;}

			/// @brief Extract common bits from several integral keys
			template<unsigned BitStep, class ExtractKey, class Hasher, class Iter>
			static  size_t nb_common_bits(const Hasher& h, size_t start_bit, Iter start, Iter end) noexcept
			{
				size_t bits = sizeof(Integral) * 8U - start_bit;
				Integral prefix = (h.hash(ExtractKey{}(*start)).hash) << static_cast<Integral>(start_bit);
				++start;
				for (; start != end && bits > 0; ++start) {
					Integral tmp = ((h.hash(ExtractKey{}(*start)).hash) << static_cast<Integral>(start_bit));
					std::uint64_t x = tmp ^ prefix;
					
					if (x) {
						bits = std::min(bits, sizeof(Integral) * 8U - static_cast<size_t>(bit_scan_reverse_64(x)) - 1U );
						bits = (bits / BitStep) * BitStep;
					}
				}
				return bits;
			}

			template<class Key>
			static SEQ_ALWAYS_INLINE bool check_prefix(const IntegralHash& hash, const Key& key, size_t bits) noexcept
			{			
				IntegralHash match = SortedHasher<Key>{}.hash(key);
				if (hash.n_bits(hash.hash_shift, bits) == match.n_bits(hash.hash_shift, bits)) {
					hash.add_shift(bits);
					return true;
				}
				return false;
			}

		};

		/// @brief Hash value type for unordered radix tree (hash table)
		struct SizeTHash
		{
			static constexpr size_t integral_bits = sizeof(size_t) * 8U;
			size_t hash{ 0 };
			SizeTHash() {}
			SizeTHash(size_t h) noexcept :hash(h)   {}
			bool add_shift(size_t shift) const noexcept {
				const_cast<SizeTHash*>(this)->hash <<= shift;
				return true; //still valid
			}
			unsigned get() const noexcept {return n_bits(32U);}
			size_t get_shift() const noexcept {return 0;}
			unsigned n_bits(size_t count) const noexcept {return static_cast<unsigned>(count == 0 ? 0 : ((hash) >> (integral_bits - count)));}
			unsigned n_bits(size_t start, size_t count) const noexcept {return static_cast<unsigned>(count == 0 ? 0 : (hash << start) >> (integral_bits - count));}
			size_t get_size() const noexcept {return integral_bits;}
			template<unsigned BitStep, class ExtractKey, class Hasher, class Iter>
			static size_t nb_common_bits(const Hasher& , size_t , Iter , Iter ) noexcept {return 0;}
			template<class Key>
			static bool check_prefix(const SizeTHash& , const Key&  , size_t ) noexcept {return false;}
		};

		/// @brief Hash value type for strings and sorted radix tree
		struct StringHash
		{
			using size_type = std::uint64_t;
			const char* data{ nullptr };
			std::uint64_t size{ 0 };
			std::uint64_t hash_shift{ 0 };

			StringHash() {}
			StringHash(const char* d, size_type s) noexcept
				:data(d), size(s), hash_shift(0) {}
			StringHash(size_t sh, const char* d, size_type s) noexcept
				:data(d), size(s), hash_shift(sh) {}
			size_t get_shift() const noexcept {return static_cast<size_t>(hash_shift);}
			size_t get_size() const noexcept {return static_cast<size_t>(size * 8ULL);}
			SEQ_ALWAYS_INLINE unsigned n_bits(size_t count) const noexcept{	return n_bits(static_cast<size_t>(hash_shift), count);}
			SEQ_ALWAYS_INLINE unsigned n_bits(size_t start, size_t count) const noexcept
			{
				if (count == 0) return 0;
				if (count == 4) {
					unsigned char byte = (start / 8U) >= size ? 0 : static_cast<unsigned char>(data[(start / 8U)]);
					return ((start) & 7U) ? (byte & 0xFU) : (byte >> 4U);
				}
				if (count == 32)
					return get(start);
				// We can afford to read only 4 bytes, which represents up to 28 bits of usefull data.
				// And the maximum size of a directory is on 27 bits...
				std::uint64_t byte_offset = (start) / 8U;
				std::uint64_t bit_offset = (start) & 7U;
				unsigned hash = 0;
				if (SEQ_LIKELY(size >= byte_offset + 4U))
					memcpy(&hash, data + byte_offset, 4);
				else if(byte_offset < size)
					memcpy(&hash, data + byte_offset, static_cast<size_t>(size - byte_offset));
#if SEQ_BYTEORDER_ENDIAN != SEQ_BYTEORDER_BIG_ENDIAN
				hash = seq::byte_swap_32(hash);
#endif				
				return static_cast<unsigned>((hash << bit_offset) >> (32U - count));
			}
			SEQ_ALWAYS_INLINE bool add_shift(size_type shift) const noexcept
			{
				const_cast<StringHash*>(this)->hash_shift += shift;
				return (hash_shift) <= size * 8U;; //still valid
			}
			unsigned get(size_t shift) const noexcept
			{
				std::uint64_t byte_offset = (shift) / 8U;
				std::uint64_t bit_offset = (shift) & 7U;
				std::uint64_t hash = 0;
				if (SEQ_LIKELY(size >= byte_offset + 8U))
					memcpy(&hash, data + byte_offset, 8);
				else if (byte_offset < size)
					memcpy(&hash, data + byte_offset, static_cast<size_t>(size - byte_offset));
#if SEQ_BYTEORDER_ENDIAN != SEQ_BYTEORDER_BIG_ENDIAN
				hash = seq::byte_swap_64(hash);
#endif				
				return static_cast<unsigned>((hash << bit_offset) >> (64U - 32U));
			}
			unsigned get() const noexcept{return get(static_cast<size_t>(hash_shift));}

			template<unsigned BitStep, class ExtractKey, class Hasher, class Iter>
			static size_t nb_common_bits(const Hasher& h, size_t start_bit, Iter start, Iter end) noexcept
			{
				
				size_t max_bits = ExtractKey{}(*start).size() * 8U > start_bit ? ExtractKey{}(*start).size() * 8U - start_bit : 0;
				for(Iter it = start; it != end; ++it) {
					tstring_view tmp = ExtractKey{}(*it);
					if (tmp.size() * 8U > start_bit)
					{
						size_t bits = tmp.size() * 8U - start_bit;
						max_bits = std::max(max_bits, bits);
					}
				}

				tstring_view first = ExtractKey{}(*start); 
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
							common += 32U - bit_scan_reverse_32(x) -1U;
							break;
						}
					}
					bits = std::min(bits, common);
					bits = (bits / BitStep) * BitStep;

				}
				
				return bits;
			}

			template<class Key>
			static SEQ_ALWAYS_INLINE bool check_prefix(const StringHash& hash, const Key& val, size_t bits) noexcept
			{
				// get hash to compare to
				auto match = StringHash(hash.get_shift(), val.data(), val.size());

				if (match.get_shift() >= match.get_size()  && hash.get_shift() >= hash.get_size() ) {
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


		/// @brief Hash class for unordered radix tree
		/// @tparam Hash hash function to be used, usually std::hash<>
		template<class Hash, class Equal, class Less = default_less >
		struct Hasher : public Hash
		{
			static constexpr bool prefix_search = false;
			static constexpr bool variable_length = false;
			static constexpr size_t max_bits = sizeof(size_t) * 8U;
			using type = SizeTHash;
			using const_reference = type;
			using less_type = Less;

			template<class A, class B>
			static bool equal(const A& a, const B& b) { return Equal{}(a, b); }
			template<class A, class B>
			static bool less(const A& a, const B& b) { return Less{}(a, b); }

			Hasher() : Hash() {}
			Hasher(const Hasher & h) : Hash(h) {}
			Hasher(const Hash& h) : Hash(h) {}

			const Hash& hash_function() const noexcept { return *this; }

			template<class Key>
			type hash(const Key& k) const
			{
				return hash_value(*this, k);
			}
			template<class Key>
			type hash_shift(size_t shift , const Key& k) const
			{
				return hash(k).hash << shift;
			}
			template<class HashType, class Key>
			static std::uint8_t tiny_hash(HashType hash, const Key&) noexcept
			{
				return static_cast<std::uint8_t>((hash.hash) & 255U);
			}
		};

		/// @brief Base class for sorted hashers, only provide equality and less comparisons
		struct BaseSortedHasher
		{
			using less_type = seq::less<>;
			template<class A, class B> static bool equal(const A& a, const B& b)noexcept { return a == b; }
			template<class A, class B> static bool less(const A& a, const B& b) noexcept { return a < b; }
		};

		/// @brief Hash function for integral types and sorted radix tree
		template<class Key>
		struct SortedHasher<Key, typename std::enable_if<std::is_integral<Key>::value, void>::type > : BaseSortedHasher
		{
			using integral_type = typename std::make_unsigned<Key>::type;
			using type = IntegralHash<integral_type>;
			using const_reference = const type&;
			static constexpr bool prefix_search = true;
			static constexpr bool variable_length = false;
			static constexpr size_t max_bits = sizeof(Key) * 8U;

			SEQ_ALWAYS_INLINE type hash(Key k) const noexcept
			{
				integral_type val = static_cast<integral_type>(k);
				if (std::is_signed<Key>::value)
					// wrap around to keep an unsigned value in the same order as the signed one
					val += (static_cast<integral_type>(1) << (static_cast<integral_type>(sizeof(Key) * 8 - 1)));
				return type(val);
				
			}
			SEQ_ALWAYS_INLINE type hash_shift(size_t shift, const Key& k) const noexcept
			{
				integral_type val = static_cast<integral_type>(k);
				if (std::is_signed<Key>::value)
					// wrap around to keep an unsigned value in the same order as the signed one
					val += (static_cast<integral_type>(1) << (static_cast<integral_type>(sizeof(Key) * 8 - 1)));
				return type(val,shift);
			}
			template<class Hash>
			static SEQ_ALWAYS_INLINE std::uint8_t tiny_hash(Hash hash, const Key&) noexcept
			{
				return static_cast<std::uint8_t>(hash_finalize(hash.hash) & 255u);
			}
		};

		/// @brief Hash function for float type and sorted radix tree
		template<>
		struct SortedHasher<float, void> : BaseSortedHasher
		{
			using type = IntegralHash<unsigned>;
			using const_reference = const type&;
			static constexpr bool prefix_search = true;
			static constexpr bool variable_length = false;
			static constexpr size_t max_bits = sizeof(float) * 8U;

			SEQ_ALWAYS_INLINE type hash(float k) const noexcept
			{
				union U {
					float d;
					std::uint32_t u;
				};
				U u = { k };
				// Interpret float as 32-bit unsigned.
				// Flip all except top if top bit is set.
				u.u ^= ((static_cast<unsigned>((static_cast<int>(u.u)) >> 31)) >> 1);
				// Flip top bit.
				u.u ^= (1u << 31u);
				return type(u.u);
			}
			SEQ_ALWAYS_INLINE type hash_shift(size_t shift, float k) const noexcept
			{
				return type(hash(k).hash, shift);
			}
			template<class Hash>
			static SEQ_ALWAYS_INLINE std::uint8_t tiny_hash(Hash hash, float) noexcept
			{
				return static_cast<std::uint8_t>(hash_finalize(hash.hash) & 255u);
			}
		};

		/// @brief Hash function for double type and sorted radix tree
		template<>
		struct SortedHasher<double, void > : BaseSortedHasher
		{
			using type = IntegralHash<std::uint64_t>;
			using const_reference = const type&;
			static constexpr bool prefix_search = true;
			static constexpr bool variable_length = false;
			static constexpr size_t max_bits = sizeof(double) * 8U;
			
			SEQ_ALWAYS_INLINE type hash(double k) const noexcept
			{
				union U {
					double d;
					std::uint64_t u;
				};
				U u = { k };
				// Interpret double as 64-bit unsigned.
				// Flip all except top if top bit is set.
				u.u ^= ((static_cast<std::uint64_t>((static_cast<std::int64_t>(u.u)) >> 63ll)) >> 1ll);
				// Flip top bit.
				u.u ^= (1ull << 63ull);
				return type(u.u);
			}
			SEQ_ALWAYS_INLINE type hash_shift(size_t shift, double k) const noexcept
			{
				return type(hash(k).hash, shift);
			}
			template<class Hash>
			static SEQ_ALWAYS_INLINE std::uint8_t  tiny_hash(Hash hash, double)noexcept
			{
				return static_cast<std::uint8_t>(hash_finalize(static_cast<size_t>(hash.hash)) & 255u);
			}
		};

		/// @brief Hash function for string types and sorted radix tree
		template<>
		struct SortedHasher<tstring_view, void > : BaseSortedHasher
		{
			using type = StringHash;
			using const_reference = const StringHash&;
			static constexpr bool prefix_search = true;
			static constexpr bool variable_length = true;
			static constexpr size_t max_bits = static_cast<size_t>(-1);

			SEQ_ALWAYS_INLINE type hash(const tstring_view& k) const noexcept
			{
				return StringHash(k.data(), k.size());
			}
			SEQ_ALWAYS_INLINE type hash_shift(size_t shift, const tstring_view& k) const noexcept
			{
				return StringHash(shift, k.data(), k.size());
			}
			template<class Hash>
			static SEQ_ALWAYS_INLINE std::uint8_t tiny_hash(const Hash&, const tstring_view & v) noexcept
			{
				return static_cast<std::uint8_t>(hash_bytes_komihash(v.data(), v.size()));
			}

		};

		/// @brief Hash function for pointer and sorted radix tree
		template<class T>
		struct SortedHasher<T, typename std::enable_if<std::is_pointer<T>::value,void>::type> : BaseSortedHasher
		{
			using type = IntegralHash<std::uintptr_t>;
			using const_reference = const type&;
			static constexpr bool prefix_search = true;
			static constexpr bool variable_length = false;
			static constexpr size_t max_bits = sizeof(std::uintptr_t) * 8U;

			SEQ_ALWAYS_INLINE type hash(T p) const noexcept
			{
				return type(reinterpret_cast<std::uintptr_t>(p));
			}
			SEQ_ALWAYS_INLINE type hash_shift(size_t shift, T p) const noexcept
			{
				return type(hash(p).hash, shift);
			}
			template<class Hash>
			static SEQ_ALWAYS_INLINE std::uint8_t tiny_hash(Hash hash, T) noexcept
			{
				return static_cast<std::uint8_t>(hash_finalize(hash.hash) & 255u);
			}
		};
		



		/// @brief Copy count elements from src to dst while destroying elements in src
		/// In case of exception, destroy all elements in in src and dst.
		template<class U>
		static inline void copy_destroy(U* dst, U* src, unsigned count)
		{
			if SEQ_CONSTEXPR(is_relocatable<U>::value)
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


		
		
		
		template<class T>
		SEQ_ALWAYS_INLINE void small_memmove_right(T* dst , const T* src, size_t count)
		{
			if SEQ_CONSTEXPR (sizeof(T) >= 8U ) {
				T* last = dst + count ;
				src += count ;
				while (last != dst) {
					--last; --src;
					memcpy(static_cast<void*>(last), static_cast<const void*>(src),sizeof(T));
				}
			}
			else
				memmove(static_cast<void*>(dst), static_cast<const void*>(src), count * sizeof(T));
		}
		template<>
		SEQ_ALWAYS_INLINE void small_memmove_right(std::uint8_t* dst, const std::uint8_t* src, size_t count)
		{
			memmove(dst, src, count );
		}

		/// @brief Insert element at src position while moving elements to the right at dst.
		/// Basic exception guarantee.
		template <class U,  class Policy, class... Args>
		static void SEQ_ALWAYS_INLINE insert_move_right(U* dst, U* src, unsigned count, Policy , Args&&... args)
		{
			// Move src to the right
			// In case of exception, values are in undefined state, but no new value created (basic exception guarantee)

			if SEQ_CONSTEXPR(is_relocatable<U>::value)
			{
				if (count) {
					//memmove(dst, src, count * sizeof(U));
					small_memmove_right(dst, src, count );
				}
				try {
					Policy::emplace(src, std::forward<Args>(args)...);
				}
				catch (...) {
					// move back to the left
					memmove(static_cast<void*>(src), static_cast<const void*>(dst), count * sizeof(U));
					throw;
				}
			}
			else
			{
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
		template <class U>
		static inline void erase_pos(U* src, unsigned pos, unsigned count)
		{
			if SEQ_CONSTEXPR(is_relocatable<U>::value)
			{
				src[pos].~U();
				memmove(static_cast<void*>(src + pos), static_cast<const void*>(src + pos + 1), (count - pos -1) * sizeof(U));
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

		/// @brief Swiss table like find without SSE
		template<bool UseLowerBound, class ExtractKey, class Equal, class Less, class T, class U >
		static  unsigned find_value8(const Equal& eq, const T* values, const unsigned char* ths, unsigned size, unsigned char th, unsigned* insert_pos, const U& val)
		{
			/*
			// keep this version just in case
			if (!UseLowerBound)
			{
				const unsigned char* end = ths + size;
				const unsigned char* found = (const unsigned char* )memchr(ths, (char)th, end - ths);
				while(found) {

					if (eq(ExtractKey{}(values[found - ths]), val)) return  found - ths;
					found = (const unsigned char* )memchr(found +1, (char)th, end - found -1);
				}
				return -1;
			}
			*/

			unsigned count = size & ~7U;
			std::uint64_t _th;
			memset(&_th, th, sizeof(_th));
			for (unsigned i = 0; i < count; i += 8) {
				std::uint64_t  found = movemask8((read_64(ths + i) ^ _th));
				while (found) {
					unsigned pos = bit_scan_forward_64(found) >> 3;
					if (eq(ExtractKey{}(values[i + pos]), val)) return  i + pos;
					reinterpret_cast<unsigned char*>(&found)[pos] = 0;
				}
				if (UseLowerBound) {
					//not found, check for lower bound
					if (Less{}(val, ExtractKey{}(values[i + 7]))) {
						*insert_pos = i+ LowerBound<T, ExtractKey, Less>::apply(0, values + i, 8U, val);
						return static_cast<unsigned>(-1);
					}
				}
			}

			if (count != size) {
				std::uint64_t  found = movemask8((read_64(ths + count) ^ _th)) & ((1ULL << (size - count) * 8ULL) - 1ULL);
				while (found) {
					unsigned pos = bit_scan_forward_64(found) >> 3;
					if (eq(ExtractKey{}(values[count + pos]), val)) return  count + pos;
					reinterpret_cast<unsigned char*>(&found)[pos] = 0;
				}
			}
			if (UseLowerBound) {
				//not found, check for lower bound
				*insert_pos = size;
				if (size != count && Less{}(val, ExtractKey{}(values[size - 1]))) 
					*insert_pos = count + LowerBound<T, ExtractKey, Less>::apply(0, values + count, size - count, val);
			}
			return static_cast<unsigned>(-1);
		}

		template<bool UseLowerBound, class ExtractKey, class Equal, class Less, class T, class U >
		static SEQ_ALWAYS_INLINE unsigned find_value_AVX2(const Equal& eq, const T* values, const unsigned char* ths, unsigned size, unsigned char th, unsigned* insert_pos, const U& val)
		{
#if defined( __AVX2__)
			
			if(UseLowerBound) *insert_pos = size;
			for (unsigned i = 0; i < size; i += 32) {
				unsigned c = std::min(size - i, 32U);//i + 32 > size ? (size - i) : 32U;
				unsigned found = static_cast<unsigned>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(_mm256_loadu_si256(reinterpret_cast<const __m256i*>(ths + i)), _mm256_set1_epi8(static_cast<char>(th)))));
				if (c != 32U) found &= ((1U << c) - 1U);
				if (found) {
					SEQ_PREFETCH(values + i);
					do {
						unsigned pos = bit_scan_forward_32(found);
						if (eq(ExtractKey{}(values[i + pos]), val)) return  i + pos;
						found = found & ~(1U << pos);
					} while (found);
				}

				if SEQ_CONSTEXPR (UseLowerBound) {
					//not found, check for lower bound
					if (Less{}(val, ExtractKey{}(values[i + c-1]))) {
						*insert_pos = i + LowerBound<T, ExtractKey, Less>::apply(0, values + i, c, val);
						return static_cast<unsigned>(-1);
					}
				}
			}
			
#endif
			return static_cast<unsigned>(-1);
		}

		template<bool UseLowerBound, class ExtractKey, class Equal, class Less, class T, class U >
		static  unsigned find_value_SSE3(const Equal& eq, const T* values, const unsigned char* ths, unsigned size, unsigned char th, unsigned* insert_pos, const U& val)
		{
#if defined( __SSE2__)
			unsigned count = size & ~15U;
			for (unsigned i = 0; i < count; i += 16) {
				unsigned short  found = static_cast<unsigned short>( _mm_movemask_epi8(
					_mm_cmpeq_epi8(_mm_loadu_si128(reinterpret_cast<const __m128i*>(ths + i)), _mm_set1_epi8(static_cast<char>(th)))
				));
				if (found) {
					SEQ_PREFETCH(values + i);
					do {
						unsigned pos = bit_scan_forward_32(found);
						if (eq(ExtractKey{}(values[i + pos]), val)) return  i + pos;
						found = found & ~(1U << pos);
					} while (found);
				}
				if (UseLowerBound) {
					//not found, check for lower bound
					if (Less{}(val, ExtractKey{}(values[i + 15]))) {
						*insert_pos = i + LowerBound<T, ExtractKey, Less>::apply(0, values + i, 16U, val);
						return static_cast<unsigned>(-1);
					}
				}
			}

			if (count != size) {
				unsigned found = static_cast<unsigned short>(_mm_movemask_epi8(
					_mm_cmpeq_epi8(_mm_loadu_si128(reinterpret_cast<const __m128i*>(ths + count)), _mm_set1_epi8(static_cast<char>(th)))
				)) & ((1U << (size - count)) - 1U);
				if (found) {
					SEQ_PREFETCH(values + count);
					do {
						unsigned pos = bit_scan_forward_32(found);
						if (eq(ExtractKey{}(values[count + pos]), val)) return  count + pos;
						found = found & ~(1U << pos);
					} while (found);
				}
			}
			if (UseLowerBound) {
				//not found, check for lower bound
				*insert_pos = size;
				if (size != count && Less{}(val, ExtractKey{}(values[size - 1])))
					*insert_pos = count + LowerBound<T, ExtractKey, Less>::apply(0, values + count, size - count, val);
			}
			
#endif
			return static_cast<unsigned>(-1);
		}

		/// @brief Swiss table like find using AVX2, SSE3, or 8 byte movemask.
		template<bool UseLowerBound, class ExtractKey, class Equal, class Less, class T, class U >
		static SEQ_ALWAYS_INLINE unsigned find_value(const Equal& eq, const T* values, const unsigned char* ths, unsigned size, unsigned char th, unsigned* insert_pos, const U& val)
		{
#if defined( __AVX2__)
			//if (cpu_features().HAS_AVX2) //runtime check
				return find_value_AVX2< UseLowerBound, ExtractKey, Equal, Less>(eq, values, ths, size, th, insert_pos,val);
#endif
#if defined( __SSE2__)
			//if (cpu_features().HAS_SSE3) //runtime check
				return find_value_SSE3< UseLowerBound, ExtractKey, Equal, Less>(eq, values, ths, size, th, insert_pos,val);
#endif
			return find_value8<UseLowerBound, ExtractKey, Equal, Less>(eq, values, ths, size, th, insert_pos, val);
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
			//static constexpr unsigned min_capacity = sizeof(T) == 1 ? 32 : sizeof(T) <= 3 ? 16 : sizeof(T) >= 8 ? 4 : 8;
			static constexpr unsigned min_capacity = sizeof(T) == 1 ? 32 : sizeof(T) <= 3 ? 16 : sizeof(T) <= 8 ? 8 : sizeof(T) <= 16 ? 4 : 2;
			// maximum capacity (and size), lower for sorted elements
			static constexpr unsigned max_capacity = Sorted ? 64 : 96;
			
			// returns size of header and tiny hash values
			static unsigned hash_for_size(unsigned /*size*/, unsigned capacity) noexcept {
				return header_size + capacity;
			}
			// returns the capacity for a given size
			static unsigned capacity_for_size(unsigned size) noexcept
			{
				//return (size / min_capacity) * min_capacity + (size % min_capacity ? min_capacity : 0);
				if (size <= min_capacity)
					return min_capacity;
				unsigned bits = bit_scan_reverse_32(size);
				unsigned cap = 1U << bits;
				if (cap < size) {
					cap *= 2;
				}
				if(HasMaxCapacity && cap > max_capacity)
					cap = max_capacity;
				return cap;

				/*if (size <= 4) return 4;
				if (size <= 8) return 8;
				return (size / 16) * 16 + (size % 16 ? 16 : 0);
				if (size <= 16) return 16;
				if (size <= 32) return 32;
				if (size <= 48) return 48;
				if (size <= 64) return 64;
				if (size <= 80) return 80;
				if (size <= 96) return 96;
				if (size <= 112) return 112;
				if (size <= 128) return 128;*/

				
			}

			bool full() const noexcept{return count() >= max_capacity;}
			std::uint32_t* toint32() noexcept { return reinterpret_cast<std::uint32_t*>(this); }
			const std::uint32_t* toint32() const noexcept { return reinterpret_cast<const std::uint32_t*>(this); }
			std::uint32_t* size() noexcept { return toint32(); }
			const std::uint32_t* size() const noexcept { return toint32(); }
			std::uint32_t* capacity() noexcept { return toint32() + 1; }
			const std::uint32_t* capacity() const noexcept { return toint32() + 1; }
			const std::uint8_t* hashs() const noexcept { return  reinterpret_cast<const std::uint8_t*>(toint32() + 2); }
			std::uint8_t* hashs() noexcept { return  reinterpret_cast<std::uint8_t*>(toint32() + 2); }
			unsigned count() const noexcept { return *size(); }
			T* values() noexcept { return reinterpret_cast<T*>(hashs() + *capacity()); }
			const T* values() const noexcept { return reinterpret_cast<const T*>(hashs() + *capacity()) ; }
			const T& back() const noexcept { return values()[count() - 1]; }
			std::uint8_t get_tiny_hash(unsigned pos) const { return hashs()[pos]; }

			// Compute tiny hash
			template<class Hasher, class HashType, class U >
			static std::uint8_t tiny_hash(HashType h, const U& val) noexcept { return Hasher::tiny_hash(h, val);}
			// Returns lower bound for sorted leaf only
			template<class ExtractKey, class Less, class Equal, class K >
			unsigned lower_bound(size_t start_bit, unsigned , const K & key) const
			{
				return LowerBound<T, ExtractKey, Less>::apply(start_bit, values(), count(), key);
				//unsigned insert_pos = -1;
				//unsigned pos = find_value<true, ExtractKey, Equal, Less>(Equal{}, values(), hashs(), count(), th, &insert_pos, std::forward<Args>(args)...);
				//return pos == -1 ? insert_pos : pos;
			}
			// Check if given value already exists, and return insertion position for sorted leaf only
			template<bool EnsureSorted, class ExtractKey, class Equal, class Less, class U >
			SEQ_ALWAYS_INLINE std::pair<const T*, unsigned> find_insert(const Equal& eq, size_t /*start_bit*/, unsigned th, const U& val) const
			{
				using key_type = typename ExtractKeyResultType< ExtractKey, T>::type;
				if SEQ_CONSTEXPR(Sorted && EnsureSorted && std::is_arithmetic<key_type>::value)
				{
					// For arithmetic keys, checking bounds is cheap and helps a lot for ordered insertions
					if (Less{}(ExtractKey{}(values()[count() - 1]), (val)))
						return std::pair<const T*, unsigned>(nullptr, count());
					if (Less{}((val), ExtractKey{}(values()[0])))
						return std::pair<const T*, unsigned>(nullptr, 0);
				}
				unsigned insert_pos = static_cast<unsigned>(-1);
				unsigned pos = find_value<(Sorted && EnsureSorted),ExtractKey,Equal,Less>(eq, values(), hashs(), count(), static_cast<std::uint8_t>(th), &insert_pos, val);
				return std::pair<const T*, unsigned>(pos == static_cast<unsigned>(-1) ? nullptr : values() + pos, insert_pos);
			}
			// Returns value index, -1 if not found
			template<class ExtractKey, class Equal, class Less, class K >
			SEQ_ALWAYS_INLINE unsigned find(const Equal& eq, size_t /*start_bit*/, std::uint8_t th, const K& key) const
			{
				return find_value<false,ExtractKey,Equal,Less>(eq, values(), hashs(), count(), th, nullptr, key);
			}
			template<class ExtractKey, class Equal, class K >
			SEQ_ALWAYS_INLINE unsigned find(const Equal& eq, std::uint8_t th, const K& key) const
			{
				return find_value<false, ExtractKey, Equal, default_less>(eq, values(), hashs(), count(), th, nullptr, key);
			}
			// Sort leaf
			template<class ExtractKey, class Less>
			void sort(Less le)
			{
				insertion_sort<T,ExtractKey>(values(), hashs(), 0, static_cast<int>(count()), le);
			}
			// Reallocate leaf on insertion
			template <class NodeAllocator, class Policy, class... Args>
			std::pair<LeafNode*, unsigned> switch_buffer(NodeAllocator& al, unsigned old_size, unsigned pos, std::uint8_t th, Policy , Args&&... args)
			{
				unsigned new_capacity = capacity_for_size(old_size + 1);
				// might throw, fine
				LeafNode* n = al.allocate(hash_for_size(old_size + 1, new_capacity), new_capacity);
				*n->size() = this->count();
				*n->capacity() = new_capacity;
				try
				{
					// might throw
					Policy::emplace( (n->values() + pos), std::forward<Args>(args)...);
					n->hashs()[pos] = th;
					try
					{
						if (Sorted && old_size != pos)
						{
							copy_destroy(n->hashs(), this->hashs(), pos);
							copy_destroy(n->hashs() + pos + 1, this->hashs() + pos, (old_size - pos));

							// both calls might throw
							copy_destroy(n->values(), this->values(), pos);
							copy_destroy(n->values() + pos + 1, this->values() + pos, (old_size - pos));
						}
						else
						{
							copy_destroy(n->hashs(), this->hashs(), old_size);
							// both calls might throw
							copy_destroy(n->values(), this->values(), old_size);
						}
					}
					catch (...)
					{
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
			template <class ExtractKey, class Less, class NodeAllocator, class Policy, class K, class... Args>
			SEQ_ALWAYS_INLINE std::pair<LeafNode*, unsigned> insert(NodeAllocator& al, size_t start_bit, unsigned pos, std::uint8_t th, Policy p, K && key, Args&&... args)
			{
				const unsigned size = count();

				if SEQ_CONSTEXPR(Sorted) {
					if (pos > size) 
						pos = LowerBound<T, ExtractKey, Less>::apply(start_bit, values(), count(), key);
				}
				else
					pos = size;
				

				if (SEQ_UNLIKELY(*capacity() == size)) {
					// might throw, fine
					auto res = switch_buffer(al, size, pos, th, p, std::forward<K>(key), std::forward<Args>(args)...);
					(*res.first->size())++;
					return res;
				}

				if (Sorted && pos != size)
				{
					insert_move_right(hashs() + pos + 1, hashs() + pos, (size - pos), EmplacePolicy{}, th);
					insert_move_right(values() + pos + 1, values() + pos, (size - pos), p, std::forward<K>(key), std::forward<Args>(args)...);
				}
				else
				{
					Policy::emplace((values() + size), std::forward<K>(key), std::forward<Args>(args)...);
					hashs()[size] = th;
				}

				(*this->size())++;
				return std::pair<LeafNode*, unsigned>(this, pos);
			}

			template <class NodeAllocator>
			LeafNode* erase(NodeAllocator& al, unsigned pos)
			{
				erase_pos(values(), pos, *size());
				erase_pos(hashs(), pos, *size());
				(*size())--;

				if (*size() == 0) {
					unsigned cap = capacity_for_size(1);
					al.deallocate(this,hash_for_size(1, cap), cap);
					return nullptr;
				}

				unsigned cap = capacity_for_size(*size());
				if (cap != *capacity()) {
					// might throw, fine
					LeafNode* n = al.allocate(hash_for_size(*size(), cap), cap);
					*n->size() = *this->size();
					*n->capacity() = cap;

					try
					{
						copy_destroy(n->values(), values(), *size());
						copy_destroy(n->hashs(), this->hashs(), *size());
					}
					catch (...)
					{
						al.deallocate(n, hash_for_size(*size(), cap), cap);
						throw;
					}

					al.deallocate(this, hash_for_size(*size() + 1, *capacity()), *capacity());

					return n;
				}
				return this;
			}

			// Create node with one value
			template <class NodeAllocator, class Policy, class... Args>
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

				return  std::pair<LeafNode*, T*>(tmp, p);
			}

			// Destroy node
			template <class NodeAllocator>
			static void destroy(NodeAllocator& alloc, LeafNode* node)
			{
				unsigned size = node->count();

				//destroy values
				if SEQ_CONSTEXPR(!std::is_trivially_destructible<T>::value)
				{
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
			ChildPtr() :base() {}
			ChildPtr(void* ptr) noexcept :base(ptr) {}
			ChildPtr(void* ptr, std::uintptr_t t) noexcept :base(ptr, t) {}
			const Dir* to_dir() const noexcept {return static_cast<const Dir*>(this->ptr());}
			const Node* to_node() const noexcept {return static_cast<const Node*>(this->ptr());}
			const Vector* to_vector() const noexcept { return static_cast<const Vector*>(this->ptr()); }
			Dir* to_dir() noexcept { return static_cast<Dir*>(this->ptr()); }
			Node* to_node() noexcept { return static_cast<Node*>(this->ptr()); }
			Vector* to_vector() noexcept { return static_cast<Vector*>(this->ptr()); }
			unsigned size() const { return this->tag() == Dir::IsLeaf ? to_node()->count() : to_vector()->size(); }
			const typename Node::value_type & front() const { return this->tag() == Dir::IsLeaf ? *to_node()->values() : to_vector()->front(); }
		};


		/// @brief Directory within a radix tree.
		/// A directory stores a power of 2 number of children that could be either directories, leaves or vectors.
		template<class T, class Node, class Vector, bool VariableLength>
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
			static constexpr unsigned alloc_size = sizeof(std::uint64_t);

			unsigned hash_len : 5; // hash len in bits, size = 1 << hash_len
			unsigned dir_count : 27; // number of directories inside
			unsigned child_count; // total number of children
			unsigned parent_pos; //position within parent directory
			unsigned first_valid_child; // position of the first valid child, if possible a leaf node.

			size_t prefix_len;//prefix len in bits.
			Directory* parent; //parent directory (if not root leaf)

			SEQ_ALWAYS_INLINE const T& any_child() const noexcept
			{
				SEQ_ASSERT_DEBUG(first_valid_child < size(), "");
				child_ptr ch = const_child(first_valid_child);
				while (ch.tag() == IsDir)
					ch = ch.to_dir()->const_child(ch.to_dir()->first_valid_child);

				if(ch.tag() == IsLeaf)
					return  ch.to_node()->back();
				return ch.to_vector()->back();
			}
			/// @brief Recompute the first valid position within the directory, and if possible a leaf node
			void compute_first_valid() noexcept
			{
				//
				first_valid_child = static_cast<unsigned>(-1);
				for (unsigned i = 0; i < size(); ++i) {
					if (const_child(i).tag() == IsLeaf) {
						first_valid_child = i;
						break;
					}
					else if(const_child(i).tag() && first_valid_child == static_cast<unsigned>(-1))
						first_valid_child = i;
				}
			}
			/// @brief Returns the full size of the directory
			unsigned size() const noexcept {return  (1U << hash_len);}
			/// @brief Returns the children pointer
			child_ptr* children() noexcept {return (reinterpret_cast<child_ptr*>(this + 1));}
			const child_ptr* children() const noexcept {return (reinterpret_cast<const child_ptr*>(this + 1));}
			/// @brief Returns child at given position
			child_ptr& child(unsigned pos) noexcept {return children()[pos];}
			child_ptr const_child(unsigned pos) const noexcept {return children()[pos];}
			/// @brief Allocate, initialize and return a directory with given bit length (log2(size))
			template<class NodeAllocator>
			static Directory* make(NodeAllocator& alloc, unsigned hash_len)
			{
				Directory* dir = alloc.allocate_dir(hash_len);
				dir->hash_len = hash_len;
				return dir;
			}
			/// @brief Destroy and deallocate directory recursively
			template<class NodeAllocator>
			static void destroy(NodeAllocator& alloc, Directory* dir, bool recurse = true)
			{
				//recursively destroy and deallocate
				if (recurse) {
					unsigned size = dir->size();
					for (unsigned i = 0; i < size; ++i)
					{
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

			size_t size; // tree size
			directory* root; // root directory

			//NodeAllocator() : size(0),  root(nullptr) {  }
			NodeAllocator(const Allocator& al) :Allocator(al), size(0), root(nullptr) {}
			~NodeAllocator() {}

			Allocator& get_allocator() noexcept { return *this; }
			const Allocator& get_allocator() const noexcept { return *this; }

			/// @brief Allocate and construct a vector
			vector_type* make_vector()
			{
				RebindAlloc<Allocator, vector_type> al = get_allocator();
				vector_type* res = al.allocate(1);
				try {
					construct_ptr(res, get_allocator());
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

				RebindAlloc<Allocator, std::uint64_t> al = get_allocator();;
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

				RebindAlloc<Allocator, std::uint64_t> al = get_allocator();;
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
				memset(dir, 0, to_alloc * sizeof(std::uint64_t));

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
			size_t bit_pos; //bit position of the directory
			unsigned child; //node position in directory
			unsigned node_pos; //position within node

			const node* to_node() const noexcept { return static_cast<const node*>(dir->const_child(child).ptr()); }
			const VectorType* to_vector() const noexcept { return static_cast<const VectorType*>(dir->const_child(child).ptr()); }
		
			/// @brief Returns the bit position of given directory
			static size_t get_bit_pos(const Dir* dir) noexcept
			{
				const Dir* d = dir;
				size_t bit_pos = 0;
				while (d->parent)
				{
					bit_pos += d->parent->hash_len + d->parent->prefix_len;
					d = d->parent;
				}
				return bit_pos + dir->prefix_len;
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

			static PosInDir  find_next(const Dir* current, unsigned current_pos, size_t bit_pos) noexcept
			{
				//for (;;) 
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
						return PosInDir{ const_cast<Dir*>(current),current_pos,bit_pos };

					if (found.tag() == Dir::IsDir) {


						//we found a directory: investigate it
						auto tmp = find_next(found.to_dir(), 0, bit_pos + current->hash_len + found.to_dir()->prefix_len);
						if (tmp.dir)
							return tmp; //we found a node in this directory
						//current_pos = 0;
						//bit_pos += current->hash_len + found.to_dir()->prefix_len;
						//current = found.to_dir();
						//continue;
					}

					//we reach the end of this directory: try in parent
					if (current->parent && current)
					{
						return find_next(current->parent, current->parent_pos + 1, bit_pos - current->prefix_len - current->parent->hash_len);
						//bit_pos -= current->prefix_len + current->parent->hash_len;
						//current_pos = current->parent_pos + 1;
						//current = current->parent;
						//continue;
					}

					return { nullptr, 0, 0 }; // end of iteration
				}
			}

			static PosInDir  find_prev(const Dir* current, unsigned current_pos, size_t bit_pos) noexcept
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

					//we found a directory: investigate it
					auto tmp = find_prev(found.to_dir(), found.to_dir()->size(), bit_pos + current->hash_len + found.to_dir()->prefix_len);
					if (tmp.dir)
						return tmp; //we found a node in this directory
				}

				//we reach the end of this directory: try in parent
				if (current->parent )
					return find_prev(current->parent, current->parent_pos - 1, bit_pos - current->prefix_len - current->parent->hash_len);

				return { nullptr, 0,0 }; // end of iteration
			}

			auto next() noexcept -> RadixConstIter&
			{
				if (dir == data->end.dir && child == data->end.child)
				{
					dir = nullptr;
					child = 0;
					node_pos = 0;
					return *this; // end of iteration
				}
				
				auto tmp = find_next(dir, child + 1, bit_pos);
				SEQ_ASSERT_DEBUG(tmp.dir, "");
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
				auto tmp = find_prev(dir, child - 1 ,bit_pos);
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
		 
			RadixConstIter(const Data * dt, const Dir* d , unsigned c , unsigned np , size_t bp) noexcept
				: data(const_cast<Data*>(dt)), dir(const_cast<Dir*>(d)), bit_pos(bp), child(c), node_pos(np) {}
			RadixConstIter(const Data* dt = nullptr) noexcept//end
				: data(const_cast<Data*>(dt)), dir(nullptr), bit_pos(0), child(0), node_pos(0){}

			bool is_null() const noexcept { return dir == nullptr; }

			auto get() const noexcept -> reference {
				SEQ_ASSERT_DEBUG(dir, "dereferencing null iterator");
				return const_cast<value_type&>(dir->const_child(child).tag() == Dir::IsVector ?
					to_vector()->at(node_pos) :
					to_node()->values()[node_pos]);
			}
			
			auto operator*() const noexcept -> reference {return get();}
			auto operator->() const noexcept -> pointer {return std::pointer_traits<pointer>::pointer_to(**this);}

			SEQ_ALWAYS_INLINE auto operator++() noexcept -> RadixConstIter& 
			{
				SEQ_ASSERT_DEBUG(data, "");
				
				++node_pos;
				if (node_pos != (dir->const_child(child).tag() == Dir::IsVector ? to_vector()->size() : to_node()->count()))
					return *this;
				
				return next();
			}
			SEQ_ALWAYS_INLINE auto operator++(int) noexcept -> RadixConstIter {
				RadixConstIter _Tmp = *this;
				++(*this);
				return _Tmp;
			}
			SEQ_ALWAYS_INLINE auto operator--() noexcept -> RadixConstIter&
			{
				SEQ_ASSERT_DEBUG(data, "");
				if (SEQ_UNLIKELY(dir == nullptr)) {
					//end iterator
					dir = data->end.dir;
					child = data->end.child;
					bit_pos = data->end.bit_pos;
					node_pos = dir->const_child(child).tag() == Dir::IsVector ? static_cast<unsigned>(to_vector()->size()) - 1 : to_node()->count() - 1;
					return *this;
				}
				--node_pos;
				if (node_pos == static_cast<unsigned>(-1))
					return prev();
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator--(int) noexcept -> RadixConstIter {
				RadixConstIter _Tmp = *this;
				--(*this);
				return _Tmp;
			}
			SEQ_ALWAYS_INLINE bool operator==(const RadixConstIter& other) const noexcept {
				SEQ_ASSERT_DEBUG(data == other.data, "comparing iterators from different radix trees");
				return dir == other.dir && child == other.child && node_pos == other.node_pos;
			}
			SEQ_ALWAYS_INLINE bool operator!=(const RadixConstIter& other) const noexcept {
				SEQ_ASSERT_DEBUG(data == other.data, "comparing iterators from different radix trees");
				return dir != other.dir || child != other.child || node_pos != other.node_pos;
			}
		};

		inline void check_vector_size(size_t size)
		{
			// For vector nodes, the size is limited to (unsigned)-1 since the highest values is reserved for the radix iterator
			if (size == std::numeric_limits<unsigned>::max() -1)
				throw std::out_of_range("Vector node size is limited to 32 bits");
		}

		/// @brief Less functor used by vector nodes
		template<class Hasher, class ExtractKey, class Key, class T >//, bool Convertible = std::is_convertible<T,Key>::value>
		struct VectorLess
		{
			using is_transparent = int;
			/*bool operator()(const Key& a, const T& b) const { return Hasher::less(a, ExtractKey{}(b)); }
			bool operator()(const T& a, const Key& b) const {return Hasher::less(ExtractKey{}(a), b);}
			bool operator()(const Key& a, const Key& b) const { return Hasher::less(a, b); }
			bool operator()(const T& a, const T& b) const { return Hasher::less(ExtractKey{}(a), ExtractKey{}(b)); }*/
			template<class U, class V>
			bool operator()(const U& a, const V& b) const { return Hasher::less(ExtractKey{}(a), ExtractKey{}(b)); }
		};
		/*template<class Hasher, class ExtractKey, class Key, class T>
		struct VectorLess< Hasher, ExtractKey,Key,T,true>
		{
			using is_transparent = int;
			bool operator()(const Key& a, const Key& b) const {return Hasher::less(a, b);}
			template<class U, class V>
			bool operator()(const U& a, const V& b) const { return Hasher::less(ExtractKey{}(a), ExtractKey{}(b)); }
		};*/
		/// @brief Equal functor used by vector nodes
		template<class Hasher, class ExtractKey, class Key, class T>//, bool Convertible = std::is_convertible<T, Key>::value>
		struct VectorEqual
		{
			using is_transparent = int;
			/*bool operator()(const Key& a, const T& b) const { return Hasher::equal(a, ExtractKey{}(b)); }
			bool operator()(const T& a, const Key& b) const { return Hasher::equal(ExtractKey{}(a), b); }
			bool operator()(const Key& a, const Key& b) const { return Hasher::equal(a, b); }
			bool operator()(const T& a, const T& b) const { return Hasher::equal(ExtractKey{}(a), ExtractKey{}(b)); }*/
			template<class U, class V>
			bool operator()(const U& a, const V& b) const { return Hasher::equal(ExtractKey{}(a), ExtractKey{}(b)); }
		};
		/*template<class Hasher, class ExtractKey, class Key, class T>
		struct VectorEqual< Hasher, ExtractKey, Key, T, true>
		{
			using is_transparent = int;
			bool operator()(const Key& a, const Key& b) const { return Hasher::equal(a, b); }
			template<class U, class V>
			bool operator()(const U& a, const V& b) const { return Hasher::equal(a, b); }
		};*/


		/// @brief Sorted vector node based on seq::flat_set
		template<class T, class Hasher, class ExtractKey, class Allocator, bool sorted = true>
		struct VectorNode
		{
			using key_type = typename ExtractKeyResultType< ExtractKey, T>::type;
			using value_type = T;
			using Less = VectorLess<Hasher, ExtractKey, key_type, T>;
			
			flat_set<T, Less, Allocator> set;

			VectorNode(const Allocator& al)
				:set(al) {}

			size_t size() const noexcept {return set.size();}
			const T& front() const noexcept { return set.pos(0); }
			const T& back() const noexcept { return set.pos(set.size() - 1);}
			template< class... Args >
			std::pair<size_t,bool> emplace(Args&&... args)
			{
				check_vector_size(size());
				return set.emplace_pos(std::forward<Args>(args)...);
			}
			template< class... Args >
			std::pair<size_t, bool> emplace_no_check(Args&&... args)
			{
				check_vector_size(size());
				return set.emplace_pos(std::forward<Args>(args)...);
			}
			void erase(size_t pos)
			{
				set.erase_pos(pos);
			}
			template< class K>
			size_t find(const K & key) const
			{
				return set.find_pos(key);
			}
			template< class K >
			size_t lower_bound(const K& key) const
			{
				return set.lower_bound_pos(key);
			}
			template< class K >
			size_t upper_bound(const K& key) const
			{
				return set.upper_bound_pos(key);
			}
			const T& at(size_t pos) const 
			{ 
				return set.pos(pos); 
			}
		};

		/// @brief Unsorted vector node based on seq::dvector
		template<class T, class Hasher, class ExtractKey, class Allocator>
		struct VectorNode<T, Hasher, ExtractKey, Allocator,false>
		{
			using key_type = typename ExtractKeyResultType< ExtractKey, T>::type;
			using value_type = T;
			using Equal = VectorEqual<Hasher, ExtractKey, key_type, T>;

			seq::devector<T, Allocator,seq::OptimizeForPushBack> vector;

			VectorNode(const Allocator& al)
				:vector(al) {}

			size_t size() const noexcept {return vector.size();}
			const T& front() const noexcept { return vector.front(); }
			const T& back() const noexcept { return vector.back(); }
			template<class K , class... Args >
			std::pair<size_t, bool> emplace(K && key, Args&&... args)
			{
				check_vector_size(size());
				size_t found = find(key);
				if (found == size()) {
					vector.emplace_back(std::forward<K>(key), std::forward<Args>(args)...);
					return std::pair<size_t, bool>( vector.size() - 1,true);
				}
				return std::pair<size_t, bool>(found,false);
			}
			template< class... Args >
			std::pair<size_t, bool> emplace_no_check(Args&&... args)
			{
				check_vector_size(size());
				vector.emplace_back(std::forward<Args>(args)...);
				return std::pair<size_t, bool>(vector.size() - 1, true);
			}
			void erase(size_t pos)
			{
				vector.erase(vector.begin() + pos);
			}
			template< class K>
			size_t find(const K & key) const
			{
				for (size_t i = 0; i < vector.size(); ++i)
					if (Equal{}(vector[i], key))
						return i;
				return vector.size();
			}
			template< class... Args >
			size_t lower_bound(Args&&... args) const noexcept
			{
				return vector.size();
			}
			template< class... Args >
			size_t upper_bound(Args&&... args) const noexcept
			{
				return vector.size();
			}
			const T& at(size_t pos) const noexcept
			{
				return vector[pos];
			}
		};



		/// @brief Root of a radix tree
		template<class Allocator, class T, class Directory, class Node>
		struct RootTree : public NodeAllocator<Allocator, T, Directory, Node>
		{
			using base_type = NodeAllocator<Allocator, T, Directory, Node>;
			using directory = Directory;
			using node = Node;

			RootTree(const Allocator& al):base_type(al) {}
		};


		/// @brief Store a leaf position for begin and last leaves in a radix tree
		template<class Dir>
		struct BeginEndDir
		{
			using value_type = typename Dir::value_type;
			Dir* dir;
			unsigned child;
			size_t bit_pos;
			BeginEndDir() noexcept : dir(nullptr), child(0), bit_pos(0){}
			void reset() noexcept {
				dir = nullptr;
				child = 0;
			}
			bool is_null() const noexcept {
				return dir == nullptr;
			}
			const value_type& get_value() const
			{
				auto ch = this->dir->children()[this->child];
				return ch.tag() == Dir::IsLeaf ? *ch.to_node()->values() : ch.to_vector()->front();
			}
			const value_type& get_back_value() const
			{
				auto ch = this->dir->children()[this->child];
				return ch.tag() == Dir::IsLeaf ? ch.to_node()->back() : ch.to_vector()->back();
			}
		};
		/// @brief Store a leaf position for begin and last leaves in a sorted radix tree
		template<class Dir,class Hasher, class ExtractKey, class Less, class KeyType, bool IsSorted, class =void>
		struct StoreMinMaxKey : public BeginEndDir<Dir>
		{
			template<class Key, class Hash>
			void store_value(Dir* d, unsigned c, size_t bp, const Key& , const Hash& )
			{
				this->dir = d;
				this->child = c;
				this->bit_pos = bp;
			}
			void store_value(Dir* d, unsigned c, size_t bp, const Hasher& )
			{
				this->dir = d;
				this->child = c;
				this->bit_pos = bp;
			}
			template<class Hash, class U>
			SEQ_ALWAYS_INLINE bool less_than(const Hash& , const U& value) const {
				return Less{}(ExtractKey{}(this->get_value()), ExtractKey{}(value));
			}
			template<class Hash, class U>
			SEQ_ALWAYS_INLINE bool greater_than(const Hash& , const U& value) const {
				return Less{}( ExtractKey{}(value), ExtractKey{}(this->get_value()));
			}
		};
		/// @brief Store a leaf position for begin and last leaves in an unsorted radix tree
		template<class Dir, class Hasher, class ExtractKey, class Less, class KeyType>
		struct StoreMinMaxKey<Dir, Hasher, ExtractKey, Less,  KeyType, false, void> : public BeginEndDir<Dir>
		{
			size_t hash_value;
			template<class Key, class Hash>
			void store_value( Dir* d, unsigned c, size_t bp, const Key& , const Hash& h)
			{
				this->dir = d;
				this->child = c;
				this->bit_pos = bp;
				this->hash_value = h.hash;
			}
			void store_value(Dir* d, unsigned c, size_t bp, const Hasher& h)
			{
				this->dir = d;
				this->child = c;
				this->bit_pos = bp;
				this->hash_value = h.hash(ExtractKey{}(this->get_value())).hash;
			}
			template<class Hash, class U>
			SEQ_ALWAYS_INLINE bool less_than(const Hash& h, const U& ) const {
				return this->hash_value < h.hash;
			}
			template<class Hash, class U>
			SEQ_ALWAYS_INLINE bool greater_than(const Hash& h, const U& ) const {
				return this->hash_value > h.hash;
			}
		};
		/// @brief Store a leaf position for begin and last leaves in a sorted radix tree with arithmetic keys
		template<class Dir, class Hasher, class ExtractKey, class Less, class KeyType>
		struct StoreMinMaxKey<Dir, Hasher, ExtractKey, Less, KeyType, true, typename std::enable_if< std::is_arithmetic<KeyType>::value, void>::type > : public BeginEndDir<Dir>
		{
			KeyType value;
			template<class Key, class Hash>
			void store_value(Dir* d, unsigned c, size_t bp, const Key& k, const Hash&)
			{
				this->dir = d;
				this->child = c;
				this->bit_pos = bp;
				this->value = ExtractKey{}(k);
			}
			void store_value(Dir* d, unsigned c, size_t bp, const Hasher& )
			{
				this->dir = d;
				this->child = c;
				this->bit_pos = bp;
				this->value = ExtractKey{}(this->get_value());
			}
			template<class Hash, class U>
			SEQ_ALWAYS_INLINE bool less_than(const Hash&, const U& v) const {return this->value < ExtractKey{}(v);}
			template<class Hash, class U>
			SEQ_ALWAYS_INLINE bool greater_than(const Hash&, const U& v) const {return this->value > ExtractKey{}(v);}
		};


		template <class T, class ExtractKey>
		struct ExtractWrapper
		{
			using key_type = typename ExtractKeyResultType< ExtractKey, T>::type;

			template<class U>
			typename ExtractKeyResultType< ExtractKey, T>::type operator()(const U& value) const { return ExtractKey{}(value); }
			const key_type & operator()( const key_type& value) const { return value; }
		};

		/// @brief Radix tree container using Variable Arity Radix Tree
		/// @tparam T value type
		/// @tparam Allocator allocator type
		/// @tparam NodeType leaf node type
		/// @tparam Hash Hasher type, either Hasher or SortedHasher
		/// @tparam ExtractKey extract key from value type
		template<
			class T, 
			class Hash,  
			class Extract = default_key<T>,
			class Allocator = std::allocator<T>, 
			class NodeType = LeafNode<T>, 
			unsigned StartArity = Hash::variable_length ? 4 : 2, // 1 to start as a binary tree
			unsigned MaxDepth = 16
		>
		struct RadixTree : public Hash, public Allocator
		{
			static constexpr unsigned start_arity = StartArity;
			static constexpr bool prefix_search = Hash::prefix_search;
			static constexpr bool variable_length = Hash::variable_length;

			static constexpr size_t minus_one_size_t = static_cast<size_t>(-1);

			using ExtractKey = ExtractWrapper<T, Extract>;

			using node = NodeType;
			using key_type = typename ExtractKeyResultType< ExtractKey, T>::type;
			using vector_less_type = typename Hash::less_type;
			using vector_type = VectorNode<T, Hash, ExtractKey, Allocator, !std::is_same<vector_less_type, default_less>::value >;
			using directory = Directory<T, NodeType, vector_type, variable_length>;
			using child_ptr = typename directory::child_ptr;
			using root_type = RootTree<Allocator, T, directory, NodeType>;
			using this_type = RadixTree<T, Hash, ExtractKey, Allocator, NodeType, StartArity, MaxDepth>;

			template<class U>
			using RebindAlloc = typename std::allocator_traits<Allocator>::template rebind_alloc<U>;

			/// @brief Equal structure operating on keys
			using Less = VectorLess<Hash, ExtractKey, key_type, T>;
			/// @brief Less structure operating on keys
			using Equal = VectorEqual<Hash, ExtractKey, key_type, T>;
			
			
			/// @brief Store position of begin and end leaves
			using MinMaxPos = StoreMinMaxKey<directory,Hash, ExtractKey, Less, key_type, node::is_sorted>;  
			
			/// @brief Internal data
			struct PrivateData
			{
				root_type base;
				MinMaxPos begin;
				MinMaxPos end;
				PrivateData(const Allocator& al, unsigned start_len = start_arity)
					:base(al)
				{
					base.root = directory::make(base, start_len);
				}
				~PrivateData() 
				{
					directory::destroy(base, base.root);
				}
			};

			/// @brief Returns an empty directory used to initialize a radix tree
			static directory* get_null_dir() noexcept
			{
				struct null_dir
				{
					directory dir;
					typename directory::child_ptr child;
					null_dir() {
						dir.hash_len = dir.dir_count = dir.child_count = dir.first_valid_child = 0;
					}
				};
				static null_dir inst;
				return &inst.dir;
			}

			/// @brief Destroy internal data and reset root
			void destroy_data()
			{
				if (!d_data)
					return;
				RebindAlloc< PrivateData> al = get_allocator();
				destroy_ptr(d_data);
				al.deallocate(d_data, 1);
				d_data = nullptr;
				d_root = get_null_dir();
			}

			/// @brief Allocate and construct internal data with given aruty
			void make_data(unsigned start_len = start_arity)
			{
				if (d_data)
					return;
				RebindAlloc< PrivateData> al = get_allocator();
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

			/// @brief Reset begin/end position in order to be recomputed
			void reset_ends()
			{
				if (!d_data)
					return;
				d_data->begin.reset();
				d_data->end.reset();
			}

			/// @brief Compute the begin/end position
			void compute_ends() noexcept
			{
				reset_ends();
				if (size() == 0)
					return;
				compute_begin();
				compute_end();
			}
			/// @brief Compute begin leaf position
			void compute_begin()//directory* updated_dir, unsigned updated_child)
			{
				//if (!updated_dir || !d_data->begin.dir || (d_data->begin.dir == updated_dir && updated_child < d_data->begin.child)) {
					auto tmp = const_iterator::find_next(d_data->base.root, 0,0);
					SEQ_ASSERT_DEBUG(tmp.dir, "");
					d_data->begin.store_value(tmp.dir, tmp.child, tmp.bit_pos, *this);
				//}
			}
			/// @brief Compute end leaf position
			void compute_end()//directory* updated_dir, unsigned updated_child)
			{
				//if (!updated_dir || !d_data->end.dir || (d_data->end.dir == updated_dir && updated_child > d_data->end.child)) {
					auto tmp = const_iterator::find_prev(d_data->base.root, d_data->base.root->size(),0);
					SEQ_ASSERT_DEBUG(tmp.dir, "");
					d_data->end.store_value(tmp.dir, tmp.child, tmp.bit_pos, *this);
				//}
			}

			
		public:
			using value_type = T;
			using hash_type = typename Hash::type;
			using const_hash_ref = typename Hash::const_reference;
			using extract_key_type = ExtractKey;
			using const_iterator = RadixConstIter<T, PrivateData, directory, vector_type>;
			using iterator = const_iterator;
			
			PrivateData *d_data;
			directory* d_root;

			// Constructors

			RadixTree(const Allocator & al = Allocator())
				: Allocator(al), d_data(nullptr), d_root(get_null_dir())
			{}

			RadixTree(const Hash & h, const Allocator& al = Allocator())
				: Hash(h), Allocator(al), d_data(nullptr), d_root(get_null_dir())
			{}

			RadixTree(const RadixTree & other)
				: Hash(other), Allocator(copy_allocator(other.get_allocator())), d_data(nullptr), d_root(get_null_dir())
			{
				if(other.size())
					insert(other.begin(), other.end(),false);
			}
			RadixTree(const RadixTree& other, const Allocator & al)
				: Hash(other), Allocator(al), d_data(nullptr), d_root(get_null_dir())
			{
				if (other.size())
					insert(other.begin(), other.end(),false);
			}

			RadixTree( RadixTree&& other)
				noexcept(std::is_nothrow_move_constructible<Allocator>::value && std::is_nothrow_copy_constructible<Hash>::value )
				: Hash(other), Allocator(std::move(other.get_allocator())), d_data(nullptr), d_root(get_null_dir())
			{
				swap(other,false);
			}
			RadixTree(RadixTree&& other, const Allocator& alloc)
				: Hash(other), Allocator(alloc), d_data(nullptr), d_root(get_null_dir())
			{
				if (alloc == other.get_allocator()) 
					swap(other,false);
				else if(other.size())
					insert(make_move_iterator(other.begin()), make_move_iterator(other.end()),false);
			}

			template<class Iter>
			RadixTree(Iter first, Iter last, const Allocator& alloc = Allocator())
				: Allocator(alloc), d_data(nullptr), d_root(get_null_dir())
			{
				insert(first, last);
			}

			/// Destructor
			~RadixTree()
			{
				clear();
			}

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

			const_iterator begin() const noexcept { return size() ? const_iterator(d_data, d_data->begin.dir, d_data->begin.child, 0, d_data->begin.bit_pos) : const_iterator(d_data); }
			const_iterator end() const noexcept { return  const_iterator(d_data); }
			const_iterator cbegin() const noexcept { return begin(); }
			const_iterator cend() const noexcept { return end(); }
			
			// Alloctor

			Allocator& get_allocator() noexcept { return static_cast<Allocator&>(*this); }
			const Allocator& get_allocator() const noexcept { return static_cast<const Allocator&>(*this);}

			// Size functions

			bool empty() const noexcept { return !d_data || d_data->base.size == 0; }
			size_t size() const noexcept { return d_data ? d_data->base.size : 0; }
			size_t max_size() const noexcept { return std::numeric_limits<size_t>::max(); }

			/// @brief Destroy and deallocate all values, directories and nodes
			void clear()
			{
				destroy_data();
			}

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
				bits = std::min(bits, 26U);// maximum 26 bits for a directory
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
				// Create new radix tree
				RadixTree other(static_cast<Hash&>(*this), get_allocator());
				other.reserve(this->size());
				other.insert(std::make_move_iterator(begin()), std::make_move_iterator(end()), false);
				other.swap(*this);
			}


			/// @brief Apply functor to each leaf node or vector node
			template<class Fun>
			void for_each_leaf(Fun&& fun)
			{
				if (size() == 0)
					return;
				auto it = begin();
				while (true) 
				{
					fun(it.dir->children()[it.child]);
					if (it.dir == d_data->end.dir && it.child == d_data->end.child)
						return;
					it.next();
				}
			}
			/// @brief Sort all leaves
			void sort_leaves()
			{
				if (!node::is_sorted)
					return;
				for_each_leaf([](child_ptr child) 
					{
					if(child.tag() == directory::IsLeaf)
						child.to_node()->template sort<extract_key_type>(Less{}); 
					});
			}

			/// @brief Hash key and return the hash value
			template<class U>
			SEQ_ALWAYS_INLINE hash_type hash_key(const U & val) const  {return this->hash((val));}
			/// @brief Hash key and return the hash value at given bit position
			template<class U>
			SEQ_ALWAYS_INLINE hash_type hash_key_shift(size_t shift, const U& val) const  {return this->hash_shift(shift, (val));}
			/// @brief Returns the value tiny hash based on its hash representation
			template<class U>
			SEQ_ALWAYS_INLINE std::uint8_t tiny_hash(const_hash_ref hash, const U& val) const noexcept {return  node::template tiny_hash<Hash>(hash, (val));}

			
			/// @brief Returns the count first bits starting at given directory based on any of its children
			unsigned get_prefix_first_bits(directory* dir, unsigned count, size_t  bit_pos)
			{
				if (bit_pos == static_cast<size_t>(-1))
					bit_pos = iterator::get_bit_pos(dir);
				auto h = this->hash(extract_key_type{}(dir->any_child()));
				h.add_shift(bit_pos);
				return h.n_bits(count);
			}

			directory* make_intermediate(directory* parent, unsigned hash_len, unsigned parent_pos)
			{
				directory * intermediate = directory::make(d_data->base, hash_len);
				intermediate->parent = parent;
				intermediate->parent_pos = parent_pos;
				intermediate->first_valid_child = static_cast<unsigned>(-1);
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


				if (SEQ_UNLIKELY(new_hash_len >= 27)) 
					// we are above maximum allowed size for a directory
					return nullptr;

				// save internal value in order to reset it later to the new directory
				
				//might throw, fine
				directory* new_dir = directory::make(d_data->base, new_hash_len);
				// copy prefix length
				new_dir->prefix_len = dir->prefix_len;

				try
				{
					for (unsigned i = 0; i < size; ++i) 
					{
						directory* child = static_cast<directory*>(dir->children()[i].ptr());
						unsigned child_count = child->size();

						if (prefix_search && child->prefix_len >= start_arity)
						{
							//keep this directory and remove max_hash_len to the prefix.
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

						if (child->hash_len != start_arity)
						{
							// if child has more than min_hash_len bits
							unsigned rem_bits = child->hash_len - start_arity;
							unsigned mask = ((1U << rem_bits) - 1U);
						
							for (unsigned j = 0; j < child_count; ++j)
							{
								unsigned loc = (i << start_arity) | (j >> rem_bits); //take high bits of j
								directory* intermediate = static_cast<directory*>(new_dir->children()[loc].ptr());
								if (!intermediate) 
									//this part might throw which is a problem, we are in an intermediate state.
									intermediate = make_intermediate(new_dir, rem_bits, loc);

								//take low bits of j
								if ((intermediate->children()[j & mask] = child->children()[j])) //take high bits of j
								{
									intermediate->child_count++;
									if(intermediate->first_valid_child == static_cast<unsigned>(-1))
										intermediate->first_valid_child = j & mask;
								}
								if (child->children()[j].tag() == directory::IsDir) 
								{
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
							for (unsigned j = 0; j < child_count; ++j) 
							{
								// compute location
								unsigned loc = j | (i << child->hash_len);

								if ((new_dir->children()[loc] = child->children()[j])) 
									new_dir->child_count++;

								// update directory count
								if (child->children()[j].tag() == directory::IsDir) {
									new_dir->dir_count++;
									directory* d = child->const_child(j).to_dir();//static_cast<directory*>(child->children()[j].ptr());
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

				// compute first valid child
				new_dir->compute_first_valid();

				//destroy old directory
				directory::destroy(d_data->base, dir, false);


				// keep merging if possible
				while (new_dir->dir_count == new_dir->size())
				{
					if (parent_dir) {
						parent_dir->children()[parent_pos] = child_ptr(new_dir, directory::IsDir);
						new_dir->parent = parent_dir;
						new_dir->parent_pos = parent_pos;
						if (parent_dir->first_valid_child == parent_pos)
							parent_dir->compute_first_valid();
					}
					directory* d = merge_dir(new_dir);
					if (!d)
						break;
					new_dir = d;
				}

				if (parent_dir) 
				{
					parent_dir->children()[parent_pos] = child_ptr(new_dir, directory::IsDir);
					new_dir->parent = parent_dir;
					new_dir->parent_pos = parent_pos;
					if (parent_dir->first_valid_child == parent_pos)
						parent_dir->compute_first_valid();

					// try to merge up
					if (parent_dir->dir_count == (1ULL << parent_dir->hash_len))
						merge_dir(parent_dir);
				}
				else 
					d_root = d_data->base.root = new_dir;
					

				return new_dir;
			}

			/// @brief Returns the number of common bits from start_pos in the range [first,last)
			size_t compute_common_bits(size_t start_pos, const T* first, const T* last) const
			{
				return hash_type::template nb_common_bits<StartArity, extract_key_type>(*this, start_pos, first, last);
			}

			/// @brief Returns the number of common bits from start_pos between first and second
			//template<class K>
			size_t compute_common_bits(size_t start_bits, const key_type & first, const key_type& second) const
			{
				struct Iter 
				{
					const key_type** vals;
					unsigned char pos;
					Iter(const key_type** v, unsigned char p) :vals(v), pos(p) {}
					bool operator!=(const Iter& o) const { return pos != o.pos; }
					Iter& operator++() { ++pos; return *this; }
					const key_type& operator*() const { return *vals[pos]; }
				};
				const key_type* vals[2] = {&first, &second};
				return hash_type::template nb_common_bits<StartArity, extract_key_type>(*this, start_bits, Iter(vals,0), Iter(vals, 2));
			}
		
			/// @brief Move all elements within given leaf into a newly created vector, and insert given new value
			template<class Policy, class K, class... Args >
			const_iterator insert_in_vector(directory* dir, size_t bit_pos, node * child, unsigned pos, Policy , K && key, Args&&... args)
			{
				// turn node into a vector and move values
				vector_type* vec = d_data->base.make_vector();
				unsigned position = 0;
				try 
				{
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

				//destroy old child
				node::destroy(d_data->base, child);

				dir->children()[pos] = child_ptr(vec, directory::IsVector);
				++d_data->base.size;

				// recompute bounds if needed
				if (!d_data->begin.dir)
					compute_ends();

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
			

			/// @brief Rehash leaf and move its elements to deeper leaves. Then , insert new value.
			template<bool EnsureSorted, class Policy, class K, class... Args >
			const_iterator rehash_node_and_insert(directory* dir, size_t hash_bits, const_hash_ref hash, std::uint8_t th, Policy policy, K && key, Args&&... args)
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

				if ( (!variable_length && hash_bits > Hash::max_bits - StartArity) || (variable_length && get_depth(dir) > MaxDepth) )
					// We exhausted all available bits, no choice but to turn this leaf into a vector
					return insert_in_vector(dir, hash_bits, child, pos, policy, std::forward<K>(key), std::forward<Args>(args)...);
				
				// create new child directory, might throw, fine
				directory* child_dir = directory::make(d_data->base, start_arity);

				try 
				{
					// Rehash node and insert its values inside the new directory
					T* vals = child->values();
					unsigned count = child->count();
					
					if SEQ_CONSTEXPR(prefix_search) 
					{
						// Find common prefix and increment bit position accordingly
						if (size_t common_bits = compute_common_bits(hash_bits, vals, vals + count))
						{
							child_dir->prefix_len = common_bits;
							hash_bits += common_bits;
						}
						
					} 
				
					for(unsigned i = 0; i < count; ++i) 
					{
						
						// compute hash value for current bit position
						hash_type h = hash_key_shift(hash_bits, ExtractKey{}(vals[i]));
						// compute position in directory
						unsigned new_pos = h.n_bits(start_arity);
						// get the tiny hash
						std::uint8_t cth = child->get_tiny_hash(i);

						if (!child_dir->const_child(new_pos).full()) {
							// create node. If this throw, the new directory is destroyed (basic exception guarantee)
							node* n = node::make(d_data->base, cth, EmplacePolicy{}, std::move(vals[i])).first;
							child_dir->child(new_pos) = child_ptr(n, directory::IsLeaf);
							child_dir->child_count++;
							child_dir->first_valid_child = new_pos;
						}
						else {
							// move value in leaf.
							// if Sort is false, insert the value at the end of the leaf (only for sorted nodes)
							node* n = static_cast<node*>(child_dir->const_child(new_pos).ptr());
							auto p = n->template insert<extract_key_type,Less>(d_data->base, hash_bits, Sort ? static_cast<unsigned>(-1) : n->count(), cth, EmplacePolicy{}, std::move(vals[i]));
							child_dir->child(new_pos) = child_ptr(p.first, directory::IsLeaf);
						}
					}
				
				
				}
				catch (...)
				{
					// In case of exception, just destroyed the newly created directory.
					// Some values might have been moved to it but, hey, this is basic exception guarantee only
					directory::destroy(d_data->base, child_dir);
					throw;
				}

				//destroy child node unused anymore
				node::destroy(d_data->base, child);

				// update parent dir
				child_dir->parent = dir;
				child_dir->parent_pos = pos;
				dir->child(pos) = child_ptr(child_dir, directory::IsDir);
				dir->dir_count++;

				if((dir == d_data->begin.dir && pos == d_data->begin.child) || (dir == d_data->end.dir && pos == d_data->end.child))
					reset_ends();
				
				// now, check if the current directory contains only directories, and merge it if possible
				if (dir->dir_count == dir->size())
				{

					if (merge_dir(dir, prev_hash_bits)) 
					{
						reset_ends();
						// directory merging succeded, now insert the new value starting from the root
						return this->insert_hash_with_tiny<EnsureSorted>(d_data->base.root, 0, hash, th, policy, std::forward<K>(key), std::forward<Args>(args)...).first;
					}
				}

				// update first valid position
				if (dir->first_valid_child == pos)
					dir->compute_first_valid();

				// insert new value starting from dir
				return this->insert_hash_with_tiny<EnsureSorted>(dir, prev_hash_bits - dir->hash_len, this->hash(ExtractKey{}(key)), th, policy, std::forward<K>(key), std::forward<Args>(args)...).first;
			}

			/// @brief Insert value in an empty position within dir
			template< class Policy, class K, class... Args >
			std::pair<const_iterator, bool> insert_null_node(directory* dir, size_t bit_pos, unsigned pos, unsigned th, Policy policy, K&& key, Args&&... args)
			{
				// child is empty: create a new leaf, might throw (fine)
				auto p = node::make(d_data->base, th, policy, std::forward<K>(key), std::forward<Args>(args)...);
				dir->child(pos) = child_ptr(p.first, directory::IsLeaf);
				dir->child_count++;
				dir->first_valid_child = pos;
				++d_data->base.size;

				// recompute begin/end position
				compute_ends();
				return std::pair<const_iterator, bool>(const_iterator(d_data,dir,pos,0, bit_pos), true);
			}

			/// @brief Insert new value in a vector node
			template< class Policy, class K, class... Args >
			std::pair<const_iterator, bool> insert_in_vector_node(directory* dir,size_t bit_pos,  unsigned pos, unsigned , Policy , K && key, Args&&... args)
			{
				// child is a vector
				vector_type* child = static_cast<vector_type*>(dir->children()[pos].ptr());
				auto found = Policy::emplace_vector(*child, std::forward<K>(key), std::forward<Args>(args)...);
				if (!found.second)
					return std::pair<const_iterator, bool>(const_iterator(d_data, dir, pos, static_cast<unsigned>(found.first), bit_pos),false);
				++d_data->base.size;
				return std::pair<const_iterator, bool>(const_iterator(d_data, dir, pos, static_cast<unsigned>(found.first), bit_pos), true);
			}

			/// @brief The value to insert does not follow a directory prefox -> create intermediate directory
			template< class U >
			directory* check_prefix_create_intermediate(directory* dir, directory* d, size_t& hash_bits, unsigned pos, const hash_type& , const U& value)
			{
				// compute the number of common bits starting at hash_bits
				std::uint64_t common = compute_common_bits(hash_bits, ExtractKey{}(value), ExtractKey{}(d->any_child()));
				
				// create intermediate directory with a new prefix length

				// might throw, fine
				directory* new_dir = directory::make(d_data->base, StartArity);
				d->parent->child(pos) = child_ptr(new_dir, directory::IsDir);
				new_dir->parent = d->parent;
				new_dir->parent_pos = pos;
				new_dir->child_count = new_dir->dir_count = 1;
				new_dir->prefix_len = static_cast<size_t>(common);
				
				hash_type h = this->hash(ExtractKey{}(d->any_child()));
				hash_bits += static_cast<size_t>(common);
				h.add_shift(hash_bits);
				unsigned new_pos = h.n_bits(StartArity);
				new_dir->child(new_pos) = child_ptr(d, directory::IsDir);
				new_dir->first_valid_child = new_pos;
				d->parent = new_dir;
				d->parent_pos = new_pos;

				//detect nasty bug where prefix_len wrap around
				SEQ_ASSERT_DEBUG(d->prefix_len >= (StartArity + common), "");
				d->prefix_len -= StartArity + static_cast<size_t>(common);
				hash_bits -= dir->hash_len;

				SEQ_ASSERT_DEBUG(dir->first_valid_child < dir->size(), "");
				SEQ_ASSERT_DEBUG(d->first_valid_child < d->size(), "");
				SEQ_ASSERT_DEBUG(new_dir->first_valid_child < new_dir->size(), "");

				return new_dir;
			}


			/// @brief Check if the value to insert follows the directory prefix.
			/// If not, create an intermediate directory with a new prefix.
			/// Update the bit position if the prefix matches.
			template< class U >
			directory* check_prefix_insert(directory* dir, directory* d, size_t& hash_bits, unsigned pos, const hash_type& hash, const U & value)
			{
				// increment bit position
				hash_bits += dir->hash_len;
				// make hash value at this position
				hash_type h = hash;
				h.add_shift(hash_bits);

				// check prefix
				if (check_prefix(h, d))
				{
					// the value follows the directory prefix: update the bit position
					hash_bits -= dir->hash_len;
					hash_bits += d->prefix_len;
					return d;
				}

				return check_prefix_create_intermediate(dir, d, hash_bits, pos, hash, value);
			}

			/// @brief Insert in leaf node
			template< bool EnsureSorted, class Policy, class K, class... Args >
			SEQ_ALWAYS_INLINE std::pair<const_iterator, bool> insert_in_leaf(directory* dir, node* child, size_t hash_bits, unsigned pos, const_hash_ref hash, std::uint8_t th, Policy policy, K && key, Args&&... args)
			{
				// find key in node
				auto found = child->template find_insert<EnsureSorted, extract_key_type, Equal, Less>(Equal{}, hash_bits, th, ExtractKey{}(key));

				if (found.first)
					// key already exists!
					return std::pair<const_iterator, bool>(const_iterator(d_data, dir, pos, static_cast<unsigned>(found.first - child->values()), hash_bits), false);

				// check if the node is full and needs to be rehashed
				if (SEQ_UNLIKELY(child->full()))
					return std::pair<const_iterator, bool>(rehash_node_and_insert<EnsureSorted>(dir, hash_bits, hash, th, policy, std::forward<K>(key), std::forward<Args>(args)...), true);

				// add to leaf
				auto p = child->template insert< extract_key_type, Less>(d_data->base, hash_bits, EnsureSorted ? found.second : child->count(), th, policy, std::forward<K>(key), std::forward<Args>(args)...);
				// update first_valid_child
				dir->first_valid_child = pos;
				// increment size
				++d_data->base.size;
				// update child at pos
				dir->child(pos) = child_ptr(p.first, directory::IsLeaf);

				// recompute ends
				if (size() == 1) {
					// First element
					d_data->begin.store_value(dir, pos, hash_bits, *this);
					d_data->end.store_value(dir, pos, hash_bits, *this);
				}
				else if (!d_data->begin.dir) {
					// Invalid begin/end position due to a rehash
					compute_ends();
				}
				else {
					// Update for value outside current range
					if (d_data->begin.greater_than(hash, key))
						d_data->begin.store_value(dir, pos, hash_bits, key, hash);
					else if (d_data->end.less_than(hash, key))
						d_data->end.store_value(dir, pos, hash_bits, key, hash);

				}
				return std::pair<const_iterator, bool>(const_iterator(d_data, dir, pos, p.second, hash_bits), true);
			}

			/// @brief Main key insertion process, starting from dir at hash_bits position.
			template< bool EnsureSorted, class Policy, class K, class... Args >
			SEQ_ALWAYS_INLINE std::pair<const_iterator, bool> insert_hash_with_tiny(directory * dir, size_t hash_bits,  const_hash_ref hash, std::uint8_t th,  Policy p, K && key, Args&&... args)
			{
				static constexpr bool Sort = EnsureSorted && node::is_sorted;
				
				// compute position within directory
				unsigned pos = hash.n_bits(hash_bits, dir->hash_len);

				// move forward inside the tree
				while (dir->const_child(pos).tag() == directory::IsDir)
				{
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
					return insert_null_node(dir,hash_bits, pos, th, p, std::forward<K>(key), std::forward<Args>(args)...);
				else if (dir->const_child(pos).tag() == directory::IsVector)
					// insert in vector node
					return insert_in_vector_node(dir, hash_bits, pos, th, p, std::forward<K>(key), std::forward<Args>(args)...);

				// child is a leaf
				node* child = dir->const_child(pos).to_node();
				return this->template insert_in_leaf<Sort>(dir, child, hash_bits, pos, hash, th, p, std::forward<K>(key), std::forward<Args>(args)...);
			}




			/// @brief Insert new value based on its hash value
			template<  bool EnsureSorted, class Policy, class K, class... Args >
			SEQ_ALWAYS_INLINE std::pair<const_iterator, bool> emplace_hash(const_hash_ref hash, Policy p, K && key, Args&&... args)
			{
				if (SEQ_UNLIKELY(!d_data)) 
					// initialize root
					make_data();
				// compute tiny hash
				std::uint8_t th = tiny_hash(hash, ExtractKey{}(key));
				// insert
				return insert_hash_with_tiny<EnsureSorted>(d_data->base.root, 0, hash, th, p, std::forward<K>(key), std::forward<Args>(args)...);
			}

			/// @brief Construct emplace and insert
			template<  class K, class... Args >
			SEQ_ALWAYS_INLINE std::pair<const_iterator, bool> emplace( K && key, Args&&... args)
			{
				return this->template emplace_hash<true>(hash_key(ExtractKey{}(key)), EmplacePolicy{}, std::forward<K>(key), std::forward<Args>(args)...);
			}
			/// @brief Construct emplace and insert
			template<  class K, class... Args >
			SEQ_ALWAYS_INLINE std::pair<const_iterator, bool> emplace_with_hash(const_hash_ref hash, K&& key, Args&&... args)
			{
				return this->template emplace_hash<true>(hash, EmplacePolicy{}, std::forward<K>(key), std::forward<Args>(args)...);
			}


			/// @brief Insert new value based on its hash value and a hint
			template<  bool EnsureSorted, class Policy, class K, class... Args >
			SEQ_ALWAYS_INLINE std::pair<const_iterator, bool> emplace_hash_hint(const_iterator hint, const_hash_ref h, Policy p, K && key, Args&&... args)
			{
				SEQ_ASSERT_DEBUG(hint.data == d_data, "");

				// Make sure the hint points to the right leaf
				if (node::is_sorted && hint != end())
				{
					// check if iterator points to a non full, non vector node
					child_ptr ch = hint.dir->children()[hint.child];
					if (ch.tag() == directory::IsLeaf && !ch.to_node()->full())
					{
						// position within directory
						if (h.n_bits(hint.bit_pos, hint.dir->hash_len) == hint.child)
						{
							// check that this is the right directory
							if (compute_common_bits(0, ExtractKey{}(*hint), ExtractKey{}(key)) >= hint.bit_pos)
								//insert in this child
								return this->template insert_in_leaf<EnsureSorted>(hint.dir, ch.to_node(), hint.bit_pos, hint.child, h, tiny_hash(h, ExtractKey{}(key)), p, std::forward<K>(key), std::forward<Args>(args)...);
						}
					}
				}

				// standard insertion
				return this->template emplace_hash< EnsureSorted>(h, p, std::forward<K>(key), std::forward<Args>(args)...);
			}

			template< class K, class... Args >
			SEQ_ALWAYS_INLINE const_iterator emplace_hint(const_iterator hint, K && key, Args&&... args)
			{
				return this->template emplace_hash_hint<true>(hint, hash_key(ExtractKey{}(key)), EmplacePolicy{}, std::forward<K>(key), std::forward<Args>(args)...).first;
			}
			template< class K, class... Args >
			SEQ_ALWAYS_INLINE const_iterator emplace_hint_with_hash(const_hash_ref hash, const_iterator hint, K&& key, Args&&... args)
			{
				return this->template emplace_hash_hint<true>(hint, hash, EmplacePolicy{}, std::forward<K>(key), std::forward<Args>(args)...).first;
			}

			template< class K, class... Args >
			SEQ_ALWAYS_INLINE auto try_emplace(K && key, Args&&... args) -> std::pair<iterator, bool>
			{
				return this->template emplace_hash<true>(hash_key(ExtractKey{}(key)), TryEmplacePolicy{}, std::forward<K>(key), std::forward<Args>(args)...);
			}
			template< class K, class... Args >
			SEQ_ALWAYS_INLINE auto try_emplace_with_hash(const_hash_ref hash, K&& key, Args&&... args) -> std::pair<iterator, bool>
			{
				return this->template emplace_hash<true>(hash, TryEmplacePolicy{}, std::forward<K>(key), std::forward<Args>(args)...);
			}

			template< class K, class... Args >
			SEQ_ALWAYS_INLINE auto try_emplace_hint(const_iterator hint, K&& key, Args&&... args) -> std::pair<iterator, bool>
			{
				return this->template emplace_hash_hint<true>(hint, hash_key(ExtractKey{}(key)), TryEmplacePolicy{}, std::forward<K>(key), std::forward<Args>(args)...);
			}
			template< class K, class... Args >
			SEQ_ALWAYS_INLINE auto try_emplace_hint_with_hash(const_hash_ref hash, const_iterator hint, K&& key, Args&&... args) -> std::pair<iterator, bool>
			{
				return this->template emplace_hash_hint<true>(hint, hash, TryEmplacePolicy{}, std::forward<K>(key), std::forward<Args>(args)...);
			}

			/// @brief Range insertion.
			/// For sorted radix tree, insert all values in an unsorted way within leaves, and sort all leaves independantly.
			template<class Iter>
			void insert(Iter start, Iter end, bool _sort_leaves = true)
			{
				if (start == end)
					return;

				const_iterator it = this->end();
				for (; start != end; ++start)
				{
					if(std::is_arithmetic<key_type>::value)
						it = this->template emplace_hash_hint<false>(it, hash_key(ExtractKey{}(*start)), EmplacePolicy{}, *start).first;
					else
						this->template emplace_hash<false>(hash_key(ExtractKey{}(*start)), EmplacePolicy{}, *start);
					
				}
				if (_sort_leaves)
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

			/// @brief Erase a single element
			const_iterator erase(const_iterator it)
			{
				SEQ_ASSERT_DEBUG(it != end(),"");

				// Get next position
				const_iterator next = it;
				++next;

				directory* d = it.dir;
				unsigned dpos = it.child;
				
				if (d->child(dpos).tag() == directory::IsVector) {
					//erase inside vector leaf
					vector_type* v = d->child(dpos).to_vector();
					v->erase(it.node_pos);
					if (v->size() == 0) {
						//destroy empty vector
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
					//erase in leaf node
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

				if (d->child_count ) 
				{
					// First valid position
					if (d->first_valid_child == dpos)
						d->compute_first_valid();
					// Update begin/end position
					if(d_data->begin.dir == d && d_data->begin.child == dpos)
						compute_begin();
					if (d_data->end.dir == d && d_data->end.child == dpos)
						compute_end();
					return next;
				}

				// Destroy empty directories until root directory
				while (d->parent && d->child_count == 0 )
				{
					directory* parent = d->parent;
					unsigned parent_pos = d->parent_pos;
					
					directory::destroy(d_data->base, d, false);
					parent->children()[parent_pos] = child_ptr();
					parent->child_count--;
					parent->dir_count--;
					if (parent->first_valid_child == parent_pos)
						parent->compute_first_valid();
					d = parent;
				}
				compute_ends();
				if (size() == 0)
					return end();
				return next;
			}

			template <class K>
			size_t erase(const K& key)  
			{
				auto it = find(key);
				if (it == end())
					return 0;
				erase(it);
				return 1;
			}
			

			/// @brief Check if given hash value share the same prefix as d
			SEQ_ALWAYS_INLINE bool check_prefix(const hash_type& hash,const directory *d) const
			{
				return hash_type::check_prefix(hash, ExtractKey{}(d->any_child()), d->prefix_len);
			}

			/// @brief Find key in vector node
			template<class U>
			const_iterator find_in_vector(const directory * d, size_t bit_pos, unsigned pos, const vector_type* vec, const U& key) const
			{
				size_t found = vec->find(key);
				if (found == vec->size())
					return end();
				return const_iterator(d_data,d, pos, static_cast<unsigned>(found), bit_pos);
			}

			template<class U>
			const T* find_in_vector_ptr(const directory* , size_t , unsigned , const vector_type* vec, const U& key) const
			{
				size_t found = vec->find(key);
				if (found == vec->size())
					return nullptr;
				return &vec->at(found);
			}

			/// @brief Find key based on its hash value
			/// Make this function template just for hash table allowing heterogeneous lookup
			template<class U>
			SEQ_ALWAYS_INLINE const_iterator find_hash(const_hash_ref hash, const U & key) const
			{
				// start directory
				const directory* d = d_root;
				// tiny hash
				unsigned th = tiny_hash(hash, key);
				// start position within start directory
				unsigned pos = hash.n_bits(d->hash_len);

				// walk through the tree as long as we keep finding directories
				while (std::uintptr_t tag = d->children()[pos].tag())
				{
					switch (tag)
					{
						case directory::IsDir: 
							hash.add_shift(d->hash_len);
							d = d->children()[pos].to_dir();

							// check directory prefix
							if (prefix_search && d->prefix_len && !check_prefix(hash, d))
								return cend();
							
							// compute new position within current directory
							pos = hash.n_bits(d->hash_len);
							continue;
						case directory::IsVector: 
							return find_in_vector(d, hash.get_shift(), pos, d->children()[pos].to_vector(), key);
						case directory::IsLeaf:
							th = d->children()[pos].to_node()->template find<extract_key_type, Equal, Less>(Equal{}, hash.get_shift(), static_cast<std::uint8_t>( th), key);
							if (th != static_cast<unsigned>(-1)) return const_iterator(d_data, d, pos, th, hash.get_shift());
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
				unsigned th = tiny_hash(hash, key);
				// start position within start directory
				unsigned pos = hash.n_bits(d->hash_len);

				// walk through the tree as long as we keep finding directories
				while (std::uintptr_t tag = d->children()[pos].tag())
				{
					switch (tag)
					{
					case directory::IsDir:
						hash.add_shift(d->hash_len);
						d = d->children()[pos].to_dir();

						// check directory prefix
						if (prefix_search && d->prefix_len && !check_prefix(hash, d))
							return nullptr;

						// compute new position within current directory
						pos = hash.n_bits(d->hash_len);
						continue;
					case directory::IsVector:
						return find_in_vector_ptr(d, hash.get_shift(), pos, d->children()[pos].to_vector(), key);
					case directory::IsLeaf:
						th = d->children()[pos].to_node()->template find<extract_key_type, Equal, Less>(Equal{}, hash.get_shift(), static_cast<std::uint8_t>(th), key);
						if (th != static_cast<unsigned>(-1)) 
							return d->children()[pos].to_node()->values()+ th;
						return nullptr;
					}
				}
				// not found: the node is null or the key is not inside
				return nullptr;
			}

#if defined( __GNUC__) && __GNUC__ >= 13
#define _GET_RADIX_KEY(key, k) \
	SEQ_PRAGMA(GCC diagnostic push) \
	SEQ_PRAGMA(GCC diagnostic ignored "-Wdangling-reference") \
	const auto& key = extract_key_type{}(k);\
	SEQ_PRAGMA(GCC diagnostic pop)
#else
#define _GET_RADIX_KEY(key, k) const auto& key = extract_key_type{}(k);
#endif

			/// @brief Returns iterator pointing to given key, or end() if not found
			template< class U >
			SEQ_ALWAYS_INLINE const_iterator find(const U & k) const
			{
				//const auto& key = extract_key_type{}(k);
				_GET_RADIX_KEY(key,k)
				return find_hash(hash_key(key), key);
			}
			template< class U >
			SEQ_ALWAYS_INLINE const T* find_ptr(const U & k) const
			{
				//const auto& key = extract_key_type{}(k);
				_GET_RADIX_KEY(key, k)
				return find_ptr_hash(hash_key(key), key);
			}


			const_iterator lower_bound_in_vector(const directory* d, unsigned pos, const_hash_ref hash, const key_type& key) const
			{
				const vector_type* v = d->children()[pos].to_vector();
				unsigned p = static_cast<unsigned>(v->lower_bound(key));
				if (p != v->size())
					return const_iterator(d_data, d, pos, p, hash.get_shift());
				return ++const_iterator(d_data, d, pos, p - 1, hash.get_shift());
			}

			/// @brief Find key based on its hash value
			SEQ_ALWAYS_INLINE const_iterator lower_bound_hash(const_hash_ref hash, const key_type & key) const
			{
				if (SEQ_UNLIKELY(empty() || Less{}(ExtractKey{}(d_data->end.get_back_value()), key)))
					return end();
				if (SEQ_UNLIKELY(Less{}(key, ExtractKey{}(d_data->begin.get_value()))))
					return begin();
				
				// start directory
				const directory* d = d_root;
				// tiny hash
				unsigned th = tiny_hash(hash, key);
				// start position within start directory
				unsigned pos = hash.n_bits(d->hash_len);

				// walk through the tree as long as we keep finding directories
				while (d->children()[pos].tag() == directory::IsDir)
				{
					hash.add_shift(d->hash_len);

					// compute new directory
					d = d->children()[pos].to_dir();

					if SEQ_CONSTEXPR(prefix_search) {
						// check directory prefix
						if (d->prefix_len) {
							size_t shift = hash.get_shift();
							if (!check_prefix(hash, d))
							{
								bool less = Less{}(ExtractKey{}(d->any_child()), key);
								auto tmp = const_iterator::find_next(
									less ? d->parent : d,
									less ? d->parent_pos + 1 : 0,
									less ? shift - d->parent->hash_len : shift + d->prefix_len);
								// Note: could be end() iterator
								return const_iterator(d_data, tmp.dir, tmp.child, 0, tmp.bit_pos);
							}
						}
					}
					// compute new position within current directory
					pos = hash.n_bits(d->hash_len);
				}

				if (d->children()[pos].tag() == 0)
				{
					//return next item
					auto tmp = const_iterator::find_next(d, pos+1, hash.get_shift());
					// Note: could be end() iterator
					return const_iterator(d_data, tmp.dir, tmp.child, 0, tmp.bit_pos);
				}

				// at this point, the position within current directory hold either a standard leaf or a vector

				if (SEQ_UNLIKELY(d->children()[pos].tag() == directory::IsVector)) 
					return lower_bound_in_vector(d, pos, hash, key);
					
				
				const node* n = d->children()[pos].to_node();
				unsigned p = n->template lower_bound<ExtractKey, Less, Equal>(hash.get_shift(),th, key);
				if (p != n->count())
					return const_iterator(d_data, d, pos, p, hash.get_shift());
				return ++const_iterator(d_data, d, pos, p - 1, hash.get_shift());
			}
			template< class U >
			SEQ_ALWAYS_INLINE const_iterator lower_bound(const U & k) const
			{
				//const auto& key = extract_key_type{}(k);
				_GET_RADIX_KEY(key, k)
				return lower_bound_hash(hash_key(key), key);
			}


			SEQ_ALWAYS_INLINE const_iterator upper_bound_hash(const_hash_ref hash, const key_type& key) const
			{
				const_iterator it = lower_bound_hash(hash,key);
				if (it != end() && ExtractKey {}(*it) == key)
					++it;
				return it;
			}
			template< class U >
			SEQ_ALWAYS_INLINE const_iterator upper_bound(const U & k) const
			{
				//const auto& key = extract_key_type{}(k);
				_GET_RADIX_KEY(key, k)
				return upper_bound_hash(hash_key(key), key);
			}



			/// @brief Find key based on its hash value
			SEQ_ALWAYS_INLINE const_iterator prefix_hash(const_hash_ref hash, const key_type& key) const
			{
				static_assert(prefix_search, "prefix function is only available for variable length keys!");

				auto it = lower_bound_hash(hash, key);
				if (it != end() && ExtractKey {}(*it).find(key.data(), 0, key.size()) == 0)
					return it;
				return end();
			}
			template< class U >
			SEQ_ALWAYS_INLINE const_iterator prefix(const U& k) const
			{
				//const auto& key = extract_key_type{}(k);
				_GET_RADIX_KEY(key, k)
				return prefix_hash(hash_key(key), key);
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

				const_prefix_iterator(const const_iterator & i = const_iterator(), const key_type & pr = key_type()) noexcept
					: it(i), prefix(pr) {}
				
				auto operator*() const noexcept -> reference { return *it; }
				auto operator->() const noexcept -> pointer { return std::pointer_traits<pointer>::pointer_to(**this); }
				auto operator++() noexcept -> const_prefix_iterator& {
					++it;
					if (it != const_iterator(it.data) && ExtractKey {}(*it).find(prefix.data(), 0, prefix.size()) != 0)
						it = const_iterator(it.data); //end
					return *this;
				}
				auto operator++(int) noexcept -> const_prefix_iterator {
					const_prefix_iterator _Tmp = *this;
					++(*this);
					return _Tmp;
				}
				bool operator==(const const_prefix_iterator& other) const noexcept {return it == other.it;}
				bool operator!=(const const_prefix_iterator& other) const noexcept {return it != other.it;}
			};

			template< class U >
			SEQ_ALWAYS_INLINE auto prefix_range(const U& k) const -> std::pair<const_prefix_iterator, const_prefix_iterator>
			{
				auto it = prefix(k);
				return std::make_pair(const_prefix_iterator(it, extract_key_type{}(k)), const_prefix_iterator(end()));
			}
		};


	}
}

#endif
