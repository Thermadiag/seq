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
#include <memory>
#include <type_traits>
#include <atomic>
#include <cstdint>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>

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
					return ((1ull << Count) - 1ull);
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

			struct BaseBucket;
			using atomic_bucket = std::atomic<BaseBucket*>;

			struct BaseBucket
			{
				atomic_bucket prev{ nullptr };	   // Linked list
				atomic_bucket next{ nullptr };	   // Linked list
				atomic_type head_start{ invalid }; // Bucket index
			};

			// Bucket structure, stores up to count elements
			struct Bucket : BaseBucket
			{
				atomic_type cnt{ mask_full };			      // Mask of created (constructed) elements
				atomic_type pop{ 0 };				      // Mask of poped (removed) elements
				RawStorage<T, count> values;	// Elements

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
#ifdef __cpp_lib_hardware_interference_size
			static constexpr size_t cache_line_size = std::hardware_destructive_interference_size;
#else
			static constexpr size_t cache_line_size = 64;
#endif

			alignas(cache_line_size) atomic_type d_head{ 0 };// Head (insert) position
			alignas(cache_line_size) atomic_type d_tail{ 0 };// Tail (pop) position
			alignas(cache_line_size) BaseBucket d_end; // End bucket of linked list

			// Free list of buckets
			atomic_bucket d_free{ nullptr };
			size_type d_free_count = 0;
			spinlock d_free_lock;

			// Lock protecting linked list order
			lock_type d_list_lock;
			// Next bucket index allowed when the linked list is empty (protected by d_list_lock)
			size_type d_empty_next_head_start = 0;

			struct AddReserve
			{
				size_type pos = 0;
				Bucket* reserved = nullptr;
			};

			template<bool Pop, class EarlyStop = int>
			SEQ_ALWAYS_INLINE AddReserve add(atomic_type& a, EarlyStop f = {}) noexcept(Pop)
			{
				// Increment atomic counter using CAS.
				// This is the main bottleneck for concurrent insert/pop,
				// and using CAS + yield is way more effective than fetch_add().

				// An early stop condition can be provided.

				AddReserve ret{ a.load(std::memory_order_relaxed), nullptr };

				uint8_t cnt = 0;
				for (;;) {
					// Early stop condition
					if constexpr (!std::is_same_v<EarlyStop, int>)
						if (f(ret))
							return { invalid, ret.reserved };

					if constexpr (!Pop) {
						// Insertion: test position overflow (should never happen)
						if SEQ_UNLIKELY (ret.pos == std::numeric_limits<uint64_t>::max() - 1) {
							if (ret.reserved)
								free_bucket(ret.reserved);
							throw std::length_error("concurrent_queue reach its maximum ticket position");
						}
					}

					if (a.compare_exchange_strong(ret.pos, ret.pos + 1, std::memory_order_relaxed, std::memory_order_relaxed))
						return ret;

					for (uint8_t i = 0; i < cnt + 1; ++i)
						std::this_thread::yield();
					cnt = ((cnt + 1) & 31);
				}
			}

			Bucket* allocate_bucket()
			{
				Bucket* p = allocate_from<Bucket>(get_allocator());
				try {
					return new (p) Bucket;
				}
				catch (...) {
					deallocate_from(get_allocator(), p);
					throw;
				}
			}

			Bucket* pop_bucket() noexcept
			{
				// Remove and return bucket from the free list.
				std::scoped_lock<spinlock> guard(d_free_lock);
				auto* b = d_free.load(std::memory_order_relaxed);
				if (b) {
					d_free.store(b->next.load(std::memory_order_relaxed));
					--d_free_count;
				}
				return static_cast<Bucket*>(b);
			}

			void free_bucket(BaseBucket* q) noexcept
			{
				// Add bucket to free list.
				q->head_start.store(invalid, std::memory_order_release);
				std::scoped_lock<spinlock> guard(d_free_lock);
				auto fr = d_free.load(std::memory_order_relaxed);
				q->next.store(fr, std::memory_order_relaxed);
				d_free.store(static_cast<Bucket*>(q), std::memory_order_relaxed);
				++d_free_count;
			}

			bool make_bucket(size_type head_start, Bucket* b, T&& val) noexcept
			{
				// Create a bucket of one element using the provided bucket
				SEQ_ASSERT_DEBUG(b != nullptr, "boundary ticket requires reserved bucket");

				std::scoped_lock<lock_type> lock(d_list_lock);

				auto* last = d_end.prev.load(std::memory_order_relaxed);

				if (last == &d_end) {
					if (head_start != d_empty_next_head_start)
						return false;
				}
				else {
					if (head_start != last->head_start.load(std::memory_order_relaxed) + 1 || static_cast<Bucket*>(last)->cnt.load(std::memory_order_acquire) != mask_full)
						return false;
				}

				b->cnt.store(1, std::memory_order_relaxed);
				b->pop.store(0, std::memory_order_relaxed);
				new (b->values.raw_slot(0)) T(std::move(val));
				b->head_start.store(head_start, std::memory_order_release);
				b->next.store(&d_end, std::memory_order_relaxed);
				b->prev.store(last, std::memory_order_relaxed);

				last->next.store(b, std::memory_order_release);
				d_end.prev.store(b, std::memory_order_release);
				return true;
			}

			void remove_bucket(Bucket* first) noexcept
			{
				{
					std::scoped_lock<lock_type> ll(d_list_lock);

					// Remove from list and bucket to the free list
					auto next = first->next.load(std::memory_order_relaxed);
					if (next == &d_end)
						d_empty_next_head_start = first->head_start.load(std::memory_order_relaxed) + 1;
					d_end.next.store(next, std::memory_order_release);
					next->prev.store(&d_end, std::memory_order_release);
				}
				free_bucket(first);
			}

			template<class F>
			SEQ_ALWAYS_INLINE bool pop_internal(F fun) noexcept
			{
				// Get the tail position and increment it.
				// Fail if the tail already reached the head position.
				auto pos = add<true>(d_tail, [&](AddReserve& p) { return p.pos >= d_head.load(std::memory_order_relaxed); }).pos;
				// Empty
				if (pos == invalid)
					return false;

				auto idx = pos & (count - 1);
				auto head_start = pos / count;

				for (;;) {
					auto first = d_end.next.load(std::memory_order_acquire);

					// Check if empty or if this is the right bucket
					if (first == &d_end || first->head_start.load(std::memory_order_acquire) != head_start) {
						std::this_thread::yield();
						continue;
					}

					// Check if value is valid
					auto idx_bits = (1ull << idx);
					auto bucket = static_cast<Bucket*>(first);

					if (!(bucket->cnt.load(std::memory_order_acquire) & idx_bits))
						continue;

					// Retrieve/destroy value
					fun(*bucket->values.live_slot(idx));
					// Mark as poped
					auto popped = bucket->pop.fetch_or(idx_bits, std::memory_order_acq_rel) | idx_bits;

					if (popped == mask_full)
						// Destroy bucket if all values were removed
						remove_bucket(bucket);

					return true;
				}
			}

			SEQ_ALWAYS_INLINE void push_internal(AddReserve r, T&& val) noexcept
			{
				const size_type pos = r.pos;
				const size_type idx = pos & (count - 1);
				const size_type head_start = pos / count;

				// The first position in a bucket is responsible for linking it.
				if (idx == 0) {
					for (;;) {
						if (make_bucket(head_start, r.reserved, std::move(val)))
							return;

						std::this_thread::yield();
					}
				}

				// Wait until the producer responsible for idx == 0 has linked
				// this ticket's bucket.
				for (;;) {
					auto last = d_end.prev.load(std::memory_order_acquire);

					// d_end is not a Bucket and must never be downcast.
					if (last == &d_end) {
						std::this_thread::yield();
						continue;
					}

					if (last->head_start.load(std::memory_order_acquire) != head_start) {
						std::this_thread::yield();
						continue;
					}

					auto bucket = static_cast<Bucket*>(last);
					new (bucket->values.raw_slot(idx)) T(std::move(val));
					bucket->cnt.fetch_or(size_type{ 1 } << idx, std::memory_order_release);

					// Free potentially reserved bucket.
					if (r.reserved)
						free_bucket(r.reserved);

					return;
				}
			}

		public:
			QueueImpl(const Allocator& al = {})
			  : Allocator(al)
			{
				// Initialize linked list
				d_end.next.store(&d_end);
				d_end.prev.store(&d_end);
			}

			~QueueImpl() noexcept
			{
				// Remove entries
				clear();

				// Remove current bucket
				auto* start = d_end.next.load(std::memory_order_relaxed);
				if (start != &d_end)
					free_bucket(start);

				// Free pending buckets
				shrink_to_fit();
			}

			auto end_bucket() const noexcept { return &d_end; }
			auto end_bucket() noexcept { return &d_end; }

			const Allocator& get_allocator() const noexcept { return *this; }

			void shrink_to_fit() noexcept
			{
				// Free pending buckets.
				// This function is NOT thread safe.
				while (Bucket* b = pop_bucket())
					deallocate_from(get_allocator(), b);
			}

			void reserve(size_t new_count)
			{
				// Allocate enough buckets to store up to new_count elements

				for (;;) {
					auto s = size();
					if (s >= new_count)
						return;
					s = new_count - s;
					s = s / count + static_cast<size_type>(s % count != 0);

					{
						std::scoped_lock<spinlock> ll(d_free_lock);
						if (d_free_count >= s)
							return;
					}
					Bucket* q = allocate_bucket();

					std::scoped_lock<spinlock> ll(d_free_lock);
					auto fr = d_free.load(std::memory_order_relaxed);
					q->next.store(fr, std::memory_order_relaxed);
					d_free.store(q, std::memory_order_relaxed);
					++d_free_count;
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
				T val( std::forward<Args>(args)... );

				// Increment head position, and make sure a bucket is availble if needed BEFORE increment
				// to avoid potential bad_alloc exception to corrupt the head state.
				auto pos = add<false>(d_head, [&](AddReserve& v) {
					if ((v.pos & (count - 1)) == 0) {
						if (!v.reserved)
							v.reserved = this->pop_bucket();
						if (!v.reserved)
							v.reserved = allocate_bucket();
					}
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
				auto pos = add<false>(d_head, [&](AddReserve& v) {
					if ((v.pos & (count - 1)) == 0) {
						if (!v.reserved)
							v.reserved = pop_bucket();
						return !v.reserved;
					}
					return false;
				});
				if (pos.pos == invalid) {
					if (pos.reserved)
						free_bucket(pos.reserved);
					return false;
				}

				push_internal(pos, std::move(val));
				return true;
			}

			SEQ_ALWAYS_INLINE bool pop() noexcept
			{
				return pop_internal([](T& v) { destroy_ptr(std::addressof(v)); });
			}
			SEQ_ALWAYS_INLINE bool pop(T& val) noexcept
			{
				auto f = [&](T& v) {
					val = std::move(v);
					destroy_ptr(std::addressof(v));
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
			using base_bucket_type = typename Queue::BaseBucket;
			using reference = const value_type&;
			using const_reference = const value_type&;
			using pointer = const value_type*;
			using const_pointer = const value_type*;
			using iterator_category = std::bidirectional_iterator_tag;
			using difference_type = std::ptrdiff_t;
			using size_type = std::uint64_t;
			static constexpr size_type count = Queue::count;

			QueueConstIterator() noexcept = default;
			QueueConstIterator(base_bucket_type* end) noexcept // end
			  : d_bucket(end)
			  , d_end(end)
			{
			}
			QueueConstIterator(base_bucket_type* end, base_bucket_type* b, unsigned p) noexcept
			  : d_bucket(b)
			  , d_end(end)
			  , d_pos(p == (unsigned)-1 ? bucket(b)->first_valid_index() : p)
			  , d_first(bucket(b)->first_valid_index())
			  , d_last(bucket(b)->last_valid_index())
			{
			}

			bucket_type* bucket(base_bucket_type* b) noexcept { return static_cast<bucket_type*>(b); }
			bucket_type* bucket() noexcept { return bucket(d_bucket); }
			const bucket_type* bucket() const noexcept { return static_cast<const bucket_type*>(d_bucket); }

			auto operator++() noexcept -> QueueConstIterator&
			{
				SEQ_ASSERT_DEBUG(d_bucket && d_bucket != d_end, "invalid operation on end iterator");
				if (d_pos++ == d_last) {
					d_bucket = d_bucket->next.load(std::memory_order_relaxed);
					if (d_bucket == d_end)
						d_pos = d_first = d_last = 0;
					else {
						d_pos = d_first = bucket()->first_valid_index();
						d_last = bucket()->last_valid_index();
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
				SEQ_ASSERT_DEBUG(d_bucket, "invalid iterator");
				if (d_pos-- == d_first) {
					d_bucket = d_bucket->prev.load(std::memory_order_relaxed);
					if (d_bucket == d_end)
						d_pos = d_first = d_last = 0;
					else {
						d_first = bucket()->first_valid_index();
						d_pos = d_last = bucket()->last_valid_index();
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
				SEQ_ASSERT_DEBUG(d_bucket && d_bucket != d_end, "invalid operation on end iterator");
				SEQ_ASSERT_DEBUG(bucket()->is_valid(d_pos), "iterator points to an empty location");
				return *const_cast<QueueConstIterator*>(this)->bucket()->values.live_slot(d_pos);
			}
			auto operator->() const noexcept -> const_pointer { return std::pointer_traits<pointer>::pointer_to(**this); }

			bool operator==(const QueueConstIterator& other) const noexcept { return d_end == other.d_end && d_bucket == other.d_bucket && d_pos == other.d_pos; }
			bool operator!=(const QueueConstIterator& other) const noexcept { return !operator==(other); }

		private:
			base_bucket_type* d_bucket = nullptr;
			base_bucket_type* d_end = nullptr;
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
			using base_bucket_type = typename base_type::base_bucket_type;
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
			QueueIterator(base_bucket_type* end) noexcept
			  : base_type(end)
			{
			}
			QueueIterator(base_bucket_type* end, base_bucket_type* b, unsigned p) noexcept
			  : base_type(end, b, p)
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
	/// Pushing elements, removing elements, reserving or clearing the queue can be conccurently called.
	/// Members size(), empty(), and get_allocator() are safe under the stated restrictions.
	/// Iteration requires exclusive access.
	///
	///
	template<class T, class Allocator = std::allocator<T>>
	class concurrent_queue : private Allocator
	{
		using lock_type = spinlock;
		using queue_type = detail::QueueImpl<T, Allocator>;

	public:
		static_assert(std::is_nothrow_move_constructible_v<T>);
		static_assert(std::is_nothrow_move_assignable_v<T>);
		static_assert(std::is_default_constructible_v<T>);
		static_assert(std::is_nothrow_destructible_v<T>);

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
			concurrent_queue tmp(al);
			tmp.reserve(count);
			d_data.store(tmp.d_data.exchange(nullptr));
		}

		/// @brief Move constructor
		/// This function is NOT thread safe.
		/// Strong exception guarantee if noexcept.
		concurrent_queue(concurrent_queue&& other) noexcept(std::is_nothrow_move_constructible_v<Allocator>)
		  : Allocator(std::move(static_cast<Allocator&>(other)))
		{
			d_data.store(other.d_data.exchange(nullptr));
		}

		/// @brief Extended move constructor.
		/// This function is NOT thread safe.
		/// Weak exception guarantee.
		concurrent_queue(concurrent_queue&& other, const Allocator& alloc)
		  : Allocator(alloc)
		{
			if (alloc == other.get_allocator())
				d_data.store(other.d_data.exchange(nullptr));
			else {
				concurrent_queue tmp(alloc);
				T v;
				while (other.pop(v))
					tmp.emplace(std::move(v));
				d_data.store(tmp.d_data.exchange(nullptr));
			}
		}

		/// @brief Move assignment operator.
		/// This function is NOT thread safe.
		concurrent_queue& operator=(concurrent_queue&& other) noexcept(std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value
										 ? std::is_nothrow_move_assignable_v<Allocator>
										 : std::allocator_traits<Allocator>::is_always_equal::value)
		{
			if (this == std::addressof(other))
				return *this;

			using traits = std::allocator_traits<Allocator>;

			// Clear this queue and deallocate
			if (auto d = d_data.exchange(nullptr)) {
				destroy_ptr(d);
				deallocate_from(get_allocator(), d);
			}

			if constexpr (traits::propagate_on_container_move_assignment::value) {

				// Move allocator, might throw
				static_cast<Allocator&>(*this) = std::move(static_cast<Allocator&>(other));
				d_data.store(other.d_data.exchange(nullptr));
			}
			else {
				if (get_allocator() != other.get_allocator()) {
					// Different allocator: move all elements
					T tmp;
					while (other.pop(tmp))
						emplace(std::move(tmp));
				}
				else {
					// Same allocator: exchange
					d_data.store(other.d_data.exchange(nullptr));
				}
			}
			return *this;
		}

		/// @brief Destructor
		~concurrent_queue() noexcept
		{
			if (auto d = d_data.exchange(nullptr)) {
				destroy_ptr(d);
				deallocate_from(get_allocator(), d);
			}
		}

		/// @brief Returns the queue allocator
		auto get_allocator() const -> allocator_type { return static_cast<const Allocator&>(*this); }

		/// @brief Remove all elements of the queue by continuously calling pop() until empty.
		/// This function might never return if other thread(s) constantly push new values to the queue.
		void clear() noexcept
		{
			if (auto d = d_data.load())
				d->clear();
		}

		/// @brief Reserve enough space to hold at least count elements.
		/// This is a best-effort reservation when called concurrently.
		void reserve(size_t count) { data()->reserve(count); }

		/// @brief Push an element to the back of the queue.
		/// Strong exception guarantee for queue contents.
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
			auto d = d_data.load(std::memory_order_acquire);
			return d ? d->pop(v) : false;
		}
		/// @brief Try to discard the front element of the queue.
		SEQ_ALWAYS_INLINE bool pop() noexcept
		{
			auto d = d_data.load(std::memory_order_acquire);
			return d ? d->pop() : false;
		}

		/// @brief Returns an estimation of the queue size.
		SEQ_ALWAYS_INLINE auto size() const noexcept
		{
			auto d = d_data.load(std::memory_order_acquire);
			return d ? d->size() : 0;
		}
		/// @brief Returns true if the queue is empty.
		SEQ_ALWAYS_INLINE bool empty() const noexcept { return size() == 0; }

		////////////////////////////////////////////////////////////////////////////////////
		// Unsafe API
		////////////////////////////////////////////////////////////////////////////////////

		/// @brief Swap 2 queues.
		/// This function is NOT thread safe.
		void swap(concurrent_queue& other) noexcept(!std::allocator_traits<Allocator>::propagate_on_container_swap::value || std::is_nothrow_swappable_v<Allocator>)
		{
			if (this != std::addressof(other)) {
				using traits = std::allocator_traits<Allocator>;

				if constexpr (!traits::propagate_on_container_swap::value) {
					SEQ_ASSERT_DEBUG(get_allocator() == other.get_allocator(), "swap requires equal non-propagating allocators");
				}
				else {
					using std::swap;
					swap(static_cast<Allocator&>(*this), static_cast<Allocator&>(other));
				}
				auto tmp = d_data.exchange(other.d_data.load());
				other.d_data.store(tmp);
			}
		}

		/// @brief Release unused memory.
		void shrink_to_fit() noexcept
		{
			auto d = d_data.load(std::memory_order_acquire);
			if (d)
				d->shrink_to_fit();
		}

		auto cbegin() const noexcept -> const_iterator
		{
			auto d = d_data.load(std::memory_order_acquire);
			if (!d)
				return const_iterator();
			auto bucket = d->end_bucket()->next.load(std::memory_order_relaxed);
			if (bucket == d->end_bucket())
				return end();
			return const_iterator(d->end_bucket(), bucket, (unsigned)-1);
		}
		auto begin() noexcept -> iterator { return cbegin(); }
		auto begin() const noexcept -> const_iterator { return cbegin(); }

		auto cend() const noexcept -> const_iterator
		{
			auto d = d_data.load(std::memory_order_acquire);
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
			auto d = d_data.load(std::memory_order_acquire);
			if SEQ_LIKELY (d)
				return d;
			return const_cast<concurrent_queue*>(this)->make_data();
		}

		queue_type* make_data()
		{
			queue_type* d = allocate_from<queue_type>(get_allocator());
			try {
				new (d) queue_type(get_allocator());
			}
			catch (...) {
				deallocate_from(get_allocator(), d);
				throw;
			}

			queue_type* prev = nullptr;
			if (!d_data.compare_exchange_strong(prev, d)) {
				destroy_ptr(d);
				deallocate_from(get_allocator(), d);
				d = prev;
			}
			return d;
		}

		std::atomic<queue_type*> d_data{ nullptr };
	};
}

#endif
