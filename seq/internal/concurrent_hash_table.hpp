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

#ifndef SEQ_CONCURRENT_HASH_TABLE_HPP
#define SEQ_CONCURRENT_HASH_TABLE_HPP

#include "simd.hpp"
#include "hash_utils.hpp"
#include "utils.hpp"
#include "../lock.hpp"
#include "../hash.hpp"
#include "../bits.hpp"

#include <execution>
#include <limits>
#include <type_traits>
#include <shared_mutex>
#include <mutex>

namespace seq
{

	/// @brief Predefined concurrency levels for concurrent_set and concurrent_map.
	/// Higher concurrency value usually means lower raw performances on most primitives.
	/// If shared_concurrency is set, the concurrent_map will use a shared_spinlock per bucket instead of a write-only spinlock.
	enum concurrency_level : unsigned
	{
		shared_concurrency = 65536u,
		/// @brief No concurrency: the hash table behaves like any other table and is not thread safe
		no_concurrency = std::numeric_limits<unsigned>::max(),
		/// @brief Very low concurrency: only one shard is used, with one spinlock per bucket
		low_concurrency = 0,
		/// @brief Very low concurrency: only one shard is used, with one read-write spinlock per bucket
		low_concurrency_shared = low_concurrency | shared_concurrency,
		/// @brief Default concurrency level, 32 shards are used, with one spinlock per bucket
		medium_concurrency = 5,
		/// @brief Default concurrency level, 32 shards are used, with one read-write spinlock per bucket
		medium_concurrency_shared = medium_concurrency | shared_concurrency,
		/// @brief High concurrency, 256 shards are used, with one spinlock per bucket
		high_concurrency = 8,
		/// @brief High concurrency, 256 shards are used, with one read-write spinlock per bucket
		high_concurrency_shared = high_concurrency | shared_concurrency
	};

	namespace detail
	{

#define _SEQ_CONCURRENT_MAP_LOAD_FACTOR 0.7f

#if defined(__SSE2__) || defined(__ARM_NEON) || defined(__ARM_NEON__)
		// Node size with and without SSE2

		enum
		{
			max_concurrent_node_size = 16,
			chain_concurrent_node_size = 8
		};
#else
		enum
		{
			max_concurrent_node_size = 8,
			chain_concurrent_node_size = 8
		};
#endif

		// Check if given type is an execution policy
		template<typename ExecutionPolicy>
		constexpr bool internal_is_execution_policy = std::is_execution_policy_v<std::decay_t<ExecutionPolicy>>;

		/// @brief Unlock lock object at scope exit
		template<class Lock, bool Shared = false>
		struct ScopedUnlock
		{
			Lock* _l;
			SEQ_ALWAYS_INLINE ScopedUnlock(Lock* l) noexcept
			  : _l(l)
			{
			}
			SEQ_ALWAYS_INLINE ~ScopedUnlock() noexcept
			{
				if SEQ_LIKELY (_l) {
					if constexpr (Shared)
						_l->unlock_shared();
					else
						_l->unlock();
				}
			}
		};
		template<bool Shared>
		struct ScopedUnlock<null_lock, Shared>
		{
			ScopedUnlock(null_lock*) noexcept {};
		};

		/// @brief RAII atomic boolean locker
		struct BoolUnlocker
		{
			std::atomic<bool>* _l;
			BoolUnlocker(std::atomic<bool>& l) noexcept
			  : _l(&l)
			{
			}
			~BoolUnlocker() noexcept { *_l = false; }
		};

		template<class T, class Allocator>
		T* MakeRange(const Allocator& alloc, size_t count)
		{
			RebindAllocator<Allocator, T> al{ alloc };
			T* vals = al.allocate(count);
			// Construct
			size_t constructed = 0;
			try {
				for (; constructed < count; ++constructed)
					new (vals + constructed) T();
			}
			catch (...) {
				destroy_ptr(vals, constructed);
				al.deallocate(vals, count);
				throw;
			}

			return vals;
		}

		template<class Allocator, class T>
		void DestroyRange(Allocator& alloc, T* ptr, size_t count) noexcept
		{
			destroy_ptr(ptr, count);
			alloc.deallocate(ptr, count);
		}

		/// @brief Random-access array of lock objects
		/// @tparam Lock lock type
		/// @tparam Allocator allocator type
		///
		/// SharedLockArray can only grow, and provide concurrent random access.
		/// Each random access might increase the array size if necessary.
		/// Uses a power of 2 grow strategy.
		///
		template<class Lock, class Allocator = std::allocator<Lock>>
		class SharedLockArray
		{
			using allocator_type = detail::RebindAllocator<Allocator, Lock>;
			using lock_type = Lock;
			using this_type = SharedLockArray<Lock, Allocator>;

			// The extra segment permits representing the complete size_t range.
			static constexpr size_t array_count = std::numeric_limits<size_t>::digits + 1;

			std::atomic<lock_type*> arrays[array_count];
			allocator_type alloc;

			// Segment layout:
			//
			// segment 0: position 0       -- size 1
			// segment 1: position 1       -- size 1
			// segment 2: positions 2..3   -- size 2
			// segment 3: positions 4..7   -- size 4
			// segment 4: positions 8..15  -- size 8
			static SEQ_ALWAYS_INLINE constexpr size_t segment_size(size_t segment) noexcept { return segment == 0 ? size_t{ 1 } : size_t{ 1 } << (segment - 1); }
			static SEQ_ALWAYS_INLINE constexpr size_t segment_start(size_t segment) noexcept { return segment == 0 ? size_t{ 0 } : size_t{ 1 } << (segment - 1); }
			static SEQ_ALWAYS_INLINE size_t segment_from_position(size_t position) noexcept
			{
				if (position == 0)
					return 0;
				return static_cast<size_t>(bit_scan_reverse_64(static_cast<std::uint64_t>(position))) + 1;
			}

			lock_type* make_array(size_t segment)
			{
				lock_type* result = arrays[segment].load(std::memory_order_acquire);

				if (result)
					return result;

				const size_t count = segment_size(segment);
				result = MakeRange<lock_type>(alloc, count);

				lock_type* expected = nullptr;
				if (arrays[segment].compare_exchange_strong(expected, result, std::memory_order_acq_rel, std::memory_order_acquire))
					return result;

				// Another thread published the segment first.
				DestroyRange(alloc, result, count);
				return expected;
			}

		public:
			struct iterator
			{
				this_type* array = nullptr;
				size_t array_index = 0;
				size_t index = 0;

				iterator(this_type* a = nullptr, size_t segment = 0, size_t position = 0) noexcept
				  : array(a)
				  , array_index(segment)
				  , index(position)
				{
				}

				lock_type& operator*() const
				{
					lock_type* values = array->arrays[array_index].load(std::memory_order_acquire);

					if SEQ_UNLIKELY (!values)
						values = array->make_array(array_index);

					return values[index];
				}

				lock_type* operator->() const { return std::pointer_traits<lock_type*>::pointer_to(**this); }

				iterator& operator++() noexcept
				{
					if (++index == segment_size(array_index)) {
						++array_index;
						index = 0;
					}
					return *this;
				}
			};

			SharedLockArray(const Allocator& al = Allocator()) noexcept(std::is_nothrow_constructible_v<allocator_type, const Allocator&>)
			  : alloc(al)
			{
				for (auto& array : arrays)
					array.store(nullptr, std::memory_order_relaxed);
			}

			~SharedLockArray() noexcept
			{
				// Destruction requires exclusive access to the container.
				for (size_t segment = 0; segment < array_count; ++segment) {
					if (lock_type* values = arrays[segment].load(std::memory_order_relaxed))
						DestroyRange(alloc, values, segment_size(segment));
				}
			}

			void ensure_size(size_t count)
			{
				if (count == 0)
					return;

				const size_t last_segment = segment_from_position(count - 1);
				for (size_t segment = 0; segment <= last_segment; ++segment)
					make_array(segment);
			}

			/// @brief Returns element at given position.
			/// Resize array if necessary.
			/// Thread-safe member.
			SEQ_ALWAYS_INLINE lock_type& at(size_t position) const
			{
				const size_t segment = segment_from_position(position);
				const size_t index = position - segment_start(segment);
				lock_type* values = arrays[segment].load(std::memory_order_acquire);

				if SEQ_UNLIKELY (!values)
					values = const_cast<SharedLockArray*>(this)->make_array(segment);

				return values[index];
			}
			SEQ_ALWAYS_INLINE lock_type& at_existing(size_t position) const
			{
				const size_t segment = segment_from_position(position);
				const size_t index = position - segment_start(segment);
				lock_type* values = arrays[segment].load(std::memory_order_relaxed);
				SEQ_ASSERT_DEBUG(values != nullptr, "lock segment was not allocated before mask publication");
				return values[index];
			}
		};

		/// @brief Apply functor and return boolean value (true if void() functor, functor result otherwise)
		template<class F, class... Args>
		static SEQ_ALWAYS_INLINE bool ApplyFunctor(F&& f, Args&&... args) noexcept(noexcept(f(std::forward<Args>(args)...)))
		{
			if constexpr (std::is_convertible_v<std::invoke_result_t<F, Args...>, bool>)
				return f(std::forward<Args>(args)...);
			else {
				f(std::forward<Args>(args)...);
				return true;
			}
		}

		/// @brief Simulate atomic load on non atomic value
		template<class T>
		static SEQ_ALWAYS_INLINE T AtomicLoad(T v, std::memory_order = std::memory_order_relaxed) noexcept
		{
			return v;
		}
		/// @brief Acquire atomic load
		template<class T>
		static SEQ_ALWAYS_INLINE T AtomicLoad(const std::atomic<T>& v, std::memory_order o = std::memory_order_relaxed) noexcept
		{
			return v.load(o);
		}

		template<class T>
		static SEQ_ALWAYS_INLINE void AtomicStore(T& v, T val, std::memory_order = std::memory_order_relaxed) noexcept
		{
			v = val;
		}
		template<class T>
		static SEQ_ALWAYS_INLINE void AtomicStore(std::atomic<T>& v, T val, std::memory_order o = std::memory_order_relaxed) noexcept
		{
			v.store(val, o);
		}

		/// @brief Movemask function of 8 bytes word
		static SEQ_ALWAYS_INLINE auto MoveMask8(std::uint64_t word) noexcept -> std::uint64_t
		{
			std::uint64_t tmp = (word & 0x7F7F7F7F7F7F7F7FULL) + 0x7F7F7F7F7F7F7F7FULL;
			return ~(tmp | word | 0x7F7F7F7F7F7F7F7FULL);
		}
		/// @brief Movemask function of 4 bytes word
		static SEQ_ALWAYS_INLINE auto MoveMask4(std::uint32_t word) noexcept -> std::uint32_t
		{
			std::uint32_t tmp = (word & 0x7F7F7F7FU) + 0x7F7F7F7FUL;
			return ~(tmp | word | 0x7F7F7F7FU);
		}

		/// @brief Returns slot index with a tiny hash value of 0
		template<unsigned Size>
		static SEQ_ALWAYS_INLINE auto FindIndexZero(const std::uint8_t* hashs) noexcept -> unsigned
		{
			if constexpr (Size == 4) {
				std::uint32_t found = MoveMask4(read_32(hashs) ^ 0u) >> 8u;
				if (found)
					return bit_scan_forward_32(found) >> 3u;
				return static_cast<unsigned>(-1);
			}
			else if constexpr (Size == 8) {
				std::uint64_t found = MoveMask8(read_64(hashs) ^ 0ull) >> 8u;
				if (found)
					return bit_scan_forward_64(found) >> 3u;
				return static_cast<unsigned>(-1);
			}
			else if constexpr (Size == 16) {
#if defined(__SSE2__) || defined(__ARM_NEON) || defined(__ARM_NEON__)
				auto hs = _mm_load_si128(reinterpret_cast<const __m128i*>(hashs));
				int mask = _mm_movemask_epi8(_mm_cmpeq_epi8(hs, _mm_setzero_si128())) >> 1;
				if (mask)
					return bit_scan_forward_32(static_cast<unsigned>(mask));
#endif
				return static_cast<unsigned>(-1);
			}
			SEQ_UNREACHABLE();
		}

		/// @brief Dense node of chain_concurrent_node_size hashs and values
		template<class T>
		struct alignas(chain_concurrent_node_size == 16 ? 16 : std::max(alignof(std::max_align_t), alignof(T))) ConcurrentDenseNode
		{
			using value_type = T;
			static constexpr unsigned size = chain_concurrent_node_size;
			struct alignas(T) Value
			{
				char data[sizeof(T)];
			};

			ConcurrentDenseNode* right = nullptr;
			std::uintptr_t inspected = 0; // Helper value for erase_if()
			std::uint8_t hashs[size];
			Value vals[size - 1];

			ConcurrentDenseNode() noexcept { memset(hashs, 0, sizeof(hashs)); }

			SEQ_ALWAYS_INLINE auto count() const noexcept -> unsigned { return hashs[0]; }
			SEQ_ALWAYS_INLINE bool full() const noexcept { return hashs[0] == size - 1; }
			SEQ_ALWAYS_INLINE auto values() noexcept -> T* { return reinterpret_cast<T*>(vals); }
			SEQ_ALWAYS_INLINE auto values() const noexcept -> const T* { return reinterpret_cast<const T*>(vals); }
		};

		/// @brief Value node of max_concurrent_node_size values
		template<class T>
		struct ConcurrentValueNode
		{
			using value_type = T;
			struct alignas(T) Value
			{
				char data[sizeof(T)];
			};
			ConcurrentDenseNode<T>* right;
			Value vals[max_concurrent_node_size - 1];
			ConcurrentValueNode() noexcept
			  : right(nullptr)
			{
			}
			SEQ_ALWAYS_INLINE auto values() noexcept -> T* { return reinterpret_cast<T*>(vals); }
			SEQ_ALWAYS_INLINE auto values() const noexcept -> const T* { return reinterpret_cast<const T*>(vals); }
		};

		/// @brief Hash node of max_concurrent_node_size tiny hashes
		struct alignas(max_concurrent_node_size) ConcurrentHashNode
		{
			static constexpr unsigned size = max_concurrent_node_size;
			static constexpr unsigned shift = (size == 32 ? 5 : (size == 16 ? 4 : 3));
			std::uint8_t hashs[max_concurrent_node_size];

			ConcurrentHashNode() noexcept { memset(hashs, 0, sizeof(hashs)); }

			SEQ_ALWAYS_INLINE bool full() const noexcept { return hashs[0] == size - 1; }
			SEQ_ALWAYS_INLINE auto count() const noexcept -> unsigned { return hashs[0]; }
			/// @brief Compute tiny hash representation from full hash value
			static SEQ_ALWAYS_INLINE auto tiny_hash(size_t hash) noexcept -> std::uint8_t
			{
				std::uint8_t res = static_cast<std::uint8_t>(hash >> (sizeof(hash) * 8u - 8u));
				return res == 0 ? 1 : res;
			}

			template<class T, class F>
			void for_each(const ConcurrentValueNode<T>* n, F&& f) const noexcept(noexcept(f(std::declval<std::uint8_t*>(), 0, std::declval<T&>())))
			{
				for (unsigned i = 0; i < count(); ++i)
					f(hashs, i, n->values()[i]);
				if (full() && n->right) {
					const ConcurrentDenseNode<T>* d = n->right;
					do {
						for (unsigned i = 0; i < d->count(); ++i)
							f(d->hashs, i, d->values()[i]);
					} while ((d = d->right));
				}
			}
			template<class T, class F>
			bool for_each_until(const ConcurrentValueNode<T>* n, F&& f) const
			  noexcept(noexcept(ApplyFunctor(std::declval<F&>(), std::declval<const std::uint8_t*>(), 0u, std::declval<const T&>())))
			{
				for (unsigned i = 0; i < count(); ++i)
					if (!ApplyFunctor(f, hashs, i, n->values()[i]))
						return false;
				if (full() && n->right) {
					const ConcurrentDenseNode<T>* d = n->right;
					do {
						for (unsigned i = 0; i < d->count(); ++i)
							if (!ApplyFunctor(f, d->hashs, i, d->values()[i]))
								return false;
					} while ((d = d->right));
				}
				return true;
			}
			template<class T, class F>
			void for_each(ConcurrentValueNode<T>* n, F&& f) noexcept(noexcept(f(std::declval<std::uint8_t*>(), 0, std::declval<T&>())))
			{
				static_cast<const ConcurrentHashNode*>(this)->for_each(
				  n, [&f](const std::uint8_t* hs, unsigned i, const T& v) { f(const_cast<std::uint8_t*>(hs), i, const_cast<T&>(v)); });
			}
			template<class T, class F>
			bool for_each_until(ConcurrentValueNode<T>* n, F&& f) noexcept(noexcept(f(std::declval<std::uint8_t*>(), 0, std::declval<T&>())))
			{
				return static_cast<const ConcurrentHashNode*>(this)->for_each_until(
				  n, [&f](const std::uint8_t* hs, unsigned i, const T& v) { return f(const_cast<std::uint8_t*>(hs), i, const_cast<T&>(v)); });
			}
		};

		/// @brief Standard insert policy
		struct InsertConcurrentPolicy
		{
			template<class T, class K, class... Args>
			static SEQ_ALWAYS_INLINE T* emplace(T* p, K&& key, Args&&... args) noexcept(noexcept(construct_ptr(p, std::forward<K>(key), std::forward<Args>(args)...)))
			{
				construct_ptr(p, std::forward<K>(key), std::forward<Args>(args)...);
				return p;
			}
		};
		/// @brief Try insert policy
		struct TryInsertConcurrentPolicy
		{
			template<class T, class K, class... Args>
			static SEQ_ALWAYS_INLINE T* emplace(T* p, K&& key, Args&&... args) noexcept(
			  noexcept(construct_ptr(p, std::piecewise_construct, std::forward_as_tuple(std::forward<K>(key)), std::forward_as_tuple(std::forward<Args>(args)...))))
			{
				construct_ptr(p, std::piecewise_construct, std::forward_as_tuple(std::forward<K>(key)), std::forward_as_tuple(std::forward<Args>(args)...));
				return p;
			}
		};

		template<unsigned Count = 4>
		class AtomicSize_t
		{
			static_assert(Count != 0 && (Count & (Count - 1)) == 0, "Count must be a power of two");

			static SEQ_ALWAYS_INLINE unsigned thread_id() noexcept
			{
				static std::atomic<unsigned> ids{ 0 };
				thread_local unsigned id = ids.fetch_add(1);
				return id & (Count - 1);
			}
			struct alignas(64) CounterStripe
			{
				std::atomic<int64_t> value{ 0 };
				SEQ_ALWAYS_INLINE auto load(std::memory_order o = std::memory_order_relaxed) const noexcept { return value.load(o); }
				SEQ_ALWAYS_INLINE void store(int64_t v, std::memory_order o = std::memory_order_relaxed) noexcept { value.store(v, o); }
				SEQ_ALWAYS_INLINE void fetch_add(int64_t v, std::memory_order o = std::memory_order_relaxed) noexcept { value.fetch_add(v, o); }
				SEQ_ALWAYS_INLINE void fetch_sub(int64_t v, std::memory_order o = std::memory_order_relaxed) noexcept { value.fetch_sub(v, o); }
			};
			CounterStripe d_vals[Count];

		public:
			AtomicSize_t() noexcept
			{
				// for (auto& value : d_vals)
				//	std::atomic_init(&value, int64_t{ 0 });
			}
			AtomicSize_t(size_t v) noexcept
			  : AtomicSize_t()
			{
				d_vals[thread_id()].store((int64_t)v);
			}
			AtomicSize_t(const AtomicSize_t&) = delete;
			AtomicSize_t& operator=(const AtomicSize_t&) = delete;

			SEQ_ALWAYS_INLINE void incr(size_t id, int64_t v) noexcept { d_vals[id & (Count - 1)].fetch_add(v, std::memory_order_relaxed); }
			SEQ_ALWAYS_INLINE void decr(size_t id, int64_t v) noexcept { d_vals[id & (Count - 1)].fetch_sub(v, std::memory_order_relaxed); }

			SEQ_ALWAYS_INLINE void incr(int64_t v) noexcept { d_vals[thread_id()].fetch_add(v, std::memory_order_relaxed); }
			SEQ_ALWAYS_INLINE void decr(int64_t v) noexcept { d_vals[thread_id()].fetch_sub(v, std::memory_order_relaxed); }

			SEQ_ALWAYS_INLINE AtomicSize_t& operator++() noexcept
			{
				incr(1);
				return *this;
			}
			SEQ_ALWAYS_INLINE AtomicSize_t& operator--() noexcept
			{
				decr(1);
				return *this;
			}
			SEQ_ALWAYS_INLINE AtomicSize_t& operator+=(size_t v) noexcept
			{
				incr((int64_t)v);
				return *this;
			}
			SEQ_ALWAYS_INLINE AtomicSize_t& operator-=(size_t v) noexcept
			{
				decr((int64_t)v);
				return *this;
			}
			size_t load(std::memory_order o = std::memory_order_relaxed) const noexcept
			{
				auto v = d_vals[0].load(o);
				for (unsigned i = 1; i < Count; ++i)
					v += d_vals[i].load(o);
				return v < 0 ? 0 : (size_t)v;
			}
			operator size_t() const noexcept { return load(); }
			AtomicSize_t& operator=(size_t v) noexcept
			{
				for (unsigned i = 0; i < Count; ++i)
					d_vals[i].store(0);
				d_vals[thread_id()].store((int64_t)v);
				return *this;
			}
		};

		template<unsigned C>
		static SEQ_ALWAYS_INLINE auto AtomicLoad(const AtomicSize_t<C>& v, std::memory_order o = std::memory_order_relaxed)
		{
			return v.load(o);
		}

		/// @brief Concurrent swiss table using chaining instead of standard quadratic probing.
		/// This table could be used alone or combined with sharding.
		///
		/// An extra array of RW locks is used to provide fine grained locking.
		/// This array if fully thread safe and can only grow. It is also used to
		/// prevent a node usage during rehash.
		///
		template<class PrivateData, class Key, class Value = Key, class NodeLock = shared_spinner<std::uint8_t>>
		class ChainingHashTable
		{
		public:
			using data_type = PrivateData;
			using Allocator = typename PrivateData::allocator_type;
			using extract_key = ExtractKey<Key, Value>;
			using node_type = ConcurrentHashNode;
			using value_node_type = ConcurrentValueNode<Value>;
			using chain_node_type = ConcurrentDenseNode<Value>;
			using node_lock = NodeLock;
			using value_type = typename extract_key::value_type;
			using this_type = ChainingHashTable<PrivateData, Key, Value, NodeLock>;
			using lock_array = SharedLockArray<node_lock, Allocator>;

			static constexpr bool is_concurrent = !std::is_same_v<NodeLock, null_lock>;
			using size_type = std::conditional_t<is_concurrent, std::atomic<size_t>, size_t>;
			using atomic_integer = std::conditional_t<is_concurrent, AtomicSize_t<>, size_t>;
			using chain_count_type = std::conditional_t<is_concurrent, std::atomic<unsigned>, unsigned>;
			using lock_array_type = std::conditional_t<is_concurrent, std::atomic<lock_array*>, lock_array*>;
			using bucket_type = std::conditional_t<is_concurrent, std::atomic<node_type*>, node_type*>;
			using rehash_lock_type = std::conditional_t<is_concurrent, std::shared_mutex, null_lock>;

			// Maximum hash mask, depends on the SharedLockArray max size.
			// We can insert more elements than this value, but using chaining.
			static constexpr size_t max_hash_mask = is_concurrent ? ((1u << 31u) - 1u) : std::numeric_limits<size_t>::max();

			struct Entry
			{
				uint8_t* hashs;
				Value* values;
				unsigned pos;
				bool operator!=(const Entry& r) const noexcept { return values != r.values || pos != r.pos; }
			};

		private:
			node_type* d_static_node = get_static_node(); // Static (invalid) node, faster access than calling get_static_node() every time

			bucket_type d_buckets{ get_static_node() }; // hash table
			value_node_type* d_values = nullptr;	    // values
			lock_array_type d_locks{ nullptr };	    // lock array
			size_type d_hash_mask{ 0 };		    // hash mask
			PrivateData* d_data = nullptr;		    // pointer to upper PrivateData

			// Write-heavy counters on another cache line
			atomic_integer d_size{ 0 }; // total size
			size_type d_total_chains{ 0 }; // total number of dense nodes
			size_type d_chain_count{ 0 }; // total number of buckets having at least one dense node
			size_type d_next_target{ 0 }; // next size before rehash

			rehash_lock_type d_rehash_mutex;

			// Remaining 1 byte elements
			node_lock d_rehash_lock;		 // unique lock for rehash
			std::atomic<bool> d_in_rehash{ false };	 // tells if we are in rehash
			std::atomic<bool> d_need_rehash{ true }; // tells if we need to rehash
			
			// Store all allocators to avoid potential throwing in destructor
			RebindAllocator<Allocator, node_type> d_node_al;
			RebindAllocator<Allocator, value_node_type> d_value_al;
			RebindAllocator<Allocator, chain_node_type> d_chain_al;
			RebindAllocator<Allocator, lock_array> d_lock_al;

			/// @brief Find given value based on its small hash representation and hashs/values arrays
			template<unsigned Size, class Equal, class K, class Node>
			static SEQ_ALWAYS_INLINE auto FindWithTh(std::uint8_t th, const Equal& eq, K&& key, const std::uint8_t* hashs, const Node* node) noexcept(
			  noexcept(eq(extract_key::key(*node->values()), std::forward<K>(key)))) -> const Value*
			{
				if constexpr (Size == 4) {
					if (!hashs[0])
						return nullptr;
					// no SSE variant (way slower)
					std::uint32_t _th;
					memset(&_th, th, sizeof(_th));

					// do first 3 values
					std::uint32_t found = MoveMask4(read_32(hashs) ^ _th) >> 8u;
					if (found) {
						SEQ_PREFETCH(node->values());
						do {
							unsigned pos = bit_scan_forward_32(found) >> 3u;
							if (eq(extract_key::key(node->values()[pos]), std::forward<K>(key)))
								return node->values() + pos;
							reinterpret_cast<unsigned char*>(&found)[pos] = 0;
						} while (found);
					}
					return nullptr;
				}
				else if constexpr (Size == 8) {
					if (!hashs[0])
						return nullptr;
					// no SSE variant (way slower)
					std::uint64_t _th;
					memset(&_th, th, sizeof(_th));

					// do first 7 values
					std::uint64_t found = MoveMask8(read_64(hashs) ^ _th) >> 8u;
					if (found) {
						SEQ_PREFETCH(node->values());
						do {
							unsigned pos = bit_scan_forward_64(found) >> 3u;
							if (eq(extract_key::key(node->values()[pos]), std::forward<K>(key)))
								return node->values() + pos;
							reinterpret_cast<unsigned char*>(&found)[pos] = 0;
						} while (found);
					}
					return nullptr;
				}
				else if constexpr (Size == 16) {
#if defined(__SSE2__) || defined(__ARM_NEON) || defined(__ARM_NEON__)
					// SSE movemask
					if (!hashs[0])
						return nullptr;
					auto hs = _mm_load_si128(reinterpret_cast<const __m128i*>(hashs));
					auto mask = (unsigned)_mm_movemask_epi8(_mm_cmpeq_epi8(hs, _mm_set1_epi8(static_cast<char>(th)))) >> 1;
					if (mask) {
						SEQ_PREFETCH(node->values());
						do {
							unsigned pos = bit_scan_forward_32(mask);
							if (eq(extract_key::key(node->values()[pos]), std::forward<K>(key)))
								return node->values() + pos;
							mask &= mask - 1u;
						} while (mask);
					}
#endif
					return nullptr;
				}
				SEQ_UNREACHABLE();
			}

			/// @brief Returns index of the next null hash value
			/// Only used when an exception is thrown during rehash
			auto FindFreeSlotInNode(ConcurrentHashNode* node, ConcurrentValueNode<Value>* values) noexcept -> std::pair<Value*, std::uint8_t*>
			{
				// Look for a free slot in the node chain
				unsigned idx = FindIndexZero<max_concurrent_node_size>(node->hashs);
				if (idx != static_cast<unsigned>(-1))
					return { values->values() + idx, node->hashs + idx + 1 };
				auto* d = values->right;
				while (d) {
					idx = FindIndexZero<chain_concurrent_node_size>(d->hashs);
					if (idx != static_cast<unsigned>(-1))
						return { d->values() + idx, d->hashs + idx + 1 };
					d = d->right;
				}
				return { nullptr, nullptr };
			}

			/// @brief Insert value in a new dense node
			template<class Policy, class Node, class K, class... Args>
			auto InsertNewDense(std::uint8_t th, Node* n, K&& key, Args&&... args) -> std::pair<Value*, bool>
			{
				chain_node_type* d = MakeRange<chain_node_type>(d_chain_al, 1);

				try {
					Policy::emplace(d->values(), std::forward<K>(key), std::forward<Args>(args)...);
				}
				catch (...) {
					// destroy dense node
					DestroyRange(d_chain_al, d, 1);
					throw;
				}

				d_total_chains += 1;
				update_need_rehash();

				d->hashs[++d->hashs[0]] = th;
				n->right = d;
				return { d->values(), true };
			}

			/// @brief Insert value in dense node if it does not already exist
			template<class Policy, bool CheckExists, class Equal, class K, class... Args>
			auto FindInsertDense(std::uint8_t th, const Equal& eq, ConcurrentDenseNode<Value>* n, K&& key, Args&&... args) -> std::pair<Value*, bool>
			{
				auto valid = n;
				do {
					if constexpr (CheckExists) {
						auto v = FindWithTh<chain_concurrent_node_size>(th, eq, extract_key::key(std::forward<K>(key)), n->hashs, n);
						if (v)
							return { const_cast<Value*>(v), false };
					}
					valid = n;
				} while ((n = n->right));
				if SEQ_UNLIKELY (valid->full())
					return InsertNewDense<Policy>(th, valid, std::forward<K>(key), std::forward<Args>(args)...);

				// might throw, fine
				auto p = Policy::emplace(valid->values() + valid->count(), std::forward<K>(key), std::forward<Args>(args)...);
				valid->hashs[++valid->hashs[0]] = th;
				return { p, true };
			}

			/// @brief Insert value if it does not already exist
			template<class Policy, bool CheckExists, class Equal, class K, class... Args>
			SEQ_ALWAYS_INLINE auto FindInsertNode(std::uint8_t th, const Equal& eq, ConcurrentHashNode* node, ConcurrentValueNode<Value>* values, K&& key, Args&&... args)
			  -> std::pair<Value*, bool>
			{
				if constexpr (CheckExists) {
					auto v = FindWithTh<max_concurrent_node_size>(th, eq, extract_key::key(std::forward<K>(key)), node->hashs, values);
					if (v)
						return { const_cast<Value*>(v), false };
				}
				if SEQ_UNLIKELY (node->full()) {
					if (values->right)
						return FindInsertDense<Policy, CheckExists>(th, eq, values->right, std::forward<K>(key), std::forward<Args>(args)...);
					auto r = InsertNewDense<Policy>(th, values, std::forward<K>(key), std::forward<Args>(args)...);
					d_chain_count += 1;
					update_need_rehash();
					return r;
				}
				// might throw, fine
				auto p = Policy::emplace(values->values() + node->count(), std::forward<K>(key), std::forward<Args>(args)...);
				node->hashs[++node->hashs[0]] = th;
				return { p, true };
			}

			template<bool ValOnly, class Dense, class K>
			auto find_key_in_dense(Dense* dense, uint8_t th, size_t bucket_pos, const K& key) const -> std::conditional_t<ValOnly, Value*, Entry>
			{
				// Find in right dense nodes
				do {
					auto found = FindWithTh<chain_concurrent_node_size>(th, d_data->key_eq(), key, dense->hashs, dense);
					if (found) {
						if constexpr (ValOnly)
							return const_cast<Value*>(found);
						else
							return Entry{ dense->hashs, dense->values(), (unsigned)(found - dense->values()) };
					}
					dense = dense->right;
				} while (dense);

				if constexpr (ValOnly)
					return nullptr;
				else
					return Entry{ nullptr, nullptr, 0 };
			}

			template<bool ValOnly, class K>
			SEQ_ALWAYS_INLINE auto find_key(node_type* buckets, size_t hash, size_t bucket_pos, const K& key) const -> std::conditional_t<ValOnly, Value*, Entry>
			{
				if (buckets == d_static_node) {
					if constexpr (ValOnly)
						return nullptr;
					else
						return Entry{ nullptr, nullptr, 0 };
				}

				auto th = node_type::tiny_hash(hash);
				auto values = d_values + bucket_pos;
				auto bucket = buckets + bucket_pos;
				// Find in main bucket
				if (auto found = FindWithTh<max_concurrent_node_size>(th, d_data->key_eq(), key, bucket->hashs, values)) {
					if constexpr (ValOnly)
						return const_cast<Value*>(found);
					else
						return Entry{ bucket->hashs, values->values(), (unsigned)(found - values->values()) };
				}
				if (bucket->full() && values->right)
					return find_key_in_dense<ValOnly>(values->right, th, bucket_pos, key);
				if constexpr (ValOnly)
					return nullptr;
				else
					return Entry{ nullptr, nullptr, 0 };
			}

			auto right_most(node_type* buckets, size_t bucket_pos) noexcept -> Entry
			{
				auto values = d_values + bucket_pos;
				auto bucket = buckets + bucket_pos;
				auto dense = values->right;

				if (!dense)
					return { bucket->hashs, values->values(), bucket->count() - 1 };

				while (dense->right)
					dense = dense->right;
				return { dense->hashs, dense->values(), dense->count() - 1 };
			}

			SEQ_ALWAYS_INLINE auto get_allocator() const { return d_data->get_allocator(); }
			SEQ_ALWAYS_INLINE decltype(auto) key_eq() const { return d_data->key_eq(); }

			static SEQ_ALWAYS_INLINE auto get_static_node() noexcept -> node_type*
			{
				static node_type node;
				return &node;
			}

			void thread_yield() const noexcept { std::this_thread::yield(); }

			auto make_nodes(size_t count = 1) -> node_type* { return MakeRange<node_type>(d_node_al, count); }
			auto make_value_nodes(size_t count = 1) -> value_node_type* { return MakeRange<value_node_type>(d_value_al, count); }
			
			template<class Node>
			void free_nodes(Node* n, size_t count = 1) noexcept
			{
				if constexpr (std::is_same_v<node_type, Node>)
					DestroyRange(d_node_al, n, count);
				else if constexpr (std::is_same_v<value_node_type, Node>)
					DestroyRange(d_value_al, n, count);
				if constexpr (std::is_same_v<chain_node_type, Node>) {
					d_total_chains -= 1;
					DestroyRange(d_chain_al, n, count);
				}
			}

			void move_back(node_type* buckets, value_node_type* values, size_t new_hash_mask, node_type* old_buckets, value_node_type* old_values, size_t old_hash_mask) noexcept(
			  std::is_nothrow_move_constructible_v<Value> && noexcept(std::declval<PrivateData&>().hash_key(extract_key::key(std::declval<Value&>()))))
			{
				// In case of exception (bad_alloc only) during rehash, move back from new buckets to previous buckets
				for (size_t i = 0; i < new_hash_mask + 1; ++i) {
					buckets[i].for_each(values + i, [&](std::uint8_t* hashs, unsigned j, Value& v) {
						size_t h = hash_key(extract_key::key(v));
						size_t idx = h & old_hash_mask;
						auto loc = FindFreeSlotInNode(old_buckets + idx, old_values + idx);
						SEQ_ASSERT_DEBUG(loc.first, "");
						construct_ptr(loc.first, std::move(v));
						*loc.second = hashs[j + 1];
					});
				}
			}

			SEQ_ALWAYS_INLINE bool check_need_rehash() const noexcept
			{
				auto buckets = AtomicLoad(d_buckets, std::memory_order_acquire);
				auto next_target = AtomicLoad(d_next_target, std::memory_order_relaxed);
				if SEQ_UNLIKELY (buckets == d_static_node || AtomicLoad(d_chain_count, std::memory_order_relaxed) >= next_target ||
						 (AtomicLoad(d_total_chains, std::memory_order_relaxed) >> 4) >= next_target) {
					return true;
				}
				return false;
			}

			SEQ_ALWAYS_INLINE void update_need_rehash() noexcept
			{
				if (check_need_rehash())
					d_need_rehash.store(true, std::memory_order_relaxed);
			}

			SEQ_ALWAYS_INLINE void lock(node_lock& lock)
			{
				while (!lock.try_lock()) {
					if (d_in_rehash.load(std::memory_order_relaxed))
						std::shared_lock<rehash_lock_type> ll(d_rehash_mutex);
					else 
						thread_yield();
				}
			}
			SEQ_ALWAYS_INLINE void lock_shared(node_lock& lock) const
			{
				while (!lock.try_lock_shared()) {
					if (d_in_rehash.load(std::memory_order_relaxed)) {
						this_type* _this = const_cast<this_type*>(this);
						std::shared_lock<rehash_lock_type> ll(_this->d_rehash_mutex);
					}
					else 
						thread_yield();
				}
			}

			bool rehash_internal(size_t new_hash_mask, bool grow_only = false)
			{

				// Rehash the table for given mask value.
				// Do not check for potential duplicate values.

				// Avoid 2 parallel rehashs
				bool prev = false;
				if (!d_in_rehash.compare_exchange_strong(prev, true))
					return false;

				// Make sure d_in_rehash will be set to false
				BoolUnlocker bl(d_in_rehash);
				// Lock
				std::scoped_lock<node_lock> ll(d_rehash_lock);
				std::scoped_lock<rehash_lock_type> ll2(d_rehash_mutex);

				// Make sure we are growing
				auto hash_mask = AtomicLoad(d_hash_mask, std::memory_order_acquire);
				if (grow_only && new_hash_mask <= hash_mask && hash_mask != 0)
					return false;

				lock_array* locks = AtomicLoad(d_locks, std::memory_order_acquire);
				lock_array* new_locks = nullptr;
				node_type* buckets = nullptr;
				value_node_type* values = nullptr;
				size_t locked_count = 0;
				size_t i = 0;
				const auto& eq = key_eq();

				try {

					// Allocator new buckets
					buckets = make_nodes(new_hash_mask + 1u);
					values = make_value_nodes(new_hash_mask + 1u);

					// Create locks if needed
					if (is_concurrent && !locks) {
						// might throw, fine
						new_locks = d_lock_al.allocate(1);
						try {
							construct_ptr(new_locks, get_allocator());
						}
						catch (...) {
							d_lock_al.deallocate(new_locks, 1);
							throw;
						}
					}

					if constexpr (is_concurrent) {
						// Ensure all lock objects are allocated to avoid potential std::bad_alloc in clear_no_lock().
						lock_array* resulting_locks = locks ? locks : new_locks;
						resulting_locks->ensure_size(new_hash_mask + 1);
					}

					size_t count = (d_buckets != d_static_node) ? hash_mask + 1u : 0u;
					auto iter = typename lock_array::iterator(locks);
					auto old_buckets = AtomicLoad(d_buckets);

					for (i = 0; i < count; ++i) {

						// Lock position
						if (is_concurrent && locks) {
							iter->lock();
							locked_count++;
						}

						old_buckets[i].for_each(d_values + i, [&](std::uint8_t* hashs, unsigned j, Value& val) {
							auto pos = hash_key(extract_key::key(val)) & new_hash_mask;
							FindInsertNode<InsertConcurrentPolicy, false>(hashs[j + 1], eq, buckets + pos, values + pos, std::move_if_noexcept(val));

							if constexpr (std::is_nothrow_move_constructible_v<Value>) {
								destroy_ptr(&val);
								// mark position as destroyed
								hashs[j + 1] = 0;
							}
						});

						if (is_concurrent && locks)
							++iter;
					}
				}
				catch (...) {

					bool should_clear = false;
					if constexpr (std::is_nothrow_move_constructible_v<Value>) {
						try {
							// Try to recover the table previous state by moving back values from
							// the 'new' storage.
							if (buckets && values)
								move_back(buckets, values, new_hash_mask, AtomicLoad(d_buckets), d_values, hash_mask);
						}
						catch (...) {
							// We can recover from some exceptions like std::bad_alloc.
							// However, an exception in move_back() cannot be recovered properly.
							// To ensure table invariants, clear the table.
							should_clear = true;
						}
					}
					else if constexpr (!std::is_copy_constructible_v<Value>) {
						// A failed move may have changed a key in the old table.
						should_clear = true;
					}

					// Destroy and deallocate
					destroy_buckets(buckets, values, new_hash_mask + 1);

					if (is_concurrent && locks) {
						// Unlock locked nodes
						for (size_t j = 0; j != locked_count; ++j)
							locks->at_existing(j).unlock();
					}

					if (new_locks) {
						destroy_ptr(new_locks);
						d_lock_al.deallocate(new_locks, 1);
					}

					if (should_clear)
						// All previously acquired bucket locks have been released.
						// d_rehash_lock remains held, so clear_no_lock() is safe here.
						clear_no_lock();

					throw;
				}

				// Save old bucket
				node_type* old_buckets = AtomicLoad(d_buckets);
				value_node_type* old_values = d_values;
				size_t old_hash_mask = hash_mask;

				// Affect new buckets
				d_next_target =
				  std::max((new_hash_mask + 1) / 16,
					   size_t{ 1 }); // static_cast<size_t>(static_cast<double>((new_hash_mask + 1) * node_type::size) * static_cast<double>(d_data->max_load_factor()));
				d_values = values;
				AtomicStore(d_buckets, buckets, std::memory_order_release);
				AtomicStore(d_hash_mask, new_hash_mask, std::memory_order_release);
				d_need_rehash.store(false);

				if (is_concurrent && locks) {
					// Unlock all nodes
					auto iter = typename lock_array::iterator(locks);
					for (i = 0; i < locked_count; ++i, ++iter)
						iter->unlock();
				}

				// Destroy previous buckets
				destroy_buckets(old_buckets, old_values, old_hash_mask + 1, !std::is_nothrow_move_constructible_v<Value>);

				// Affect created locks
				if (new_locks)
					d_locks = new_locks;

				// d_rehash_condition.notify_all();
				return true;
			}

			auto destroy_bucket(node_type* n, value_node_type* v, bool destroy_values = true) noexcept -> size_t
			{
				size_t ret = n->count();

				// Destroy one bucket and destroy/deallocate right dense nodes
				if (!std::is_trivially_destructible_v<Value> && destroy_values) {
					for (unsigned j = 0; j < n->count(); ++j)
						destroy_ptr(v->values() + j);
				}
				if (n->full() && v->right) {
					ConcurrentDenseNode<Value>* d = v->right;
					do {
						ret += d->count();
						if (destroy_values)
							destroy_ptr(d->values(), d->count());
						ConcurrentDenseNode<Value>* right = d->right;
						free_nodes(d);
						d = right;
					} while (d);
					--d_chain_count;
				}

				v->right = nullptr;
				memset(n->hashs, 0, sizeof(n->hashs));

				return ret;
			}

			void destroy_buckets(node_type* buckets, value_node_type* values, size_t count, bool destroy_values = true) noexcept
			{
				// Deallocate nodes and destroy values
				if (!buckets || buckets == d_static_node)
					return;

				if (!values) {
					free_nodes(buckets, count);
					return;
				}

				for (size_t i = 0; i < count; ++i)
					destroy_bucket(buckets + i, values + i, destroy_values);

				free_nodes(buckets, count);
				free_nodes(values, count);
			}

			void rehash(size_t size)
			{
				bool rehash_performed = false;

				while (!rehash_performed) {

					// Rehash for given size
					if (size == 0) {
						rehash_performed = rehash_internal(0, false);
						continue;
					}
					size_t new_hash_mask = (size)-1ULL;
					if ((size & (size - 1ULL)) != 0ULL) // non power of 2
						new_hash_mask = (1ULL << (1ULL + (bit_scan_reverse_64(size)))) - 1ULL;
					new_hash_mask >>= node_type::shift;
					if (new_hash_mask > max_hash_mask)
						new_hash_mask = max_hash_mask;
					if (new_hash_mask != d_hash_mask)
						rehash_performed = rehash_internal(static_cast<size_t>(new_hash_mask), false);
					else
						break;

					if (!rehash_performed) {
						// Wait for rehash to finish
						std::shared_lock<rehash_lock_type> guard(d_rehash_mutex);
					}
				}
			}
			void rehash_on_next_target()
			{
				auto hash_mask = AtomicLoad(d_hash_mask, std::memory_order_acquire);
				if (hash_mask < max_hash_mask && (!is_concurrent || !d_in_rehash.load(std::memory_order_relaxed)))
					rehash_internal(AtomicLoad(d_buckets) == d_static_node ? 0u : static_cast<size_t>((hash_mask + 1ull) * 2ull - 1ull), true);
			}

			SEQ_ALWAYS_INLINE void rehash_on_insert()
			{
				SEQ_ASSERT_DEBUG(AtomicLoad(d_buckets) != d_static_node || AtomicLoad(d_next_target, std::memory_order_acquire) == 0, "static bucket requires zero rehash target");

				/* auto buckets = AtomicLoad(d_buckets, std::memory_order_acquire);
				auto next_target = AtomicLoad(d_next_target, std::memory_order_relaxed);
				if SEQ_UNLIKELY (buckets == d_static_node || AtomicLoad(d_chain_count, std::memory_order_relaxed) >= next_target ||
						 (AtomicLoad(d_total_chains, std::memory_order_relaxed) >> 4) >= next_target) {
					rehash_on_next_target();
				}*/
				if SEQ_UNLIKELY (d_need_rehash.load(std::memory_order_relaxed))
					rehash_on_next_target();
			}

			SEQ_NOINLINE(auto) update_lock(lock_array* locks, size_t hash, size_t& hash_mask, size_t& pos, node_lock*& l)
			{
				thread_yield();
				hash_mask = AtomicLoad(d_hash_mask, std::memory_order_acquire);
				if (((hash & hash_mask) != pos)) {
					pos = (hash & hash_mask);
					l->unlock();
					l = &locks->at_existing(pos);
					this->lock(*l);
				}
			}
			SEQ_NOINLINE(auto) update_lock_shared(lock_array* locks, size_t hash, size_t& hash_mask, size_t& pos, node_lock*& l) const
			{
				thread_yield();
				hash_mask = AtomicLoad(d_hash_mask, std::memory_order_acquire);
				if (((hash & hash_mask) != pos)) {
					pos = (hash & hash_mask);
					l->unlock_shared();
					l = &locks->at_existing(pos);
					this->lock_shared(*l);
				}
			}

			template<bool WaitForBucket = true>
			SEQ_NOINLINE(auto)
			get_node_global_lock(lock_array* locks, size_t hash, node_lock*& l) -> std::pair<node_type*, size_t>
			{
				if constexpr (WaitForBucket)
					while (AtomicLoad(d_buckets) == d_static_node)
						thread_yield();
				l = &d_rehash_lock;
				l->lock();
				if ((locks = AtomicLoad(d_locks, std::memory_order_acquire))) {
					l->unlock();
					return this->template get_node<false>(locks, hash, l);
				}
				return { AtomicLoad(d_buckets), hash & AtomicLoad(d_hash_mask, std::memory_order_acquire) };
			}
			template<bool WaitForBucket = true>
			SEQ_ALWAYS_INLINE auto get_node(lock_array* locks, size_t hash, node_lock*& l) -> std::pair<node_type*, size_t>
			{
				if SEQ_UNLIKELY (!locks)
					return this->template get_node_global_lock<WaitForBucket>(locks, hash, l);
				// Returns node locked for given hash value
				size_t hash_mask = AtomicLoad(d_hash_mask, std::memory_order_acquire);
				size_t pos = hash & hash_mask;
				l = &locks->at_existing(pos);
				this->lock(*l);
				auto buckets = AtomicLoad(d_buckets, std::memory_order_acquire);
				while ((WaitForBucket && buckets == d_static_node) || hash_mask != AtomicLoad(d_hash_mask, std::memory_order_acquire)) {
					update_lock(locks, hash, hash_mask, pos, l);
					buckets = AtomicLoad(d_buckets, std::memory_order_acquire);
				}
				return { buckets, pos };
			}

			SEQ_NOINLINE(auto) get_node_shared_global_lock(lock_array* locks, size_t hash, node_lock*& l) const -> std::pair<node_type*, size_t>
			{
				l = const_cast<node_lock*>(&d_rehash_lock);
				l->lock_shared();
				if ((locks = AtomicLoad(d_locks, std::memory_order_acquire))) {
					l->unlock_shared();
					return get_node_shared(locks, hash, l);
				}
				return { AtomicLoad(d_buckets), hash & AtomicLoad(d_hash_mask, std::memory_order_acquire) };
			}
			SEQ_ALWAYS_INLINE auto get_node_shared(lock_array* locks, size_t hash, node_lock*& l) const -> std::pair<node_type*, size_t>
			{
				if SEQ_UNLIKELY (!locks)
					return get_node_shared_global_lock(locks, hash, l);
				// Returns node locked for given hash value
				size_t hash_mask = AtomicLoad(d_hash_mask, std::memory_order_acquire);
				size_t pos = hash & hash_mask;
				l = &locks->at_existing(pos);
				this->lock_shared(*l);
				while (hash_mask != AtomicLoad(d_hash_mask, std::memory_order_acquire))
					update_lock_shared(locks, hash, hash_mask, pos, l);
				return { AtomicLoad(d_buckets, std::memory_order_acquire), pos };
			}

			/// @brief Insert new value based on provided policy
			/// Only insert if value does not already exist.
			/// Call provided function if value already exist.
			template<class Policy, bool CheckExists, class F, class K, class... Args>
			SEQ_ALWAYS_INLINE auto insert_policy(size_t hash, F&& fun, K&& key, Args&&... args) -> bool
			{
				// Compute tiny hash and get (locked) node
				auto th = node_type::tiny_hash(hash);
				node_lock* ll = nullptr;
				std::pair<node_type*, size_t> pos;
				if constexpr (is_concurrent)
					pos = this->template get_node<true>(AtomicLoad(d_locks, std::memory_order_acquire), hash, ll);
				else
					pos = { AtomicLoad(d_buckets), hash & AtomicLoad(d_hash_mask, std::memory_order_acquire) };

				// Grab lock
				ScopedUnlock<node_lock> lock(ll);

				auto p = FindInsertNode<Policy, CheckExists>(th, key_eq(), pos.first + pos.second, d_values + pos.second, std::forward<K>(key), std::forward<Args>(args)...);
				if (!p.second) {
					// Key exist: call functor
					fun(*p.first);
					return false;
				}

				if constexpr (is_concurrent)
					d_size.incr(hash, 1);
				else
					++d_size;
				return true;
			}

			SEQ_ALWAYS_INLINE bool contains_value(const Value& key_value) const
			{
				// Returns true if given value (key AND value) exists in this table
				size_t hash = hash_key(extract_key::key(key_value));
				bool ret = false;
				visit_hash(hash, extract_key::key(key_value), [&](const auto& v) { ret = extract_key::has_value ? (extract_key::value(v) == extract_key::value(key_value)) : true; });
				return ret;
			}
			SEQ_ALWAYS_INLINE bool contains(const Value& key_value) const
			{
				// Returns true if given value (key AND value) exists in this table
				size_t hash = hash_key(extract_key::key(key_value));
				return visit_hash(hash, extract_key::key(key_value), [&](const auto&) {});
			}

		public:
			/// @brief Constructor
			explicit ChainingHashTable(PrivateData* data) noexcept
			  : d_data(data)
			  , d_node_al(data->get_allocator())
			  , d_value_al(data->get_allocator())
			  , d_chain_al(data->get_allocator())
			  , d_lock_al(data->get_allocator())
			{
			}

			/// @brief Destructor
			~ChainingHashTable() noexcept
			{
				auto buckets = AtomicLoad(d_buckets);
				if (buckets != d_static_node)
					destroy_buckets(buckets, d_values, AtomicLoad(d_hash_mask) + 1);

				if (auto* locks = AtomicLoad(d_locks, std::memory_order_acquire))
					DestroyRange(d_lock_al, locks, 1);
			}

			/// @brief Returns hash table size
			SEQ_ALWAYS_INLINE auto size() const noexcept -> size_t { return AtomicLoad(d_size, std::memory_order_acquire); }

			/// @brief Hash key using provided hasher
			/// Apply hash mixin if hasher does not provide the is_avalanching typedef
			template<class H, class K>
			static SEQ_ALWAYS_INLINE auto hash_key(const H& hasher, const K& key) -> size_t
			{
				return hash_value(hasher, (extract_key::key(key)));
			}
			/// @brief Hash key
			template<class K>
			SEQ_ALWAYS_INLINE auto hash_key(const K& key) const -> size_t
			{
				return hash_key(d_data->hash_function(), key);
			}
			/// @brief Returns the maximum load factor
			auto max_load_factor() const noexcept -> float { return _SEQ_CONCURRENT_MAP_LOAD_FACTOR; }

			/// @brief Returns current load factor
			auto load_factor() const noexcept -> float
			{
				// Returns the current load factor
				auto s = size();
				return s == 0 ? 0.f : static_cast<float>(s) / static_cast<float>(capacity());
			}
			auto capacity() const noexcept
			{
				size_t bucket_count = AtomicLoad(d_buckets) != d_static_node ? d_hash_mask + 1u : 0u;
				return bucket_count * node_type::size;
			}
			/// @brief Reserve enough space in the hash table
			void reserve(size_t size)
			{
				auto buckets = AtomicLoad(d_buckets, std::memory_order_acquire);
				size_t current_capacity = buckets == d_static_node ? size_t{ 0 } : static_cast<size_t>( (AtomicLoad(d_hash_mask) + 1) * node_type::size * _SEQ_CONCURRENT_MAP_LOAD_FACTOR);

				if (size > current_capacity)
					rehash(static_cast<size_t>(static_cast<double>(size) / (double)_SEQ_CONCURRENT_MAP_LOAD_FACTOR));
			}
			/// @brief Rehash table for given number of buckets
			void rehash_table(size_t n)
			{
				if (n == 0)
					rehash(1);
				else
					rehash(n);
			}
			/// @brief Performs a key lookup, and call given functor on the table entry if found.
			/// Returns 1 if the key is found, 0 otherwise.
			template<class K, class F>
			SEQ_ALWAYS_INLINE size_t visit_hash(size_t hash, const K& key, F&& f) const
			{
				node_lock* lock = nullptr;
				std::pair<node_type*, size_t> pos;
				if constexpr (is_concurrent)
					pos = get_node_shared(AtomicLoad(d_locks, std::memory_order_acquire), hash, lock);
				else
					pos = { AtomicLoad(d_buckets), (hash & d_hash_mask) };

				ScopedUnlock<node_lock, true> ll(lock);
				if (auto entry = find_key<true>(pos.first, hash, pos.second, key)) {
					(f)(static_cast<const Value&>(*entry));
					return 1;
				}
				return 0;
			}
			template<class K, class F>
			SEQ_ALWAYS_INLINE size_t visit_hash(size_t hash, const K& key, F&& f)
			{
				node_lock* lock = nullptr;
				std::pair<node_type*, size_t> pos;
				if constexpr (is_concurrent)
					pos = this->template get_node<false>(AtomicLoad(d_locks, std::memory_order_acquire), hash, lock);
				else
					pos = { AtomicLoad(d_buckets), (hash & d_hash_mask) };

				ScopedUnlock<node_lock> ll(lock);
				if (auto entry = find_key<true>(pos.first, hash, pos.second, key)) {
					(f)(const_cast<Value&>(*entry));
					return 1;
				}
				return 0;
			}

			/// @brief Visit all entries and call functor for each of them.
			/// If the functor returns a boolean value evaluated to false, stop visiting and return false.
			/// Otherwise return true.
			template<class F>
			bool visit_all(F&& fun) const
			{
				// Avoid rehash while calling visit_all
				std::shared_lock<rehash_lock_type> lock(const_cast<rehash_lock_type&>(d_rehash_mutex));
				if (AtomicLoad(d_buckets) == d_static_node)
					return true;

				size_t count = d_hash_mask + 1u;
				lock_array* locks = AtomicLoad(d_locks, std::memory_order_acquire);
				auto iter = typename lock_array::iterator(locks);
				for (size_t i = 0; i < count; ++i) {
					std::shared_lock<node_lock> ll;
					if (locks)
						ll = std::shared_lock<node_lock>(*iter);
					const node_type* n = AtomicLoad(d_buckets) + i;
					const value_node_type* v = d_values + i;

					if (!n->for_each_until(v, [&](auto, auto, const Value& val) { return (fun)(val); }))
						return false;
					if (locks)
						++iter;
				}
				return true;
			}
			template<class F>
			bool visit_all(F&& fun)
			{
				// Avoid rehash while calling visit_all
				std::shared_lock<rehash_lock_type> lock(d_rehash_mutex);
				if (AtomicLoad(d_buckets) == d_static_node)
					return true;

				size_t count = d_hash_mask + 1u;
				lock_array* locks = AtomicLoad(d_locks, std::memory_order_acquire);
				auto iter = typename lock_array::iterator(locks);
				for (size_t i = 0; i < count; ++i) {
					std::unique_lock<node_lock> ll;
					if (locks)
						ll = std::unique_lock<node_lock>{ *iter };
					node_type* n = AtomicLoad(d_buckets) + i;
					value_node_type* v = d_values + i;

					if (!n->for_each_until(v, [&](auto, auto, Value& val) { return fun(val); }))
						return false;
					if (locks)
						++iter;
				}
				return true;
			}

			/// @brief Insert entry based on provided policy
			template<class Policy, class K, class... Args>
			SEQ_ALWAYS_INLINE auto emplace_policy(size_t hash, K&& key, Args&&... args) -> bool
			{
				rehash_on_insert();
				return insert_policy<Policy, true>(hash, [](const auto&) {}, std::forward<K>(key), std::forward<Args>(args)...);
			}
			/// @brief Insert entry based on provided policy without checking for duplicates
			template<class Policy, class K, class... Args>
			SEQ_ALWAYS_INLINE auto emplace_policy_no_check(size_t hash, K&& key, Args&&... args) -> bool
			{
				rehash_on_insert();
				return insert_policy<Policy, false>(hash, [](const auto&) {}, std::forward<K>(key), std::forward<Args>(args)...);
			}
			template<class Policy, class F, class K, class... Args>
			SEQ_ALWAYS_INLINE auto emplace_policy_visit(size_t hash, F&& fun, K&& key, Args&&... args) -> bool
			{
				rehash_on_insert();
				return insert_policy<Policy, true>(hash, std::forward<F>(fun), std::forward<K>(key), std::forward<Args>(args)...);
			}

			bool erase_entry(node_type* buckets, size_t bucket_pos, const Entry& entry)
			{
				// Erase given entry.
				// Returns true if the full node was deallocated, false otherwise.

				auto last = right_most(buckets, bucket_pos);

				// Exchange value with right most value, this might throw
				if (entry != last) {
					try {
						entry.values[entry.pos] = std::move(last.values[last.pos]);
					}
					catch (...) {
						// Erase full bucket to preserve invariant
						d_size -= destroy_bucket(buckets + bucket_pos, d_values + bucket_pos);
						throw;
					}
				}

				// Destroy
				destroy_ptr(last.values + last.pos);

				// Exchange hash
				entry.hashs[entry.pos + 1] = last.hashs[last.pos + 1];
				last.hashs[last.pos + 1] = 0;
				// Decrement last node size and full table size
				--d_size;
				if (--last.hashs[0] == 0) {
					// Bucket (main or dense) is empty
					// Main bucket: nothing to do
					if (buckets[bucket_pos].hashs != last.hashs) {
						// Dense bucket: find the previous one
						if (d_values[bucket_pos].right->hashs == last.hashs) {
							// This is the dense bucket right after the main one
							free_nodes(d_values[bucket_pos].right);
							d_values[bucket_pos].right = nullptr;
							d_chain_count -= 1;
							return true;
						}
						else {
							auto dense = d_values[bucket_pos].right;
							while (dense->right && dense->right->hashs != last.hashs)
								dense = dense->right;
							SEQ_ASSERT_DEBUG(dense->right, "corrupted hash table");
							free_nodes(dense->right);
							dense->right = nullptr;
							return true;
						}
					}
				}
				return false;
			}

			/// @brief Erase key if found AND is fun(value) returns true.
			/// Returns number of erased entries (1 or 0).
			template<class F, class K>
			SEQ_ALWAYS_INLINE auto erase_key(size_t hash, F&& fun, const K& key) -> size_t
			{
				node_lock* lock = nullptr;
				std::pair<node_type*, size_t> pos;
				if constexpr (is_concurrent)
					pos = this->template get_node<false>(AtomicLoad(d_locks, std::memory_order_acquire), hash, lock);
				else
					pos = { AtomicLoad(d_buckets), (hash & d_hash_mask) };

				ScopedUnlock<node_lock> ll(lock);

				if SEQ_UNLIKELY (pos.first == d_static_node)
					return 0;

				auto entry = find_key<false>(pos.first, hash, pos.second, key);
				if (!entry.hashs)
					return 0;

				if (!fun(const_cast<Value&>(entry.values[entry.pos])))
					return 0;

				erase_entry(pos.first, pos.second, entry);
				return 1;
			}

			auto next_non_inspected(size_t pos) noexcept -> chain_node_type*
			{
				auto dense = d_values[pos].right;
				while (dense) {
					if (!dense->inspected)
						return dense;
					dense = dense->right;
				}
				return nullptr;
			}

			/// @brief Erase all entries for which given functor returns true
			template<class F>
			size_t erase_if(F&& fun)
			{
				struct ResetInspectFlag
				{
					// Reset the inspected to 0 for all chained nodes
					this_type* table;
					size_t pos;
					~ResetInspectFlag() noexcept
					{
						auto dense = table->d_values[pos].right;
						while (dense) {
							dense->inspected = 0;
							dense = dense->right;
						}
					}
				};

				// Avoid rehash while calling erase_if
				std::scoped_lock<rehash_lock_type> lock(d_rehash_mutex);
				if (AtomicLoad(d_buckets) == d_static_node)
					return 0;

				lock_array* locks = AtomicLoad(d_locks, std::memory_order_acquire);
				size_t count = d_hash_mask + 1u;
				size_t res = 0;
				auto iter = typename lock_array::iterator(locks);

				node_type* buckets = AtomicLoad(d_buckets);

				for (size_t i = 0; i < count; ++i) {

					// Lock bucket
					std::unique_lock<node_lock> ll;
					if (locks)
						ll = std::unique_lock<node_lock>(*iter);

					ResetInspectFlag guard{ this, i };

					node_type* n = buckets + i;
					value_node_type* vals = d_values + i;

					// Erase from main
					for (unsigned j = 0; j < n->count();) {
						if (fun(vals->values()[j])) {
							erase_entry(buckets, i, { n->hashs, vals->values(), j });
							++res;
						}
						else
							++j;
					}

					// Erase from right dense nodes
					while (auto dense = next_non_inspected(i)) {

						dense->inspected = 1;

						for (unsigned j = 0; j < dense->count();) {
							if (fun(dense->values()[j])) {
								if (erase_entry(buckets, i, { dense->hashs, dense->values(), j })) {
									++res;
									break;
								}
								++res;
							}
							else
								++j;
						}
					}

					// Increment lock
					if (locks)
						++iter;
				}

				return res;
			}

			void clear()
			{
				// Cannot clear while rehashing
				std::scoped_lock<rehash_lock_type> lock(d_rehash_mutex);
				clear_no_lock();
			}
			/// @brief Clear the hash table
			void clear_no_lock() noexcept
			{
				if (AtomicLoad(d_buckets) == d_static_node)
					return;

				lock_array* locks = AtomicLoad(d_locks, std::memory_order_acquire);
				size_t count = d_hash_mask + 1;

				if (is_concurrent && locks) {
					// lock all buckets
					auto iter = typename lock_array::iterator(locks);
					for (size_t i = 0; i < count; ++i, ++iter)
						(*iter).lock();
				}

				auto buckets = AtomicLoad(d_buckets);

				for (size_t i = 0; i < count; ++i) {
					destroy_bucket(buckets + i, d_values + i, true);
				}
				d_size = 0;
				d_chain_count = 0;
				d_total_chains = 0;
				d_need_rehash.store(false);

				if (is_concurrent && locks) {
					// unlock all buckets
					auto iter = typename lock_array::iterator(locks);
					for (size_t i = 0; i < count; ++i, ++iter)
						(*iter).unlock();
				}
			}
		};

		/// @brief Base concurrent hash table class, used by both concurrent_set and concurrent_map.
		///
		/// Holds N sub-tables (of type ChainingHashTable) based on _Shards template parameter.
		///
		template<class Key, class Value, class PresentedValue, class Hash, class KeyEqual, class Allocator, unsigned _Shards>
		class ConcurrentHashTable
		  : public HashEqual<Hash, KeyEqual>
		  , private Allocator
		{
			static_assert(((_Shards & ~shared_concurrency) <= 10 || _Shards == no_concurrency), "Concurrency factor too high! (limited to 11)");

			// We need the hash function to be noexcept for roolback in rehash in case of exception
			// static_assert(noexcept(std::declval<const Hash&>()(std::declval<const Key&>())));

			static constexpr bool is_concurrent = (_Shards != no_concurrency);
			static constexpr unsigned shards = is_concurrent ? (_Shards & ~shared_concurrency) : 0;

			// Forward declare PrivateData;
			class PrivateData;

			using possible_lock_type = typename std::conditional<static_cast<bool>(_Shards& shared_concurrency), shared_spinner<std::uint8_t>, spinlock>::type;
			using load_factor_type = typename std::conditional<is_concurrent, std::atomic<float>, float>::type;
			using node_lock_type = typename std::conditional<is_concurrent, possible_lock_type, null_lock>::type;
			template<class U>
			using rebind_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<U>;
			using hash_map = detail::ChainingHashTable<PrivateData, Key, Value, node_lock_type>;
			using base_hash_equal = HashEqual<Hash, KeyEqual>;
			using data_type = typename std::conditional<is_concurrent, std::atomic<PrivateData*>, PrivateData*>::type;
			using data_lock_type = typename std::conditional<is_concurrent, spinlock, null_lock>::type;

			/// @brief Internal data
			class PrivateData
			  : public base_hash_equal
			  , private Allocator
			{
				struct DummyPrivate
				{
					using allocator_type = Allocator;
				};
				using dummy_map = detail::ChainingHashTable<DummyPrivate, Key, Value, node_lock_type>;
				struct alignas(dummy_map) MapHolder
				{
					std::uint8_t data[sizeof(dummy_map)];
					hash_map* as_map() noexcept { return reinterpret_cast<hash_map*>(data); }
					const hash_map* as_map() const noexcept { return reinterpret_cast<const hash_map*>(data); }
				};

			public:
				using allocator_type = Allocator;
				static constexpr unsigned map_count = 1u << shards;

				MapHolder maps[map_count];

				PrivateData(const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual(), const Allocator& alloc = Allocator())
				  : base_hash_equal(hash, equal)
				  , Allocator(alloc)
				{
					// Allocate/construct all sub-tables
					unsigned i = 0;
					try {
						for (; i < map_count; ++i)
							construct_ptr(&at(i), this);
					}
					catch (...) {
						for (unsigned j = 0; j < i; ++j)
							destroy_ptr(&at(j));
						throw;
					}
				}
				~PrivateData()
				{
					// Destroy all sub-tables
					for (unsigned i = 0; i < map_count; ++i)
						destroy_ptr(&at(i));
				}

				SEQ_ALWAYS_INLINE auto get_allocator() const noexcept -> const Allocator& { return *this; }
				SEQ_ALWAYS_INLINE auto get_allocator() noexcept -> Allocator& { return *this; }

				SEQ_ALWAYS_INLINE auto max_load_factor() const noexcept -> float { return _SEQ_CONCURRENT_MAP_LOAD_FACTOR; }

				template<class K>
				SEQ_ALWAYS_INLINE auto hash_key(const K& key) const noexcept(noexcept(hash_map::hash_key(std::declval<const Hash&>(), std::declval<const K&>()))) -> size_t
				{
					return hash_map::hash_key(this->hash_function(), key);
				}

				SEQ_ALWAYS_INLINE auto at(unsigned pos) noexcept -> hash_map& { return reinterpret_cast<hash_map&>(maps[pos]); }
				SEQ_ALWAYS_INLINE auto at(unsigned pos) const noexcept -> const hash_map& { return reinterpret_cast<const hash_map&>(maps[pos]); }

				/// @brief Make a PrivateData object
				static auto make(const Allocator& alloc, const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual()) -> PrivateData*
				{
					rebind_alloc<PrivateData> al = alloc;
					PrivateData* d = al.allocate(1);
					try {
						construct_ptr(d, hash, equal, alloc);
					}
					catch (...) {
						al.deallocate(d, 1);
						throw;
					}
					return d;
				}

				/// @brief Destroy and deallocate a PrivateData object
				static void destroy(PrivateData* d)
				{
					if (d) {
						rebind_alloc<PrivateData> al{ d->get_allocator() };
						destroy_ptr(d);
						al.deallocate(d, 1);
					}
				}
			};

			using extract_key = typename hash_map::extract_key;

			data_type d_data{ nullptr };

			// Lock type used to prevent concurrent access
			// for operations involving 2 or more tables.
			// This include equal_to() and mere().
			node_lock_type d_cross_lock;

			PrivateData* make_data()
			{
				PrivateData* d = AtomicLoad(d_data, std::memory_order_acquire);
				if (d)
					return d;

				if constexpr (is_concurrent) {
					d = PrivateData::make(this->get_allocator(), this->hash_function(), this->key_eq());
					PrivateData* expected = nullptr;
					if (!d_data.compare_exchange_strong(expected, d)) {
						PrivateData::destroy(d);
						return expected;
					}
				}
				else
					d_data = d = PrivateData::make(this->get_allocator(), this->hash_function(), this->key_eq());
				return d;
			}

			SEQ_ALWAYS_INLINE PrivateData* get_data()
			{
				PrivateData* d = AtomicLoad(d_data, std::memory_order_acquire);
				if SEQ_UNLIKELY (!d) {
					d = make_data();
				}
				return d;
			}

			SEQ_ALWAYS_INLINE const PrivateData* get_data() const noexcept { return AtomicLoad(d_data, std::memory_order_acquire); }
			SEQ_ALWAYS_INLINE const PrivateData* cget_data() const noexcept { return AtomicLoad(d_data, std::memory_order_acquire); }

			/// @brief Given a hash value, returns the corresponding sub-table index
			SEQ_ALWAYS_INLINE auto index_from_hash(size_t hash) const noexcept -> unsigned
			{
				if constexpr (shards == 0)
					return 0;
				else {
#ifdef SEQ_ARCH_64
					return static_cast<unsigned>(((hash >> 47)) & ((1u << shards) - 1u));
#else
					return ((hash >> 24) ^ (hash >> 26) ^ (hash >> 8)) & ((1u << shards) - 1u);
#endif
				}
			}
			template<class Policy, class K, class... Args>
			SEQ_ALWAYS_INLINE auto emplace_policy_no_check(K&& key, Args&&... args) -> bool
			{
				PrivateData* d = get_data();
				size_t hash = d->hash_key(std::forward<K>(key));
				return d->at(index_from_hash(hash)).template emplace_policy_no_check<Policy>(hash, std::forward<K>(key), std::forward<Args>(args)...);
			}

			void destroy_data() noexcept
			{
				PrivateData::destroy(AtomicLoad(d_data, std::memory_order_acquire));
				d_data = (nullptr);
			}

		public:
			ConcurrentHashTable(const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual(), const Allocator& alloc = Allocator())
			  : base_hash_equal(hash, equal)
			  , Allocator(alloc)
			  , d_data(nullptr)
			{
			}
			ConcurrentHashTable(const ConcurrentHashTable& other, const Allocator& alloc)
			  : base_hash_equal(other)
			  , Allocator(alloc)
			  , d_data(PrivateData::make(alloc, other.hash_function(), other.key_eq()))
			{
				try {
					this->reserve(other.size());
					other.visit_all([&](const auto& v) { this->emplace_policy_no_check<InsertConcurrentPolicy>(v); });
				}
				catch (...) {
					destroy_data();
					throw;
				}
			}
			ConcurrentHashTable(const ConcurrentHashTable& other)
			  : ConcurrentHashTable(other, std::allocator_traits<Allocator>::select_on_container_copy_construction(other.get_allocator()))
			{
			}
			ConcurrentHashTable(ConcurrentHashTable&& other, const Allocator& alloc)
			  : base_hash_equal(other)
			  , Allocator(alloc)
			{
				if (alloc == other.get_allocator()) {
					d_data = AtomicLoad(other.d_data, std::memory_order_acquire);
					other.d_data = nullptr;
				}
				else {
					// Move to a temporary and commit.
					// other is cleared after to preserve its invariant.

					ConcurrentHashTable tmp(other.hash_function(), other.key_eq(), alloc);
					tmp.reserve(other.size());

					try {
						other.visit_all([&](auto& v) { tmp.emplace_policy_no_check<InsertConcurrentPolicy>(std::move(reinterpret_cast<Value&>(v))); });
					}
					catch (...) {
						// We MUST clear or the hash table will contain elements in invalid slots,
						// making the whole hash table invalid.
						// This does not hold for trivially move constructible types.
						if (!std::is_trivially_move_constructible_v<Value>)
							other.clear();
						throw;
					}
					d_data = AtomicLoad(tmp.d_data, std::memory_order_acquire);
					tmp.d_data = nullptr;
					other.clear();
				}
			}
			ConcurrentHashTable(ConcurrentHashTable&& other) noexcept(std::is_nothrow_move_constructible_v<Allocator> && std::is_nothrow_copy_constructible_v<base_hash_equal>)
			  : base_hash_equal(other)
			  , Allocator(std::move(static_cast<Allocator&>(other)))
			{
				d_data = AtomicLoad(other.d_data, std::memory_order_acquire);
				other.d_data = nullptr;
			}
			~ConcurrentHashTable() noexcept { destroy_data(); }

			auto base() noexcept -> base_hash_equal& { return *this; }
			auto base() const noexcept -> const base_hash_equal& { return *this; }

			ConcurrentHashTable& operator=(const ConcurrentHashTable& other)
			{
				if (this == std::addressof(other))
					return *this;

				using traits = std::allocator_traits<Allocator>;

				// Copy allocator, might throw
				if constexpr (traits::propagate_on_container_copy_assignment::value) {
					// We need to destroy the table content before assigning the new allocator
					destroy_data();
					static_cast<Allocator&>(*this) = other.get_allocator();
				}

				ConcurrentHashTable tmp(other.hash_function(), other.key_eq(), get_allocator());
				tmp.reserve(other.size());
				other.visit_all([&](const Value& v) { tmp.template emplace_policy<InsertConcurrentPolicy>([](const auto&) {}, v); });

				// Commit
				destroy_data();
				base() = other.base();
				d_data = AtomicLoad(tmp.d_data, std::memory_order_acquire);
				tmp.d_data = nullptr;

				return *this;
			}

			ConcurrentHashTable& operator=(ConcurrentHashTable&& other) noexcept((std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value
												? std::is_nothrow_move_assignable_v<Allocator>
												: std::allocator_traits<Allocator>::is_always_equal::value) &&
											     std::is_nothrow_copy_assignable_v<base_hash_equal>)
			{
				if (this == std::addressof(other))
					return *this;

				using traits = std::allocator_traits<Allocator>;

				if constexpr (traits::propagate_on_container_move_assignment::value) {

					// Move allocator, might throw
					static_cast<Allocator&>(*this) = std::move(static_cast<Allocator&>(other));
					destroy_data();
					base() = other.base();
					d_data = AtomicLoad(other.d_data, std::memory_order_acquire);
					other.d_data = nullptr;
				}
				else {
					if (get_allocator() == other.get_allocator()) {
						destroy_data();
						base() = other.base();
						d_data = AtomicLoad(other.d_data, std::memory_order_acquire);
						other.d_data = nullptr;
					}
					else {
						ConcurrentHashTable tmp(other.hash_function(), other.key_eq(), get_allocator());
						tmp.reserve(other.size());

						try {
							other.visit_all([&](Value& v) { tmp.template emplace_policy<InsertConcurrentPolicy>([](const auto&) {}, std::move(v)); });
							destroy_data();
							base() = other.base();
						}
						catch (...) {
							if constexpr (!std::is_trivially_move_constructible_v<Value>)
								other.clear();
							throw;
						}

						if constexpr (!std::is_trivially_move_constructible_v<Value>)
							other.clear();

						d_data = AtomicLoad(tmp.d_data, std::memory_order_acquire);
						tmp.d_data = nullptr;
					}
				}

				return *this;
			}

			SEQ_ALWAYS_INLINE auto max_size() const noexcept { return std::allocator_traits<Allocator>::max_size(static_cast<const Allocator&>(*this)); }

			template<class F>
			bool visit_all(F&& fun)
			{
				const PrivateData* d = cget_data();
				if (!d)
					return true;
				auto& f = fun;
				for (unsigned i = 0; i < PrivateData::map_count; ++i)
					// if (!const_cast<PrivateData*>(d)->at(i).visit_all(f))
					if (!const_cast<PrivateData*>(d)->at(i).visit_all([&](Value& v) { return f(reinterpret_cast<PresentedValue&>(v)); }))
						return false;
				return true;
			}
			template<class F>
			bool visit_all(F&& fun) const
			{
				const PrivateData* d = get_data();
				if (!d)
					return true;
				auto& f = fun;
				for (unsigned i = 0; i < PrivateData::map_count; ++i)
					if (!d->at(i).visit_all([&](const Value& v) { return f(reinterpret_cast<const PresentedValue&>(v)); }))
						return false;
				return true;
			}

			template<class ExecPolicy, class F>
			bool visit_all(ExecPolicy&& p, F&& fun)
			{
				if constexpr (!is_concurrent)
					return visit_all(std::forward<F>(fun));
				const PrivateData* d = cget_data();
				if (!d)
					return true;
				std::atomic<bool> res{ true };
				auto& f = fun;
				std::for_each(std::forward<ExecPolicy>(p), d->maps, d->maps + PrivateData::map_count, [&](const auto& m) {
					if (!const_cast<hash_map*>(m.as_map())->visit_all([&](Value& v) { return f(reinterpret_cast<PresentedValue&>(v)); }))
						res.store(false);
				});
				return AtomicLoad(res, std::memory_order_acquire);
			}
			template<class ExecPolicy, class F>
			bool visit_all(ExecPolicy&& p, F&& fun) const
			{
				if constexpr (!is_concurrent)
					return visit_all(std::forward<F>(fun));
				const PrivateData* d = get_data();
				if (!d)
					return true;
				std::atomic<bool> res{ true };
				auto& f = fun;
				std::for_each(std::forward<ExecPolicy>(p), d->maps, d->maps + PrivateData::map_count, [&](auto& m) {
					if (!m.as_map()->visit_all([&](const Value& v) { return f(reinterpret_cast<const PresentedValue&>(v)); }))
						res.store(false);
				});
				return AtomicLoad(res, std::memory_order_acquire);
			}

			void reserve(size_t size)
			{
				if (size) {
					PrivateData* d = get_data();

					constexpr size_t shards_count = size_t{ 1 } << shards;
					size_t size_per_shard = size / shards_count + (size % shards_count ? 1 : 0);

					for (unsigned i = 0; i < PrivateData::map_count; ++i)
						d->at(i).reserve(size_per_shard);
				}
			}

			void rehash(size_t n)
			{
				n >>= shards;
				if (!n)
					n = 1;
				PrivateData* d = get_data();
				for (unsigned i = 0; i < PrivateData::map_count; ++i)
					d->at(i).rehash_table(n);
			}
			void clear()
			{
				const PrivateData* d = cget_data();
				if (!d)
					return;
				for (unsigned i = 0; i < PrivateData::map_count; ++i)
					const_cast<PrivateData*>(d)->at(i).clear();
			}
			auto get_allocator() const -> Allocator { return *this; }

			auto max_load_factor() const noexcept -> float
			{
				const PrivateData* d = get_data();
				return d ? d->max_load_factor() : _SEQ_CONCURRENT_MAP_LOAD_FACTOR;
			}

			auto load_factor() const -> float
			{
				const PrivateData* d = get_data();
				if (!d)
					return 0.f;
				size_t total_size = 0;
				size_t total_capacity = 0;
				for (unsigned i = 0; i < PrivateData::map_count; ++i) {
					total_size += d->at(i).size();
					total_capacity += d->at(i).capacity();
				}
				if (!total_capacity)
					return 0.f;
				return (float)total_size / (float)total_capacity;
			}

			void swap(ConcurrentHashTable& other) noexcept((!std::allocator_traits<Allocator>::propagate_on_container_swap::value || std::is_nothrow_swappable_v<Allocator>) &&
								       std::is_nothrow_swappable_v<base_hash_equal>)
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

					using std::swap;
					swap(base(), other.base());

					auto l = AtomicLoad(d_data, std::memory_order_acquire);
					auto r = AtomicLoad(other.d_data, std::memory_order_acquire);
					d_data = r;
					other.d_data = l;
				}
			}

			auto size() const noexcept -> size_t
			{
				if (const PrivateData* d = get_data()) {
					size_t res = 0;
					for (unsigned i = 0; i < PrivateData::map_count; ++i)
						res += d->at(i).size();
					return res;
				}
				return 0;
			}

			/// @brief This provides strong exception guarantee only if:
			///	-	The hash function and comparator function are noexcept,
			/// -	The value type is nothrow movable
			template<class K, class... Args>
			SEQ_ALWAYS_INLINE auto emplace(K&& key, Args&&... args) -> bool
			{
				return this->emplace_policy<InsertConcurrentPolicy>([](const auto&) {}, std::forward<K>(key), std::forward<Args>(args)...);
			}

			template<class Policy, class K, class... Args>
			SEQ_ALWAYS_INLINE auto emplace_policy_hash_no_exist_no_rehash(size_t hash, K&& key, Args&&... args) -> bool
			{
				return get_data()->at(index_from_hash(hash)).template insert_policy<Policy, false>(hash, [](const auto&) {}, std::forward<K>(key), std::forward<Args>(args)...);
			}

			template<class Policy, class F, class K, class... Args>
			SEQ_ALWAYS_INLINE auto emplace_policy(F&& fun, K&& key, Args&&... args) -> bool
			{
				PrivateData* d = get_data();
				size_t hash = d->hash_key(std::forward<K>(key));
				return d->at(index_from_hash(hash))
				  .template emplace_policy_visit<Policy>(hash, [&](Value& v) { return fun(reinterpret_cast<PresentedValue&>(v)); }, std::forward<K>(key), std::forward<Args>(args)...);
			}

			template<class Policy, class F, class K, class... Args>
			SEQ_ALWAYS_INLINE auto emplace_policy_hash(F&& fun, size_t hash, K&& key, Args&&... args) -> bool
			{
				return get_data()
				  ->at(index_from_hash(hash))
				  .template emplace_policy_visit<Policy>(hash, [&](Value& v) { return fun(reinterpret_cast<PresentedValue&>(v)); }, std::forward<K>(key), std::forward<Args>(args)...);
			}

			template<class Iter>
			void insert(Iter first, Iter last)
			{
				if (is_random_access_v<Iter>)
					this->reserve(size() + std::distance(first, last));
				for (; first != last; ++first)
					emplace(*first);
			}

			template<class K, class F>
			SEQ_ALWAYS_INLINE auto visit(const K& key, F&& fun) const -> size_t
			{
				const PrivateData* d = get_data();
				if SEQ_UNLIKELY (!d)
					return 0;
				size_t hash = d->hash_key((key));
				return d->at(index_from_hash(hash)).visit_hash(hash, key, [&](const Value& v) { return fun(reinterpret_cast<const PresentedValue&>(v)); });
			}
			template<class K, class F>
			SEQ_ALWAYS_INLINE auto visit_hash(size_t hash, const K& key, F&& fun) const -> size_t
			{
				const PrivateData* d = get_data();
				if SEQ_UNLIKELY (!d)
					return 0;
				return d->at(index_from_hash(hash)).visit_hash(hash, key, [&](const Value& v) { return fun(reinterpret_cast<const PresentedValue&>(v)); });
			}
			template<class K, class F>
			SEQ_ALWAYS_INLINE auto visit(const K& key, F&& fun) -> size_t
			{
				PrivateData* d = const_cast<PrivateData*>(cget_data());
				if SEQ_UNLIKELY (!d)
					return 0;
				size_t hash = d->hash_key((key));
				return d->at(index_from_hash(hash)).visit_hash(hash, key, [&](Value& v) { return fun(reinterpret_cast<PresentedValue&>(v)); });
			}
			template<class K, class F>
			SEQ_ALWAYS_INLINE auto visit_hash(size_t hash, const K& key, F&& fun) -> size_t
			{
				PrivateData* d = const_cast<PrivateData*>(cget_data());
				if SEQ_UNLIKELY (!d)
					return 0;
				return d->at(index_from_hash(hash)).visit_hash(hash, key, [&](Value& v) { return fun(reinterpret_cast<PresentedValue&>(v)); });
			}
			template<class K>
			SEQ_ALWAYS_INLINE bool contains(const K& key) const
			{
				return visit(key, [](const auto&) { return false; });
			}
			template<class K>
			SEQ_ALWAYS_INLINE bool contains_hash(size_t hash, const K& key) const
			{
				return visit_hash(hash, key, [](const auto&) { return false; });
			}
			template<class K>
			SEQ_ALWAYS_INLINE auto count(const K& key) const -> size_t
			{
				return contains(key);
			}
			template<class K>
			SEQ_ALWAYS_INLINE auto count_hash(size_t hash, const K& key) const -> size_t
			{
				return contains_hash(hash, key);
			}
			template<class K, class F>
			SEQ_ALWAYS_INLINE auto erase(const K& key, F&& fun) -> size_t
			{
				PrivateData* d = const_cast<PrivateData*>(cget_data());
				if SEQ_UNLIKELY (!d)
					return 0;
				size_t hash = d->hash_key(key);
				return d->at(index_from_hash(hash)).erase_key(hash, [&](Value& v) { return fun(reinterpret_cast<PresentedValue&>(v)); }, key);
			}

			template<class F>
			auto erase_if(F&& fun) -> size_t
			{
				const PrivateData* d = cget_data();
				if SEQ_UNLIKELY (!d)
					return 0;
				size_t res = 0;
				auto& f = fun;
				for (unsigned i = 0; i < PrivateData::map_count; ++i)
					res += const_cast<PrivateData*>(d)->at(i).erase_if([&](Value& v) { return f(reinterpret_cast<PresentedValue&>(v)); });
				return res;
			}

			template<class ExecPolicy, class F>
			auto erase_if(ExecPolicy&& p, F&& fun) -> size_t
			{
				if constexpr (!is_concurrent)
					return erase_if([&](Value& v) { return fun(reinterpret_cast<PresentedValue&>(v)); });
				else {

					const PrivateData* d = cget_data();
					if SEQ_UNLIKELY (!d)
						return 0;
					std::atomic<size_t> res{ 0 };
					auto& f = fun;
					std::for_each(std::forward<ExecPolicy>(p), d->maps, d->maps + PrivateData::map_count, [&](const auto& m) {
						res += const_cast<hash_map*>(m.as_map())->erase_if([&](Value& v) { return f(reinterpret_cast<PresentedValue&>(v)); });
					});
					return res;
				}
			}

			auto merge(ConcurrentHashTable& other) -> size_t
			{
				static_assert(std::is_nothrow_move_constructible_v<Value> || std::is_copy_constructible_v<Value>);

				if (this == std::addressof(other))
					return 0;

				std::scoped_lock<node_lock_type, node_lock_type> guard(d_cross_lock, other.d_cross_lock);

				PrivateData* d2 = const_cast<PrivateData*>(other.cget_data());
				if (!d2)
					return 0;

				return other.erase_if([&](auto& v) { return this->emplace_policy<InsertConcurrentPolicy>([](const auto&) {}, std::move_if_noexcept(reinterpret_cast<Value&>(v))); });
			}

			template<class ExecPolicy>
			auto merge(ExecPolicy&& p, ConcurrentHashTable& other) -> size_t
			{
				static_assert(std::is_nothrow_move_constructible_v<Value> || std::is_copy_constructible_v<Value>);

				if (this == std::addressof(other))
					return 0;

				if constexpr (!is_concurrent)
					return merge(other);

				std::scoped_lock<node_lock_type, node_lock_type> guard(d_cross_lock, other.d_cross_lock);

				PrivateData* d2 = const_cast<PrivateData*>(other.cget_data());
				if (!d2)
					return 0;

				return other.erase_if(std::forward<ExecPolicy>(p),
						      [&](auto& v) { return this->emplace_policy<InsertConcurrentPolicy>([](const auto&) {}, std::move_if_noexcept(reinterpret_cast<Value&>(v))); });
			}

			bool operator==(const ConcurrentHashTable& other) const
			{
				if (this == std::addressof(other))
					return true;

				std::scoped_lock<node_lock_type, node_lock_type> guard(const_cast<node_lock_type&>(d_cross_lock), const_cast<node_lock_type&>(other.d_cross_lock));

				const PrivateData* d1 = cget_data();
				const PrivateData* d2 = other.cget_data();
				if (!d1 && !d2)
					return true;
				if (!d1)
					return other.size() == 0;
				if (!d2)
					return size() == 0;
				if (size() != other.size())
					return false;

				return this->visit_all([&](const auto& value) {
					bool equal = false;
					other.visit(ExtractKey<Key, Value>{}(value), [&](const auto& other_value) { equal = (value == other_value); });
					return equal;
				});
			}

			bool operator!=(const ConcurrentHashTable& other) const { return !(*this == other); }
		};

		template<size_t N>
		struct ApplyFLastFunctor
		{
			template<typename F, typename Last, typename Tuple, typename... A>
			static SEQ_ALWAYS_INLINE auto apply(F&& f, Last&& last, Tuple&& t, A&&... a)
			{
				return ApplyFLastFunctor<N - 1>::apply(
				  std::forward<F>(f), std::forward<Last>(last), std::forward<Tuple>(t), std::get<N - 1>(std::forward<Tuple>(t)), std::forward<A>(a)...);
			}
		};
		template<>
		struct ApplyFLastFunctor<0>
		{
			template<typename F, typename Last, typename Tuple, typename... A>
			static SEQ_ALWAYS_INLINE auto apply(F&& f, Last&& last, Tuple&&, A&&... a)
			{
				return (f)(std::forward<Last>(last), std::forward<A>(a)...);
			}
		};

		template<class F, class Tuple>
		SEQ_ALWAYS_INLINE auto ApplyFLastImpl(F&& f, Tuple&& t)
		{
			static constexpr size_t nargs = std::tuple_size<Tuple>::value;
			return ApplyFLastFunctor<nargs - 1>::apply(std::forward<F>(f), std::get<nargs - 1>(t), std::forward<Tuple>(t));
		}

		/// @brief Call functor f with provided arguments, but shifting last argument to first position
		template<class F, class... Args>
		SEQ_ALWAYS_INLINE auto ApplyFLast(F&& f, Args&&... args)
		{
			return ApplyFLastImpl(std::forward<F>(f), std::forward_as_tuple(std::forward<Args>(args)...));
		}

	}
}

#endif
