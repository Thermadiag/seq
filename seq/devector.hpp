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

#ifndef SEQ_DEVECTOR_HPP
#define SEQ_DEVECTOR_HPP



/** @file */

#include "type_traits.hpp"
#include "bits.hpp"
#include "utils.hpp"

#include <algorithm>

// Value used to define the limit between moving elements and reallocating new elements for push_back/front
#define SEQ_DEVECTOR_SIZE_LIMIT 16U

namespace seq
{
	/// @brief Flag indicating whether the devector is optimized for back insertion, front insertion or both.
	enum DEVectorFlag
	{
		OptimizeForPushBack,
		OptimizeForPushFront,
		OptimizeForBothEnds
	};
	

	namespace detail
	{
		template<class T, class Allocator, DEVectorFlag flag>
		struct DEVectorData : private Allocator
		{
			// internal devector implementation

			static constexpr bool relocatable = is_relocatable<T>::value;

			T* data;			// pointer to the memory storage
			T* start;			// pointer to the first value
			T* end;				// past-the-end pointer
			size_t capacity;	// memory storage capacity

			DEVectorData(const Allocator & al = Allocator()) 
				: Allocator(al), data(nullptr), start(nullptr), end(nullptr), capacity(0){}
			DEVectorData(DEVectorData&& other) noexcept(std::is_nothrow_move_constructible<Allocator>::value)
				: Allocator(std::move(static_cast<Allocator&>(other))), data(other.data), start(other.start), end(other.end), capacity(other.capacity) {
				other.data = other.start = other.end = nullptr;
				other.capacity = 0;
			}
			/*DEVectorData(DEVectorData&& other, const Allocator& al)
				: Allocator(al), data(other.data), start(other.start), end(other.end), capacity(other.capacity) {
				other.data = other.start = other.end = nullptr;
				other.capacity = 0;
			}*/
			~DEVectorData() {
				destroy_range(start, end);
				deallocate(data, capacity);
			}

			

			auto get_allocator() noexcept -> Allocator& { return *this; }
			auto get_allocator() const noexcept -> const Allocator& { return *this; }
			auto allocate(size_t n) -> T* {return n ? get_allocator().allocate(n) : nullptr;}
			void deallocate(T* p, size_t n) { if(p) get_allocator().deallocate(p, n);}

			void destroy_range(T* begin, T* en)
			{
				// destroy values in the range [begin,end)
				if (!std::is_trivially_destructible<T>::value) {
					for (T* p = begin; p != en; ++p)
						destroy_ptr(p);
				}
			}

			template< class... Args >
			void construct_range(T* first, T* last, Args&&... args)
			{
				// construct values in the range [begin,end) with given arguments
				// in case of exception, destroy created values
				T* saved = first;
				try {
					while (first != last) {
						construct_ptr(first, std::forward<Args>(args)...);
						++first;
					}
				}
				catch (...) {
					destroy_range(saved, first);
					throw;
				}
			}


			void copy_destroy_input( T* first,  T* last, T* dst) 
			{
				// Copy range [first, last) to non overlapping dst, and destroy input range.
				// In case of exception, input is not detroyed, and created values are destroyed
				// Strong exception guarantee

				static constexpr bool noexcept_move =  std::is_nothrow_move_constructible<T>::value;

				if (relocatable)
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
					//no exception thrown, destroy input
					if(!noexcept_move)
						destroy_range(saved, last);
				}
			}

			auto grow_capacity() const -> size_t
			{
				size_t c = static_cast<size_t>(static_cast<double>(capacity) * SEQ_GROW_FACTOR);
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

				//static constexpr bool noexcept_move = std::is_nothrow_move_constructible<T>::value && std::is_nothrow_copy_constructible<T>::value;

				size_t size = static_cast<size_t>(last - first);
				if (dst + size < first || dst >= last)
					//no overlapp, use copy_destroy_input
					return copy_destroy_input(first, last, dst);

				if (dst == first || first == last)
					return; //no op

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
						//construct first (right) part
						while (end_dst != last) {
							construct_ptr(end_dst -1, std::move(*(src_end-1)));
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
						//destroy previously created values
						while (saved != end_dst && saved != last) {
							destroy_ptr(--saved);
						}
						throw;
					}
				}

			}

			void clear() noexcept
			{
				destroy_range(start, end);
				start = end = data;
			}

			void shrink_to_fit()
			{
				// Strong exception guarantee
				size_t size = static_cast<size_t>(end - start);
				if (size == capacity)
					return;

				T* _new = allocate(size);
				try {
					copy_destroy_input(start, end, _new);
				}
				catch (...) {
					deallocate(_new, size);
					throw;
				}
				data = _new;
				start = _new;
				capacity = size;
				end = start + capacity;
			}

			void reserve(size_t new_capacity)
			{
				// Strong exception guatantee
				if (new_capacity <= capacity)
					return;

				size_t size = static_cast<size_t>(end - start);
				T* _new = allocate(new_capacity);
				T* _new_start = flag == OptimizeForPushBack ? _new : flag == OptimizeForPushFront ? _new + new_capacity - size : (_new + (new_capacity - size) / 2);
				T* _new_end = _new_start + size;

				try {

					// copy from old to new
					copy_destroy_input(start, end, _new_start);
				}
				catch (...) {
					deallocate(_new, new_capacity);
					throw;
				}

				deallocate(data, capacity);

				data = _new;
				start = _new_start;
				end = _new_end;
				capacity = new_capacity;
			}

			void reserve_back(size_t new_back_capacity)
			{
				// Basic exception guarantee
				size_t size = static_cast<size_t>(end - start);
				size_t back_capacity = static_cast<size_t>((data + capacity) - end);
				if (back_capacity >= new_back_capacity)
					return;

				size_t required_capacity = new_back_capacity + size;
				
				if (required_capacity <= capacity) {
					//move data
					T* _new_start = flag == OptimizeForPushBack ? data : data + capacity - required_capacity;
					move_destroy_input(start, end, _new_start);
					start = _new_start;
					end = start + size;						
				}
				else
				{
					T* _new = allocate(required_capacity);
					try {
						copy_destroy_input(start, end, _new);
					}
					catch(...) {
						deallocate(_new, required_capacity);
						throw;
					}
					deallocate(data, capacity);
					data = _new;
					start = _new;
					end = _new + size;
					capacity = required_capacity;
				}
			}

			void reserve_front(size_t new_front_capacity)
			{
				// Basic exception guarantee
				size_t size = static_cast<size_t>(end - start);
				size_t front_capacity = static_cast<size_t>(start - data);
				if (front_capacity >= new_front_capacity)
					return;

				size_t required_capacity = new_front_capacity + size;

				if (required_capacity <= capacity) {
					//move data
					T* _new_start = flag == OptimizeForPushFront ? data + capacity -size : data + new_front_capacity;
					move_destroy_input(start, end, _new_start);
					start = _new_start;
					end = start + size;
				}
				else
				{
					T* _new = allocate(required_capacity);
					T* _new_start = _new + new_front_capacity;
					try {
						copy_destroy_input(start, end, _new_start);
					}
					catch (...) {
						deallocate(_new, required_capacity);
						throw;
					}
					deallocate(data, capacity);
					data = _new;
					start = _new_start;
					end = start + size;
					capacity = required_capacity;
				}
			}

			template< class... Args >
			void resize(size_t new_size, Args&&... args)
			{
				// Strong exception guarantee

				size_t size = static_cast<size_t>(end - start);
				if (size == new_size)
					return;

				if (new_size > size)
				{
					// Grow
					size_t remaining = static_cast<size_t>((data + capacity) - end);
					if (remaining >= (new_size - size)) {
						// no need to allocate, just construct
						T* new_end = end + (new_size - size);
						construct_range(end, new_end, std::forward<Args>(args)...);
						end = new_end;
					}
					else {
						//reallocate, might throw, fine
						size_t _new_capacity = new_size;
						T* _new = allocate(_new_capacity);
						T* _new_start = flag == OptimizeForPushBack ? _new : flag == OptimizeForPushFront ? _new + _new_capacity - new_size : (_new + (_new_capacity - new_size) / 2);
						T* _new_end = _new_start + new_size;

						try {
							// construct right elements
							construct_range(_new_start + size, _new_end, std::forward<Args>(args)...);
						}
						catch (...) {
							deallocate(_new, _new_capacity);
							throw;
						}
						try {
							// copy from old to new
							copy_destroy_input(start, end, _new_start);
						}
						catch (...) {
							deallocate(_new, _new_capacity);
							throw;
						}

						deallocate(data, capacity);

						data = _new;
						start = _new_start;
						end = _new_end;
						capacity = _new_capacity;
					}
				}
				else
				{
					// Shrink
					T* _new_end = start + new_size;
					destroy_range(_new_end, end);
					end = _new_end;
				}

			}

			template< class... Args >
			void resize_front(size_t new_size, Args&&... args)
			{
				// Strong exception guarantee

				size_t size = end - start;
				if (size == new_size)
					return;

				if (new_size > size)
				{
					// Grow
					size_t remaining = start - data;
					if (remaining >= (new_size - size)) {
						// no need to allocate, just construct
						T* new_start = start - (new_size - size);
						construct_range(new_start, start, std::forward<Args>(args)...);
						start = new_start;
					}
					else {
						//reallocate, might throw, fine
						size_t _new_capacity = new_size;
						T* _new = allocate(_new_capacity);
						T* _new_start = flag == OptimizeForPushBack ? _new : flag == OptimizeForPushFront ? _new + _new_capacity - new_size : (_new + (_new_capacity - new_size) / 2);
						T* _new_end = _new_start + new_size;

						try {
							construct_range(_new, _new + (new_size - size), std::forward<Args>(args)...);
						}
						catch (...) {
							deallocate(_new, _new_capacity);
							throw;
						}
						try {
							copy_destroy_input(start, end, _new_start + (new_size - size));
						}
						catch (...) {
							deallocate(_new, _new_capacity);
							throw;
						}
						data = _new;
						start = _new_start;
						end = _new_end;
						capacity = _new_capacity;
					}
				}
				else
				{
					// Shrink
					T* _new_start = start + (size - new_size);
					destroy_range(start, _new_start);
					start = _new_start;
				}
			}


			void grow_back()
			{
				// Strong exception guarantee if move constructor and move assignation operator are noexcept
				// Otherwise basic exception guarantee

				// grow_back is only called when there is no more room on the back
				SEQ_ASSERT_DEBUG(end == data + capacity, "");

				size_t size = static_cast<size_t>(end - start);
				size_t remaining_front = static_cast<size_t>(start - data);

				// Check if we have enough space at the front to move data
				if (remaining_front)
					if (remaining_front > size / SEQ_DEVECTOR_SIZE_LIMIT || flag == OptimizeForPushFront) {
						// Move data toward the front
						T* new_start = flag == OptimizeForPushBack ? data : flag == OptimizeForBothEnds ? (data + remaining_front / 2) : start  ;
						if (new_start == start)
							--new_start;
						move_destroy_input(start, end, new_start);
						start = new_start;
						end = start + size;
						return;
					}

				// reallocate
				size_t new_capacity = grow_capacity();
				T* _new = allocate(new_capacity);
				T* _new_start = flag == OptimizeForPushBack ? _new : flag == OptimizeForPushFront ? _new + new_capacity - size : (_new + (new_capacity - size) / 2);
				if (_new_start + size == _new + new_capacity)
					--_new_start;
				T* _new_end = _new_start + size;

				try {

					// copy from old to new
					copy_destroy_input(start, end, _new_start);
				}
				catch (...) {
					deallocate(_new, new_capacity);
					throw;
				}

				deallocate(data, capacity);

				data = _new;
				start = _new_start;
				end = _new_end;
				capacity = new_capacity;
			}

			void grow_front()
			{
				// Strong exception guarantee if move constructor and move assignation operator are noexcept
				// Otherwise basic exception guarantee

				// grow_front is only called when there is no more room on the back
				SEQ_ASSERT_DEBUG(start == data, "");

				size_t size = static_cast<size_t>(end - start);
				size_t remaining_back = static_cast<size_t>((data + capacity) - end);

				// Check if we have enough space at the front to move data
				if (remaining_back)
					if (remaining_back > size / SEQ_DEVECTOR_SIZE_LIMIT || flag == OptimizeForPushBack) {
						// Move data toward the back
						T* new_start = flag == OptimizeForPushBack ? data : flag == OptimizeForBothEnds ? (data + remaining_back / 2) : start;
						if (new_start == start)
							++new_start;
						move_destroy_input(start, end, new_start);
						start = new_start;
						end = start + size;
						return;
					}

				// reallocate
				size_t new_capacity = grow_capacity();
				T* _new = allocate(new_capacity);
				T* _new_start = flag == OptimizeForPushBack ? _new : flag == OptimizeForPushFront ? _new + new_capacity - size : (_new + (new_capacity - size) / 2);
				if (_new_start  == _new)
					++_new_start;
				T* _new_end = _new_start + size;

				try {

					// copy from old to new
					copy_destroy_input(start, end, _new_start);
				}
				catch (...) {
					deallocate(_new, new_capacity);
					throw;
				}

				deallocate(data, capacity);

				data = _new;
				start = _new_start;
				end = _new_end;
				capacity = new_capacity;
			}

		};
	}


	/// @brief Double-ending vector implementation which can be optimized for several use case.
	/// @tparam T value type
	/// @tparam Allocator allocator type
	/// @tparam flag optimization flag
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
	/// Optimization flags
	/// ------------------
	/// 
	/// devector can be configured with the following flags:
	///		-	OptimizeForPushBack: the devector behaves like a std::vector, adding free space at the back based on the growth factor SEQ_GROW_FACTOR.
	///			In this case, inserting elements at the front is as slow as for std::vector as it require to move all elements toward the back.
	///		-	OptimizeForPushFront: the devector adds free space at the front based on the growth factor SEQ_GROW_FACTOR. Inserting elements at the front
	///			is amortized O(1), inserting at the back is O(N).
	///		-	OptimizeForBothEnds (default): the devector has as many free space at the back and the front. Both push_back() and push_front() behave in amortized O(1).
	///			When the memory storage grows (by a factor of SEQ_GROW_FACTOR), the elements are moved to the middle of the storage, leaving as much space at the front and the back.
	///			When inserting an element at the back, several scenarios are checked (this is similar for front insertion):
	///				-#	Free slots are available at the back and the element is inserted there.
	///				-#	The devector does not have available slots at the back or the front, a new chunk of memory of size size()*SEQ_GROW_FACTOR is allocated,
	///				elements are moved to this new memory location (leaving the same capacity at the back and the front) and the new element is inserted at the back.
	///				-#	The devector does not have enough capacity at the back, but has free capacity at the front. In this case, there are 2 possibilities:
	///					1.	front capacity is greater than size() / __SEQ_DEVECTOR_SIZE_LIMIT: elements are moved toward the front, leaving the same capacity at the back and the front.
	///					The new element is then inserted at the back.
	///					2.	front capacity is lower or equal to size() / __SEQ_DEVECTOR_SIZE_LIMIT: a new chunk is allocated like in b). By default, __SEQ_DEVECTOR_SIZE_LIMIT is set to 16.
	///					__SEQ_DEVECTOR_SIZE_LIMIT can be adjusted to provide a different trade-off between insertion speed and memory usage.
	/// 
	/// 
	/// Performances
	/// ------------
	/// 
	/// Internal benchmarks show that devector is as fast as std::vector when inserting at the back with OptimizeForPushBack, or inserting at the front with OptimizeForPushFront.
	/// Using OptimizeForBothEnds makes insertion at both ends usually twice as slow as back insertion for std::vector.
	/// 
	/// seq::devector is faster than std::vector for relocatable types (where seq::is_relocatable<T>::value is true) as memcpy and memmove can be used instead of std::copy or std::move on reallocation.
	/// 
	/// Inserting a new element in the middle of a devector is on average twice as fast as on std::vector, since the values can be pushed to either ends, whichever is faster (at least with OptimizeForBothEnds).
	/// 
	template<class T, class Allocator = std::allocator<T>, DEVectorFlag flag = OptimizeForBothEnds >
	class devector : private detail::DEVectorData<T, Allocator, flag>
	{
		using base_type = detail::DEVectorData<T, Allocator, flag>;

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
			:base_type() {}
		/// @brief Constructs an empty container with the given allocator alloc.
		/// @param alloc allocator to use for all memory allocations of this container
		explicit devector(const Allocator& alloc)
			:base_type(alloc) {}
		/// @brief Constructs the container with count copies of elements with value value.
		/// @param count the size of the container
		/// @param value the value to initialize elements of the container with
		/// @param alloc allocator to use for all memory allocations of this container
		devector(size_type count, const T& value, const Allocator& alloc)
			:base_type(alloc) {
			assign(count, value);
		}
		/// @brief Constructs the container with count element default constructed.
		/// @param count the size of the container
		/// @param alloc allocator to use for all memory allocations of this container
		explicit devector(size_type count, const Allocator& alloc = Allocator())
			:base_type(alloc) {
			resize(count);
		}
		/// @brief Constructs the container with the contents of the range [first, last).
		/// @param first the range to copy the elements from
		/// @param last the range to copy the elements from
		/// @param alloc allocator to use for all memory allocations of this container
		template< class InputIt >
		devector(InputIt first, InputIt last, const Allocator& alloc = Allocator())
			: base_type(alloc) {
			assign(first, last);
		}
		/// @brief Copy constructor
		/// @param other another container to be used as source to initialize the elements of the container with
		devector(const devector& other)
			:base_type(copy_allocator(other.get_allocator())) {
			assign(other.begin(), other.end());
		}
		/// @brief Copy constructor
		/// @param other another container to be used as source to initialize the elements of the container with
		/// @param alloc allocator to use for all memory allocations of this container
		devector(const devector& other, const Allocator & alloc)
			:base_type(alloc) {
			assign(other.begin(), other.end());
		}
		/// @brief Move constructor
		/// @param other another container to be used as source to initialize the elements of the container with
		devector(devector&& other) noexcept(std::is_nothrow_move_constructible<base_type>::value)
			:base_type(std::move(static_cast<base_type&>(other))) {}
		/// @brief Move constructor
		/// @param other another container to be used as source to initialize the elements of the container with
		/// @param alloc allocator to use for all memory allocations of this container
		devector(devector&& other, const Allocator& alloc)
			:base_type(alloc)
		{
			if (alloc == other.get_allocator()) {
				swap(other);
			}
			else {
				resize(other.size());
				std::move(other.begin(), other.end(), begin());
			}
		}
		/// @brief Constructs the container with the contents of the initializer list init
		/// @param init initializer list to initialize the elements of the container with
		/// @param alloc allocator to use for all memory allocations of this container
		devector(std::initializer_list<T> init, const Allocator& alloc = Allocator())
			:base_type(alloc) {
			assign(init.begin(), init.end());
		}

		~devector() {
			clear();
		}

		/// @brief Returns the container size
		auto size() const noexcept -> size_t { return static_cast<size_t>(this->base_type::end - this->start); }
		/// @brief Returns the container full capacity (back_capacity() + size() + front_capacity())
		auto capacity() const noexcept -> size_t { return this->base_type::capacity; }
		/// @brief Returns the container back capacity
		auto back_capacity() const noexcept -> size_t { return this->data + capacity() - this->base_type::end; }
		/// @brief Returns the container front capacity
		auto front_capacity() const noexcept -> size_t { return this->start - this->data; }
		/// @brief Returns the container maximum size
		auto max_size() const noexcept -> size_t { return std::numeric_limits<size_t>::max(); }
		/// @brief Returns true if the container is empty, false otherwise
		auto empty() const noexcept -> bool {return this->base_type::end == this->start;}
		/// @brief Returns the container allocator object
		auto get_allocator() noexcept -> Allocator& { return this->base_type::get_allocator(); }
		/// @brief Returns the container allocator object
		auto get_allocator() const -> const Allocator & { return this->base_type::get_allocator(); }

		/// @brief Clear the container, but does not deallocate the storage
		void clear() noexcept
		{
			this->base_type::clear();
		}
		/// @brief Requests the removal of unused capacity.
		/// Strong exception guarentee.
		void shrink_to_fit()
		{
			this->base_type::shrink_to_fit();
		}

		/// @brief Insert an element at the back of the container.
		/// The complexity is amortized O(1) for OptimizeForPushBack and OptimizeForBothEnds, O(N) for OptimizeForPushFront.
		/// Strong exception guarantee if move constructor and move assignment operator are noexcept. Otherwise basic exception guarantee.
		/// Invalidate all references and iterators if back_capacity() == 0.
		/// @param value value to insert
		SEQ_ALWAYS_INLINE void push_back(const T& value) 
		{
			if (SEQ_UNLIKELY(this->base_type::end == this->base_type::data + capacity()))
				this->grow_back();

			construct_ptr(this->base_type::end, value);
			++this->base_type::end;
		}
		/// @brief Insert an element at the back of the container using move semantic.
		/// The complexity is amortized O(1) for OptimizeForPushBack and OptimizeForBothEnds, O(N) for OptimizeForPushFront.
		/// Strong exception guarantee if move constructor and move assignment operator are noexcept. Otherwise basic exception guarantee.
		/// Invalidate all references and iterators if back_capacity() == 0.
		/// @param value value to insert
		SEQ_ALWAYS_INLINE void push_back( T&& value)
		{
			if (SEQ_UNLIKELY(this->base_type::end == this->base_type::data + capacity()))
				this->grow_back();

			construct_ptr(this->base_type::end, std::move(value));
			++this->base_type::end;
		}
		/// @brief Appends a new element to the end of the container. 
		/// The element is constructed through std::allocator_traits::construct, which typically uses placement-new to construct the element in-place at the location provided by the container. 
		/// The arguments args... are forwarded to the constructor as std::forward<Args>(args)....
		/// The complexity is amortized O(1) for OptimizeForPushBack and OptimizeForBothEnds, O(N) for OptimizeForPushFront.
		/// Strong exception guarantee if move constructor and move assignment operator are noexcept. Otherwise basic exception guarantee.
		/// Invalidate all references and iterators if back_capacity() == 0.
		template< class... Args >
		SEQ_ALWAYS_INLINE auto emplace_back(Args&&... args) -> reference
		{
			if (SEQ_UNLIKELY(this->base_type::end == this->base_type::data + capacity()))
				this->grow_back();
			construct_ptr(this->base_type::end, std::forward<Args>(args)...);
			++this->base_type::end;
			return *(this->base_type::end - 1);
		}


		/// @brief Insert an element at the front of the container.
		/// The complexity is amortized O(1) for OptimizeForPushFront and OptimizeForBothEnds, O(N) for OptimizeForPushBack.
		/// Strong exception guarantee if move constructor and move assignment operator are noexcept. Otherwise basic exception guarantee.
		/// Invalidate all references and iterators if front_capacity() == 0.
		/// @param value value to insert
		SEQ_ALWAYS_INLINE void push_front(const T& value)
		{
			if (SEQ_UNLIKELY(this->start == this->base_type::data ))
				this->grow_front();

			construct_ptr(this->start - 1, value);
			--this->start;
		}
		/// @brief Insert an element at the front of the container using move semantic.
		/// The complexity is amortized O(1) for OptimizeForPushFront and OptimizeForBothEnds, O(N) for OptimizeForPushBack.
		/// Strong exception guarantee if move constructor and move assignment operator are noexcept. Otherwise basic exception guarantee.
		/// Invalidate all references and iterators if front_capacity() == 0.
		/// @param value value to insert
		SEQ_ALWAYS_INLINE void push_front( T&& value)
		{
			if (SEQ_UNLIKELY(this->start == this->base_type::data))
				this->grow_front();

			construct_ptr(this->start - 1, std::move(value));
			--this->start;
		}
		/// @brief Appends a new element to the front of the container. 
		/// The element is constructed through std::allocator_traits::construct, which typically uses placement-new to construct the element in-place at the location provided by the container. 
		/// The arguments args... are forwarded to the constructor as std::forward<Args>(args)....
		/// The complexity is amortized O(1) for OptimizeForPushFront and OptimizeForBothEnds, O(N) for OptimizeForPushBack.
		/// Strong exception guarantee if move constructor and move assignment operator are noexcept. Otherwise basic exception guarantee.
		/// Invalidate all references and iterators if front_capacity() == 0.
		template< class... Args >
		SEQ_ALWAYS_INLINE auto emplace_front(Args&&... args) -> reference
		{
			if (SEQ_UNLIKELY(this->start == this->base_type::data))
				this->grow_front();
			construct_ptr(this->start - 1, std::forward<Args>(args)...);
			--this->start;
			return *this->start;
		}


		/// @brief Inserts a new element into the container directly before pos.
		/// The element is constructed through std::allocator_traits::construct, which typically uses placement-new to construct the element in-place at a location provided by the container. 
		/// However, if the required location has been occupied by an existing element, the inserted element is constructed at another location at first, and then move assigned into the required location.
		/// The arguments args... are forwarded to the constructor as std::forward<Args>(args).... args... may directly or indirectly refer to a value in the container.
		/// Invalidate all references and iterators.
		/// Strong exception guarantee if move constructor and move assignment operator are noexcept. Otherwise basic exception guarantee.
		///
		/// @return iterator to the inserted element
		template< class... Args >
		auto emplace(const_iterator pos, Args&&... args) -> iterator
		{
			size_t dist = static_cast<size_t>(pos - begin());
			SEQ_ASSERT_DEBUG(dist <= size() , "devector: invalid insertion location");

			if (dist < size() / 2) {
				//insert on the left side
				emplace_front();
				std::move(begin() + 1, begin() + 1 + dist, begin());
				*(begin() + dist) = T(std::forward<Args>(args)...);
			}
			else {
				//insert on the right side
				emplace_back(std::forward<Args>(args)...);
				std::rotate(begin() + dist, end() - 1, end());
			}
			return begin() + dist;
		}
		/// @brief Inserts a new element into the container directly before pos.
		/// Invalidate all references and iterators.
		/// Strong exception guarantee if move constructor and move assignment operator are noexcept. Otherwise basic exception guarantee.
		///
		/// @return iterator to the inserted element
		auto insert(const_iterator pos, const T& value) -> iterator
		{
			return emplace(pos, value);
		}
		/// @brief Inserts a new element into the container directly before pos using move semantic.
		/// Invalidate all references and iterators.
		/// Strong exception guarantee if move constructor and move assignment operator are noexcept. Otherwise basic exception guarantee.
		///
		/// @return iterator to the inserted element
		auto insert(const_iterator pos, T&& value) -> iterator
		{
			return emplace(pos, std::move(value));
		}
		/// @brief Inserts elements from range [first, last) before pos.
		/// Invalidate all references and iterators.
		/// Strong exception guarantee if move constructor and move assignment operator are noexcept. Otherwise basic exception guarantee.
		/// @param pos 	iterator before which the content will be inserted. pos may be the end() iterator
		/// @param first the range of elements to insert, can't be iterators into container for which insert is called
		/// @param last the range of elements to insert, can't be iterators into container for which insert is called
		/// @return iterator pointing to the first element inserted, or pos if first==last.
		template< class InputIt >
		auto insert(const_iterator pos, InputIt first, InputIt last) -> iterator
		{	
			size_type off = static_cast<size_t>(pos - begin());
			SEQ_ASSERT_DEBUG(off <= size() , "devector insert iterator outside range");
			size_type oldsize = size();

			if (first == last)
				;
			else if (off <= size() / 2)
			{	// closer to front, push to front then rotate
				try {
					if (size_t len = seq::distance(first, last))
						reserve_front(len);

					for (; first != last; ++first)
						push_front(*first);	// prepend flipped
				}
				catch (...) {
					for (; oldsize < size(); )
						pop_front();	// restore old size, at least
					throw;
				}

				difference_type num = static_cast<difference_type>(size() - oldsize);
				std::reverse(begin(), begin() + num);	// flip new stuff in place
				std::rotate(begin(), begin() + num, begin() + num + static_cast<difference_type>(off));
			}
			else
			{	// closer to back
				try {
					if (size_t len = seq::distance(first, last))
						reserve_back(len);
					
					for (; first != last; ++first)
						push_back(*first);	// append
					
				}
				catch (...) {
					for (; oldsize < size(); )
						pop_back();	// restore old size, at least
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
		auto insert(const_iterator pos, size_type count, const T& value) -> iterator
		{
			return insert(pos, cvalue_iterator<T>(0,value), cvalue_iterator<T>(count));
		}
		/// @brief  inserts elements from initializer list ilist before pos
		/// Invalidate all references and iterators.
		/// Strong exception guarantee if move constructor and move assignment operator are noexcept. Otherwise basic exception guarantee.
		/// @param pos iterator before which the content will be inserted. pos may be the end() iterator
		/// @param ilist 	initializer list to insert the values from
		/// @return iterator pointing to the first element inserted, or pos if first==last
		auto insert(const_iterator pos, std::initializer_list<T> ilist) -> iterator
		{
			return insert(pos, ilist.begin(), ilist.end());
		}


		/// @brief Assign elements from range [first, last) to the container.
		/// Invalidate all references and iterators.
		/// Basic exception guarantee.
		template< class InputIt >
		void assign(InputIt first, InputIt last)
		{
			try {
				if (size_t len = seq::distance(first, last)) {
					resize(len);
					std::copy(first, last, begin());
				}
				else {
					clear();
					for (; first != last; ++first)
						push_back(*first);
				}
			}
			catch (...) {
				clear();
				throw;
			}			
		}
		/// @brief Replaces the contents with count copies of value value
		/// Invalidate all references and iterators.
		/// Basic exception guarantee.
		void assign(size_type count, const T& value)
		{
			assign(cvalue_iterator<T>(0, value), cvalue_iterator<T>(count));
		}
		/// @brief Replaces the contents with the elements from the initializer list ilist
		/// Invalidate all references and iterators.
		/// Basic exception guarantee.
		void assign(std::initializer_list<T> ilist)
		{
			assign(ilist.begin(), ilist.end());
		}


		/// @brief Removes the last element of the container
		/// Iterators and references to the last element, as well as the end() iterator, are invalidated.
		void pop_back() noexcept
		{
			SEQ_ASSERT_DEBUG(size() > 0, "pop_back() on empty devector");
			destroy_ptr(--this->base_type::end);
		}
		/// @brief Removes the first element of the container
		/// Iterators and references to the first element are invalidated.
		void pop_front() noexcept
		{
			SEQ_ASSERT_DEBUG(size() > 0, "pop_front() on empty devector");
			destroy_ptr(this->base_type::start++);
		}

		/// @brief Removes the elements in the range [first, last).
		/// Basic exception guarantee
		/// @param first range of elements to remove
		/// @param last range of elements to remove
		/// @return Iterator following the last removed element
		auto erase(const_iterator first, const_iterator last) -> const_iterator
		{	
			SEQ_ASSERT_DEBUG(last >= first && first >= begin() && last <= end(), "devector erase iterator outside range");
			if (first == last)
				return last;

			size_type off = static_cast<size_t>(first - begin());
			size_type count = static_cast<size_t>(last - first);

			if (off < static_cast<size_type>(end() - last))
			{	// closer to front
				std::move_backward(begin(), const_cast<iterator>(first), const_cast<iterator>(last));	// copy over hole
				for (; 0 < count; --count)
					pop_front();	// pop copied elements
			}
			else
			{	// closer to back
				std::move(const_cast<iterator>(last), end(), const_cast<iterator>(first));	// copy over hole
				for (; 0 < count; --count)
					pop_back();	// pop copied elements
			}

			return cbegin() + off;
		}
		/// @brief Removes the element at pos
		/// Basic exception guarantee
		/// @param pos iterator to the position to erase
		/// @return Iterator following the last removed element.
		auto erase(const_iterator pos) -> const_iterator
		{
			return erase(pos, pos + 1);
		}

		/// @brief Swap this container with other
		/// Does not invalidated iterators, including end() iterator.
		void swap(devector& other) noexcept(noexcept(swap_allocator(std::declval<Allocator&>(), std::declval<Allocator&>())))
		{
			if (this != std::addressof(other)) {
				std::swap(base_type::data, other.base_type::data);
				std::swap(base_type::start, other.base_type::start);
				std::swap(base_type::end, other.base_type::end);
				std::swap(base_type::capacity, other.base_type::capacity);
				swap_allocator(get_allocator(), other.get_allocator());
			}
		}

		/// @brief Increase the capacity of the devector (the total number of elements that the devector can hold without requiring reallocation) to a value that's greater or equal to new_cap.
		/// If new_cap is greater than the current capacity(), new storage is allocated, otherwise the function does nothing.
		/// When reallocating, the front and back capacity is adjusted depending on the optimization flag.
		/// Invalidate iterators and references if a new storage is allocated.
		/// Strong exception guarantee.
		void reserve(size_t new_cap)
		{
			this->base_type::reserve(new_cap);
		}

		/// @brief Ensure that the devector has at least new_back_capacity free slots at the back, no matter what is the optimization flag.
		/// Invalidate iterators and references if a new storage is allocated.
		/// Strong exception guarantee if move constructor and move assignment operator are noexcept. Otherwise basic exception guarantee.
		/// @param new_back_capacity minimum back capacity
		void reserve_back(size_t new_back_capacity)
		{
			this->base_type::reserve_back(new_back_capacity);
		}
		/// @brief Ensure that the devector has at least new_front_capacity free slots at the front, no matter what is the optimization flag.
		/// Invalidate iterators and references if a new storage is allocated.
		/// Strong exception guarantee if move constructor and move assignment operator are noexcept. Otherwise basic exception guarantee.
		/// @param new_back_capacity minimum front capacity
		void reserve_front(size_t new_front_capacity)
		{
			this->base_type::reserve_front(new_front_capacity);
		}

		/// @brief Resizes the container to contain count elements.
		/// Invalidate iterators and references if a new storage is allocated.
		/// Strong exception guarantee.
		/// @param count new container size 
		void resize(size_t count)
		{
			this->base_type::resize(count);
		}
		/// @brief Resizes the container to contain count elements.
		/// Invalidate iterators and references if a new storage is allocated.
		/// Strong exception guarantee.
		/// @param count new container size 
		/// @param value if count is greater than size(), copies of value are appended to the back of the devector
		void resize(size_t count, const T & value)
		{
			this->base_type::resize(count,value);
		}

		/// @brief Resizes the container to contain count elements.
		/// The container is extended by the front.
		/// Invalidate iterators and references if a new storage is allocated.
		/// Strong exception guarantee.
		/// @param count new container size 
		void resize_front(size_t new_size)
		{
			this->base_type::resize_front(new_size);
		}
		/// @brief Resizes the container to contain count elements.
		/// The container is extended by the front.
		/// Invalidate iterators and references if a new storage is allocated.
		/// Strong exception guarantee.
		/// @param count new container size 
		/// @param value if count is greater than size(), copies of value are prepended to the back of the devector
		void resize_front(size_t new_size, const T& value)
		{
			this->base_type::resize_front(new_size, value);
		}

		/// @brief Returns pointer to the underlying array serving as element storage. The pointer is such that range [data(); data() + size()) is always a valid range, 
		/// even if the container is empty (data() is not dereferenceable in that case).
		auto data() noexcept -> T* { return this->start; }
		/// @brief Returns pointer to the underlying array serving as element storage. The pointer is such that range [data(); data() + size()) is always a valid range, 
		/// even if the container is empty (data() is not dereferenceable in that case).
		auto data() const noexcept -> const T* { return this->start; }

		/// @brief Returns a reference to the back element
		auto back() noexcept -> T& { SEQ_ASSERT_DEBUG(size() > 0, "empty container"); return *(this->base_type::end -1); }
		/// @brief Returns a reference to the back element
		auto back() const noexcept -> const T& { SEQ_ASSERT_DEBUG(size() > 0, "empty container"); return *(this->base_type::end - 1); }

		/// @brief Returns a reference to the front element
		auto front() noexcept -> T& { SEQ_ASSERT_DEBUG(size() > 0, "empty container"); return *(this->start); }
		/// @brief Returns a reference to the front element
		auto front() const noexcept -> const T& { SEQ_ASSERT_DEBUG(size() > 0, "empty container"); return *(this->start); }

		/// @brief Returns a reference to the element at pos
		auto operator[](size_t pos) const noexcept -> const T& { SEQ_ASSERT_DEBUG(pos < size(), "invalid position"); return this->start[pos]; }
		/// @brief Returns a reference to the element at pos
		auto operator[](size_t pos) noexcept -> T& { SEQ_ASSERT_DEBUG(pos < size(), "invalid position"); return this->start[pos]; }

		/// @brief Returns a reference to the element at pos.
		/// Throw std::out_of_range if pos is invalid. 
		auto at(size_t pos) const -> const T&  {
			if (pos >= size()) throw std::out_of_range("devector out of range");
			return this->start[pos]; 
		}
		/// @brief Returns a reference to the element at pos.
		/// Throw std::out_of_range if pos is invalid. 
		auto at(size_t pos) -> T&  { 
			if (pos >= size()) throw std::out_of_range("devector out of range");
			return this->start[pos];
		}

		/// @brief Returns an iterator to the first element of the devector.
		auto begin() const noexcept -> const_iterator { return this->start; }
		/// @brief Returns an iterator to the first element of the devector.
		auto begin() noexcept -> iterator { return this->start; }
		/// @brief Returns an iterator to the element following the last element of the devector.
		auto end() const noexcept -> const_iterator { return this->base_type::end; }
		/// @brief Returns an iterator to the element following the last element of the devector.
		auto end() noexcept -> iterator { return this->base_type::end; }
		/// @brief Returns a reverse iterator to the first element of the reversed devector.
		auto rbegin() noexcept -> reverse_iterator { return reverse_iterator(end()); }
		/// @brief Returns a reverse iterator to the first element of the reversed devector.
		auto rbegin() const noexcept -> const_reverse_iterator { return const_reverse_iterator(end()); }
		/// @brief Returns a reverse iterator to the element following the last element of the reversed devector.
		auto rend() noexcept -> reverse_iterator { return reverse_iterator(begin()); }
		/// @brief Returns a reverse iterator to the element following the last element of the reversed devector.
		auto rend() const noexcept -> const_reverse_iterator { return const_reverse_iterator(begin()); }
		/// @brief Returns an iterator to the first element of the devector.
		auto cbegin() const noexcept -> const_iterator { return begin(); }
		/// @brief Returns an iterator to the element following the last element of the devector.
		auto cend() const noexcept -> const_iterator { return end(); }
		/// @brief Returns a reverse iterator to the first element of the reversed devector.
		auto crbegin() const noexcept -> const_reverse_iterator { return rbegin(); }
		/// @brief Returns a reverse iterator to the element following the last element of the reversed devector.
		auto crend() const noexcept -> const_reverse_iterator { return rend(); }

		/// @brief Copy operator
		template<class Alloc, DEVectorFlag F>
		auto operator=(const devector<T,Alloc,F>& other) -> devector &
		{
			resize(other.size());
			std::copy(other.begin(), other.end(), begin());
			return *this;
		}
		/// @brief Copy operator
		auto operator=(const devector& other) -> devector&
		{
			if (this != std::addressof(other)) 
			{
				if SEQ_CONSTEXPR(assign_alloc<Allocator>::value)
				{
					if (get_allocator() != other.get_allocator()) {
						clear();
					}
				}
				assign_allocator(get_allocator(), other.get_allocator());
				resize(other.size());
				std::copy(other.begin(), other.end(), begin());
			}
			return *this;
		}
		/// @brief Move assignment operator
		auto operator=( devector&& other) noexcept(noexcept(std::declval<devector&>().swap(std::declval<devector&>()))) -> devector&
		{
			swap(other);
			return *this;
		}
	};


	/// @brief  Specialization of is_relocatable for devector
	template<class T , class Alloc, DEVectorFlag F>
	struct is_relocatable<devector<T, Alloc, F> > : std::true_type {};

}
#endif
