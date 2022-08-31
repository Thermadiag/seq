#pragma once

#include <string>
#include <type_traits>
#include <memory>
#include <functional>

namespace seq
{

	/// @brief Compute integer type maximum value at compile time
	template <class T, bool Signed = std::is_signed<T>::value>
	struct integer_max
	{
		static const T value = static_cast<T>( ~(static_cast < T>(1) << (static_cast<T>(sizeof(T) * 8) - static_cast < T>(1))) );
	};
	template <class T>
	struct integer_max<T,false>
	{
		static const T value = static_cast<T>(-1);
	};

	/// @brief Compute integer type minimum value at compile time
	template <class T, bool Signed = std::is_signed<T>::value>
	struct integer_min
	{
		static const T value =  (-integer_max<T>::value) - static_cast < T>(1) ;
	};
	template <class T>
	struct integer_min<T,false>
	{
		static const T value = static_cast < T>(0);
	};

	/// @brief Define the return type of seq::negate_if_signed and seq::abs
	template<class T, bool Signed = std::is_signed<T>::value, size_t Size = sizeof(T)>
	struct integer_abs_return
	{
		using type = T;
	};
	template<class T>
	struct integer_abs_return<T,true,1>
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

	namespace detail
	{
		template<class T, bool Signed = std::is_signed<T>::value>
		struct IntegerAbs
		{
			using type = typename integer_abs_return<T>::type;
			static inline type neg_if_signed(T v) { return static_cast<type>(-v); }
			static inline type abs(T v) { return static_cast<type>(v < 0 ? -v : v); }
		};
		template<class T>
		struct IntegerAbs<T,false>
		{
			using type = T;
			static inline T neg_if_signed(T v) { return v; }
			static inline T abs(T v) { return v; }
		};
	}

	/// @brief Returns -v if v is signed, v otherwise.
	template<class T>
	typename integer_abs_return<T>::type negate_if_signed(T v) { return detail::IntegerAbs<T>::neg_if_signed(v); }
	/// @brief Returns absolute value of v.
	template<class T>
	typename integer_abs_return<T>::type abs(T v) { return detail::IntegerAbs<T>::abs(v); }




	namespace detail
	{
		template<class T>
		struct unique_ptr_traits {};

		template<class T, class Del>
		struct unique_ptr_traits<std::unique_ptr<T, Del> >
		{
			using value_type = T;
			using pointer = T*;
			using const_pointer = const T*;
			using reference = T&;
			using const_reference = const T&;
		};
		template<class T, class Del>
		struct unique_ptr_traits<const std::unique_ptr<T, Del> >
		{
			using value_type = T;
			using pointer = const T*;
			using const_pointer = const T*;
			using reference = const T&;
			using const_reference = const T&;
		};
	}

	/// @brief Inherits std::true_type is T is of type std::unique_ptr<...>, false otherwise
	template<class T>
	struct is_unique_ptr : std::false_type {};

	template<class T, class Del>
	struct is_unique_ptr<std::unique_ptr<T, Del> > : std::true_type {};

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
	struct is_relocatable {
		static constexpr bool value = std::is_trivially_copyable<T>::value && std::is_trivially_destructible<T>::value;
	};

	// Specilizations for unique_ptr, shared_ptr and pair

	template<class T, class D>
	struct is_relocatable<std::unique_ptr<T, D> > {
		static constexpr bool value = is_relocatable<D>::value;
	};
	template<class T>
	struct is_relocatable<std::shared_ptr<T> > {
		static constexpr bool value = true;
	};
	template<class T, class V>
	struct is_relocatable<std::pair<T, V> > {
		static constexpr bool value = is_relocatable<T>::value && is_relocatable<V>::value;
	};
	template<class T>
	struct is_relocatable<std::allocator<T> > {
		static constexpr bool value = true;
	};



	// On msvc, std::string is relocatable, as opposed to gcc implementation that stores a pointer to its internal data
#if defined( _MSC_VER) 
	template<class T, class Traits, class Alloc>
	struct is_relocatable<std::basic_string<T, Traits, Alloc> > {
		static constexpr bool value = true;
	};
#endif

	/// @brief Tells if given type is hashable with std::hash.
	/// True by default, optimistically assume that all types are hashable.
	/// Used by seq::hold_any.
	template<class T>
	struct is_hashable : std::true_type {};

	/// @brief Tells if given type can be streamed to a std::ostream object
	template<class T>
	class is_ostreamable
	{
		template<class SS, class TT>
		static auto test(int)
			-> decltype(std::declval<SS&>() << std::declval<TT>(), std::true_type());

		template<class, class>
		static auto test(...)->std::false_type;

	public:
		static constexpr bool value = decltype(test<std::ostream, T>(0))::value;
	};

	/// @brief Tells if given type can be read from a std::istream object
	template<class T>
	class is_istreamable
	{
		template<class SS, class TT>
		static auto test(int)
			-> decltype(std::declval<SS&>() >> std::declval<TT&>(), std::true_type());

		template<class, class>
		static auto test(...)->std::false_type;

	public:
		static constexpr bool value = decltype(test<std::istream, T>(0))::value;
	};

	/// @brief Tells if given type supports equality comparison with operator ==
	template<class T>
	class is_equal_comparable
	{
		template<class TT>
		static auto test(int)
			-> decltype(std::declval<TT&>() == std::declval<TT&>(), std::true_type());

		template<class>
		static auto test(...)->std::false_type;

	public:
		static constexpr bool value = decltype(test<T>(0))::value;
	};

	/// @brief Tells if given type supports comparison with operator <
	template<class T>
	class is_less_comparable
	{
		template<class TT>
		static auto test(int)
			-> decltype(std::declval<TT&>() < std::declval<TT&>(), std::true_type());

		template<class>
		static auto test(...)->std::false_type;

	public:
		static constexpr bool value = decltype(test<T>(0))::value;
	};

	/// @brief Tells if given type supports invocation with signature void(Args ...)
	/// Equivalent to C++17 std::is_invocable
	template <typename F, typename... Args>
	struct is_invocable :
		std::is_constructible<
		std::function<void(Args ...)>,
		std::reference_wrapper<typename std::remove_reference<F>::type>
		>
	{
	};

	/// @brief Tells if given type supports invocation with signature R(Args ...)
	/// Equivalent to C++17 std::is_invocable_r
	template <typename R, typename F, typename... Args>
	struct is_invocable_r :
		std::is_constructible<
		std::function<R(Args ...)>,
		std::reference_wrapper<typename std::remove_reference<F>::type>
		>
	{
	};
}