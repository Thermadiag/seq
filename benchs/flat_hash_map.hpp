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

#ifndef SEQ_FLAT_HASH_MAP_HPP
#define SEQ_FLAT_HASH_MAP_HPP

#include "seq/concurrent_map.hpp"

#include <limits>
#include <type_traits>


namespace seq
{

	namespace detail
	{



		/// @brief Standard insert policy
		struct InsertFlatPolicy
		{
			template<class T, class K, class... Args>
			static SEQ_ALWAYS_INLINE T* emplace(T* p, K&& key, Args&&... args) noexcept(noexcept(construct_ptr(p, std::forward<K>(key), std::forward<Args>(args)...)))
			{
				construct_ptr(p, std::forward<K>(key), std::forward<Args>(args)...);
				return p;
			}
		};
		/// @brief Try insert policy
		struct TryInsertFlatPolicy
		{
			template<class T, class K, class... Args>
			static SEQ_ALWAYS_INLINE T* emplace(T* p, K&& key, Args&&... args) noexcept(
			  noexcept(construct_ptr(p, std::piecewise_construct, std::forward_as_tuple(std::forward<K>(key)), std::forward_as_tuple(std::forward<Args>(args)...))))
			{
				construct_ptr(p, std::piecewise_construct, std::forward_as_tuple(std::forward<K>(key)), std::forward_as_tuple(std::forward<Args>(args)...));
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
		template<class Key, class Value, class Hash, class KeyEqual, class Alloc>
		class FlatHashTable
		  : public HashEqual<Hash, KeyEqual>
		  , private Alloc
		{
		public:
			using Allocator = Alloc;
			using extract_key = ExtractKey<Key, Value>;
			template<class U>
			using rebind_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<U>;
			using node_type = ConcurrentHashNode;
			using value_node_type = ConcurrentValueNode<Value>;
			using chain_node_type = ConcurrentDenseNode<Value>;
			using node_allocator = rebind_alloc<node_type>;
			using value_node_allocator = rebind_alloc<value_node_type>;
			using chain_node_allocator = rebind_alloc<chain_node_type>;
			using value_type = typename extract_key::value_type;
			using this_type = FlatHashTable<Key, Value, Hash, KeyEqual, Alloc>;

			using size_type = size_t;
			using chain_count_type = unsigned;

			// Maximum hash mask, depends on the SharedLockArray max size.
			// We can insert more elements than this value, but using chaining.
			static constexpr size_t max_hash_mask = std::numeric_limits<size_t>::max();

			struct ChainAllocator
			{
				this_type* table;
				chain_node_type* allocate(size_t ) 
				{ 
					if (table->first_free) {
						auto* r = table->first_free;
						table->first_free = table->first_free->right;
						return r;
					}
					chain_node_allocator al{ table->get_allocator() };
					return al.allocate(1);
					
				}
				void deallocate(chain_node_type* node, size_t) noexcept
				{ 
					node->right = table->first_free;
					table->first_free = node;
				}
			};

		private:
			node_type* d_buckets;	       // hash table
			value_node_type* d_values;     // values
			size_type d_size;	       // total size
			size_t d_next_target;	       // next size before rehash
			size_t d_hash_mask;	       // hash mask
			chain_count_type d_chain_count; // number of chained nodes, used to optimize detection of needed rehash on insert
			float d_max_load_factor = 0.75f;
			chain_node_type* first_free = nullptr;

			SEQ_ALWAYS_INLINE auto get_allocator() const noexcept { return static_cast<const Alloc&>(*this); }

			static auto get_static_node() noexcept -> node_type*
			{
				static node_type node;
				return &node;
			}

			auto make_nodes(size_t count = 1) -> node_type*
			{
				node_type* n = node_allocator{ get_allocator() }.allocate(count);
				memset(static_cast<void*>(n), 0, count * sizeof(node_type));
				return n;
			}
			auto make_value_nodes(size_t count = 1) -> value_node_type*
			{
				value_node_type* n = value_node_allocator{ get_allocator() }.allocate(count);
				for(size_t i=0; i < count; ++i)
					n[i].right = nullptr; 
				
				return n;
			}

			void free_nodes(node_type* n, size_t count) noexcept(noexcept(node_allocator{ Allocator{} })) { node_allocator{ get_allocator() }.deallocate(n, count); }
			void free_nodes(value_node_type* n, size_t count) noexcept(noexcept(value_node_allocator{ Allocator{} })) { value_node_allocator{ get_allocator() }.deallocate(n, count); }
			void free_nodes(chain_node_type* n) noexcept(noexcept(chain_node_allocator{ Allocator{} })) 
			{ 
				//chain_node_allocator{ get_allocator() }.deallocate(n, 1); 
				ChainAllocator{ this }.deallocate(n, 1);
			}

			void move_back(node_type* buckets, value_node_type* values, size_t new_hash_mask, node_type* old_buckets, value_node_type* old_values, size_t old_hash_mask) noexcept(
			  std::is_nothrow_move_constructible_v<Value>)
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

			void rehash_internal(size_t new_hash_mask, bool grow_only = false)
			{

				// Rehash the table for given mask value.
				// Do not check for potential duplicate values.

				
				// Make sure we are growing
				if (grow_only && new_hash_mask <= d_hash_mask && d_hash_mask != 0)
					return;

				node_type* buckets = nullptr;
				value_node_type* values = nullptr;
				size_t i = 0;

				// Reset chain count
				d_chain_count = 0;

				try {

					// Alloc new buckets
					buckets = make_nodes(new_hash_mask + 1u);
					values = make_value_nodes(new_hash_mask + 1u);

					size_t count = (d_buckets != get_static_node()) ? d_hash_mask + 1u : 0u;

					for (i = 0; i < count; ++i) {

						d_buckets[i].for_each(d_values + i, [&](std::uint8_t* hashs, unsigned j, Value& val) {
							auto pos = hash_key(extract_key::key(val)) & new_hash_mask;
							FindInsertNode<extract_key, InsertFlatPolicy, false>(d_chain_count,
													     //chain_node_allocator{ get_allocator() },
													     ChainAllocator{ this },
													     hashs[j + 1],
													     this->key_eq(),
													     buckets + pos,
													     values + pos,
													     std::move_if_noexcept(val));

							if (std::is_nothrow_move_constructible_v<Value>) {
								if (!std::is_trivially_destructible_v<Value>)
									val.~Value();
								// mark position as destroyed
								hashs[j + 1] = 0;
							}
						});

					}
				}
				catch (...) {

					if (std::is_nothrow_move_constructible_v<Value> && buckets && values) {
						// Move back values from new to old buckets
						move_back(buckets, values, new_hash_mask, d_buckets, d_values, d_hash_mask);
					}

					// Destroy and deallocate
					destroy_buckets(buckets, values, new_hash_mask + 1);

					throw;
				}

				// Save old bucket
				node_type* old_buckets = d_buckets;
				value_node_type* old_values = d_values;
				size_t old_hash_mask = d_hash_mask;

				// Affect new buckets
				d_next_target = static_cast<size_t>(static_cast<double>((new_hash_mask + 1) * node_type::size) * static_cast<double>(max_load_factor()));
				d_buckets = buckets;
				d_values = values;
				d_hash_mask = new_hash_mask;

				// Destroy previous buckets
				destroy_buckets(old_buckets, old_values, old_hash_mask + 1, !std::is_nothrow_move_constructible_v<Value>);
			}

			void destroy_buckets(node_type* buckets, value_node_type* values, size_t count, bool destroy_values = true)
			{
				// Deallocate nodes and destroy values
				if (buckets == get_static_node())
					return;

				for (size_t i = 0; i < count; ++i) {
					node_type* n = buckets + i;
					value_node_type* v = values + i;
					if (destroy_values && !std::is_trivially_destructible_v<Value>) {
						for (unsigned j = 0; j < n->count(); ++j)
							destroy_ptr(v->values() + j);
					}
					if (n->full() && v->right) {
						ConcurrentDenseNode<Value>* d = v->right;
						do {
							if (destroy_values && !std::is_trivially_destructible_v<Value>) {
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
				if (values)
					free_nodes(values, count);
			}

			void rehash(size_t size)
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
				if (d_hash_mask < max_hash_mask)
					rehash_internal(s == 0 ? 0u : static_cast<size_t>((d_hash_mask + 1ull) * 2ull - 1ull), true);
			}
			SEQ_ALWAYS_INLINE void rehash_on_insert()
			{
				// Rehash on insert
				if SEQ_UNLIKELY ((d_size >= d_next_target) && (d_buckets == get_static_node() || d_chain_count > ((d_hash_mask + 1) >> 5)))
					// delay rehash as long as there are few chains
					rehash_on_next_target(d_size);
			}

			/// @brief Insert new value based on provided policy
			/// Only insert if value does not already exist.
			/// Call provided function if value already exist.
			template<class Policy, class F, class K, class... Args>
			SEQ_ALWAYS_INLINE auto insert_policy(size_t hash, F&& fun, K&& key, Args&&... args) -> bool
			{
				// Compute tiny hash and get (locked) node
				auto th = node_type::tiny_hash(hash);
				size_t pos = (hash & d_hash_mask);
				
				auto p = FindInsertNode<extract_key, Policy, true>(
				  d_chain_count, ChainAllocator{ this }, th, this->key_eq(), d_buckets + pos, d_values + pos, std::forward<K>(key), std::forward<Args>(args)...);
				if (!p.second) {
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
				if (!std::is_trivially_destructible_v<Value>) {
					for (unsigned i = 0; i < n->count(); ++i)
						v->values()[i].~Value();
				}
				d_size -= n->count();
				memset(n->hashs, 0, sizeof(n->hashs));

				chain_node_type* d = v->right;
				while (d) {
					if (!std::is_trivially_destructible_v<Value>) {
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
					// Erase full node, no ways to revert back the state
					erase_full_bucket(bucket, values);
					throw;
				}

				// decrease size and delete node if necessary
				if (--n->hashs[0] == 0) {
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

			

		public:
			/// @brief Constructor
			explicit FlatHashTable() noexcept
			  : d_buckets(get_static_node())
			  , d_values(nullptr)
			  , d_size(0)
			  , d_next_target(0)
			  , d_hash_mask(0)
			  , d_chain_count(0)
			{
			}

			/// @brief Destructor
			~FlatHashTable() noexcept
			{
				// Clear tables
				clear_no_lock();
			}

			/// @brief Returns hash table size
			SEQ_ALWAYS_INLINE auto size() const noexcept -> size_t { return AtomicLoad(d_size); }

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
				return hash_key(this->hash_function(), key);
			}
			/// @brief Returns the maximum load factor
			SEQ_ALWAYS_INLINE auto max_load_factor() const noexcept -> float { return d_max_load_factor; }
			/// @brief Set the maximum load factor
			SEQ_ALWAYS_INLINE void max_load_factor(float f)
			{
				if (f < 0.1f)
					f = 0.1f;
				d_max_load_factor = f;
				rehash(static_cast<size_t>(static_cast<double>(size()) / static_cast<double>(f)));
			}
			/// @brief Returns current load factor
			SEQ_ALWAYS_INLINE auto load_factor() const noexcept -> float
			{
				// Returns the current load factor
				size_t bucket_count = d_buckets != get_static_node() ? d_hash_mask + 1u : 0u;
				return size() == 0 ? 0.f : static_cast<float>(size()) / static_cast<float>(bucket_count * node_type::size);
			}
			/// @brief Reserve enough space in the hash table
			void reserve(size_t size)
			{
				if (size > this->size())
					rehash(static_cast<size_t>(static_cast<double>(size) / static_cast<double>(max_load_factor())));
			}
			/// @brief Rehashtable for given number of buckets
			void rehash_table(size_t n)
			{
				if (n == 0)
					clear();
				else
					rehash(n);
			}
			/// @brief Performs a key lookup, and call given functor on the table entry if found.
			/// Returns 1 if the key is found, 0 otherwise.
			template<class K, class F>
			SEQ_ALWAYS_INLINE size_t visit(const K& key, F&& f) const
			{
				size_t hash = hash_key(key);
				size_t pos = (hash & d_hash_mask);
				return FindInNode<extract_key>(node_type::tiny_hash(hash), this->key_eq(), key, d_buckets + pos, d_values + pos, std::forward<F>(f));
			}
			template<class K, class F>
			SEQ_ALWAYS_INLINE size_t visit(const K& key, F&& f)
			{
				size_t hash = hash_key(key);
				size_t pos = (hash & d_hash_mask);
				return FindInNode<extract_key>(node_type::tiny_hash(hash), this->key_eq(), key, d_buckets + pos, d_values + pos, std::forward<F>(f));
			}

			SEQ_ALWAYS_INLINE bool contains_value(const Value& key_value) const
			{
				// Returns true if given value (key AND value) exists in this table
				bool ret = false;
				visit( extract_key::key(key_value), [&](const auto& v) {
					ret = extract_key::has_value ? (extract_key::value(v) == extract_key::value(key_value)) : true;
				});
				return ret;
			}
			SEQ_ALWAYS_INLINE bool contains(const Key& key) const
			{
				// Returns true if given value (key AND value) exists in this table
				return visit( key, [&](const auto&) {});
			}

			/// @brief Visit all entries and call functor for each of them.
			/// If the functor returns a boolean value evaluated to false, stop visiting and return false.
			/// Otherwise return true.
			template<class F>
			bool visit_all(F&& fun) const
			{
				if (d_buckets == get_static_node())
					return true;

				size_t count = d_hash_mask + 1u;
				for (size_t i = 0; i < count; ++i) {
					
					const node_type* n = d_buckets + i;
					const value_node_type* v = d_values + i;

					if (!n->for_each_until(v, [&](auto, auto, const Value& val) { return std::forward<F>(fun)(val); }))
						return false;
					
				}
				return true;
			}
			template<class F>
			bool visit_all(F&& fun)
			{
				// Avoid rehash while calling visit_all
				if (d_buckets == get_static_node())
					return true;

				size_t count = d_hash_mask + 1u;
				for (size_t i = 0; i < count; ++i) {
					
					node_type* n = d_buckets + i;
					value_node_type* v = d_values + i;

					if (!n->for_each_until(v, [&](auto, auto, Value& val) { return std::forward<F>(fun)(val); }))
						return false;
					
				}
				return true;
			}

			
			template<class K, class... Args>
			SEQ_ALWAYS_INLINE auto emplace(K&& key, Args&&... args) -> bool
			{
				return this->emplace_policy<InsertFlatPolicy>(std::forward<K>(key), std::forward<Args>(args)...);
			}
			/// @brief Insert entry based on provided policy
			template<class Policy, class K, class... Args>
			SEQ_ALWAYS_INLINE auto emplace_policy( K&& key, Args&&... args) -> bool
			{
				rehash_on_insert();
				return insert_policy<Policy>(hash_key(key), [](const auto&) {}, std::forward<K>(key), std::forward<Args>(args)...);
			}
			
			template<class Policy, class F, class K, class... Args>
			SEQ_ALWAYS_INLINE auto emplace_policy_visit( F&& fun, K&& key, Args&&... args) -> bool
			{
				rehash_on_insert();
				return insert_policy<Policy>(hash_key(key), std::forward<F>(fun), std::forward<K>(key), std::forward<Args>(args)...);
			}

			/// @brief Erase key if found AND is fun(value) returns true.
			/// Returns number of erased entries (1 or 0).
			template<class F, class K>
			auto erase_key_dense(node_type* bucket, value_node_type* values, uint8_t th, F&& fun, const K& key) -> size_t
			{
				auto* d = values->right;
				do {
					// Look in dense node
					auto found = FindWithTh<extract_key, chain_concurrent_node_size>(th, this->key_eq(), key, d->hashs, d->values());
					if (found) {
						// Erase from dense node if functor returns true
						if (!std::forward<F>(fun)(const_cast<Value&>(*found)))
							return 0;

						erase_from_dense(bucket, values, d, static_cast<unsigned>(found - d->values()));
						--d_size;
						return 1;
					}
				} while ((d = d->right));
				return 0;
			}

			/// @brief Erase key if found AND is fun(value) returns true.
			/// Returns number of erased entries (1 or 0).
			template<class F, class K>
			SEQ_ALWAYS_INLINE auto erase_key(size_t hash, F&& fun, const K& key) -> size_t
			{
				size_t pos = (hash & d_hash_mask);

				if SEQ_UNLIKELY (d_buckets == get_static_node())
					return 0;

				auto th = node_type::tiny_hash(hash);
				auto values = d_values + pos;
				auto bucket = d_buckets + pos;
				// Find in main bucket
				auto found = FindWithTh<extract_key, max_concurrent_node_size>(th, this->key_eq(), key, bucket->hashs, values->values());
				if (found) {
					// Erase from main bucket if functor returns true
					if (!std::forward<F>(fun)(const_cast<Value&>(*found)))
						return 0;
					erase_from_bucket(bucket, values, static_cast<unsigned>(found - values->values()));
					--d_size;
					return 1;
				}

				// Go right if possible
				if (!bucket->full() || !values->right)
					return 0;

				return erase_key_dense(bucket, values, th, std::forward<F>(fun), key);
			}
			template<class F, class K>
			SEQ_ALWAYS_INLINE auto erase(F&& fun, const K& key) -> size_t
			{
				return erase_key(hash_key(key), std::forward<F>(fun), key);
			}

			/// @brief Erase all entries for which given functor returns true
			template<class F>
			size_t erase_if(F&& fun)
			{
				// Avoid rehash while calling erase_if
				if (d_buckets == get_static_node())
					return 0;

				size_t count = d_hash_mask + 1u;
				size_t res = 0;

				for (size_t i = 0; i < count; ++i) {
					
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

				}

				return res;
			}

			void clear()
			{
				// Cannot clear while rehashing
				clear_no_lock();
			}
			/// @brief Clear the hash table
			void clear_no_lock()
			{
				while (first_free) {
					auto* next = first_free->right;
					chain_node_allocator{ get_allocator() }.deallocate(first_free, 1);
					first_free = next;
				}

				if (d_buckets == get_static_node())
					return;

				size_t count = d_hash_mask + 1;

				// reset members
				destroy_buckets(d_buckets, d_values, count);
				d_buckets = get_static_node();
				d_values = nullptr;
				d_size = d_next_target = 0;
				d_hash_mask = 0;
			}

			/// @brief Returns true if two FlatHashTable are equals
			bool equal_to(const FlatHashTable& other) const
			{
				if (d_size != other.size())
					return false;

				return visit_all([&](const auto& v) { return other.contains_value(v); });
			}

			/// @brief insert all entries from other not found in this, and remove them from other
			size_t merge(FlatHashTable& other)
			{
				return other.erase_if([&](auto& v) {
					size_t hash = hash_key(extract_key::key(v));
					return this->emplace_policy<InsertFlatPolicy>(hash, std::move_if_noexcept(v));
				});
			}
		};

	}





	template<class Key, class Hash = hasher<Key>, class Equal = std::equal_to<>, class Allocator = std::allocator<Key>>
	class flat_hash_set : private detail::FlatHashTable<Key, Key, Hash, Equal, Allocator>
	{

		using base_type = detail::FlatHashTable<Key, Key, Hash, Equal, Allocator>;
		using this_type = flat_hash_set<Key, Hash, Equal, Allocator>;
		using extract_key = detail::ExtractKey<Key, Key>;

		template<class K, class H, class E>
		using is_transparent = std::integral_constant<bool, !std::is_same_v<K, void> && has_is_transparent<H>::value && has_is_transparent<E>::value>;

		template<class K, class H, class KE, class A>
		friend bool operator==(flat_hash_set<K, H, KE, A> const& lhs, flat_hash_set<K, H, KE, A> const& rhs);

		template<class K, class H, class KE, class A, class Predicate>
		friend typename flat_hash_set<K, H, KE, A>::size_type erase_if(flat_hash_set<K, H, KE, A>& set, Predicate pred);

		struct emplace_or_visit_impl
		{
			this_type* _this;
			template<class F, class... A>
			bool operator()(F&& f, A&&... a)
			{
				return _this->base_type::template emplace_policy<detail::InsertFlatPolicy>(std::forward<F>(f), std::forward<A>(a)...);
			}
		};
		struct emplace_or_cvisit_impl
		{
			this_type* _this;
			template<class F, class... A>
			bool operator()(F&& f, A&&... a)
			{
				return _this->base_type::template emplace_policy<detail::InsertFlatPolicy>([&](const auto& v) { std::forward<F>(f)(v); }, std::forward<A>(a)...);
			}
		};

		using Policy = detail::BuildValue<Key, has_is_transparent<Hash>::value && has_is_transparent<Equal>::value>;

	public:
		using key_type = Key;
		using value_type = Key;
		using allocator_type = Allocator;
		using size_type = size_t;
		using difference_type = std::ptrdiff_t;
		using hasher = Hash;
		using key_equal = Equal;
		using reference = value_type&;
		using const_reference = const value_type&;
		using pointer = typename std::allocator_traits<Allocator>::pointer;
		using const_pointer = typename std::allocator_traits<Allocator>::const_pointer;

		flat_hash_set()
			: base_type()
		{
		}

		/* explicit flat_hash_set(size_type n, const hasher& hf = hasher(), const key_equal& eql = key_equal(), const allocator_type& a = allocator_type())
			: base_type(hf, eql, a)
		{
			if (n)
				this->rehash(n);
		}

		template<class InputIterator>
		flat_hash_set(InputIterator f, InputIterator l, size_type n = 0, const hasher& hf = hasher(), const key_equal& eql = key_equal(), const allocator_type& a = allocator_type())
			: base_type(hf, eql, a)
		{
			if (n)
				this->rehash(n);
			this->insert(f, l);
		}

		flat_hash_set(const flat_hash_set& other)
			: base_type(other)
		{
		}

		flat_hash_set(flat_hash_set&& other) noexcept(std::is_nothrow_move_constructible_v<base_type>)
			: base_type(std::move(other))
		{
		}

		template<class InputIterator>
		flat_hash_set(InputIterator f, InputIterator l, allocator_type const& a)
			: flat_hash_set(f, l, 0, hasher(), key_equal(), a)
		{
		}

		explicit flat_hash_set(allocator_type const& a)
			: base_type(hasher(), key_equal(), a)
		{
		}

		flat_hash_set(const flat_hash_set& other, const allocator_type& alloc)
			: base_type(other, alloc)
		{
		}

		flat_hash_set(flat_hash_set&& other, const allocator_type& alloc)
			: base_type(std::move(other), alloc)
		{
		}

		flat_hash_set(std::initializer_list<value_type> il,
				    size_type n = 0,
				    const hasher& hf = hasher(),
				    const key_equal& eql = key_equal(),
				    const allocator_type& a = allocator_type())
			: flat_hash_set(n, hf, eql, a)
		{
			this->insert(il.begin(), il.end());
		}

		flat_hash_set(size_type n, const allocator_type& a)
			: flat_hash_set(n, hasher(), key_equal(), a)
		{
		}

		flat_hash_set(size_type n, const hasher& hf, const allocator_type& a)
			: flat_hash_set(n, hf, key_equal(), a)
		{
		}

		template<typename InputIterator>
		flat_hash_set(InputIterator f, InputIterator l, size_type n, const allocator_type& a)
			: flat_hash_set(f, l, n, hasher(), key_equal(), a)
		{
		}

		template<typename InputIterator>
		flat_hash_set(InputIterator f, InputIterator l, size_type n, const hasher& hf, const allocator_type& a)
			: flat_hash_set(f, l, n, hf, key_equal(), a)
		{
		}

		flat_hash_set(std::initializer_list<value_type> il, const allocator_type& a)
			: flat_hash_set(il, 0, hasher(), key_equal(), a)
		{
		}

		flat_hash_set(std::initializer_list<value_type> il, size_type n, const allocator_type& a)
			: flat_hash_set(il, n, hasher(), key_equal(), a)
		{
		}

		flat_hash_set(std::initializer_list<value_type> il, size_type n, const hasher& hf, const allocator_type& a)
			: flat_hash_set(il, n, hf, key_equal(), a)
		{
		}
		*/
		~flat_hash_set() = default;

		/* auto operator=(const flat_hash_set& other) -> flat_hash_set&
		{
			static_cast<base_type&>(*this) = static_cast<const base_type&>(other);
			return *this;
		}
		auto operator=(flat_hash_set&& other) noexcept(noexcept(std::declval<base_type&>().swap(std::declval<base_type&>()))) -> flat_hash_set&
		{
			base_type::swap(other);
			return *this;
		}*/

		SEQ_ALWAYS_INLINE auto size() const noexcept -> size_t { return base_type::size(); }
		SEQ_ALWAYS_INLINE auto max_size() const noexcept -> size_t { return std::numeric_limits<size_t>::max(); }
		SEQ_ALWAYS_INLINE auto empty() const noexcept -> bool { return size() == 0; }
		SEQ_ALWAYS_INLINE auto load_factor() const noexcept -> float { return base_type::load_factor(); }
		SEQ_ALWAYS_INLINE auto max_load_factor() const noexcept -> float { return base_type::max_load_factor(); }
		SEQ_ALWAYS_INLINE void max_load_factor(float f) { base_type::max_load_factor(f); }
		SEQ_ALWAYS_INLINE auto get_allocator() const -> allocator_type { return base_type::get_allocator(); }
		SEQ_ALWAYS_INLINE auto hash_function() const -> hasher { return base_type::hash_function(); }
		SEQ_ALWAYS_INLINE auto key_eq() const -> key_equal { return base_type::key_eq(); }

		SEQ_ALWAYS_INLINE void clear() { base_type::clear(); }
		SEQ_ALWAYS_INLINE void rehash(size_t n) { base_type::rehash(n); }
		SEQ_ALWAYS_INLINE void reserve(size_t size) { base_type::reserve(size); }
		//SEQ_ALWAYS_INLINE void swap(flat_hash_set& other) noexcept(noexcept(std::declval<base_type&>().swap(std::declval<base_type&>()))) { base_type::swap(other); }

		template<class F>
		SEQ_ALWAYS_INLINE bool visit_all(F&& fun)
		{
			return base_type::visit_all(std::forward<F>(fun));
		}
		template<class F>
		SEQ_ALWAYS_INLINE bool visit_all(F&& fun) const
		{
			return base_type::visit_all(std::forward<F>(fun));
		}
		template<class F>
		SEQ_ALWAYS_INLINE bool cvisit_all(F&& fun) const
		{
			return base_type::visit_all(std::forward<F>(fun));
		}

		template<class ExecPolicy, class F>
		SEQ_ALWAYS_INLINE typename std::enable_if<detail::internal_is_execution_policy<ExecPolicy>, bool>::type visit_all(ExecPolicy&& p, F&& fun)
		{
			return base_type::visit_all(std::forward<ExecPolicy>(p), std::forward<F>(fun));
		}
		template<class ExecPolicy, class F>
		SEQ_ALWAYS_INLINE typename std::enable_if<detail::internal_is_execution_policy<ExecPolicy>, bool>::type visit_all(ExecPolicy&& p, F&& fun) const
		{
			return base_type::visit_all(std::forward<ExecPolicy>(p), std::forward<F>(fun));
		}
		template<class ExecPolicy, class F>
		SEQ_ALWAYS_INLINE typename std::enable_if<detail::internal_is_execution_policy<ExecPolicy>, bool>::type cvisit_all(ExecPolicy&& p, F&& fun) const
		{
			return base_type::visit_all(std::forward<ExecPolicy>(p), std::forward<F>(fun));
		}

		template<class F>
		SEQ_ALWAYS_INLINE size_type visit(const Key& key, F&& fun) const
		{
			return base_type::visit(key, std::forward<F>(fun));
		}
		template<class F>
		SEQ_ALWAYS_INLINE size_type cvisit(const Key& key, F&& fun) const
		{
			return base_type::visit(key, std::forward<F>(fun));
		}
		template<class F>
		SEQ_ALWAYS_INLINE size_type visit(const Key& key, F&& fun)
		{
			return base_type::visit(key, std::forward<F>(fun));
		}

		template<class K, class F>
		SEQ_ALWAYS_INLINE typename std::enable_if<is_transparent<K, Equal, Hash>::value, size_type>::type visit(const K& key, F&& fun) const
		{
			return base_type::visit(key, std::forward<F>(fun));
		}
		template<class K, class F>
		SEQ_ALWAYS_INLINE typename std::enable_if<is_transparent<K, Equal, Hash>::value, size_type>::type cvisit(const K& key, F&& fun) const
		{
			return base_type::visit(key, std::forward<F>(fun));
		}
		template<class K, class F>
		SEQ_ALWAYS_INLINE typename std::enable_if<is_transparent<K, Equal, Hash>::value, size_type>::type visit(const K& key, F&& fun)
		{
			return base_type::visit(key, std::forward<F>(fun));
		}

		template<class... Args>
		SEQ_ALWAYS_INLINE auto emplace(Args&&... args) -> bool
		{
			return base_type::emplace(Policy::make(std::forward<Args>(args)...));
		}
		template<class... Args>
		SEQ_ALWAYS_INLINE bool emplace_or_visit(Args&&... args)
		{
			return detail::ApplyFLast(emplace_or_visit_impl{ this }, std::forward<Args>(args)...);
		}
		template<class... Args>
		SEQ_ALWAYS_INLINE bool emplace_or_cvisit(Args&&... args)
		{
			return detail::ApplyFLast(emplace_or_cvisit_impl{ this }, std::forward<Args>(args)...);
		}

		SEQ_ALWAYS_INLINE auto insert(const value_type& value) -> bool { return base_type::emplace(value); }

		SEQ_ALWAYS_INLINE auto insert(value_type&& value) -> bool { return base_type::emplace(std::move(value)); }

		template<class InputIt>
		SEQ_ALWAYS_INLINE void insert(InputIt first, InputIt last)
		{
			base_type::insert(first, last);
		}

		SEQ_ALWAYS_INLINE void insert(std::initializer_list<value_type> ilist) { insert(ilist.begin(), ilist.end()); }

		template<class Ty, class F>
		SEQ_ALWAYS_INLINE bool insert_or_visit(Ty&& value, F&& f)
		{
			return base_type::template emplace_policy<detail::InsertFlatPolicy>(std::forward<F>(f), Policy::make(std::forward<Ty>(value)));
		}
		template<class InputIterator, class F>
		void insert_or_visit(InputIterator first, InputIterator last, F&& f)
		{
			for (; first != last; ++first)
				insert_or_visit(*first, std::forward<F>(f));
		}
		template<class F>
		void insert_or_visit(std::initializer_list<value_type> ilist, F&& f)
		{
			this->insert_or_visit(ilist.begin(), ilist.end(), std::forward<F>(f));
		}

		template<class Ty, class F>
		SEQ_ALWAYS_INLINE bool insert_or_cvisit(Ty&& value, F&& f)
		{
			return base_type::template emplace_policy<detail::InsertFlatPolicy>([&](const auto& v) { std::forward<F>(f)(v); }, Policy::make(std::forward<Ty>(value)));
		}
		template<class InputIterator, class F>
		void insert_or_cvisit(InputIterator first, InputIterator last, F&& f)
		{
			for (; first != last; ++first)
				insert_or_cvisit(*first, std::forward<F>(f));
		}
		template<class F>
		void insert_or_cvisit(std::initializer_list<value_type> ilist, F&& f)
		{
			this->insert_or_cvisit(ilist.begin(), ilist.end(), std::forward<F>(f));
		}

		SEQ_ALWAYS_INLINE auto erase(const Key& key) -> size_type
		{
			return base_type::erase([](const auto&) { return true; }, key);
		}

		template<class K>
		SEQ_ALWAYS_INLINE typename std::enable_if<is_transparent<K, Equal, Hash>::value, size_type>::type erase(const K& key)
		{
			return base_type::erase([](const auto&) { return true; }, key);
		}

		template<class F>
		SEQ_ALWAYS_INLINE auto erase_if(const Key& key, F&& fun) -> size_type
		{
			return base_type::erase(std::forward<F>(fun), key);
		}


		template<class F>
		SEQ_ALWAYS_INLINE size_type erase_if(F&& fun)
		{
			return base_type::erase_if(std::forward<F>(fun));
		}

		SEQ_ALWAYS_INLINE auto count(const Key& key) const -> size_type {return (size_type)base_type::contains(key); }

		template<class K>
		SEQ_ALWAYS_INLINE typename std::enable_if<is_transparent<K, Equal, Hash>::value, size_type>::type count(const K& key) const
		{
			return base_type::count(key);
		}

		SEQ_ALWAYS_INLINE bool contains(const Key& key) const { return base_type::contains(key); }

		template<class K>
		SEQ_ALWAYS_INLINE typename std::enable_if<is_transparent<K, Equal, Hash>::value, bool>::type contains(const K& key) const
		{
			return base_type::contains(key);
		}

		/* template<class H2, class P2>
		size_type merge(flat_hash_set<Key, H2, P2, Allocator, Shards>& x)
		{
			SEQ_ASSERT_DEBUG(get_allocator() == x.get_allocator(), "");
			return base_type::merge(x);
		}

		template<class H2, class P2>
		size_type merge(flat_hash_set<Key, H2, P2, Allocator, Shards>&& x)
		{
			return merge(x);
		}

		template<class ExecPolicy, class H2, class P2>
		typename std::enable_if<detail::internal_is_execution_policy<ExecPolicy>, size_type>::type merge(ExecPolicy&& p, flat_hash_set<Key, H2, P2, Allocator>& x)
		{
			SEQ_ASSERT_DEBUG(get_allocator() == x.get_allocator(), "");
			return base_type::merge(std::forward<ExecPolicy>(p), x);
		}

		template<class ExecPolicy, class H2, class P2>
		typename std::enable_if<detail::internal_is_execution_policy<ExecPolicy>, size_type>::type merge(ExecPolicy&& p, flat_hash_set<Key, H2, P2, Allocator, Shards>&& x)
		{
			return merge(std::forward<ExecPolicy>(p), x);
		}*/
	};
}

#endif
