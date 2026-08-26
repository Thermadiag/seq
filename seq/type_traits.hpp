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
	template<class T, bool Signed = std::is_signed_v<T>>
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
	template<class T, bool Signed = std::is_signed_v<T>>
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
	template<class T, bool Signed = std::is_signed_v<T>, size_t Size = sizeof(T)>
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
		using U = std::make_unsigned_t<T>;
		U u = static_cast<U>(v);
		return v < 0 ? U(0) - u : u;
	}
	/// @brief Returns absolute value of v.
	template<class T>
	auto abs(T v) -> typename integer_abs_return<T>::type
	{
		if constexpr (std::is_signed_v<T>)
			return static_cast<std::make_unsigned_t<T>>(negate_if_signed(v));
		else
			return v;
	}

	/// Check if iterator is random access
	template<class Iter>
	struct is_random_access : std::is_base_of<std::random_access_iterator_tag, typename std::iterator_traits<Iter>::iterator_category>
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
		static constexpr bool value = std::is_trivially_copyable_v<T> && std::is_trivially_destructible_v<T>;
	};

	template<class T>
	constexpr bool is_relocatable_v = is_relocatable<T>::value;

	// Specilizations for unique_ptr, shared_ptr and pair

	template<class T, class D>
	struct is_relocatable<std::unique_ptr<T, D>> : std::bool_constant<is_relocatable_v<D> && is_relocatable_v<typename std::unique_ptr<T, D>::pointer>>
	{
	};
	template<class T, class V>
	struct is_relocatable<std::pair<T, V>> : std::bool_constant<is_relocatable<T>::value && is_relocatable<V>::value>
	{
	};
	template<class T>
	struct is_relocatable<std::allocator<T>> : std::true_type
	{
	};
	template<class... Args>
	struct is_relocatable<std::tuple<Args...>> : std::bool_constant<(is_relocatable_v<Args> && ...)>
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


	namespace detail
	{
		// To allow ADL with custom begin/end
		using std::begin;
		using std::end;

		template<class T>
		auto is_iterable_impl(int) -> decltype(begin(std::declval<const T&>()) != end(std::declval<const T&>()),   // begin/end and operator !=
						       void(),						       // Handle evil operator ,
						       ++std::declval<decltype(begin(std::declval<const T&>()))&>(), // operator ++
						       void(*begin(std::declval<const T&>())),		       // operator*
						       std::true_type{});

		template<typename T>
		std::false_type is_iterable_impl(...);
	}

	/// @brief Check if type is iterable
	template<class T>
	using is_iterable = decltype(detail::is_iterable_impl<T>(0));
	
	template<class T>
	constexpr bool is_iterable_v = is_iterable<T>::value;

	/// @brief Check if givern type is an iterator
	template<class T, class = void>
	struct is_iterator : std::false_type
	{
	};

	template<class T>
	struct is_iterator<T, std::void_t<typename std::iterator_traits<T>::iterator_category>> : std::true_type
	{
	};
	template<class T>
	constexpr bool is_iterator_v = is_iterator<T>::value;


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


	/// @brief Detect if T is a char type (removing const and reference)
	template<class T>
	struct is_character_type
	{
		using C = typename std::decay<T>::type;
		static constexpr bool value = std::is_same_v<C, char> || std::is_same_v<C, wchar_t> || std::is_same_v<C, char16_t> || std::is_same_v<C, char32_t>
#ifdef SEQ_HAS_CPP_20
					      || std::is_same_v<C, char8_t>
#endif
		  ;
	};
	template<class T>
	constexpr bool is_character_type_v = is_character_type<T>::value;

	/// @brief Detect if type has member data() returning a pointer
	template<class T, class = void>
	struct has_data_pointer : std::false_type
	{
		using type = char;
	};
	template<class T>
	struct has_data_pointer<T, typename std::void_t<decltype(std::declval<const T&>().data())>>
	{
		using pointer = std::decay_t<decltype(std::declval<const T&>().data())>;
		using ctype = typename std::remove_pointer<pointer>::type;
		using type = typename std::remove_const<ctype>::type;
		static constexpr bool value = std::is_pointer_v<pointer>;
	};
	template<class T>
	constexpr bool has_data_pointer_v = has_data_pointer<T>::value;

	/// @brief Detect if type has member size() returning an integral
	template<class T, class = void>
	struct has_size : std::false_type
	{
		using type = char;
	};
	template<class T>
	struct has_size<T, typename std::void_t<decltype(std::declval<const T&>().size())>>
	{
		using type = decltype(std::declval<const T&>().size());
		static constexpr bool value = std::is_integral_v<type>;
	};
	template<class T>
	constexpr bool has_size_v = has_size<T>::value;


	template<class T, class  = void>
	struct is_string_class : std::false_type
	{
	};
	template<class T>
	struct is_string_class<T, std::void_t<typename T::value_type>>
	{
		static constexpr bool value = is_iterable_v<T> && has_data_pointer_v<T> && has_size_v<T> && is_character_type_v<typename T::value_type>;
	};
	template<class T>
	constexpr bool is_string_class_v = is_string_class<T>::value;

	template<class T, class Char, class = void>
	struct is_string_class_for : std::false_type
	{
	};
	template<class T, class Char>
	struct is_string_class_for<T, Char, std::void_t<typename T::value_type>>
	{
		static constexpr bool value = is_iterable_v<T> && has_data_pointer_v<T> && has_size_v<T> && std::is_same_v<typename T::value_type, Char> && std::is_same_v<typename has_data_pointer<T>::type, Char>;
	};
	template<class T, class Char>
	constexpr bool is_string_class_for_v = is_string_class_for<T,Char>::value;


	/// @brief Detect if T if a char pointer or array
	template<class T>
	struct is_character_pointer
	{
		using decayed = std::decay_t<T>;
		using c_type = typename std::remove_pointer<decayed>::type;
		using char_type = typename std::remove_const<c_type>::type;
		static constexpr bool value = (std::is_pointer_v<decayed> || std::is_array_v<decayed>) && is_character_type<char_type>::value;
	};
	template<class T>
	constexpr bool is_character_pointer_v = is_character_pointer<T>::value;


	/// @brief Detect all possible string types of any character type (std::basic_string, basic_tstring, std::basic_string_view, const Char*, Char*
	template<class T>
	struct is_generic_string
	{
		static constexpr bool value = is_string_class_v<T> || is_character_pointer_v<T>;
	};
	template<class T>
	constexpr bool is_generic_string_v = is_generic_string<T>::value;


	/// @brief Similar to is_generic_string, but only validate given character type
	template<class T, class Char>
	struct is_generic_string_for
	{
		static constexpr bool value = is_string_class_for_v<T, Char> || (is_character_pointer_v<T> && std::is_same_v<Char, typename is_character_pointer<T>::char_type>);
	};
	template<class T, class Char>
	constexpr bool is_generic_string_for_v = is_generic_string_for<T,Char>::value;


	/// @brief Detect generic string character type
	template<class T, class = void>
	struct character_type
	{
		using type = void;
	};
	template<class T>
	struct character_type<T, typename std::enable_if<is_character_pointer_v<T>, void>::type>
	{
		using type = typename is_character_pointer<T>::char_type;
	};
	template<class T>
	struct character_type<T, typename std::enable_if<is_string_class_v<T>, void>::type>
	{
		using type = typename T::value_type;
	};
	template<class T>
	using character_type_t = typename character_type<T>::type;

}
#endif
