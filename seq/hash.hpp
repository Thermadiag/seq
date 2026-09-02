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

#ifndef SEQ_HASH_HPP
#define SEQ_HASH_HPP

/** @file */

/**\defgroup hash Hash: small collection of hash utilities

*/

/** \addtogroup hash
 *  @{
 */

#include <string>
#include <type_traits>
#include <tuple>
#include <utility>
#include <memory>
#include <chrono>
#include <cstddef>    // std::size_t
#include <cstdint>    // std::uint64_t, uintptr_t, etc.
#include <functional> // std::hash
#include <cstring>

#include "internal/utils.hpp"
#include "internal/hash_impl.hpp"
#include "type_traits.hpp"

namespace seq
{

	/// @brief Detect is_avalanching typedef
	template<typename T>
	struct hash_is_avalanching : has_is_avalanching<T>
	{
	};

	/// @brief Detect is_transparent typedef
	template<typename T>
	struct hash_is_transparent : has_is_transparent<T>
	{
	};


	/// @brief Mix input hash value for better avalanching
	SEQ_ALWAYS_INLINE std::size_t hash_finalize(std::uint64_t a) noexcept
	{
#ifdef SEQ_HAS_FAST_UMUL128
		static constexpr std::uint64_t k = 0xde5fb9d2630458e9ULL;
		std::uint64_t l, h;
		umul128(a, k, &l, &h);
		return static_cast<std::size_t>(h + l);
#else
		a ^= a >> 23;
		a *= 0x2127599bf4325c37ULL;
		a ^= a >> 47;
		return static_cast<std::size_t>(a);
#endif
	}

	/// @brief Combine 2 hash values. Uses murmurhash2 mixin.
	/// @param seed in/out seed value
	/// @param h2 hash value to combine with
	SEQ_ALWAYS_INLINE void hash_combine(std::size_t& seed, std::size_t h2) noexcept
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
	SEQ_ALWAYS_INLINE std::size_t hash_value(const Hasher& h, const T& v) noexcept(noexcept(std::declval<const Hasher&>()(std::declval<const T&>())))
	{
		if constexpr (hash_is_avalanching<Hasher>::value)
			return h(v);
		else
			return hash_finalize(h(v));
	}

	inline auto hash_bytes_murmur64(const void* ptr, std::size_t size) noexcept
	{
		return detail::hash_bytes_murmur64_impl(ptr, size);
	}
	inline auto hash_bytes_fnv1a(const void* ptr, std::size_t size) noexcept
	{
		return detail::hash_bytes_fnv1a_impl(ptr, size);
	}
	inline auto hash_bytes_komihash(const void* ptr, std::size_t size) noexcept
	{
		return detail::hash_bytes_komihash_impl(ptr, size);
	}

	template<class T, class Enable = void>
	struct hasher : public std::hash<T>
	{
		SEQ_ALWAYS_INLINE std::size_t operator()(const T& v) const noexcept(noexcept(std::declval<const std::hash<T>&>()(std::declval<const T&>()))) { return (std::hash<T>::operator()(v)); }
	};

#define SEQ_INTEGRAL_HASH_FUNCTION(T)                                                                                                                                                                  \
	template<>                                                                                                                                                                                     \
	struct hasher<T>                                                                                                                                                                               \
	{                                                                                                                                                                                              \
		using is_avalanching = void;                                                                                                                                                           \
		using is_transparent = void;                                                                                                                                                           \
		template<class U>                                                                                                                                                                      \
		SEQ_ALWAYS_INLINE std::size_t operator()(const U& v) const noexcept                                                                                                                         \
		{                                                                                                                                                                                      \
			return hash_finalize(static_cast<std::uint64_t>(static_cast<T>(v)));                                                                                                                  \
		}                                                                                                                                                                                      \
	}

	SEQ_INTEGRAL_HASH_FUNCTION(bool);
	SEQ_INTEGRAL_HASH_FUNCTION(char);
	SEQ_INTEGRAL_HASH_FUNCTION(signed char);
	SEQ_INTEGRAL_HASH_FUNCTION(unsigned char);
	SEQ_INTEGRAL_HASH_FUNCTION(short);
	SEQ_INTEGRAL_HASH_FUNCTION(unsigned short);
	SEQ_INTEGRAL_HASH_FUNCTION(int);
	SEQ_INTEGRAL_HASH_FUNCTION(unsigned int);
	SEQ_INTEGRAL_HASH_FUNCTION(long);
	SEQ_INTEGRAL_HASH_FUNCTION(unsigned long);
	SEQ_INTEGRAL_HASH_FUNCTION(long long);
	SEQ_INTEGRAL_HASH_FUNCTION(unsigned long long);
	SEQ_INTEGRAL_HASH_FUNCTION(wchar_t);
	SEQ_INTEGRAL_HASH_FUNCTION(char16_t);
	SEQ_INTEGRAL_HASH_FUNCTION(char32_t);
#ifdef SEQ_HAS_CPP_20
	SEQ_INTEGRAL_HASH_FUNCTION(char8_t);
#endif

	template<>
	struct hasher<float>
	{
		using is_avalanching = void;
		using is_transparent = void;
		template<class U>
		SEQ_ALWAYS_INLINE std::size_t operator()(const U& _v) const noexcept
		{
			std::uint32_t bits;
			float v = static_cast<float>(_v);

			if constexpr (std::is_signed_v<U>) {
				if (v == 0.0f) // 0.f and -0.f compare equal but have different bits representation
					return hash_finalize(0);
			}

			std::memcpy(&bits, &v, sizeof(bits));
			return hash_finalize(bits);
		}
	};

	template<>
	struct hasher<double>
	{
		using is_avalanching = void;
		using is_transparent = void;
		template<class U>
		SEQ_ALWAYS_INLINE std::size_t operator()(const U& _v) const noexcept
		{
			std::uint64_t bits;
			double v = static_cast<double>(_v);

			if constexpr (std::is_signed_v<U>) {
				if (v == 0.0) // 0.f and -0.f compare equal but have different bits representation
					return hash_finalize(0);
			}

			std::memcpy(&bits, &v, sizeof(bits));
			return hash_finalize(bits);
		}
	};
	template<>
	struct hasher<long double> : hasher<double>
	{
	};

	template<class T>
	struct hasher<T*>
	{
		using is_avalanching = void;
		using is_transparent = void;
		template<class U>
		SEQ_ALWAYS_INLINE std::size_t operator()(U* ptr) const noexcept
		{
			return hash_finalize(reinterpret_cast<std::uintptr_t>(ptr));
		}
	};

	template<class T, class D>
	struct hasher<std::unique_ptr<T, D>>
	{
		using is_avalanching = void;
		using is_transparent = void;
		SEQ_ALWAYS_INLINE std::size_t operator()(const std::unique_ptr<T, D>& ptr) const noexcept { return hash_finalize(reinterpret_cast<std::uintptr_t>(ptr.get())); }
		SEQ_ALWAYS_INLINE std::size_t operator()(const T* ptr) const noexcept { return hash_finalize(reinterpret_cast<std::uintptr_t>(ptr)); }
	};

	template<class T>
	struct hasher<std::shared_ptr<T>>
	{
		using is_avalanching = void;
		using is_transparent = void;
		SEQ_ALWAYS_INLINE std::size_t operator()(const std::shared_ptr<T>& ptr) const noexcept { return hash_finalize(reinterpret_cast<std::uintptr_t>(ptr.get())); }
		SEQ_ALWAYS_INLINE std::size_t operator()(const T* ptr) const noexcept { return hash_finalize(reinterpret_cast<std::uintptr_t>(ptr)); }
	};

	template<typename Enum>
	struct hasher<Enum, typename std::enable_if<std::is_enum_v<Enum>, void>::type>
	{
		using is_avalanching = void;
		using is_transparent = void;
		SEQ_ALWAYS_INLINE std::size_t operator()(Enum e) const noexcept
		{
			using Underlying = typename std::underlying_type<Enum>::type;
			return hasher<Underlying>{}(static_cast<Underlying>(e));
		}
		template<class Integral>
		SEQ_ALWAYS_INLINE std::size_t operator()(Integral e) const noexcept
		{
			return hasher<Integral>{}(e);
		}
	};

	template<class A, class B>
	struct hasher<std::pair<A, B>>
	{
		using is_avalanching = void;
		using is_transparent = void;
		template<class U, class V>
		SEQ_ALWAYS_INLINE std::size_t operator()(const std::pair<U, V>& p) const
		{
			std::size_t s = hash_value(hasher<A>{}, p.first);
			hash_combine(s, hash_value(hasher<B>{},p.second));
			return s;
		}
	};

	namespace detail
	{
		template<class Tuple, std::size_t Index = std::tuple_size<Tuple>::value - 1>
		struct HashTuple
		{
			template<class OtherTuple>
			static SEQ_ALWAYS_INLINE void apply(std::size_t& seed, OtherTuple const& tuple)
			{
				using elem_type = typename std::tuple_element<Index, Tuple>::type;
				HashTuple<Tuple, Index - 1>::apply(seed, tuple);
				hash_combine(seed, hash_value( hasher<elem_type>{},std::get<Index>(tuple)));
			}
		};

		template<class Tuple>
		struct HashTuple<Tuple, 0>
		{
			template<class OtherTuple>
			static SEQ_ALWAYS_INLINE void apply(std::size_t& seed, OtherTuple const& tuple)
			{
				using elem_type = typename std::tuple_element<0, Tuple>::type;
				hash_combine(seed, hash_value(hasher<elem_type>{},std::get<0>(tuple)));
			}
		};
	}

	template<class... Args>
	struct hasher<std::tuple<Args...>>
	{
		using is_avalanching = void;
		using is_transparent = void;
		template<class OtherTuple>
		SEQ_ALWAYS_INLINE std::size_t operator()(const OtherTuple& t) const
		{
			std::size_t seed = 0;
			detail::HashTuple<std::tuple<Args...>>::apply(seed, t);
			return seed;
		}
	};

	template<>
	struct hasher<std::tuple<>>
	{
		using is_avalanching = void;
		using is_transparent = void;

		std::size_t operator()(const std::tuple<>&) const noexcept { return 0; }
	};

	template<class Rep, class Ratio>
	struct hasher<std::chrono::duration<Rep, Ratio>>
	{
		using is_avalanching = void;
		using is_transparent = void;
		using type = std::chrono::duration<Rep, Ratio>;

		std::size_t operator()(const type& s) const noexcept { return hasher<Rep>{}(s.count()); }
		template<class T, class = std::enable_if_t<std::is_arithmetic_v<T>, void>>
		std::size_t operator()(T s) const noexcept
		{
			return hasher<Rep>{}(static_cast<Rep>(s));
		}
	};

	template<class Clock, class Duration>
	struct hasher<std::chrono::time_point<Clock, Duration>>
	{
		using is_avalanching = void;
		using is_transparent = void;
		using type = std::chrono::time_point<Clock, Duration>;
		using integral = decltype(type{}.time_since_epoch().count());

		std::size_t operator()(const type& s) const noexcept { return hasher<integral>{}(s.time_since_epoch().count()); }
		template<class T, class = std::enable_if_t<std::is_arithmetic_v<T>, void>>
		std::size_t operator()(T s) const noexcept
		{
			return hasher<integral>{}(static_cast<integral>(s));
		}
	};
}

/** @}*/
// end hash

#endif
