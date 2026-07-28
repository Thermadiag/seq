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

#ifndef SEQ_UTILS_HPP
#define SEQ_UTILS_HPP

/** @file */

#include <memory>

#include "../bits.hpp"
#include "../type_traits.hpp"

namespace seq
{
	/// @brief Convenient random access iterator on a constant value
	template<class T>
	class cvalue_iterator
	{
		using alloc_traits = std::allocator_traits<std::allocator<T>>;

	public:
		using iterator_category = std::random_access_iterator_tag;
		using value_type = T;
		using difference_type = typename alloc_traits::difference_type;
		using size_type = typename alloc_traits::size_type;
		using pointer = typename alloc_traits::const_pointer;
		using reference = const value_type&;

		explicit cvalue_iterator(size_type _pos)
		  : pos(_pos)
		{
		}
		cvalue_iterator(size_type _pos, const T& _value)
		  : pos(_pos)
		  , value(_value)
		{
		}

		auto operator*() const noexcept -> reference { return value; }
		auto operator->() const noexcept -> pointer { return std::pointer_traits<pointer>::pointer_to(**this); }
		auto operator++() noexcept -> cvalue_iterator&
		{
			++pos;
			return *this;
		}
		auto operator++(int) noexcept -> cvalue_iterator
		{
			cvalue_iterator _Tmp = *this;
			++(*this);
			return _Tmp;
		}
		auto operator--() noexcept -> cvalue_iterator&
		{
			// TODO(VM213788): check decrement
			--pos;
			return *this;
		}
		auto operator--(int) noexcept -> cvalue_iterator
		{
			cvalue_iterator _Tmp = *this;
			--(*this);
			return _Tmp;
		}
		auto operator==(const cvalue_iterator& other) const noexcept -> bool { return pos == other.pos; }
		auto operator!=(const cvalue_iterator& other) const noexcept -> bool { return pos != other.pos; }
		auto operator+=(difference_type diff) noexcept -> cvalue_iterator&
		{
			pos += diff;
			return *this;
		}
		auto operator-=(difference_type diff) noexcept -> cvalue_iterator&
		{
			pos -= diff;
			return *this;
		}
		auto operator[](difference_type diff) const noexcept -> const value_type& { return value; }

		T value;
		size_type pos;
	};

	template<class T>
	auto operator+(const cvalue_iterator<T>& it, typename cvalue_iterator<T>::difference_type diff) noexcept -> cvalue_iterator<T>
	{
		cvalue_iterator<T> res = it;
		return res += diff;
	}
	template<class T>
	auto operator-(const cvalue_iterator<T>& it, typename cvalue_iterator<T>::difference_type diff) noexcept -> cvalue_iterator<T>
	{
		cvalue_iterator<T> res = it;
		return res -= diff;
	}
	template<class T>
	auto operator-(const cvalue_iterator<T>& it1, const cvalue_iterator<T>& it2) noexcept -> typename cvalue_iterator<T>::difference_type
	{
		return it1.pos - it2.pos;
	}
	template<class T>
	auto operator<(const cvalue_iterator<T>& it1, const cvalue_iterator<T>& it2) noexcept -> bool
	{
		return it1.pos < it2.pos;
	}
	template<class T>
	auto operator>(const cvalue_iterator<T>& it1, const cvalue_iterator<T>& it2) noexcept -> bool
	{
		return it1.pos > it2.pos;
	}
	template<class T>
	auto operator<=(const cvalue_iterator<T>& it1, const cvalue_iterator<T>& it2) noexcept -> bool
	{
		return it1.pos <= it2.pos;
	}
	template<class T>
	auto operator>=(const cvalue_iterator<T>& it1, const cvalue_iterator<T>& it2) noexcept -> bool
	{
		return it1.pos >= it2.pos;
	}

	/// @brief Simply call p->~T(), used as a replacement to std::allocator::destroy() which was removed in C++20
	template<class T>
	SEQ_ALWAYS_INLINE void destroy_ptr(T* p) noexcept
	{
		p->~T();
	}

	/// @brief Simply call new (p) T(...), used as a replacement to std::allocator::construct() which was removed in C++20
	template<class T, class... Args>
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
			SEQ_ALWAYS_INLINE static auto key(const std::pair<U, V>& value) noexcept -> const U&
			{
				return value.first;
			}
			template<class U, class V>
			SEQ_ALWAYS_INLINE static auto key(const std::pair<const U, V>& value) noexcept -> const U&
			{
				return value.first;
			}
			template<class U>
			SEQ_ALWAYS_INLINE static auto key(const U& value) noexcept -> const U&
			{
				return value;
			}

			SEQ_ALWAYS_INLINE static auto value(const value_type& value) noexcept -> const key_type& { return value.second; }
			template<class U, class V>
			SEQ_ALWAYS_INLINE static auto value(const std::pair<U, V>& value) noexcept -> const U&
			{
				return value.second;
			}
			template<class U, class V>
			SEQ_ALWAYS_INLINE static auto value(const std::pair<const U, V>& value) noexcept -> const U&
			{
				return value.second;
			}
			template<class U>
			SEQ_ALWAYS_INLINE static auto value(const U& value) noexcept -> const U&
			{
				return value;
			}

			SEQ_ALWAYS_INLINE auto operator()(const value_type& value) noexcept -> const key_type& { return value.first; }
			template<class U, class V>
			SEQ_ALWAYS_INLINE auto operator()(const std::pair<U, V>& value) noexcept -> const U&
			{
				return value.first;
			}
			template<class U, class V>
			SEQ_ALWAYS_INLINE auto operator()(const std::pair<const U, V>& value) noexcept -> const U&
			{
				return value.first;
			}
			template<class U>
			SEQ_ALWAYS_INLINE auto operator()(const U& value) noexcept -> const U&
			{
				return value;
			}
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
			SEQ_ALWAYS_INLINE static auto key(const U& value) noexcept -> const U&
			{
				return value;
			}

			SEQ_ALWAYS_INLINE static auto value(const T& value) noexcept -> const T& { return value; }
			template<class U>
			SEQ_ALWAYS_INLINE static auto value(const U& value) noexcept -> const U&
			{
				return value;
			}

			SEQ_ALWAYS_INLINE auto operator()(const T& value) noexcept -> const T& { return value; }
			template<class U>
			SEQ_ALWAYS_INLINE auto operator()(const U& value) noexcept -> const U&
			{
				return value;
			}
		};

		/// @brief Build value from any kind of argument,
		/// used by emplace() member of containers
		template<class T, bool IsTransparent>
		struct BuildValue
		{
			template<class U>
			static auto make(U&& u) -> typename std::enable_if<IsTransparent, U&&>::type
			{
				// For one argument and transparency: perfect forwarding
				return std::forward<U>(u);
			}
			template<class... Args>
			static SEQ_ALWAYS_INLINE T make(Args&&... args)
			{
				// For several arguments or no transparency: construct from args
				return T(std::forward<Args>(args)...);
			}
			static SEQ_ALWAYS_INLINE T&& make(T&& val) noexcept { return std::move(val); }
			static SEQ_ALWAYS_INLINE const T& make(const T& val) noexcept { return val; }
			static SEQ_ALWAYS_INLINE T& make(T& val) noexcept { return val; }
		};

		template<class T, bool WithValue = true>
		struct ResizeHelper
		{
			T val;
			template<class... U>
			ResizeHelper(const U&... vals)
			  : val(std::forward<const U&>(vals)...)
			{
			}
			ResizeHelper() noexcept(std::is_nothrow_default_constructible_v<T>) {}
			void construct(T* dst) const noexcept(std::is_nothrow_copy_constructible_v<T>) { construct_ptr(dst, val); }
		};
		template<class T>
		struct ResizeHelper<T,false>
		{
			template<class... U>
			ResizeHelper(const U&... vals)
			{
			}
			ResizeHelper() noexcept(std::is_nothrow_default_constructible_v<T>) {}
			void construct(T* dst) const noexcept(std::is_nothrow_default_constructible_v<T>) { construct_ptr(dst); }
		};
		template<class T>
		struct ResizeHelperDirect
		{
			const T& val;
			void construct(T* dst) const noexcept(std::is_nothrow_default_constructible_v<T>) { construct_ptr(dst,val); }
		};
		// Returns a helper class used by
		// containers providing both members
		// resize(size_t size) and resize(size_t size, const T & value)
		template<class T, class... U>
		auto resize_helper(const U&... vals)
		{
			static_assert(sizeof...(U) < 2, "invalid number of arguments for function resize()");
			return ResizeHelper < T, sizeof...(U) == 1 > (std::forward<const U&>(vals)...);
		}
		template<class T>
		auto resize_helper(const T & v)
		{
			return ResizeHelperDirect<T>{ v };
		}

		template<class Allocator, class T>
		using RebindAllocator = typename std::allocator_traits<Allocator>::template rebind_alloc<T>;
	}


	/// @brief Allocate count elements of type T using provided allocator
	template<class T, class Alloc>
	T* allocate_from(const Alloc& al, size_t count = 1)
	{
		detail::RebindAllocator<Alloc, T> alloc{ al };
		return alloc.allocate(count);
	}
	/// @brief Deallocate count elements using provided allocator
	template<class Alloc, class T>
	void deallocate_from(const Alloc& al, T* p, size_t count = 1) noexcept
	{
		detail::RebindAllocator<Alloc, T> alloc{ al };
		alloc.deallocate(p, count);
	}

	/// @brief Returns rebind allocator max size
	template<class T, class Alloc>
	size_t allocator_max_size(const Alloc& al) noexcept
	{
		using rebind = detail::RebindAllocator<Alloc, T>;
		using traits = std::allocator_traits<rebind>;
		return traits::max_size(rebind{ al });
	}
	



	/// @brief Copy allocator for container copy constructor
	template<class Allocator>
	auto copy_allocator(const Allocator& alloc) noexcept(std::is_nothrow_copy_constructible_v<Allocator>) -> Allocator
	{
		return std::allocator_traits<Allocator>::select_on_container_copy_construction(alloc);
	}

	/// @brief Swap allocators for container.swap member
	template<class Allocator>
	void swap_allocator(Allocator& left,
			    Allocator& right) noexcept(!std::allocator_traits<Allocator>::propagate_on_container_swap::value || std::allocator_traits<Allocator>::is_always_equal::value)
	{
		if constexpr (std::allocator_traits<Allocator>::propagate_on_container_swap::value) {
			std::swap(left, right);
		}
		else {
			SEQ_ASSERT_DEBUG(left == right, "containers incompatible for swap");
		}
	}

	/// @brief Assign allocator for container copy operator
	template<class Allocator>
	void assign_allocator(Allocator& left,
			      const Allocator& right) noexcept(!std::allocator_traits<Allocator>::propagate_on_container_copy_assignment::value || std::is_nothrow_copy_assignable_v<Allocator>)
	{
		if constexpr (std::allocator_traits<Allocator>::propagate_on_container_copy_assignment::value) {
			left = right;
		}
	}

	/// @brief Move allocator for container move assignment
	template<class Allocator>
	void move_allocator(Allocator& left,
			    Allocator& right) noexcept(!std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value || std::is_nothrow_move_assignable_v<Allocator>)
	{
		// (maybe) propagate on container move assignment
		if constexpr (std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value) {
			left = std::move(right);
		}
	}

	// Returns whether an attempt to propagate allocators is necessary in copy assignment operations.
	// Note that even when false_type, callers should call assign_allocator as we want to assign allocators even when equal.
	template<class Allocator>
	struct assign_alloc
	{
		static constexpr bool value = std::allocator_traits<Allocator>::propagate_on_container_copy_assignment::value && !is_always_equal<Allocator>::value;
	};

	template<class Allocator>
	struct move_alloc
	{
		static constexpr bool value = std::allocator_traits<Allocator>::propagate_on_container_move_assignment::type && !is_always_equal<Allocator>::value;
	};




} // end namespace seq

#endif
