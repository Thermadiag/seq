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

#ifndef SEQ_DEVECTOR_HPP
#define SEQ_DEVECTOR_HPP

/** @file */

#include "type_traits.hpp"
#include "bits.hpp"
#include "internal/utils.hpp"

#include <algorithm>

namespace seq
{
	namespace detail
	{
		template<class T, class Allocator>
		struct DEVectorData : private Allocator
		{
			// internal devector implementation

			static constexpr bool relocatable = is_relocatable<T>::value;

			T* data = nullptr;    // pointer to the memory storage
			size_t start_pos = 0; // position of the first value
			size_t end_pos = 0;   // past-the-end position
			size_t capacity = 0;  // memory storage capacity

			DEVectorData(const Allocator& al = Allocator())
			  : Allocator(al)
			{
			}
			DEVectorData(DEVectorData&& other) noexcept(std::is_nothrow_move_constructible_v<Allocator>)
			  : Allocator(std::move(static_cast<Allocator&>(other)))
			  , data(other.data)
			  , start_pos(other.start_pos)
			  , end_pos(other.end_pos)
			  , capacity(other.capacity)
			{
				other.data = nullptr;
				other.start_pos = other.end_pos = other.capacity = 0;
			}

			DEVectorData(const DEVectorData& other, const Allocator& al)
			  : Allocator(al)
			  , data(nullptr)
			  , start_pos(0)
			  , end_pos(0)
			  , capacity(0)
			{
				size_t size = capacity = static_cast<size_t>(other.end_pos - other.start_pos);
				if (size) {
					data = allocate(size);
					start_pos = 0;
					end_pos = start_pos + size;

					if constexpr (std::is_trivial_v<T>)
						memcpy(static_cast<void*>(data), other.start_ptr(), size * sizeof(T));
					else {
						size_t i = 0;
						try {
							for (; i != size; ++i)
								construct_ptr(data + i, other.start_ptr()[i]);
						}
						catch (...) {
							destroy_range(data, data + i);
							deallocate(data, size);
							throw;
						}
					}
				}
			}

			~DEVectorData() noexcept
			{
				destroy_range(start_ptr(), end_ptr());
				deallocate(data, capacity);
			}

			auto start_ptr() noexcept { return data + start_pos; }
			auto start_ptr() const noexcept { return data + start_pos; }
			auto end_ptr() noexcept { return data + end_pos; }
			auto end_ptr() const noexcept { return data + end_pos; }

			auto get_size() const noexcept { return static_cast<size_t>(end_pos - start_pos); }

			auto max_size_internal() const noexcept
			{
				using alloc_traits = std::allocator_traits<Allocator>;
				return std::min<size_t>(alloc_traits::max_size(get_allocator()), static_cast<size_t>(std::numeric_limits<std::ptrdiff_t>::max()));
			}

			auto get_allocator() noexcept -> Allocator& { return *this; }
			auto get_allocator() const noexcept -> const Allocator& { return *this; }
			auto allocate(size_t n) -> T* { return n ? get_allocator().allocate(n) : nullptr; }
			void deallocate(T* p, size_t n)
			{
				if (p)
					get_allocator().deallocate(p, n);
			}

			void destroy_range(T* begin, T* en)
			{
				// destroy values in the range [begin,end)
				if constexpr (!std::is_trivially_destructible_v<T>) {
					for (T* p = begin; p != en; ++p)
						destroy_ptr(p);
				}
			}

			template<class Helper>
			void construct_range(T* first, T* last, const Helper& h)
			{
				// construct values in the range [begin,end) with given arguments
				// in case of exception, destroy created values
				T* saved = first;
				try {
					while (first != last) {
						h.construct(first);
						++first;
					}
				}
				catch (...) {
					destroy_range(saved, first);
					throw;
				}
			}

			void construct_destroy_input(T* first, T* last, T* dst)
			{
				// Copy range [first, last) to non overlapping dst, and destroy input range.
				// In case of exception, input is not detroyed, and created values are destroyed
				//
				// strong guarantee when copying is selected, or moving is nonthrowing;
				// otherwise valid but unspecified / moved - from source state, matching std::vector - style limitations

				static constexpr bool noexcept_move = std::is_nothrow_move_constructible_v<T>;

				if constexpr (relocatable)
					memcpy(static_cast<void*>(dst), static_cast<void*>(first), static_cast<size_t>(last - first) * sizeof(T));
				else {
					T* saved = first;
					T* saved_dst = dst;
					try {
						while (first != last) {
							construct_ptr(dst, std::move_if_noexcept(*first));
							// if T is inothrow move constructible, destroy input while iterating to avoid another loop on inputs
							if (noexcept_move)
								first->~T();
							++first;
							++dst;
						}
					}
					catch (...) {
						// in case of exception, destroy previously constructed elements
						destroy_range(saved_dst, dst);
						throw;
					}
					// no exception thrown, destroy input
					if (!noexcept_move)
						destroy_range(saved, last);
				}
			}

			auto grow_capacity() const -> size_t
			{
				auto max = max_size_internal();
				if (capacity == max)
					throw std::length_error("devector capacity exceeded");

				size_t c = capacity * 2;
				if (capacity > max / 2)
					c = max;

				if (c == capacity)
					++c;
				if (c < 2)
					c = 2;
				return c;
			}

			void move_destroy_input(T* first, T* last, T* dst)
			{
				// Move range [first, last) to overlapping dst and destroy input
				// Basic exception guarantee only

				// static constexpr bool noexcept_move = std::is_nothrow_move_constructible<T>::value && std::is_nothrow_copy_constructible<T>::value;

				size_t size = static_cast<size_t>(last - first);
				if (dst + size < first || dst >= last)
					// no overlapp, use construct_destroy_input
					return construct_destroy_input(first, last, dst);

				if (dst == first || first == last)
					return; // no op

				if (dst < first) {
					T* saved = dst;

					try {
						// Construct first part
						T* en = first;
						while (dst < en && first != last) {
							construct_ptr(dst, std::move(*first));
							++dst;
							++first;
						}
						// move second part
						while (first != last) {
							*dst = std::move(*first);
							++dst;
							++first;
						}
						// destroy from dst to last
						destroy_range(dst, last);
					}
					catch (...) {
						// destroy previously created values (first part only, before first)
						while (saved != dst && saved != first) {
							destroy_ptr(saved++);
						}
						throw;
					}
				}
				else {
					// dst is in between first and last

					T* end_dst = dst + size;
					T* saved = end_dst;
					T* src_end = last;
					try {
						// construct first (right) part
						while (end_dst != last) {
							construct_ptr(end_dst - 1, std::move(*(src_end - 1)));
							--end_dst;
							--src_end;
						}
						// move remaining
						while (end_dst != dst) {
							*(end_dst - 1) = std::move(*(src_end - 1));
							--end_dst;
							--src_end;
						}
						destroy_range(first, dst);
					}
					catch (...) {
						// destroy previously created values
						while (saved != end_dst && saved != last) {
							destroy_ptr(--saved);
						}
						throw;
					}
				}
			}

			void clear() noexcept
			{
				destroy_range(start_ptr(), end_ptr());
				start_pos = end_pos = 0;
			}

			void shrink_to_fit()
			{
				// Strong exception guarantee
				size_t size = get_size();
				if (size == capacity)
					return;

				if (size == 0) {
					if (capacity)
						deallocate(data, capacity);
					data = nullptr;
					start_pos = end_pos = capacity = 0;
					return;
				}

				T* _new = allocate(size);
				try {
					construct_destroy_input(start_ptr(), end_ptr(), _new);
				}
				catch (...) {
					deallocate(_new, size);
					throw;
				}
				deallocate(data, capacity);

				data = _new;
				start_pos = 0;
				end_pos = capacity = size;
			}

			void reserve(size_t new_capacity)
			{
				// Strong exception guatantee
				if (new_capacity <= capacity)
					return;

				size_t size = get_size();
				T* _new = allocate(new_capacity);
				T* _new_start = _new + (start_pos); // keep previous left position
				T* _new_end = _new_start + size;

				try {

					// copy from old to new
					construct_destroy_input(start_ptr(), end_ptr(), _new_start);
				}
				catch (...) {
					deallocate(_new, new_capacity);
					throw;
				}

				deallocate(data, capacity);

				data = _new;
				start_pos = static_cast<size_t>(_new_start - _new);
				end_pos = static_cast<size_t>(_new_end - _new);
				capacity = new_capacity;
			}

			void reserve_back(size_t new_back_capacity)
			{
				// Basic exception guarantee
				size_t size = get_size();
				size_t back_capacity = static_cast<size_t>(capacity - end_pos);
				if (back_capacity >= new_back_capacity)
					return;

				size_t required_capacity = new_back_capacity + size;

				if (required_capacity > max_size_internal())
					throw std::length_error("devector::reserve");

				if (required_capacity <= capacity) {
					// move data
					T* _new_start = data + capacity - required_capacity;
					move_destroy_input(start_ptr(), end_ptr(), _new_start);
					start_pos = capacity - required_capacity;
					end_pos = start_pos + size;
				}
				else {
					T* _new = allocate(required_capacity);
					try {
						construct_destroy_input(start_ptr(), end_ptr(), _new);
					}
					catch (...) {
						deallocate(_new, required_capacity);
						throw;
					}
					deallocate(data, capacity);
					data = _new;
					start_pos = 0;
					end_pos = size;
					capacity = required_capacity;
				}
			}

			void reserve_front(size_t new_front_capacity)
			{
				// Basic exception guarantee
				size_t size = get_size();
				size_t front_capacity = start_pos;
				if (front_capacity >= new_front_capacity)
					return;

				size_t required_capacity = new_front_capacity + size;

				if (required_capacity > max_size_internal())
					throw std::length_error("devector::reserve");

				if (required_capacity <= capacity) {
					// move data
					T* _new_start = data + new_front_capacity;
					move_destroy_input(start_ptr(), end_ptr(), _new_start);
					start_pos = new_front_capacity;
					end_pos = start_pos + size;
				}
				else {
					T* _new = allocate(required_capacity);
					T* _new_start = _new + new_front_capacity;
					try {
						construct_destroy_input(start_ptr(), end_ptr(), _new_start);
					}
					catch (...) {
						deallocate(_new, required_capacity);
						throw;
					}
					deallocate(data, capacity);
					data = _new;
					start_pos = new_front_capacity;
					end_pos = new_front_capacity + size;
					capacity = required_capacity;
				}
			}

			template<class... U>
			void resize(size_t new_size, const U&... value)
			{
				if (new_size > max_size_internal())
					throw std::length_error("devector::resize");

				// Strong exception guarantee

				size_t size = get_size();
				if (size == new_size)
					return;

				if (new_size > size) {

					auto helper = detail::resize_helper<T>(std::forward<const U&>(value)...);

					// Grow
					size_t remaining = static_cast<size_t>(capacity - end_pos);
					if (remaining >= (new_size - size)) {
						// no need to allocate, just construct
						T* new_end = data + end_pos + (new_size - size);
						construct_range(end_ptr(), new_end, helper);
						end_pos += (new_size - size);
					}
					else {
						// reallocate, might throw, fine
						size_t _new_capacity = new_size;
						T* _new = allocate(_new_capacity);
						T* _new_start = (_new + (_new_capacity - new_size) / 2); // good balance: leave as much space at the left and the right
						T* _new_end = _new_start + new_size;

						try {
							// construct right elements
							construct_range(_new_start + size, _new_end, helper);
						}
						catch (...) {
							deallocate(_new, _new_capacity);
							throw;
						}
						try {
							// copy from old to new
							construct_destroy_input(start_ptr(), end_ptr(), _new_start);
						}
						catch (...) {
							destroy_range(_new_start + size, _new_end);
							deallocate(_new, _new_capacity);
							throw;
						}

						deallocate(data, capacity);

						data = _new;
						start_pos = static_cast<size_t>(_new_start - _new);
						end_pos = static_cast<size_t>(_new_end - _new);
						capacity = _new_capacity;
					}
				}
				else {
					// Shrink
					T* _new_end = data + start_pos + new_size;
					destroy_range(_new_end, end_ptr());
					end_pos = start_pos + new_size;
				}
			}

			template<class... U>
			void resize_front(size_t new_size, const U&... value)
			{
				if (new_size > max_size_internal())
					throw std::length_error("devector::resize");

				// Strong exception guarantee

				size_t size = get_size();
				if (size == new_size)
					return;

				if (new_size > size) {

					auto helper = detail::resize_helper<T>(std::forward<const U&>(value)...);

					// Grow
					size_t remaining = start_pos;
					if (remaining >= (new_size - size)) {
						// no need to allocate, just construct
						T* new_start = data + start_pos - (new_size - size);
						construct_range(new_start, start_ptr(), helper);
						start_pos -= (new_size - size);
					}
					else {
						// reallocate, might throw, fine
						size_t _new_capacity = new_size;
						T* _new = allocate(_new_capacity);
						T* _new_start = (_new + (_new_capacity - new_size) / 2); // good balance: leave as much space at the left and the right
						T* _new_end = _new_start + new_size;

						try {
							construct_range(_new, _new + (new_size - size), helper);
						}
						catch (...) {
							deallocate(_new, _new_capacity);
							throw;
						}
						try {
							construct_destroy_input(start_ptr(), end_ptr(), _new_start + (new_size - size));
						}
						catch (...) {
							destroy_range(_new, _new + (new_size - size));
							deallocate(_new, _new_capacity);
							throw;
						}

						deallocate(data, capacity);

						data = _new;
						start_pos = static_cast<size_t>(_new_start - _new);
						end_pos = static_cast<size_t>(_new_end - _new);
						capacity = _new_capacity;
					}
				}
				else {
					// Shrink
					T* _new_start = data + start_pos + (size - new_size);
					destroy_range(start_ptr(), _new_start);
					start_pos += (size - new_size);
				}
			}
			template<class... Args>
			void grow_back(Args&&... args)
			{
				// Strong exception guarantee if move constructor and move assignation operator are noexcept
				// Otherwise basic exception guarantee

				// grow_back is only called when there is no more room on the back
				SEQ_ASSERT_DEBUG(end_pos == capacity, "");

				size_t size = get_size();

				if (start_pos > size) {
					// Front capacity is greater than current size:
					// move data to front
					T tmp(std::forward<Args>(args)...); // copy to solve potential aliasing issue
					construct_destroy_input(start_ptr(), end_ptr(), data);
					start_pos = 0;
					end_pos = size;
					construct_ptr(end_ptr(), std::move(tmp));
					return;
				}

				// reallocate
				size_t new_capacity = grow_capacity();
				T* _new = allocate(new_capacity);
				T* _new_start = _new + (start_pos); // keep previous left position
				if (_new_start + size == _new + new_capacity)
					--_new_start;
				T* _new_end = _new_start + size;

				try {
					try {
						new (_new_end) T(std::forward<Args>(args)...);
					}
					catch (...) {
						_new_end = nullptr;
						throw;
					}
					// copy from old to new
					construct_destroy_input(start_ptr(), end_ptr(), _new_start);
				}
				catch (...) {
					if (_new_end)
						destroy_ptr(_new_end);
					deallocate(_new, new_capacity);
					throw;
				}

				deallocate(data, capacity);

				data = _new;
				start_pos = static_cast<size_t>(_new_start - _new);
				end_pos = static_cast<size_t>(_new_end - _new);
				capacity = new_capacity;
			}
			template<class... Args>
			void grow_front(Args&&... args)
			{
				// Strong exception guarantee if move constructor and move assignation operator are noexcept
				// Otherwise basic exception guarantee

				// grow_front is only called when there is no more room on the back
				SEQ_ASSERT_DEBUG(start_pos == 0, "");

				size_t size = get_size();

				if (static_cast<size_t>((capacity - size)) > size) {
					// Back capacity is greater than current size:
					// move data to back
					T tmp(std::forward<Args>(args)...); // copy to solve potential aliasing issue
					construct_destroy_input(start_ptr(), end_ptr(), data + (capacity - size));
					start_pos = (capacity - size);
					end_pos = capacity;
					construct_ptr(start_ptr() - 1, std::move(tmp));
					return;
				}

				// reallocate
				size_t new_capacity = grow_capacity();
				T* _new = allocate(new_capacity);
				T* _new_start = _new + (new_capacity - (capacity - start_pos)); // keep previous right position
				if (_new_start == _new)
					++_new_start;
				T* _new_end = _new_start + size;

				try {
					try {
						new (_new_start - 1) T(std::forward<Args>(args)...);
					}
					catch (...) {
						_new_start = nullptr;
						throw;
					}
					// copy from old to new
					construct_destroy_input(start_ptr(), end_ptr(), _new_start);
				}
				catch (...) {
					if (_new_start)
						destroy_ptr(_new_start - 1);
					deallocate(_new, new_capacity);
					throw;
				}

				deallocate(data, capacity);

				data = _new;
				start_pos = static_cast<size_t>(_new_start - _new);
				end_pos = static_cast<size_t>(_new_end - _new);
				capacity = new_capacity;
			}
		};
	}

	/// @brief Double-ending vector implementation which can be optimized for several use case.
	/// @tparam T value type
	/// @tparam Allocator allocator type
	///
	/// seq::devector is a double-ending vector class that mixes the behavior and performances of std::deque and std::vector.
	/// Elements are stored in a contiguous memory chunk exatcly like a vector, but might contain free space at the front in addition to free
	/// space at the back in order to provide O(1) insertion at the front.
	///
	/// seq::devector provides a similar interface as std::vector with the following additional members:
	///		-	push_front() and emplace_front(): insert an element at the front of the devector
	///		-	resize_front(): resize the devector from the front instead of the back of the container
	///		-	back_capacity(): returns the capacity (free slots) at the back of the devector
	/// 	-	front_capacity(): returns the capacity (free slots) at the front of the devector
	///
	/// Almost all members provide basic exception guarantee, except if the value type has a noexcept move constructor and move assignment operator,
	/// in which case members provide strong exception guarantee.
	///
	/// References and iterators are invalidated by insertion/removal of elements.
	///
	/// seq::devector is used by seq::tiered_vector for bucket storage.
	///
	///
	template<class T, class Allocator = std::allocator<T>>
	class devector : private detail::DEVectorData<T, Allocator>
	{
		using base_type = detail::DEVectorData<T, Allocator>;

		// We do NOT support fancy pointer
		static_assert(std::is_same_v<typename std::allocator_traits<Allocator>::pointer, T*>);

		void steal_storage(devector& other) noexcept
		{
			auto& this_base = static_cast<base_type&>(*this);
			auto& other_base = static_cast<base_type&>(other);

			this_base.data = other_base.data;
			this_base.start_pos = other_base.start_pos;
			this_base.end_pos = other_base.end_pos;
			this_base.capacity = other_base.capacity;
			other_base.data = nullptr;
			other_base.end_pos = other_base.start_pos = other_base.capacity = 0;
		}
		void clear_and_deallocate() noexcept
		{
			clear();
			shrink_to_fit();
		}

	public:
		using value_type = T;
		using allocator_type = Allocator;
		using size_type = size_t;
		using difference_type = std::ptrdiff_t;
		using reference = T&;
		using const_reference = const T&;
		using pointer = T*;
		using const_pointer = const T*;
		using iterator = T*;
		using const_iterator = const T*;
		using reverse_iterator = std::reverse_iterator<iterator>;
		using const_reverse_iterator = std::reverse_iterator<const_iterator>;

		/// @brief Default constructor
		devector()
		  : base_type()
		{
		}
		/// @brief Constructs an empty container with the given allocator alloc.
		/// @param alloc allocator to use for all memory allocations of this container
		explicit devector(const Allocator& alloc)
		  : base_type(alloc)
		{
		}
		/// @brief Constructs the container with count copies of elements with value value.
		/// @param count the size of the container
		/// @param value the value to initialize elements of the container with
		/// @param alloc allocator to use for all memory allocations of this container
		devector(size_type count, const T& value, const Allocator& alloc)
		  : base_type(alloc)
		{
			assign(count, value);
		}
		/// @brief Constructs the container with count element default constructed.
		/// @param count the size of the container
		/// @param alloc allocator to use for all memory allocations of this container
		explicit devector(size_type count, const Allocator& alloc = Allocator())
		  : base_type(alloc)
		{
			resize(count);
		}
		/// @brief Constructs the container with the contents of the range [first, last).
		/// @param first the range to copy the elements from
		/// @param last the range to copy the elements from
		/// @param alloc allocator to use for all memory allocations of this container
		template<class InputIt, std::enable_if_t<is_iterator<InputIt>::value, int> = 0>
		devector(InputIt first, InputIt last, const Allocator& alloc = Allocator())
		  : base_type(alloc)
		{
			assign(first, last);
		}
		/// @brief Copy constructor
		/// @param other another container to be used as source to initialize the elements of the container with
		devector(const devector& other)
		  : base_type(other, copy_allocator(other.get_allocator()))
		{
		}
		/// @brief Copy constructor
		/// @param other another container to be used as source to initialize the elements of the container with
		/// @param alloc allocator to use for all memory allocations of this container
		devector(const devector& other, const Allocator& alloc)
		  : base_type(other, alloc)
		{
		}
		/// @brief Move constructor
		/// @param other another container to be used as source to initialize the elements of the container with
		devector(devector&& other) noexcept(std::is_nothrow_move_constructible_v<base_type>)
		  : base_type(std::move(static_cast<base_type&>(other)))
		{
		}
		/// @brief Move constructor
		/// @param other another container to be used as source to initialize the elements of the container with
		/// @param alloc allocator to use for all memory allocations of this container
		devector(devector&& other, const Allocator& alloc)
		  : base_type(alloc)
		{
			if (alloc == other.get_allocator()) {
				swap(other);
			}
			else if (other.size()) {
				// Any exception would leave both devector in a valid but unspecified state
				this->base_type::data = get_allocator().allocate(other.size());
				this->base_type::capacity = other.size();

				auto ptr = this->base_type::data;
				for (; this->end_pos != this->base_type::capacity; ++this->end_pos) {
					construct_ptr(ptr + this->end_pos, std::move(other[this->end_pos]));
				}
				other.clear();
			}
		}
		/// @brief Constructs the container with the contents of the initializer list init
		/// @param init initializer list to initialize the elements of the container with
		/// @param alloc allocator to use for all memory allocations of this container
		devector(std::initializer_list<T> init, const Allocator& alloc = Allocator())
		  : base_type(alloc)
		{
			assign(init.begin(), init.end());
		}

		~devector() noexcept = default;

		/// @brief Returns the container size
		SEQ_ALWAYS_INLINE auto size() const noexcept -> size_t { return this->base_type::get_size(); }
		/// @brief Returns the container full capacity (back_capacity() + size() + front_capacity())
		SEQ_ALWAYS_INLINE auto capacity() const noexcept -> size_t { return this->base_type::capacity; }
		/// @brief Returns the container back capacity
		SEQ_ALWAYS_INLINE auto back_capacity() const noexcept -> size_t { return capacity() - this->end_pos; }
		/// @brief Returns the container front capacity
		SEQ_ALWAYS_INLINE auto front_capacity() const noexcept -> size_t { return this->start_pos; }
		/// @brief Returns the container maximum size
		SEQ_ALWAYS_INLINE auto max_size() const noexcept -> size_t { return this->max_size_internal(); }
		/// @brief Returns true if the container is empty, false otherwise
		SEQ_ALWAYS_INLINE auto empty() const noexcept -> bool { return this->base_type::end_pos == this->start_pos; }

		/// @brief Returns the container allocator object
		SEQ_ALWAYS_INLINE auto get_allocator() const -> Allocator { return this->base_type::get_allocator(); }

		/// @brief Clear the container, but does not deallocate the storage
		void clear() noexcept { this->base_type::clear(); }
		/// @brief Requests the removal of unused capacity.
		/// Strong exception guarentee.
		void shrink_to_fit() { this->base_type::shrink_to_fit(); }

		/// @brief Insert an element at the back of the container.
		/// The complexity is amortized O(1) for OptimizeForPushBack and OptimizeForBothEnds, O(N) for OptimizeForPushFront.
		/// Strong exception guarantee if move constructor and move assignment operator are noexcept. Otherwise basic exception guarantee.
		/// Invalidate all references and iterators if back_capacity() == 0.
		/// @param value value to insert
		SEQ_ALWAYS_INLINE void push_back(const T& value) { emplace_back(value); }
		/// @brief Insert an element at the back of the container using move semantic.
		/// The complexity is amortized O(1) for OptimizeForPushBack and OptimizeForBothEnds, O(N) for OptimizeForPushFront.
		/// Strong exception guarantee if move constructor and move assignment operator are noexcept. Otherwise basic exception guarantee.
		/// Invalidate all references and iterators if back_capacity() == 0.
		/// @param value value to insert
		SEQ_ALWAYS_INLINE void push_back(T&& value) { emplace_back(std::move(value)); }
		/// @brief Appends a new element to the end of the container.
		/// The element is constructed through std::allocator_traits::construct, which typically uses placement-new to construct the element in-place at the location provided by the container.
		/// The arguments args... are forwarded to the constructor as std::forward<Args>(args)....
		/// The complexity is amortized O(1) for OptimizeForPushBack and OptimizeForBothEnds, O(N) for OptimizeForPushFront.
		/// Strong exception guarantee if move constructor and move assignment operator are noexcept. Otherwise basic exception guarantee.
		/// Invalidate all references and iterators if back_capacity() == 0.
		template<class... Args>
		SEQ_ALWAYS_INLINE auto emplace_back(Args&&... args) -> reference
		{
			if SEQ_UNLIKELY (this->end_pos == capacity())
				this->grow_back(std::forward<Args>(args)...);
			else
				construct_ptr(this->end_ptr(), std::forward<Args>(args)...);
			++this->end_pos;
			return *(this->end_ptr() - 1);
		}

		/// @brief Insert an element at the front of the container.
		/// The complexity is amortized O(1) .
		/// Strong exception guarantee if move constructor and move assignment operator are noexcept. Otherwise basic exception guarantee.
		/// Invalidate all references and iterators if front_capacity() == 0.
		/// @param value value to insert
		SEQ_ALWAYS_INLINE void push_front(const T& value) { emplace_front(value); }
		/// @brief Insert an element at the front of the container using move semantic.
		/// The complexity is amortized O(1) .
		/// Strong exception guarantee if move constructor and move assignment operator are noexcept. Otherwise basic exception guarantee.
		/// Invalidate all references and iterators if front_capacity() == 0.
		/// @param value value to insert
		SEQ_ALWAYS_INLINE void push_front(T&& value) { emplace_front(std::move(value)); }
		/// @brief Appends a new element to the front of the container.
		/// The element is constructed through std::allocator_traits::construct, which typically uses placement-new to construct the element in-place at the location provided by the container.
		/// The arguments args... are forwarded to the constructor as std::forward<Args>(args)....
		/// The complexity is amortized O(1).
		/// Strong exception guarantee if move constructor and move assignment operator are noexcept. Otherwise basic exception guarantee.
		/// Invalidate all references and iterators if front_capacity() == 0.
		template<class... Args>
		SEQ_ALWAYS_INLINE auto emplace_front(Args&&... args) -> reference
		{
			if SEQ_UNLIKELY (this->start_pos == 0)
				this->grow_front(std::forward<Args>(args)...);
			else
				construct_ptr(this->start_ptr() - 1, std::forward<Args>(args)...);
			--this->start_pos;
			return *data();
		}

		/// @brief Inserts a new element into the container directly before pos.
		/// The element is constructed through std::allocator_traits::construct, which typically uses placement-new to construct the element in-place at a location provided by the container.
		/// However, if the required location has been occupied by an existing element, the inserted element is constructed at another location at first, and then move assigned into the
		/// required location. The arguments args... are forwarded to the constructor as std::forward<Args>(args).... args... may directly or indirectly refer to a value in the container.
		/// Invalidate all references and iterators.
		/// Strong exception guarantee if move constructor and move assignment operator are noexcept. Otherwise basic exception guarantee.
		///
		/// @return iterator to the inserted element
		template<class... Args>
		auto emplace(const_iterator pos, Args&&... args) -> iterator
		{
			size_t dist = static_cast<size_t>(pos - begin());
			SEQ_ASSERT_DEBUG(dist <= size(), "devector: invalid insertion location");

			if (pos == cbegin()) {
				emplace_front(std::forward<Args>(args)...);
				return begin();
			}

			if (pos == cend()) {
				emplace_back(std::forward<Args>(args)...);
				return end() - 1;
			}

			T tmp(std::forward<Args>(args)...);

			if (dist < size() / 2) {
				emplace_front(std::move(front()));
				std::move(begin() + 2, begin() + dist + 1, begin() + 1);
				begin()[dist] = std::move(tmp);
			}
			else {
				emplace_back(std::move(back()));
				std::move_backward(begin() + dist, end() - 2, end() - 1);
				begin()[dist] = std::move(tmp);
			}
			return begin() + dist;
		}
		/// @brief Inserts a new element into the container directly before pos.
		/// Invalidate all references and iterators.
		/// Strong exception guarantee if move constructor and move assignment operator are noexcept. Otherwise basic exception guarantee.
		///
		/// @return iterator to the inserted element
		auto insert(const_iterator pos, const T& value) -> iterator { return emplace(pos, value); }
		/// @brief Inserts a new element into the container directly before pos using move semantic.
		/// Invalidate all references and iterators.
		/// Strong exception guarantee if move constructor and move assignment operator are noexcept. Otherwise basic exception guarantee.
		///
		/// @return iterator to the inserted element
		auto insert(const_iterator pos, T&& value) -> iterator { return emplace(pos, std::move(value)); }
		/// @brief Inserts elements from range [first, last) before pos.
		/// Invalidate all references and iterators.
		/// Strong exception guarantee if move constructor and move assignment operator are noexcept. Otherwise basic exception guarantee.
		/// @param pos 	iterator before which the content will be inserted. pos may be the end() iterator
		/// @param first the range of elements to insert, can't be iterators into container for which insert is called
		/// @param last the range of elements to insert, can't be iterators into container for which insert is called
		/// @return iterator pointing to the first element inserted, or pos if first==last.
		template<class InputIt, std::enable_if_t<is_iterator<InputIt>::value, int> = 0>
		auto insert(const_iterator pos, InputIt first, InputIt last) -> iterator
		{
			size_type off = static_cast<size_t>(pos - begin());
			SEQ_ASSERT_DEBUG(off <= size(), "devector insert iterator outside range");
			size_type oldsize = size();

			std::ptrdiff_t n = 0;
			if constexpr (is_random_access_v<InputIt>) {
				n = std::distance(first, last);
				if (n < 0)
					throw std::length_error("invalid iterator range");
			}

			if (first == last)
				;
			else if (off <= size() / 2) { // closer to front, push to front then rotate
				try {
					if constexpr (is_random_access_v<InputIt>)
						reserve_front(static_cast<size_t>(n));

					for (; first != last; ++first)
						push_front(*first); // prepend flipped
				}
				catch (...) {
					for (; oldsize < size();)
						pop_front(); // restore old size, at least
					throw;
				}

				difference_type num = static_cast<difference_type>(size() - oldsize);
				std::reverse(begin(), begin() + num); // flip new stuff in place
				std::rotate(begin(), begin() + num, begin() + num + static_cast<difference_type>(off));
			}
			else { // closer to back
				try {
					if constexpr (is_random_access_v<InputIt>)
						reserve_back(static_cast<size_t>(n));

					for (; first != last; ++first)
						push_back(*first); // append
				}
				catch (...) {
					for (; oldsize < size();)
						pop_back(); // restore old size, at least
					throw;
				}

				std::rotate(begin() + off, begin() + oldsize, end());
			}
			return (begin() + off);
		}
		/// @brief inserts count copies of the value before pos
		/// Invalidate all references and iterators.
		/// Strong exception guarantee if move constructor and move assignment operator are noexcept. Otherwise basic exception guarantee.
		/// @param pos iterator before which the content will be inserted. pos may be the end() iterator
		/// @param count number of elements to insert
		/// @param value element value to insert
		/// @return iterator pointing to the first element inserted, or pos if first==last
		auto insert(const_iterator pos, size_type count, const T& value) -> iterator { return insert(pos, cvalue_iterator<T>(0, value), cvalue_iterator<T>(count)); }
		/// @brief  inserts elements from initializer list ilist before pos
		/// Invalidate all references and iterators.
		/// Strong exception guarantee if move constructor and move assignment operator are noexcept. Otherwise basic exception guarantee.
		/// @param pos iterator before which the content will be inserted. pos may be the end() iterator
		/// @param ilist 	initializer list to insert the values from
		/// @return iterator pointing to the first element inserted, or pos if first==last
		auto insert(const_iterator pos, std::initializer_list<T> ilist) -> iterator { return insert(pos, ilist.begin(), ilist.end()); }

		/// @brief Assign elements from range [first, last) to the container.
		/// Invalidate all references and iterators.
		/// Basic exception guarantee.
		template<class InputIt, std::enable_if_t<is_iterator<InputIt>::value, int> = 0>
		void assign(InputIt first, InputIt last)
		{
			if constexpr (is_random_access_v<InputIt>) {
				auto n = std::distance(first, last);
				if (n < 0)
					throw std::length_error("invalid iterator range");

				resize(static_cast<size_t>(n));
				std::copy(first, last, begin());
			}
			else {
				clear();
				for (; first != last; ++first)
					push_back(*first);
			}
		}
		/// @brief Replaces the contents with count copies of value value
		/// Invalidate all references and iterators.
		/// Basic exception guarantee.
		void assign(size_type count, const T& value) { assign(cvalue_iterator<T>(0, value), cvalue_iterator<T>(count)); }
		/// @brief Replaces the contents with the elements from the initializer list ilist
		/// Invalidate all references and iterators.
		/// Basic exception guarantee.
		void assign(std::initializer_list<T> ilist) { assign(ilist.begin(), ilist.end()); }

		/// @brief Removes the last element of the container
		/// Iterators and references to the last element, as well as the end() iterator, are invalidated.
		SEQ_ALWAYS_INLINE void pop_back() noexcept
		{
			SEQ_ASSERT_DEBUG(size() > 0, "pop_back() on empty devector");
			destroy_ptr(this->base_type::data + --this->base_type::end_pos);
		}
		/// @brief Removes the first element of the container
		/// Iterators and references to the first element are invalidated.
		SEQ_ALWAYS_INLINE void pop_front() noexcept
		{
			SEQ_ASSERT_DEBUG(size() > 0, "pop_front() on empty devector");
			destroy_ptr(this->base_type::data + this->base_type::start_pos++);
		}

		/// @brief Removes the elements in the range [first, last).
		/// Basic exception guarantee
		/// @param first range of elements to remove
		/// @param last range of elements to remove
		/// @return Iterator following the last removed element
		auto erase(const_iterator first, const_iterator last) noexcept(std::is_nothrow_move_assignable_v<T> || is_relocatable_v<T>) -> const_iterator
		{
			SEQ_ASSERT_DEBUG(last >= first && first >= begin() && last <= end(), "devector erase iterator outside range");
			if (first == last)
				return last;

			size_type off = static_cast<size_t>(first - begin());
			size_type count = static_cast<size_t>(last - first);

			if (off < static_cast<size_type>(end() - last)) { // closer to front

				if constexpr (is_relocatable_v<T>) {
					this->destroy_range(const_cast<T*>(first), const_cast<T*>(last));
					memmove(static_cast<void*>(begin() + count), static_cast<void*>(begin()), off * sizeof(T));
					this->base_type::start_pos += count;
				}
				else {
					std::move_backward(begin(), const_cast<iterator>(first), const_cast<iterator>(last)); // copy over hole
					for (; 0 < count; --count)
						pop_front(); // pop copied elements
				}
			}
			else { // closer to back
				if constexpr (is_relocatable_v<T>) {
					this->destroy_range(const_cast<T*>(first), const_cast<T*>(last));
					memmove(static_cast<void*>(const_cast<T*>(first)), last, (end() - last) * sizeof(T));
					this->base_type::end_pos -= count;
				}
				else {
					std::move(const_cast<iterator>(last), end(), const_cast<iterator>(first)); // copy over hole
					for (; 0 < count; --count)
						pop_back(); // pop copied elements
				}
			}

			return cbegin() + off;
		}
		/// @brief Removes the element at pos
		/// Basic exception guarantee
		/// @param pos iterator to the position to erase
		/// @return Iterator following the last removed element.
		auto erase(const_iterator pos) noexcept(std::is_nothrow_move_assignable_v<T> || is_relocatable_v<T>) -> const_iterator { return erase(pos, pos + 1); }

		/// @brief Swap this container with other
		/// Does not invalidated iterators, including end() iterator.
		void swap(devector& other) noexcept(noexcept(swap_allocator(std::declval<Allocator&>(), std::declval<Allocator&>())))
		{
			if (this != std::addressof(other)) {
				swap_allocator(base_type::get_allocator(), other.base_type::get_allocator());
				std::swap(base_type::data, other.base_type::data);
				std::swap(base_type::start_pos, other.base_type::start_pos);
				std::swap(base_type::end_pos, other.base_type::end_pos);
				std::swap(base_type::capacity, other.base_type::capacity);
			}
		}

		/// @brief Increase the capacity of the devector (the total number of elements that the devector can hold without requiring reallocation) to a value that's greater or equal to new_cap.
		/// If new_cap is greater than the current capacity(), new storage is allocated, otherwise the function does nothing.
		/// Invalidate iterators and references if a new storage is allocated.
		/// Strong exception guarantee.
		void reserve(size_t new_cap) { this->base_type::reserve(new_cap); }

		/// @brief Ensure that the devector has at least new_back_capacity free slots at the back.
		/// Invalidate iterators and references if a new storage is allocated.
		/// Strong exception guarantee if move constructor and move assignment operator are noexcept. Otherwise basic exception guarantee.
		/// @param new_back_capacity minimum back capacity
		void reserve_back(size_t new_back_capacity) { this->base_type::reserve_back(new_back_capacity); }
		/// @brief Ensure that the devector has at least new_front_capacity free slots at the front.
		/// Invalidate iterators and references if a new storage is allocated.
		/// Strong exception guarantee if move constructor and move assignment operator are noexcept. Otherwise basic exception guarantee.
		/// @param new_front_capacity minimum front capacity
		void reserve_front(size_t new_front_capacity) { this->base_type::reserve_front(new_front_capacity); }

		/// @brief Resizes the container to contain count elements.
		/// Invalidate iterators and references if a new storage is allocated.
		/// Strong exception guarantee.
		/// @param count new container size
		void resize(size_t count) { this->base_type::resize(count); }
		/// @brief Resizes the container to contain count elements.
		/// Invalidate iterators and references if a new storage is allocated.
		/// Strong exception guarantee.
		/// @param count new container size
		/// @param value if count is greater than size(), copies of value are appended to the back of the devector
		void resize(size_t count, const T& value) { this->base_type::resize(count, value); }

		/// @brief Resizes the container to contain count elements.
		/// The container is extended by the front.
		/// Invalidate iterators and references if a new storage is allocated.
		/// Strong exception guarantee.
		/// @param count new container size
		void resize_front(size_t new_size) { this->base_type::resize_front(new_size); }
		/// @brief Resizes the container to contain count elements.
		/// The container is extended by the front.
		/// Invalidate iterators and references if a new storage is allocated.
		/// Strong exception guarantee.
		/// @param count new container size
		/// @param value if count is greater than size(), copies of value are prepended to the back of the devector
		void resize_front(size_t new_size, const T& value) { this->base_type::resize_front(new_size, value); }

		/// @brief Returns pointer to the underlying array serving as element storage. The pointer is such that range [data(); data() + size()) is always a valid range,
		/// even if the container is empty (data() is not dereferenceable in that case).
		SEQ_ALWAYS_INLINE auto data() noexcept -> T* { return this->start_ptr(); }
		/// @brief Returns pointer to the underlying array serving as element storage. The pointer is such that range [data(); data() + size()) is always a valid range,
		/// even if the container is empty (data() is not dereferenceable in that case).
		SEQ_ALWAYS_INLINE auto data() const noexcept -> const T* { return this->start_ptr(); }

		/// @brief Returns a reference to the back element
		SEQ_ALWAYS_INLINE auto back() noexcept -> T&
		{
			SEQ_ASSERT_DEBUG(size() > 0, "empty container");
			return *(this->end_ptr() - 1);
		}
		/// @brief Returns a reference to the back element
		SEQ_ALWAYS_INLINE auto back() const noexcept -> const T&
		{
			SEQ_ASSERT_DEBUG(size() > 0, "empty container");
			return *(this->end_ptr() - 1);
		}

		/// @brief Returns a reference to the front element
		SEQ_ALWAYS_INLINE auto front() noexcept -> T&
		{
			SEQ_ASSERT_DEBUG(size() > 0, "empty container");
			return *data();
		}
		/// @brief Returns a reference to the front element
		SEQ_ALWAYS_INLINE auto front() const noexcept -> const T&
		{
			SEQ_ASSERT_DEBUG(size() > 0, "empty container");
			return *data();
		}

		/// @brief Returns a reference to the element at pos
		SEQ_ALWAYS_INLINE auto operator[](size_t pos) const noexcept -> const T&
		{
			SEQ_ASSERT_DEBUG(pos < size(), "invalid position");
			return data()[pos];
		}
		/// @brief Returns a reference to the element at pos
		SEQ_ALWAYS_INLINE auto operator[](size_t pos) noexcept -> T&
		{
			SEQ_ASSERT_DEBUG(pos < size(), "invalid position");
			return data()[pos];
		}

		/// @brief Returns a reference to the element at pos.
		/// Throw std::out_of_range if pos is invalid.
		SEQ_ALWAYS_INLINE auto at(size_t pos) const -> const T&
		{
			if (pos >= size())
				throw std::out_of_range("devector out of range");
			return data()[pos];
		}
		/// @brief Returns a reference to the element at pos.
		/// Throw std::out_of_range if pos is invalid.
		SEQ_ALWAYS_INLINE auto at(size_t pos) -> T&
		{
			if (pos >= size())
				throw std::out_of_range("devector out of range");
			return data()[pos];
		}

		/// @brief Returns an iterator to the first element of the devector.
		SEQ_ALWAYS_INLINE auto begin() const noexcept -> const_iterator { return this->start_ptr(); }
		/// @brief Returns an iterator to the first element of the devector.
		SEQ_ALWAYS_INLINE auto begin() noexcept -> iterator { return this->start_ptr(); }
		/// @brief Returns an iterator to the element following the last element of the devector.
		SEQ_ALWAYS_INLINE auto end() const noexcept -> const_iterator { return this->end_ptr(); }
		/// @brief Returns an iterator to the element following the last element of the devector.
		SEQ_ALWAYS_INLINE auto end() noexcept -> iterator { return this->end_ptr(); }
		/// @brief Returns a reverse iterator to the first element of the reversed devector.
		SEQ_ALWAYS_INLINE auto rbegin() noexcept -> reverse_iterator { return reverse_iterator(end()); }
		/// @brief Returns a reverse iterator to the first element of the reversed devector.
		SEQ_ALWAYS_INLINE auto rbegin() const noexcept -> const_reverse_iterator { return const_reverse_iterator(end()); }
		/// @brief Returns a reverse iterator to the element following the last element of the reversed devector.
		SEQ_ALWAYS_INLINE auto rend() noexcept -> reverse_iterator { return reverse_iterator(begin()); }
		/// @brief Returns a reverse iterator to the element following the last element of the reversed devector.
		SEQ_ALWAYS_INLINE auto rend() const noexcept -> const_reverse_iterator { return const_reverse_iterator(begin()); }
		/// @brief Returns an iterator to the first element of the devector.
		SEQ_ALWAYS_INLINE auto cbegin() const noexcept -> const_iterator { return begin(); }
		/// @brief Returns an iterator to the element following the last element of the devector.
		SEQ_ALWAYS_INLINE auto cend() const noexcept -> const_iterator { return end(); }
		/// @brief Returns a reverse iterator to the first element of the reversed devector.
		SEQ_ALWAYS_INLINE auto crbegin() const noexcept -> const_reverse_iterator { return rbegin(); }
		/// @brief Returns a reverse iterator to the element following the last element of the reversed devector.
		SEQ_ALWAYS_INLINE auto crend() const noexcept -> const_reverse_iterator { return rend(); }

		/// @brief Copy operator
		template<class Alloc>
		auto operator=(const devector<T, Alloc>& other) -> devector&
		{
			resize(other.size());
			std::copy(other.begin(), other.end(), begin());
			return *this;
		}
		/// @brief Copy operator
		auto operator=(const devector& other) -> devector&
		{
			if (this != std::addressof(other)) {
				if constexpr (assign_alloc<Allocator>::value) {
					if (get_allocator() != other.get_allocator()) {
						// clear and deallocate
						clear();
						shrink_to_fit();
					}
				}
				assign_allocator(base_type::get_allocator(), other.get_allocator());
				resize(other.size());
				std::copy(other.begin(), other.end(), begin());
			}
			return *this;
		}

		/// @brief Move assignment operator
		auto operator=(devector&& other) noexcept(std::is_nothrow_move_assignable_v<T> && std::is_nothrow_move_assignable_v<Allocator>) -> devector&
		{
			if (this == std::addressof(other))
				return *this;

			using traits = std::allocator_traits<Allocator>;

			if constexpr (traits::propagate_on_container_move_assignment::value) {
				clear_and_deallocate();
				base_type::get_allocator() = std::move(other.base_type::get_allocator());
				steal_storage(other);
			}
			else if (base_type::get_allocator() == other.base_type::get_allocator()) {
				clear_and_deallocate();
				steal_storage(other);
			}
			else {
				assign(std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
				other.clear();
			}

			return *this;
		}
	};

	/// @brief  Specialization of is_relocatable for devector
	template<class T, class Alloc>
	struct is_relocatable<devector<T, Alloc>> : std::bool_constant<is_relocatable_v<Alloc>>
	{
	};

}
#endif
