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

#ifndef SEQ_CONCURRENT_HASH_TABLE_HPP
#define SEQ_CONCURRENT_HASH_TABLE_HPP



#include "simd.hpp"
#include "hash_utils.hpp"
#include "../memory.hpp"
#include "../lock.hpp"
#include "../utils.hpp"
#include "../hash.hpp"
#include "../bits.hpp"

#ifdef SEQ_HAS_CPP_17
#include <execution>
#endif
#include <limits>
#include <type_traits>


#define SEQ_CONCURRENT_INLINE SEQ_ALWAYS_INLINE

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
		medium_concurrency = 5 , 
		/// @brief Default concurrency level, 32 shards are used, with one read-write spinlock per bucket
		medium_concurrency_shared = medium_concurrency | shared_concurrency,
		/// @brief High concurrency, 256 shards are used, with one spinlock per bucket
		high_concurrency = 8,
		/// @brief High concurrency, 256 shards are used, with one read-write spinlock per bucket
		high_concurrency_shared = high_concurrency | shared_concurrency
	};
	
	namespace detail
	{



#if defined( __SSE2__) 
		// Node size with and without SSE2

		enum {
			max_concurrent_node_size = 16,
			chain_concurrent_node_size = 16
		};
#else
		enum {
			max_concurrent_node_size = 8,
			chain_concurrent_node_size = 8
		};
#endif

#ifdef SEQ_HAS_CPP_17
		// Check ig given type is an execution policy

		template<typename ExecutionPolicy>
		using internal_is_execution_policy = std::is_execution_policy<
			typename std::remove_cv<
			typename std::remove_reference<ExecutionPolicy>::type
			>::type
		>;
#else
		template<typename ExecutionPolicy>
		using internal_is_execution_policy = std::false_type;
#endif

		/// @brief Basic/lighter equivalent to std::shared_lock
		template<class Lock>
		struct LockShared 
		{
			Lock* _l;
			SEQ_CONCURRENT_INLINE LockShared() noexcept :_l(nullptr) {}
			SEQ_CONCURRENT_INLINE LockShared(Lock& l) noexcept :_l(&l) {
				_l->lock_shared();
			}
			SEQ_CONCURRENT_INLINE LockShared(Lock* l, bool) noexcept :_l(l) {}
			SEQ_CONCURRENT_INLINE void init(Lock& l) noexcept {
				SEQ_ASSERT_DEBUG(!_l, "LockShared::init can only be called on default constructed LockShared object");
				_l = &l;
				l.lock_shared();
			}
			SEQ_CONCURRENT_INLINE ~LockShared() noexcept {
				if (_l)
					_l->unlock_shared();
			}
		};
		template<>
		struct LockShared<null_lock> 
		{
			LockShared() noexcept  {}
			LockShared(null_lock& ) noexcept {}
			LockShared(null_lock* , bool) noexcept  {}
			void init(null_lock& ) noexcept {}
		};

		/// @brief Basic/lighter equivalent to std::unique_lock
		template<class Lock>
		struct LockUnique 
		{
			Lock* _l;
			SEQ_CONCURRENT_INLINE LockUnique() noexcept : _l(nullptr) {}
			SEQ_CONCURRENT_INLINE LockUnique(Lock& l) noexcept :_l(&l) {
				_l->lock();
			}
			SEQ_CONCURRENT_INLINE LockUnique(Lock* l, bool) noexcept :_l(l) {}
			SEQ_CONCURRENT_INLINE ~LockUnique() noexcept {
				if (SEQ_LIKELY(_l))
					_l->unlock();
			}
			SEQ_CONCURRENT_INLINE void init(Lock& l) noexcept {
				SEQ_ASSERT_DEBUG(!_l, "LockUnique::init can only be called on default constructed LockUnique object");
				_l = &l;
				l.lock();
			}
			SEQ_CONCURRENT_INLINE void init(Lock& l, bool) noexcept {
				SEQ_ASSERT_DEBUG(!_l, "LockUnique::init can only be called on default constructed LockUnique object");
				_l = &l;
			}
			SEQ_CONCURRENT_INLINE void unlock_and_forget() noexcept {
				if (_l) {
					_l->unlock();
					_l = nullptr;
				}
			}
		};
		template<>
		struct LockUnique<null_lock> 
		{
			LockUnique() noexcept  {}
			LockUnique(null_lock& ) noexcept  {}
			LockUnique(null_lock* , bool) noexcept {}
			void init(null_lock& ) noexcept {}
			void init(null_lock& , bool) noexcept {}
			void unlock_and_forget() noexcept {}
		};

		/// @brief RAII atomic boolean locker
		struct BoolUnlocker
		{
			std::atomic<bool>* _l;
			BoolUnlocker(std::atomic<bool>& l) noexcept : _l(&l) {}
			~BoolUnlocker() noexcept  { *_l = false; }
		};

		/// @brief Random-access array of lock objects
		/// @tparam Lock lock type
		/// @tparam Allocator allocator type
		/// 
		/// SharedLockArray can only grow, and provide concurrent random access.
		/// Each random access might increase the array size if necessary.
		/// Uses a power of 2 grow strategy.
		/// 
		template<class Lock, class Allocator = std::allocator<Lock> >
		class SharedLockArray : private Allocator
		{
			template< class U>
			using rebind_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<U>;
			using lock_type = Lock;
			using this_type = SharedLockArray<Lock, Allocator>;

			// Store ahead of time 32 arrays of lock objects.
			// Each array has a size which is a power of 2 : size = (1 << array_index)
			std::atomic<lock_type*> arrays[32];

			// Allocate/initialize the array for given index (between 0 and 31)
			lock_type* make_array(size_t index) {
				lock_type* l = arrays[index].load(std::memory_order_relaxed);
				if (l) 
					return l;
				rebind_alloc<lock_type> al = *this;
				l = al.allocate(1ull << index);
				memset(static_cast<void*>(l), 0, (1ull << index) * sizeof(lock_type));
				lock_type* prev = nullptr;
				if (arrays[index].compare_exchange_strong(prev, l))
					return l;
				al.deallocate(l, 1ull << index);
				return prev;
			}

		public:
			struct iterator
			{
				this_type*	array;
				unsigned	array_index;
				unsigned	index;

				iterator(this_type* a = nullptr, unsigned ai = 0, unsigned i = 0) noexcept
					:array(a), array_index(ai), index(i) {}
				auto operator*() const noexcept -> Lock& {
					lock_type* l = array->arrays[array_index].load(std::memory_order_relaxed);
					if (SEQ_UNLIKELY(!l))
						l = array->make_array(array_index);
					return l[index];
				}
				auto operator->() const noexcept -> Lock* { return std::pointer_traits<Lock*>::pointer_to(**this); }
				auto operator++() noexcept -> iterator& {
					if (++index == (1u << array_index)) {
						++array_index;
						index = 0;
					}
					return *this;
				}
			};

			SharedLockArray(const Allocator& al = Allocator())
				:Allocator(al) {
				memset(static_cast<void*>(&arrays[0]), 0, sizeof(arrays));
			}
			~SharedLockArray() {
				for (size_t i = 0; i < 32; ++i) {
					if (lock_type* l = arrays[i].load()) {
						size_t size = 1ull << i;
						rebind_alloc<lock_type> al = *this;
						al.deallocate(l, size);
					}
				}
			}
			/// @brief Resize array. Not mandatory as at() member will do it if necessary.
			void resize(size_t size)
			{
				size_t count = bit_scan_reverse_32(static_cast<unsigned>(size));
				for (size_t i = 0; i <= count; ++i)
					if (!arrays[i].load(std::memory_order_relaxed))
						at(i);
			}
			/// @brief Returns element at given position.
			/// Resize array if necessary.
			/// Thread-safe member.
			SEQ_CONCURRENT_INLINE lock_type& at(size_t i) const {
				unsigned ar_index = bit_scan_reverse_32(static_cast<unsigned>(i + 1u));
				unsigned in_array = static_cast<unsigned>(i) - ((1u << ar_index) - 1u);
				lock_type* l = arrays[ar_index].load(std::memory_order_relaxed);
				if (SEQ_UNLIKELY(!l))
					l = const_cast<SharedLockArray*>(this)->make_array(ar_index);
				return l[in_array];
			}
		};



		template<class F, class Ret, class... Args>
		struct Apply
		{
			static SEQ_CONCURRENT_INLINE bool apply(F&& f, Args&&... args) noexcept(noexcept(std::forward<F>(f)(std::forward<Args>(args)...))) {
				std::forward<F>(f)(std::forward<Args>(args)...);
				return true;
			}
		};
		template<class F, class... Args>
		struct Apply<F, bool, Args...>
		{
			static SEQ_CONCURRENT_INLINE bool apply(F&& f, Args&&... args) noexcept(noexcept(std::forward<F>(f)(std::forward<Args>(args)...))) {
				return std::forward<F>(f)(std::forward<Args>(args)...);
			}
		};

		/// @brief Apply functor and return boolean value (true if void() functor, functor result otherwise)
		template<class F, class... Args>
		static SEQ_CONCURRENT_INLINE bool ApplyFunctor(F&& f, Args&&... args) noexcept(noexcept(std::forward<F>(f)(std::forward<Args>(args)...)))
		{
			return Apply<F,
				decltype(std::declval<F>()(std::forward<Args>(args)...)),
				Args...>::apply(std::forward<F>(f), std::forward<Args>(args)...);
		}

		/// @brief Simulate atomic load on non atomic value
		template<class T>
		static SEQ_CONCURRENT_INLINE T AtomicLoad(T v) noexcept { return v; }

		/// @brief Relaxed atomic load
		template<class T>
		static SEQ_CONCURRENT_INLINE T AtomicLoad(const std::atomic<T>& v) noexcept { return v.load(std::memory_order_relaxed); }


		/// @brief Movemask function of 8 bytes word
		static SEQ_CONCURRENT_INLINE auto MoveMask8(std::uint64_t word) noexcept -> std::uint64_t
		{
			std::uint64_t tmp = (word & 0x7F7F7F7F7F7F7F7FULL) + 0x7F7F7F7F7F7F7F7FULL;
			return ~(tmp | word | 0x7F7F7F7F7F7F7F7FULL);
		}

		/// @brief Returns slot index with a tiny hash value of 0
		template<unsigned Size>
		static SEQ_CONCURRENT_INLINE auto FindIndexZero(const std::uint8_t* hashs) noexcept -> unsigned
		{
			if SEQ_CONSTEXPR(Size == 8)
			{
				std::uint64_t _th = 0;
				std::uint64_t found = MoveMask8((*reinterpret_cast<const std::uint64_t*>(hashs)) ^ _th) >> 8u;
				if (found) return bit_scan_forward_64(found) >> 3u;
				return static_cast<unsigned>(-1);
			}
			if SEQ_CONSTEXPR(Size == 16)
			{
#if defined( __SSE2__) 
				auto hs = _mm_loadu_si128(reinterpret_cast<const __m128i*>(hashs));
				int mask = _mm_movemask_epi8(
					_mm_cmpeq_epi8(
						hs, _mm_set1_epi8(0))) >> 1;
				if (mask) 
					return bit_scan_forward_32(static_cast<unsigned>(mask));
#endif
				return static_cast<unsigned>(-1);
			}
			SEQ_UNREACHABLE();
		}

		/// @brief Find given value based on its small hash representation and hashs/values arrays
		template<class ExtractKey, unsigned Size, class Equal, class K, class Value>
		static SEQ_CONCURRENT_INLINE auto FindWithTh(std::uint8_t th, const Equal& eq, K&& key, const std::uint8_t * hashs, const Value* values) 
			noexcept(noexcept(eq(ExtractKey::key(*values), std::forward<K>(key))))
			-> const Value*
		{
			if SEQ_CONSTEXPR(Size == 8) 
			{
				if (!hashs[0]) return nullptr;
				// no SSE variant (way slower)
				std::uint64_t _th;
				memset(&_th, th, sizeof(_th)); 

				// do first 7 values
				std::uint64_t found = MoveMask8((*reinterpret_cast<const std::uint64_t*>(hashs)) ^ _th) >> 8u;
				if (found) {
					SEQ_PREFETCH(values);
					do {
						unsigned pos = bit_scan_forward_64(found) >> 3u;
						if (SEQ_LIKELY(eq(ExtractKey::key(values[pos]), std::forward<K>(key))))
							return values + pos;
						reinterpret_cast<unsigned char*>(&found)[pos] = 0;
					} while (found);
				}
				return nullptr;
			}

			if SEQ_CONSTEXPR(Size == 16 )
			{
#if defined( __SSE2__) 
				// SSE movemask
				if (!hashs[0]) return nullptr;
				auto hs = _mm_loadu_si128(reinterpret_cast<const __m128i*>(hashs));
				int mask = _mm_movemask_epi8(
					_mm_cmpeq_epi8(
						hs, _mm_set1_epi8(static_cast<char>(th)))) >> 1;
				if (mask) {
					SEQ_PREFETCH(values);
					do {
						unsigned pos = bit_scan_forward_32(static_cast<unsigned>(mask));
						if (SEQ_LIKELY(eq(ExtractKey::key(values[pos]), std::forward<K>(key))))
							return values + pos;
						mask &= mask - 1;
					} while (mask);
				}
#endif
				return nullptr; 
			}
		}

		/// @brief Dense node of chain_concurrent_node_size hashs and values
		template<class T>
		struct ConcurrentDenseNode
		{
			using value_type = T;
			static constexpr unsigned size = chain_concurrent_node_size;
			struct alignas(T) Value {
				char data[sizeof(T)];
			};
			
			ConcurrentDenseNode* right;
			ConcurrentDenseNode* left;
			std::uint8_t hashs[size];
			Value vals[size-1];
			
			SEQ_CONCURRENT_INLINE auto count() const noexcept -> unsigned {return hashs[0];}
			SEQ_CONCURRENT_INLINE bool full() const noexcept {return hashs[0]== size - 1;}
			SEQ_CONCURRENT_INLINE auto values() noexcept -> T* {return reinterpret_cast<T*>(vals);}
			SEQ_CONCURRENT_INLINE auto values() const noexcept -> const T* {return reinterpret_cast<const T*>(vals);}
		};

		/// @brief Value node of max_concurrent_node_size values
		template<class T>
		struct ConcurrentValueNode
		{
			using value_type = T;
			struct alignas(T) Value {
				char data[sizeof(T)];
			};
			ConcurrentDenseNode<T>* right;
			Value vals[max_concurrent_node_size - 1];
			SEQ_CONCURRENT_INLINE auto values() noexcept -> T* { return reinterpret_cast<T*>(vals); }
			SEQ_CONCURRENT_INLINE auto values() const noexcept -> const T* { return reinterpret_cast<const T*>(vals); }
		};

		/// @brief Hash node of max_concurrent_node_size tiny hashes
		struct ConcurrentHashNode
		{
			static constexpr unsigned size = max_concurrent_node_size;
			static constexpr unsigned shift = (size == 32 ? 5 : (size == 16 ? 4 : 3));
			std::uint8_t hashs[max_concurrent_node_size];

			ConcurrentHashNode() {
				memset(hashs, 0, sizeof(hashs));
			}

			SEQ_CONCURRENT_INLINE bool full() const noexcept { return hashs[0] == size - 1; }
			SEQ_CONCURRENT_INLINE auto count() const noexcept -> unsigned { return hashs[0]; }
			/// @brief Compute tiny hash representation from full hash value
			static SEQ_CONCURRENT_INLINE auto tiny_hash(size_t hash) noexcept -> std::uint8_t {
				std::uint8_t res = static_cast<std::uint8_t>(hash >> (sizeof(hash) * 8u - 8u));
				return res == 0 ? 1 : res;
			}

			template<class T, class F>
			void for_each(const ConcurrentValueNode<T>* n, F&& f) const 
				noexcept(noexcept(std::forward<F>(f)(std::declval<std::uint8_t*>(), 0, std::declval<T&>())))
			{
				for (unsigned i = 0; i < count(); ++i) 
					std::forward<F>(f)(hashs, i, n->values()[i]);
				if (full() && n->right) {
					const ConcurrentDenseNode<T>* d = n->right;
					do {
						for (unsigned i = 0; i < d->count(); ++i)
							std::forward<F>(f)(d->hashs, i, d->values()[i]);
					} while ((d = d->right));
				}
			}
			template<class T, class F>
			bool for_each_until(const ConcurrentValueNode<T>* n, F&& f) const 
				noexcept(noexcept(std::forward<F>(f)(std::declval<std::uint8_t*>(), 0, std::declval<T&>())))
			{
				for (unsigned i = 0; i < count(); ++i) 
					if (!ApplyFunctor(std::forward<F>(f), hashs, i, n->values()[i]))
						return false;
				if (full() && n->right) {
					const ConcurrentDenseNode<T>* d = n->right;
					do {
						for (unsigned i = 0; i < d->count(); ++i) 
							if(!ApplyFunctor(std::forward<F>(f), d->hashs, i, d->values()[i]))
								return false;
					} while ((d = d->right));
				}
				return true;
			}
			template<class T, class F>
			void for_each(ConcurrentValueNode<T>* n, F&& f) 
				noexcept(noexcept(std::forward<F>(f)(std::declval<std::uint8_t*>(), 0, std::declval<T&>()))) 
			{
				static_cast<const ConcurrentHashNode*>(this)->for_each(n, [&f](const std::uint8_t * hs, unsigned i, const T& v) {std::forward<F>(f)(const_cast<std::uint8_t*>(hs), i, const_cast<T&>(v)); });
			}
			template<class T, class F>
			bool for_each_until(ConcurrentValueNode<T>* n, F&& f) 
				noexcept(noexcept(std::forward<F>(f)(std::declval<std::uint8_t*>(), 0, std::declval<T&>()))) 
			{
				return static_cast<const ConcurrentHashNode*>(this)->for_each_until(n, [&f](const std::uint8_t* hs, unsigned i, const T& v) {std::forward<F>(f)(const_cast<std::uint8_t*>(hs), i, const_cast<T&>(v)); });
			}
		};


		/// @brief Find given value in a dense node.
		/// Performs lookup on the full chain.
		template<class ExtractKey, class Equal, class K, class Value, class F>
		auto FindInDense(std::uint8_t th, const Equal& eq, K&& key, const ConcurrentDenseNode<Value>* n, F && f) 
			noexcept(noexcept(eq(std::forward<K>(key), ExtractKey::key(std::declval<Value&>()))))
			-> size_t
		{
			do {
				auto v = FindWithTh< ExtractKey, chain_concurrent_node_size>(th, eq, std::forward<K>(key), n->hashs, n->values());
				if (v)
				{
					std::forward<F>(f)(const_cast<Value&>(*v));
					return 1;
				}
			} while ((n = n->right));
			return 0;
		}

		/// @brief Find given value in main node.
		/// Performs lookup on the full chain.
		template<class ExtractKey, class Equal, class K, class Value, class F>
		SEQ_CONCURRENT_INLINE auto FindInNode(std::uint8_t th, const Equal& eq, K&& key, const ConcurrentHashNode* node, const ConcurrentValueNode<Value>* values, F && f) 
			noexcept(noexcept(eq(std::forward<K>(key), ExtractKey::key(std::declval<Value&>()))))
			-> size_t
		{
			if (auto v = FindWithTh< ExtractKey, max_concurrent_node_size>(th, eq, std::forward<K>(key), node->hashs, values->values()))
			{
				std::forward<F>(f)(const_cast<Value&>(*v));
				return 1;
			}
			if (SEQ_UNLIKELY(node->full() && values->right))
				return FindInDense< ExtractKey>(th, eq, std::forward<K>(key), values->right, std::forward<F>(f));
			return 0;
		}

		/// @brief Returns index of the next null hash value
		/// Only used when an exception is thrown during rehash
		template<class Value>
		auto FindFreeSlotInNode( ConcurrentHashNode* node, ConcurrentValueNode<Value>* values) noexcept -> std::pair<Value*,std::uint8_t*>
		{
			// Look for a free slot in the node chain
			unsigned idx = FindIndexZero< max_concurrent_node_size>(node->hashs);
			if (idx != static_cast<unsigned>(-1)) 
				return {values->values() + idx, node->hashs + idx + 1};
			auto* d = values->right;
			while (d) {
				idx = FindIndexZero< chain_concurrent_node_size>(d->hashs);
				if (idx != static_cast<unsigned>(-1))
					return { d->values() + idx, d->hashs + idx + 1 };
				d = d->right;
			}
			return {nullptr, nullptr};
		}

		/// @brief Insert value in a new dense node
		template<class Policy, class ChainCount, class Allocator, class Node, class K, class... Args>
		auto InsertNewDense(ChainCount & counter, const Allocator& al, std::uint8_t th, Node* n, K&& key, Args&&... args)
			-> std::pair<typename Node::value_type*, bool>
		{
			using value_type = typename Node::value_type;
			using Alloc = typename std::allocator_traits<Allocator>::template rebind_alloc< ConcurrentDenseNode< value_type> >;
			Alloc a = al;
			ConcurrentDenseNode< value_type>* d = a.allocate(1);
			memset(d, 0, sizeof(ConcurrentDenseNode< value_type>));
			d->left = reinterpret_cast<ConcurrentDenseNode< value_type>*>(n);
			n->right = d;
			++counter;

			try {
				Policy::emplace(d->values(), std::forward<K>(key), std::forward<Args>(args)...);
				d->hashs[++d->hashs[0]] = th;
			}
			catch (...) {
				// destroy dense node
				n->right = nullptr;
				a.deallocate(d, 1);
				--counter;
				throw;
			}
			return { d->values(),true };
		}

		/// @brief Insert value in dense node if it does not already exist
		template<class ExtractKey, class Policy, bool CheckExists, class ChainCount, class Allocator, class Equal, class K, class Value, class... Args>
		auto FindInsertDense(ChainCount & counter, const Allocator& al, std::uint8_t th, const Equal& eq, ConcurrentDenseNode<Value>* n, K&& key, Args&&... args)
			-> std::pair<Value*, bool>
		{
			auto valid = n;
			do {
				if (CheckExists) {
					auto v = FindWithTh< ExtractKey, chain_concurrent_node_size>(th, eq, ExtractKey::key(std::forward<K>(key)), n->hashs, n->values());
					if (v)
						return { const_cast<Value*>(v), false };
				}
				valid = n;
			} while ((n = n->right));
			if(SEQ_UNLIKELY(valid->full()))
				return  InsertNewDense<Policy>(counter, al, th, valid, std::forward<K>(key), std::forward<Args>(args)...);

			// might throw, fine
			auto p = Policy::emplace(valid->values() + valid->count(), std::forward<K>(key), std::forward<Args>(args)...);
			valid->hashs[++valid->hashs[0]] = th;
			return { p, true };
		}

		/// @brief Insert value if it does not already exist
		template<class ExtractKey, class Policy,  bool CheckExists, class ChainCount, class Allocator, class Equal, class K, class Value, class... Args>
		SEQ_CONCURRENT_INLINE auto FindInsertNode(ChainCount& counter, const Allocator & al, std::uint8_t th, const Equal& eq, ConcurrentHashNode* node, ConcurrentValueNode<Value>* values, K&& key, Args&&... args)
			-> std::pair<Value*,bool>
		{
			if (CheckExists ) {
				auto v = FindWithTh< ExtractKey, max_concurrent_node_size>(th, eq, ExtractKey::key(std::forward<K>(key)), node->hashs, values->values());
				if (v)
					return { const_cast<Value*>(v),false };
			}
			if (SEQ_UNLIKELY(node->full())) {
				if (values->right)
					return FindInsertDense< ExtractKey, Policy, CheckExists>(counter, al, th, eq, values->right, std::forward<K>(key), std::forward<Args>(args)...);
				return InsertNewDense<Policy>(counter, al, th, values, std::forward<K>(key), std::forward<Args>(args)...);
			}
			// might throw, fine
			auto p = Policy::emplace(values->values() + node->count(), std::forward<K>(key), std::forward<Args>(args)...);
			node->hashs[++node->hashs[0]] = th;
			return { p , true };
		}

		/// @brief Standard insert policy
		struct InsertConcurrentPolicy
		{
			template<class T, class K, class... Args >
			static SEQ_CONCURRENT_INLINE T* emplace(T* p, K&& key, Args&&... args)
				noexcept(noexcept(construct_ptr(p, std::forward<K>(key), std::forward<Args>(args)...)))
			{
				construct_ptr(p, std::forward<K>(key), std::forward<Args>(args)...);
				return p;
			}
		};
		/// @brief Try insert policy
		struct TryInsertConcurrentPolicy
		{
			template<class T, class K, class... Args >
			static SEQ_CONCURRENT_INLINE T* emplace(T* p, K&& key, Args&&... args) 
				noexcept(noexcept(construct_ptr(p, std::piecewise_construct,
					std::forward_as_tuple(std::forward<K>(key)),
					std::forward_as_tuple(std::forward<Args>(args)...))))
			{
				construct_ptr(p, std::piecewise_construct,
					std::forward_as_tuple(std::forward<K>(key)),
					std::forward_as_tuple(std::forward<Args>(args)...));
				return p;
			}
		};

		

		/// @brief Concurrent swiss table using chaining instead of standard quadratic probing.
		/// This table could be used alone or combined with sharding.
		/// 
		/// An extra array of RW locks is used to provide fine grained locking.
		/// This array if fully thread safe and can only grow. It is also used to
		/// prevent a node usage during rehash.
		/// 
		template<class PrivateData, class Key, class Value = Key, class NodeLock = shared_spinner<std::uint8_t> >
		class ChainingHashTable 
		{
		public:

			using data_type = PrivateData;
			using Allocator = typename PrivateData::allocator_type;
			using extract_key = ExtractKey<Key, Value>;
			template< class U>
			using rebind_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<U>;
			using node_type = ConcurrentHashNode;
			using value_node_type = ConcurrentValueNode<Value>;
			using chain_node_type = ConcurrentDenseNode<Value>;
			using node_lock = NodeLock;
			using node_allocator = rebind_alloc<node_type>;
			using value_node_allocator = rebind_alloc<value_node_type>;
			using chain_node_allocator = rebind_alloc<chain_node_type>;
			using value_type = typename extract_key::value_type;
			using this_type = ChainingHashTable<PrivateData, Key, Value, NodeLock>;
			using lock_array = SharedLockArray <node_lock, Allocator>;

			static constexpr bool is_concurrent = !std::is_same<NodeLock, null_lock>::value;
			using size_type = typename std::conditional< is_concurrent, std::atomic<size_t>, size_t>::type;
			using chain_count_type = typename std::conditional< is_concurrent, std::atomic<unsigned>, unsigned>::type;
			using lock_array_type = typename std::conditional< is_concurrent, std::atomic< lock_array*>, lock_array*>::type;

			// Maximum hash mask, depends on the SharedLockArray max size.
			// We can insert more elements than this value, but using chaining.
			static constexpr size_t max_hash_mask = is_concurrent ? ((1u << 31u) - 1u) : std::numeric_limits<size_t>::max();

		private:

			node_type*					d_buckets;		// hash table 
			value_node_type*			d_values;		// values
			PrivateData*				d_data;			// pointer to upper PrivateData
			lock_array_type				d_locks;		// lock array
			size_type					d_size;			// total size
			size_t						d_next_target;	// next size before rehash
			size_t						d_hash_mask;	// hash mask
			node_lock					d_rehash_lock;	// unique lock for rehash
			std::atomic<bool>			d_in_rehash;	// tells if we are in rehash
			chain_count_type			d_chain_count;	// number of chained nodes, used to optimize detection of needed rehash on insert

			SEQ_ALWAYS_INLINE auto get_allocator() const noexcept { return d_data->get_allocator(); }
			SEQ_ALWAYS_INLINE auto key_eq() const noexcept { return d_data->key_eq(); }

			static auto  get_static_node() noexcept -> node_type* {
				static node_type node;
				return &node;
			}

			auto make_nodes(size_t count = 1)  -> node_type* 
			{
				node_type* n = node_allocator{ get_allocator() }.allocate(count);
				memset(static_cast<void*>(n), 0, count * sizeof(node_type));
				return n;
			}
			auto make_value_nodes(size_t count = 1)  -> value_node_type*
			{
				value_node_type* n = value_node_allocator{ get_allocator() }.allocate(count);
				memset(n, 0, count * sizeof(value_node_type));
				return n;
			}

			void free_nodes(node_type* n, size_t count) 
				noexcept(noexcept(node_allocator{ Allocator{} }))
			{
				node_allocator{ get_allocator() }.deallocate(n, count);
			}
			void free_nodes(value_node_type* n, size_t count)
				noexcept(noexcept(value_node_allocator{ Allocator{} }))
			{
				value_node_allocator{ get_allocator() }.deallocate(n, count);
			}
			void free_nodes(chain_node_type* n)
				noexcept(noexcept(chain_node_allocator{ Allocator{} }))
			{
				chain_node_allocator{ get_allocator() }.deallocate(n, 1);
			}

			void move_back(node_type* buckets, value_node_type* values, size_t new_hash_mask, 
				node_type* old_buckets, value_node_type* old_values, size_t old_hash_mask)
				noexcept(std::is_nothrow_move_constructible<Value>::value)
			{
				// In case of exception (bad_alloc only) during rehash, move back from new buckets to previous buckets
				for (size_t i = 0; i < new_hash_mask + 1; ++i) {
					buckets[i].for_each(values + i, [&]( std::uint8_t* hashs, unsigned j, Value& v) {
						size_t h = hash_key(extract_key::key(v));
						size_t idx = h & old_hash_mask;
						auto loc = FindFreeSlotInNode(old_buckets + idx, old_values + idx);
						SEQ_ASSERT_DEBUG(loc.first, "");
						construct_ptr(loc.first, std::move(v));
						*loc.second = hashs[j + 1];
						});
				}
			}

			void rehash_internal(size_t new_hash_mask, bool grow_only = false)
			{

				// Rehash the table for given mask value.
				// Do not check for potential duplicate values.

				// Avoid 2 parallel rehashs
				bool prev = false;
				if (!d_in_rehash.compare_exchange_strong(prev, true))
					return;

				// Make sure d_in_rehash will be set to false
				BoolUnlocker bl(d_in_rehash);
				// Lock
				LockUnique<node_lock> ll(d_rehash_lock);

				// Make sure we are growing
				if (grow_only && new_hash_mask <= d_hash_mask && d_hash_mask != 0)
					return;

				lock_array* locks = AtomicLoad(d_locks);
				node_type* buckets = nullptr;
				value_node_type* values = nullptr;
				size_t i = 0;

				// Reset chain count
				d_chain_count = 0;

				try {
					
					// Alloc new buckets
					buckets = make_nodes(new_hash_mask + 1u );
					values = make_value_nodes(new_hash_mask + 1u);

					size_t count = (d_buckets != get_static_node()) ? d_hash_mask + 1u : 0u;
					auto iter = typename lock_array::iterator(locks);

					for (i = 0; i < count; ++i) {

						// Lock position
						if (is_concurrent && locks)
							iter->lock();

						if (new_hash_mask + 1 == (d_hash_mask + 1) * 2) {
							// Grow by factor 2: prefetch the 2 possible destination nodes
							SEQ_PREFETCH(buckets + i);
							SEQ_PREFETCH(buckets + i + d_hash_mask + 1);
						}
						
						d_buckets[i].for_each(d_values + i, [&](std::uint8_t * hashs, unsigned j, Value& val)
							{
								auto pos = hash_key(extract_key::key(val)) & new_hash_mask;
								FindInsertNode<extract_key,InsertConcurrentPolicy, false>(d_chain_count, get_allocator(), hashs[j+1], key_eq(), buckets + pos, values + pos, std::move_if_noexcept(val));
								
								if (std::is_nothrow_move_constructible<Value>::value) {
									if (!std::is_trivially_destructible<Value>::value)
										val.~Value();
									// mark position as destroyed
									hashs[j + 1] = 0;
								}
							});
						
						if (is_concurrent && locks)
							++iter;
					}
				}
				catch (...) {

					if (std::is_nothrow_move_constructible<Value>::value &&
						buckets && values) {
						// Move back values from new to old buckets
						move_back(buckets, values, new_hash_mask, d_buckets, d_values, d_hash_mask);
					}

					// Destroy and deallocate
					destroy_buckets(buckets, values, new_hash_mask + 1);
					
					if (is_concurrent && locks) {
						// Unlock locked nodes
						for (size_t j = 0; j <= i; ++j)
							locks->at(j).unlock();
					}
					throw;
				}

				// Save old bucket
				node_type* old_buckets = d_buckets;
				value_node_type *old_values = d_values;
				size_t old_hash_mask = d_hash_mask;

				// Affect new buckets
				d_next_target = static_cast<size_t>(static_cast<double>((new_hash_mask + 1) * node_type::size) * static_cast<double>(d_data->max_load_factor()));
				d_buckets = buckets;
				d_values = values;
				d_hash_mask = new_hash_mask;
				
				if (is_concurrent && locks) {
					// Unlock all nodes
					auto iter = typename lock_array::iterator(locks);
					size_t count = old_buckets ? old_hash_mask + 1u : 0u;
					for (i = 0; i < count; ++i, ++iter)
						iter->unlock();
				}

				// Destroy previous buckets
				destroy_buckets(old_buckets, old_values, old_hash_mask + 1, !std::is_nothrow_move_constructible<Value>::value);

				// Create locks if needed
				if (is_concurrent && !locks && new_hash_mask >= 1) {
					// might throw, fine
					rebind_alloc<lock_array> al = get_allocator();
					locks = al.allocate(1);
					construct_ptr(locks, get_allocator());
					d_locks = (locks);
				}
			}


			void destroy_buckets(
				node_type* buckets, 
				value_node_type * values, 
				size_t count, 
				bool destroy_values = true)
			{
				// Deallocate nodes and destroy values
				if (buckets == get_static_node())
					return;

				for (size_t i = 0; i < count; ++i) 
				{
					node_type* n = buckets + i;
					value_node_type* v = values + i;
					if (destroy_values && !std::is_trivially_destructible<Value>::value) {
						for (unsigned j = 0; j < n->count(); ++j)
							destroy_ptr(v->values() + j);
					}
					if (n->full() && v->right) {
						ConcurrentDenseNode<Value>* d = v->right;
						do {
							if (destroy_values && !std::is_trivially_destructible<Value>::value) {
								for (unsigned j = 0; j < d->count(); ++j)
									destroy_ptr(d->values() + j);
							}
							ConcurrentDenseNode<Value>* right = d->right;
							free_nodes(d);
							d = right;
						} while (d);
					}
				}
				free_nodes(buckets, count);
				if(values)
					free_nodes(values, count);
			}

			void rehash(size_t size )
			{
				// Rehash for given size 
				if (size == 0)
					return rehash_internal(0, false);
				size_t new_hash_mask = size - 1ULL;
				if ((size & (size - 1ULL)) != 0ULL) // non power of 2
					new_hash_mask = (1ULL << (1ULL + (bit_scan_reverse_64(size)))) - 1ULL;
				new_hash_mask >>= node_type::shift;
				if (new_hash_mask > max_hash_mask)
					new_hash_mask = max_hash_mask;
				if (new_hash_mask != d_hash_mask)
					rehash_internal(static_cast<size_t>(new_hash_mask), false);
			}
			void rehash_on_next_target(size_t s)
			{
				if (d_hash_mask < max_hash_mask && (!is_concurrent || !d_in_rehash.load(std::memory_order_relaxed)))
					rehash_internal(s == 0 ? 0u : static_cast<size_t>((d_hash_mask + 1ull) * 2ull - 1ull), true);
			}
			SEQ_CONCURRENT_INLINE void rehash_on_insert()
			{
				// Rehash on insert
				size_t s = AtomicLoad(d_size);
				if (SEQ_UNLIKELY(s >= d_next_target ))
					// delay rehash as long as there are few chains
					if (d_buckets == get_static_node() || AtomicLoad(d_chain_count) > ((d_hash_mask + 1) >> 5))
						rehash_on_next_target(s);
			}

			SEQ_NOINLINE(auto) update_lock(lock_array* locks, size_t hash, size_t& hash_mask, size_t& pos, node_lock*& l) {
				hash_mask = d_hash_mask;
				if (((hash & hash_mask) != pos)) {
					pos = (hash & hash_mask);
					l->unlock();
					l = &locks->at(pos);
					l->lock();
				}
			}
			SEQ_NOINLINE(auto) update_lock_shared(lock_array* locks, size_t hash, size_t& hash_mask, size_t& pos, node_lock*& l) const {
				hash_mask = d_hash_mask;
				if (((hash & hash_mask) != pos)) {
					pos = (hash & hash_mask);
					l->unlock_shared();
					l = &locks->at(pos);
					l->lock_shared();
				}
			}

			template<bool WaitForBucket = true>
			SEQ_NOINLINE(auto) get_node_global_lock(lock_array* locks, size_t hash, node_lock*& l)-> size_t {
				if (WaitForBucket)
					while (d_buckets == get_static_node()) std::this_thread::yield();
				l = &d_rehash_lock;
				l->lock();
				if ((locks = AtomicLoad(d_locks))) {
					l->unlock();
					return this->template get_node<false>(locks, hash, l);
				}
				return (hash & d_hash_mask);
			}
			template<bool WaitForBucket = true>
			SEQ_CONCURRENT_INLINE auto get_node(lock_array* locks, size_t hash, node_lock*& l) -> size_t {
				if (SEQ_UNLIKELY(!locks))
					return this->template get_node_global_lock<WaitForBucket>(locks, hash, l);
				// Returns node locked for given hash value
				size_t hash_mask = d_hash_mask;
				size_t pos = hash & hash_mask;
				l = &locks->at(pos);
				l->lock();
				while (SEQ_UNLIKELY((WaitForBucket && d_buckets== get_static_node()) || hash_mask != d_hash_mask))
					update_lock(locks, hash, hash_mask, pos, l);
				return pos;
			}

			SEQ_NOINLINE(auto) get_node_shared_global_lock(lock_array* locks, size_t hash, node_lock*& l) const -> size_t {
				l = const_cast<node_lock*>(&d_rehash_lock);
				l->lock_shared();
				if ((locks = AtomicLoad(d_locks)) ){
					l->unlock_shared();
					return get_node_shared(locks, hash, l);
				}
				return (hash & d_hash_mask);
			}
			SEQ_CONCURRENT_INLINE auto get_node_shared(lock_array* locks, size_t hash, node_lock*& l) const -> size_t {
				if (SEQ_UNLIKELY(!locks))
					return get_node_shared_global_lock(locks, hash, l);
				// Returns node locked for given hash value
				size_t hash_mask = d_hash_mask;
				size_t pos = hash & hash_mask;
				l = &locks->at(pos);
				l->lock_shared();
				while (SEQ_UNLIKELY(hash_mask != d_hash_mask))
					update_lock_shared(locks, hash, hash_mask, pos, l);
				return pos;
			}
			
			/// @brief Insert new value based on provided policy
			/// Only insert if value does not already exist.
			/// Call provided function if value already exist.
			template<class Policy, bool CheckExists, class F, class K, class... Args >
			SEQ_CONCURRENT_INLINE auto insert_policy(size_t hash, F&& fun, K&& key, Args&&... args) -> bool
			{
				// Compute tiny hash and get (locked) node
				auto th = node_type::tiny_hash(hash);
				node_lock* ll = nullptr;
				size_t pos = is_concurrent ? this->template get_node<true>(AtomicLoad(d_locks), hash, ll) : (hash & d_hash_mask);

				// Grad lock
				LockUnique<node_lock> lock(ll, true);

				auto p = FindInsertNode<extract_key, Policy, true>(d_chain_count, get_allocator(), th, key_eq(), d_buckets + pos, d_values + pos, std::forward<K>(key), std::forward<Args>(args)...);
				if (SEQ_UNLIKELY(!p.second)) {
					// Key exist: call functor
					std::forward<F>(fun)(*p.first);
					return false;
				}
				
				++d_size;
				return true;
			}

			void erase_full_bucket(node_type* n, value_node_type* v) noexcept
			{
				// Clear full bucket in case of exception
				if (!std::is_trivially_destructible<Value>::value) {
					for (unsigned i = 0; i < n->count(); ++i)
						v->values()[i].~Value();
				}
				d_size -= n->count();
				memset(n->hashs, 0, sizeof(n->hashs));

				chain_node_type* d = v->right;
				while (d) {
					if (!std::is_trivially_destructible<Value>::value) {
						for (unsigned i = 0; i < d->count(); ++i)
							d->values()[i].~Value();
					}
					d_size -= d->count();
					chain_node_type* right = d->right;
					free_nodes(d);
					d = right;
				}
			}

			void erase_from_dense(node_type* bucket, value_node_type* values, chain_node_type* n, unsigned pos)
			{
				// Erase position within a dense node and shift left remaining values
				try {
					while (n->right) {
						// Replace by last value of right node
						unsigned count = n->right->hashs[0];
						n->values()[pos] = std::move(n->right->values()[count - 1]);
						n->hashs[pos + 1] = n->right->hashs[count];
						pos = count - 1;
						n = n->right;
					}
					// move hashs and values toward the left
					if (unsigned move_count = (n->hashs[0] - pos - 1)) {
						std::move(n->values() + pos + 1, n->values() + pos + 1 + move_count, n->values() + pos);
						memmove(n->hashs + pos + 1, n->hashs + pos + 2, move_count);
					}
					// invalidate last hash, destroy value
					n->hashs[n->hashs[0]] = 0;
					n->values()[n->hashs[0] - 1].~Value();
				}
				catch (...) {
					// Erase full node, no ways revert back the state
					erase_full_bucket(bucket, values);
					throw;
				}

				// decrease size and delete node if necessary
				if (--n->hashs[0] == 0 ) {
					chain_node_type* prev = n->left;
					prev->right = nullptr;
					free_nodes(n);
				}
			}
			void erase_from_bucket(node_type* bucket, value_node_type* values, unsigned pos)
			{
				// Erase position from main node and shift left remaining values
				if (values->right) {
					// Take last value from right node
					values->values()[pos] = std::move(values->right->values()[values->right->count() - 1]);
					bucket->hashs[pos + 1] = values->right->hashs[values->right->count()];
					// Erase last value from right node
					erase_from_dense(bucket, values, values->right, values->right->count() - 1);
				}
				else {
					// move hashs and values toward the left
					if (unsigned move_count = (bucket->hashs[0] - pos - 1)) {
						std::move(values->values() + pos + 1, values->values() + pos + 1 + move_count, values->values() + pos);
						memmove(bucket->hashs + pos + 1, bucket->hashs + pos + 2, move_count);
					}
					// invalidate last hash, destroy value, decrease size
					bucket->hashs[bucket->hashs[0]] = 0;
					values->values()[bucket->hashs[0] - 1].~Value();
					bucket->hashs[0]--;
				}
			}

			bool contains_value(const Value& key_value) const
			{
				// Returns true if given value (key AND value) exists in this table
				size_t hash = hash_key(extract_key::key(key_value));
				bool ret = false;
				visit_hash(hash, extract_key::key(key_value), [&](const auto& v) { 
					ret = extract_key::has_value ? (extract_key::value(v) == extract_key::value(key_value)) : true;
					});
				return ret;
			}
			bool contains(const Value& key_value) const
			{
				// Returns true if given value (key AND value) exists in this table
				size_t hash = hash_key(extract_key::key(key_value));
				return visit_hash(hash, extract_key::key(key_value), [&](const auto& ) {});
			}
			
			
		public:

			/// @brief Constructor
			explicit ChainingHashTable(PrivateData* data) noexcept
				: d_buckets(get_static_node())
				, d_values(nullptr)
				, d_data(data)
				, d_locks(nullptr)
				, d_size(0)
				, d_next_target(0)
				, d_hash_mask(0)
				, d_in_rehash(false)
				, d_chain_count(0)
			{}

			/// @brief Destructor
			~ChainingHashTable() noexcept
			{
				// Lock rehash spinlock
				LockUnique<node_lock> lock(d_rehash_lock);
				// Clear tables
				clear_no_lock();
				// Destroy lock array
				if (lock_array* locks = AtomicLoad(d_locks)) {
					destroy_ptr(locks);
					rebind_alloc< lock_array>{get_allocator()}.deallocate(locks, 1);
				}
			}

			/// @brief Returns hash table size
			SEQ_CONCURRENT_INLINE auto size() const noexcept -> size_t { return AtomicLoad(d_size); }

			/// @brief Hash key using provided hasher
			/// Apply hash mixin if hasher does not provide the is_avalanching typedef
			template< class H, class K >
			static SEQ_CONCURRENT_INLINE auto hash_key(const H& hasher, const K& key) -> size_t {
				return hash_value(hasher,(extract_key::key(key)));
			}
			/// @brief Hash key
			template< class K >
			SEQ_CONCURRENT_INLINE auto hash_key(const K& key) const -> size_t { return hash_key(d_data->hash_function(), key); }
			/// @brief Returns the maximum load factor
			SEQ_CONCURRENT_INLINE auto max_load_factor() const noexcept -> float { return d_data->max_load_factor(); }
			/// @brief Set the maximum load factor
			SEQ_CONCURRENT_INLINE void max_load_factor(float f) {
				if (f < 0.1f) f = 0.1f;
				rehash(static_cast<size_t>(static_cast<double>(size()) / static_cast<double>(f)));
			}
			/// @brief Returns current load factor
			SEQ_CONCURRENT_INLINE auto load_factor() const noexcept -> float {
				// Returns the current load factor
				size_t bucket_count = d_buckets != get_static_node() ? d_hash_mask + 1u : 0u;
				return size() == 0 ? 0.f : static_cast<float>(size()) / static_cast<float>(bucket_count * node_type::size);
			}
			/// @brief Reserve enough space in the hash table
			void reserve(size_t size) {
				if (size > this->size())
					rehash(static_cast<size_t>(static_cast<double>(size) / static_cast<double>(max_load_factor())));
			}
			/// @brief Rehashtable for given number of buckets
			void rehash_table(size_t n) {
				if (n == 0)
					clear();
				else
					rehash(n);
			}
			/// @brief Performs a key lookup, and call given functor on the table entry if found.
			/// Returns 1 if the key is found, 0 otherwise.
			template< class K, class F>
			SEQ_CONCURRENT_INLINE size_t visit_hash(size_t hash, const K& key, F&& f) const
			{
				node_lock* lock = nullptr;
				size_t pos = is_concurrent ? get_node_shared(AtomicLoad(d_locks), hash, lock) :  (hash & d_hash_mask);
				LockShared<node_lock> ll(lock, true);
				return FindInNode<extract_key>(node_type::tiny_hash(hash), d_data->key_eq(), key, d_buckets + pos, d_values + pos, std::forward<F>(f));
				
			}
			template< class K, class F>
			SEQ_CONCURRENT_INLINE size_t visit_hash(size_t hash, const K& key, F&& f)
			{
				node_lock* lock = nullptr;
				size_t pos = is_concurrent ? this->template get_node<false>(AtomicLoad(d_locks), hash, lock) :  (hash & d_hash_mask);
				LockUnique<node_lock> ll(lock, true);
				return FindInNode<extract_key>(node_type::tiny_hash(hash), d_data->key_eq(), key, d_buckets + pos, d_values + pos, std::forward<F>(f));
			}
			
			/// @brief Visit all entries and call functor for each of them.
			/// If the functor returns a boolean value evaluated to false, stop visiting and return false.
			/// Otherwise return true.
			template<class F>
			bool visit_all(F&& fun) const
			{
				// Avoid rehash while calling visit_all
				LockShared<node_lock> lock(const_cast<node_lock&>(d_rehash_lock));
				if (d_buckets == get_static_node())
					return true;

				size_t count = d_hash_mask + 1u;
				lock_array* locks = AtomicLoad(d_locks);
				auto iter = typename lock_array::iterator(locks);
				for (size_t i = 0; i < count; ++i) {
					LockShared<node_lock> ll;
					if (locks) ll.init(*iter);
					const node_type* n = d_buckets + i;
					const value_node_type* v = d_values + i;

					if (!n->for_each_until(v, [&](auto, auto, const Value& val)
						{return std::forward<F>(fun)(val); }))
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
				LockShared<node_lock> lock(const_cast<node_lock&>(d_rehash_lock));
				if (d_buckets == get_static_node())
					return true;

				size_t count = d_hash_mask + 1u;
				lock_array* locks = AtomicLoad(d_locks);
				auto iter = typename lock_array::iterator(locks);
				for (size_t i = 0; i < count; ++i) {
					LockUnique<node_lock> ll;
					if (locks) ll.init(*iter);
					node_type* n = d_buckets + i;
					value_node_type* v = d_values + i;

					if (!n->for_each_until(v, [&](auto, auto, Value& val)
						{return std::forward<F>(fun)(val); }))
						return false;
					if (locks)
						++iter;
				}
				return true;
			}

			/// @brief Insert entry based on provided policy
			template<class Policy, class K, class... Args >
			SEQ_CONCURRENT_INLINE auto emplace_policy(size_t hash, K&& key, Args&&... args) -> bool
			{
				rehash_on_insert();
				return insert_policy<Policy, true>(hash, [](const auto&) {}, std::forward<K>(key), std::forward<Args>(args)...);
			}
			/// @brief Insert entry based on provided policy without checking for duplicates
			template<class Policy, class K, class... Args >
			SEQ_CONCURRENT_INLINE auto emplace_policy_no_check(size_t hash, K&& key, Args&&... args) -> bool
			{
				rehash_on_insert();
				return insert_policy<Policy, false>(hash, [](const auto&) {}, std::forward<K>(key), std::forward<Args>(args)...);
			}
			template<class Policy, class F, class K, class... Args >
			SEQ_CONCURRENT_INLINE auto emplace_policy_visit(size_t hash, F&& fun, K&& key, Args&&... args) -> bool
			{
				rehash_on_insert();
				return insert_policy<Policy, true>(hash,std::forward<F>(fun), std::forward<K>(key), std::forward<Args>(args)...);
			}

			/// @brief Erase key if found AND is fun(value) returns true.
			/// Returns number of erased entries (1 or 0).
			template<class F, class K>
			auto erase_key(size_t hash, F&& fun, const K& key) -> size_t
			{
				node_lock* lock = nullptr;
				size_t pos = is_concurrent ? this->template get_node<false>(AtomicLoad(d_locks), hash, lock) : (hash & d_hash_mask);
				LockUnique<node_lock> ll(lock, true);

				if (SEQ_UNLIKELY(d_buckets == get_static_node()))
					return 0;

				auto th = node_type::tiny_hash(hash);
				auto values = d_values + pos;
				auto bucket = d_buckets + pos;
				// Find in main bucket
				auto found = FindWithTh< extract_key, max_concurrent_node_size>(th, d_data->key_eq(), key, bucket->hashs, values->values());

				if (found) {
					// Erase from main bucket if functor returns true
					if (!std::forward<F>(fun)(*found))
						return 0;
					erase_from_bucket(bucket, values, static_cast<unsigned>( found - values->values()));
					--d_size;
					return 1;
				}

				// Go right if possible
				if (!bucket->full() || !values->right)
					return 0;

				auto d = values->right;
				do {
					// Look in dense node
					found = FindWithTh< extract_key, chain_concurrent_node_size>(th, d_data->key_eq(), key, d->hashs, d->values());
					if (found) {
						// Erase from dense node if functor returns true
						if (!std::forward<F>(fun)(*found))
							return 0;
						
						erase_from_dense(bucket, values, d, static_cast<unsigned>(found - d->values()));
						--d_size;
						return 1;
					}
				} while((d = d->right));
				return 0;
			}

			/// @brief Erase all entries for which given functor returns true
			template<class F>
			size_t erase_if(F&& fun)
			{
				// Avoid rehash while calling erase_if
				LockUnique<node_lock> lock(d_rehash_lock);
				if (d_buckets == get_static_node())
					return 0;

				lock_array* locks = AtomicLoad( d_locks);
				size_t count = d_hash_mask + 1u;
				size_t res = 0;
				auto iter = typename lock_array::iterator(locks);

				for (size_t i = 0; i < count; ++i) {
					LockUnique<node_lock> ll;
					if (locks) ll.init(*iter);
					node_type* n = d_buckets + i;
					value_node_type* vals = d_values + i;

					// Go to the most right node
					auto* d = n->full() ? vals->right : nullptr;
					while (d && d->right)
						d = d->right;

					// Erase from dense nodes
					while (d && static_cast<void*>(d) != static_cast<void*>(vals)) {
						auto* prev = d->left;
						// delete backward from dense node
						for (int j = static_cast<int>(d->count() - 1); j >= 0; --j) {
							if (std::forward<F>(fun)(d->values()[j])) {
								erase_from_dense(n, vals, d, static_cast<unsigned>(j));
								--d_size;
								++res;
							}
						}		
						d = prev;
					}

					// Erase from main bucket
					for (int j = static_cast<int>(n->count() - 1); j >= 0; --j) {
						if (std::forward<F>(fun)(vals->values()[j])) {
							erase_from_bucket(n, vals, static_cast<unsigned>(j));
							--d_size;
							++res;
						}
					}

					if (locks)
						++iter;
				}
				
				return res;
			}


			void clear() {
				// Cannot clear while rehashing
				LockUnique<node_lock> lock(d_rehash_lock);
				clear_no_lock();
			}
			/// @brief Clear the hash table
			void clear_no_lock()
			{
				if (d_buckets == get_static_node())
					return;

				lock_array* locks = AtomicLoad(d_locks);
				size_t count = d_hash_mask + 1;

				if (is_concurrent && locks) {
					// lock all buckets
					auto iter = typename lock_array::iterator(locks);
					for (size_t i = 0; i < count; ++i, ++iter)
						(*iter).lock();
				}

				// reset members
				destroy_buckets(d_buckets, d_values, count);
				d_buckets = get_static_node();
				d_values = nullptr;
				d_size = d_next_target = 0;
				d_hash_mask = 0;

				if (is_concurrent && locks) {
					// unlock all buckets
					auto iter = typename lock_array::iterator(locks);
					for (size_t i = 0; i < count; ++i, ++iter)
						(*iter).unlock();
				}
			}

			/// @brief Returns true if two ChainingHashTable are equals
			bool equal_to(const ChainingHashTable& other) const
			{
				if (d_size != other.size())
					return false;

				return visit_all([&](const auto& v) {
					return other.contains_value(v);
					});
			}

			/// @brief insert all entries from other not found in this, and remove them from other
			size_t merge(ChainingHashTable& other)
			{
				return other.erase_if([&](auto& v) {
					size_t hash = hash_key(extract_key::key(v));
					return this->emplace_policy< InsertConcurrentPolicy>(hash, std::move_if_noexcept(v));
					});
			}
		};





		/// @brief Base concurrent hash table class, used by both concurrent_set and concurrent_map.
		/// 
		/// Holds N sub-tables (of type ChainingHashTable) based on _Shards template parameter.
		/// 
		template<
			class Key,
			class Value,
			class Hash,
			class KeyEqual,
			class Alloc,
			unsigned _Shards
		>
			class ConcurrentHashTable : public HashEqual<Hash, KeyEqual>, private Alloc
		{
			static_assert(((_Shards & ~shared_concurrency) <= 10 || _Shards == no_concurrency), "Concurrency factor too high! (limited to 11)");

			static constexpr bool is_concurrent = (_Shards != no_concurrency);
			static constexpr unsigned shards = is_concurrent ? (_Shards & ~shared_concurrency) : 0;

			// Forward declare PrivateData;
			class PrivateData;

			using possible_lock_type = typename std::conditional<static_cast<bool>(_Shards & shared_concurrency), shared_spinner<std::uint8_t>, spinlock>::type;
			using load_factor_type = typename std::conditional<is_concurrent, std::atomic<float>, float>::type;
			using node_lock_type = typename std::conditional<is_concurrent, possible_lock_type, null_lock>::type;
			template< class U>
			using rebind_alloc = typename std::allocator_traits<Alloc>::template rebind_alloc<U>;
			using hash_map = detail::ChainingHashTable<PrivateData, Key, Value, node_lock_type>;
			using base_hash_equal = HashEqual<Hash, KeyEqual>;
			using data_type = typename std::conditional<is_concurrent, std::atomic<PrivateData*>, PrivateData*>::type;
			using data_lock_type = typename std::conditional<is_concurrent, spinlock, null_lock>::type;


			/// @brief Internal data
			class PrivateData : public base_hash_equal, private Alloc
			{
				struct DummyPrivate { using allocator_type = Alloc; };
				using dummy_map = detail::ChainingHashTable<DummyPrivate, Key, Value, node_lock_type>;
				struct alignas(dummy_map) MapHolder {
					std::uint8_t data[sizeof(dummy_map)];
					hash_map* as_map() noexcept { return reinterpret_cast<hash_map*>(data); }
					const hash_map* as_map() const noexcept { return reinterpret_cast<const hash_map*>(data); }
				};
				 
			public:
				using allocator_type = Alloc;
				static constexpr unsigned map_count = 1u << shards;

				load_factor_type load_factor{ 0.7f };
				MapHolder maps[map_count];

				PrivateData( 
					const Hash& hash = Hash(),
					const KeyEqual& equal = KeyEqual(),
					const Alloc& alloc = Alloc())
					: base_hash_equal(hash, equal), Alloc(alloc)
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

				SEQ_CONCURRENT_INLINE auto get_allocator() const noexcept -> const Alloc& { return *this; }
				SEQ_CONCURRENT_INLINE auto get_allocator() noexcept -> Alloc& { return *this; }

				SEQ_CONCURRENT_INLINE auto max_load_factor() const noexcept -> float { return AtomicLoad(load_factor); }

				template<class K>
				SEQ_CONCURRENT_INLINE auto hash_key(const K& key) const noexcept -> size_t { return hash_map::hash_key(this->hash_function(), key); }

				SEQ_CONCURRENT_INLINE auto at(unsigned pos) noexcept -> hash_map& { return reinterpret_cast<hash_map&>(maps[pos]); }
				SEQ_CONCURRENT_INLINE auto at(unsigned pos) const noexcept -> const hash_map& { return reinterpret_cast<const hash_map&>(maps[pos]); }

				void max_load_factor(float f) {
					if (f < 0.1f)
						f = 0.1f;
					load_factor = f;
					for (unsigned i = 0; i < map_count; ++i)
						at(i).max_load_factor(f);
				}
 
				/// @brief Make a PrivateData object
				static auto make(const Alloc& alloc,
					const Hash& hash = Hash(),
					const KeyEqual& equal = KeyEqual()) -> PrivateData*
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

			data_type			d_data;
			data_lock_type		d_data_lock;

			PrivateData* make_data()  {
				PrivateData* d = AtomicLoad(d_data);
				if(d) return d;
				d_data = d = PrivateData::make(this->get_allocator(), this->hash_function(), this->key_eq());
				return d;
			}

			SEQ_CONCURRENT_INLINE PrivateData* get_data() {
				PrivateData* d = AtomicLoad(d_data);
				if (SEQ_UNLIKELY(!d)) {
					LockUnique<data_lock_type> ll(d_data_lock);
					d = make_data();
				}
				return d;
			}
			SEQ_CONCURRENT_INLINE PrivateData* get_data_no_lock() {
				PrivateData* d = AtomicLoad(d_data);
				if (SEQ_UNLIKELY(!d)) {
					d = make_data();
				}
				return d;
			}
			SEQ_CONCURRENT_INLINE const PrivateData* get_data() const noexcept { return AtomicLoad(d_data); }
			SEQ_CONCURRENT_INLINE const PrivateData* cget_data() const noexcept { return AtomicLoad(d_data); }

			/// @brief Given a hash value, returns the corresponding sub-table index
			SEQ_CONCURRENT_INLINE auto index_from_hash(size_t hash) const noexcept -> unsigned
			{
				if SEQ_CONSTEXPR(shards == 0 )
					return 0;
#ifdef SEQ_ARCH_64
				return (hash >> (55u - shards)) & ((1u << shards) - 1u);
#else
				return ((hash >> 24) ^ (hash >> 26) ^ (hash >> 8)) & ((1u << shards) - 1u);
#endif
			}
			template<class Policy, class K, class... Args >
			SEQ_CONCURRENT_INLINE auto emplace_policy_no_check(K&& key, Args&&... args) -> bool
			{
				PrivateData* d = get_data();
				size_t hash = d->hash_key(std::forward<K>(key));
				return d->at(index_from_hash(hash)).template emplace_policy_no_check<Policy>(hash, std::forward<K>(key), std::forward<Args>(args)...);
			}

		public:

			ConcurrentHashTable(const Hash& hash = Hash(), const KeyEqual& equal = KeyEqual(), const Alloc& alloc = Alloc())
				:base_hash_equal(hash, equal), Alloc(alloc), d_data(nullptr)
			{
			}
			ConcurrentHashTable(const ConcurrentHashTable& other, const Alloc& alloc)
				:base_hash_equal(other), Alloc(alloc), d_data(PrivateData::make(alloc, other.hash_function(), other.key_eq()))
			{
				this->reserve(other.size());
				other.visit_all([&](const Value& v) {
					this->emplace_policy_no_check<InsertConcurrentPolicy>(v);
					});
			}
			ConcurrentHashTable(const ConcurrentHashTable& other)
				:ConcurrentHashTable(other, copy_allocator(other.get_safe_allocator()))
			{}
			ConcurrentHashTable(ConcurrentHashTable&& other, const Alloc& alloc)
				:base_hash_equal(other), Alloc(alloc), d_data(PrivateData::make(alloc, other.hash_function(), other.key_eq()))
			{
				if (alloc == other.get_safe_allocator()) {
					this->swap(other, false);
				}
				else {
					this->reserve(other.size());
					other.visit_all([&](Value& v) {
						this->emplace_policy_no_check<InsertConcurrentPolicy>(std::move(v));
						});
				}
			}
			ConcurrentHashTable(ConcurrentHashTable&& other)
				noexcept(std::is_nothrow_move_constructible<Alloc>::value  && std::is_nothrow_copy_constructible< base_hash_equal>::value)
				:base_hash_equal(other), Alloc(std::move(other.get_safe_allocator())), d_data(nullptr)
			{
				LockUnique<data_lock_type> lock(other.d_data_lock);
				d_data = AtomicLoad(other.d_data);
				other.d_data = nullptr;
			}
			~ConcurrentHashTable()
			{
				LockUnique<data_lock_type> ll(d_data_lock);
				PrivateData::destroy(AtomicLoad(d_data));
				d_data = (nullptr);
			}

			ConcurrentHashTable& operator=(const ConcurrentHashTable& other)
			{
				if (this != std::addressof(other)) {
					clear();
					{
						// Allocator assignment
						std::lock(d_data_lock, const_cast<data_lock_type&>(other.d_data_lock));
						LockUnique< data_lock_type> l1(&d_data_lock, true);
						LockUnique< data_lock_type> l2(&const_cast<data_lock_type&>(other.d_data_lock), true);

						assign_allocator<Alloc>(*this, other.get_allocator());
						assign_allocator<Alloc>(get_data_no_lock()->get_allocator(), other.get_allocator());
					}
					other.visit_all([this](const Value& v) {
						this->template emplace_policy<InsertConcurrentPolicy>([](const auto &) {},v);
						});
				}
				return *this;
			}
			ConcurrentHashTable& operator=(ConcurrentHashTable&& other)
				noexcept(noexcept(std::declval<ConcurrentHashTable&>().swap(std::declval<ConcurrentHashTable&>())))
			{
				swap(other);
				return *this;
			}

			template<class F>
			bool visit_all(F&& fun)
			{
				const PrivateData* d = cget_data();
				if (SEQ_UNLIKELY(!d)) return true;
				for (unsigned i = 0; i < PrivateData::map_count; ++i)
					if (!const_cast<PrivateData*>(d)->at(i).visit_all(std::forward<F>(fun)))
						return false;
				return true;
			}
			template<class F>
			bool visit_all(F&& fun) const
			{
				const PrivateData* d = get_data();
				if (SEQ_UNLIKELY(!d)) return true;
				for (unsigned i = 0; i < PrivateData::map_count; ++i)
					if (!d->at(i).visit_all([&fun](const Value& v) {std::forward<F>(fun)(v); }))
						return false;
				return true;
			}

#ifdef SEQ_HAS_CPP_17
			template<class ExecPolicy, class F>
			bool visit_all(ExecPolicy&& p, F&& fun)
			{
				if (!is_concurrent)
					return visit_all(std::forward<F>(fun));
				const PrivateData* d = cget_data();
				if (SEQ_UNLIKELY(!d)) return true;
				std::atomic<bool> res{ true };
				std::for_each(std::forward<ExecPolicy>(p), d->maps, d->maps + PrivateData::map_count,
					[&](const auto& m) {if(! const_cast<hash_map*>(m.as_map())->visit_all(std::forward<F>(fun))) res.store(false); });
				return AtomicLoad(res);
			}
			template<class ExecPolicy, class F>
			bool visit_all(ExecPolicy&& p, F&& fun) const
			{
				if (!is_concurrent)
					return visit_all(std::forward<F>(fun));
				const PrivateData* d = get_data();
				if (SEQ_UNLIKELY(!d)) return true;
				std::atomic<bool> res{ true };
				std::for_each(std::forward<ExecPolicy>(p), d->maps, d->maps + PrivateData::map_count,
					[&](auto& m) {if(! m.as_map()->visit_all(std::forward<F>(fun))) res.store(false); });
				return AtomicLoad(res);
			}
#endif

			void reserve(size_t size)
			{
				if (size) {
					PrivateData* d = get_data();
					for (unsigned i = 0; i < PrivateData::map_count; ++i)
						d->at(i).reserve(size >> shards);
				}
			}
			
			void rehash(size_t n)
			{
				n >>= shards;
				if (!n) n = 1;
				PrivateData* d = get_data();
				for (unsigned i = 0; i < PrivateData::map_count; ++i)
					d->at(i).rehash_table(n );
			}
			void clear()
			{
				const PrivateData* d = cget_data();
				if (SEQ_UNLIKELY(!d)) return;
				for (unsigned i = 0; i < PrivateData::map_count; ++i)
					const_cast<PrivateData*>(d)->at(i).clear();
			}
			auto get_allocator() const -> Alloc
			{
				return *this;
			}
			auto get_safe_allocator() const -> Alloc
			{
				LockUnique<data_lock_type> lock(const_cast<data_lock_type&>(d_data_lock));
				Alloc al = get_allocator();
				return al;
			}
			auto max_load_factor() const noexcept -> float
			{
				const PrivateData* d = get_data();
				return d ? d->max_load_factor() : 0.7f;
			}
			void max_load_factor(float f)
			{
				PrivateData* d = get_data();
				d->max_load_factor(f);
			}
			auto load_factor() const -> float
			{
				const PrivateData* d = get_data();
				if (SEQ_UNLIKELY(!d)) return 0.f;
				float f = 0;
				for (unsigned i = 0; i < PrivateData::map_count; ++i)
					f += d->at(i).load_factor();
				f /= PrivateData::map_count;
				return f;
			}
			void swap(ConcurrentHashTable& other, bool swap_alloc = true)
				noexcept(noexcept(swap_allocator<Alloc>(std::declval<Alloc&>(), std::declval<Alloc&>())) &&
					noexcept(std::declval<base_hash_equal&>().swap(std::declval<base_hash_equal&>())))
			{
				if (this == std::addressof(other))
					return;
				if(is_concurrent)
					std::lock(d_data_lock, other.d_data_lock);
				LockUnique<data_lock_type> l1(&d_data_lock, true);
				LockUnique<data_lock_type> l2(&other.d_data_lock, true);

				PrivateData* odata = AtomicLoad(other.d_data);
				other.d_data = (AtomicLoad(d_data));
				d_data= (odata);
				if (swap_alloc)
					swap_allocator<Alloc>(*this, other);
				static_cast<base_hash_equal&>(*this).swap(other);
			}

			auto get_key_eq() const -> KeyEqual {
				LockUnique<data_lock_type> lock(const_cast<data_lock_type&>(d_data_lock));
				auto eq = this->key_eq();
				return eq;
			}
			auto get_hash_function() const -> Hash {
				LockUnique<data_lock_type> lock(const_cast<data_lock_type&>(d_data_lock));
				auto h = this->hash_function();
				return h;
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

			template<class K, class... Args >
			SEQ_CONCURRENT_INLINE auto emplace(K&& key, Args&&... args) -> bool {
				return this->emplace_policy<InsertConcurrentPolicy>([](const auto&) {}, std::forward<K>(key), std::forward<Args>(args)...);
			}

			template<class Policy, class F, class K, class... Args >
			SEQ_CONCURRENT_INLINE auto emplace_policy(F&& fun, K&& key, Args&&... args) -> bool
			{
				PrivateData* d = get_data();
				size_t hash = d->hash_key(std::forward<K>(key));
				return d->at(index_from_hash(hash)).template emplace_policy_visit<Policy>(hash, std::forward<F>(fun), std::forward<K>(key), std::forward<Args>(args)...);
			}

			template<class Iter>
			void insert(Iter first, Iter last)
			{
				size_t count = seq::distance(first, last);
				if (count)
					this->reserve(size() + count);
				for (; first != last; ++first)
					emplace(*first);
			}

			template<class K, class F>
			SEQ_CONCURRENT_INLINE auto visit(const K& key, F&& fun) const -> size_t
			{
				const PrivateData* d = get_data();
				if (SEQ_UNLIKELY(!d)) 
					return 0;
				size_t hash = d->hash_key((key));
				return d->at(index_from_hash(hash)).visit_hash(hash, key, std::forward<F>(fun));
			}
			template<class K, class F>
			SEQ_CONCURRENT_INLINE auto visit(const K& key, F&& fun) -> size_t
			{
				PrivateData* d = const_cast<PrivateData*>(cget_data());
				if (SEQ_UNLIKELY(!d))
					return 0;
				size_t hash = d->hash_key((key));
				return d->at(index_from_hash(hash)).visit_hash(hash, key, std::forward<F>(fun));
			}
			template<class K>
			SEQ_CONCURRENT_INLINE bool contains(const K& key) const
			{
				return visit(key, [](const auto&) {return false; });
			}
			template<class K>
			SEQ_CONCURRENT_INLINE auto count(const K& key) const -> size_t
			{
				return contains(key);
			}
			template<class K, class F>
			SEQ_CONCURRENT_INLINE auto erase(const K& key, F&& fun) -> size_t
			{
				PrivateData* d = const_cast<PrivateData*>(cget_data());
				if (SEQ_UNLIKELY(!d))
					return 0;
				size_t hash = d->hash_key(key);
				return d->at(index_from_hash(hash)).erase_key(hash, std::forward<F>(fun), key);
			}

			template<class F>
			auto erase_if(F&& fun) -> size_t
			{
				const PrivateData* d = cget_data();
				if (SEQ_UNLIKELY(!d))
					return 0;
				size_t res = 0;
				for (unsigned i = 0; i < PrivateData::map_count; ++i)
					res += const_cast<PrivateData*>(d)->at(i).erase_if(std::forward<F>(fun));
				return res;
			}
#ifdef SEQ_HAS_CPP_17
			template<class ExecPolicy, class F>
			auto erase_if(ExecPolicy&& p, F&& fun) -> size_t
			{
				if (!is_concurrent)
					return erase_if(std::forward<F>(fun));
				const PrivateData* d = cget_data();
				if (SEQ_UNLIKELY(!d)) return 0;
				std::atomic<size_t> res{ 0 };
				std::for_each(std::forward<ExecPolicy>(p), d->maps, d->maps + PrivateData::map_count,
					[&](const auto& m) {res += const_cast<hash_map*>(m.as_map())->erase_if(std::forward<F>(fun)); });
				return res;
			}
#endif

			auto merge(ConcurrentHashTable& other) -> size_t
			{
				// Lock internal data to avoid swap or move during merging
				if(is_concurrent)
					std::lock(const_cast<data_lock_type&>(d_data_lock), const_cast<data_lock_type&>(other.d_data_lock));
				
				LockUnique<data_lock_type> l1(const_cast<data_lock_type*>(&d_data_lock), true);
				LockUnique<data_lock_type> l2(const_cast<data_lock_type*>(&other.d_data_lock), true);

				PrivateData* d1 = get_data_no_lock();
				PrivateData* d2 = const_cast<PrivateData*>(other.cget_data());
				if (!d2) return 0;

				size_t res = 0;
				for (unsigned i = 0; i < PrivateData::map_count; ++i) 
					res += d1->at(i).merge(d2->at(i));
				return res;
			}

#ifdef SEQ_HAS_CPP_17
			template<class ExecPolicy>
			auto merge(ExecPolicy && p, ConcurrentHashTable& other) -> size_t
			{
				// Lock internal data to avoid swap or move during merging
				if (is_concurrent)
					std::lock(const_cast<data_lock_type&>(d_data_lock), const_cast<data_lock_type&>(other.d_data_lock));
				else
					return merge(other);
				LockUnique<data_lock_type> l1(const_cast<data_lock_type*>(&d_data_lock), true);
				LockUnique<data_lock_type> l2(const_cast<data_lock_type*>(&other.d_data_lock), true);

				PrivateData* d1 = get_data_no_lock();
				PrivateData* d2 = const_cast<PrivateData*>(other.cget_data());
				if (!d2) return 0;

				std::atomic<size_t> res = 0;
				std::for_each(std::forward<ExecPolicy>(p), d1->maps, d1->maps + PrivateData::map_count,
					[&](const auto& m) {
						unsigned index = static_cast<unsigned>( &m - d1->maps);
						res += d1->at(index).merge(d2->at(index));
					}
				);
				return res;
			}
#endif


			bool operator==(const ConcurrentHashTable& other) const
			{
				// Lock internal data to avoid swap or move during comparison
				if (is_concurrent)
					std::lock(const_cast<data_lock_type&>(d_data_lock), const_cast<data_lock_type&>(other.d_data_lock));
				LockUnique<data_lock_type> l1(const_cast<data_lock_type*>(&d_data_lock), true);
				LockUnique<data_lock_type> l2(const_cast<data_lock_type*>(&other.d_data_lock), true);

				const PrivateData* d1 = cget_data();
				const PrivateData* d2 = other.cget_data();
				if (!d1 && !d2) return true;
				if (!d1) return other.size() == 0;
				if (!d2) return size() == 0;

				for (unsigned i = 0; i < PrivateData::map_count; ++i) {
					if (!(d1->at(i).equal_to(d2->at(i))))
						return false;
				}
				return true;
			}

			bool operator!=(const ConcurrentHashTable& other) const
			{
				return !(*this == other);
			}
		};






		template<size_t N>
		struct ApplyFLastFunctor
		{
			template<typename F, typename Last, typename Tuple, typename... A>
			static SEQ_CONCURRENT_INLINE auto apply(F&& f, Last&& last, Tuple&& t, A&&... a)
			{
				return ApplyFLastFunctor<N - 1>::apply(std::forward<F>(f), std::forward<Last>(last), std::forward<Tuple>(t), std::get<N - 1>(std::forward<Tuple>(t)), std::forward<A>(a)...);
			}
		};
		template<>
		struct ApplyFLastFunctor<0>
		{
			template<typename F, typename Last, typename Tuple, typename... A>
			static SEQ_CONCURRENT_INLINE auto apply(F&& f, Last&& last, Tuple&&, A&&... a)
			{
				return std::forward<F>(f)(std::forward<Last>(last), std::forward<A>(a)...);
			}
		};

		template<class F, class Tuple>
		SEQ_CONCURRENT_INLINE auto ApplyFLastImpl(F&& f, Tuple&& t)
		{
			static constexpr size_t nargs = std::tuple_size< Tuple>::value;
			return ApplyFLastFunctor< nargs - 1>::apply(std::forward<F>(f), std::get< nargs - 1>(t), std::forward<Tuple>(t));
		}

		/// @brief Call functor f with provided arguments, but shifting last argument to first position
		template<class F, class... Args>
		SEQ_CONCURRENT_INLINE auto ApplyFLast(F&& f, Args&&... args)
		{
			return ApplyFLastImpl(std::forward<F>(f), std::forward_as_tuple(std::forward<Args>(args)...));
		}

	}
}

#endif
