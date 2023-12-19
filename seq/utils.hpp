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

#ifndef SEQ_UTILS_HPP
#define SEQ_UTILS_HPP



/** @file */

#include <memory>

#include "bits.hpp"
#include "type_traits.hpp"


namespace seq
{

	/// @brief Memory Layout Management for containers like seq::sequence or seq::tiered_vector
	enum LayoutManagement
	{
		OptimizeForSpeed, //! Use more memory to favor speed
		OptimizeForMemory //! Use as few memory as possible
	};

	
	/// @brief Convenient random access iterator on a constant value
	template<class T>
	class cvalue_iterator
	{
		using alloc_traits = std::allocator_traits<std::allocator<T> >;
	public:
		using iterator_category = std::random_access_iterator_tag;
		using value_type = T;
		using difference_type = typename alloc_traits::difference_type;
		using size_type = typename alloc_traits::size_type;
		using pointer = typename alloc_traits::const_pointer;
		using reference = const value_type&;

		explicit cvalue_iterator(size_type _pos) : pos(_pos) {}
		cvalue_iterator(size_type _pos, const T& _value) :pos(_pos), value(_value) {}

		auto operator*() const noexcept -> reference {
			return value;
		}
		auto operator->() const noexcept -> pointer {
			return std::pointer_traits<pointer>::pointer_to(**this);
		}
		auto operator++() noexcept -> cvalue_iterator& {
			++pos;
			return *this;
		}
		auto operator++(int) noexcept -> cvalue_iterator {
			cvalue_iterator _Tmp = *this;
			++(*this);
			return _Tmp;
		}
		auto operator--() noexcept -> cvalue_iterator& {
			// TODO(VM213788): check decrement
			--pos;
			return *this;
		}
		auto operator--(int) noexcept -> cvalue_iterator {
			cvalue_iterator _Tmp = *this;
			--(*this);
			return _Tmp;
		}
		auto operator==(const cvalue_iterator& other) const noexcept -> bool {
			return pos == other.pos;
		}
		auto operator!=(const cvalue_iterator& other) const noexcept -> bool {
			return pos != other.pos;
		}
		auto operator+=(difference_type diff)noexcept -> cvalue_iterator& {
			pos += diff;
			return *this;
		}
		auto operator-=(difference_type diff)noexcept -> cvalue_iterator& {
			pos -= diff;
			return *this;
		}
		auto operator[](difference_type diff) const noexcept -> const value_type& {
			return value;
		}
		
		T value;
		size_type pos;
	};

	template < class T>
	auto operator+(const cvalue_iterator< T>& it, typename cvalue_iterator< T>::difference_type diff)noexcept -> cvalue_iterator< T> {
		cvalue_iterator< T> res = it;
		return res += diff;
	}
	template < class T>
	auto operator-(const cvalue_iterator< T>& it, typename cvalue_iterator< T>::difference_type diff)noexcept -> cvalue_iterator< T> {
		cvalue_iterator< T> res = it;
		return res -= diff;
	}
	template < class T>
	auto operator-(const cvalue_iterator< T>& it1, const cvalue_iterator< T>& it2)noexcept -> typename cvalue_iterator< T>::difference_type {
		return it1.pos - it2.pos;
	}
	template < class T>
	auto operator<(const cvalue_iterator< T>& it1, const cvalue_iterator< T>& it2)noexcept -> bool {
		return it1.pos < it2.pos;
	}
	template < class T>
	auto operator>(const cvalue_iterator< T>& it1, const cvalue_iterator< T>& it2) noexcept -> bool {
		return it1.pos > it2.pos;
	}
	template < class T>
	auto operator<=(const cvalue_iterator< T>& it1, const cvalue_iterator< T>& it2)noexcept -> bool {
		return it1.pos <= it2.pos;
	}
	template < class T>
	auto operator>=(const cvalue_iterator< T>& it1, const cvalue_iterator< T>& it2)noexcept -> bool {
		return it1.pos >= it2.pos;
	}

	/// @brief Simply call p->~T(), used as a replacement to std::allocator::destroy() which was removed in C++20
	template<class T>
	SEQ_ALWAYS_INLINE void destroy_ptr(T* p) noexcept
	{
		p->~T();
	}

	/// @brief Simply call new (p) T(...), used as a replacement to std::allocator::construct() which was removed in C++20
	template<class T, class... Args >
	SEQ_ALWAYS_INLINE void construct_ptr(T* p, Args&&... args) noexcept(noexcept(T(std::forward<Args>(args)...)))
	{
		new (p) T(std::forward<Args>(args)...);
	}


	namespace detail
	{
		// Extract the key on a std::pair or a Key value 
		template<class Key, class T>
		struct ExtractKey
		{
			using key_type = Key;
			using value_type = T;
			using mapped_type = typename T::second_type;
			static constexpr bool has_value = true;
			SEQ_ALWAYS_INLINE static auto key(const value_type& value) noexcept -> const key_type& { return value.first; }
			template<class U, class V>
			SEQ_ALWAYS_INLINE static auto key(const std::pair<U,V>& value) noexcept -> const U& { return value.first; }
			template<class U, class V>
			SEQ_ALWAYS_INLINE static auto key(const std::pair<const U, V>& value) noexcept -> const U& { return value.first; }
			template<class U>
			SEQ_ALWAYS_INLINE static auto key(const U& value) noexcept -> const U& { return value; }

			SEQ_ALWAYS_INLINE static auto value(const value_type& value) noexcept -> const key_type& { return value.second; }
			template<class U, class V>
			SEQ_ALWAYS_INLINE static auto value(const std::pair<U, V>& value) noexcept -> const U& { return value.second; }
			template<class U, class V>
			SEQ_ALWAYS_INLINE static auto value(const std::pair<const U, V>& value) noexcept -> const U& { return value.second; }
			template<class U>
			SEQ_ALWAYS_INLINE static auto value(const U& value) noexcept -> const U& { return value; }

			SEQ_ALWAYS_INLINE auto operator()(const value_type& value) noexcept -> const key_type& { return value.first; }
			template<class U, class V>
			SEQ_ALWAYS_INLINE auto operator()(const std::pair<U, V>& value) noexcept -> const U& { return value.first; }
			template<class U, class V>
			SEQ_ALWAYS_INLINE auto operator()(const std::pair<const U, V>& value) noexcept -> const U& { return value.first; }
			template<class U>
			SEQ_ALWAYS_INLINE auto operator()(const U& value) noexcept -> const U& { return value; }
		};
		template<class T>
		struct ExtractKey<T, T>
		{
			using key_type = T;
			using value_type = T;
			using mapped_type = T;
			static constexpr bool has_value = false;
			SEQ_ALWAYS_INLINE static auto key(const T& value) noexcept -> const T& { return value; }
			template<class U>
			SEQ_ALWAYS_INLINE static auto key(const U& value) noexcept -> const U& { return value; }

			SEQ_ALWAYS_INLINE static auto value(const T& value) noexcept -> const T& { return value; }
			template<class U>
			SEQ_ALWAYS_INLINE static auto value(const U& value) noexcept -> const U& { return value; }

			SEQ_ALWAYS_INLINE auto operator()(const T& value) noexcept -> const T& { return value; }
			template<class U>
			SEQ_ALWAYS_INLINE auto operator()(const U& value) noexcept -> const U& { return value; }
		};

		// Build a std::pair or Key value from forwarded arguments
		template<class T, class... Args >
		struct BuildValue
		{
			using type = T;
			SEQ_ALWAYS_INLINE static T build(Args&&... args) { return T(std::forward<Args>(args)...); }
		};
		template<class T>
		struct BuildValue<T, T>
		{
			using type = const T&;
			SEQ_ALWAYS_INLINE static const T& build(const T& v) { return v; }
		};
		template<class T>
		struct BuildValue<T, T&>
		{
			using type = const T&;
			SEQ_ALWAYS_INLINE static const T& build(const T& v) { return v; }
		};
		template<class T>
		struct BuildValue<T, T&&>
		{
			using type = T&&;
			SEQ_ALWAYS_INLINE static T&& build(T&& v) { return std::move(v); }
		};
		template<class T>
		struct BuildValue<T, const T&>
		{
			using type = const T&;
			SEQ_ALWAYS_INLINE static const T& build(const T& v) { return v; }
		};


		// Helper class to detect is_transparent typedef in hash functor or comparison functor

		
		template <class T, class = void>
		struct has_is_transparent : std::false_type {};

		template <class T>
		struct has_is_transparent<T,
			typename make_void<typename T::is_transparent>::type>
			: std::true_type {};


		template <class T, class = void>
		struct key_transparent : std::true_type {};

		template <class T>
		struct key_transparent<T,
			typename make_void<typename T::seq_unused_huge_typedef>::type>
			: std::true_type {};


		template <class T, class = void>
		struct has_is_always_equal : std::false_type {};

		template <class T>
		struct has_is_always_equal<T,
			typename make_void<typename T::is_always_equal>::type>
			: std::true_type {};

		/// Provide a is_always_equal type traits for allocators in case current compiler 
		/// std::allocator_traits::is_always_equal is not present.
		template<class Alloc, bool HasIsAlwaysEqual = has_is_always_equal<Alloc>::value>
		struct is_always_equal
		{
			using equal = typename std::allocator_traits<Alloc>::is_always_equal;
			static constexpr bool value = equal::value;
		};
		template<class Alloc>
		struct is_always_equal<Alloc,false>
		{
			static constexpr bool value = std::is_empty<Alloc>::value;
		};

		
		// Returns distance between 2 iterators, or 0 for non random access iterators
		template<class Iter, class Cat>
		auto iter_distance(const Iter& , const Iter& , Cat /*unused*/)  noexcept -> size_t { return 0; }
		template<class Iter>
		auto iter_distance(const Iter& first, const Iter& last, std::random_access_iterator_tag /*unused*/)  noexcept -> size_t { 
			return (last > first) ?  static_cast<size_t>(last - first) : 0; 
		}

	}

	/// @brief Returns the distance between first and last iterators for random access iterator category, 0 otherwise.
	template<class Iter>
	auto distance(const Iter& first, const Iter& last) noexcept -> size_t {
		return detail::iter_distance(first, last, typename std::iterator_traits<Iter>::iterator_category());
	}


	namespace detail
	{

		template<class L1, class L2, bool IsL1>
		struct CallLambda
		{
			template <class... Args>
			using return_type = decltype(std::declval<L1&>()(std::declval<Args>()...));

			template <class... Args>
			auto operator()(const L1& l1, const L2&  /*unused*/, Args&&... args) const -> return_type<Args ...>
			{
				return l1(std::forward<Args>(args)...);
			}
		};
		template<class L1, class L2>
		struct CallLambda<L1, L2, false>
		{
			template <class... Args>
			using return_type = decltype(std::declval<L2&>()(std::declval<Args>()...));

			template <class... Args>
			auto operator()(const L1&  /*unused*/, const L2& l2, Args&&... args) const -> return_type<Args ...>
			{
				return l2(std::forward<Args>(args)...);
			}
		};
	}

	/// @brief Simulation of C++17 if constexpr in C++14
	template<bool IsL1, class L1, class L2, class... Args>
	auto constexpr_if( const L1& l1,  const L2& l2, Args&&... args) 
		-> decltype(std::declval<detail::CallLambda<L1, L2, IsL1>&>()(std::declval<L1&>(), std::declval<L2&>(),std::declval<Args>()...))
	{
		return detail::CallLambda<L1, L2, IsL1>{}(l1,l2, std::forward<Args>(args)...);
	}

	// C++11 equal_to
	template <class _Ty = void>
	struct equal_to {
		typedef _Ty first_argument_type;
		typedef _Ty second_argument_type;
		typedef bool result_type;
		constexpr bool operator()(const _Ty& left, const _Ty& right) const {
			return left == right;
		}
	};
	template <>
	struct equal_to<void> {
		template <class _Ty1, class _Ty2>
		constexpr auto operator()(_Ty1&& left, _Ty2&& right) const
			noexcept(noexcept(static_cast<_Ty1&&>(left) == static_cast<_Ty2&&>(right))) 
			-> decltype(static_cast<_Ty1&&>(left) == static_cast<_Ty2&&>(right)) {
			return static_cast<_Ty1&&>(left) == static_cast<_Ty2&&>(right);
		}

		using is_transparent = int;
	};

	// C++11 less
	template <class _Ty = void>
	struct less {
		typedef _Ty first_argument_type;
		typedef _Ty second_argument_type;
		typedef bool result_type;
		constexpr bool operator()(const _Ty& left, const _Ty& right) const {
			return left < right;
		}
	};
	template <>
	struct less<void> {
		template <class _Ty1, class _Ty2>
		constexpr auto operator()(_Ty1&& left, _Ty2&& right) const
			noexcept(noexcept(static_cast<_Ty1&&>(left) < static_cast<_Ty2&&>(right)))
			-> decltype(static_cast<_Ty1&&>(left) < static_cast<_Ty2&&>(right)) {
			return static_cast<_Ty1&&>(left) < static_cast<_Ty2&&>(right);
		}

		using is_transparent = int;
	};

	// C++11 greater
	template <class _Ty = void>
	struct greater {
		typedef _Ty first_argument_type;
		typedef _Ty second_argument_type;
		typedef bool result_type;
		constexpr bool operator()(const _Ty& left, const _Ty& right) const {
			return left > right;
		}
	};
	template <>
	struct greater<void> {
		template <class _Ty1, class _Ty2>
		constexpr auto operator()(_Ty1&& left, _Ty2&& right) const
			noexcept(noexcept(static_cast<_Ty1&&>(left) > static_cast<_Ty2&&>(right)))
			-> decltype(static_cast<_Ty1&&>(left) > static_cast<_Ty2&&>(right)) {
			return static_cast<_Ty1&&>(left) > static_cast<_Ty2&&>(right);
		}

		using is_transparent = int;
	};

	/// @brief Copy allocator for container copy constructor
	template<class Allocator>
	auto copy_allocator(const Allocator& alloc)
		noexcept(std::is_nothrow_copy_constructible<Allocator>::value)
		-> Allocator
	{
		return std::allocator_traits< Allocator>::select_on_container_copy_construction(alloc);
	}

	/// @brief Swap allocators for container.swap member
	template <class Allocator>
	void swap_allocator(Allocator& left, Allocator& right) 
		noexcept(!std::allocator_traits<Allocator>::propagate_on_container_swap::value || 
			std::allocator_traits<Allocator>::is_always_equal::value)
	{
		if SEQ_CONSTEXPR (std::allocator_traits<Allocator>::propagate_on_container_swap::value) {
			std::swap(left, right);
		}
		else {
			SEQ_ASSERT_DEBUG(left == right, "containers incompatible for swap");
		}
	}

	/// @brief Assign allocator for container copy operator
	template <class Allocator>
	void assign_allocator(Allocator& left, const Allocator& right)
		noexcept (!std::allocator_traits<Allocator>::propagate_on_container_copy_assignment::value ||
			std::is_nothrow_copy_assignable<Allocator>::value)
	{
		if SEQ_CONSTEXPR (std::allocator_traits<Allocator>::propagate_on_container_copy_assignment::value) {
			left = right;
		}
	}

	/// @brief Move allocator for container move assignment
	template <class Allocator>
	void move_allocator(Allocator& left, Allocator& right) 
		noexcept(!std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value ||
			std::is_nothrow_move_assignable<Allocator>::value)
	{
		// (maybe) propagate on container move assignment
		if SEQ_CONSTEXPR (std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value) {
			left = std::move(right);
		}
	}

	// Returns whether an attempt to propagate allocators is necessary in copy assignment operations.
	// Note that even when false_type, callers should call assign_allocator as we want to assign allocators even when equal.
	template <class Allocator>
	struct assign_alloc
	{
		static constexpr bool value = std::allocator_traits<Allocator>::propagate_on_container_copy_assignment::value
			&& !detail::is_always_equal<Allocator>::value;
	};

	template <class Allocator>
	struct move_alloc
	{
		static constexpr bool value = std::allocator_traits<Allocator>::propagate_on_container_move_assignment::type
			&& !detail::is_always_equal<Allocator>::value;
	};


}//end namespace seq

#endif
