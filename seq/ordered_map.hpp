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

#ifndef SEQ_ORDERED_MAP_HPP
#define SEQ_ORDERED_MAP_HPP



/** @file */

#include "sequence.hpp"
#include "utils.hpp"


namespace seq
{
	
	
	namespace detail
	{
	
	#if (defined (__x86_64__) || defined (_M_X64)) && !defined(SEQ_NO_COMPRESSED_PTR)
		
		/// @brief Node type used by ordered_map/set. Stores an iterator to the underlying seq::sequence object, a part of the hash value and the distance to the right location (for robin-hood probing)
		/// @tparam T Object type
		/// @tparam Extract Key extractor
		template<class T, class Extract>
		struct SequenceNode
		{
			// Node stored in flat hash table. Based on tagged pointer : the last 16 bits are used to store part of the hash value and the distance.
			using tiny_hash = std::uint8_t;
			using dist_type = std::int16_t;
			using value_type = T;
			using store_type = void*;
		
			static constexpr std::uint64_t pos_bits = base_list_chunk<T>::count_bits;
			static constexpr dist_type max_distance = 126;
			static constexpr dist_type tombstone = 127;
			static constexpr std::uint8_t mask_pos = (1ULL << pos_bits) - 1ULL;
			static constexpr std::uint64_t mask_node = ~((1ULL << pos_bits) - 1ULL) & ((1ULL<<(64ULL- 16ULL ))-1ULL);
			static constexpr std::uint64_t mask_node_and_pos = ((1ULL << (64ULL -16ULL)) - 1ULL);

			std::uint64_t val;

			// Extract part of hash value
			static SEQ_ALWAYS_INLINE auto small_hash(size_t h) noexcept -> tiny_hash {
				return ((h) >> (64U - 8U));// *16777619U;
			}
		
			SEQ_ALWAYS_INLINE SequenceNode() : val(0) { (reinterpret_cast<char*>(&val))[6] = -1; /*set_distance(-1);*/ }
			template< class Iter>
			SEQ_ALWAYS_INLINE SequenceNode(tiny_hash h, dist_type dist, const Iter& it)
			{
				val = static_cast<std::uint64_t>(it.pos) | (reinterpret_cast<std::uint64_t>(it.node));
				(reinterpret_cast<unsigned char*>(&val))[7] = h;
				(reinterpret_cast<char*>(&val))[6] = static_cast<char>(dist);
			}
			
			// Check if node is a tombstone (only for pure linear hashing)
			SEQ_ALWAYS_INLINE bool is_tombstone() const noexcept {return distance() == tombstone;}
			// check if node is valid and not a tombstone
			SEQ_ALWAYS_INLINE bool null_for_search() const noexcept { return (val & ((1ULL << 48ULL) - 1ULL)) == 0 && distance() != tombstone; }
			// check if node is not null
			SEQ_ALWAYS_INLINE bool null() const noexcept { return (val & ((1ULL << 48ULL)-1ULL)) == 0; }
			// position withing sequence chunk (max == 63)
			SEQ_ALWAYS_INLINE auto pos() const noexcept -> std::uint8_t {return val & mask_pos;}
			// sequence chunk, aligned on 64 bytes at most
			SEQ_ALWAYS_INLINE auto node() const noexcept -> detail::list_chunk<T>* {return reinterpret_cast<detail::list_chunk<T>*>(val & mask_node);}
			SEQ_ALWAYS_INLINE auto value() const noexcept -> const T& {
				return node()->buffer()[pos()]; 
			}
			SEQ_ALWAYS_INLINE auto hash() const noexcept -> tiny_hash {
				return (reinterpret_cast<const unsigned char*>(&val))[7];
			}
			SEQ_ALWAYS_INLINE auto distance() const noexcept -> dist_type { return  (reinterpret_cast< const char*>(&val))[6];}
			template<class Iter>
			SEQ_ALWAYS_INLINE bool is_same(const Iter& it) const noexcept {
				//check iterator equality
				return node() == it.node && pos() == it.pos;
			}
			
			SEQ_ALWAYS_INLINE void empty() noexcept {
				// mark the node as null
				val = 0;
				(reinterpret_cast<char*>(&val))[6] = -1;
			}
			SEQ_ALWAYS_INLINE void empty_tombstone() noexcept {
				// mark the node as tombstone
				val = 0;
				(reinterpret_cast<char*>(&val))[6] = static_cast<char>(tombstone);
			}
			SEQ_ALWAYS_INLINE void set_distance(dist_type dist) noexcept {
				(reinterpret_cast< char*>(&val))[6] = static_cast<char>(dist);
			}	

		};

	#else

		template<class T, class Extract>
		struct SequenceNode
		{
			// Node stored in flat hash table. Part of the hash value and the distance are stored in additinoal 8 bits integers.
			using tiny_hash = std::uint8_t;
			using dist_type = std::int16_t;
			using value_type = T;
			using key_type = T;
			using store_type = void*;
			static constexpr std::uint64_t pos_bits = base_list_chunk<T>::count_bits;
			static constexpr dist_type max_distance = 126;
			static constexpr dist_type tombstone = 127;
			static constexpr std::uint64_t tag_bits = pos_bits;// tag_ptr::tag_bits;
			static constexpr std::uint64_t mask_high = (~((1ULL << tag_bits) - 1ULL));
			static constexpr std::uint8_t mask_low = ((1U << static_cast<unsigned>(tag_bits)) - 1U);
			static SEQ_ALWAYS_INLINE tiny_hash small_hash(size_t h) { return h >> (64U - 8U); }

			std::uint8_t storage[sizeof(void*)];
			std::uint8_t _hash;
			std::int8_t _dist;

			SEQ_ALWAYS_INLINE SequenceNode():_hash(0),_dist(-1) { memset(storage, 0, sizeof(storage)); }
			template< class Iter>
			SEQ_ALWAYS_INLINE SequenceNode(tiny_hash h, size_t dist, const Iter& it)
			{
				std::uintptr_t p = (reinterpret_cast<std::uintptr_t>(it.node)) | (it.pos);
				memcpy(storage, &p, sizeof(p));
				_hash = h;
				_dist = static_cast<char>(dist);
			}

			SEQ_ALWAYS_INLINE bool is_tombstone() const noexcept { return distance() == tombstone; }
			SEQ_ALWAYS_INLINE bool null_for_search() const noexcept { return null() && distance() != tombstone; }
			SEQ_ALWAYS_INLINE bool null() const noexcept { return read_ptr_t(storage) == 0; }
			SEQ_ALWAYS_INLINE auto distance() const noexcept -> dist_type {return _dist;}
			SEQ_ALWAYS_INLINE auto pos() const noexcept -> std::uint8_t { return storage[0] & mask_low; }
			SEQ_ALWAYS_INLINE auto node() const noexcept -> detail::list_chunk<T>* {return reinterpret_cast<detail::list_chunk<T>*>(read_size_t(storage) & mask_high);}
			SEQ_ALWAYS_INLINE auto value() noexcept -> T& { return node()->buffer()[pos()]; }
			SEQ_ALWAYS_INLINE auto value() const noexcept -> const T& { return node()->buffer()[pos()]; }
			SEQ_ALWAYS_INLINE auto hash() const noexcept -> tiny_hash { return _hash; }// flags >> max_dist_bits;
			template<class Iter>
			SEQ_ALWAYS_INLINE bool is_same(const Iter& it) const noexcept {
				return node() == it.node && pos() == it.pos;
			}
			SEQ_ALWAYS_INLINE void empty() noexcept {
				memset(storage, 0, sizeof(storage));
				_hash =  0;
				_dist = -1;
			}
			SEQ_ALWAYS_INLINE void empty_tombstone() noexcept {
				memset(storage, 0, sizeof(storage));
				_hash = 0;
				_dist = tombstone;
			}
			SEQ_ALWAYS_INLINE void set_distance(dist_type dist) noexcept {
				_dist = static_cast<char>(dist);
			}
		};

	#endif





		
		/// @brief Gather hash class and equal_to class in the same struct. Inherits both for non empty classes.
		/// This is a simple way to handle statefull hash function or equality comparison function.
		template< class Hash, class Equal, bool EmptyHash = std::is_empty<Hash>::value, bool EmptyEqual = std::is_empty<Equal>::value>
		struct HashEqual : private Hash, private Equal
		{
			HashEqual() {}
			HashEqual(const Hash& h, const Equal& e) : Hash(h), Equal(e) {}
			HashEqual(const HashEqual& other) : Hash(other), Equal(other) {}

			auto hash_function() const -> Hash { return static_cast<Hash&>(*this); }
			auto key_eq() const -> Equal { return static_cast<Equal&>(*this); }

			template< class... Args >
			SEQ_ALWAYS_INLINE auto hash(Args&&... args) const noexcept -> size_t { return (Hash::operator()(std::forward<Args>(args)...)); }
			template< class... Args >
			SEQ_ALWAYS_INLINE bool operator()(Args&&... args) const noexcept { return Equal::operator()(std::forward<Args>(args)...); }
		};
		template< class Hash, class Equal>
		struct HashEqual<Hash,Equal,true,true>
		{
			HashEqual() {}
			HashEqual(const Hash&  /*unused*/, const Equal&  /*unused*/)  {}
			HashEqual(const HashEqual&  /*unused*/) {}

			auto hash_function() const -> Hash { return Hash(); }
			auto key_eq() const -> Equal { return Equal(); }

			template< class... Args >
			SEQ_ALWAYS_INLINE auto hash(Args&&... args) const noexcept -> size_t { return (Hash{}(std::forward<Args>(args)...)); }
			template< class... Args >
			SEQ_ALWAYS_INLINE bool operator()(Args&&... args) const noexcept { return Equal{}(std::forward<Args>(args)...); }
		};
		template< class Hash, class Equal>
		struct HashEqual<Hash, Equal, true,false> : private Equal
		{
			HashEqual() {}
			HashEqual(const Hash&  /*unused*/, const Equal& e) :  Equal(e) {}
			HashEqual(const HashEqual& other) : Equal(other) {}

			auto hash_function() const -> Hash { return Hash(); }
			auto key_eq() const -> Equal { return static_cast<Equal&>(*this); }

			template< class... Args >
			SEQ_ALWAYS_INLINE auto hash(Args&&... args) const noexcept -> size_t { return (Hash{}(std::forward<Args>(args)...)); }
			template< class... Args >
			SEQ_ALWAYS_INLINE bool operator()(Args&&... args) const noexcept { return Equal::operator()(std::forward<Args>(args)...); }
		};
		template< class Hash, class Equal>
		struct HashEqual<Hash, Equal, false, true> : private Hash
		{
			HashEqual() {}
			HashEqual(const Hash& h, const Equal&  /*unused*/) : Hash(h) {}
			HashEqual(const HashEqual& other) : Hash(other) {}

			auto hash_function() const -> Hash { return static_cast<Hash&>(*this); }
			auto key_eq() const -> Equal { return Equal(); }

			template< class... Args >
			SEQ_ALWAYS_INLINE auto hash(Args&&... args) const noexcept -> size_t { return (Hash::operator()(std::forward<Args>(args)...)); }
			template< class... Args >
			SEQ_ALWAYS_INLINE bool operator()(Args&&... args) const noexcept { return Equal{}(std::forward<Args>(args)...); }
		};

		

		/// @brief Insertion location within a seq::sequence object
		enum Location
		{
			Back,
			Front,
			Anywhere
		};

		// Insert value into a sequence at the back, front, or anywhere
		template<class Sequence, Location Loc, class Value>
		struct Inserter
		{
		};

		// Insert by copy
		template<class Sequence, class Value>
		struct Inserter<Sequence, Back, const Value&>
		{
			
			static SEQ_ALWAYS_INLINE auto insert(Sequence& seq, const Value& value) -> typename Sequence::iterator {return seq.emplace_back_iter((value));}
		};
		template<class Sequence, class Value>
		struct Inserter<Sequence, Front, const Value&>
		{
			static SEQ_ALWAYS_INLINE auto insert(Sequence& seq, const Value& value) -> typename Sequence::iterator { return seq.emplace_front_iter((value)); }
		};
		template<class Sequence, class Value>
		struct Inserter<Sequence, Anywhere, const Value&>
		{
			static SEQ_ALWAYS_INLINE auto insert(Sequence& seq, const Value& value) -> typename Sequence::iterator { return seq.emplace((value)); }
		};

		// Insert by move semantic for value constructed on the stack
		template<class Sequence, class Value>
		struct Inserter<Sequence, Back,  Value>
		{
			static SEQ_ALWAYS_INLINE auto insert(Sequence& seq,  Value& value) -> typename Sequence::iterator { return seq.emplace_back_iter(std::move(value)); }
		};
		template<class Sequence, class Value>
		struct Inserter<Sequence, Front,  Value>
		{
			static SEQ_ALWAYS_INLINE auto insert(Sequence& seq,  Value& value) -> typename Sequence::iterator { return seq.emplace_front_iter(std::move(value)); }
		};
		template<class Sequence, class Value>
		struct Inserter<Sequence, Anywhere,  Value>
		{
			static SEQ_ALWAYS_INLINE auto insert(Sequence& seq,  Value& value) -> typename Sequence::iterator { return seq.emplace(std::move(value)); }
		};

		// Insert by move semantic for move insertion
		template<class Sequence, class Value>
		struct Inserter<Sequence,Back,Value&&>
		{
			static SEQ_ALWAYS_INLINE auto insert(Sequence& seq, Value& value) -> typename Sequence::iterator {return seq.emplace_back_iter(std::move(value));}
		};
		template<class Sequence, class Value>
		struct Inserter<Sequence, Front, Value&&>
		{
			static SEQ_ALWAYS_INLINE auto insert(Sequence& seq, Value& value) -> typename Sequence::iterator { return seq.emplace_front_iter(std::move(value)); }
		};
		template<class Sequence, class Value>
		struct Inserter<Sequence, Anywhere, Value&&>
		{
			static SEQ_ALWAYS_INLINE auto insert(Sequence& seq, Value& value) -> typename Sequence::iterator { return seq.emplace(std::move(value)); }
		};
		


		/// @brief Base class for robin-hood hash table.
		/// Values are stored in a seq::sequence object, and the hash table contains iterators to the sequence with additional hash value and distance to perfect location.
		template< class Key, class Value, class Hash, class Equal, class Allocator, LayoutManagement layout>
		struct SparseFlatNodeHashTable : public HashEqual<Hash,Equal>
		{
			
		public:
			using base_type = HashEqual<Hash, Equal>;
			using extract_key = ExtractKey<Key, Value>;
			template< class U>
			using RebindAlloc = typename std::allocator_traits<Allocator>::template rebind_alloc<U>;
			using node_type = SequenceNode<Value, extract_key>;
			using dist_type = typename node_type::dist_type;
			using difference_type = std::ptrdiff_t;
			using tiny_hash = typename node_type::tiny_hash;
			using node_allocator = RebindAlloc<node_type>;
			using bucket_vector = std::vector<node_type, RebindAlloc<node_type> >;
			using value_type = typename extract_key::value_type;
			using key_type = typename extract_key::key_type;
			using mapped_type = typename extract_key::mapped_type;
			using sequence_type = sequence<Value, Allocator, layout, true>;
			using iterator = typename sequence_type::iterator;
			using const_iterator = typename sequence_type::const_iterator;

			static constexpr typename node_type::dist_type mask_dirty = integer_max<typename node_type::dist_type>::value;

			sequence_type	d_seq;			// sequence object holding the actual values
			node_type*		d_buckets;		// hash table with robin-hood probing
			size_t			d_hash_mask;	// hash mask
			size_t			d_next_target;	// next size before rehash
			int				d_max_dist;		// current maximum distance of a node to its theoric best location
			float			d_load_factor;	// maximum load factor


		private:
			static auto null_node() -> node_type* {
				// Null node used to initialize d_buckets (avoid a check on lookup)
				static node_type null;
				return &null;
			}

			SEQ_ALWAYS_INLINE auto find_node(size_t hash, const const_iterator& it) -> node_type*
			{
				// Find an existing node based on a sequence iterator and the hash value
				size_t index = hash & d_hash_mask;
				//std::uintptr_t addr = static_cast<std::uintptr_t>(it.pos) | reinterpret_cast<std::uintptr_t>(it.node);
				while (!d_buckets[index].is_same(it)) {
					SEQ_ASSERT_DEBUG(d_max_dist == node_type::max_distance || !d_buckets[index].null(), "corrupted hash map");
					if (SEQ_UNLIKELY(++index == bucket_size()))
						index = 0;
				}
				return d_buckets + index;
			}
			auto find_node(const const_iterator& it) -> node_type*
			{
				// Find an existing node based on a sequence iterator
				return find_node(hash_key(extract_key::key(*it)), it);
			}

			template<class Iter>
			void rehash(size_t new_hash_mask, Iter first, Iter last)
			{
				// Rehash the table for given mask value.
				// The old table is dropped before creating the new one, avoiding memory peaks.
				// Do not check for potential duplicate values.

				try {
					free_buckets(d_buckets);
					d_buckets = null_node();
					d_next_target = d_hash_mask = 0;
					d_max_dist = 1;
					d_buckets = make_buckets((new_hash_mask + 1ULL));
				}
				catch (...) {
					// mark as dirty
					mark_dirty();
					throw;
				}

				typename node_type::dist_type dist;
				size_t hash, index;
				size_t bsize = new_hash_mask + 1ULL;

				// Loop through values
				for (auto it = first; it != last; ++it) {

					// Hash value
					hash = hash_key(extract_key::key(*it));
					// Compute index
					index = (hash & new_hash_mask);

					if (SEQ_UNLIKELY(d_max_dist == node_type::max_distance)) {
						//Pure linear hashing
						while (!d_buckets[index].null() ) {
							if (SEQ_UNLIKELY(++index == bsize))
								index = 0;
						}
						// Insert
						d_buckets[index] = node_type(node_type::small_hash(hash), index == (hash & new_hash_mask) ? 0 : node_type::max_distance, it);
						continue;
					}

					// Robin-hood probing
					dist = 0;
					while (dist <= d_buckets[index].distance()) {
						if (SEQ_UNLIKELY(++index == bsize))
							index = 0;
						dist++;
					}
					if (SEQ_UNLIKELY(dist > d_max_dist))
						d_max_dist = dist; // Update maximum distance

					node_type n = d_buckets[index];
					d_buckets[index] = node_type(node_type::small_hash(hash), dist, it);

					// Insert node
					if (dist)
						start_insert(d_buckets, bsize, index, dist, n);

				}

				d_hash_mask = new_hash_mask;
				d_next_target = static_cast<size_t>(static_cast<double>(bucket_size()) * static_cast<double>(d_load_factor));
			}

			template<class Iter>
			void rehash_remove_duplicates(size_t new_hash_mask, Iter first, Iter last)
			{
				// Rehash the table for given mask value.
				// The old table is dropped before creating the new one, avoiding memory peaks.
				// Check for potential duplicate values, and erase duplicates from the sequence object in a stable way.

				try {
					free_buckets(d_buckets);
					d_buckets = null_node();
					d_next_target = d_hash_mask = 0;
					d_max_dist = 1;
					d_buckets = make_buckets((new_hash_mask + 1ULL));
				}
				catch (...) {
					mark_dirty();
					throw;
				}



				size_t hash, index, dist;
				size_t bsize = new_hash_mask + 1ULL;

				for (auto it = first; it != last; ) {

					hash = hash_key(extract_key::key(*it));
					tiny_hash h = node_type::small_hash(hash);
					index = (hash & new_hash_mask);

					dist = 0;
					while (!d_buckets[index].null() && static_cast<difference_type>(dist) <= static_cast<difference_type>(d_buckets[index].distance())) {
						if (h == d_buckets[index].hash() && (*this)(extract_key::key(d_buckets[index].value()), extract_key::key (*it)))
						{
							dist = static_cast<size_t>(-1);
							break;
						}
						if (SEQ_UNLIKELY(++index == bsize))
							index = 0;
						dist++;
					}

					if (SEQ_UNLIKELY(d_max_dist == static_cast<int>(node_type::max_distance) && dist != static_cast<size_t>(-1))) {
						// linear hash table, we must go up to an empty node
						while (!d_buckets[index].null()) {

							if (h == d_buckets[index].hash() && (*this)(extract_key::key(d_buckets[index].value()), extract_key::key(*it)))
							{
								dist = static_cast<size_t>(-1);
								break;
							}
							if (SEQ_UNLIKELY(++index == bsize))
								index = 0;
						}
					}


					// the value exists
					if (dist == static_cast<size_t>(-1)) {
						it = d_seq.erase(it);
						continue;
					}

					if (SEQ_UNLIKELY(d_max_dist == node_type::max_distance)) {
						d_buckets[index] = node_type(static_cast<tiny_hash>(h), index == (hash & new_hash_mask) ? 0 : node_type::max_distance, it);
					}
					else {
						if (dist > static_cast<size_t>(d_max_dist))
							d_max_dist = static_cast<int>(dist);// = (dist > node_type::max_distance) ? node_type::max_distance : dist;

						node_type n = node_type(static_cast<tiny_hash>(h), static_cast<dist_type>(dist), it);
						std::swap(n, d_buckets[index]);
						if (dist)
							start_insert(d_buckets, bsize, index, static_cast<dist_type>(dist), n);
					}
					++it;
				}

				d_hash_mask = new_hash_mask;
				d_next_target = static_cast<size_t>(static_cast<double>(bucket_size()) * static_cast<double>(d_load_factor));
			}


			void start_insert(node_type* buckets, size_t bucket_size, size_t index, typename node_type::dist_type dist, node_type node) noexcept
			{
				// Use robin-hood hashing to move keys on insertion
				dist_type od;
				dist = node.distance();
				while (dist != -1) {
					do {
						++dist;
						if (SEQ_UNLIKELY(++index == bucket_size))
							index = 0;
						od = buckets[index].distance();
					} while (od >= dist);
					if (dist > d_max_dist )
						d_max_dist = dist = dist > node_type::max_distance ? node_type::max_distance : dist;

					node.set_distance(dist);
					std::swap(buckets[index], node);
					dist = od;
				}
			}

			auto make_buckets(size_t size) const -> node_type*
			{
				// Allocate/initialize nodes
				node_allocator al = d_seq.get_allocator();
				node_type* res = al.allocate(size);
				for (size_t i = 0; i < size; ++i)
					new (res + i) node_type();
				return res;
			}
			void free_buckets(node_type* buckets) const
			{
				// Deallocate nodes
				if (buckets == null_node() || !buckets)
					return;
				node_allocator al = d_seq.get_allocator();
				al.deallocate(buckets, bucket_size());
			}


		public:

			explicit SparseFlatNodeHashTable(const Hash& hash,
				const Equal& equal,
				const Allocator& alloc) noexcept
				:base_type(hash,equal), d_seq(alloc), d_buckets(null_node()), d_hash_mask(0), d_next_target(0), d_max_dist(1), d_load_factor(0.6f)
			{
			}
			SparseFlatNodeHashTable(SparseFlatNodeHashTable&& other) noexcept 				
				:base_type(other.hash_function(), other.key_eq()), d_seq(std::move(other.d_seq)),
				d_buckets(other.d_buckets), d_hash_mask(other.d_hash_mask),
				d_next_target(other.d_next_target), d_max_dist(other.d_max_dist),
				d_load_factor(other.d_load_factor)
			{
				other.d_buckets = null_node();
				other.d_hash_mask = 0;
				other.d_next_target = 0;
				other.d_max_dist = 1;
			}
			SparseFlatNodeHashTable(SparseFlatNodeHashTable&& other, const Allocator & alloc)
				:base_type(other.hash_function(), other.key_eq()), d_seq(std::move(other.d_seq),alloc),
				d_buckets(other.d_buckets), d_hash_mask(other.d_hash_mask),
				d_next_target(other.d_next_target), d_max_dist(other.d_max_dist),
				d_load_factor(other.d_load_factor)
			{
				if (alloc == other.d_seq.get_allocator()) {
					other.d_buckets = null_node();
					other.d_hash_mask = 0;
					other.d_next_target = 0;
					other.d_max_dist = 1;
				}
				else {
					// rehash
					d_buckets = null_node();
					rehash(0,true);
				}
			}

			~SparseFlatNodeHashTable()
			{
				free_buckets(d_buckets);
			}

			void swap(SparseFlatNodeHashTable& other) noexcept
			{
				if (this != std::addressof(other)) {
					std::swap(static_cast<base_type&>(*this), static_cast<base_type&>(other));
					std::swap(d_buckets, other.d_buckets);
					std::swap(d_hash_mask, other.d_hash_mask);
					std::swap(d_next_target, other.d_next_target);
					std::swap(d_max_dist, other.d_max_dist);
					std::swap(d_load_factor, other.d_load_factor);
					d_seq.swap(other.d_seq);
				}
			}

			SEQ_ALWAYS_INLINE bool dirty() const noexcept 
			{
				// check for dirty
				return d_max_dist == mask_dirty;
			}
			SEQ_ALWAYS_INLINE void mark_dirty() noexcept
			{
				// mark the table as dirty
				d_max_dist = mask_dirty;
			}
			SEQ_ALWAYS_INLINE void check_hash_operation() const 
			{
				if (SEQ_UNLIKELY(dirty()))
					throw std::logic_error("ordered hash table is dirty");
			}
			SEQ_ALWAYS_INLINE auto size() const noexcept -> size_t
			{ 
				return d_seq.size();
			}
			void  reserve(size_t size)
			{
				if(size > this->size())
					rehash(static_cast<size_t>(static_cast<double>(size)/ static_cast<double>(d_load_factor)));
				d_seq.reserve(size);
			}
			void rehash(size_t size = 0, bool force = false)
			{
				// Rehash for given size if one of the following is true:
				// - the hash table is dirty
				// - the new hash table size is different from the current one
				// - force is true

				const bool null_size = (size == 0);

				if(null_size)
					size = static_cast<size_t>(static_cast<double>(this->size()) / static_cast<double>(d_load_factor));
				if (size == 0)
					return rehash(63, d_seq.end(), d_seq.end()); // Minimum size is 64
				size_t new_hash_mask;
				if ((size & (size - 1ULL)) == 0ULL) new_hash_mask = size - 1ULL;
				else new_hash_mask = (1ULL << (1ULL + (bit_scan_reverse_64(size)))) - 1ULL;
				if(dirty())
					rehash_remove_duplicates(new_hash_mask, d_seq.begin(), d_seq.end());
				else if (force || new_hash_mask != d_hash_mask || d_max_dist == node_type::max_distance) {
					if (d_max_dist == node_type::max_distance && null_size) {
						// linear hashing behavior: make sure to increase hash table size
						new_hash_mask = ((new_hash_mask + 1U) * 2U) - 1U;
					}
					rehash(new_hash_mask, d_seq.begin(), d_seq.end());
				}
			}
			
			template< class... Args >
			SEQ_ALWAYS_INLINE auto hash_key(Args&&... args) const noexcept -> size_t
			{
				// Hash the key and multiply the result.
				// The multiplication is mandatory for very bad hash functions that do not distribute well their values (like libstdc++ default hash function for integers)
				return this->hash(std::forward<Args>(args)...);// *14313749767032793493ULL;
			}

			auto max_load_factor() const noexcept -> float
			{ 
				// Returns the maximum load factor
				return d_load_factor; 
			}
			void max_load_factor(float f) noexcept
			{ 
				// Set the maximum load factor
				// Load factor must be between 0.1 and 0.95
				d_load_factor = f;
				if (d_load_factor > 0.95f) d_load_factor = 0.95f;
				else if (d_load_factor < 0.1f ) d_load_factor = 0.1f;
				d_next_target = static_cast<size_t>(static_cast<double>(bucket_size()) * static_cast<double>(d_load_factor));
			}
			auto load_factor()const noexcept -> float
			{
				// Returns the current load factor
				return static_cast<float>(size()) / static_cast<float>(bucket_size());
			}
			auto bucket_size() const noexcept -> size_t
			{
				// Returns the node array size
				return  d_hash_mask+1; 
			}
			

			template< class... Args >
			auto find_linear_hash(size_t hash, Args&&... args) const -> const_iterator
			{
				// Search for given value considering he hash table to be purely linear (no robin hood probing and potential tombstones)
				check_hash_operation();
				size_t index = (hash & d_hash_mask);
				const tiny_hash h = node_type::small_hash(hash);

				while (SEQ_LIKELY(!d_buckets[index].null_for_search())) {
					if (!d_buckets[index].is_tombstone() && h == d_buckets[index].hash() && 
						(*this)(extract_key::key(d_buckets[index].value()), std::forward<Args>(args)...))
						return const_iterator(d_buckets[index].node(), d_buckets[index].pos());
					if (SEQ_UNLIKELY(++index == bucket_size()))
						index = 0;
				}
				return d_seq.end();
			}

			template< class... Args >
			SEQ_ALWAYS_INLINE auto find_hash(size_t hash, Args&&... args) const -> const_iterator
			{
				// Key lookup

				// Check for linear probing and dirty cases
				if (SEQ_UNLIKELY(d_max_dist == node_type::max_distance || dirty()))
					return find_linear_hash(hash, std::forward<Args>(args)...);

				size_t index = (hash & d_hash_mask);
				dist_type dist = 0;
				const tiny_hash h = node_type::small_hash(hash);

				// Probe until null node or until a node with a greater distance than the current one
				while (dist <= d_buckets[index].distance())
				{
					// Check for equality (first the hash part and then the key itself).
					if (h == d_buckets[index].hash() && (*this)(extract_key::key(d_buckets[index].value()), std::forward<Args>(args)...))
						return const_iterator(d_buckets[index].node(), d_buckets[index].pos());
					//if (SEQ_UNLIKELY(index++ == d_hash_mask))
					//	index = 0;
					index = (index + 1) & d_hash_mask;
					++dist;
				}
				// Failed lookup
				return d_seq.end();
			}
			template< class... Args >
			SEQ_ALWAYS_INLINE auto find(Args&&... args) const  -> const_iterator
			{
				return find_hash(hash_key(std::forward<Args>(args)...), std::forward<Args>(args)...);
			}

			template<Location loc, class... Args >
			auto insert_linear(Args&&... args) -> std::pair<iterator, bool>
			{
				// Insert value using linear probing

				using store_type = typename BuildValue<Value, Args&&...>::type;
				store_type value = BuildValue<Value, Args&&...>::build(std::forward<Args>(args)...);
				const size_t hash = hash_key(extract_key::key(value));
				size_t index = (hash & d_hash_mask);
				const tiny_hash h = node_type::small_hash(hash);

				// Pure linear hashing
				while (!d_buckets[index].null() && !d_buckets[index].is_tombstone()) {
					if (d_buckets[index].hash() == h && (*this)(extract_key::key(d_buckets[index].value()), extract_key::key(value)))
						return std::pair< iterator, bool>(iterator(d_buckets[index].node(), d_buckets[index].pos()), false);
					if (SEQ_UNLIKELY(++index == bucket_size()))
						index = 0;
				}
				iterator tmp_it = Inserter<sequence_type, loc, store_type>::insert(d_seq, value);
				d_buckets[index] = node_type(h, index == (hash & d_hash_mask) ? 0 : node_type::max_distance, tmp_it);
				return std::pair< iterator, bool>(tmp_it, true);
			}

			template<Location loc, class... Args >
			auto insert(Args&&... args) -> std::pair<iterator, bool>
			{
				// Insert new key

				// Check for dirty
				check_hash_operation();

				// Check for potential rehash.
				// Avoid rehashing if the maximum distance is below 7.
				if (SEQ_UNLIKELY(size() >= d_hash_mask || (d_max_dist > 7 && size() >= d_next_target) )) {
					rehash(size() * 2U);
				}

				// Check for purely linear hash table
				if (SEQ_UNLIKELY(d_max_dist == node_type::max_distance))
					return insert_linear<loc>(std::forward<Args>(args)...);
				
				// Build/extract value to insert (avoid building value on the stack if possible)
				using store_type = typename BuildValue<Value, Args&&...>::type;
				store_type value = BuildValue<Value, Args&&...>::build(std::forward<Args>(args)...);

				// Compute hash value
				const size_t hash = hash_key(extract_key::key(value));
				size_t index = (hash & d_hash_mask);
				dist_type dist = 0;
				tiny_hash h = node_type::small_hash(hash);

				// Find start insert location and check for equal value
				while (dist <= d_buckets[index].distance()) {
					if (SEQ_UNLIKELY(d_buckets[index].hash() == h && (*this)(extract_key::key(d_buckets[index].value()), extract_key::key(value))))
						return std::pair< iterator, bool>(iterator(d_buckets[index].node(), d_buckets[index].pos()), false);
					if (SEQ_UNLIKELY(++index == bucket_size()))
						index = 0;
					++dist;
				}

				// At this point, we are sure the key does not already exist in the table, let's proceed to the insertion

				// Update maximum distance
				if (SEQ_UNLIKELY(dist > d_max_dist))
					d_max_dist = dist;
				
				// Insert in sequence, might throw (fine)
				iterator tmp_it = Inserter<sequence_type, loc, store_type>::insert(d_seq, value);

				// Move nodes based on distance (robin hood hashing), only if computed distance is not null
				node_type n = d_buckets[index];
				d_buckets[index] = node_type(h, dist, tmp_it);
				if (dist ) 
					start_insert(d_buckets,bucket_size(), index, dist, n);
				
				return std::pair< iterator, bool>(tmp_it,true);
			}

			template<Location loc, class... Args >
			auto insert_no_check(Args&&... args) -> std::pair<iterator, bool>
			{
				// Insert new key without checking for existing equal key

				// Check for dirty
				check_hash_operation();

				// Check for potential rehash
				if (SEQ_UNLIKELY(size() >= d_hash_mask || (d_max_dist > 7 && size() >= d_next_target))) {
					rehash(size() * 2U);
				}

				// Check for purely linear hash map
				if (SEQ_UNLIKELY(d_max_dist == node_type::max_distance))
					return insert_linear<loc>(std::forward<Args>(args)...);

				// Build/extract value to insert (avoid copy if possible)
				using store_type = typename BuildValue<Value, Args&&...>::type;
				store_type value = BuildValue<Value, Args&&...>::build(std::forward<Args>(args)...);


				const size_t hash = hash_key(extract_key::key(value));
				size_t index = (hash & d_hash_mask);
				dist_type dist = 0;
				tiny_hash h = node_type::small_hash(hash);

				// Find start insert location and check for equal value
				while (dist <= d_buckets[index].distance()) {
					if (SEQ_UNLIKELY(++index == bucket_size()))
						index = 0;
					++dist;
				}

				// Update max distance
				if (SEQ_UNLIKELY(dist > d_max_dist))
					d_max_dist = dist;

				// Insert in sequence
				iterator tmp_it = Inserter<sequence_type, loc, store_type>::insert(d_seq, value);

				// Move nodes based on distance
				node_type n = d_buckets[index];
				d_buckets[index] = node_type(h, dist, tmp_it);
				if (dist)
					start_insert(d_buckets, bucket_size(), index, dist, n);

				return std::pair< iterator, bool>(tmp_it, true);
			}

			template<class Iter>
			void insert(Iter first, Iter last)
			{
				// Insert range of values.

				// Try to reserve the hash table ahead
				if (size_t count = seq::distance(first, last)) {
					reserve(size() + count);
				}
					
				// Insert one by one.
				// It would be faster in most scenario to insert everything in the sequence
				// and then rehashing while removing duplicates. However, it would be way slower in some scenarios,
				// like small range or lots of similar keys.
				for (; first != last; ++first)
					insert<Anywhere>(*first);
			}
			
			auto erase_hash(size_t hash, const_iterator it) -> iterator
			{
				// Erase key

				SEQ_ASSERT_DEBUG(it != d_seq.end(), "invalid erase position");

				// Check for dirty
				check_hash_operation();

				// Find the node corresponding to this iterator
				node_type* n = find_node(hash,it);
				node_type* prev = n++;
				const node_type* end = d_buckets + bucket_size();
				
				if (SEQ_UNLIKELY(n == end))
					n = d_buckets;

				if (SEQ_UNLIKELY(d_max_dist == node_type::max_distance))
				{
					// Pure linear hash map: use tombstone
					prev->empty_tombstone();
					return d_seq.erase(it);//return res;
				}
				
				// Robin hood backward shift deletion
				dist_type dist = n->distance();
				while (dist > 0) {
					*prev = *n;
					prev->set_distance(dist - 1);
					prev = n++;
					if (SEQ_UNLIKELY(n == end))
						n = d_buckets;
					dist = n->distance();
				}
				prev->empty();
				
				return d_seq.erase(it); //return res;
			}
			SEQ_ALWAYS_INLINE auto erase(const_iterator it) -> iterator
			{
				return erase_hash(hash_key(extract_key::key (*it)), it);
			}

			template<class K>
			auto erase(const K& key) -> size_t
			{
				size_t hash = hash_key(key);
				const_iterator it = find_hash(hash, key);
				if (it == d_seq.end()) return 0;
				erase_hash(hash, it);
				return 1;
			}

			auto erase(const_iterator first, const_iterator last) -> iterator
			{
				// Erase range of iterators. Most of the checks are performed in sequence::erase().
				// Works for dirty hash map
				iterator res = d_seq.erase(first, last);
				// force rehash
				rehash(0,true);
				return res;
			}

			void clear()
			{
				d_seq.clear();
				free_buckets(d_buckets);
				d_buckets = null_node();
				d_hash_mask = d_next_target = 0;
				d_max_dist = 1;
			}
		};



	}// end namespace detail




	/// @brief Associative container that contains a set of unique objects of type Key. Search, insertion, and removal have average constant-time complexity.
	/// @tparam Key Key type
	/// @tparam Hash Hash function
	/// @tparam KeyEqual Equality comparison function
	/// @tparam Allocator allocator object
	/// @tparam Layout memory layout used by the underlying sequence object
	/// 
	/// seq::ordered_set is a open addressing hash table using robin hood hashing and backward shift deletion. Its main properties are:
	///		-	Keys are ordered by insertion order. Therefore, ordered_set provides the additional members push_back(), push_front(), emplace_back() and emplace_front() to control key ordering.
	///		-	Since the container is ordered, it is also sortable. ordered_set provides the additional members sort() and stable_sort() for this purpose.
	///		-	The hash table itself basically stores iterators to a seq::sequence object storing the actual values. Therefore, seq::ordered_set provides <b>stable references
	///			and iterators, even on rehash</b> (unlike std::unordered_set that invalidates iterators on rehash).
	///		-	No memory peak on rehash.
	///		-	seq::ordered_set uses robin hood probing with backward shift deletion. It does not rely on tombstones and supports high load factors.
	///		-	It is fast and memory efficient compared to other node based hash tables (see section <b>Performances</b>), but still slower than most open addressing hash tables due to the additional indirection.
	/// 
	/// 
	/// Interface
	/// ---------
	/// 
	/// seq::ordered_set provides a similar interface to std::unordered_set with the following differences:
	///		-	The bucket related functions are not implemented,
	///		-	The default load factor is set to 0.6,
	///		-	Additional members push_back(), push_front(), emplace_back() and emplace_front() let you control the key ordering,
	///		-	Additional members sort() and stable_sort() let you sort the container,
	///		-	The member ordered_set::sequence() returns a reference to the underlying seq::sequence object,
	///		-	Its iterator and const_iterator types are bidirectional iterators.
	/// 
	/// The underlying sequence object stores plain non const Key objects. However, in order to avoid modifying the keys
	/// through iterators (and potentially invalidating the order), both iterator and const_iterator types can only return
	/// const references.
	/// 
	/// 
	/// Direct access to sequence
	/// ----------------------
	/// 
	/// Unlike most hash table implementations, it it possible to access and modify the underlying value storage directly (a seq::sequence object). 
	/// This possibility musy be used with great care, as modifying directly the sequence might break the hashing. 
	/// When calling the non-const version of ordered_set::sequence(), the ordered_set will be marked as dirty, and further attempts 
	/// to call functions like ordered_set::find() of ordered_set::insert() (functions based on hash value) will throw a std::logic_error.
	/// 
	/// Therefore, after finishing modifying the sequence, you must call ordered_set::rehash() to rehash the sequence, remove potential duplicates,
	/// and mark the ordered_set as non dirty anymore.
	/// 
	/// This way of modifying a ordered_set must be used carefully, but is way faster than multiple calls to ordered_set::insert() of ordered_set::erase().
	/// For instance, it is usually faster to insert values this way than reserving the hash table ahead, except when inserting lots of duplicate keys.
	/// Example:
	/// 
	/// \code{.cpp}
	/// std::vector<double> keys = ...
	/// 
	/// seq::ordered_set<double> set;
	/// for(size_t i=0; i < keys.size(); ++i)
	///		set.sequence().insert(keys[i]);
	/// 
	/// // rehash the set and remove potential duplicate values in a stable way
	/// set.rehash();
	/// \endcode
	/// 
	/// 
	/// Exception guarantee
	/// -------------------
	/// 
	/// Most members of seq::ordered_set provide <b>strong exception guarantee</b>, except if specified otherwise (mentionned in function documentation).
	/// 
	/// 
	/// Growth policy and load factor
	/// -----------------------------
	/// 
	/// seq::ordered_set uses a growth factor of 2 to use the fast modulo. The hash table size is multiplied by 2 each time the table load factor exceeds
	/// the given max_load_factor(). The default maximum load factor is set to 0.6 and can by set up to 0.95, which is well supported thanks to the robin hood hashing.
	/// 
	/// In some cases, the actual load factor can exceed the provided maximum load factor. This holds when the keys are very well distributed, and the maximum 
	/// distance of a key to its computed location is low (below 8). This strategy avoids some unnecessary rehash for very strong hash function (or well distributed keys).
	/// Note however that the load factor will never exceed 0.95.
	/// 
	/// On rehash, the old table is deallocated before allocating the new (twice as big) one. This is possible thanks to the double storage strategy (values are stored in an 
	/// independant seq::sequence object) and avoid memory peaks.
	/// 
	/// 
	/// Handling of bad hash function
	/// -----------------------------
	/// 
	/// Like most robin hood based hash tables (ska::flat_hash_set, tsl::robin_set...), seq::ordered_set uses 8 bits to store the node distance to 
	/// its computed location. For a bad hash function leading to strong clustering, this 8 bits distance quickly overflows. The usual strategy in this
	/// case is to rehash the table based on the growth strategy, hopping for a better key distribution.
	/// 
	/// For very bad hash function (or in case of DOS attack), hash tables like ska::flat_hash_set and tsl::robin_set will keep rehashing its values and reallocating
	/// the table, until a fatal std::bad_alloc is thrown. seq::ordered_set uses a different strategy to cope with such situation:
	///		-	When the distance value overflows, it is discarded and the table relies on pure linear hashing.
	///		-	In this case, deleting an entry will create a tombstone instead of the standard backward shift deletion.
	///		-	The linear probing behavior is kept until a call to ordered_set::rehash() that might switch back the behavior to robin hood hashing.
	/// 
	/// Therefore, a (very) bad hash function will only make the table slower but will never explode with a std::bad_alloc exception
	/// 
	/// 
	/// Deleting entries
	/// ----------------
	/// 
	/// seq::ordered_set uses backward shift deletion to avoid introducing tombstone, except for bad hash functions (see section above).
	/// The key lookup remains fast when deleting lots of entries, and mixed scenarios involving lots of interleaved insertion/deletion
	/// are well supported.
	/// 
	/// Note however that removing a key will never trigger a rehash. The user is free to rehash the ordered_set at any point using ordered_set::rehash() member.
	///
	/// 
	/// Sorting
	/// -------
	/// 
	/// Since the ordered_set is ordered by key insertion order, it makes sense to provide a function to sort it.
	/// The members ordered_set::sort() and ordered_set::stable_sort() are provided and call respectively seq::sequence::sort() and seq::sequence::stable_sort().
	/// These functions rehash the full table after sorting.
	/// 
	/// 
	/// Performances
	/// ------------
	/// 
	/// Performances of seq::ordered_set has been measured and compared to other node based hash tables: std::unordered_set, <a href="https://github.com/skarupke/flat_hash_map/blob/master/unordered_map.hpp">ska::unordered_set</a>,
	/// <a href="https://github.com/martinus/robin-hood-hashing">robin_hood::unordered_node_set</a>, <a href="https://github.com/greg7mdp/parallel-hashmap">phmap::node_hash_set</a> (based on abseil hash table) and
	/// <a href="https://www.boost.org/doc/libs/1_51_0/doc/html/boost/unordered_set.html">boost::unordered_set</a>.
	/// The following table show the results when compiled with gcc 10.1.0 (-O3) for msys2 on Windows 10,
	/// using Intel(R) Core(TM) i7-10850H at 2.70GHz. Measured operations are:
	///		-	Insert successfully 5M unique double randomly shuffled in an empty table using hash_table::insert()
	///		-	Insert successfully 5M unique double randomly shuffled in an empty table using hash_table::insert() after reserving enough space
	///		-	Successfully search for 5M double in random order using hash_table::find(const Key&)
	///		-	Search for 5M double not present in the table (failed lookup)
	///		-	Walk through the full table (5M double) using iterators
	///		-	Erase half the table in random order using hash_table::erase(iterator)
	///		-	Perform successfull lookups on the remaining 2.5M keys.
	/// 
	/// For each tested hash table, the maximum load factor is left to its default value, and std::hash<double> is used. For insert and erase operations,
	/// the program memory consumption is given. Note that the memory consumption is not the exact memory usage of the hash table, and should only be used
	/// to measure the difference between implementations.
	/// 
	/// Hash table name               |       Insert       |  Insert(reserve)   |Find (success) | Find (failed) |    Iterate    |       Erase        |  Find again   |
	/// ------------------------------|--------------------|--------------------|---------------|---------------|---------------|--------------------|---------------|
	/// seq::ordered_set              |   461 ms/145 MO    |   310 ms/145 MO    |    321 ms     |    177 ms     |     5 ms      |   462 ms/222 MO    |    203 ms     |
	/// phmap::node_hash_set          |   975 ms/188 MO    |   489 ms/187 MO    |    438 ms     |    132 ms     |     95 ms     |   732 ms/264 MO    |    250 ms     |
	/// robin_hood::unordered_node_set|   583 ms/182 MO    |   242 ms/149 MO    |    341 ms     |    142 ms     |     83 ms     |   379 ms/258 MO    |    224 ms     |
	/// ska::unordered_set            |   1545 ms/257 MO   |   774 ms/256 MO    |    324 ms     |    258 ms     |    128 ms     |   613 ms/333 MO    |    238 ms     |
	/// boost::unordered_set          |   1708 ms/257 MO   |   901 ms/257 MO    |    571 ms     |    532 ms     |    262 ms     |   1073 ms/333 MO   |    405 ms     |
	/// std::unordered_set            |   1830 ms/238 MO   |   1134 ms/232 MO   |    847 ms     |    878 ms     |    295 ms     |   1114 ms/315 MO   |    646 ms     |
	///
	/// This benchmark is available in file 'seq/benchs/bench_hash.hpp'.
	/// seq::ordered_set is among the fastest hashtable for each operation except for failed lookup, and has a lower memory overhead.
	/// Note that this benchmark does not represent all possible workloads, and additional tests must be fullfilled for specific scenarios.
	/// 
	/// seq::ordered_set uses internally and if possible compressed pointers to reduce its memory footprint. In such case, the last 16 bits of a pointer are used to store 
	/// metadata. Situations where this is not possible are detected at compile time, but it is possible to manually disable this optimization by defining SEQ_NO_COMPRESSED_PTR.
	/// 
	template<
		class Key,
		class Hash = std::hash<Key>,
		class KeyEqual = std::equal_to<Key>,
		class Allocator = std::allocator<Key>,
		LayoutManagement Layout = OptimizeForSpeed
	>
	class ordered_set : private detail::SparseFlatNodeHashTable<Key,Key,Hash,KeyEqual,Allocator, Layout>
	{
		using base_type = detail::SparseFlatNodeHashTable<Key, Key, Hash, KeyEqual, Allocator, Layout>;
		using this_type = ordered_set<Key, Hash, KeyEqual, Allocator, Layout>;
		
		template <typename U>
		using has_is_transparent = detail::has_is_transparent<U>;

	public:

		using sequence_type = typename base_type::sequence_type;

		struct const_iterator
		{
			using iter_type = typename sequence_type::const_iterator;
			using value_type = Key;
			using iterator_category = std::bidirectional_iterator_tag;
			using size_type = size_t;
			using difference_type = std::ptrdiff_t;
			using pointer = const value_type*;
			using reference = const value_type&;
			iter_type iter;

			const_iterator() {}
			const_iterator(iter_type it) : iter(it) {}
			SEQ_ALWAYS_INLINE auto operator++() noexcept -> const_iterator& {
				++iter;
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator++(int) noexcept -> const_iterator {
				const_iterator _Tmp = *this;
				++(*this);
				return _Tmp;
			}
			SEQ_ALWAYS_INLINE auto operator--() noexcept -> const_iterator& {
				--iter;
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator--(int) noexcept -> const_iterator {
				const_iterator _Tmp = *this;
				--(*this);
				return _Tmp;
			}
			SEQ_ALWAYS_INLINE auto operator*() const noexcept -> reference { return *iter; }
			SEQ_ALWAYS_INLINE auto operator->() const noexcept -> pointer { return  std::pointer_traits<pointer>::pointer_to(**this); }
			SEQ_ALWAYS_INLINE auto operator+=(difference_type diff) noexcept -> const_iterator& {
				iter += diff;
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator-=(difference_type diff) noexcept -> const_iterator& {
				iter -= diff;
				return *this;
			}
			SEQ_ALWAYS_INLINE bool operator==(const const_iterator& it) const noexcept { return iter == it.iter; }
			SEQ_ALWAYS_INLINE bool operator!=(const const_iterator& it) const noexcept { return iter != it.iter; }
			SEQ_ALWAYS_INLINE bool operator<(const const_iterator& it) const noexcept { return iter < it.iter; }
			SEQ_ALWAYS_INLINE bool operator>(const const_iterator& it) const noexcept { return iter > it.iter; }
			SEQ_ALWAYS_INLINE bool operator<=(const const_iterator& it) const noexcept { return iter <= it.iter; }
			SEQ_ALWAYS_INLINE bool operator>=(const const_iterator& it) const noexcept { return iter >= it.iter; }
			SEQ_ALWAYS_INLINE auto operator+(difference_type diff) const noexcept -> const_iterator { return iter + diff; }
			SEQ_ALWAYS_INLINE auto operator-(difference_type diff) const noexcept -> const_iterator { return iter + diff; }
			SEQ_ALWAYS_INLINE auto operator-(const const_iterator& other) const noexcept -> difference_type { return iter - other.iter; }
		};

		using iterator = const_iterator;
		using key_type = Key;
		using value_type = Key;
		using allocator_type = Allocator;
		using size_type = size_t;
		using difference_type = std::ptrdiff_t;
		using hasher = Hash;
		using key_equal = KeyEqual;
		using reference	= value_type&;
		using const_reference= const value_type&;
		using pointer= typename std::allocator_traits<Allocator>::pointer;
		using const_pointer = typename std::allocator_traits<Allocator>::const_pointer;
		using reverse_iterator = std::reverse_iterator<iterator>;
		using const_reverse_iterator = std::reverse_iterator<const_iterator>;

		/// @brief Constructs empty container. Sets max_load_factor() to 0.6.
		/// @param hash hash function to use
		/// @param equal comparison function to use for all key comparisons of this container
		/// @param alloc allocator to use for all memory allocations of this container
		ordered_set(const Hash& hash = Hash(),
			const KeyEqual& equal = KeyEqual(),
			const Allocator& alloc = Allocator()) noexcept
			:base_type(hash, equal, alloc)
		{}
		/// @brief Constructs empty container. Sets max_load_factor() to 0.6.
		/// @param alloc allocator to use for all memory allocations of this container
		explicit ordered_set(const Allocator& alloc)
			:ordered_set(Hash(), KeyEqual(), alloc)
		{}
		/// @brief constructs the container with the contents of the range [first, last). Sets max_load_factor() to 0.6. 
		/// If multiple elements in the range have keys that compare equivalent, only the first occurence is inserted.
		/// Input iteration order is preserved.
		/// @tparam InputIt iterator type
		/// @param first the range to copy the elements from
		/// @param last the range to copy the elements from
		/// @param hash hash function to use
		/// @param equal comparison function to use for all key comparisons of this container
		/// @param alloc allocator to use for all memory allocations of this container
		template< class InputIt >
		ordered_set(InputIt first, InputIt last,
			const Hash& hash = Hash(),
			const key_equal& equal = key_equal(),
			const Allocator& alloc = Allocator())
			: base_type(hash, equal, alloc)
		{
			insert(first, last);
		}
		/// @brief constructs the container with the contents of the range [first, last). Sets max_load_factor() to 0.6. 
		/// If multiple elements in the range have keys that compare equivalent, only the first occurence is inserted.
		/// Input iteration order is preserved.
		/// @tparam InputIt iterator type
		/// @param first the range to copy the elements from
		/// @param last the range to copy the elements from
		/// @param alloc allocator to use for all memory allocations of this container
		template< class InputIt >
		ordered_set(InputIt first, InputIt last,
			const Allocator& alloc)
			: ordered_set(first, last, Hash(), key_equal(), alloc)
		{}
		/// @brief constructs the container with the contents of the range [first, last). Sets max_load_factor() to 0.6. 
		/// If multiple elements in the range have keys that compare equivalent, only the first occurence is inserted.
		/// Input iteration order is preserved.
		/// @tparam InputIt iterator type
		/// @param first the range to copy the elements from
		/// @param last the range to copy the elements from
		/// @param hash hash function to use
		/// @param alloc allocator to use for all memory allocations of this container
		template< class InputIt >
		ordered_set(InputIt first, InputIt last,
			const Hash& hash,
			const Allocator& alloc)
			: ordered_set(first, last, hash, key_equal(), alloc)
		{}
		/// @brief Copy constructor
		/// @param other another container to be used as source to initialize the elements of the container with
		/// @param alloc allocator to use for all memory allocations of this container
		ordered_set(const ordered_set& other, const Allocator& alloc)
			:base_type(other.hash_function(), other.key_eq(), alloc)
		{
			this->d_seq.resize(other.size());
			std::copy(other.sequence().begin(), other.sequence().end(), this->d_seq.begin());
			if (other.dirty()) base_type::mark_dirty();
			max_load_factor(other.max_load_factor());
			rehash();
		}
		/// @brief Copy constructor
		/// @param other another container to be used as source to initialize the elements of the container with
		ordered_set(const ordered_set& other)
			:ordered_set(other, copy_allocator(other.get_allocator()))
		{}
		/// @brief Move constructor
		/// @param other another container to be used as source to initialize the elements of the container with
		ordered_set(ordered_set&& other)
			:base_type(std::move(other))
		{}
		/// @brief Move constructor
		/// @param other another container to be used as source to initialize the elements of the container with
		/// @param alloc allocator to use for all memory allocations of this container
		ordered_set(ordered_set&& other, const Allocator& alloc)
			:base_type(std::move(other), alloc)
		{}
		ordered_set(std::initializer_list<value_type> init,
			const Hash& hash = Hash(),
			const key_equal& equal = key_equal(),
			const Allocator& alloc = Allocator())
			:ordered_set(init.begin(), init.end(), hash, equal, alloc)
		{}
		/// @brief constructs the container with the contents of the initializer list init, same as ordered_set(init.begin(), init.end())
		/// @param init initializer list to initialize the elements of the container with
		/// @param hash hash function to use
		/// @param alloc allocator to use for all memory allocations of this container
		ordered_set(std::initializer_list<value_type> init,
			const Hash& hash,
			const Allocator& alloc)
			:ordered_set(init.begin(), init.end(), hash, alloc)
		{}
		/// @brief constructs the container with the contents of the initializer list init, same as ordered_set(init.begin(), init.end())
		/// @param init initializer list to initialize the elements of the container with
		/// @param alloc allocator to use for all memory allocations of this container
		ordered_set(std::initializer_list<value_type> init,
			const Allocator& alloc)
			:ordered_set(init.begin(), init.end(), alloc)
		{}


		/// @brief Copy assignment operator
		auto operator=(const ordered_set& other) -> ordered_set&
		{
			if (this != std::addressof(other)) {
				this->d_seq = other.sequence();
				if (other.dirty()) this->mark_dirty();
				max_load_factor(other.max_load_factor());
				rehash();
			}
			return *this;
		}

		/// @brief Move assignment operator
		auto operator=( ordered_set&& other) noexcept -> ordered_set&
  		{
			base_type::swap(other);
			return *this;
		}

		/// @brief Returns the container size
		auto size() const noexcept -> size_t { return this->d_seq.size(); }
		/// @brief Returns the container maximum size
		auto max_size() const noexcept -> size_t { return this->d_seq.max_size(); }
		/// @brief Returns true if the container is empty, false otherwise
		auto empty() const noexcept -> bool { return this->d_seq.empty(); }
		/// @brief Returns the current maximum possible probe distance
		auto max_probe_distance() const noexcept -> int { return this->d_max_dist; }
		/// @brief Returns the current load factor
		auto load_factor() const noexcept -> float { return base_type::load_factor(); }
		/// @brief Returns the current maximum load factor
		auto max_load_factor() const noexcept -> float { return base_type::max_load_factor(); }
		/// @brief Set the maximum load factor
		void max_load_factor(float f) noexcept { base_type::max_load_factor(f); }
		/// @brief Returns the container allocator object
		auto get_allocator() noexcept -> allocator_type& { return this->d_seq.get_allocator(); }
		/// @brief Returns the container allocator object
		auto get_allocator() const noexcept -> const allocator_type & { return this->d_seq.get_allocator(); }
		/// @brief Returns the hash function
		auto hash_function() const -> hasher { return this->base_type::hash_function(); }
		/// @brief Returns the equality comparison function
		auto key_eq() const -> key_equal { return this->base_type::key_eq(); }
		/// @brief Returns the underlying sequence object.
		/// Calling this function will mark the container as dirty. Any further attempts to call members like find() or insert() (relying on the hash function)
		/// will raise a std::logic_error. To mark the container as non dirty anymore, the user must call ordered_set::rehash().
		/// @return a reference to the underlying seq::sequence object
		auto sequence() noexcept -> sequence_type& { this->base_type::mark_dirty(); return this->d_seq; }
		/// @brief Returns the underlying sequence object. Do NOT mark the container as dirty.
		auto sequence() const noexcept -> const sequence_type & { return this->d_seq; }
		/// @brief Returns the underlying sequence object. Do NOT mark the container as dirty.
		auto csequence() const noexcept -> const sequence_type & { return this->d_seq; }
		/// @brief Returns an iterator to the first element of the container.
		auto end() noexcept -> iterator { return this->d_seq.end(); }
		/// @brief Returns an iterator to the first element of the container.
		auto end() const noexcept -> const_iterator { return this->d_seq.end(); }
		/// @brief Returns an iterator to the first element of the container.
		auto cend() const noexcept -> const_iterator { return this->d_seq.end(); }
		/// @brief Returns an iterator to the element following the last element of the container.
		auto begin() noexcept -> iterator { return this->d_seq.begin(); }
		/// @brief Returns an iterator to the element following the last element of the container.
		auto begin() const noexcept -> const_iterator { return this->d_seq.begin(); }
		/// @brief Returns an iterator to the element following the last element of the container.
		auto cbegin() const noexcept -> const_iterator { return this->d_seq.begin(); }
		/// @brief Returns a reverse iterator to the first element of the reversed list.
		auto rbegin() noexcept -> reverse_iterator { return this->d_seq.rbegin(); }
		/// @brief Returns a reverse iterator to the first element of the reversed list.
		auto rbegin() const noexcept -> const_reverse_iterator  { return this->d_seq.rbegin(); }
		/// @brief Returns a reverse iterator to the first element of the reversed list.
		auto crbegin() const noexcept -> const_reverse_iterator  { return this->d_seq.rbegin(); }
		/// @brief Returns a reverse iterator to the element following the last element of the reversed list.
		auto rend() noexcept -> reverse_iterator { return this->d_seq.rend(); }
		/// @brief Returns a reverse iterator to the element following the last element of the reversed list.
		auto rend() const noexcept -> const_reverse_iterator { return this->d_seq.rend(); }
		/// @brief Returns a reverse iterator to the element following the last element of the reversed list.
		auto crend() const noexcept -> const_reverse_iterator { return this->d_seq.rend(); }
		/// @brief Clear the container
		void clear() {this->base_type::clear();}

		/// @brief Rehash the container.
		/// This function triggers a full rehash if:
		/// 
		///		-	the container is dirty (through a call to ordered_set::sequence()
		///		-	the hash table size is too big or too small considering the max load factor (for instance after several calls to ordered_set::erase())
		///		-	the hash table is in linear hashing state (because of bad hash function). In this case, the hash table size is doubled.
		/// 
		/// Otherwise, this function does nothing.
		/// 
		void rehash() {this->base_type::rehash();}
		
		/// @brief Sets the number of nodes to the number needed to accomodate at least count elements without exceeding maximum load factor and rehashes the container.
		/// @param count new capacity of the container
		void reserve(size_t count) {this->base_type::reserve(count);}

		/// @brief Sort the container based on given comparator.
		/// The full container is rehashed afterward.
		/// Invalidate all references and iterators.
		/// Basic exception guarantee only.
		/// @param le comparison function
		template<class Less>
		void sort(Less le)
		{
			sequence().sort(le);
			rehash();
		}
		/// @brief Sort the container using std::less<Key>.
		/// The full container is rehashed afterward.
		/// Invalidate all references and iterators.
		/// Basic exception guarantee only.
		void sort()
		{
			sequence().sort();
			rehash();
		}
		/// @brief Sort the container based on given comparator and using std::stable_sort.
		/// The full container is rehashed afterward.
		/// Invalidate all references and iterators.
		/// Basic exception guarantee only.
		/// @param le comparison function
		template<class Less>
		void stable_sort(Less le)
		{
			sequence().stable_sort(le);
			rehash();
		}
		/// @brief Sort the container based on std::less<Key> and using std::stable_sort.
		/// The full container is rehashed afterward.
		/// Invalidate all references and iterators.
		/// Basic exception guarantee only.
		void stable_sort()
		{
			sequence().stable_sort();
			rehash();
		}

		/// @brief Calls seq::sequence::shrink_to_fit().
		/// Remove potential holes in the sequence object due to calls to ordered_set::erase().
		/// Does not allocate memory, except for the hash table itself.
		/// Invalidate all references and iterators.
		/// Basic exception guarantee only.
		void shrink_to_fit()
		{
			sequence().shrink_to_fit();
			rehash();
		}

		/// @brief Swap this container with other
		void swap(ordered_set& other)
		{
			base_type::swap(other);
		}

		/// @brief Inserts a new element into the container constructed in-place with the given args if there is no element with the key in the container.
		/// Careful use of emplace allows the new element to be constructed while avoiding unnecessary copy or move operations. 
		/// The constructor of the new element is called with exactly the same arguments as supplied to emplace, forwarded via std::forward<Args>(args).... 
		/// The element may be constructed even if there already is an element with the key in the container, in which case the newly constructed element will be destroyed immediately.
		/// Iterators and references are not invalidated. Rehashing occurs only if the new number of elements is greater than max_load_factor()*size().
		/// 
		/// This function calls seq::sequence::insert() which insert the new element anywhere in the sequence, trying to fill holes left by calls to ordered_set::erase().
		/// 
		/// @return pair consisting of an iterator to the inserted element, or the already-existing element if no insertion happened, 
		/// and a bool denoting whether the insertion took place (true if insertion happened, false if it did not).
		template< class... Args >
		auto emplace(Args&&... args) -> std::pair<iterator, bool>
		{
			return this->base_type::template insert<detail::Anywhere>(std::forward<Args>(args)...);
		}
		/// @brief Inserts a new element into the container constructed in-place with the given args if there is no element with the key in the container.
		/// Same as ordered_set::emplace().
		template <class... Args>
		auto emplace_hint(const_iterator hint, Args&&... args) -> iterator
		{
			(void)hint;
			return this->base_type::template insert<detail::Anywhere>(std::forward<Args>(args)...).first;
		}
		/// @brief Inserts element into the container, if the container doesn't already contain an element with an equivalent key.
		/// Iterators and references are not invalidated. Rehashing occurs only if the new number of elements is greater than max_load_factor()*size().
		/// This function calls seq::sequence::insert() which insert the new element anywhere in the sequence, trying to fill holes left by calls to ordered_set::erase().
		/// Iterators and references are not invalidated. Rehashing occurs only if the new number of elements is greater than max_load_factor()*size().
		/// @param value value to be inserted
		/// @return pair consisting of an iterator to the inserted element, or the already-existing element if no insertion happened, 
		/// and a bool denoting whether the insertion took place (true if insertion happened, false if it did not).
		auto insert(const value_type& value) -> std::pair<iterator, bool>
		{
			return this->base_type::template insert<detail::Anywhere>(value);
		}
		/// @brief Inserts element into the container using move semantic, if the container doesn't already contain an element with an equivalent key.
		/// Iterators and references are not invalidated. Rehashing occurs only if the new number of elements is greater than max_load_factor()*size().
		/// This function calls seq::sequence::insert() which insert the new element anywhere in the sequence, trying to fill holes left by calls to ordered_set::erase().
		/// Iterators and references are not invalidated. Rehashing occurs only if the new number of elements is greater than max_load_factor()*size().
		/// @param value value to be inserted
		/// @return pair consisting of an iterator to the inserted element, or the already-existing element if no insertion happened, 
		/// and a bool denoting whether the insertion took place (true if insertion happened, false if it did not).
		auto insert( value_type&& value) -> std::pair<iterator, bool>
		{
			return this->base_type::template insert<detail::Anywhere>(std::move(value));
		}
		/// @brief Inserts element into the container, if the container doesn't already contain an element with an equivalent key.
		/// Same as ordered_set::insert().
		auto insert(const_iterator hint, const value_type& value) -> iterator
		{
			(void)hint;
			return insert(value).first;
		}
		/// @brief Inserts element into the container using move semantic, if the container doesn't already contain an element with an equivalent key.
		/// Same as ordered_set::insert().
		auto insert(const_iterator hint, value_type&& value) -> iterator
		{
			(void)hint;
			return insert(std::move(value)).first;
		}
		/// @brief Inserts elements from range [first, last). If multiple elements in the range have keys that compare equivalent, only the first occurence is inserted.
		/// Elements are inserted in any order.
		/// Iterators and references are not invalidated. Rehashing occurs only if the new number of elements is greater than max_load_factor()*size().
		/// @param first range of elements to insert
		/// @param last range of elements to insert
		template< class InputIt >
		void insert(InputIt first, InputIt last)
		{
			this->base_type::insert(first, last);
		}
		/// @brief Inserts elements from range [init.begin(), init.end()). If multiple elements in the range have keys that compare equivalent, only the first occurence is inserted.
		/// Elements are inserted in any order.
		/// Iterators and references are not invalidated. Rehashing occurs only if the new number of elements is greater than max_load_factor()*size().
		/// @param first range of elements to insert
		/// @param last range of elements to insert
		void insert(std::initializer_list<value_type> ilist)
		{
			insert(ilist.begin(), ilist.end());
		}



		/// @brief Inserts a new element at the back of the container, constructed in-place with the given args if there is no element with the key in the container.
		/// Careful use of emplace_back allows the new element to be constructed while avoiding unnecessary copy or move operations. 
		/// The constructor of the new element is called with exactly the same arguments as supplied to emplace_back, forwarded via std::forward<Args>(args).... 
		/// The element may be constructed even if there already is an element with the key in the container, in which case the newly constructed element will be destroyed immediately.
		/// Iterators and references are not invalidated. Rehashing occurs only if the new number of elements is greater than max_load_factor()*size().
		/// 
		/// @return pair consisting of an iterator to the inserted element, or the already-existing element if no insertion happened, 
		/// and a bool denoting whether the insertion took place (true if insertion happened, false if it did not).
		template< class... Args >
		auto emplace_back(Args&&... args) -> std::pair<iterator, bool>
		{
			return this->base_type::template insert<detail::Back>(std::forward<Args>(args)...);
		}
		/// @brief Inserts element at the back of the container, if the container doesn't already contain an element with an equivalent key.
		/// Iterators and references are not invalidated. Rehashing occurs only if the new number of elements is greater than max_load_factor()*size().
		/// @param value value to be inserted
		/// @return pair consisting of an iterator to the inserted element, or the already-existing element if no insertion happened, 
		/// and a bool denoting whether the insertion took place (true if insertion happened, false if it did not).
		auto push_back(const value_type& value) -> std::pair<iterator, bool>
		{
			return this->base_type::template insert<detail::Back>(value);
		}
		/// @brief Inserts element at the back of the container use move semantic, if the container doesn't already contain an element with an equivalent key.
		/// Iterators and references are not invalidated. Rehashing occurs only if the new number of elements is greater than max_load_factor()*size().
		/// @param value value to be inserted
		/// @return pair consisting of an iterator to the inserted element, or the already-existing element if no insertion happened, 
		/// and a bool denoting whether the insertion took place (true if insertion happened, false if it did not).
		auto push_back(value_type&& value) -> std::pair<iterator, bool>
		{
			return this->base_type::template insert<detail::Back>(std::move(value));
		}



		/// @brief Inserts a new element at the front of the container, constructed in-place with the given args if there is no element with the key in the container.
		/// Careful use of emplace_front allows the new element to be constructed while avoiding unnecessary copy or move operations. 
		/// The constructor of the new element is called with exactly the same arguments as supplied to emplace_front, forwarded via std::forward<Args>(args).... 
		/// The element may be constructed even if there already is an element with the key in the container, in which case the newly constructed element will be destroyed immediately.
		/// Iterators and references are not invalidated. Rehashing occurs only if the new number of elements is greater than max_load_factor()*size().
		/// 
		/// @return pair consisting of an iterator to the inserted element, or the already-existing element if no insertion happened, 
		/// and a bool denoting whether the insertion took place (true if insertion happened, false if it did not).
		template< class... Args >
		auto emplace_front(Args&&... args) -> std::pair<iterator, bool>
		{
			return this->base_type::template insert<detail::Front>(std::forward<Args>(args)...);
		}
		/// @brief Inserts element at the front of the container, if the container doesn't already contain an element with an equivalent key.
		/// Iterators and references are not invalidated. Rehashing occurs only if the new number of elements is greater than max_load_factor()*size().
		/// @param value value to be inserted
		/// @return pair consisting of an iterator to the inserted element, or the already-existing element if no insertion happened, 
		/// and a bool denoting whether the insertion took place (true if insertion happened, false if it did not).
		auto push_front(const value_type& value) -> std::pair<iterator, bool>
		{
			return this->base_type::template insert<detail::Front>(value);
		}
		/// @brief Inserts element at the front of the container using move semantic, if the container doesn't already contain an element with an equivalent key.
		/// Iterators and references are not invalidated. Rehashing occurs only if the new number of elements is greater than max_load_factor()*size().
		/// @param value value to be inserted
		/// @return pair consisting of an iterator to the inserted element, or the already-existing element if no insertion happened, 
		/// and a bool denoting whether the insertion took place (true if insertion happened, false if it did not).
		auto push_front(value_type&& value) -> std::pair<iterator, bool>
		{
			return this->base_type::template insert<detail::Front>(std::move(value));
		}


		/// @brief Erase element at given location.
		/// Iterators and references are not invalidated. Rehashing never occurs.
		/// @param pos iterator to the element to erase
		/// @return iterator to the next element
		auto erase(const_iterator pos) -> iterator
		{
			return this->base_type::erase(pos.iter);
		}
		/// @brief Erase element comparing equal to given key (if any).
		/// Iterators and references are not invalidated. Rehashing never occurs.
		/// @param key key to be erased
		/// @return number of erased elements (0 or 1)
		auto erase(const Key& key) -> size_type
		{
			return this->base_type::erase(key);
		}
		/// @brief Erase element comparing equal to given key (if any).
		/// Removes the element (if one exists) with key that compares equivalent to the value x. 
		/// This overload participates in overload resolution only if Hash::is_transparent and KeyEqual::is_transparent are valid and each denotes a type, 
		/// and neither iterator nor const_iterator is implicitly convertible from K. 
		/// This assumes that such Hash is callable with both K and Key type, and that the KeyEqual is transparent, which, together, allows calling this function without constructing an instance of Key.
		/// @return number of erased elements (0 or 1)
		template <class K, class KE = KeyEqual, class H = Hash,
			typename std::enable_if<has_is_transparent<KE>::value&& has_is_transparent<H>::value>::type* = nullptr>
		auto erase(const K& x) -> size_type
		{
			return this->base_type::erase(x);
		}
		/// @brief Removes the elements in the range [first; last), which must be a valid range in *this.
		/// @param first range of elements to remove
		/// @param last range of elements to remove
		/// @return Iterator following the last removed element
		auto erase(const_iterator first, const_iterator last) -> iterator
		{
			return this->base_type::erase(first.iter,last.iter);
		}
		

		/// @brief Finds an element with key equivalent to key
		/// @param value key value to search for
		/// @return iterator pointing to found key on success, end iterator on failure.
		SEQ_ALWAYS_INLINE auto find(const Key& key) const -> const_iterator
		{
			return this->base_type::find(key);
		}
		/// @brief Finds an element with key equivalent to key
		/// @param value key value to search for
		/// @return iterator pointing to found key on success, end iterator on failure.
		SEQ_ALWAYS_INLINE auto find(const Key& value) -> iterator
		{
			return static_cast<iterator>(const_cast<this_type*>(this)->base_type::find(value));
		}
		/// @brief Finds an element with key equivalent to key.
		/// Finds an element with key that compares equivalent to the value x. 
		/// This overload participates in overload resolution only if Hash::is_transparent and KeyEqual::is_transparent are valid and each denotes a type. 
		/// This assumes that such Hash is callable with both K and Key type, and that the KeyEqual is transparent, which, together, allows calling this function without constructing an instance of Key.
		/// @param value key value to search for
		/// @return iterator pointing to found key on success, end iterator on failure.
		template <class K, class KE = KeyEqual, class H = Hash,
			typename std::enable_if<has_is_transparent<KE>::value && has_is_transparent<H>::value>::type* = nullptr>
		SEQ_ALWAYS_INLINE auto find(const K& x) const -> const_iterator
		{
			return const_cast<this_type*>(this)->base_type::find(x);
		}
		/// @brief Finds an element with key equivalent to key.
		/// Finds an element with key that compares equivalent to the value x. 
		/// This overload participates in overload resolution only if Hash::is_transparent and KeyEqual::is_transparent are valid and each denotes a type. 
		/// This assumes that such Hash is callable with both K and Key type, and that the KeyEqual is transparent, which, together, allows calling this function without constructing an instance of Key.
		/// @param value key value to search for
		/// @return iterator pointing to found key on success, end iterator on failure.
		template <class K, class KE = KeyEqual, class H = Hash,
			typename std::enable_if<has_is_transparent<KE>::value&& has_is_transparent<H>::value>::type* = nullptr>
		SEQ_ALWAYS_INLINE auto find(const K& key) -> iterator
		{
			return static_cast<iterator>(const_cast<this_type*>(this)->base_type::find(key));
		}

		/// @brief Returns 1 of key exists, 0 otherwise
		auto count(const Key& key) const -> size_type
		{
			return find(key) != end();
		}
		/// @brief Returns 1 of key exists, 0 otherwise
		template <class K, class KE = KeyEqual, class H = Hash,
			typename std::enable_if<has_is_transparent<KE>::value&& has_is_transparent<H>::value>::type* = nullptr>
		auto count(const K& key) const -> size_type
		{
			return find(key) != end();
		}

		/// @brief Returns true of key exists, false otherwise
		bool contains(const Key& key) const
		{
			return find(key) != end();
		}
		/// @brief Returns true of key exists, false otherwise
		template <class K, class KE = KeyEqual, class H = Hash,
			typename std::enable_if<has_is_transparent<KE>::value&& has_is_transparent<H>::value>::type* = nullptr>
		bool contains(const K& key) const
		{
			return find(key) != end();
		}

	};









	/// @brief Associative container that contains key-value pairs with unique keys. Search, insertion, and removal of elements have average constant-time complexity.
	/// @tparam Key Key type
	/// @tparam T mapped type
	/// @tparam Hash Hash function
	/// @tparam KeyEqual Equality comparison function
	/// @tparam Allocator allocator object
	/// @tparam Layout memory layout used by the underlying sequence object
	/// 
	/// seq::ordered_map is a hash table using robin hood hashing and backward shift deletion. Its main properties are:
	///		-	Keys are ordered by insertion order. Therefore, ordered_set provides the additional members push_back(), push_front(), emplace_back() and emplace_front() to constrol key ordering.
	///		-	Since the container is ordered, it is also sortable. ordered_set provides the additional members sort() and stable_sort() for this purpose.
	///		-	The hash table itself basically stores iterators to a seq::sequence object storing the actual values. Therefore, seq::ordered_set provides <b>stable references
	///			and iterators, even on rehash</b> (unlike std::unordered_set that invalidates iterators on rehash).
	///		-	No memory peak on rehash.
	///		-	seq::ordered_set uses robin hood probing with backard shift deletion. It does not rely on tombstones and supports high load factors.
	///		-	It is fast and memory efficient compared to other node based hash tables (see section <b>Performances</b>), but still slower than most open addressing hash tables.
	/// 
	/// seq::ordered_map behaves like #seq::ordered_set except that the underlying sequence stores elements of types std::pair<Key,T> instead of Key.
	/// Its interface is similar to std::unordered_map, except for the bucket based members.
	/// 
	/// Unlike std::unordered_map which stores objects of type std::pair<const Key,T>, ordered_map stores objects of type  std::pair<Key,T>.
	/// To avoid modifying the key value through iterators, the value is reinterpreted to std::pair<const Key,T>.
	/// Although quite ugly, this solution works on all tested compilers.
	/// 
	/// For more details on its internal implementation, see #seq::ordered_set documentation.
	/// 
	template<
		class Key,
		class T,
		class Hash = std::hash<Key>,
		class KeyEqual = std::equal_to<Key>,
		class Allocator = std::allocator< std::pair<Key, T> >,
		LayoutManagement Layout = OptimizeForSpeed
	>
		class ordered_map : private detail::SparseFlatNodeHashTable<Key, std::pair< Key, T>, Hash, KeyEqual, Allocator, Layout>
	{
		using base_type = detail::SparseFlatNodeHashTable<Key, std::pair<Key, T>, Hash, KeyEqual, Allocator, Layout>;
		using this_type = ordered_map<Key, T, Hash, KeyEqual, Allocator, Layout>;
		using extract_key = detail::ExtractKey<Key, std::pair<Key, T> >;

		template <typename U>
		using has_is_transparent = detail::has_is_transparent<U>;

		template<class Less>
		struct LessAdapter
		{
			Less l;
			LessAdapter() {}
			LessAdapter(Less l) :l(l) {}
			template<class U, class V>
			auto operator()(const U& v1, const V& v2) const -> bool {
				return l(extract_key::key(v1), extract_key::key(v2));
			}
		};

	public:
		using sequence_type = typename base_type::sequence_type;

		struct const_iterator
		{
			using iter_type = typename sequence_type::const_iterator;
			using value_type = std::pair<const Key, T>;
			using iterator_category = std::bidirectional_iterator_tag;
			using size_type = size_t;
			using difference_type = std::ptrdiff_t;
			using pointer = const value_type*;
			using reference = const value_type&;
			iter_type iter;

			const_iterator() {}
			const_iterator(const iter_type& it) : iter(it) {}
			SEQ_ALWAYS_INLINE auto operator++() noexcept -> const_iterator& {
				++iter;
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator++(int) noexcept -> const_iterator {
				const_iterator _Tmp = *this;
				++(*this);
				return _Tmp;
			}
			SEQ_ALWAYS_INLINE auto operator--() noexcept -> const_iterator& {
				--iter;
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator--(int) noexcept -> const_iterator {
				const_iterator _Tmp = *this;
				--(*this);
				return _Tmp;
			}
			SEQ_ALWAYS_INLINE auto operator*() const noexcept -> reference { return reinterpret_cast<const value_type&>(*iter); }
			SEQ_ALWAYS_INLINE auto operator->() const noexcept -> pointer { return  std::pointer_traits<pointer>::pointer_to(**this); }
			SEQ_ALWAYS_INLINE auto operator+=(difference_type diff) noexcept -> const_iterator& {
				iter += diff;
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator-=(difference_type diff) noexcept -> const_iterator& {
				iter -= diff;
				return *this;
			}
			SEQ_ALWAYS_INLINE bool operator==(const const_iterator& it) const noexcept { return iter == it.iter; }
			SEQ_ALWAYS_INLINE bool operator!=(const const_iterator& it) const noexcept { return iter != it.iter; }
			SEQ_ALWAYS_INLINE bool operator<(const const_iterator& it) const noexcept { return iter < it.iter; }
			SEQ_ALWAYS_INLINE bool operator>(const const_iterator& it) const noexcept { return iter > it.iter; }
			SEQ_ALWAYS_INLINE bool operator<=(const const_iterator& it) const noexcept { return iter <= it.iter; }
			SEQ_ALWAYS_INLINE bool operator>=(const const_iterator& it) const noexcept { return iter >= it.iter; }
			SEQ_ALWAYS_INLINE auto operator+(difference_type diff) const noexcept -> const_iterator { return iter + diff; }
			SEQ_ALWAYS_INLINE auto operator-(difference_type diff) const noexcept -> const_iterator { return iter + diff; }
			SEQ_ALWAYS_INLINE auto operator-(const const_iterator& other) const noexcept -> difference_type { return iter - other.iter; }
		};
		struct iterator : public const_iterator
		{
			using iter_type = typename sequence_type::const_iterator;
			using value_type = std::pair<const Key, T>;
			using iterator_category = std::random_access_iterator_tag;
			using size_type = size_t;
			using difference_type = std::ptrdiff_t;
			using pointer = value_type*;
			using reference = value_type&;

			iterator() : const_iterator() {}
			iterator(const iter_type& it) : const_iterator(it) {}
			iterator(const const_iterator& it) : const_iterator(it) {}
			SEQ_ALWAYS_INLINE auto operator++() noexcept -> iterator& {
				++this->iter;
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator++(int) noexcept -> iterator {
				iterator _Tmp = *this;
				++(*this);
				return _Tmp;
			}
			SEQ_ALWAYS_INLINE auto operator--() noexcept -> iterator& {
				--this->iter;
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator--(int) noexcept -> iterator {
				iterator _Tmp = *this;
				--(*this);
				return _Tmp;
			}
			SEQ_ALWAYS_INLINE auto operator*() noexcept -> reference { return reinterpret_cast<value_type&>(const_cast<std::pair<Key, T>&>(*this->iter)); }
			SEQ_ALWAYS_INLINE auto operator->() noexcept -> pointer { return  std::pointer_traits<pointer>::pointer_to(**this); }
			SEQ_ALWAYS_INLINE auto operator+=(difference_type diff) noexcept -> iterator& {
				this->iter += diff;
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator-=(difference_type diff) noexcept -> iterator& {
				this->iter -= diff;
				return *this;
			}
			SEQ_ALWAYS_INLINE bool operator==(const const_iterator& it) const noexcept { return this->iter == it.iter; }
			SEQ_ALWAYS_INLINE bool operator!=(const const_iterator& it) const noexcept { return this->iter != it.iter; }
			SEQ_ALWAYS_INLINE bool operator<(const const_iterator& it) const noexcept { return this->iter < it.iter; }
			SEQ_ALWAYS_INLINE bool operator>(const const_iterator& it) const noexcept { return this->iter > it.iter; }
			SEQ_ALWAYS_INLINE bool operator<=(const const_iterator& it) const noexcept { return this->iter <= it.iter; }
			SEQ_ALWAYS_INLINE bool operator>=(const const_iterator& it) const noexcept { return this->iter >= it.iter; }
			SEQ_ALWAYS_INLINE auto operator+(difference_type diff) const noexcept -> iterator { return this->iter + diff; }
			SEQ_ALWAYS_INLINE auto operator-(difference_type diff) const noexcept -> iterator { return this->iter + diff; }
			SEQ_ALWAYS_INLINE auto operator-(const const_iterator& other) const noexcept -> difference_type { return this->iter - other.iter; }
		};

		using key_type = Key;
		using mapped_type = T;
		using value_type = std::pair<Key, T>;
		using allocator_type = Allocator;
		using size_type = size_t;
		using difference_type = std::ptrdiff_t;
		using hasher = Hash;
		using key_equal = KeyEqual;
		using reference = value_type&;
		using const_reference = const value_type&;
		using pointer = typename std::allocator_traits<Allocator>::pointer;
		using const_pointer = typename std::allocator_traits<Allocator>::const_pointer;
		using reverse_iterator = std::reverse_iterator<iterator>;
		using const_reverse_iterator = std::reverse_iterator<const_iterator>;

		ordered_map(const Hash& hash = Hash(),
			const KeyEqual& equal = KeyEqual(),
			const Allocator& alloc = Allocator()) noexcept
			:base_type(hash, equal, alloc)
		{}

		explicit ordered_map(const Allocator& alloc)
			:ordered_map(Hash(), KeyEqual(), alloc)
		{}

		template< class InputIt >
		ordered_map(InputIt first, InputIt last,
			const Hash& hash = Hash(),
			const key_equal& equal = key_equal(),
			const Allocator& alloc = Allocator())
			: base_type(hash, equal, alloc)
		{
			insert(first, last);
		}
		template< class InputIt >
		ordered_map(InputIt first, InputIt last,
			const Allocator& alloc)
			: ordered_map(first, last, Hash(), key_equal(), alloc)
		{}
		template< class InputIt >
		ordered_map(InputIt first, InputIt last,
			const Hash& hash,
			const Allocator& alloc)
			: ordered_map(first, last, hash, key_equal(), alloc)
		{}
		ordered_map(const ordered_map& other, const Allocator& alloc)
			:base_type(other.hash_function(), other.key_eq(), alloc)
		{
			this->d_seq.resize(other.size());
			std::copy(other.sequence().begin(), other.sequence().end(), this->d_seq.begin());
			if (other.dirty()) base_type::mark_dirty();
			max_load_factor(other.max_load_factor());
			rehash();
		}
		ordered_map(const ordered_map& other)
			:ordered_map(other, copy_allocator( other.get_allocator()))
		{
		}
		ordered_map(ordered_map&& other)
			:base_type(std::move(other))
		{}
		ordered_map(ordered_map&& other, const Allocator& alloc)
			:base_type(std::move(other), alloc)
		{}
		ordered_map(std::initializer_list<value_type> init,
			const Hash& hash = Hash(),
			const key_equal& equal = key_equal(),
			const Allocator& alloc = Allocator())
			:ordered_map(init.begin(), init.end(), hash, equal, alloc)
		{}
		ordered_map(std::initializer_list<value_type> init,
			const Hash& hash,
			const Allocator& alloc)
			:ordered_map(init.begin(), init.end(), hash, alloc)
		{}
		ordered_map(std::initializer_list<value_type> init,
			const Allocator& alloc)
			:ordered_map(init.begin(), init.end(), alloc)
		{}

		auto operator=(const ordered_map& other) -> ordered_map&
		{
			if (this != std::addressof(other)) {
				this->d_seq = other.sequence();
				if (other.dirty()) this->mark_dirty();
				max_load_factor(other.max_load_factor());
				rehash();
			}
			return *this;
		}
		auto operator=(ordered_map&& other) noexcept -> ordered_map&
  		{
			base_type::swap(other);
			return *this;
		}

		auto size() const noexcept -> size_t { return this->d_seq.size(); }
		auto max_size() const noexcept -> size_t { return this->d_seq.max_size(); }
		auto empty() const noexcept -> bool { return this->d_seq.empty(); }

		auto max_probe_distance() const noexcept -> int { return this->d_max_dist; }

		auto load_factor() const noexcept -> float { return base_type::load_factor(); }
		auto max_load_factor() const noexcept -> float { return base_type::max_load_factor(); }
		void max_load_factor(float f) noexcept { base_type::max_load_factor(f); }

		auto get_allocator() noexcept -> allocator_type& { return this->d_seq.get_allocator(); }
		auto get_allocator() const noexcept -> const allocator_type & { return this->d_seq.get_allocator(); }

		auto hash_function() const -> hasher { return this->base_type::hash_function(); }
		auto key_eq() const -> key_equal { return this->base_type::key_eq(); }

		auto sequence() noexcept -> sequence_type& { this->base_type::mark_dirty(); return this->d_seq; }
		auto sequence() const noexcept -> const sequence_type& { return this->d_seq; }
		auto csequence() const noexcept -> const sequence_type& { return this->d_seq; }

		auto end() noexcept -> iterator { return this->d_seq.end(); }
		auto end() const noexcept -> const_iterator { return this->d_seq.end(); }
		auto cend() const noexcept -> const_iterator { return this->d_seq.end(); }

		auto begin() noexcept -> iterator { return this->d_seq.begin(); }
		auto begin() const noexcept -> const_iterator { return this->d_seq.begin(); }
		auto cbegin() const noexcept -> const_iterator { return this->d_seq.begin(); }

		auto rbegin() noexcept -> reverse_iterator { return this->d_seq.rbegin(); }
		auto rbegin() const noexcept -> const_reverse_iterator { return this->d_seq.rbegin(); }
		auto crbegin() const noexcept -> const_reverse_iterator { return this->d_seq.rbegin(); }

		auto rend() noexcept -> reverse_iterator { return this->d_seq.rend(); }
		auto rend() const noexcept -> const_reverse_iterator { return this->d_seq.rend(); }
		auto crend() const noexcept -> const_reverse_iterator { return this->d_seq.rend(); }

		void clear()
		{
			this->base_type::clear();
		}
		void rehash()
		{
			this->base_type::rehash();
		}
		void reserve(size_t size)
		{
			this->base_type::reserve(size);
		}
		template<class Less>
		void sort(Less le)
		{
			sequence().sort(LessAdapter<Less>(le));
			rehash();
		}
		void sort()
		{
			sequence().sort(LessAdapter<std::less<Key>>());
			rehash();
		}
		template<class Less>
		void stable_sort(Less le)
		{
			sequence().stable_sort(LessAdapter<Less>(le));
			rehash();
		}
		void stable_sort()
		{
			sequence().stable_sort(LessAdapter<std::less<Key>>());
			rehash();
		}
		void shrink_to_fit()
		{
			sequence().shrink_to_fit();
			rehash();
		}

		void swap(ordered_map& other)
		{
			base_type::swap(other);
		}


		template< class... Args >
		auto emplace(Args&&... args) -> std::pair<iterator, bool>
		{
			return this->base_type::template insert<detail::Anywhere>(std::forward<Args>(args)...);
		}
		template <class... Args>
		auto emplace_hint(const_iterator hint, Args&&... args) -> iterator
		{
			(void)hint;
			return this->base_type::template insert<detail::Anywhere>(std::forward<Args>(args)...).first;
		}
		auto insert(const value_type& value) -> std::pair<iterator, bool>
		{
			return this->base_type::template insert<detail::Anywhere>(value);
		}
		auto insert(value_type&& value) -> std::pair<iterator, bool>
		{
			return this->base_type::template insert<detail::Anywhere>(std::move(value));
		}
		template< class P >
		auto insert(P&& value) -> std::pair<iterator, bool>
		{
			return this->base_type::template insert<detail::Anywhere>(std::forward<P>(value));
		}
		auto insert(const_iterator hint, const value_type& value) -> iterator
		{
			(void)hint;
			return insert(value).first;
		}
		auto insert(const_iterator hint, value_type&& value) -> iterator
		{
			(void)hint;
			return insert(std::move(value)).first;
		}
		template< class P >
		auto insert(const_iterator hint, P&& value) -> iterator
		{
			(void)hint;
			return this->base_type::template insert<detail::Anywhere>(std::forward<P>(value));
		}
		template< class InputIt >
		void insert(InputIt first, InputIt last)
		{
			this->base_type::insert(first, last);
		}
		void insert(std::initializer_list<value_type> ilist)
		{
			insert(ilist.begin(), ilist.end());
		}




		template <class M>
		auto insert_or_assign(const Key& k, M&& obj) -> std::pair<iterator, bool>
		{
			auto it = find(k);
			if (it != end()) {
				it->second = std::forward<M>(obj);
				return std::pair<iterator, bool>(it, false);
			}
			return this->base_type::template insert_no_check<detail::Anywhere>(k, std::forward<M>(obj));
		}
		template <class M>
		auto insert_or_assign(Key&& k, M&& obj) -> std::pair<iterator, bool>
		{
			auto it = find(k);
			if (it != end()) {
				it->second = std::forward<M>(obj);
				return std::pair<iterator, bool>(it, false);
			}
			return this->base_type::template insert_no_check<detail::Anywhere>(std::move(k), std::forward<M>(obj));
		}
		template <class M>
		auto insert_or_assign(const_iterator hint, const Key& k, M&& obj) -> iterator
		{
			(void)hint;
			return insert_or_assign(k, std::forward<M>(obj)).first;
		}
		template <class M>
		auto insert_or_assign(const_iterator hint, Key&& k, M&& obj) -> iterator
		{
			(void)hint;
			return insert_or_assign(std::move(k), std::forward<M>(obj)).first;
		}


		template <class M>
		auto push_back_or_assign(const Key& k, M&& obj) -> std::pair<iterator, bool>
		{
			auto it = find(k);
			if (it != end()) {
				it->second = std::forward<M>(obj);
				return std::pair<iterator, bool>(it, false);
			}
			return this->base_type::template insert_no_check<detail::Back>(k, std::forward<M>(obj));
		}
		template <class M>
		auto push_back_or_assign(Key&& k, M&& obj) -> std::pair<iterator, bool>
		{
			auto it = find(k);
			if (it != end()) {
				it->second = std::forward<M>(obj);
				return std::pair<iterator, bool>(it, false);
			}
			return this->base_type::template insert_no_check<detail::Back>(std::move(k), std::forward<M>(obj));
		}
		template <class M>
		auto push_back_or_assign(const_iterator hint, const Key& k, M&& obj) -> iterator
		{
			(void)hint;
			return push_back_or_assign(k, std::forward<M>(obj)).first;
		}
		template <class M>
		auto push_back_or_assign(const_iterator hint, Key&& k, M&& obj) -> iterator
		{
			(void)hint;
			return push_back_or_assign(std::move(k), std::forward<M>(obj)).first;
		}



		template <class M>
		auto push_front_or_assign(const Key& k, M&& obj) -> std::pair<iterator, bool>
		{
			auto it = find(k);
			if (it != end()) {
				it->second = std::forward<M>(obj);
				return std::pair<iterator, bool>(it, false);
			}
			return this->base_type::template insert_no_check<detail::Front>(k, std::forward<M>(obj));
		}
		template <class M>
		auto push_front_or_assign(Key&& k, M&& obj) -> std::pair<iterator, bool>
		{
			auto it = find(k);
			if (it != end()) {
				it->second = std::forward<M>(obj);
				return std::pair<iterator, bool>(it, false);
			}
			return this->base_type::template insert_no_check<detail::Front>(std::move(k), std::forward<M>(obj));
		}
		template <class M>
		auto push_front_or_assign(const_iterator hint, const Key& k, M&& obj) -> iterator
		{
			(void)hint;
			return push_front_or_assign(k, std::forward<M>(obj)).first;
		}
		template <class M>
		auto push_front_or_assign(const_iterator hint, Key&& k, M&& obj) -> iterator
		{
			(void)hint;
			return push_front_or_assign(std::move(k), std::forward<M>(obj)).first;
		}


		template< class... Args >
		auto emplace_back(Args&&... args) -> std::pair<iterator, bool>
		{
			return this->base_type::template insert<detail::Back>(std::forward<Args>(args)...);
		}
		auto push_back(const value_type& value) -> std::pair<iterator, bool>
		{
			return this->base_type::template insert<detail::Back>(value);
		}
		auto push_back(value_type&& value) -> std::pair<iterator, bool>
		{
			return this->base_type::template insert<detail::Back>(std::move(value));
		}
		template<class P>
		auto push_back(P&& value) -> std::pair<iterator, bool>
		{
			return this->base_type::template insert<detail::Back>(std::forward<P>(value));
		}

		template< class... Args >
		auto emplace_front(Args&&... args) -> std::pair<iterator, bool>
		{
			return this->base_type::template insert<detail::Front>(std::forward<Args>(args)...);
		}
		auto push_front(const value_type& value) -> std::pair<iterator, bool>
		{
			return this->base_type::template insert<detail::Front>(value);
		}
		auto push_front(value_type&& value) -> std::pair<iterator, bool>
		{
			return this->base_type::template insert<detail::Front>(std::move(value));
		}
		template<class P>
		auto push_front(P&& value) -> std::pair<iterator, bool>
		{
			return this->base_type::template insert<detail::Front>(std::forward<P>(value));
		}




		template< class... Args >
		auto try_emplace(const Key& k, Args&&... args) -> std::pair<iterator, bool>
		{
			auto it = find(k);
			if (it != end())
				return std::pair<iterator, bool>(it, false);
			return this->base_type::template insert_no_check<detail::Anywhere>(
				value_type(std::piecewise_construct,
				std::forward_as_tuple(k),
				std::forward_as_tuple(std::forward<Args>(args)...)));
		}
		template< class... Args >
		auto try_emplace(Key&& k, Args&&... args) -> std::pair<iterator, bool>
		{
			auto it = find(k);
			if (it != end())
				return std::pair<iterator, bool>(it, false);
			return this->base_type::template insert_no_check<detail::Anywhere>(
				value_type(std::piecewise_construct,
					std::forward_as_tuple(std::move(k)),
					std::forward_as_tuple(std::forward<Args>(args)...)));
		}
		template< class... Args >
		auto try_emplace(const_iterator hint, const Key& k, Args&&... args) -> iterator
		{
			(void)hint;
			return try_emplace(k, std::forward<Args>(args)...).first;
		}
		template< class... Args >
		auto try_emplace(const_iterator hint, Key&& k, Args&&... args) -> iterator
		{
			(void)hint;
			return try_emplace(std::move(k), std::forward<Args>(args)...).first;
		}




		template< class... Args >
		auto try_emplace_back(const Key& k, Args&&... args) -> std::pair<iterator, bool>
		{
			auto it = find(k);
			if (it != end())
				return std::pair<iterator, bool>(it, false);
			return this->base_type::template insert_no_check<detail::Back>(
				value_type(std::piecewise_construct,
					std::forward_as_tuple(k),
					std::forward_as_tuple(std::forward<Args>(args)...)));
		}
		template< class... Args >
		auto try_emplace_back(Key&& k, Args&&... args) -> std::pair<iterator, bool>
		{
			auto it = find(k);
			if (it != end())
				return std::pair<iterator, bool>(it, false);
			return this->base_type::template insert_no_check<detail::Back>(
				value_type(std::piecewise_construct,
					std::forward_as_tuple(std::move(k)),
					std::forward_as_tuple(std::forward<Args>(args)...)));
		}
		template< class... Args >
		auto try_emplace_back(const_iterator hint, const Key& k, Args&&... args) -> iterator
		{
			(void)hint;
			return try_emplace_back(k, std::forward<Args>(args)...).first;
		}
		template< class... Args >
		auto try_emplace_back(const_iterator hint, Key&& k, Args&&... args) -> iterator
		{
			(void)hint;
			return try_emplace_back(std::move(k), std::forward<Args>(args)...).first;
		}




		template< class... Args >
		auto try_emplace_front(const Key& k, Args&&... args) -> std::pair<iterator, bool>
		{
			auto it = find(k);
			if (it != end())
				return std::pair<iterator, bool>(it, false);
			return this->base_type::template insert_no_check<detail::Front>(
				value_type(std::piecewise_construct,
					std::forward_as_tuple(k),
					std::forward_as_tuple(std::forward<Args>(args)...)));
		}
		template< class... Args >
		auto try_emplace_front(Key&& k, Args&&... args) -> std::pair<iterator, bool>
		{
			auto it = find(k);
			if (it != end())
				return std::pair<iterator, bool>(it, false);
			return this->base_type::template insert_no_check<detail::Front>(
				value_type(std::piecewise_construct,
					std::forward_as_tuple(std::move(k)),
					std::forward_as_tuple(std::forward<Args>(args)...)));
		}
		template< class... Args >
		auto try_emplace_front(const_iterator hint, const Key& k, Args&&... args) -> iterator
		{
			(void)hint;
			return try_emplace_front(k, std::forward<Args>(args)...).first;
		}
		template< class... Args >
		auto try_emplace_front(const_iterator hint, Key&& k, Args&&... args) -> iterator
		{
			(void)hint;
			return try_emplace_front(std::move(k), std::forward<Args>(args)...).first;
		}



		auto at(const Key& key) -> T&
		{
			auto it = find(key);
			if (it == end())
				throw std::out_of_range("ordered_map: key not found");
			return it->second;
		}
		auto at(const Key& key) const -> const T&
		{
			auto it = find(key);
			if (it == end())
				throw std::out_of_range("ordered_map: key not found");
			return it->second;
		}



		auto operator[](const Key& key) -> T&
		{
			auto it = find(key);
			if (it != end())
				return it->second;
			return this->base_type::template insert_no_check<detail::Anywhere>(
				value_type(std::piecewise_construct, std::forward_as_tuple(key), std::tuple<>())).first->second;
		}
		auto operator[](Key&& key) -> T&
		{
			auto it = find(key);
			if (it != end())
				return it->second;
			return this->base_type::template insert_no_check<detail::Anywhere>(
				value_type(std::piecewise_construct, std::forward_as_tuple(std::move(key)), std::tuple<>())).first->second;
		}



		auto erase(const_iterator pos) -> iterator
		{
			return this->base_type::erase(pos.iter);
		}
		auto erase(const Key& key) -> size_type
		{
			return this->base_type::erase(key);
		}
		template <class K, class KE = KeyEqual, class H = Hash,
			typename std::enable_if<has_is_transparent<KE>::value&& has_is_transparent<H>::value>::type* = nullptr>
			auto erase(const K& key) -> size_type
		{
			return this->base_type::erase(key);
		}
		auto erase(const_iterator first, const_iterator last) -> iterator
		{
			return this->base_type::erase(first.iter, last.iter);
		}

		SEQ_ALWAYS_INLINE auto find(const Key& value) const -> const_iterator
		{
			return this->base_type::find(value);
		}
		SEQ_ALWAYS_INLINE auto find(const Key& value) -> iterator
		{
			return static_cast<iterator>(const_cast<this_type*>(this)->base_type::find(value));
		}

		template <class K, class KE = KeyEqual, class H = Hash,
			typename std::enable_if<has_is_transparent<KE>::value&& has_is_transparent<H>::value>::type* = nullptr>
		SEQ_ALWAYS_INLINE auto find(const K& key) const -> const_iterator
		{
			return const_cast<this_type*>(this)->base_type::find(key);
		}
		template <class K, class KE = KeyEqual, class H = Hash,
			typename std::enable_if<has_is_transparent<KE>::value&& has_is_transparent<H>::value>::type* = nullptr>
		SEQ_ALWAYS_INLINE auto find(const K& key) -> iterator
		{
			return static_cast<iterator>(const_cast<this_type*>(this)->base_type::find(key));
		}


		auto count(const Key& key) const -> size_type
		{
			return find(key) != end();
		}

		template <class K, class KE = KeyEqual, class H = Hash,
			typename std::enable_if<has_is_transparent<KE>::value&& has_is_transparent<H>::value>::type* = nullptr>
		auto count(const K& key) const -> size_type
		{
			return find(key) != end();
		}

		bool contains(const Key& key) const
		{
			return find(key) != end();
		}

		template <class K, class KE = KeyEqual, class H = Hash,
			typename std::enable_if<has_is_transparent<KE>::value&& has_is_transparent<H>::value>::type* = nullptr>
		bool contains(const K& key) const
		{
			return find(key) != end();
		}
	};




	/// @brief Compare two ordered_set for equality.
	/// Two ordered_set are considered equal if they contain the same keys. Key ordering is not considered.
	/// @param lhs left ordered_set
	/// @param rhs right ordered_set
	/// @return true if both containers compare equals, false otherwise
	template<
		class Key,
		class Hash1 ,
		class Hash2,
		class KeyEqual,
		class Allocator1,
		class Allocator2,
		LayoutManagement Layout1,
		LayoutManagement Layout2
	>
		auto operator == (const ordered_set<Key, Hash1, KeyEqual, Allocator1, Layout1>& lhs, const ordered_set<Key, Hash2, KeyEqual, Allocator2, Layout2>& rhs) -> bool
	{
		if (lhs.size() != rhs.size())
			return false;
		for (auto it = lhs.begin(); it != lhs.end(); ++it) {
			auto found = rhs.find(*it);
			if (found == rhs.end())
				return false;
		}
		return true;
	}
	/// @brief Compare two ordered_set for inequality, synthesized from operator==.
	/// @return true if both containers compare non equals, false otherwise
	template<
		class Key,
		class Hash1,
		class Hash2,
		class KeyEqual,
		class Allocator1,
		class Allocator2,
		LayoutManagement Layout1,
		LayoutManagement Layout2
	>
		auto operator != (const ordered_set<Key, Hash1, KeyEqual, Allocator1, Layout1>& lhs, const ordered_set<Key, Hash2, KeyEqual, Allocator2, Layout2>& rhs) -> bool
	{
		return !(lhs == rhs);
	}
	/// @brief Erases all elements that satisfy the predicate p from the container.
	/// @param set container from which to erase
	/// @param p predicate that returns true if the element should be erased
	/// @return number of erased elements
	template<
		class Key,
		class Hash1,
		class KeyEqual,
		class Allocator1,
		LayoutManagement Layout1,
		class Pred
	>
	auto erase_if(ordered_set<Key, Hash1, KeyEqual, Allocator1, Layout1>& set, Pred p) -> size_t
	{
		using sequence_type = typename ordered_set<Key, Hash1, KeyEqual, Allocator1, Layout1>::sequence_type;
		// avoid flagging the map as dirty
		auto& seq = const_cast<sequence_type&>(set.csequence());
		size_t count = 0;
		for (auto it = seq.begin(); it != seq.end(); ) {
			if (p(*it)) {
				it = seq.erase(it);
				++count;
			}
			else
				++it;
		}
		if (count) {
			//flag as dirty and rehash
			set.sequence();
			set.rehash();
		}
		return count;
	}

	/// @brief Compare two ordered_map for equality.
	/// Two ordered_map are considered equal if they contain the same pairs key->value. Key ordering is not considered.
	/// @param lhs left ordered_map
	/// @param rhs right ordered_map
	/// @return true if both containers compare equals, false otherwise
	template<
		class Key,
		class T,
		class Hash1,
		class Hash2,
		class KeyEqual,
		class Allocator1,
		class Allocator2,
		LayoutManagement Layout1,
		LayoutManagement Layout2
	>
		auto operator == (const ordered_map<Key, T, Hash1, KeyEqual, Allocator1, Layout1>& lhs, const ordered_map<Key, T, Hash2, KeyEqual, Allocator2, Layout2>& rhs) -> bool
	{
		if (lhs.size() != rhs.size())
			return false;
		for (auto it = lhs.begin(); it != lhs.end(); ++it) {
			auto found = rhs.find(it->first);
			if (found == rhs.end() || !(found->second == it->second))
				return false;
		}
		return true;
	}
	/// @brief Compare two ordered_map for inequality, synthesized from operator==.
	/// @return true if both containers compare non equals, false otherwise
	template<
		class Key,
		class T,
		class Hash1,
		class Hash2,
		class KeyEqual,
		class Allocator1,
		class Allocator2,
		LayoutManagement Layout1,
		LayoutManagement Layout2
	>
		auto operator != (const ordered_map<Key, T, Hash1, KeyEqual, Allocator1, Layout1>& lhs, const ordered_map<Key, T, Hash2, KeyEqual, Allocator2, Layout2>& rhs) -> bool
	{
		return !(lhs == rhs);
	}
	/// @brief Erases all elements that satisfy the predicate p from the container.
	/// @param set container from which to erase
	/// @param p predicate that returns true if the element should be erased
	/// @return number of erased elements
	template<
		class Key,
		class T, 
		class Hash1,
		class KeyEqual,
		class Allocator1,
		LayoutManagement Layout1,
		class Pred
	>
		auto erase_if(ordered_map<Key, T, Hash1, KeyEqual, Allocator1, Layout1>& set, Pred p) -> size_t
	{
		using sequence_type = typename ordered_map<Key, T, Hash1, KeyEqual, Allocator1, Layout1>::sequence_type;
		// avoid flagging the map as dirty
		auto& seq = const_cast<sequence_type&>(set.csequence());
		size_t count = 0;
		for (auto it = seq.begin(); it != seq.end(); ) {
			if (p(*it)) {
				it = seq.erase(it);
				++count;
			}
			else
				++it;
		}
		if (count) {
			//flag as dirty and rehash
			set.sequence();
			set.rehash();
		}
		return count;
	}

}//end namespace seq

#endif
