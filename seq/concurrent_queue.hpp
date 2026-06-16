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

#ifndef SEQ_CONCURRENT_QUEUE_HPP
#define SEQ_CONCURRENT_QUEUE_HPP

#include <iterator>

#include "lock.hpp"
#include "internal/utils.hpp"

namespace seq
{
	namespace detail
	{

		// Internal implementation of the concurrent queue
		template<class T, class Allocator = std::allocator<T>>
		class QueueImpl : private Allocator
		{
			// Create mask value based on bit count
			template<uint64_t Count>
			static constexpr uint64_t QueueMask()
			{
				if constexpr (Count == 64)
					return std::numeric_limits<uint64_t>::max();
				else
					return ((1ull << count) - 1ull);
			}

		public:
			using value_type = T;
			using size_type = uint64_t;
			using lock_type = std::mutex;
			using atomic_type = std::atomic<size_type>;

			// Number of elements per bucket (power of 2)
			static constexpr size_type count = sizeof(T) <= 32 ? 64 : sizeof(T) <= 64 ? 32 : sizeof(T) <= 128 ? 16 : sizeof(T) <= 256 ? 8 : 4;

			// Mask value to retrieve bucket position based on head/tail position
			static constexpr size_type mask_full = QueueMask<count>();

			// Invalid position
			static constexpr size_type invalid = std::numeric_limits<size_type>::max();

			struct Bucket;
			using atomic_bucket = std::atomic<Bucket*>;

			struct BaseBucket
			{
				atomic_bucket prev{ nullptr };	  // Linked list
				atomic_bucket next{ nullptr };	  // Linked list
				size_type head_start = mask_full; // Bucket index

				bool is_end() const noexcept { return head_start == mask_full; }
			};
			// Bucket structure, stores up to count elements
			struct Bucket : BaseBucket
			{
				atomic_type cnt{ mask_full };			      // Mask of created (constructed) elements
				atomic_type pop{ 0 };				      // Mask of poped (removed) elements
				alignas(T) char values[sizeof(T) * count] = { '\0' }; // Elements

				// Return pointer to elements
				SEQ_ALWAYS_INLINE T* ptr() noexcept { return (T*)values; }

				// Return bitmask of valid elements
				size_type valid_mask() const noexcept { return cnt.load(std::memory_order_relaxed) & (~pop.load(std::memory_order_relaxed)); }

				// Return index of first valid element
				uint16_t first_valid_index() const noexcept
				{
					auto mask = valid_mask();
					return mask ? (uint16_t)bit_scan_forward_64(mask) : 0;
				}

				// Return index of last valid element
				uint16_t last_valid_index() const noexcept
				{
					auto mask = valid_mask();
					return mask ? (uint16_t)bit_scan_reverse_64(mask) : 0;
				}

				// Return if given index contains a valid element
				bool is_valid(unsigned idx) const noexcept
				{
					auto mask = valid_mask();
					return mask & (1ull << idx);
				}
			};

		private:
			// Head (insert) position
			atomic_type d_head{ 0 };

			// Tail (pop) position
			atomic_type d_tail{ 0 };

			// End bucket of linked list
			BaseBucket d_end;

			// Free list of buckets
			atomic_bucket d_free{ nullptr };
			atomic_type d_free_count{ 0 };
			lock_type d_free_lock;

			template<class EarlyStop = int>
			static SEQ_ALWAYS_INLINE size_type add(atomic_type& a, EarlyStop f = {}) noexcept
			{
				// Increment atomic counter using CAS.
				// This is the main bottleneck for concurrent insert/pop,
				// and using CAS + yield is way more effective than fetch_add().

				// An early stop condition can be provided.
				
				auto val = a.load(std::memory_order_relaxed);
				uint8_t cnt = 0;
				for (;;) {
					// Early stop condition
					if constexpr (!std::is_same_v<EarlyStop, int>)
						if (f(val))
							return invalid;

					if (a.compare_exchange_strong(val, val + 1))
						return val;

					
					for (uint8_t i = 0; i < cnt +1; ++i)
						std::this_thread::yield();
					cnt = ((cnt + 1) & 31);
				}
			}

			SEQ_ALWAYS_INLINE void ensure_has_bucket()
			{
				// Make sure we have at least one free bucket available
				if (!d_free.load(std::memory_order_relaxed))
					free_bucket(new (allocate_from<Bucket>(get_allocator())) Bucket());
			}

			Bucket* pop_bucket() noexcept
			{
				// Remove and return bucket from the free list.
				auto* b = d_free.load(std::memory_order_relaxed);
				while (b && !d_free.compare_exchange_strong(b, b->next.load(std::memory_order_relaxed)))
					;
				if (b)
					d_free_count.fetch_sub(count);
				return b;
			}

			void free_bucket(Bucket* q) noexcept
			{
				// Add bucket to free list.
				auto fr = d_free.load(std::memory_order_relaxed);
				for (;;) {
					q->next.store(fr, std::memory_order_release);
					if (d_free.compare_exchange_strong(fr, q))
						break;
				}
				d_free_count.fetch_add(count);
			}

			void make_bucket(size_type head_start, T&& val) noexcept
			{
				// Create a bucket of one element using the internal free list of buckets
				Bucket* b = pop_bucket();
				b->cnt.store(1);
				b->pop.store(0);
				b->head_start = head_start;
				new (b->ptr()) T(std::move(val));

				std::scoped_lock<lock_type> ll(d_free_lock);

				// Add to list
				auto last = d_end.prev.load(std::memory_order_relaxed);
				b->next.store(end_bucket(), std::memory_order_release);
				b->prev.store(last, std::memory_order_release);

				last->next.store(b, std::memory_order_release);
				end_bucket()->prev.store(b, std::memory_order_release);
			}

			void remove_bucket(Bucket* first) noexcept
			{
				{
					std::scoped_lock<lock_type> ll(d_free_lock);

					// Remove from list and bucket to the free list
					auto next = first->next.load(std::memory_order_relaxed);
					end_bucket()->next.store(next, std::memory_order_release);
					next->prev.store(end_bucket(), std::memory_order_release);
				}
				free_bucket(first);
			}

			static SEQ_ALWAYS_INLINE void destroy(T* v) noexcept
			{
				// Destroy element without throwing
				if constexpr (!std::is_trivially_destructible_v<T>) {
					if constexpr (std::is_nothrow_destructible_v<T>)
						v->~T();
					else {
						try {
							v->~T();
						}
						catch (...) {
						}
					}
				}
			}

			template<class F>
			SEQ_ALWAYS_INLINE bool pop_internal(F fun) noexcept
			{
				// Get the tail position and increment it.
				// Fail if the tail already reached the head position.
				auto pos = add(d_tail, [&](auto p) { return p >= d_head.load(std::memory_order_relaxed); });
				// Empty
				if (pos == invalid)
					return false;

				auto idx = pos & (count - 1);
				auto head_start = pos / count;

				for (;;) {
					auto first = d_end.next.load(std::memory_order_relaxed);

					// Check if empty or if this is the right bucket
					if (first == end_bucket() || first->head_start != head_start)
						continue;

					// Check if value is valid
					auto idx_bits = (1ull << idx);
					if (!(first->cnt.load(std::memory_order_relaxed) & idx_bits))
						continue;

					// Retrieve/destroy value
					fun(first->ptr()[idx]);
					// Mark as poped
					auto poped = first->pop.fetch_or(idx_bits) | idx_bits;

					if (poped == mask_full)
						// Destroy bucket if all values were removed
						remove_bucket(first);

					return true;
				}
			}

			SEQ_ALWAYS_INLINE void push_internal(uint64_t pos, T&& val) noexcept
			{
				auto idx = pos & (count - 1);
				auto head_start = pos / count;

				for (;;) {
					auto last = d_end.prev.load(std::memory_order_relaxed);
					if (idx != 0) {
						if (head_start != last->head_start)
							continue;
						new (last->ptr() + idx) T(std::move(val));
						last->cnt.fetch_or(1ull << idx, std::memory_order_release);
						return;
					}
					else {
						if (last != end_bucket()) {
							// Ensure that we ARE in the last bucket and that it is full
							if (head_start != last->head_start + 1 || last->cnt.load(std::memory_order_relaxed) != mask_full)
								continue;
						}

						return make_bucket(head_start, std::move(val));
					}
				}
			}

		public:
			QueueImpl(const Allocator& al = {})
			  : Allocator(al)
			{
				// Initialize linked list
				d_end.next.store(end_bucket());
				d_end.prev.store(end_bucket());
			}

			~QueueImpl() noexcept
			{
				// Remove entries
				clear();

				// Remove current bucket
				auto* start = d_end.next.load(std::memory_order_relaxed);
				if (start != end_bucket())
					free_bucket(start);

				// Free pending buckets
				shrink_to_fit();
			}

			Bucket* end_bucket() const noexcept { return (Bucket*)&d_end; }

			const Allocator& get_allocator() const noexcept { return *this; }

			void shrink_to_fit() noexcept
			{
				// Free pending buckets.
				// This function is NOT thread safe.
				while (Bucket* b = pop_bucket())
					deallocate_from(get_allocator(), b);
			}

			void reserve(size_t count)
			{
				// Allocate enough buckets to store up to count elements
				auto s = size();
				if (s >= count)
					return;
				s = count - s;

				std::scoped_lock<lock_type> ll(d_free_lock);
				while (d_free_count < s) {
					Bucket* b = new (allocate_from<Bucket>(get_allocator())) Bucket();
					free_bucket(b);
				}
			}

			void clear() noexcept
			{
				while (!empty())
					pop();
			}

			template<class... Args>
			SEQ_ALWAYS_INLINE void emplace(Args&&... args)
			{
				// Create value upfront as this might throw
				T val{ std::forward<Args>(args)... };

				// Increment head position, and make sure a bucket is availble if needed BEFORE increment
				// to avoid potential bad_alloc exception to corrupt the head state.
				auto pos = add(d_head, [&](auto v) {
					if ((v & (count - 1)) == 0)
						ensure_has_bucket();
					return false;
				});
				push_internal(pos, std::move(val));
			}

			template<class... Args>
			SEQ_ALWAYS_INLINE bool try_emplace(Args&&... args)
			{
				// Create value upfront as this might throw
				T val{ std::forward<Args>(args)... };

				// Increment head position, and make sure a bucket is availble if needed BEFORE increment
				// to avoid potential bad_alloc exception to corrupt the head state.
				auto pos = add(d_head, [&](auto v) {
					if ((v & (count - 1)) == 0)
						return (!d_free.load(std::memory_order_relaxed));
					return false;
				});
				if (pos == invalid)
					return false;
				push_internal(pos, std::move(val));
				return true;
			}

			SEQ_ALWAYS_INLINE bool pop() noexcept
			{
				return pop_internal([](T& v) { destroy(&v); });
			}
			SEQ_ALWAYS_INLINE bool pop(T& val) noexcept
			{
				auto f = [&](T& v) {
					val = std::move(v);
					destroy(&v);
				};
				return pop_internal(f);
			}
			SEQ_ALWAYS_INLINE size_t size() const noexcept
			{
				auto h = d_head.load(std::memory_order_relaxed);
				auto t = d_tail.load(std::memory_order_relaxed);
				return h > t ? (h - t) : 0;
			}
			SEQ_ALWAYS_INLINE bool empty() const noexcept { return size() == 0; }
		};

		// Const iterator fo concurrent_queue (unsafe)
		template<class Queue>
		struct QueueConstIterator
		{
			using value_type = typename Queue::value_type;
			using bucket_type = typename Queue::Bucket;
			using reference = value_type&;
			using const_reference = const value_type&;
			using pointer = value_type*;
			using const_pointer = const value_type*;
			using iterator_category = std::bidirectional_iterator_tag;
			using difference_type = std::ptrdiff_t;
			using size_type = std::uint64_t;
			static constexpr size_type count = Queue::count;

			QueueConstIterator() noexcept = default;
			QueueConstIterator(bucket_type* b) noexcept // end
			  : d_bucket(b)
			{
			}
			QueueConstIterator(bucket_type* b, unsigned p) noexcept
			  : d_bucket(b)
			  , d_pos(p)
			  , d_first(b->first_valid_index())
			  , d_last(b->last_valid_index())
			{
			}

			auto operator++() noexcept -> QueueConstIterator&
			{
				SEQ_ASSERT_DEBUG(d_bucket && !d_bucket->is_end(), "invalid operation on end iterator");
				if (d_pos++ == d_last) {
					d_bucket = d_bucket->next.load(std::memory_order_relaxed);
					if (d_bucket->is_end())
						d_pos = d_first = d_last = 0;
					else {
						d_pos = d_first = d_bucket->first_valid_index();
						d_last = d_bucket->last_valid_index();
					}
				}
				return *this;
			}
			auto operator++(int) noexcept -> QueueConstIterator
			{
				auto ret = *this;
				++(*this);
				return ret;
			}
			auto operator--() noexcept -> QueueConstIterator&
			{
				if (d_pos-- == d_first) {
					d_bucket = d_bucket->prev.load(std::memory_order_relaxed);
					if (d_bucket->is_end())
						d_pos = d_first = d_last = 0;
					else {
						d_first = d_bucket->first_valid_index();
						d_pos = d_last = d_bucket->last_valid_index();
					}
				}
				return *this;
			}
			auto operator--(int) noexcept -> QueueConstIterator
			{
				auto ret = *this;
				--(*this);
				return ret;
			}
			auto operator*() const noexcept -> const_reference
			{
				SEQ_ASSERT_DEBUG(d_bucket && !d_bucket->is_end(), "invalid operation on end iterator");
				SEQ_ASSERT_DEBUG(d_bucket->is_valid(d_pos), "iterator points to an empty location");
				return d_bucket->ptr()[d_pos];
			}
			auto operator->() const noexcept -> const_pointer { return std::pointer_traits<pointer>::pointer_to(**this); }

			bool operator==(const QueueConstIterator& other) const noexcept { return d_bucket == other.d_bucket && d_pos == other.d_pos; }
			bool operator!=(const QueueConstIterator& other) const noexcept { return !operator==(other); }

		private:
			bucket_type* d_bucket = nullptr;
			unsigned d_pos = 0;
			uint16_t d_first = 0;
			uint16_t d_last = 0;
		};

		// Iterator fo concurrent_queue (unsafe)
		template<class Queue>
		struct QueueIterator : public QueueConstIterator<Queue>
		{
			using base_type = QueueConstIterator<Queue>;
			using bucket_type = typename base_type::bucket_type;
			using value_type = typename base_type::value_type;
			using reference = value_type&;
			using const_reference = const value_type&;
			using pointer = value_type*;
			using const_pointer = const value_type*;
			using iterator_category = std::bidirectional_iterator_tag;
			using difference_type = std::ptrdiff_t;
			using size_type = std::uint64_t;

			QueueIterator() noexcept = default;
			QueueIterator(const base_type& other) noexcept
			  : base_type(other)
			{
			}
			QueueIterator(bucket_type* b) noexcept
			  : base_type(b)
			{
			}
			QueueIterator(bucket_type* b, unsigned p) noexcept
			  : base_type(b, p)
			{
			}
			auto operator*() const noexcept -> reference { return const_cast<reference>(base_type::operator*()); }
			auto operator->() const noexcept -> pointer { return std::pointer_traits<pointer>::pointer_to(**this); }
			auto operator++() noexcept -> QueueIterator&
			{
				base_type::operator++();
				return *this;
			}
			auto operator++(int) noexcept -> QueueIterator
			{
				QueueIterator ret = *this;
				base_type::operator++();
				return ret;
			}
			auto operator--() noexcept -> QueueIterator&
			{
				base_type::operator--();
				return *this;
			}
			auto operator--(int) noexcept -> QueueIterator
			{
				QueueIterator ret = *this;
				base_type::operator--();
				return ret;
			}
			bool operator==(const base_type& other) const noexcept { return base_type::operator==(other); }
			bool operator!=(const base_type& other) const noexcept { return base_type::operator!=(other); }
		};

	}

	/// @brief Thread-safe FIFO container.
	/// @tparam T Value type
	/// @tparam Allocator Allocator type
	///
	/// seq::concurrent_queue is a thread-safe unbounded queue (or FIFO)
	/// aiming at better performances than using std::queue + std::mutex.
	/// It is designed for Multi-Producer, Multi-Consumer (MPMC) scenarios.
	///
	/// seq::concurrent_queue is not lock-free nor wait-free, but combines
	/// atomic-based operations with locks to provide a certain level of
	/// concurrency. In all tested scenarios, concurrent_queue is faster
	/// than a regular std::queue with std::mutex, with a gain of up to
	/// a factor 2 in some situations.
	///
	/// In addition, concurrent_queue provides unsafe but usefull features
	/// like iteration.
	///
	template<class T, class Allocator = std::allocator<T>>
	class concurrent_queue : private Allocator
	{
		using lock_type = spinlock;
		using queue_type = detail::QueueImpl<T, Allocator>;

	public:
		static_assert(std::is_nothrow_move_constructible_v<T>);
		static_assert(std::is_nothrow_move_assignable_v<T>);

		using value_type = T;
		using reference = T&;
		using pointer = T*;
		using size_type = uint64_t;
		using allocator_type = Allocator;
		using iterator = detail::QueueIterator<queue_type>;
		using const_iterator = detail::QueueConstIterator<queue_type>;
		using reverse_iterator = std::reverse_iterator<iterator>;
		using const_reverse_iterator = std::reverse_iterator<const_iterator>;

		// Safe API

		/// @brief Default constructor
		concurrent_queue(const Allocator al = {})
		  : Allocator(al)
		{
		}

		/// @brief Construct and reserve enough space to store at least count elements
		concurrent_queue(size_type count, const Allocator al = {})
		  : Allocator(al)
		{
			reserve(count);
		}

		/// @brief Move constructor
		/// It is safe to call this while other is still being used.
		concurrent_queue(concurrent_queue&& other) noexcept(std::is_nothrow_move_constructible_v<Allocator>)
		  : Allocator(std::move(static_cast<Allocator&>(other)))
		{
			std::scoped_lock<lock_type> ll(other.d_data_lock);
			d_data.store(other.d_data.exchange(nullptr));
		}

		/// @brief Move assignment operator.
		/// It is safe to call this operator while both queues are being used.
		concurrent_queue& operator=(concurrent_queue&& other) noexcept
		{
			swap(other);
			return *this;
		}

		/// @brief Destructor
		~concurrent_queue()
		{
			if (auto* d = d_data.load()) {
				d->~queue_type();
				deallocate_from(get_allocator(), d);
			}
		}

		/// @brief Returns the queue allocator
		const allocator_type& get_allocator() const noexcept { return static_cast<const Allocator&>(*this); }

		/// @brief Remove all elements of the queue.
		/// This function might never return if other thread(s) constantly push new values to the queue.
		void clear() noexcept
		{
			auto d = d_data.load(std::memory_order_relaxed);
			if (d)
				d->clear();
		}

		/// @brief Swap 2 queues.
		/// It is safe to call swap while both queues are being used.
		void swap(concurrent_queue& other) noexcept(noexcept(swap_allocator<Allocator>(std::declval<Allocator&>(), std::declval<Allocator&>())))
		{
			std::scoped_lock<lock_type, lock_type> ll(d_data_lock, other.d_data_lock);

			swap_allocator<Allocator>(static_cast<Allocator&>(*this), static_cast<Allocator&>(other));

			auto d = d_data.load();
			d_data.store(other.d_data.load());
			other.d_data.store(d);
		}

		/// @brief Reserve enough space to hold at least count elements.
		void reserve(size_t count) { data()->reserve(count); }

		/// @brief Push an element to the back of the queue.
		/// Strong exception guarantee.
		template<class... Args>
		SEQ_ALWAYS_INLINE void emplace(Args&&... args)
		{
			return data()->emplace(std::forward<Args>(args)...);
		}
		/// @brief Push an element to the back of the queue.
		/// Strong exception guarantee.
		SEQ_ALWAYS_INLINE void push(const T& v) { return emplace(v); }
		/// @brief Push an element to the back of the queue.
		/// Strong exception guarantee.
		SEQ_ALWAYS_INLINE void push(T&& v) { return emplace(std::move(v)); }

		/// @brief Try to retrieve the front element of the queue.
		SEQ_ALWAYS_INLINE bool pop(T& v) noexcept
		{
			auto d = d_data.load(std::memory_order_relaxed);
			return d ? d->pop(v) : false;
		}
		/// @brief Try to discard the front element of the queue.
		SEQ_ALWAYS_INLINE bool pop() noexcept
		{
			auto d = d_data.load(std::memory_order_relaxed);
			return d ? d->pop() : false;
		}

		/// @brief Returns an estimation of the queue size.
		SEQ_ALWAYS_INLINE auto size() const noexcept
		{
			auto d = d_data.load(std::memory_order_relaxed);
			return d ? d->size() : 0;
		}
		/// @brief Returns true if the queue is empty.
		SEQ_ALWAYS_INLINE bool empty() const noexcept { return size() == 0; }

		// Unsafe API

		/// @brief Release unused memory.
		void shrink_to_fit() noexcept
		{
			auto d = d_data.load(std::memory_order_relaxed);
			if (d)
				d->shrink_to_fit();
		}

		auto cbegin() const noexcept -> const_iterator
		{
			auto d = d_data.load(std::memory_order_relaxed);
			if (!d)
				return const_iterator();
			auto bucket = d->end_bucket()->next.load(std::memory_order_relaxed);
			if (bucket == d->end_bucket())
				return end();
			return const_iterator(bucket, bucket->first_valid_index());
		}
		auto begin() noexcept -> iterator { return cbegin(); }
		auto begin() const noexcept -> const_iterator { return cbegin(); }

		auto cend() const noexcept -> const_iterator
		{
			auto d = d_data.load(std::memory_order_relaxed);
			return d ? const_iterator(d->end_bucket()) : const_iterator();
		}
		auto end() noexcept -> iterator { return cend(); }
		auto end() const noexcept -> const_iterator { return cend(); }

		auto rbegin() noexcept { return reverse_iterator(end()); }
		auto crbegin() const noexcept { return const_reverse_iterator(end()); }
		auto rbegin() const noexcept { return const_reverse_iterator(end()); }

		auto rend() noexcept { return reverse_iterator(begin()); }
		auto crend() const noexcept { return const_reverse_iterator(begin()); }
		auto rend() const noexcept { return const_reverse_iterator(begin()); }

	private:
		SEQ_ALWAYS_INLINE queue_type* data() const
		{
			auto d = d_data.load(std::memory_order_relaxed);
			if SEQ_LIKELY (d)
				return d;
			return const_cast<concurrent_queue*>(this)->make_data();
		}

		queue_type* make_data()
		{
			std::scoped_lock<lock_type> ll(d_data_lock);
			auto d = new (allocate_from<queue_type>(get_allocator())) queue_type(get_allocator());
			queue_type* prev = nullptr;
			if (!d_data.compare_exchange_strong(prev, d)) {
				d->~queue_type();
				deallocate_from(get_allocator(), d);
				d = prev;
			}
			return d;
		}

		std::atomic<queue_type*> d_data{ nullptr };
		lock_type d_data_lock;
	};
}

#endif