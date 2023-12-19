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

#ifndef SEQ_HASH_HPP
#define SEQ_HASH_HPP



/** @file */


/**\defgroup hash Hash: small collection of hash utilities

The hash module provides several hash-related functions:
	-	seq::hash_combine: combine 2 hash values
	-	seq::hash_bytes_murmur64: murmurhash2 algorithm
	-	seq::hash_bytes_fnv1a: fnv1a hash algorithm working on chunks of 4 to 8 bytes
	-	seq::hash_bytes_fnv1a_slow: standard fnv1a hash algorithm

Note that the specialization of std::hash for seq::tiny_string uses murmurhash2 algorithm.

*/


/** \addtogroup hash
 *  @{
 */


#include "bits.hpp"
#include <string>
#include <type_traits>
#include <tuple>
#include <utility>
#include <memory>

namespace seq
{

	/// @brief Detect is_avalanching typedef 
	/// @tparam T 
	template <typename T>
	struct hash_is_avalanching {
	private:
		template <typename T1>
		static typename T1::is_avalanching test(int);
		template <typename>
		static void test(...);
	public:
		enum { value = !std::is_void<decltype(test<T>(0))>::value };
	};




	namespace detail
	{
		SEQ_ALWAYS_INLINE std::uint64_t Mixin64(std::uint64_t a) noexcept
		{
			a ^= a >> 23;
			a *= 0x2127599bf4325c37ULL;
			a ^= a >> 47;
			return a;
		}
		template<size_t Size>
		struct Mixin
		{
			static SEQ_ALWAYS_INLINE size_t mix(size_t a) noexcept
			{
				return static_cast<size_t>(Mixin64(a));
			}
		};
		template<>
		struct Mixin<8>
		{
			static SEQ_ALWAYS_INLINE size_t mix(size_t a) noexcept
			{
#ifdef SEQ_HAS_FAST_UMUL128
				static constexpr uint64_t k = 0xde5fb9d2630458e9ULL;
				uint64_t l, h;
				umul128(a, k, &l, &h);
				return static_cast<size_t>(h + l);
#else
				return static_cast<size_t>(Mixin64(a));
#endif
			}
		};


		template<class Hash, bool avalanching = hash_is_avalanching<Hash>::value>
		struct HashVal
		{
			template<class T>
			static SEQ_ALWAYS_INLINE size_t hash(const Hash& h, const T& v) noexcept(noexcept(std::declval<Hash&>()(std::declval<T&>())))
			{
				return h(v);
			}
		};
		template<class Hash>
		struct HashVal<Hash,false>
		{
			template<class T>
			static SEQ_ALWAYS_INLINE size_t hash(const Hash& h, const T& v) noexcept(noexcept(std::declval<Hash&>()(std::declval<T&>())))
			{
				return Mixin<sizeof(size_t)>::mix(h(v));
			}
		};
	}

	/// @brief Mix input hash value for better avalanching
	SEQ_ALWAYS_INLINE size_t hash_finalize(size_t h) noexcept
	{
		return detail::Mixin<sizeof(size_t)>::mix(h);
	}

	/// @brief Combine 2 hash values. Uses murmurhash2 mixin.
	/// @param h1 first hash value
	/// @param h2 second hash value
	/// @return combination of both hash value
	SEQ_ALWAYS_INLINE void hash_combine(size_t & seed, size_t h2) noexcept 
	{
#ifdef SEQ_ARCH_64
		static constexpr std::uint64_t m = 14313749767032793493ULL;
		static constexpr std::uint64_t r = 47ULL;

		h2 *= m;
		h2 ^= h2 >> r;
		h2 *= m;

		seed ^= h2;
		seed *= m;
#else
		seed ^= h2 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
#endif
	}

	/// @brief Hash value v using provided hasher.
	/// Mix the result if Hasher does not provide the is_avalanching typedef.
	template<class Hasher, class T>
	SEQ_ALWAYS_INLINE size_t hash_value(const Hasher& h, const T& v)  noexcept(noexcept(std::declval<Hasher&>()(std::declval<T&>())))
	{
		return detail::HashVal<Hasher>::hash(h, v);
	}





}


#ifndef SEQ_HEADER_ONLY
namespace seq
{
	SEQ_EXPORT auto hash_bytes_murmur64(const void* ptr, size_t len) noexcept -> size_t;
	SEQ_EXPORT auto hash_bytes_fnv1a(const void* ptr, size_t size)noexcept -> size_t;
	SEQ_EXPORT auto hash_bytes_komihash(const void* ptr, size_t size)noexcept -> size_t;
}
#else
#include "internal/hash.cpp"
#endif


namespace seq
{

	template<class T, class Enable = void>
	struct hasher : public std::hash<T>
	{
		SEQ_ALWAYS_INLINE size_t operator()(const T& v) const noexcept(noexcept(std::declval<std::hash<T>&>()(std::declval<T&>()))) {
			return (std::hash<T>::operator()(v));
		}
	};

#define _SEQ_INTEGRAL_HASH_FUNCTION(T) \
	template<> struct hasher <T> { \
		using is_avalanching = int;\
		SEQ_ALWAYS_INLINE size_t operator()(T v) const noexcept {return hash_finalize(static_cast<size_t>(v));} \
	}

	_SEQ_INTEGRAL_HASH_FUNCTION(bool);
	_SEQ_INTEGRAL_HASH_FUNCTION(char);
	_SEQ_INTEGRAL_HASH_FUNCTION(signed char);
	_SEQ_INTEGRAL_HASH_FUNCTION(unsigned char);
	_SEQ_INTEGRAL_HASH_FUNCTION(short);
	_SEQ_INTEGRAL_HASH_FUNCTION(unsigned short);
	_SEQ_INTEGRAL_HASH_FUNCTION(int);
	_SEQ_INTEGRAL_HASH_FUNCTION(unsigned int);
	_SEQ_INTEGRAL_HASH_FUNCTION(long);
	_SEQ_INTEGRAL_HASH_FUNCTION(unsigned long);
	_SEQ_INTEGRAL_HASH_FUNCTION(long long);
	_SEQ_INTEGRAL_HASH_FUNCTION(unsigned long long);
	_SEQ_INTEGRAL_HASH_FUNCTION(wchar_t);
	_SEQ_INTEGRAL_HASH_FUNCTION(char16_t);
	_SEQ_INTEGRAL_HASH_FUNCTION(char32_t);

	template<> 
	struct hasher <float> {
		using is_avalanching = int;
		SEQ_ALWAYS_INLINE size_t operator()(float v) const noexcept{
			union { float fv; std::uint32_t uv; };
			fv = v;
			return hash_finalize(uv);
		}
	};

	template<> 
	struct hasher <double>{
		using is_avalanching = int;
		SEQ_ALWAYS_INLINE size_t operator()(double v) const noexcept{
			union { double fv; std::uint64_t uv; };
			fv = v;
			return hash_finalize(static_cast<size_t>(uv));
		}
	};


	template <class T>
	struct hasher<T*> {
		using is_avalanching = int;
		SEQ_ALWAYS_INLINE size_t operator()(T* ptr) const noexcept {
			return hash_finalize(reinterpret_cast<std::uintptr_t>(ptr));
		}
	};

	template <class T>
	struct hasher<std::unique_ptr<T>> {
		using is_avalanching = int;
		using is_transparent = int;
		SEQ_ALWAYS_INLINE size_t operator()(const std::unique_ptr<T> & ptr) const noexcept {
			return hash_finalize(reinterpret_cast<std::uintptr_t>(ptr.get()));
		}
		SEQ_ALWAYS_INLINE size_t operator()(T* ptr) const noexcept {
			return hash_finalize(reinterpret_cast<std::uintptr_t>(ptr));
		}
	};

	template <class T>
	struct hasher<std::shared_ptr<T>> {
		using is_avalanching = int;
		using is_transparent = int;
		SEQ_ALWAYS_INLINE size_t operator()(const std::shared_ptr<T> & ptr) const noexcept {
			return hash_finalize(reinterpret_cast<std::uintptr_t>(ptr.get()));
		}
		SEQ_ALWAYS_INLINE size_t operator()(T* ptr) const noexcept {
			return hash_finalize(reinterpret_cast<std::uintptr_t>(ptr));
		}
	};

	template <typename Enum>
	struct hasher<Enum, typename std::enable_if<std::is_enum<Enum>::value,void>::type> {
		using is_avalanching = int;
		using is_transparent = int;
		SEQ_ALWAYS_INLINE size_t operator()(Enum e) const noexcept {
			using Underlying = typename std::underlying_type<Enum>::type;
			return hasher<Underlying>{}(static_cast<Underlying>(e));
		}
		template<class Integral>
		SEQ_ALWAYS_INLINE size_t operator()(Integral e) const noexcept {
			return hasher<Integral>{}(e);
		}
	};

	template <class A, class B>
	struct hasher<std::pair<A,B>> {
		using is_avalanching = int;
		SEQ_ALWAYS_INLINE size_t operator()(const std::pair<A, B> & p) const noexcept {
			size_t s = hasher<A>{}(p.first);
			hash_combine(s  , hasher<B>{}(p.second));
			return s;
		}
	};



	namespace detail
	{
		template <class Tuple, size_t Index = std::tuple_size<Tuple>::value - 1>
		struct HashTuple
		{
			static SEQ_ALWAYS_INLINE void apply(size_t& seed, Tuple const& tuple) noexcept
			{
				using elem_type = typename std::tuple_element<Index, Tuple>::type;
				HashTuple<Tuple, Index - 1>::apply(seed, tuple);
				hash_combine(seed , hasher<elem_type>{}(std::get<Index>(tuple)));
			}
		};

		template <class Tuple>
		struct HashTuple<Tuple, 0>
		{
			static SEQ_ALWAYS_INLINE void apply(size_t& seed, Tuple const& tuple) noexcept
			{
				using elem_type = typename std::tuple_element<0, Tuple>::type;
				hash_combine(seed, hasher<elem_type>{}(std::get<0>(tuple)));
			}
		};
	}

	template <class... Args>
	struct hasher<std::tuple<Args...>> {
		using is_avalanching = int;
		SEQ_ALWAYS_INLINE size_t operator()(const std::tuple<Args...>& t) const noexcept {
			size_t seed = 0;
			detail::HashTuple<std::tuple<Args...> >::apply(seed, t);
			return seed;
		}
	};
}

/** @}*/
//end hash

#endif
