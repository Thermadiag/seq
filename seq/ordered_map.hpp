#pragma once

/** @file */

#include "sequence.hpp"
#include "utils.hpp"

namespace seq
{
	
	
	namespace detail
	{
	
		
	#if (defined (__x86_64__) || defined (_M_X64)) && !defined(SEQ_NO_COMPRESSED_PTR)
		
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
			static SEQ_ALWAYS_INLINE tiny_hash small_hash(size_t h) { 
				return ( (h) >> (64U - 8U)); 
			}
		
			SEQ_ALWAYS_INLINE SequenceNode() : val(0) { ((char*)&val)[7] = (char)-1; /*set_distance(-1);*/ }
			template< class Iter>
			SEQ_ALWAYS_INLINE SequenceNode(tiny_hash h, dist_type dist, const Iter& it)
			{
				val = (std::uint64_t)it.pos | ((std::uint64_t)it.node);
				((unsigned char*)&val)[6] = h;
				((char*)&val)[7] = (char)dist;
			}
			
			// Check if node is a tombstone (only for pure linear hashing)
			SEQ_ALWAYS_INLINE bool is_tombstone() const noexcept {return distance() == tombstone;}
			// check if node is valid and not a tombstone
			SEQ_ALWAYS_INLINE bool null_for_search() const noexcept { return (val & ((1ULL << 48ULL) - 1ULL)) == 0 && distance() != tombstone; }
			// check if node is not null
			SEQ_ALWAYS_INLINE bool null() const noexcept { return (val & ((1ULL << 48ULL)-1ULL)) == 0; }
			// position withing sequence chunk (max == 63)
			SEQ_ALWAYS_INLINE std::uint8_t pos() const noexcept {return val & mask_pos;}
			// sequence chunk, aligned on 64 bytes at most
			SEQ_ALWAYS_INLINE detail::list_chunk<T>* node() const noexcept {return reinterpret_cast<detail::list_chunk<T>*>(val & mask_node);}
			SEQ_ALWAYS_INLINE T& value() noexcept { return node()->buffer()[pos()];}
			SEQ_ALWAYS_INLINE const T& value() const noexcept {return node()->buffer()[pos()]; }
			SEQ_ALWAYS_INLINE tiny_hash hash() const noexcept { return ((unsigned char*)&val)[6]; }
			SEQ_ALWAYS_INLINE dist_type distance() const noexcept { return  (( char*)&val)[7];}
			
			template<class Iter>
			SEQ_ALWAYS_INLINE bool is_same(const Iter& it) const noexcept {
				//check iterator equality
				return node() == it.node && pos() == it.pos;
			}
			SEQ_ALWAYS_INLINE void empty() {
				// mark the node as null
				val = 0;
				((char*)&val)[7] = (char)-1;
			}
			SEQ_ALWAYS_INLINE void empty_tombstone() {
				// mark the node as tombstone
				val = 0;
				((char*)&val)[7] = (char)tombstone;
			}

			SEQ_ALWAYS_INLINE void set_distance(dist_type dist)
			{
				(( char*)&val)[7] = (char)dist;
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
			static constexpr std::uint8_t mask_low = ((1U << (unsigned)tag_bits) - 1U);
			static SEQ_ALWAYS_INLINE tiny_hash small_hash(size_t h) { return h >> (64U - 8U); }

			std::uint8_t storage[sizeof(void*)];
			std::uint8_t _hash;
			std::int8_t _dist;

			SEQ_ALWAYS_INLINE SequenceNode():_hash(0),_dist(-1) { memset(storage, 0, sizeof(storage)); }
			template< class Iter>
			SEQ_ALWAYS_INLINE SequenceNode(tiny_hash h, size_t dist, const Iter& it)
			{
				std::uintptr_t p = ((std::uintptr_t)it.node) | (it.pos);
				memcpy(storage, &p, sizeof(p));
				_hash = h;
				_dist = (char)dist;
			}

			SEQ_ALWAYS_INLINE bool is_tombstone() const noexcept { return distance() == tombstone; }
			SEQ_ALWAYS_INLINE bool null_for_search() const noexcept { return null() && distance() != tombstone; }
			SEQ_ALWAYS_INLINE bool null() const noexcept { return read_ptr_t(storage) == 0; }
			SEQ_ALWAYS_INLINE dist_type distance() const noexcept {return _dist;}
			SEQ_ALWAYS_INLINE std::uint8_t pos() const noexcept { return storage[0] & mask_low; }
			SEQ_ALWAYS_INLINE detail::list_chunk<T>* node() const noexcept {return reinterpret_cast<detail::list_chunk<T>*>(read_size_t(storage) & mask_high);}
			SEQ_ALWAYS_INLINE T& value() noexcept { return node()->buffer()[pos()]; }
			SEQ_ALWAYS_INLINE const T& value() const noexcept { return node()->buffer()[pos()]; }
			SEQ_ALWAYS_INLINE tiny_hash hash() const noexcept { return _hash; }// flags >> max_dist_bits;
		
			template<class Iter>
			SEQ_ALWAYS_INLINE bool is_same(const Iter& it) const noexcept {
				return node() == it.node && pos() == it.pos;
			}
			SEQ_ALWAYS_INLINE void empty() {
				memset(storage, 0, sizeof(storage));
				_hash =  0;
				_dist = -1;
			}

			SEQ_ALWAYS_INLINE void empty_tombstone() {
				memset(storage, 0, sizeof(storage));
				_hash = 0;
				_dist = tombstone;
			}

			SEQ_ALWAYS_INLINE void set_distance(dist_type dist)
			{
				_dist = (char)dist;
			}

		};

	#endif





		
		// Gather hash class and equal_to class in the same struct. Inherits both for non empty classes.
		template< class Hash, class Equal, bool EmptyHash = std::is_empty<Hash>::value, bool EmptyEqual = std::is_empty<Equal>::value>
		struct HashEqual : private Hash, private Equal
		{
			HashEqual() {}
			HashEqual(const Hash& h, const Equal& e) : Hash(h), Equal(e) {}
			HashEqual(const HashEqual& other) : Hash(other), Equal(other) {}

			Hash hash_function() const { return static_cast<Hash&>(*this); }
			Equal key_eq() const { return static_cast<Equal&>(*this); }

			template< class... Args >
			SEQ_ALWAYS_INLINE size_t hash(Args&&... args) const noexcept { return (Hash::operator()(std::forward<Args>(args)...)); }
			template< class... Args >
			SEQ_ALWAYS_INLINE bool operator()(Args&&... args) const noexcept { return Equal::operator()(std::forward<Args>(args)...); }
		};
		template< class Hash, class Equal>
		struct HashEqual<Hash,Equal,true,true>
		{
			HashEqual() {}
			HashEqual(const Hash& , const Equal& )  {}
			HashEqual(const HashEqual& ) {}

			Hash hash_function() const { return Hash(); }
			Equal key_eq() const { return Equal(); }

			template< class... Args >
			SEQ_ALWAYS_INLINE size_t hash(Args&&... args) const noexcept { return (Hash{}(std::forward<Args>(args)...)); }
			template< class... Args >
			SEQ_ALWAYS_INLINE bool operator()(Args&&... args) const noexcept { return Equal{}(std::forward<Args>(args)...); }
		};
		template< class Hash, class Equal>
		struct HashEqual<Hash, Equal, true,false> : private Equal
		{
			HashEqual() {}
			HashEqual(const Hash& , const Equal& e) :  Equal(e) {}
			HashEqual(const HashEqual& other) : Equal(other) {}

			Hash hash_function() const { return Hash(); }
			Equal key_eq() const { return static_cast<Equal&>(*this); }

			template< class... Args >
			SEQ_ALWAYS_INLINE size_t hash(Args&&... args) const noexcept { return (Hash{}(std::forward<Args>(args)...)); }
			template< class... Args >
			SEQ_ALWAYS_INLINE bool operator()(Args&&... args) const noexcept { return Equal::operator()(std::forward<Args>(args)...); }
		};
		template< class Hash, class Equal>
		struct HashEqual<Hash, Equal, false, true> : private Hash
		{
			HashEqual() {}
			HashEqual(const Hash& h, const Equal& ) : Hash(h) {}
			HashEqual(const HashEqual& other) : Hash(other) {}

			Hash hash_function() const { return static_cast<Hash&>(*this); }
			Equal key_eq() const { return Equal(); }

			template< class... Args >
			SEQ_ALWAYS_INLINE size_t hash(Args&&... args) const noexcept { return (Hash::operator()(std::forward<Args>(args)...)); }
			template< class... Args >
			SEQ_ALWAYS_INLINE bool operator()(Args&&... args) const noexcept { return Equal{}(std::forward<Args>(args)...); }
		};

		


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
			
			static SEQ_ALWAYS_INLINE typename Sequence::iterator insert(Sequence& seq, const Value& value) {return seq.emplace_back_iter((value));}
		};
		template<class Sequence, class Value>
		struct Inserter<Sequence, Front, const Value&>
		{
			static SEQ_ALWAYS_INLINE typename Sequence::iterator insert(Sequence& seq, const Value& value) { return seq.emplace_front_iter((value)); }
		};
		template<class Sequence, class Value>
		struct Inserter<Sequence, Anywhere, const Value&>
		{
			static SEQ_ALWAYS_INLINE typename Sequence::iterator insert(Sequence& seq, const Value& value) { return seq.emplace((value)); }
		};

		// Insert by move semantic for value constructed on the stack
		template<class Sequence, class Value>
		struct Inserter<Sequence, Back,  Value>
		{
			static SEQ_ALWAYS_INLINE typename Sequence::iterator insert(Sequence& seq,  Value& value) { return seq.emplace_back_iter(std::move(value)); }
		};
		template<class Sequence, class Value>
		struct Inserter<Sequence, Front,  Value>
		{
			static SEQ_ALWAYS_INLINE typename Sequence::iterator insert(Sequence& seq,  Value& value) { return seq.emplace_front_iter(std::move(value)); }
		};
		template<class Sequence, class Value>
		struct Inserter<Sequence, Anywhere,  Value>
		{
			static SEQ_ALWAYS_INLINE typename Sequence::iterator insert(Sequence& seq,  Value& value) { return seq.emplace(std::move(value)); }
		};

		// Insert by move semantic for move insertion
		template<class Sequence, class Value>
		struct Inserter<Sequence,Back,Value&&>
		{
			static SEQ_ALWAYS_INLINE typename Sequence::iterator insert(Sequence& seq, Value& value) {return seq.emplace_back_iter(std::move(value));}
		};
		template<class Sequence, class Value>
		struct Inserter<Sequence, Front, Value&&>
		{
			static SEQ_ALWAYS_INLINE typename Sequence::iterator insert(Sequence& seq, Value& value) { return seq.emplace_front_iter(std::move(value)); }
		};
		template<class Sequence, class Value>
		struct Inserter<Sequence, Anywhere, Value&&>
		{
			static SEQ_ALWAYS_INLINE typename Sequence::iterator insert(Sequence& seq, Value& value) { return seq.emplace(std::move(value)); }
		};
		



		template< class Key, class Value, class Hash, class Equal, class Allocator, LayoutManagement layout>
		struct SparseFlatNodeHashTable : public HashEqual<Hash,Equal>
		{
			// Base class for robin-hood hash table.
			// Values are stored in a seq::sequence object, and the hash table contains iterators to the sequence with additional hash value and distance to perfect location.
			
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
			using sequence_type = sequence<Value, std::allocator<Value>, layout, true>;
			using iterator = typename sequence_type::iterator;
			using const_iterator = typename sequence_type::const_iterator;

			static constexpr typename node_type::dist_type mask_dirty = integer_max<typename node_type::dist_type>::value;

			sequence_type	d_seq;
			node_type*		d_buckets;
			size_t			d_hash_mask;
			size_t			d_next_target;
			int				d_max_dist;
			float			d_load_factor;


		private:
			static node_type* null_node() {
				// Null node used to initialize d_buckets (avoid a check on lookup)
				static node_type null;
				return &null;
			}

			node_type* find_node(size_t hash, const const_iterator& it)
			{
				// Find an existing node based on a sequence iterator and the hash value
				size_t index = hash & d_hash_mask;
				while (!d_buckets[index].is_same(it)) {
					SEQ_ASSERT_DEBUG(d_max_dist == node_type::max_distance || !d_buckets[index].null(), "corrupted hash map");
					if (SEQ_UNLIKELY(++index == bucket_size()))
						index = 0;
				}
				return d_buckets + index;
			}
			node_type* find_node(const const_iterator& it)
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

				for (auto it = first; it != last; ++it) {

					hash = hash_key(extract_key::key(*it));
					index = (hash & new_hash_mask);

					if (SEQ_UNLIKELY(d_max_dist == node_type::max_distance)) {
						// pure linear hashing
						while (!d_buckets[index].null() ) {
							if (SEQ_UNLIKELY(++index == bsize))
								index = 0;
						}
						d_buckets[index] = node_type(node_type::small_hash(hash), index == (hash & new_hash_mask) ? 0 : node_type::max_distance, it);
						continue;
					}

					dist = 0;
					while (dist <= d_buckets[index].distance()) {
						if (SEQ_UNLIKELY(++index == bsize))
							index = 0;
						dist++;
					}
					if (SEQ_UNLIKELY(dist > d_max_dist))
						d_max_dist = dist;// = (dist > node_type::max_distance) ? node_type::max_distance : dist;

					node_type n = d_buckets[index];
					d_buckets[index] = node_type(node_type::small_hash(hash), dist, it);

					if (dist)
						start_insert(d_buckets, bsize, index, dist, n);

				}

				d_hash_mask = new_hash_mask;
				d_next_target = static_cast<size_t>( bucket_size() * (double)d_load_factor);
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
					while (!d_buckets[index].null() && (difference_type)dist <= (difference_type)d_buckets[index].distance()) {
						if (h == d_buckets[index].hash() && (*this)(extract_key::key(d_buckets[index].value()), extract_key::key (*it)))
						{
							dist = (size_t)-1;
							break;
						}
						if (SEQ_UNLIKELY(++index == bsize))
							index = 0;
						dist++;
					}

					if (SEQ_UNLIKELY(d_max_dist == (int)node_type::max_distance && dist != (size_t)-1)) {
						// linear hash table, we must go up to an empty node
						while (!d_buckets[index].null()) {

							if (h == d_buckets[index].hash() && (*this)(extract_key::key(d_buckets[index].value()), extract_key::key(*it)))
							{
								dist = (size_t)-1;
								break;
							}
							if (SEQ_UNLIKELY(++index == bsize))
								index = 0;
						}
					}


					// the value exists
					if (dist == (size_t)-1) {
						it = d_seq.erase(it);
						continue;
					}

					if (SEQ_UNLIKELY(d_max_dist == node_type::max_distance)) {
						d_buckets[index] = node_type((tiny_hash)h, index == (hash & new_hash_mask) ? 0 : node_type::max_distance, it);
					}
					else {
						if (dist > (size_t)d_max_dist)
							d_max_dist = (int)dist;// = (dist > node_type::max_distance) ? node_type::max_distance : dist;

						node_type n = node_type((tiny_hash)h, (dist_type)dist, it);
						std::swap(n, d_buckets[index]);
						if (dist)
							start_insert(d_buckets, bsize, index, (dist_type)dist, n);
					}
					++it;
				}

				d_hash_mask = new_hash_mask;
				d_next_target = static_cast<size_t>(bucket_size() * (double)d_load_factor);
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

			node_type* make_buckets(size_t size) const
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

			// Returns distance between 2 iterators, or 0 for non random access iterators
			template<class Iter, class Cat>
			std::ptrdiff_t iter_distance(const Iter& it1, const Iter& it2, Cat) const noexcept { return 0; }
			template<class Iter>
			std::ptrdiff_t iter_distance(const Iter& it1, const Iter& it2, std::random_access_iterator_tag) const noexcept { return it1 - it2; }
			template<class Iter>
			std::ptrdiff_t distance(const Iter& it1, const Iter& it2)const noexcept {
				return iter_distance(it1, it2, typename std::iterator_traits<Iter>::iterator_category());
			}

		public:

			SparseFlatNodeHashTable(const Hash& hash = Hash(),
				const Equal& equal = Equal(),
				const Allocator& alloc = Allocator()) noexcept
				:base_type(hash,equal), d_seq(alloc), d_buckets(null_node()), d_hash_mask(0), d_next_target(0), d_max_dist(1), d_load_factor(0.6f)
			{
			}
			SparseFlatNodeHashTable(SparseFlatNodeHashTable&& other)
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
				other.d_buckets = null_node();
				other.d_hash_mask = 0;
				other.d_next_target = 0;
				other.d_max_dist = 1;
			}

			~SparseFlatNodeHashTable()
			{
				free_buckets(d_buckets);
			}

			void swap(SparseFlatNodeHashTable& other) noexcept
			{
				std::swap(static_cast<base_type&>(*this), static_cast<base_type&>(other));
				std::swap(d_buckets, other.d_buckets);
				std::swap(d_hash_mask, other.d_hash_mask);
				std::swap(d_next_target, other.d_next_target);
				std::swap(d_max_dist, other.d_max_dist);
				std::swap(d_load_factor, other.d_load_factor);
				std::swap(d_seq, other.d_seq);
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
			SEQ_ALWAYS_INLINE size_t size() const noexcept 
			{ 
				return d_seq.size();
			}
			void  reserve(size_t size)
			{
				if(size > this->size())
					rehash(static_cast<size_t>(size/ (double)d_load_factor));
			}
			void rehash(size_t size = 0, bool force = false)
			{
				// Rehash for given size if one of the following is true:
				// - the hash table is dirty
				// - the new hash table size is different from the current one
				// - force is true

				const bool null_size = size == 0;

				if(null_size)
					size = static_cast<size_t>(this->size() / (double)d_load_factor);
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
			SEQ_ALWAYS_INLINE size_t hash_key(Args&&... args) const noexcept 
			{
				// Hash the key and multiply the result.
				// The multiplication is mandatory for very bad hash functions that do not distribute well their values (like libstdc++ default hash function for integers)
				return this->hash(std::forward<Args>(args)...) *UINT64_C(0xc4ceb9fe1a85ec53);
			}

			SEQ_ALWAYS_INLINE float max_load_factor() const noexcept
			{ 
				// Returns the maximum load factor
				return d_load_factor; 
			}
			SEQ_ALWAYS_INLINE void max_load_factor(float f) noexcept
			{ 
				// Set the maximum load factor
				// Load factor must be between 0.1 and 0.95
				d_load_factor = f;
				if (d_load_factor > 0.95f) d_load_factor = 0.95f;
				else if (d_load_factor < 0.1f ) d_load_factor = 0.1f;
				d_next_target = static_cast<size_t>(bucket_size() * (double)d_load_factor);
			}
			SEQ_ALWAYS_INLINE float load_factor()const noexcept 
			{
				// Returns the current load factor
				return (float)size() / (float)bucket_size(); 
			}
			SEQ_ALWAYS_INLINE size_t bucket_size() const noexcept 
			{
				// Returns the node array size
				return  d_hash_mask+1; 
			}
			

			template< class... Args >
			const_iterator find_linear_hash(size_t hash, Args&&... args) const noexcept
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
			SEQ_ALWAYS_INLINE const_iterator find_hash(size_t hash, Args&&... args) const
			{
				// Key lookup

				// Check for linear probing and dirty cases
				if (SEQ_UNLIKELY(d_max_dist == node_type::max_distance || dirty()))
					return find_linear_hash(hash, std::forward<Args>(args)...);

				size_t index = (hash & d_hash_mask);
				dist_type dist = 0;
				const tiny_hash h = node_type::small_hash(hash);

				// Probe until null node or until a node with a greater distance than the current one
				while (dist++ <= d_buckets[index].distance())
				{
					// Check for eaquality (first the hash part and then the key itself).
					if (h == d_buckets[index].hash() && (*this)(extract_key::key(d_buckets[index].value()), std::forward<Args>(args)...))
						return const_iterator(d_buckets[index].node(), d_buckets[index].pos());
					if (SEQ_UNLIKELY(++index == bucket_size()))
						index = 0;
				}
				// Failed lookup
				return d_seq.end();
			}
			template< class... Args >
			SEQ_ALWAYS_INLINE const_iterator find(Args&&... args) const   
			{
				return find_hash(hash_key(std::forward<Args>(args)...), std::forward<Args>(args)...);
			}

			template<Location loc, class... Args >
			std::pair<iterator, bool> insert_linear(Args&&... args)
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
			std::pair<iterator,bool> insert(Args&&... args)
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
				
				// Insert in sequence
				iterator tmp_it = Inserter<sequence_type, loc, store_type>::insert(d_seq, value);

				// Move nodes based on distance (robin hood hashing), only if computed distance is not null
				node_type n = d_buckets[index];
				d_buckets[index] = node_type(h, dist, tmp_it);
				if (dist ) 
					start_insert(d_buckets,bucket_size(), index, dist, n);
				
				return std::pair< iterator, bool>(tmp_it,true);
			}

			template<Location loc, class... Args >
			std::pair<iterator, bool> insert_no_check(Args&&... args)
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
				if (size_t count = distance(last, first)) 
					reserve(size() + count);
					
				// Insert one by one.
				// It would be faster in most scenario to insert everything in the sequence
				// and then rehashing while removing duplicates. However, it would be way slower in some scenarios,
				// like small range or lots of similar keys.
				for (; first != last; ++first)
					insert<Anywhere>(*first);
			}
			
			iterator erase_hash(size_t hash, const_iterator it)
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

				if (d_max_dist == node_type::max_distance)
				{
					// Pure linear hash map: use tombstone
					prev->empty_tombstone();
				}
				else
				{
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
				}
				
				// Erase in sequence
				return d_seq.erase(it);
			}
			iterator erase(const_iterator it)
			{
				return erase_hash(hash_key(extract_key::key (*it)), it);
			}

			template<class K>
			size_t erase(const K& key)
			{
				size_t hash = hash_key(key);
				const_iterator it = find_hash(hash, key);
				if (it == d_seq.end()) return 0;
				erase_hash(hash, it);
				return 1;
			}

			iterator erase(const_iterator first, const_iterator last)
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








	/// @brief Transparent comparison functor, in case std::equal_to<void> is not available
	///
	struct equal_to_t
	{
		using is_transparent = std::true_type;
		template<class U, class V>
		bool operator()(const U& u, const V& v) const {
			return u == v;
		}
	};
	/// @brief Transparent hash functor using std::hash<T>
	///
	template<class T>
	struct hash_t
	{
		using is_transparent = std::true_type;
		template<class U>
		size_t operator()(const U & value) const { return std::hash<T>{}(value); }
	};



	/// @brief Associative container that contains a set of unique objects of type Key. Search, insertion, and removal have average constant-time complexity.
	/// @tparam Key Key type
	/// @tparam Hash Hash function
	/// @tparam KeyEqual Equality comparison function
	/// @tparam Allocator allocator object
	/// @tparam Layout memory layout used by the underlying sequence object
	/// 
	/// seq::ordered_set is a open addressing hash table using robin hood hashing and backward shift deletion. Its main properties are:
	///		-	Keys are ordered by insertion order. Therefore, ordered_set provides the additional members push_back(), push_front(), emplace_back() and emplace_front() to constrol key ordering.
	///		-	Since the container is ordered, it is also sortable. ordered_set provides the additional members sort() and stable_sort() for this purpose.
	///		-	The hash table itself basically stores iterators to a seq::sequence object storing the actual values. Therefore, seq::ordered_set provides <b>stable references
	///			and iterators, even on rehash</b> (unlike std::unordered_set that invalidates iterators on rehash).
	///		-	No memory peak on rehash.
	///		-	seq::ordered_set uses robin hood probing with backard shift deletion. It does not rely on tombstones and supports high load factors.
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
	/// independant sequence object) and avoid memory peaks.
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
	/// <a href="https://github.com/martinus/robin-hood-hashing">robin_hood::unordered_node_set</a> and <a href="https://github.com/greg7mdp/parallel-hashmap">phmap::node_hash_set</a> (based on abseil hash table).
	/// The following table show the results when compiled with gcc 10.1.0 (-O3) for msys2 on Windows 10,
	/// using Intel(R) Core(TM) i7-10850H at 2.70GHz. Measured operations are:
	///		-	Insert successfully 5M unique double randomly shuffled in an empty table using hash_table::insert()
	///		-	Insert successfully 5M unique double randomly shuffled in an empty table using hash_table::insert() after reserving enough space
	///		-	Successfully search for 5M double in random order using hash_table::find(const Key&)
	///		-	Search for 5M double not present in the table (failed lookup)
	///		-	Walk through the full table (5M double) using iterators
	///		-	Erase half the table in random order using hash_table::erase(iterator)
	///		-	Perform mixed failed/successfull lookups on the remaining 2.5M keys.
	/// 
	/// For each tested hash table, the maximum load factor is left to its default value, and std::hash<double> is used. For insert and erase operations,
	/// the program memory consumption is given. Note that the memory consumption is not the exact memory usage of the hash table, and should only bu used
	/// to measure the difference between implementations.
	/// 
	/// Hash table name                | Insert        | Insert (reserve) | Find (success)| Find (failed)  | Iterate     | Erase	     |  Find again |
	/// -------------------------------|---------------|------------------|---------------|----------------|-------------|---------------|-------------|
	/// seq::ordered_set               | 477 ms/145 MO |  302 ms/145 MO   |   282 ms      |    188 ms      |    6 ms     | 269 ms/210 MO |  166 ms     |
	/// phmap::node_hash_set           | 926 ms/187 MO |  492 ms/188 MO   |   370 ms      |    130 ms      |    96 ms    | 501 ms/232 MO |  193 ms     |
	/// robin_hood::unordered_node_set | 598 ms/182 MO |  448 ms/182 MO   |   394 ms      |    134 ms      |    87 ms    | 407 ms/259 MO |  212 ms     |
	/// ska::unordered_set             | 1540 ms/256 MO|  728 ms/256 MO   |   300 ms      |    261 ms      |   141 ms    | 513 ms/268 MO |  204 ms     |
	/// std::unordered_set             | 1844 ms/238 MO| 1148 ms/231 MO   |   751 ms      |    941 ms      |   205 ms    | 868 ms/245 MO |  509 ms     |
	///  
	/// seq::ordered_set is significantly faster than the other hash tables except for failed lookup, and has a lower memory overhead.
	/// Note that this benchmark does not represent all possible workloads, and additional tests must be fullfilled for specific scenarios.
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
			SEQ_ALWAYS_INLINE const_iterator& operator++() noexcept {
				++iter;
				return *this;
			}
			SEQ_ALWAYS_INLINE const_iterator operator++(int) noexcept {
				const_iterator _Tmp = *this;
				++(*this);
				return _Tmp;
			}
			SEQ_ALWAYS_INLINE const_iterator& operator--() noexcept {
				--iter;
				return *this;
			}
			SEQ_ALWAYS_INLINE const_iterator operator--(int) noexcept {
				const_iterator _Tmp = *this;
				--(*this);
				return _Tmp;
			}
			SEQ_ALWAYS_INLINE reference operator*() const noexcept { return *iter; }
			SEQ_ALWAYS_INLINE pointer operator->() const noexcept { return  std::pointer_traits<pointer>::pointer_to(**this); }
			SEQ_ALWAYS_INLINE const_iterator& operator+=(difference_type diff) noexcept {
				iter += diff;
				return *this;
			}
			SEQ_ALWAYS_INLINE const_iterator& operator-=(difference_type diff) noexcept {
				iter -= diff;
				return *this;
			}
			SEQ_ALWAYS_INLINE bool operator==(const const_iterator& it) const { return iter == it.iter; }
			SEQ_ALWAYS_INLINE bool operator!=(const const_iterator& it) const { return iter != it.iter; }
			SEQ_ALWAYS_INLINE const_iterator operator+(difference_type diff) { return iter + diff; }
			SEQ_ALWAYS_INLINE const_iterator operator-(difference_type diff) { return iter + diff; }
			SEQ_ALWAYS_INLINE difference_type operator-(const const_iterator& other) { return iter - other.iter; }
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
			:base_type(hash,equal,alloc)
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
			this->d_seq = other.sequence();
			if (other.dirty()) base_type::mark_dirty();
			max_load_factor(other.max_load_factor());
			rehash();
		}
		/// @brief Copy constructor
		/// @param other another container to be used as source to initialize the elements of the container with
		ordered_set(const ordered_set& other)
			:ordered_set(other, other.get_allocator())
		{
			this->d_seq = other.sequence();
			if (other.dirty()) base_type::mark_dirty();
			max_load_factor(other.max_load_factor());
			rehash();
		}
		/// @brief Move constructor
		/// @param other another container to be used as source to initialize the elements of the container with
		ordered_set(ordered_set&& other)
			:base_type(std::move(other))
		{}
		/// @brief Move constructor
		/// @param other another container to be used as source to initialize the elements of the container with
		/// @param alloc allocator to use for all memory allocations of this container
		ordered_set(ordered_set&& other, const Allocator& alloc)
			:base_type(std::move(other),alloc)
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
		ordered_set& operator=(const ordered_set& other)
		{
			this->d_seq = other.sequence();
			if (other.dirty()) this->mark_dirty();
			max_load_factor(other.max_load_factor());
			rehash();
			return *this;
		}

		/// @brief Move assignment operator
		ordered_set& operator=( ordered_set&& other)
		{
			base_type::swap(other);
			return *this;
		}

		/// @brief Returns the container size
		size_t size() const noexcept { return this->d_seq.size(); }
		/// @brief Returns the container maximum size
		size_t max_size() const noexcept { return this->d_seq.max_size(); }
		/// @brief Returns true if the container is empty, false otherwise
		bool empty() const noexcept { return this->d_seq.empty(); }

		/// @brief Returns the current maximum possible probe distance
		int max_probe_distance() const noexcept { return this->d_max_dist; }

		/// @brief Returns the current load factor
		float load_factor() const noexcept { return base_type::load_factor(); }
		/// @brief Returns the current maximum load factor
		float max_load_factor() const noexcept { return base_type::max_load_factor(); }
		/// @brief Set the maximum load factor
		void max_load_factor(float f) noexcept { base_type::max_load_factor(f); }

		/// @brief Returns the container allocator object
		allocator_type& get_allocator() noexcept { return this->d_seq.get_allocator(); }
		/// @brief Returns the container allocator object
		allocator_type get_allocator() const noexcept { return this->d_seq.get_allocator(); }

		/// @brief Returns the hash function
		hasher hash_function() const { return this->base_type::hash_function(); }
		/// @brief Returns the equality comparison function
		key_equal key_eq() const { return this->base_type::key_eq(); }

		/// @brief Returns the underlying sequence object.
		/// Calling this function will mark the container as dirty. Any further attempts to call members like find() or insert() (relying on the hash function)
		/// will raise a std::logic_error. To mark the container as non dirty anymore, the user must call ordered_set::rehash().
		/// @return a reference to the underlying seq::sequence object
		sequence_type& sequence() noexcept { this->base_type::mark_dirty(); return this->d_seq; }
		/// @brief Returns the underlying sequence object. Do NOT mark the container as dirty.
		const sequence_type & sequence() const noexcept { return this->d_seq; }
		/// @brief Returns the underlying sequence object. Do NOT mark the container as dirty.
		const sequence_type & csequence() const noexcept { return this->d_seq; }

		/// @brief Returns an iterator to the first element of the container.
		iterator end() noexcept { return this->d_seq.end(); }
		/// @brief Returns an iterator to the first element of the container.
		const_iterator end() const noexcept { return this->d_seq.end(); }
		/// @brief Returns an iterator to the first element of the container.
		const_iterator cend() const noexcept { return this->d_seq.end(); }
		
		/// @brief Returns an iterator to the element following the last element of the container.
		iterator begin() noexcept { return this->d_seq.begin(); }
		/// @brief Returns an iterator to the element following the last element of the container.
		const_iterator begin() const noexcept { return this->d_seq.begin(); }
		/// @brief Returns an iterator to the element following the last element of the container.
		const_iterator cbegin() const noexcept { return this->d_seq.begin(); }

		/// @brief Returns a reverse iterator to the first element of the reversed list.
		reverse_iterator rbegin() noexcept { return this->d_seq.rbegin(); }
		/// @brief Returns a reverse iterator to the first element of the reversed list.
		const_reverse_iterator rbegin() const noexcept  { return this->d_seq.rbegin(); }
		/// @brief Returns a reverse iterator to the first element of the reversed list.
		const_reverse_iterator crbegin() const noexcept  { return this->d_seq.rbegin(); }

		/// @brief Returns a reverse iterator to the element following the last element of the reversed list.
		reverse_iterator rend() noexcept { return this->d_seq.rend(); }
		/// @brief Returns a reverse iterator to the element following the last element of the reversed list.
		const_reverse_iterator rend() const noexcept { return this->d_seq.rend(); }
		/// @brief Returns a reverse iterator to the element following the last element of the reversed list.
		const_reverse_iterator crend() const noexcept { return this->d_seq.rend(); }

		/// @brief Clear the container
		void clear()
		{
			this->base_type::clear();
		}

		/// @brief Rehash the container.
		/// This function triggers a full rehash if:
		/// 
		///		-	the container is dirty (through a call to ordered_set::sequence()
		///		-	the hash table size is too big or too small considering the max load factor (for instance after several calls to ordered_set::erase())
		///		-	the hash table is in linear hashing state (because of bad hash function). In this case, the hash table size is doubled.
		/// 
		/// Otherwise, this function does nothing.
		/// 
		void rehash()
		{
			this->base_type::rehash();
		}
		
		/// @brief Sets the number of nodes to the number needed to accomodate at least count elements without exceeding maximum load factor and rehashes the container.
		/// @param count new capacity of the container
		void reserve(size_t count)
		{
			this->base_type::reserve(count);
		}

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
		std::pair<iterator, bool> emplace(Args&&... args)
		{
			return this->base_type::template insert<detail::Anywhere>(std::forward<Args>(args)...);
		}
		/// @brief Inserts a new element into the container constructed in-place with the given args if there is no element with the key in the container.
		/// Same as ordered_set::emplace().
		template <class... Args>
		iterator emplace_hint(const_iterator hint, Args&&... args)
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
		std::pair<iterator, bool> insert(const value_type& value)
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
		std::pair<iterator, bool> insert( value_type&& value)
		{
			return this->base_type::template insert<detail::Anywhere>(std::move(value));
		}
		/// @brief Inserts element into the container, if the container doesn't already contain an element with an equivalent key.
		/// Same as ordered_set::insert().
		iterator insert(const_iterator hint, const value_type& value)
		{
			(void)hint;
			return insert(value).first;
		}
		/// @brief Inserts element into the container using move semantic, if the container doesn't already contain an element with an equivalent key.
		/// Same as ordered_set::insert().
		iterator insert(const_iterator hint, value_type&& value)
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
		std::pair<iterator, bool> emplace_back(Args&&... args)
		{
			return this->base_type::template insert<detail::Back>(std::forward<Args>(args)...);
		}
		/// @brief Inserts element at the back of the container, if the container doesn't already contain an element with an equivalent key.
		/// Iterators and references are not invalidated. Rehashing occurs only if the new number of elements is greater than max_load_factor()*size().
		/// @param value value to be inserted
		/// @return pair consisting of an iterator to the inserted element, or the already-existing element if no insertion happened, 
		/// and a bool denoting whether the insertion took place (true if insertion happened, false if it did not).
		std::pair<iterator, bool> push_back(const value_type& value)
		{
			return this->base_type::template insert<detail::Back>(value);
		}
		/// @brief Inserts element at the back of the container use move semantic, if the container doesn't already contain an element with an equivalent key.
		/// Iterators and references are not invalidated. Rehashing occurs only if the new number of elements is greater than max_load_factor()*size().
		/// @param value value to be inserted
		/// @return pair consisting of an iterator to the inserted element, or the already-existing element if no insertion happened, 
		/// and a bool denoting whether the insertion took place (true if insertion happened, false if it did not).
		std::pair<iterator, bool> push_back(value_type&& value)
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
		std::pair<iterator, bool> emplace_front(Args&&... args)
		{
			return this->base_type::template insert<detail::Front>(std::forward<Args>(args)...);
		}
		/// @brief Inserts element at the front of the container, if the container doesn't already contain an element with an equivalent key.
		/// Iterators and references are not invalidated. Rehashing occurs only if the new number of elements is greater than max_load_factor()*size().
		/// @param value value to be inserted
		/// @return pair consisting of an iterator to the inserted element, or the already-existing element if no insertion happened, 
		/// and a bool denoting whether the insertion took place (true if insertion happened, false if it did not).
		std::pair<iterator, bool> push_front(const value_type& value)
		{
			return this->base_type::template insert<detail::Front>(value);
		}
		/// @brief Inserts element at the front of the container using move semantic, if the container doesn't already contain an element with an equivalent key.
		/// Iterators and references are not invalidated. Rehashing occurs only if the new number of elements is greater than max_load_factor()*size().
		/// @param value value to be inserted
		/// @return pair consisting of an iterator to the inserted element, or the already-existing element if no insertion happened, 
		/// and a bool denoting whether the insertion took place (true if insertion happened, false if it did not).
		std::pair<iterator, bool> push_front(value_type&& value)
		{
			return this->base_type::template insert<detail::Front>(std::move(value));
		}


		/// @brief Erase element at given location.
		/// Iterators and references are not invalidated. Rehashing never occurs.
		/// @param pos iterator to the element to erase
		/// @return iterator to the next element
		iterator erase(const_iterator pos)
		{
			return this->base_type::erase(pos.iter);
		}
		/// @brief Erase element comparing equal to given key (if any).
		/// Iterators and references are not invalidated. Rehashing never occurs.
		/// @param key key to be erased
		/// @return number of erased elements (0 or 1)
		size_type erase(const Key& key)
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
		size_type erase(const K& x)
		{
			return this->base_type::erase(x);
		}
		/// @brief Removes the elements in the range [first; last), which must be a valid range in *this.
		/// @param first range of elements to remove
		/// @param last range of elements to remove
		/// @return Iterator following the last removed element
		iterator erase(const_iterator first, const_iterator last)
		{
			return this->base_type::erase(first.iter,last.iter);
		}
		

		/// @brief Finds an element with key equivalent to key
		/// @param value key value to search for
		/// @return iterator pointing to found key on success, end iterator on failure.
		SEQ_ALWAYS_INLINE const_iterator find(const Key& key) const
		{
			return this->base_type::find(key);
		}
		/// @brief Finds an element with key equivalent to key
		/// @param value key value to search for
		/// @return iterator pointing to found key on success, end iterator on failure.
		SEQ_ALWAYS_INLINE iterator find(const Key& value) 
		{
			return (iterator)const_cast<this_type*>(this)->base_type::find(value);
		}
		/// @brief Finds an element with key equivalent to key.
		/// Finds an element with key that compares equivalent to the value x. 
		/// This overload participates in overload resolution only if Hash::is_transparent and KeyEqual::is_transparent are valid and each denotes a type. 
		/// This assumes that such Hash is callable with both K and Key type, and that the KeyEqual is transparent, which, together, allows calling this function without constructing an instance of Key.
		/// @param value key value to search for
		/// @return iterator pointing to found key on success, end iterator on failure.
		template <class K, class KE = KeyEqual, class H = Hash,
			typename std::enable_if<has_is_transparent<KE>::value && has_is_transparent<H>::value>::type* = nullptr>
		SEQ_ALWAYS_INLINE	const_iterator find(const K& x) const
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
		SEQ_ALWAYS_INLINE	iterator find(const K& key)
		{
			return (iterator)const_cast<this_type*>(this)->base_type::find(key);
		}

		/// @brief Returns 1 of key exists, 0 otherwise
		size_type count(const Key& key) const
		{
			return find(key) != end();
		}
		/// @brief Returns 1 of key exists, 0 otherwise
		template <class K, class KE = KeyEqual, class H = Hash,
			typename std::enable_if<has_is_transparent<KE>::value&& has_is_transparent<H>::value>::type* = nullptr>
			size_type count(const K& key) const
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
		class Allocator = std::allocator< std::pair<const Key, T> >,
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
			bool operator()(const U& v1, const V& v2) const {
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
			SEQ_ALWAYS_INLINE const_iterator& operator++() noexcept {
				++iter;
				return *this;
			}
			SEQ_ALWAYS_INLINE const_iterator operator++(int) noexcept {
				const_iterator _Tmp = *this;
				++(*this);
				return _Tmp;
			}
			SEQ_ALWAYS_INLINE const_iterator& operator--() noexcept {
				--iter;
				return *this;
			}
			SEQ_ALWAYS_INLINE const_iterator operator--(int) noexcept {
				const_iterator _Tmp = *this;
				--(*this);
				return _Tmp;
			}
			SEQ_ALWAYS_INLINE reference operator*() const noexcept { return reinterpret_cast<const value_type&>(*iter); }
			SEQ_ALWAYS_INLINE pointer operator->() const noexcept { return  std::pointer_traits<pointer>::pointer_to(**this); }
			SEQ_ALWAYS_INLINE const_iterator& operator+=(difference_type diff) noexcept {
				iter += diff;
				return *this;
			}
			SEQ_ALWAYS_INLINE const_iterator& operator-=(difference_type diff) noexcept {
				iter -= diff;
				return *this;
			}
			SEQ_ALWAYS_INLINE bool operator==(const const_iterator& it) const { return iter == it.iter; }
			SEQ_ALWAYS_INLINE bool operator!=(const const_iterator& it) const { return iter != it.iter; }
			SEQ_ALWAYS_INLINE const_iterator operator+(difference_type diff) { return iter + diff; }
			SEQ_ALWAYS_INLINE const_iterator operator-(difference_type diff) { return iter + diff; }
			SEQ_ALWAYS_INLINE difference_type operator-(const const_iterator& other) { return iter - other.iter; }
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
			SEQ_ALWAYS_INLINE iterator& operator++() noexcept {
				++this->iter;
				return *this;
			}
			SEQ_ALWAYS_INLINE iterator operator++(int) noexcept {
				iterator _Tmp = *this;
				++(*this);
				return _Tmp;
			}
			SEQ_ALWAYS_INLINE iterator& operator--() noexcept {
				--this->iter;
				return *this;
			}
			SEQ_ALWAYS_INLINE iterator operator--(int) noexcept {
				iterator _Tmp = *this;
				--(*this);
				return _Tmp;
			}
			SEQ_ALWAYS_INLINE reference operator*() noexcept { return reinterpret_cast<value_type&>(const_cast<std::pair<Key, T>&>(*this->iter)); }
			SEQ_ALWAYS_INLINE pointer operator->() noexcept { return  std::pointer_traits<pointer>::pointer_to(**this); }
			SEQ_ALWAYS_INLINE iterator& operator+=(difference_type diff) noexcept {
				this->iter += diff;
				return *this;
			}
			SEQ_ALWAYS_INLINE iterator& operator-=(difference_type diff) noexcept {
				this->iter -= diff;
				return *this;
			}
			SEQ_ALWAYS_INLINE bool operator==(const const_iterator& it) const { return this->iter == it.iter; }
			SEQ_ALWAYS_INLINE bool operator!=(const const_iterator& it) const { return this->iter != it.iter; }
			SEQ_ALWAYS_INLINE iterator operator+(difference_type diff) { return this->iter + diff; }
			SEQ_ALWAYS_INLINE iterator operator-(difference_type diff) { return this->iter + diff; }
			SEQ_ALWAYS_INLINE difference_type operator-(const const_iterator& other) { return this->iter - other.iter; }
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
			this->d_seq = other.sequence();
			if (other.dirty()) base_type::mark_dirty();
			max_load_factor(other.max_load_factor());
			rehash();
		}
		ordered_map(const ordered_map& other)
			:ordered_map(other, other.get_allocator())
		{
			this->d_seq = other.sequence();
			if (other.dirty()) base_type::mark_dirty();
			max_load_factor(other.max_load_factor());
			rehash();
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

		ordered_map& operator=(const ordered_map& other)
		{
			this->d_seq = other.sequence();
			if (other.dirty()) this->mark_dirty();
			max_load_factor(other.max_load_factor());
			rehash();
			return *this;
		}
		ordered_map& operator=(ordered_map&& other)
		{
			base_type::swap(other);
			return *this;
		}

		size_t size() const noexcept { return this->d_seq.size(); }
		size_t max_size() const noexcept { return this->d_seq.max_size(); }
		bool empty() const noexcept { return this->d_seq.empty(); }

		int max_probe_distance() const noexcept { return this->d_max_dist; }

		float load_factor() const noexcept { return base_type::load_factor(); }
		float max_load_factor() const noexcept { return base_type::max_load_factor(); }
		void max_load_factor(float f) noexcept { base_type::max_load_factor(f); }

		allocator_type& get_allocator() noexcept { return this->d_seq.get_allocator(); }
		allocator_type get_allocator() const noexcept { return this->d_seq.get_allocator(); }

		hasher hash_function() const { return this->base_type::hash_function(); }
		key_equal key_eq() const { return this->base_type::key_eq(); }

		sequence_type& sequence() noexcept { this->base_type::mark_dirty(); return this->d_seq; }
		const sequence_type& sequence() const noexcept { return this->d_seq; }
		const sequence_type& csequence() const noexcept { return this->d_seq; }

		iterator end() noexcept { return this->d_seq.end(); }
		const_iterator end() const noexcept { return this->d_seq.end(); }
		const_iterator cend() const noexcept { return this->d_seq.end(); }

		iterator begin() noexcept { return this->d_seq.begin(); }
		const_iterator begin() const noexcept { return this->d_seq.begin(); }
		const_iterator cbegin() const noexcept { return this->d_seq.begin(); }

		reverse_iterator rbegin() noexcept { return this->d_seq.rbegin(); }
		const_reverse_iterator rbegin() const noexcept { return this->d_seq.rbegin(); }
		const_reverse_iterator crbegin() const noexcept { return this->d_seq.rbegin(); }

		reverse_iterator rend() noexcept { return this->d_seq.rend(); }
		const_reverse_iterator rend() const noexcept { return this->d_seq.rend(); }
		const_reverse_iterator crend() const noexcept { return this->d_seq.rend(); }

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
		std::pair<iterator, bool> emplace(Args&&... args)
		{
			return this->base_type::template insert<detail::Anywhere>(std::forward<Args>(args)...);
		}
		template <class... Args>
		iterator emplace_hint(const_iterator hint, Args&&... args)
		{
			(void)hint;
			return this->base_type::template insert<detail::Anywhere>(std::forward<Args>(args)...).first;
		}
		std::pair<iterator, bool> insert(const value_type& value)
		{
			return this->base_type::template insert<detail::Anywhere>(value);
		}
		std::pair<iterator, bool> insert(value_type&& value)
		{
			return this->base_type::template insert<detail::Anywhere>(std::move(value));
		}
		template< class P >
		std::pair<iterator, bool> insert(P&& value)
		{
			return this->base_type::template insert<detail::Anywhere>(std::forward<P>(value));
		}
		iterator insert(const_iterator hint, const value_type& value)
		{
			(void)hint;
			return insert(value).first;
		}
		iterator insert(const_iterator hint, value_type&& value)
		{
			(void)hint;
			return insert(std::move(value)).first;
		}
		template< class P >
		iterator insert(const_iterator hint, P&& value)
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
		std::pair<iterator, bool> insert_or_assign(const Key& k, M&& obj)
		{
			auto it = find(k);
			if (it != end()) {
				it->second = std::forward<M>(obj);
				return std::pair<iterator, bool>(it, false);
			}
			return this->base_type::template insert_no_check<detail::Anywhere>(k, std::forward<M>(obj));
		}
		template <class M>
		std::pair<iterator, bool> insert_or_assign(Key&& k, M&& obj)
		{
			auto it = find(k);
			if (it != end()) {
				it->second = std::forward<M>(obj);
				return std::pair<iterator, bool>(it, false);
			}
			return this->base_type::template insert_no_check<detail::Anywhere>(std::move(k), std::forward<M>(obj));
		}
		template <class M>
		iterator insert_or_assign(const_iterator hint, const Key& k, M&& obj)
		{
			(void)hint;
			return insert_or_assign(k, std::forward<M>(obj)).first;
		}
		template <class M>
		iterator insert_or_assign(const_iterator hint, Key&& k, M&& obj)
		{
			(void)hint;
			return insert_or_assign(std::move(k), std::forward<M>(obj)).first;
		}


		template <class M>
		std::pair<iterator, bool> push_back_or_assign(const Key& k, M&& obj)
		{
			auto it = find(k);
			if (it != end()) {
				it->second = std::forward<M>(obj);
				return std::pair<iterator, bool>(it, false);
			}
			return this->base_type::template insert_no_check<detail::Back>(k, std::forward<M>(obj));
		}
		template <class M>
		std::pair<iterator, bool> push_back_or_assign(Key&& k, M&& obj)
		{
			auto it = find(k);
			if (it != end()) {
				it->second = std::forward<M>(obj);
				return std::pair<iterator, bool>(it, false);
			}
			return this->base_type::template insert_no_check<detail::Back>(std::move(k), std::forward<M>(obj));
		}
		template <class M>
		iterator push_back_or_assign(const_iterator hint, const Key& k, M&& obj)
		{
			(void)hint;
			return push_back_or_assign(k, std::forward<M>(obj)).first;
		}
		template <class M>
		iterator push_back_or_assign(const_iterator hint, Key&& k, M&& obj)
		{
			(void)hint;
			return push_back_or_assign(std::move(k), std::forward<M>(obj)).first;
		}



		template <class M>
		std::pair<iterator, bool> push_front_or_assign(const Key& k, M&& obj)
		{
			auto it = find(k);
			if (it != end()) {
				it->second = std::forward<M>(obj);
				return std::pair<iterator, bool>(it, false);
			}
			return this->base_type::template insert_no_check<detail::Front>(k, std::forward<M>(obj));
		}
		template <class M>
		std::pair<iterator, bool> push_front_or_assign(Key&& k, M&& obj)
		{
			auto it = find(k);
			if (it != end()) {
				it->second = std::forward<M>(obj);
				return std::pair<iterator, bool>(it, false);
			}
			return this->base_type::template insert_no_check<detail::Front>(std::move(k), std::forward<M>(obj));
		}
		template <class M>
		iterator push_front_or_assign(const_iterator hint, const Key& k, M&& obj)
		{
			(void)hint;
			return push_front_or_assign(k, std::forward<M>(obj)).first;
		}
		template <class M>
		iterator push_front_or_assign(const_iterator hint, Key&& k, M&& obj)
		{
			(void)hint;
			return push_front_or_assign(std::move(k), std::forward<M>(obj)).first;
		}


		template< class... Args >
		std::pair<iterator, bool> emplace_back(Args&&... args)
		{
			return this->base_type::template insert<detail::Back>(std::forward<Args>(args)...);
		}
		std::pair<iterator, bool> push_back(const value_type& value)
		{
			return this->base_type::template insert<detail::Back>(value);
		}
		std::pair<iterator, bool> push_back(value_type&& value)
		{
			return this->base_type::template insert<detail::Back>(std::move(value));
		}
		template<class P>
		std::pair<iterator, bool> push_back(P&& value)
		{
			return this->base_type::template insert<detail::Back>(std::forward<P>(value));
		}

		template< class... Args >
		std::pair<iterator, bool> emplace_front(Args&&... args)
		{
			return this->base_type::template insert<detail::Front>(std::forward<Args>(args)...);
		}
		std::pair<iterator, bool> push_front(const value_type& value)
		{
			return this->base_type::template insert<detail::Front>(value);
		}
		std::pair<iterator, bool> push_front(value_type&& value)
		{
			return this->base_type::template insert<detail::Front>(std::move(value));
		}
		template<class P>
		std::pair<iterator, bool> push_front(P&& value)
		{
			return this->base_type::template insert<detail::Front>(std::forward<P>(value));
		}




		template< class... Args >
		std::pair<iterator, bool> try_emplace(const Key& k, Args&&... args)
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
		std::pair<iterator, bool> try_emplace(Key&& k, Args&&... args)
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
		iterator try_emplace(const_iterator hint, const Key& k, Args&&... args)
		{
			(void)hint;
			return try_emplace(k, std::forward<Args>(args)...).first;
		}
		template< class... Args >
		iterator try_emplace(const_iterator hint, Key&& k, Args&&... args)
		{
			(void)hint;
			return try_emplace(std::move(k), std::forward<Args>(args)...).first;
		}




		template< class... Args >
		std::pair<iterator, bool> try_emplace_back(const Key& k, Args&&... args)
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
		std::pair<iterator, bool> try_emplace_back(Key&& k, Args&&... args)
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
		iterator try_emplace_back(const_iterator hint, const Key& k, Args&&... args)
		{
			(void)hint;
			return try_emplace_back(k, std::forward<Args>(args)...).first;
		}
		template< class... Args >
		iterator try_emplace_back(const_iterator hint, Key&& k, Args&&... args)
		{
			(void)hint;
			return try_emplace_back(std::move(k), std::forward<Args>(args)...).first;
		}




		template< class... Args >
		std::pair<iterator, bool> try_emplace_front(const Key& k, Args&&... args)
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
		std::pair<iterator, bool> try_emplace_front(Key&& k, Args&&... args)
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
		iterator try_emplace_front(const_iterator hint, const Key& k, Args&&... args)
		{
			(void)hint;
			return try_emplace_front(k, std::forward<Args>(args)...).first;
		}
		template< class... Args >
		iterator try_emplace_front(const_iterator hint, Key&& k, Args&&... args)
		{
			(void)hint;
			return try_emplace_front(std::move(k), std::forward<Args>(args)...).first;
		}



		T& at(const Key& key)
		{
			auto it = find(key);
			if (it == end())
				throw std::out_of_range("ordered_map: key not found");
			return it->second;
		}
		const T& at(const Key& key) const
		{
			auto it = find(key);
			if (it == end())
				throw std::out_of_range("ordered_map: key not found");
			return it->second;
		}



		T& operator[](const Key& key)
		{
			auto it = find(key);
			if (it != end())
				return it->second;
			return this->base_type::template insert_no_check<detail::Anywhere>(
				value_type(std::piecewise_construct, std::forward_as_tuple(key), std::tuple<>())).first->second;
		}
		T& operator[](Key&& key)
		{
			auto it = find(key);
			if (it != end())
				return it->second;
			return this->base_type::template insert_no_check<detail::Anywhere>(
				value_type(std::piecewise_construct, std::forward_as_tuple(std::move(key)), std::tuple<>())).first->second;
		}



		iterator erase(const_iterator pos)
		{
			return this->base_type::erase(pos.iter);
		}
		size_type erase(const Key& key)
		{
			return this->base_type::erase(key);
		}
		template <class K, class KE = KeyEqual, class H = Hash,
			typename std::enable_if<has_is_transparent<KE>::value&& has_is_transparent<H>::value>::type* = nullptr>
			size_type erase(const K& key)
		{
			return this->base_type::erase(key);
		}
		iterator erase(const_iterator first, const_iterator last)
		{
			return this->base_type::erase(first.iter, last.iter);
		}

		SEQ_ALWAYS_INLINE const_iterator find(const Key& value) const
		{
			return this->base_type::find(value);
		}
		SEQ_ALWAYS_INLINE iterator find(const Key& value)
		{
			return (iterator)const_cast<this_type*>(this)->base_type::find(value);
		}

		template <class K, class KE = KeyEqual, class H = Hash,
			typename std::enable_if<has_is_transparent<KE>::value&& has_is_transparent<H>::value>::type* = nullptr>
			SEQ_ALWAYS_INLINE	const_iterator find(const K& key) const
		{
			return const_cast<this_type*>(this)->base_type::find(key);
		}
		template <class K, class KE = KeyEqual, class H = Hash,
			typename std::enable_if<has_is_transparent<KE>::value&& has_is_transparent<H>::value>::type* = nullptr>
			SEQ_ALWAYS_INLINE	iterator find(const K& key)
		{
			return (iterator)const_cast<this_type*>(this)->base_type::find(key);
		}


		size_type count(const Key& key) const
		{
			return find(key) != end();
		}

		template <class K, class KE = KeyEqual, class H = Hash,
			typename std::enable_if<has_is_transparent<KE>::value&& has_is_transparent<H>::value>::type* = nullptr>
			size_type count(const K& key) const
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
		bool operator == (const ordered_set<Key, Hash1, KeyEqual, Allocator1, Layout1>& lhs, const ordered_set<Key, Hash2, KeyEqual, Allocator2, Layout2>& rhs)
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
		bool operator != (const ordered_set<Key, Hash1, KeyEqual, Allocator1, Layout1>& lhs, const ordered_set<Key, Hash2, KeyEqual, Allocator2, Layout2>& rhs)
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
	size_t erase_if(ordered_set<Key, Hash1, KeyEqual, Allocator1, Layout1>& set, Pred p)
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
		bool operator == (const ordered_map<Key, T, Hash1, KeyEqual, Allocator1, Layout1>& lhs, const ordered_map<Key, T, Hash2, KeyEqual, Allocator2, Layout2>& rhs)
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
		bool operator != (const ordered_map<Key, T, Hash1, KeyEqual, Allocator1, Layout1>& lhs, const ordered_map<Key, T, Hash2, KeyEqual, Allocator2, Layout2>& rhs)
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
		size_t erase_if(ordered_map<Key, T, Hash1, KeyEqual, Allocator1, Layout1>& set, Pred p)
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
