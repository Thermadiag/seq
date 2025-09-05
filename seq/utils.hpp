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

#ifndef SEQ_UTILS_HPP
#define SEQ_UTILS_HPP

/** @file */

#include <memory>

#include "bits.hpp"
#include "type_traits.hpp"

namespace seq
{

	/// @brief Constants used by #object_pool, #parallel_object_pool and #object_allocator
	enum
	{
		// EnoughForSharedPtr = INT_MAX, ///! MaxObjectsAllocation value for #object_pool and #parallel_object_pool to enable shared_ptr building
		DefaultAlignment = 0 ///! Default alignment value for #object_pool, #parallel_object_pool and #object_allocator
	};

	/// @brief Allocator class with custom alignment.
	/// @tparam T object type to allocate
	/// @tparam Allocator underlying allocator
	///
	/// aligned_allocator is a stl conforming allocator class supporting custom alignment.
	/// It relies on an underlying allocator class to perform the actual memory allocation.
	/// aligned_allocator will allocate more memory than required for over aligned types.
	///
	/// If \a Align is 0, the default system alignment is used.
	///
	/// If provided Allocator class is std::allocator<T>, aligned_allocator is specialized to
	/// use seq::aligned_malloc and seq::aligned_free.
	///
	template<class T, class Allocator = std::allocator<T>, size_t Align = DefaultAlignment>
	class aligned_allocator
	{

		template<class U>
		using rebind_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<U>;

		Allocator d_alloc;

	public:
		static_assert(Align == 0 || ((Align - 1) & Align) == 0, "wrong alignment value (must be a power of 2)");
		static constexpr size_t alignment = (Align == 0 || Align < SEQ_DEFAULT_ALIGNMENT) ? SEQ_DEFAULT_ALIGNMENT : Align;

		using value_type = T;
		using pointer = T*;
		using const_pointer = const T*;
		using reference = T&;
		using const_reference = const T&;
		using size_type = size_t;
		using difference_type = std::ptrdiff_t;
		using propagate_on_container_swap = typename std::allocator_traits<Allocator>::propagate_on_container_swap;
		using propagate_on_container_copy_assignment = typename std::allocator_traits<Allocator>::propagate_on_container_copy_assignment;
		using propagate_on_container_move_assignment = typename std::allocator_traits<Allocator>::propagate_on_container_move_assignment;
		using is_always_equal = typename std::allocator_traits<Allocator>::is_always_equal;

		template<class U>
		struct rebind
		{
			using other = aligned_allocator<U, Allocator, Align>;
		};

		auto select_on_container_copy_construction() const noexcept -> aligned_allocator { return *this; }

		aligned_allocator(const Allocator al)
		  : d_alloc(al)
		{
		}
		aligned_allocator(const aligned_allocator& other)
		  : d_alloc(other.d_alloc)
		{
		}
		aligned_allocator(aligned_allocator&& other) noexcept
		  : d_alloc(std::move(other.d_alloc))
		{
		}
		template<class U>
		aligned_allocator(const aligned_allocator<U, Allocator, Align>& other)
		  : d_alloc(other.get_allocator())
		{
		}
		~aligned_allocator() {}

		auto get_allocator() noexcept -> Allocator& { return d_alloc; }
		auto get_allocator() const noexcept -> Allocator { return d_alloc; }

		auto operator=(const aligned_allocator& other) -> aligned_allocator&
		{
			d_alloc = other.d_alloc;
			return *this;
		}
		auto operator=(aligned_allocator&& other) noexcept -> aligned_allocator&
		{
			d_alloc = std::move(other.d_alloc);
			return *this;
		}
		auto operator==(const aligned_allocator& other) const noexcept -> bool { return d_alloc == other.d_alloc; }
		auto operator!=(const aligned_allocator& other) const noexcept -> bool { return d_alloc != other.d_alloc; }
		auto address(reference x) const noexcept -> pointer { return static_cast<std::allocator<T>*>(this)->address(x); }
		auto address(const_reference x) const noexcept -> const_pointer { return static_cast<std::allocator<T>*>(this)->address(x); }
		auto allocate(size_t n, const void* /*unused*/) -> T* { return allocate(n); }
		auto allocate(size_t n) -> T*
		{
			if (alignment == SEQ_DEFAULT_ALIGNMENT) {
				rebind_alloc<T> al = get_allocator();
				return al.allocate(n);
			}

			rebind_alloc<std::uint8_t> al = get_allocator();
			void* ptr;
			size_t align = alignment - 1;
			size_t size = n * sizeof(T);

			// Room for padding and extra pointer stored in front of allocated area
			size_t overhead = align + sizeof(void*);

			// Avoid integer overflow
			if (size > (SIZE_MAX - overhead))
				return nullptr;

			std::uint8_t* mem = al.allocate(size + overhead);

			// Use the fact that align + 1U is a power of 2
			size_t offset = ((align ^ (reinterpret_cast<size_t>(mem + sizeof(void*)) & align)) + 1U) & align;
			ptr = (mem + sizeof(void*) + offset);
			(reinterpret_cast<void**>(ptr))[-1] = mem;

			return static_cast<T*>(ptr);
		}
		void deallocate(T* p, size_t n)
		{
			if (alignment == SEQ_DEFAULT_ALIGNMENT) {
				rebind_alloc<T> al = get_allocator();
				return al.deallocate(p, n);
			}
			if (p != nullptr) {
				rebind_alloc<std::uint8_t> al = get_allocator();
				al.deallocate(reinterpret_cast<std::uint8_t*>((reinterpret_cast<void**>(p))[-1]), n * sizeof(T) + alignment - 1ULL + sizeof(void*));
			}
		}
		template<class... Args>
		void construct(T* p, Args&&... args)
		{
			//((std::allocator<T>*)(this))->construct(p, std::forward<Args>(args)...);
			construct_ptr(p, std::forward<Args>(args)...);
		}
		void destroy(T* p) { destroy_ptr(p); }
	};

	template<class T, size_t Align>
	class aligned_allocator<T, std::allocator<T>, Align> : public std::allocator<T>
	{
		using Allocator = std::allocator<T>;
		template<class U>
		using rebind_alloc = typename std::allocator_traits<std::allocator<T>>::template rebind_alloc<U>;

	public:
		static_assert(Align == 0 || ((Align - 1) & Align) == 0, "wrong alignment value (must be a power of 2)");
		static constexpr size_t alignment = (Align == 0 || Align < SEQ_DEFAULT_ALIGNMENT) ? SEQ_DEFAULT_ALIGNMENT : Align;

		using value_type = T;
		using pointer = T*;
		using const_pointer = const T*;
		using reference = T&;
		using const_reference = const T&;
		using size_type = size_t;
		using difference_type = std::ptrdiff_t;
		using propagate_on_container_swap = typename std::allocator_traits<Allocator>::propagate_on_container_swap;
		using propagate_on_container_copy_assignment = typename std::allocator_traits<Allocator>::propagate_on_container_copy_assignment;
		using propagate_on_container_move_assignment = typename std::allocator_traits<Allocator>::propagate_on_container_move_assignment;
		template<class U>
		struct rebind
		{
			using other = aligned_allocator<U, std::allocator<U>, Align>;
		};

		auto select_on_container_copy_construction() const noexcept -> aligned_allocator { return *this; }

		aligned_allocator() {}
		aligned_allocator(const Allocator& al)
		  : std::allocator<T>(al)
		{
		}
		aligned_allocator(const aligned_allocator& al)
		  : std::allocator<T>(al)
		{
		}
		aligned_allocator(aligned_allocator&& al) noexcept
		  : std::allocator<T>(std::move(al))
		{
		}
		template<class U, class V>
		aligned_allocator(const aligned_allocator<U, std::allocator<V>, Align>& /*unused*/)
		{
		}

		auto get_allocator() noexcept -> Allocator& { return *this; }
		auto get_allocator() const noexcept -> Allocator { return *this; }

		auto operator=(const aligned_allocator& /*unused*/) -> aligned_allocator& { return *this; }
		auto operator==(const aligned_allocator& /*unused*/) const noexcept -> bool { return true; }
		auto operator!=(const aligned_allocator& /*unused*/) const noexcept -> bool { return false; }
		auto address(reference x) const noexcept -> pointer { return static_cast<std::allocator<T>*>(this)->address(x); }
		auto address(const_reference x) const noexcept -> const_pointer { return static_cast<std::allocator<T>*>(this)->address(x); }
		auto allocate(size_t n, const void* /*unused*/) -> T* { return allocate(n); }
		auto allocate(size_t n) -> T* { return static_cast<T*>(aligned_malloc(n * sizeof(T), alignment)); }
		void deallocate(T* p, size_t /*unused*/) { aligned_free(p); }
		template<class... Args>
		void construct(T* p, Args&&... args)
		{
			construct_ptr(p, std::forward<Args>(args)...);
		}
		void destroy(T* p) { destroy_ptr(static_cast<T*>(p)); }
	};

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

	/// @brief Iterator range class, provides a valid range object from a start and end iterator.
	template<class Iter>
	class iterator_range
	{
		Iter d_begin;
		Iter d_end;

	public:
		using iterator = Iter;
		using value_type = typename std::iterator_traits<Iter>::value_type;
		using reference = typename std::iterator_traits<Iter>::reference;
		using pointer = typename std::iterator_traits<Iter>::pointer;
		using difference_type = typename std::iterator_traits<Iter>::difference_type;
		using size_type = difference_type;

		iterator_range(const Iter& b = Iter(), const Iter& e = Iter())
		  : d_begin(b)
		  , d_end(e)
		{
		}
		iterator_range(const iterator_range&) = default;
		iterator_range(iterator_range&&) noexcept = default;
		iterator_range& operator=(const iterator_range&) = default;
		iterator_range& operator=(iterator_range&&) = default;

		Iter begin() const { return d_begin; }
		Iter end() const { return d_end; }
	};

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
	}

	/// @brief Copy allocator for container copy constructor
	template<class Allocator>
	auto copy_allocator(const Allocator& alloc) noexcept(std::is_nothrow_copy_constructible<Allocator>::value) -> Allocator
	{
		return std::allocator_traits<Allocator>::select_on_container_copy_construction(alloc);
	}

	/// @brief Swap allocators for container.swap member
	template<class Allocator>
	void swap_allocator(Allocator& left,
			    Allocator& right) noexcept(!std::allocator_traits<Allocator>::propagate_on_container_swap::value || std::allocator_traits<Allocator>::is_always_equal::value)
	{
		if SEQ_CONSTEXPR (std::allocator_traits<Allocator>::propagate_on_container_swap::value) {
			std::swap(left, right);
		}
		else {
			SEQ_ASSERT_DEBUG(left == right, "containers incompatible for swap");
		}
	}

	/// @brief Assign allocator for container copy operator
	template<class Allocator>
	void assign_allocator(Allocator& left,
			      const Allocator& right) noexcept(!std::allocator_traits<Allocator>::propagate_on_container_copy_assignment::value || std::is_nothrow_copy_assignable<Allocator>::value)
	{
		if SEQ_CONSTEXPR (std::allocator_traits<Allocator>::propagate_on_container_copy_assignment::value) {
			left = right;
		}
	}

	/// @brief Move allocator for container move assignment
	template<class Allocator>
	void move_allocator(Allocator& left,
			    Allocator& right) noexcept(!std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value || std::is_nothrow_move_assignable<Allocator>::value)
	{
		// (maybe) propagate on container move assignment
		if SEQ_CONSTEXPR (std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value) {
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
