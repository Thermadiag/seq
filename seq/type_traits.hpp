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

#ifndef SEQ_TYPE_TRAITS_HPP
#define SEQ_TYPE_TRAITS_HPP

#include <ostream>
#include <istream>
#include <memory>
#include <type_traits>
#include <functional>
#include <limits>
#include <cstdint>
#include <tuple>
#include <iterator>

namespace seq
{
	/// @brief Compute integer type maximum value at compile time
	template<class T, bool Signed = std::is_signed<T>::value>
	struct integer_max
	{
		static constexpr T value = std::numeric_limits<T>::max(); // static_cast<T>( ~(static_cast < T>(1) << (static_cast<T>(sizeof(T) * 8) - static_cast < T>(1))) );
	};
	template<class T>
	struct integer_max<T, false>
	{
		static constexpr T value = static_cast<T>(-1);
	};

	/// @brief Compute integer type minimum value at compile time
	template<class T, bool Signed = std::is_signed<T>::value>
	struct integer_min
	{
		static constexpr T value = (-integer_max<T>::value) - static_cast<T>(1);
	};
	template<class T>
	struct integer_min<T, false>
	{
		static constexpr T value = static_cast<T>(0);
	};

	/// @brief Define the return type of seq::negate_if_signed and seq::abs
	template<class T, bool Signed = std::is_signed<T>::value, size_t Size = sizeof(T)>
	struct integer_abs_return
	{
		using type = T;
	};
	template<class T>
	struct integer_abs_return<T, true, 1>
	{
		using type = std::uint16_t;
	};
	template<class T>
	struct integer_abs_return<T, true, 2>
	{
		using type = std::uint32_t;
	};
	template<class T>
	struct integer_abs_return<T, true, 4>
	{
		using type = std::uint64_t;
	};
	template<class T>
	struct integer_abs_return<T, true, 8>
	{
		using type = std::uint64_t;
	};

	/// @brief Returns -v if v is signed, v otherwise.
	template<class T>
	auto negate_if_signed(T v) -> typename integer_abs_return<T>::type
	{
		if constexpr (std::is_signed_v<T>)
			return static_cast<std::make_unsigned_t<T>>(-v);
		else
			return v;
	}
	/// @brief Returns absolute value of v.
	template<class T>
	auto abs(T v) -> typename integer_abs_return<T>::type
	{
		if constexpr (std::is_signed_v<T>)
			return static_cast<std::make_unsigned_t<T>>(v < 0 ? -v : v);
		else
			return v;
	}

	/// Check if iterator is random access
	template<class Iter>
	struct is_random_access : std::is_same<std::random_access_iterator_tag, typename std::iterator_traits<Iter>::iterator_category>
	{
	};
	template<class Iter>
	constexpr bool is_random_access_v = is_random_access<Iter>::value;

	/// Check if iterator is a std::reverse_iterator
	template<class It>
	struct is_reverse_iterator : std::false_type
	{
	};
	template<class It>
	struct is_reverse_iterator<std::reverse_iterator<It>> : std::true_type
	{
	};
	template<class Iter>
	constexpr bool is_reverse_iterator_v = is_reverse_iterator<Iter>::value;

	/// @brief Inherits std::true_type is T is of type std::unique_ptr<...>, false otherwise
	template<class T>
	struct is_unique_ptr : std::false_type
	{
	};
	template<class T, class Del>
	struct is_unique_ptr<std::unique_ptr<T, Del>> : std::true_type
	{
	};
	template<class T>
	constexpr bool is_unique_ptr_v = is_unique_ptr<T>::value;

	/// @brief Type trait telling if a class is relocatable or not.
	///
	/// A type is considered relocatable if these consecutive calls
	/// \code{.cpp}
	/// new(new_place) T(std::move(old_place));
	/// old_place.~T();
	/// \endcode
	/// can be replaced by
	/// \code{.cpp}
	/// memcpy(&new_place, &old_place, sizeof(T));
	/// \endcode
	///
	/// This property is used to optimize containers like seq::devector, seq::tiered_vector, seq::flat_(map/set/multimap/multiset).
	///
	template<class T>
	struct is_relocatable
	{
		static constexpr bool value = std::is_trivially_copyable<T>::value && std::is_trivially_destructible<T>::value;
	};

	template<class T>
	constexpr bool is_relocatable_v = is_relocatable<T>::value;

	// Specilizations for unique_ptr, shared_ptr and pair

	template<class T, class D>
	struct is_relocatable<std::unique_ptr<T, D>>
	{
		static constexpr bool value = is_relocatable<D>::value;
	};
	template<class T>
	struct is_relocatable<std::shared_ptr<T>>
	{
		static constexpr bool value = true;
	};
	template<class T, class V>
	struct is_relocatable<std::pair<T, V>>
	{
		static constexpr bool value = is_relocatable<T>::value && is_relocatable<V>::value;
	};
	template<class T>
	struct is_relocatable<std::allocator<T>>
	{
		static constexpr bool value = true;
	};

	namespace detail
	{
		template<class Tuple, size_t Pos = std::tuple_size<Tuple>::value>
		struct is_relocatable_tupe
		{
			using type = typename std::tuple_element<std::tuple_size<Tuple>::value - Pos, Tuple>::type;
			static constexpr bool value = seq::is_relocatable<type>::value && is_relocatable_tupe<Tuple, Pos - 1>::value;
		};
		template<class Tuple>
		struct is_relocatable_tupe<Tuple, 0>
		{
			static constexpr bool value = true;
		};
	}
	template<class... Args>
	struct is_relocatable<std::tuple<Args...>> : detail::is_relocatable_tupe<std::tuple<Args...>>
	{
	};

	template<class T>
	struct is_tuple : std::bool_constant<false>
	{
	};
	template<class... Args>
	struct is_tuple<std::tuple<Args...>> : std::bool_constant<true>
	{
	};

	/// @brief Tells if given type is hashable with std::hash.
	/// True by default, optimistically assume that all types are hashable.
	/// Used by seq::hold_any.
	template<class T>
	struct is_hashable : std::true_type
	{
	};
	template<class T>
	constexpr bool is_hashable_v = is_hashable<T>::value;

	/// @brief Tells if given type can be streamed to a std::ostream object
	template<class T, class = void>
	struct is_ostreamable : std::false_type
	{
	};

	template<class T>
	struct is_ostreamable<T, std::void_t<decltype(std::declval<std::ostream&>() << std::declval<const T&>())>> : std::true_type
	{
	};

	/// @brief Tells if given type can be read from a std::istream object
	template<class T, class = void>
	struct is_istreamable : std::false_type
	{
	};

	template<class T>
	struct is_istreamable<T, std::void_t<decltype(std::declval<std::istream&>() >> std::declval<T&>())>> : std::true_type
	{
	};

	/// @brief Tells if given type supports equality comparison with operator ==
	template<class T, class = void>
	struct is_equal_comparable : std::false_type
	{
	};
	template<class T>
	struct is_equal_comparable<T, std::void_t<decltype(std::declval<T&>() == std::declval<T&>())>> : std::true_type
	{
	};

	/// @brief Tells if given type supports comparison with operator <
	template<class T, class = void>
	struct is_less_comparable : std::false_type
	{
	};
	template<class T>
	struct is_less_comparable<T, std::void_t<decltype(std::declval<T&>() < std::declval<T&>())>> : std::true_type
	{
	};

	template<class F>
	struct is_function_pointer : std::false_type
	{
	};
	template<class R, class... A>
	struct is_function_pointer<R (*)(A...)> : std::true_type
	{
	};

	/// @brief Check if type provides the 'iterator' typedef
	template<class T, class = void>
	struct has_iterator : std::false_type
	{
	};

	template<class T>
	struct has_iterator<T, std::void_t<typename T::iterator>> : std::true_type
	{
	};

	/// @brief Check if type provides the 'value_type' typedef
	template<class T, class = void>
	struct has_value_type : std::false_type
	{
	};

	template<class T>
	struct has_value_type<T, std::void_t<typename T::value_type>> : std::true_type
	{
	};

	template<class C>
	struct is_iterable
	{
		static constexpr bool value = has_iterator<C>::value && has_value_type<C>::value;
	};

	template<class T, class = void>
	struct has_is_transparent : std::false_type
	{
	};

	template<class T>
	struct has_is_transparent<T, std::void_t<typename T::is_transparent>> : std::true_type
	{
	};

	template<class T, class = void>
	struct has_is_avalanching : std::false_type
	{
	};

	template<class T>
	struct has_is_avalanching<T, std::void_t<typename T::is_avalanching>> : std::true_type
	{
	};

	template<class T, class = void>
	struct has_is_always_equal : std::false_type
	{
	};

	template<class T>
	struct has_is_always_equal<T, std::void_t<typename T::is_always_equal>> : std::true_type
	{
	};

	template<class T, class = void>
	struct has_comparable : std::false_type
	{
	};
	template<class T>
	struct has_comparable<T, std::void_t<typename T::comparable>> : std::true_type
	{
	};

	template<class T, class = void>
	struct has_plus_equal : std::false_type
	{
	};
	template<class T>
	struct has_plus_equal<T, std::void_t<decltype(std::declval<T&>() += 1)>> : std::true_type
	{
	};

	/// Provide a is_always_equal type traits for allocators in case current compiler
	/// std::allocator_traits::is_always_equal is not present.
	template<class Alloc, bool HasIsAlwaysEqual = has_is_always_equal<Alloc>::value>
	struct is_always_equal
	{
		using equal = typename std::allocator_traits<Alloc>::is_always_equal;
		static constexpr bool value = equal::value;
	};
	template<class Alloc>
	struct is_always_equal<Alloc, false>
	{
		static constexpr bool value = std::is_empty_v<Alloc>;
	};

	namespace metafunction
	{
		template<class MetaFunction>
		using result_of = typename MetaFunction::result;

		template<class Tuple, template<class> class Function>
		struct transform_elements;

		// meta-function which takes a tuple and a unary metafunction
		// and yields a tuple of the result of applying the metafunction
		// to each element_type of the tuple.
		// type: binary metafunction
		// arg1 = the tuple of types to be wrapped
		// arg2 = the unary metafunction to apply to each element_type
		// returns tuple<result_of<arg2<element>>...> for each element in arg1

		template<class... Elements, template<class> class UnaryMetaFunction>
		struct transform_elements<std::tuple<Elements...>, UnaryMetaFunction>
		{
			template<class Arg>
			using function = UnaryMetaFunction<Arg>;
			using result = std::tuple<result_of<function<Elements>>...>;
		};
	}
}
#endif
