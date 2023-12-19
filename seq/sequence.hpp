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

#ifndef SEQ_SEQUENCE_HPP
#define SEQ_SEQUENCE_HPP



/** @file */

#include <climits>

#include "memory.hpp"
#include "pdqsort.hpp"
#include "type_traits.hpp"
#include "utils.hpp"



namespace seq
{
	namespace detail
	{
		template<std::uint64_t  Count>
		struct shift_left
		{
			static constexpr std::uint64_t value = 1ULL << Count;
		};
		template<>
		struct shift_left<64ULL>
		{
			static constexpr std::uint64_t value = 0;
		} ;

		// Contiguous storage for up to 64 objects
		template< class T>
		struct base_list_chunk
		{
			// Max number of elements
			static constexpr std::uint64_t count = sizeof(T) <= 8 ? 64 : sizeof(T) <= 16 ? 32 : sizeof(T) <= 32 ? 16 : sizeof(T) <= 64 ? 8 : 4;
			// log2(count)
			static constexpr std::uint64_t count_bits = sizeof(T) <= 8 ? 6 : sizeof(T) <= 16 ? 5 : sizeof(T) <= 32 ? 4 : sizeof(T) <= 64 ? 3 : 2;
			// Mask value when full
			static constexpr std::uint64_t full = (count == 64ULL ? static_cast<std::uint64_t>(-1) : (shift_left<count>::value - 1ULL));
			// Invalid index value
			static constexpr std::int64_t no_index = LLONG_MIN;

			// Previous node
			base_list_chunk<T>* prev;
			// Next node
			base_list_chunk<T>* next;
			// Previous node with at least one free element
			base_list_chunk<T>* prev_free;
			// Next node with at least one free element
			base_list_chunk<T>* next_free;
			// Mask (bit 1 for allocated, 0 for free)
			std::uint64_t used;
			// Index in the list of node. Ordered, but not necessarly incremented by one.
			std::int64_t node_index; 
			// User define flag
			std::int64_t user_flag; 
			// Index of the first valid value
			int start;
			// Past the end index of the last valid value
			int end;
			// First free index
			SEQ_ALWAYS_INLINE auto firstFree() const noexcept -> unsigned { return bit_scan_forward_64(~used); }
			// First used index
			SEQ_ALWAYS_INLINE auto firstUsed() const noexcept -> unsigned { return bit_scan_forward_64(used); }
			// Number of valid (allocated) elements
			SEQ_ALWAYS_INLINE auto size() const noexcept -> unsigned { return popcnt64(used); }
		};


		// Actual chunk class, store up to 64 objects
		// Its size is a multiple of 64 bytes
		template< class T>
		struct list_chunk : public base_list_chunk<T>
		{
			using base_type = base_list_chunk<T>;
			using base_type::count;
			using base_type::count_bits;
			using base_type::used;
			using base_type::prev;
			using base_type::next;
			using base_type::prev_free;
			using base_type::next_free;
			using base_type::start;
			using base_type::end;

			using storage_type = typename std::aligned_storage<count * sizeof(T), alignof(T) >::type;
			// Storage for values
			storage_type storage;

			SEQ_ALWAYS_INLINE storage_type* get_storage() noexcept { return &storage; }
			SEQ_ALWAYS_INLINE const storage_type* get_storage() const noexcept { return &storage; }

			//Raw buffer access
			SEQ_ALWAYS_INLINE T* buffer() noexcept { return  reinterpret_cast<T*>(get_storage()); }
			SEQ_ALWAYS_INLINE const T* buffer() const noexcept { return  reinterpret_cast<const T*>(get_storage()); }
			SEQ_ALWAYS_INLINE T& front() { return buffer()[start]; }
			SEQ_ALWAYS_INLINE const T& front() const { return buffer()[start]; }
			SEQ_ALWAYS_INLINE T& back() { return buffer()[end - 1]; }
			SEQ_ALWAYS_INLINE const T& back() const { return buffer()[end - 1]; }
		};
	


		/// @brief Allocate list_chunk objects (64 bytes aligned) using a seq::object_pool
		/// Note that the object_pool is only used after a few allocations to avoid a too big memory overhead for small sequences
		template< class T, class Allocator = std::allocator<T> >
		class chunk_pool_alloc : public Allocator
		{
			template< class U>
			using rebind_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<U>;
			using chunk_type = list_chunk<T>;
			using pool_type = object_pool< chunk_type, Allocator, 64, linear_object_allocation< 1> >;

			std::uintptr_t d_pool;

			pool_type* pool() const noexcept{return reinterpret_cast<pool_type*>(d_pool);}
			auto can_use_aligned_alloc() const noexcept -> bool {return std::is_same < Allocator, std::allocator<T> >::value && d_pool < 4;}
			auto pool_allocated() const noexcept -> bool { return d_pool > 4; }

		public:
			chunk_pool_alloc(const Allocator& alloc) noexcept(std::is_nothrow_copy_constructible<Allocator>::value) 
				: Allocator(alloc), d_pool(0) {}
			chunk_pool_alloc(size_t elem_count, const Allocator& alloc) : Allocator(alloc), d_pool(0) {
				//resize(elem_count);
			}
			~chunk_pool_alloc() {
				if (pool_allocated()) {
					rebind_alloc< pool_type> al = get_allocator();
					destroy_ptr(pool());
					al.deallocate(pool(), 1);
				}
			}
			// disable copy, only allox move semantic
			chunk_pool_alloc(const chunk_pool_alloc&) = delete;
			auto operator=(const chunk_pool_alloc&) -> chunk_pool_alloc& = delete;

			auto get_allocator() const noexcept -> const Allocator& {
				return *this;
			}
			auto get_allocator() noexcept -> Allocator& {
				return *this;
			}

			// total memory footprint in bytes excluding sizeof(*this)
			auto memory_footprint() const noexcept -> size_t {
				return d_pool ? ( can_use_aligned_alloc() ? d_pool * sizeof(list_chunk<T>) : pool()->memory_footprint()) : 0;
			}
			
			// extend the pool
			void resize(std::size_t count) {
				if (pool_allocated() || count > 4) {
					// Allocate pool if necessary
					if (d_pool < 5)
					{
						rebind_alloc< pool_type> al = get_allocator();
						d_pool = reinterpret_cast<std::uintptr_t>(al.allocate(1));
						construct_ptr(pool(), get_allocator());
					}
					pool()->reserve(1, count);
				}
			}

			//allocate one list_chunk<T>
			auto allocate_chunk() -> list_chunk<T>*
			{
				if (can_use_aligned_alloc()) {
					list_chunk<T>* res = static_cast<list_chunk<T>*>(aligned_malloc(sizeof(list_chunk<T>), 64));
					if (!res)
						throw std::bad_alloc();
					res->user_flag = 0; 
					d_pool++;
					return res;
				}

				if (d_pool < 5)
				{
					rebind_alloc< pool_type> al = get_allocator();
					d_pool = reinterpret_cast<std::uintptr_t>(al.allocate(1));
					construct_ptr(pool(), get_allocator());
				}

				list_chunk<T>* res = pool()->allocate(1);
				res->user_flag = 1; 
				return res;
			}

			//deallocate a list_chunk<T>
			void deallocate_chunk(list_chunk<T>* ptr)
			{
				if (ptr->user_flag == 0) { 
					aligned_free(ptr);
					if (d_pool < 5)
						--d_pool;
				}
				else
					pool()->deallocate(ptr, 1);
			}

			void clear_all()
			{
				if(pool_allocated())
					pool()->clear();
			}
		};

		// Standard list_chunk allocator for OptimizeForMemory
		// Keep track of the list_chunk number for fast memory footprint
		template< class T, class Allocator = std::allocator<T>, bool Align64 = false >
		struct std_alloc : private Allocator
		{
			template< class U>
			using rebind_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<U>;

			size_t chunks;
			std_alloc(const Allocator& alloc) noexcept(std::is_nothrow_copy_constructible<Allocator>::value)
				:Allocator(alloc), chunks(0) {}
			std_alloc(size_t /*unused*/, const Allocator& alloc) noexcept : Allocator(alloc), chunks(0) {}

			auto get_allocator() noexcept -> Allocator& { return static_cast<Allocator&>(*this); }
			auto get_allocator() const noexcept -> const Allocator& { return static_cast<const Allocator&>(*this); }
			void resize(size_t /*unused*/) {}
			auto allocate_chunk() -> list_chunk<T>*
			{
				rebind_alloc< list_chunk<T> > _al = get_allocator();
				list_chunk<T>* res;
				if (Align64) {
					aligned_allocator< list_chunk<T>, rebind_alloc< list_chunk<T> >, list_chunk<T>::count> al = _al;
					res = al.allocate(1);
				}
				else
					res = _al.allocate(1);
				++chunks;
				return res;
			}
			void deallocate_chunk(list_chunk<T>* ptr)
			{
				rebind_alloc< list_chunk<T> > _al = get_allocator();
				if (Align64) {
					aligned_allocator< list_chunk<T>, rebind_alloc< list_chunk<T> >, list_chunk<T>::count> al = _al;
					al.deallocate(ptr, 1);
				}
				else 
					_al.deallocate(ptr, 1);
				--chunks;
			}

			// total memory footprint in bytes excluding sizeof(*this)
			auto memory_footprint() const noexcept -> size_t {
				return  chunks * sizeof(list_chunk<T>);
			}
			// capacity in terms of chunk
			auto get_capacity() const noexcept -> size_t { return chunks; }

			void clear_all() {}
		};

		struct NullChunkAllocator {} ;

		// Select the chunk allocator based on the layout strategy and an optional ChunkAllocator class
		template<class T, class Allocator, LayoutManagement layout, class ChunkAllocator, bool Align64>
		struct select_layout
		{
		};
		template<class T, class Allocator, bool Align64>
		struct select_layout<T, Allocator, OptimizeForSpeed, NullChunkAllocator, Align64>
		{
			using type = chunk_pool_alloc<T, Allocator>;
		};
		template<class T, class Allocator, class ChunkAllocator, bool Align64>
		struct select_layout<T, Allocator, OptimizeForSpeed, ChunkAllocator, Align64>
		{
			using type = ChunkAllocator;
		};
		template<class T, class Allocator, class ChunkAllocator, bool Align64>
		struct select_layout<T, Allocator, OptimizeForMemory, ChunkAllocator, Align64>
		{
			using type = std_alloc<T, Allocator, Align64>;
		};



		// 
		// const iterator for sequence object
		//
		template< class List>
		class sequence_const_iterator
		{
		public:
			using iterator_category = std::bidirectional_iterator_tag;
			using value_type = typename List::value_type;
			using difference_type = typename List::difference_type;
			using pointer = typename List::const_pointer;
			using reference = const value_type&;
			using list_data = typename List::Data;
			using chunk_type = detail::list_chunk<value_type>;
	#ifdef _MSC_VER
			using pos_type = int;
	#else
			using pos_type = difference_type;
	#endif

			static constexpr int count = detail::list_chunk<value_type>::count;

			chunk_type* node;
			pos_type pos;

			sequence_const_iterator() noexcept {}
			sequence_const_iterator(const chunk_type* n) noexcept
				:node(const_cast<chunk_type*>(n)), pos(n ? static_cast<pos_type>(n->start) : 0) {}
			sequence_const_iterator(const chunk_type* n, pos_type p) noexcept
				:node(const_cast<chunk_type*>(n)), pos(p) {}

			SEQ_ALWAYS_INLINE std::uintptr_t as_uint() const noexcept{
				return (reinterpret_cast<std::uintptr_t>(node)) | static_cast<std::uintptr_t>(pos);
			}
			SEQ_ALWAYS_INLINE void from_uint(std::uintptr_t p) noexcept {
				node = reinterpret_cast<chunk_type*>(p & ~(chunk_type::count - 1));
				pos = static_cast<pos_type>(p & (chunk_type::count - 1));
			}

			SEQ_ALWAYS_INLINE auto operator*() const noexcept -> reference {
				SEQ_ASSERT_DEBUG(pos >= node->start && pos < node->end, "invalid iterator position");
				return node->buffer()[pos];
			}
			SEQ_ALWAYS_INLINE auto operator->() const noexcept -> pointer {
				return std::pointer_traits<pointer>::pointer_to(**this);
			}
			void update_incr_pos() noexcept
			{
				if (pos == node->end) {
					node = static_cast<chunk_type*>(node->next);
					pos = static_cast<pos_type>(node->start);
				}
				else {
					pos = static_cast<pos_type>(bit_scan_forward_64(node->used >> pos)) + pos;//ptr - node->buffer();
				}
			}
			SEQ_ALWAYS_INLINE auto operator++() noexcept -> sequence_const_iterator& {
				++pos;
				if (SEQ_UNLIKELY(pos == count || !((node->used & (1ULL << pos))))) {
					update_incr_pos();
				}
				//SEQ_ASSERT_DEBUG((pos >= 0 && pos < node->end) || (pos == 0 && node == &data->end), "invalid iterator position");
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator++(int) noexcept -> sequence_const_iterator {
				sequence_const_iterator _Tmp = *this;
				++(*this);
				return _Tmp;
			}
			void update_decr_pos() noexcept
			{
				if (pos < node->start) {
					node = static_cast<chunk_type*>(node->prev);
					pos = (node->end - 1);
				}
				else {
					//ptr = node->buffer() + bit_scan_forward_64(node->used >> pos) + pos;
					pos = static_cast<pos_type>(bit_scan_reverse_64(node->used & ((1ULL << pos) - 1ULL)));//ptr - node->buffer();
				}
			}
			SEQ_ALWAYS_INLINE auto operator--() noexcept -> sequence_const_iterator& {
				--pos;
				if (SEQ_UNLIKELY(pos == -1 || !((node->used & (1ULL << pos)))  /*ptr == node->end*/)) {
					update_decr_pos();
				}
				//SEQ_ASSERT_DEBUG(pos >= 0 && pos < node->end, "invalid iterator position");
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator--(int) noexcept -> sequence_const_iterator {
				sequence_const_iterator _Tmp = *this;
				--(*this);
				return _Tmp;
			}
			SEQ_ALWAYS_INLINE auto operator+=(difference_type diff) noexcept -> sequence_const_iterator& {
				increment(diff);
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator-=(difference_type diff) noexcept -> sequence_const_iterator& {
				increment(-diff);
				return *this;
			}

			// Increment iterator, tries to jump other buckets as much as possible
			void increment(difference_type diff)  noexcept
			{
				if (diff > 0) {
					unsigned rem = pos == count - 1 ? 0 : popcnt64(node->used >> (static_cast<unsigned>(pos) + 1ULL));
					if (static_cast<unsigned>(diff) <= rem) {
						while (diff--)
							++(*this);
					}
					else {
						diff -= rem + 1;
						(*this).node = static_cast<chunk_type*>((*this).node->next);
						(*this).pos = (*this).node->start;
						unsigned size = (*this).node->size();
						while (diff > count && (*this).node->used == chunk_type::full) {
							(*this).node = static_cast<chunk_type*>((*this).node->next);
							diff -= count;
						}
						size = (*this).node->size();
						(*this).pos = (*this).node->start;
						while (diff > count) {
							diff -= size;
							(*this).node = static_cast<chunk_type*>((*this).node->next);
							(*this).pos = (*this).node->start;
							size = (*this).node->used == chunk_type::full ? count : (*this).node->size();
						}
						while (diff--)
							++(*this);
					}
				}
				else if (diff < 0) {
					diff = -diff;
					unsigned rem = popcnt64((*this).node->used & ((1ULL << (*this).pos) - 1ULL));
					if (static_cast<unsigned>(diff) <= rem) {
						while (diff--)
							--(*this);
					}
					else {
						diff -= rem + 1;
						(*this).node = static_cast<chunk_type*>((*this).node->prev);
						(*this).pos = (*this).node->end - 1;
						unsigned size = (*this).node->size();
						while (diff > count && (*this).node->used == chunk_type::full) {
							(*this).node = static_cast<chunk_type*>((*this).node->prev);
							diff -= count;
						}
						size = (*this).node->size();
						(*this).pos = (*this).node->end - 1;
						while (diff > count) {
							diff -= size;
							(*this).node = static_cast<chunk_type*>((*this).node->prev);
							(*this).pos = (*this).node->end - 1;
							size = (*this).node->used == chunk_type::full ? count : (*this).node->size();
						}
						while (diff--)
							--(*this);
					}
				}
			}


			// Distance between iterators
			static auto distance(const sequence_const_iterator& it1, const sequence_const_iterator& it2) -> difference_type
			{
				if (it1.node == it2.node) {
					// Same node
					if (it1.pos > it2.pos) {
						return static_cast<difference_type>(popcnt64((it1.node->used & ((1ULL << it1.pos) - 1)) >> it2.pos));
					}
					else {
						return -(static_cast<difference_type>(popcnt64((it1.node->used & ((1ULL << it2.pos) - 1)) >> it1.pos)));
					}
				}
				else {
					// Different nodes
					difference_type diff = 0, sign;
					sequence_const_iterator start, target;
					if (it1 > it2) {
						start = it2; target = it1;
						sign = 1;
					}
					else {
						start = it1; target = it2;
						sign = -1;
					}
					unsigned rem = start.pos == count - 1 ? 0 : popcnt64(start.node->used >> (start.pos + 1));
					diff += rem + 1;
					start.node = static_cast<chunk_type*>(start.node->next);
					start.pos = start.node->start;
					while (start.node != target.node) {
						diff += start.node->size();
						start.node = static_cast<chunk_type*>(start.node->next);
						start.pos = start.node->start;
					}
					while (start != target) {
						++start;
						++diff;
					}
					return diff * sign;
				}
			}

		};

		// 
		// iterator for sequence object
		//
		template<class List >
		class sequence_iterator : public sequence_const_iterator< List>
		{
		public:
			using base_type = sequence_const_iterator< List>;
			using iterator_category = std::bidirectional_iterator_tag;
			using value_type = typename List::value_type;
			using difference_type = typename List::difference_type;
			using pointer = typename List::pointer;
			using reference = value_type&;

			sequence_iterator() noexcept {}
			sequence_iterator(const typename base_type::chunk_type* n)noexcept
				:sequence_const_iterator<List>(n) {}
			sequence_iterator(const typename base_type::chunk_type* n, typename base_type::pos_type p) noexcept
				:sequence_const_iterator<List>(n, p) {}
			sequence_iterator(const sequence_const_iterator<List>& other) noexcept : base_type(other) {}
			auto operator*() const noexcept -> reference {
				return const_cast<reference>(base_type::operator*());
			}
			auto operator->() const noexcept -> pointer {
				return std::pointer_traits<pointer>::pointer_to(**this);
			}
			auto operator++() noexcept -> sequence_iterator& {
				base_type::operator++();
				return *this;
			}
			auto operator++(int) noexcept -> sequence_iterator {
				sequence_iterator _Tmp = *this;
				base_type::operator++();
				return _Tmp;
			}
			auto operator--() noexcept -> sequence_iterator& {
				base_type::operator--();
				return *this;
			}
			auto operator--(int) noexcept -> sequence_iterator {
				sequence_iterator _Tmp = *this;
				base_type::operator--();
				return _Tmp;
			}
			auto operator+=(difference_type diff)noexcept -> sequence_iterator& {
				base_type::operator+=(diff);
				return *this;
			}
			auto operator-=(difference_type diff)noexcept -> sequence_iterator& {
				base_type::operator-=(diff);
				return *this;
			}
		};

		template<class List>
		SEQ_ALWAYS_INLINE bool operator==(const sequence_const_iterator<List>& a, const sequence_const_iterator<List>& b) noexcept {
			return a.node == b.node && a.pos == b.pos;
		}
		template<class List>
		SEQ_ALWAYS_INLINE bool operator!=(const sequence_const_iterator<List>& a, const sequence_const_iterator<List>& b) noexcept {
			return a.node != b.node || a.pos != b.pos;
		}
		template<class List>
		SEQ_ALWAYS_INLINE bool operator>(const sequence_const_iterator<List>& a, const sequence_const_iterator<List>& b) noexcept {
			return (a.node->node_index > b.node->node_index) || ((a.node->node_index == b.node->node_index) && ((a.pos) > (b.pos)));
		}
		template<class List>
		SEQ_ALWAYS_INLINE bool operator<(const sequence_const_iterator<List>& a, const sequence_const_iterator<List>& b) noexcept {
			return (a.node->node_index < b.node->node_index) || ((a.node->node_index == b.node->node_index) && ((a.pos) < (b.pos)));
		}
		template<class List>
		SEQ_ALWAYS_INLINE bool operator>=(const sequence_const_iterator<List>& a, const sequence_const_iterator<List>& b) noexcept {
			return (a == b) || (a > b);
		}
		template<class List>
		SEQ_ALWAYS_INLINE bool operator<=(const sequence_const_iterator<List>& a, const sequence_const_iterator<List>& b) noexcept {
			return (a == b) || (a < b);
		}
		template<class List>
		SEQ_ALWAYS_INLINE auto operator-(const sequence_const_iterator<List>& a, const sequence_const_iterator<List>& b) noexcept -> typename sequence_const_iterator<List>::difference_type {
			return a.distance(a, b);
		}
		template < class List>
		SEQ_ALWAYS_INLINE auto operator+(const sequence_const_iterator< List>&  it, typename sequence_const_iterator< List>::difference_type  diff) noexcept -> sequence_const_iterator< List> {
			sequence_const_iterator< List> res  = it;
			res += diff;
			return res;
		}
		template < class List>
		SEQ_ALWAYS_INLINE auto operator+(const sequence_iterator< List>&  it, typename sequence_iterator< List>::difference_type  diff) noexcept -> sequence_iterator< List> {
			sequence_iterator< List> res  = it;
			res += diff;
			return res;
		}
		template < class List>
		SEQ_ALWAYS_INLINE auto operator-(const sequence_const_iterator< List>&  it, typename sequence_const_iterator< List>::difference_type  diff) noexcept -> sequence_const_iterator< List> {
			sequence_const_iterator< List> res  = it;
			res -= diff;
			return res;
		}
		template < class List>
		SEQ_ALWAYS_INLINE auto operator-(const sequence_iterator< List>&  it, typename sequence_iterator< List>::difference_type  diff) noexcept -> sequence_iterator< List> {
			sequence_iterator< List> res  = it;
			res -= diff;
			return res;
		}








		// 
		// Random access  iterator for sequence class, used to sort the sequence
		//
		template< class List>
		struct sequence_ra_iterator
		{
			struct Data {
				std::vector< detail::list_chunk<typename List::value_type>*> chunks{};
				detail::list_chunk<typename List::value_type>* end;
				size_t size{};
			};
			using iterator_category = std::random_access_iterator_tag;
			using value_type = typename List::value_type;
			using difference_type = typename List::difference_type;
			using pointer = typename List::pointer;
			using reference = value_type&;
			using chunk_type = detail::list_chunk<value_type>;
			using pos_type = difference_type;

			static constexpr int count = detail::list_chunk<value_type>::count;

			Data* data;
			chunk_type* node;
			difference_type abs_pos;
			pos_type pos;

			sequence_ra_iterator() noexcept {}
			sequence_ra_iterator(const Data* d, const chunk_type* n) noexcept
				:data(const_cast<Data*>(d)), node(const_cast<chunk_type*>(n)), abs_pos(0), pos(n->start) {}
			sequence_ra_iterator(const Data* d, const chunk_type* n, pos_type p, difference_type _abs_pos) noexcept
				:data(const_cast<Data*>(d)), node(const_cast<chunk_type*>(n)), abs_pos(_abs_pos), pos(p) {}


			SEQ_ALWAYS_INLINE auto absolutePos() const noexcept -> size_t {
				return static_cast<size_t>(abs_pos);
			}
			SEQ_ALWAYS_INLINE void setAbsolutePos(std::size_t _abs_pos) noexcept 
			{
				SEQ_ASSERT_DEBUG(_abs_pos <= (data->size), "invalid iterator position");
				if (SEQ_UNLIKELY(_abs_pos == data->size)) {
					node = data->end;
					pos = node->start;
				}
				else  {
					
					size_t front_size = static_cast<size_t>(data->chunks.front()->end - data->chunks.front()->start);
					size_t bucket = (_abs_pos + (chunk_type::count - front_size)) >> chunk_type::count_bits;
					node = data->chunks[bucket];
					pos = node->start + static_cast<int>((_abs_pos - (_abs_pos < front_size ? 0 : front_size)) & (chunk_type::count - 1));
					
				}
				this->abs_pos = static_cast<difference_type>(_abs_pos);
			}
			SEQ_ALWAYS_INLINE auto operator*()  -> reference {
				
				SEQ_ASSERT_DEBUG(pos >= node->start && pos < node->end, "invalid iterator position");
				return node->buffer()[pos];
			}
			SEQ_ALWAYS_INLINE auto operator->() noexcept -> pointer {
				return std::pointer_traits<pointer>::pointer_to(**this);
			}
			void update_incr() noexcept
			{
				if (pos == node->end) {
					node = static_cast<chunk_type*>(node->next);
					pos = node->start;
				}
				else {
					pos = static_cast<pos_type>(bit_scan_forward_64(node->used >> pos) + pos);//ptr - node->buffer();
				}
			}
			SEQ_ALWAYS_INLINE auto operator++() noexcept -> sequence_ra_iterator& {
				SEQ_ASSERT_DEBUG(abs_pos < static_cast<difference_type>(data->size), "invalid iterator position");
				++pos;
				++abs_pos;
				if (pos >= node->end) {
					update_incr();
				}
				//SEQ_ASSERT_DEBUG((pos >= 0 && pos < node->end) || (pos == 0 && node == &data->end), "invalid iterator position");
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator++(int) noexcept -> sequence_ra_iterator {
				sequence_ra_iterator _Tmp = *this;
				++(*this);
				return _Tmp;
			}
			void update_decr() noexcept
			{
				if (pos < node->start) {
					node = static_cast<detail::list_chunk<value_type>*>(node->prev);
					pos = (node->end - 1);
				}
				else {
					//ptr = node->buffer() + bit_scan_forward_64(node->used >> pos) + pos;
					pos = static_cast<pos_type>(bit_scan_reverse_64(node->used & ((1ULL << pos) - 1)));//ptr - node->buffer();
				}
			}
			SEQ_ALWAYS_INLINE auto operator--() noexcept -> sequence_ra_iterator& {
				SEQ_ASSERT_DEBUG(abs_pos > 0, "invalid iterator position");
				--pos;
				--abs_pos;
				if (/*pos == (pos_type)-1 || !((node->used & (1ULL << pos)))*/ pos < node->start ) {
					update_decr();
				}
				SEQ_ASSERT_DEBUG(pos >= 0 && pos < node->end, "invalid iterator position");
				return*this;
			}
			SEQ_ALWAYS_INLINE auto operator--(int) noexcept -> sequence_ra_iterator {
				sequence_ra_iterator _Tmp = *this;
				--(*this);
				return _Tmp;
			}
			SEQ_ALWAYS_INLINE auto operator+=(difference_type diff) noexcept -> sequence_ra_iterator& {
				setAbsolutePos(static_cast<size_t>(abs_pos + diff)); 
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator-=(difference_type diff) noexcept -> sequence_ra_iterator& {
				setAbsolutePos(static_cast<size_t>(abs_pos - diff));
				return *this;
			}

			SEQ_ALWAYS_INLINE bool operator==(const sequence_ra_iterator& other) const noexcept { return abs_pos == other.abs_pos; }
			SEQ_ALWAYS_INLINE bool operator!=(const sequence_ra_iterator& other) const noexcept { return abs_pos != other.abs_pos; }
			SEQ_ALWAYS_INLINE bool operator<(const sequence_ra_iterator& other) const noexcept { return abs_pos < other.abs_pos; }
			SEQ_ALWAYS_INLINE bool operator>(const sequence_ra_iterator& other) const noexcept { return abs_pos > other.abs_pos; }
			SEQ_ALWAYS_INLINE bool operator<=(const sequence_ra_iterator& other) const noexcept { return abs_pos <= other.abs_pos; }
			SEQ_ALWAYS_INLINE bool operator>=(const sequence_ra_iterator& other) const noexcept { return abs_pos >= other.abs_pos; }
			SEQ_ALWAYS_INLINE auto operator+(difference_type diff) const noexcept -> sequence_ra_iterator {
				sequence_ra_iterator tmp = *this;
				tmp += diff;
				return tmp;
			}
			SEQ_ALWAYS_INLINE auto operator-(difference_type diff) const noexcept -> sequence_ra_iterator {
				sequence_ra_iterator tmp = *this;
				tmp -= diff;
				return tmp;
			}
			SEQ_ALWAYS_INLINE auto operator-(const sequence_ra_iterator& other) const noexcept -> difference_type { return abs_pos - other.abs_pos; }
		};

	} // end namespace detail






	///
	/// @brief sequence is an ordered container supporting constant time insertion at both end and constant time removal anywhere.
	/// @tparam T value type
	/// @tparam Allocator allocator type
	///
	/// The sequence container behaves like a hybrid version of std::deque and std::list. It provides:
	///		- Constant time insertion at the back or the front using members push_back(), emplace_back(), push_front() and emplace_front()
	///		- Constant time removal of one or more elements with erase()
	///		- Stability of references and iterators.
	/// 
	/// Unlike std::list, the sequence container does not provide insertion anywhere in the container.
	/// Instead, sequence provides unordered insertion through its member #insert(), much like the <a href="https://plflib.org/colony.htm">plf::colony</a> class.
	/// Unordered insertion is usually prefered to back or front insertion as it can reuse free slots created by erase()
	/// and avoid potential allocations.
	/// In addition, its sort(), stable_sort() and shrink_to_fit() members do not preserve reference and iterator stability.
	/// 
	/// Its main advantages other std::list (and other containers) are:
	///		-	Insertion at both ends is faster than a std::deque or std::vector (without reserve)
	///		-	Walking through the sequence with iterators is usually faster than walking through a std::deque
	///		-	Sorting a sequence (with sort() or stable_sort()) is usually as fast as sorting a std::deque.
	///			Note that #sequence::sort() uses <a href="https://github.com/orlp/pdqsort">pdqsort</a> from Orson Peters.
	///		-	Its memory overhead is lower than a std::list: around 1 byte per element.
	/// 
	/// The sequence container is a perfect candidate for std::queue and std::stack.
	/// It is used by the seq library as the backend container for seq::ordered_set and seq::ordered_map.
	/// 
	/// 
	/// Technical description
	/// ---------------------
	/// 
	/// sequence container is implemented as a linked list of buckets. Each bucket holds (up to) 64 elements in a contiguous storage,
	/// and a 64 bits integer telling if a slot is empty or occupied. 
	/// 
	/// In order to retrieve the index of the first (or last) used slot in a bucket, or to get the number of occupied slots,
	/// the sequence container uses OS intrinsics to scan the 64 bits integer. For instance, on Windows, 
	/// \a _BitScanForward64, \a _BitScanRevers64 and \a _mm_popcnt_u64 are used.
	/// Removing an element from the sequence will set the corresponding bit to 0, inserting will set the bit to 1.
	/// 
	/// In addition, the sequence maintains another linked list of partially free buckets in order to perform fast
	/// unordered insertion using #insert() member and therefore reuse slots previously deleted by #erase().
	/// 
	/// 
	/// Iterators
	/// ---------
	/// 
	/// sequence iterator and const_iterator are bidirectional iterators. However, they provide all the members to behave
	/// like random access iterator, except the std::random_access_iterator_tag typedef.
	/// 
	/// Indeed, using \a iterator::operator+= is, for instance, much faster than \a std::advance as it
	/// can skip whole buckets to reach the required location. Likewise subtracting 2 iterators
	/// is much faster than using \a std::distance.
	/// 
	/// sequence iterator also provide comparison operators <, >, <= and >=.
	/// 
	/// 
	/// Memory managment
	/// ----------------
	/// 
	/// When using the \a OptimizeForMemory flag, the sequence will allocate each bucket independently using
	/// the provided allocator.
	/// 
	/// When using the \a OptimizeForSpeed flag, the sequence will use a memory pool (seq::object_pool) to allocate several buckets at once,
	/// still using the provided allocator. The memory pool uses a growing strategy to allocate more and more buckets
	/// based on a growth factor (SEQ_GROW_FACTOR define, defaulting to 1.6). The memory pool makes insertion tipically
	/// 50% faster than the default allocation strategy, but will consume slightly more memory.
	/// 
	/// Whenever a bucket is empty (by calls to erase(), clear(), resize(), resize_front() or shrink_to_fit()),
	/// the sequence releases the occupied memory.
	/// 
	template<class T, class Allocator = std::allocator<T>, LayoutManagement layout = OptimizeForSpeed, bool ForceAlign64 = false >
	class sequence : private Allocator
	{
		using this_type = sequence<T, Allocator, layout, ForceAlign64>;

	public:
		using value_type = T;
		using pointer = T*;
		using reference = T&;
		using const_pointer = const T*;
		using allocator_type = Allocator;
		using difference_type = typename std::allocator_traits<Allocator>::difference_type;
		using size_type = typename std::allocator_traits<Allocator>::size_type;
		using iterator = detail::sequence_iterator< this_type >;
		using const_iterator = detail::sequence_const_iterator< this_type >;
		using reverse_iterator = std::reverse_iterator<iterator>;
		using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	private:
		
		friend class detail::sequence_const_iterator<this_type>; //iterator has access to internal Data object
		friend class detail::sequence_iterator<this_type>; //iterator has access to internal Data object
		template< class U>
		using rebind_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<U>;
		using chunk_type = detail::list_chunk<T>;
		using layout_manager = typename detail::select_layout<T, Allocator, layout, detail::NullChunkAllocator, ForceAlign64>::type;
		static constexpr std::uint64_t count = detail::list_chunk<T>::count;
		static constexpr std::uint64_t count1 = detail::list_chunk<T>::count - 1;
		static constexpr std::uint64_t count_bits = detail::list_chunk<T>::count_bits;
		static constexpr std::uint64_t full = detail::list_chunk<T>::full;

		/// Internal data 
		/// We use a pointer to Data internally as it fasten the move copy and assignment, and simplifies th iterator implemention.
		/// In addition, it reduces the size of empty sequences.
		/// 
		struct Data : layout_manager
		{
			using difference_type = typename std::allocator_traits<Allocator>::difference_type;
			using chunk_type = detail::list_chunk<T>;
			detail::base_list_chunk<T> end;									//end chunk
			std::size_t size;												//full size

			Data(const Allocator& al ) noexcept(std::is_nothrow_copy_constructible<Allocator>::value)
				:layout_manager(al), size(0) {
				endNode()->prev = endNode()->next = endNode();
				endNode()->prev_free = endNode()->next_free = endNode();
				endNode()->used = full;
				endNode()->start = endNode()->end = 0;
				endNode()->node_index = 0ULL;
			}
		
			SEQ_ALWAYS_INLINE auto endNode() noexcept -> chunk_type* { return static_cast<chunk_type*>(&end); }
			SEQ_ALWAYS_INLINE auto endNode() const noexcept -> const chunk_type* { return static_cast<const chunk_type*>(&end); }


			void shrink_to_fit(std::vector<chunk_type*>* vec_chunk = nullptr)
			{
				// Compact sequence by shifting left all values and remove holes produced by calls to erase()
				// If vec_chunk is not null, it is filled with pointers to all chunks (used for sorting)

				if (vec_chunk) {
					vec_chunk->clear();
					//throw now and not later
					vec_chunk->reserve(size / chunk_type::count);
				}

				// Does nothing if the sequence is alreay compact
				if (size == 0 ) {
					//fill vec_chunk
					if (vec_chunk) {
						chunk_type* chunk = static_cast<chunk_type*>(endNode()->next);
						while (chunk != endNode()) {
							vec_chunk->push_back(chunk);
							chunk = static_cast<chunk_type*>(chunk->next);
						}
					}
					return;
				}

				// Make sure dirty node is valid
				chunk_type*	dirty = static_cast<chunk_type*>(endNode()->next);

				std::uint64_t chunks = 0; // chunk index

				// Push dirty to the right while nodes are full (already compact)
				while (dirty != endNode() && dirty->used == full) {
					dirty->node_index = static_cast<std::int64_t>(chunks++);
					if (vec_chunk) vec_chunk->push_back(dirty);
					dirty->next_free = dirty->prev_free = endNode();
					dirty = static_cast<chunk_type*>(dirty->next);
				}

				// Now, start packing left
				if (dirty != endNode()) {

					if (vec_chunk) vec_chunk->push_back(dirty);

					// iterator to the dirty node start
					iterator it(dirty);
					// iterator to end of sequence
					iterator it_end(endNode());
					// current node
					chunk_type* node = dirty;
					// current index in current node
					std::uint64_t index = 0;
					
					// set current node index
					node->node_index = static_cast<std::int64_t>(chunks++);
					//remove from free list
					node->next_free = node->prev_free = endNode();

					size_t saved_size = this->size;

					try {
						// Loop until the end
						while (it != it_end) {

							std::uint64_t mask = 1ULL << index;

							// This might throw
							if (SEQ_LIKELY(node->used & mask)) {
								// avoid moving value on itself, as it is buggy with mingw (gcc 10.1.0) which just clear the string
								if(node->buffer() + index != it.node->buffer() + it.pos)
									node->buffer()[index] = std::move(*it);
							}
							else {
								construct_ptr(node->buffer() + index, std::move(*it));
								++this->size; // Increment size just in case of exception thrown
							}

							// Update used mask
							node->used |= mask;
							// Increment current node index
							++index;

							if (SEQ_UNLIKELY(index == count)) {
								// End of node, go to next one
								// First, update node bounds
								node->start = 0;
								node->end = count;
								SEQ_ASSERT_DEBUG(node->size() == count,"");
								// Remove from free nodes
								node->next_free = node->prev_free = endNode();
								node = static_cast<chunk_type*>(node->next);
								if (vec_chunk && node != endNode()) vec_chunk->push_back(node);
								index = 0;
								node->node_index = static_cast<std::int64_t>(chunks++);
							}
							++it;
						}
					}
					catch (...) {
						// Update current node bounds in case of exception
						node->start = static_cast<int>(bit_scan_forward_64(node->used));
						node->end = static_cast<int>(bit_scan_reverse_64(node->used)) + 1;
						throw;
					}

					this->size = saved_size;

					// Destroy last elements in last node
					while (index != count) {
						std::uint64_t mask = 1ULL << index;
						if (node->used & mask) {
							destroy_ptr(node->buffer() + index);
							node->used &= ~mask;
						}
						++index;
					}
					// Update last node bounds
					node->start = 0;
					node->end = static_cast<int>(node->size());
					chunk_type* last = node;

					// Deallocate next nodes (never throws)
					chunk_type* _del = static_cast<chunk_type*>(node->next);
					if (node->start == node->end) {
						_del = node;
						last = static_cast<chunk_type*>(node->prev);
					}

					last->next = endNode();
					endNode()->prev = last;
					while (_del != endNode()) {
						// Only call destructor if necessary
						if (!std::is_trivially_destructible<T>::value && _del->used != 0) {
							std::uint64_t id = 0;
							while (id != count) {
								std::uint64_t mask = 1ULL << id;
								if (_del->used & mask) {
									destroy_ptr(_del->buffer() + id);
									_del->used &= ~mask;
								}
								++id;
							}
						}
						// Next node
						chunk_type* next = static_cast<chunk_type*>(_del->next);
						this->deallocate_chunk(_del);
						_del = next;
					}
				}

				// Get last chunk. If not full, set it as the first free node.
				chunk_type* last = static_cast<chunk_type*>(endNode()->prev);
				if (last->used == full)
					endNode()->prev_free = endNode()->next_free = endNode();
				else
					endNode()->prev_free = endNode()->next_free = last;

			}

			// Returns a const_iterator at given position
			auto iterator_at(size_t pos) const noexcept -> const_iterator
			{
				if (pos < this->size / 2)
					return const_iterator(static_cast<chunk_type*>(end.next), end.next->start) + static_cast<difference_type>(pos);
				else
					return const_iterator(static_cast<chunk_type*>(&end), 0) - static_cast<difference_type>(this->size - pos);
			}
			// Returns an iterator at given position
			auto iterator_at(size_t pos)  noexcept -> iterator
			{
				if (pos < this->size / 2)
					return iterator(static_cast<chunk_type*>(end.next), end.next->start) + static_cast<difference_type>(pos);
				else
					return iterator(static_cast<chunk_type*>(&end), 0) - static_cast<difference_type>(this->size - pos);
			}

		};

		// Allocate and build a chunk with uninitialized storage
		auto make_chunk(chunk_type* prev, chunk_type* next, std::int64_t index = chunk_type::no_index) -> chunk_type*
		{
			// Allocate, might throw
			chunk_type* ptr = static_cast<chunk_type*>(d_data->allocate_chunk());
			// Set the previous and next chunks
			ptr->prev = prev;
			ptr->next = next;
			prev->next = next->prev = ptr;

			ptr->start = ptr->end = 0;
			ptr->used = 0;

			// Insert this chunk in the list of partially free chunks
			ptr->prev_free = &d_data->end;
			ptr->next_free = d_data->end.next_free;
			d_data->end.next_free = d_data->end.next_free->prev_free = ptr;

			// Set its index
			ptr->node_index = index;
			if (index == chunk_type::no_index) {
				// Build index for this node
				if (prev == &d_data->end) {
					ptr->node_index = (next == &d_data->end) ? 0 : next->node_index - 1LL;
				}
				else if (next == &d_data->end) {
					ptr->node_index = (prev == &d_data->end) ? 0 : prev->node_index + 1LL;
				}
			}
			return ptr;
		}

		void remove_free_node(chunk_type* node) noexcept
		{
			// Remove from list of free chunks
			node->prev_free->next_free = node->next_free;
			node->next_free->prev_free = node->prev_free;
			node->next_free = node->prev_free = &d_data->end;
		}
		void add_free_node(chunk_type* node) noexcept
		{
			// Add to list of free node
			node->next_free = d_data->end.next_free;
			node->prev_free = &d_data->end;
			node->next_free->prev_free = node->prev_free->next_free = node;
		}
		void remove_node(chunk_type* node) noexcept
		{
			// Remove from list of free chunks
			node->prev->next = node->next;
			node->next->prev = node->prev;
			node->next = node->prev = &d_data->end;
		}
		void destroy_node_elements(chunk_type* node) noexcept
		{
			// Destroy all valid (constructed) elements of a node
			if (!std::is_trivially_destructible<T>::value && node->used) {
				for (int i = node->start; i < node->end; ++i)
					if(node->used & (1ULL << static_cast<std::uint64_t>(i)))
						destroy_ptr(node->buffer() + i);
			}
			node->start = node->end = 0;
			node->used = 0;
		}


		template <class... Args>
		auto emplace_anywhere(Args&&... args)->iterator
		{
			chunk_type* node = static_cast<chunk_type*>(d_data->end.next_free);
			std::uint64_t index = static_cast<std::uint64_t>(node->start != 0 ? node->start - 1 : (node->end != count ? node->end : static_cast<int>(node->firstFree())));
			T* res = node->buffer() + index;
			// Construct first as it might throw
			construct_ptr(res, std::forward<Args>(args)...);

			// Remove from list of free chunks if necessary
			node->used |= (1ULL << index);
			if (node->used == full)
				remove_free_node(node);

			// Update boundaries
			if (static_cast<int>(index) == node->end) node->end++;
			else if (static_cast<int>(index) < node->start) node->start = static_cast<int>(index);

			++d_data->size;
			return iterator(node, static_cast<int>(res - node->buffer()));
		}


		// Returns pointer to back value
		SEQ_ALWAYS_INLINE auto back_ptr() const noexcept -> const T* { SEQ_ASSERT_DEBUG(d_data->size > 0,"empty container"); return &((static_cast<chunk_type*>(d_data->end.prev))->back()); }
		// Returns pointer to front value
		SEQ_ALWAYS_INLINE auto front_ptr() const noexcept -> const T* { SEQ_ASSERT_DEBUG(d_data->size > 0, "empty container"); return &((static_cast<chunk_type*>(d_data->end.next))->front()); }
		// Returns pointer to back value
		SEQ_ALWAYS_INLINE auto back_ptr() noexcept -> T* { SEQ_ASSERT_DEBUG(d_data->size > 0, "empty container"); return &((static_cast<chunk_type*>(d_data->end.prev))->back()); }
		// Returns pointer to front value
		SEQ_ALWAYS_INLINE auto front_ptr() noexcept -> T* { SEQ_ASSERT_DEBUG(d_data->size > 0, "empty container"); return &((static_cast<chunk_type*>(d_data->end.next))->front()); }

		// Assign  range for non random access iterator
		template<class Iter, class Cat>
		void assign_cat(Iter first, Iter last, Cat) {
			iterator it = begin();
			iterator en = end();
			size_type new_count = 0;
			while (it != en && first != last) {
				*it = *first;
				++it;
				++first;
				++new_count;
			}
			while (first != last) {
				push_back(*first);
				++first;
				++new_count;
			}
			resize(new_count);
		}
		// Assign range for random access iterator
		template<class Iter>
		void assign_cat(Iter first, Iter last, std::random_access_iterator_tag) {
			size_type new_count = static_cast<size_t>(last - first);
			resize(new_count);
			std::copy(first, last, begin());
		}

		// Insert back creating a new chunk
		template <class... Args>
		SEQ_NOINLINE(T&) emplace_back_new_chunk(chunk_type* last, Args&&... args)
		{
			// Build chunk, might throw (which is fine)
			last = make_chunk(last, static_cast<chunk_type*>(&d_data->end)); //specify chunk index if not dirty
			try {
				//construct object, might throw
				construct_ptr(&last->front(), std::forward<Args>(args)...);
			}
			catch (...) {
				//delete chunk
				remove_node(last);
				remove_free_node(last);
				d_data->deallocate_chunk(last);
				throw;
			}

			// Finish
			last->used = 1ULL;
			last->end = last->start + 1;
			++d_data->size;
			return last->front();
		}

		template <class... Args>
		SEQ_NOINLINE(T&) emplace_front_new_chunk(chunk_type* first, Args&&... args)
		{
			// Build chunk, migh throw (which is fine)
			first = make_chunk(static_cast<chunk_type*>( &d_data->end), first);
			first->end = count;

			try {
				//construct object, might throw
				construct_ptr(&first->back(), std::forward<Args>(args)...);
			}
			catch (...) {
				//delete chunk
				remove_node(first);
				remove_free_node(first);
				d_data->deallocate_chunk(first);
				throw;
			}

			first->used = (1ULL << (count - 1));
			first->start = first->end - 1;
			++d_data->size;
			return first->front();
		}

		template<class Alloc, LayoutManagement L, bool Align>
		void import(const sequence<T,Alloc,L,Align> & other)
		{
			if (!d_data)
				d_data = make_data(get_allocator());

			// Assign another sequence

			// Check self assignment
			if (this == &other)
				return;

			size_t osize = other.size();

			if (osize == size())
			{
				// Same size, plain copy
				std::copy(other.begin(), other.end(), begin());
				return;
			}

			if (osize == 0) {
				// Check for empty source
				clear();
				return;
			}

			if (osize > size()) {

				// Assign a bigger size, try to reserve first
				reserve(osize);

				// Copy first part
				auto this_it = begin();
				auto other_it = other.begin();
				while (this_it != end()) {
					*this_it = *other_it;
					++this_it;
					++other_it;
				}
			
				size_type diff = osize - size();
				chunk_type* last = d_data->endNode();

				// First, fill last chunk
				if (size()) {

					// Fill back last chunk
					last = static_cast<chunk_type*>(d_data->endNode()->prev);
					if (last->end != chunk_type::count) {
						while (last->end != chunk_type::count && diff) {
							// Might throw, fine
							construct_ptr(last->buffer() + last->end, *other_it);
							last->used |= 1ULL << last->end;
							++last->end;
							++d_data->size;
							++other_it;
							--diff;
						}
						if (last->used == full)
							remove_free_node(last);
					}

					if (diff == 0) return; // Finished!
				}

				// Add chunks
				size_type chunks = diff / chunk_type::count;
				size_type rem = diff % chunk_type::count;

				while (chunks--) {
					last = make_chunk(last, static_cast<chunk_type*>(&d_data->end));
					remove_free_node(last);
					last->used = full;

					try {
						// Fill last chunk, might throw
						while (last->end != chunk_type::count) {
							construct_ptr(last->buffer() + last->end, *other_it);
							++last->end;
							++other_it;
						}
					}
					catch (...) {
						// In case of exception, remove full chunk
						remove_node(last);
						destroy_node_elements(last);
						d_data->deallocate_chunk(last);
						throw;
					}
					d_data->size += chunk_type::count;
				}
				// Add remaining
				if (rem) {
					// Might throw, ok
					last = make_chunk(last, static_cast<chunk_type*>(&d_data->end));
					last->used = (1ULL << rem) - 1ULL;

					try {
						// Fill last chunk, might throw
						while (last->end != static_cast<int>(rem)) {
							construct_ptr(last->buffer() + last->end,*other_it);
							++last->end;
							++other_it;
						}
					}
					catch (...) {
						// In case of exception, remove full chunk
						remove_node(last);
						destroy_node_elements(last);
						remove_free_node(last);
						d_data->deallocate_chunk(last);
						throw;
					}
					d_data->size += rem;
				}
			}
			else {
				// Assign a smaller sequence
			
				// Copy first part
				std::copy(other.begin(), other.end(), begin());

				chunk_type* last = static_cast<chunk_type*>(d_data->end.prev);
				difference_type diff = static_cast<difference_type>( size() - osize);

				// empty last chunk
				while (last == d_data->end.prev && diff--)
					pop_back();

				while (diff > static_cast<difference_type>(chunk_type::count)) {
					// Destroy full chunks
					last = static_cast<chunk_type*>(d_data->end.prev);
					unsigned size = last->size();
					diff -= size;
					d_data->size -= size;
					if (last->used != full)
						remove_free_node(last);
					destroy_node_elements(last);
					remove_node(last);
					d_data->deallocate_chunk(last);
				}

				// Finish
				while (diff--)
					pop_back();
			}
		}

		void pop_front_remove_chunk(chunk_type* node) noexcept
		{
			// Remove chunk due to pop_front() call
			
			//remove from list
			remove_node(node);
			//remove from free list
			remove_free_node(node);
			d_data->deallocate_chunk(node);
		}

		void pop_back_remove_chunk(chunk_type* node) noexcept
		{
			// Remove chunk due to pop_front() call
			
			//remove from list
			remove_node(node);
			//remove from free list
			remove_free_node(node);
			d_data->deallocate_chunk(node);
		}

		void erase_remove_chunk(chunk_type* node) noexcept
		{			
			//remove from list
			remove_node(node);
			//remove from free list
			remove_free_node(node);
			d_data->deallocate_chunk(node);
		}



		// Sequence object internal data
		Data* d_data;

		auto make_data(const Allocator& al ) -> Data*
		{
			rebind_alloc<Data> a = al;
			Data * d = a.allocate(1);
			construct_ptr(d, al);
			return d;
		}
		void destroy_data(Data* d)
		{
			if (d) {
				rebind_alloc<Data> a = get_allocator();
				destroy_ptr(d);
				a.deallocate(d, 1);
			}
		}

	public:

		/// @brief Default constructor, initialize internal data
		sequence() noexcept(std::is_nothrow_default_constructible<Allocator>::value)
			: Allocator(), d_data(nullptr)
		{
		}
		/// @brief Constructor from an allocator object
		/// @param al allocator object
		explicit sequence(const Allocator& al)
			: Allocator(al), d_data(make_data(al))
		{
		}
		/// @brief Construct with an initial size and a fill value
		/// @param count initial size
		/// @param value fill value
		/// @param al allocator object
		sequence(size_type count, const T& value, const Allocator& al = Allocator())
			:Allocator(al), d_data(make_data(al))
		{
			resize(count, value);
		}
		/// @brief Construct with an initial size. Objects will be value initialize.
		/// @param count initial size
		/// @param al allocator object
		explicit sequence(size_type count, const Allocator& al = Allocator())
			:Allocator(al), d_data(make_data(al))
		{
			resize(count);
		}
		/// @brief Copy constructor
		/// @param other input sequence to copy
		sequence(const sequence& other)
			:Allocator(copy_allocator(other.get_allocator())), d_data(nullptr)
		{
			if(other.size())
				import(other);
		}
		/// @brief Copy constructor
		/// @param other input sequence to copy
		/// @param al allocator object
		sequence(const sequence& other, const Allocator& al)
			:Allocator(al), d_data(nullptr)
		{
			if (other.size())
				import(other);
		}
		/// @brief Move constructor
		/// @param other 
		sequence(sequence&& other) noexcept(std::is_nothrow_move_constructible<Allocator>::value)
			:Allocator(std::move(other.get_allocator())), d_data(other.d_data)
		{
			other.d_data = nullptr;
		}
		/// @brief  Allocator-extended move constructor. Using alloc as the allocator for the new container, moving the contents from other; if alloc != other.get_allocator(), this results in an element-wise move.
		/// @param other another container to be used as source to initialize the elements of the container with
		/// @param alloc allocator object
		sequence(sequence&& other, const Allocator& alloc) 
			:Allocator(alloc), d_data(make_data(alloc))
		{
			if(alloc == other.get_allocator())
				std::swap(other.d_data, d_data);
			else {
				resize(other.size());
				std::move(other.begin(), other.end(), begin());
			}
		}
		/// @brief Constructs the sequence with the contents of the initializer list \a lst
		/// @param lst initializer list
		/// @param al allocator object
		sequence(const std::initializer_list<T>& lst, const Allocator& al = Allocator())
			:Allocator(al), d_data(make_data(al))
		{
			assign(lst.begin(), lst.end());
		}
		/// @brief Constructs the sequence with the contents of the range [first, last).
		/// @tparam Iter iterator type
		/// @param first begin iterator
		/// @param last end iterator
		/// @param al allocator object
		template< class Iter>
		sequence(Iter first, Iter last, const Allocator& al = Allocator())
			: Allocator(al), d_data(make_data(al))
		{
			assign(first, last);
		}
		/// @brief Destructor
		~sequence()
		{
			clear();
		}

		/// @brief Copy operator, basic exception guarantee
		/// @param other input sequence object
		/// @return reference to this
		auto operator=(const sequence& other) -> sequence&
		{
			if (this != std::addressof(other)) {

				if SEQ_CONSTEXPR(assign_alloc<Allocator>::value)
				{
					if (get_allocator() != other.get_allocator()) {
						destroy_data(d_data);
						d_data = nullptr;
					}
				}
				assign_allocator(get_allocator(), other.get_allocator());

				if (other.size()) {
					import(other);
				}
				else
					clear();
			}
			return *this;
		}

		/// @brief Move assignment operator
		/// @param other input sequence object
		/// @return reference to this
		auto operator=( sequence&& other) noexcept(noexcept(std::declval<sequence&>().swap(std::declval<sequence&>()))) -> sequence&
		{
			this->swap(other);
			return *this;
		}

		/// @brief Exchanges the contents of the container with those of other. Does not invoke any move, copy, or swap operations on individual elements.
		/// @param other other sequence to swap with
		/// All iterators and references remain valid.
		/// An iterator holding the past-the-end value in this container will refer to the other container after the operation.
		void swap(sequence& other) noexcept(noexcept(swap_allocator(std::declval < Allocator&>(), std::declval < Allocator&>())))
		{
			if (this != std::addressof(other)) {
				swap_allocator(get_allocator(), other.get_allocator());
				std::swap(d_data, other.d_data);
			}
		}

		/// @brief Returns the sequence internal data. Internal use only.
		auto data() noexcept -> Data*  { return d_data; }
		auto data() const noexcept -> const Data* { return d_data; }

		/// @brief Returns the full memory footprint of this sequence in bytes, excluding sizeof(*this).
		auto memory_footprint() const noexcept -> std::size_t {
			return d_data ? sizeof(*d_data) + d_data->memory_footprint() : 0;
		}

		/// @brief Returns the allocator associated with the container.
		auto get_allocator() noexcept -> Allocator&{
			return static_cast<Allocator&>(*this);
		}
		/// @brief Returns the allocator associated with the container.
		auto get_allocator() const noexcept -> const Allocator& {
			return static_cast<const Allocator&>(*this);
		}

		/// @brief Returns the sequence maximum size.
		static auto max_size() noexcept -> size_type { return (sizeof(size_t) > 4) ? static_cast<size_t>(std::numeric_limits<std::int64_t>::max()) : std::numeric_limits<std::size_t>::max(); }

		/// @brief Returns thenumber of elements in this sequence.
		auto size() const noexcept -> size_type { return d_data ? d_data->size : 0; }

		auto empty() const noexcept -> bool {return !d_data || d_data->size== 0;}

		/// @brief Returns the number of elements that the container has currently allocated space for.
		auto capacity() const noexcept -> size_t { return d_data ? d_data->get_capacity() * 64ULL : 0; }

		/// @brief Returns the back sequence value.
		auto back() const noexcept -> const T& { SEQ_ASSERT_DEBUG(d_data->size > 0, "empty container"); return (static_cast<chunk_type*>(d_data->end.prev))->back(); }

		/// @brief Returns the back sequence value.
		auto back() noexcept -> T& { SEQ_ASSERT_DEBUG(d_data->size > 0, "empty container"); return (static_cast<chunk_type*>(d_data->end.prev))->back(); }

		/// @brief Returns the front sequence value.
		auto front() const noexcept -> const T& { SEQ_ASSERT_DEBUG(d_data->size > 0, "empty container"); return (static_cast<chunk_type*>(d_data->end.next))->front(); }

		/// @brief Returns the front sequence value.
		auto front() noexcept -> T& { SEQ_ASSERT_DEBUG(d_data->size > 0, "empty container"); return (static_cast<chunk_type*>(d_data->end.next))->front(); }

		/// @brief Clears the contents.
		/// Erases all elements from the container. After this call, size() returns zero.
		/// Invalidates any references, pointers, or iterators referring to contained elements. 
		/// Any past-the-end iterator remains valid.
		void clear() noexcept
		{
			if (!d_data)
				return;

			if (std::is_trivially_destructible<T>::value && layout == OptimizeForSpeed)
			{
				int no_pool_count = 0;
				chunk_type* node = static_cast<chunk_type*>(d_data->endNode()->next);
				while (node != d_data->endNode()) {
					// Deallocate chunk
					chunk_type* next = static_cast<chunk_type*>(node->next);
					if (node->user_flag == 0)
						d_data->deallocate_chunk(node);
					node = next;
					if (++no_pool_count == 4)
						break;
				}
				// VERY fast path
				d_data->clear_all();
			}
			else
			{
				chunk_type* node = static_cast<chunk_type*>(d_data->endNode()->next);
				while (node != d_data->endNode()) {
					// Destroy node content only if needed
					if (!std::is_trivially_destructible<T>::value && node->used) 
						destroy_node_elements(node);
					// Deallocate chunk
					chunk_type* next = static_cast<chunk_type*>(node->next);
					d_data->deallocate_chunk(node);
					node = next;
				}
			}
			destroy_data(d_data);
			d_data = nullptr;
		}

		/// @brief Constructs an element in-place at the end
		/// @tparam ...Args 
		/// @param ...args 
		/// @return reference to the newly constructed object
		/// No iterators or references are invalidated.
		/// Strong exception guarantee.
		template <class... Args>
		auto emplace_back(Args&&... args) -> T&
		{
			if (SEQ_UNLIKELY(!d_data)) d_data = make_data(get_allocator());
			chunk_type* last = static_cast<chunk_type*>(d_data->end.prev);
			if (SEQ_UNLIKELY(last->used & (1ULL << (count - 1ULL))))
				return emplace_back_new_chunk(last,std::forward<Args>(args)...);
		
			// Might throw which is fine
			construct_ptr(last->buffer() + last->end, std::forward<Args>(args)...);
			last->used |= (1ULL << (last->end));
			if (SEQ_UNLIKELY(last->used == full))
				remove_free_node(last);
			++d_data->size;
			return *(last->buffer() + last->end++);		
		}

		/// @brief Constructs an element in-place at the end and returns an iterator pointing to this element.
		/// @tparam ...Args 
		/// @param ...args 
		/// @return iterator to the inserted element
		/// No iterators or references are invalidated.
		/// Strong exception guarantee.
		template <class... Args>
		SEQ_ALWAYS_INLINE auto emplace_back_iter(Args&&... args) -> iterator {
			T* back = &emplace_back(std::forward<Args>(args)...);
			return iterator(static_cast<chunk_type*>(d_data->end.prev), static_cast<int>( back - (static_cast<chunk_type*>(d_data->end.prev))->buffer()));
		}

		/// @brief Appends the given element value to the end of the sequence.
		/// @param value 
		/// No iterators or references are invalidated.
		/// Strong exception guarantee.
		void push_back(const T& value) { emplace_back(value); }

		/// @brief Appends the given element value to the end of the sequence. value is moved into the new element.
		/// @param value 
		void push_back(T&& value) { emplace_back(std::move(value)); }

		/// @brief Constructs an element in-place at the beginning
		/// @tparam ...Args 
		/// @param ...args 
		/// @return reference to the newly constructed object
		/// No iterators or references are invalidated.
		/// Strong exception guarantee.
		template <class... Args>
		auto emplace_front(Args&&... args) -> T&
		{
			if (SEQ_UNLIKELY(!d_data)) d_data = make_data(get_allocator());
			chunk_type* first = static_cast<chunk_type*>(d_data->end.next);
			if (SEQ_UNLIKELY(first->used & 1)) 
				return emplace_front_new_chunk(first, std::forward<Args>(args)...);
			// Construct, might throw (which is ok)
			construct_ptr(first->buffer() + first->start - 1, std::forward<Args>(args)...);
			first->used |= (1ULL << (--first->start));
			if (first->used == full)
				remove_free_node(first);
			++d_data->size;
			return first->front();
		
		}

		/// @brief Constructs an element in-place at the beginning and returns an iterator pointing to this element.
		/// @tparam ...Args 
		/// @param ...args 
		/// @return iterator to the inserted element
		/// No iterators or references are invalidated.
		/// Strong exception guarantee.
		template <class... Args>
		auto emplace_front_iter(Args&&... args) -> iterator {
			T* front = &emplace_front(std::forward<Args>(args)...);
			return iterator(static_cast<chunk_type*>(d_data->end.next), static_cast<int>(front - (static_cast<chunk_type*>(d_data->end.next))->buffer()));
		}

		/// @brief Prepends the given element value to the beginning of the sequence.
		/// @param value 
		/// No iterators or references are invalidated.
		/// Strong exception guarantee.
		void push_front(const T& value) { emplace_front(value); }

		/// @brief Prepends the given element value to the beginning of the sequence. value is moved into the new element.
		/// @param value 
		/// No iterators or references are invalidated.
		/// Strong exception guarantee.
		void push_front(T&& value) { emplace_front(std::move(value)); }


		

		/// @brief Constructs an element in-place anywhere into the sequence.
		/// @tparam ...Args 
		/// @param ...args 
		/// @return iterator to newly inserted element
		/// The element could be inserted anywhere inside the sequence, including at the back or front.
		/// This function tries to recyclate free slots after calls to erase().
		/// You should favor this function if you don't care about the sequence ordering.
		/// No iterators or references are invalidated.
		/// Strong exception guarantee.
		template <class... Args>
		auto emplace(Args&&... args) -> iterator
		{
			if (SEQ_UNLIKELY(!d_data)) d_data = make_data(get_allocator());

			if (d_data->end.next_free == &d_data->end)
				// If no free slot, default to emplace_back
				return emplace_back_iter(std::forward<Args>(args)...);
			
			return emplace_anywhere(std::forward<Args>(args)...);
		}
	
		/// @brief Insert the given element into the sequence.
		/// @tparam ...Args 
		/// @param ...args 
		/// @return iterator to newly inserted element
		/// The element could be inserted anywhere inside the sequence, including at the back or front.
		/// This function tries to recyclate free slots after calls to erase().
		/// You should favor this function if you don't care about the sequence ordering.
		/// No iterators or references are invalidated.
		/// Strong exception guarantee.
		auto insert(const T& value) -> iterator {
			return emplace(value);
		}

		/// @brief Insert the given element into the sequence. value is moved into the new element.
		/// @tparam ...Args 
		/// @param ...args 
		/// @return iterator to newly inserted element
		/// The element could be inserted anywhere inside the sequence, including at the back or front.
		/// This function tries to recyclate free slots after calls to erase().
		/// You should favor this function if you don't care about the sequence ordering.
		/// No iterators or references are invalidated.
		/// Strong exception guarantee.
		auto insert(T&& value) -> iterator {
			return emplace(std::move(value));
		}


		/// @brief Resizes the container to contain count elements.
		/// @param count new size of the container
		/// If the current size is greater than count, the container is reduced to its first count elements.
		/// If the current size is less than count, additional default-inserted elements are appended.
		/// Basic exception guarantee.
		void resize(size_type new_size)
		{
			if (new_size == size())
				// No-op
				return; 

			if (new_size == 0) {
				clear();
				return;
			}

			if (SEQ_UNLIKELY(!d_data)) 
				d_data = make_data(get_allocator());

			if (new_size > size()) {
				reserve(new_size);
				size_type diff = new_size - size();
				chunk_type* last = d_data->endNode();

				// First, fill last chunk
				if (size()) {

					// Fill back last chunk
					last = static_cast<chunk_type*>(d_data->endNode()->prev);
					if (last->end != chunk_type::count) {
						while (last->end != chunk_type::count && diff) {
							// Might throw, fine
							construct_ptr(last->buffer() + last->end);
							last->used |= 1ULL << last->end;
							++last->end;
							++d_data->size;
							--diff;
						}
						if (last->used == full)
							remove_free_node(last);
					}

					if (diff == 0) return; // Finished!
				}

				// Add chunks
				size_type chunks = diff / chunk_type::count;
				size_type rem = diff % chunk_type::count;

				while (chunks--) {
					last = make_chunk(last, static_cast<chunk_type*>(&d_data->end));
					remove_free_node(last);
					last->used = full;

					try {
						// Fill last chunk, might throw
						while (last->end != chunk_type::count) {
							construct_ptr(last->buffer() + last->end);
							++last->end;
						}
					}
					catch (...) {
						// In case of exception, remove full chunk
						destroy_node_elements(last);
						remove_node(last);
						d_data->deallocate_chunk(last);
						throw;
					}
					d_data->size += chunk_type::count;
				}
				// Add remaining
				if (rem) {
					// Might throw, ok
					last = make_chunk(last, static_cast<chunk_type*>(&d_data->end));
					last->used = (1ULL << rem) - 1ULL;

					try {
						// Fill last chunk, might throw
						while (last->end != static_cast<int>(rem)) {
							construct_ptr(last->buffer() + last->end);
							++last->end;
						}
					}
					catch (...) {
						// In case of exception, remove full chunk
						destroy_node_elements(last);
						remove_node(last);
						remove_free_node(last);
						d_data->deallocate_chunk(last);
						throw;
					}
					d_data->size += rem;
				}
			}
			else {
				chunk_type* last = static_cast<chunk_type*>(d_data->end.prev);
				difference_type diff = static_cast<difference_type>(size() - new_size);

				// empty last chunk
				while (last == d_data->end.prev && diff--)
					pop_back();

				while (diff > static_cast<difference_type>(chunk_type::count)) {
					// destroy full chunks
					last = static_cast<chunk_type*>(d_data->end.prev);
					unsigned size = last->size();
					diff -= size;
					d_data->size -= size;
					if (last->used != full)
						remove_free_node(last);
					destroy_node_elements(last);
					remove_node(last);
					d_data->deallocate_chunk(last);
				}

				// finish
				while (diff--)
					pop_back();
			}
		}

		/// @brief Resizes the container to contain count elements.
		/// @param count new size of the container
		/// @param value the value to initialize the new elements with
		/// If the current size is greater than count, the container is reduced to its first count elements.
		/// If the current size is less than count, additional copies of value are appended.
		/// Basic exception guarantee.
		void resize(size_type new_size, const T & value)
		{
			if (new_size == size())
				// No-op
				return;

			if (new_size == 0) {
				clear();
				return;
			}

			if (SEQ_UNLIKELY(!d_data)) 
				d_data = make_data(get_allocator());

			if (new_size > size()) {
				reserve(new_size);
				size_type diff = new_size - size();
				chunk_type* last = d_data->endNode();

				// First, fill last chunk
				if (size()) {

					// Fill back last chunk
					last = static_cast<chunk_type*>(d_data->endNode()->prev);
					if (last->end != chunk_type::count) {
						while (last->end != chunk_type::count && diff) {
							// Might throw, fine
							construct_ptr(last->buffer() + last->end, value);
							last->used |= 1ULL << last->end;
							++last->end;
							++d_data->size;
							--diff;
						}
						if (last->used == full)
							remove_free_node(last);
					}

					if (diff == 0) return; // Finished!
				}

				// Add chunks
				size_type chunks = diff / chunk_type::count;
				size_type rem = diff % chunk_type::count;

				while (chunks--) {
					last = make_chunk(last, static_cast<chunk_type*>(&d_data->end));
					remove_free_node(last);
					last->used = full;

					try {
						// Fill last chunk, might throw
						while (last->end != chunk_type::count) {
							construct_ptr(last->buffer() + last->end, value);
							++last->end;
						}
					}
					catch (...) {
						// In case of exception, remove full chunk
						destroy_node_elements(last);
						remove_node(last);
						d_data->deallocate_chunk(last);
						throw;
					}
					d_data->size += chunk_type::count;
				}
				// Add remaining
				if (rem) {
					// Might throw, ok
					last = make_chunk(last, static_cast<chunk_type*>(&d_data->end));
					last->used = (1ULL << rem) - 1ULL;

					try {
						// Fill last chunk, might throw
						while (last->end != static_cast<int>(rem)) {
							construct_ptr(last->buffer() + last->end, value);
							++last->end;
						}
					}
					catch (...) {
						// In case of exception, remove full chunk
						destroy_node_elements(last);
						remove_free_node(last);
						remove_node(last);
						d_data->deallocate_chunk(last);
						throw;
					}
					d_data->size += rem;
				}
			}
			else {
				chunk_type* last = static_cast<chunk_type*>(d_data->end.prev);
				difference_type diff = static_cast<difference_type>(size() - new_size);

				// empty last chunk
				while (last == d_data->end.prev && diff--)
					pop_back();

				while (diff > static_cast<difference_type>(chunk_type::count)) {
					// destroy full chunks
					last = static_cast<chunk_type*>(d_data->end.prev);
					unsigned size = last->size();
					diff -= size;
					d_data->size -= size;
					if (last->used != full)
						remove_free_node(last);
					destroy_node_elements(last);
					remove_node(last);
					d_data->deallocate_chunk(last);
				}

				// finish
				while (diff--)
					pop_back();
			}
		}


		/// @brief Resizes the container to contain count elements.
		/// @param count new size of the container
		/// If the current size is greater than count, the container is reduced to its last count elements.
		/// If the current size is less than count, additional default-inserted elements are prepended.
		/// Basic exception guarantee.
		void resize_front(size_type new_size)
		{
			if (new_size == size())
				// No-op
				return; 
		
			if (new_size == 0) {
				clear();
				return;
			}

			if (SEQ_UNLIKELY(!d_data))
				d_data = make_data(get_allocator());

			if (new_size > size()) {
				reserve(new_size);
				size_type diff = new_size - size();
				chunk_type* front = d_data->endNode();
				if (size()) {

					// Fill front first chunk
					front = static_cast<chunk_type*>(d_data->endNode()->next);
					if (front->start != 0) {
						while (front->start != 0 && diff) {
							// Might throw, ok
							construct_ptr(front->buffer() + front->start - 1);
							--front->start;
							front->used |= 1ULL << front->start;
							++d_data->size;
							--diff;
						}
						if (front->used == full)
							remove_free_node(front);
					}

					if (diff == 0) return; // Finished!
				}

				// Add chunks
				size_type chunks = diff / chunk_type::count;
				size_type rem = diff % chunk_type::count;
			
				while (chunks--) {
					// Might throw, ok
					front = make_chunk(static_cast<chunk_type*>(&d_data->end), front);
					remove_free_node(front);
					front->used = full;
					front->start = front->end = chunk_type::count;

					try {
						while (front->start != 0) {
							// Might throw, ok
							construct_ptr(front->buffer() + front->start - 1);
							--front->start;
						}
					
					}
					catch (...) {
						// In case of exception, remove front chunk
						destroy_node_elements(front);
						remove_node(front);
						d_data->deallocate_chunk(front);
						throw;
					}
					d_data->size += chunk_type::count;
				}
				// Add remaining
				if (rem) {
					//might throw, ok
					front = make_chunk(static_cast<chunk_type*>(&d_data->end), front);
					front->start = front->end = chunk_type::count;
					front->used = ((1ULL << rem) - 1ULL) << (chunk_type::count - rem);
					size_type target = chunk_type::count - rem;
					try {
						while (front->start != static_cast<int>(target)) {
							construct_ptr(front->buffer() + front->start - 1);
							--front->start;
						}
					}
					catch (...) {
						// In case of exception, remove front chunk
						destroy_node_elements(front);
						remove_free_node(front);
						remove_node(front);
						d_data->deallocate_chunk(front);
						throw;
					}
					d_data->size += rem;
				}
			
			}
			else {

				chunk_type* front = static_cast<chunk_type*>(d_data->end.next);
				difference_type diff = static_cast<difference_type>(size() - new_size);

				// empty last chunk
				while (front == d_data->end.next && diff--)
					pop_front();

				while (diff > static_cast<difference_type>(chunk_type::count)) {
					// destroy full chunks
					front = static_cast<chunk_type*>(d_data->end.next);
					unsigned size = front->size();
					diff -= size;
					d_data->size -= size;
					if (front->used != full)
						remove_free_node(front);
					destroy_node_elements(front);
					remove_node(front);
					d_data->deallocate_chunk(front);
				}

				// finish
				while (diff--)
					pop_front();

			}
		}

		/// @brief Resizes the container to contain count elements.
		/// @param new_size new size of the container
		/// @param value the value to initialize the new elements with
		/// If the current size is greater than count, the container is reduced to its last count elements.
		/// If the current size is less than count, additional copies of value are prepended.
		/// Basic exception guarantee.
		void resize_front(size_type new_size, const T & value)
		{
			if (new_size == size())
				// No-op
				return;

			if (new_size == 0) {
				clear();
				return;
			}

			if (SEQ_UNLIKELY(!d_data)) 
				d_data = make_data(get_allocator());

			if (new_size > size()) {
				reserve(new_size);
				size_type diff = new_size - size();
				chunk_type* front = d_data->endNode();
				if (size()) {

					// Fill front first chunk
					front = static_cast<chunk_type*>(d_data->endNode()->next);
					if (front->start != 0) {
						while (front->start != 0 && diff) {
							// Might throw, ok
							construct_ptr(front->buffer() + front->start - 1, value);
							--front->start;
							front->used |= 1ULL << front->start;
							++d_data->size;
							--diff;
						}
						if (front->used == full)
							remove_free_node(front);
					}

					if (diff == 0) return; // Finished!
				}

				// Add chunks
				size_type chunks = diff / chunk_type::count;
				size_type rem = diff % chunk_type::count;

				while (chunks--) {
					// Might throw, ok
					front = make_chunk(static_cast<chunk_type*>(&d_data->end), front);
					remove_free_node(front);
					front->used = full;
					front->start = front->end = chunk_type::count;

					try {
						while (front->start != 0) {
							// Might throw, ok
							construct_ptr(front->buffer() + front->start - 1, value);
							--front->start;
						}

					}
					catch (...) {
						// In case of exception, remove front chunk
						destroy_node_elements(front);
						remove_node(front);
						d_data->deallocate_chunk(front);
						throw;
					}
					d_data->size += chunk_type::count;
				}
				// Add remaining
				if (rem) {
					//might throw, ok
					front = make_chunk(static_cast<chunk_type*>(&d_data->end), front);
					front->start = front->end = chunk_type::count;
					front->used = ((1ULL << rem) - 1ULL) << (chunk_type::count - rem);
					size_type target = chunk_type::count - rem;
					try {
						while (front->start != static_cast<int>(target)) {
							construct_ptr(front->buffer() + front->start - 1, value);
							--front->start;
						}
					}
					catch (...) {
						// In case of exception, remove front chunk
						destroy_node_elements(front);
						remove_free_node(front);
						remove_node(front);
						d_data->deallocate_chunk(front);
						throw;
					}
					d_data->size += rem;
				}

			}
			else {

				chunk_type* front = static_cast<chunk_type*>(d_data->end.next);
				difference_type diff = static_cast<difference_type>(size() - new_size);

				// empty last chunk
				while (front == d_data->end.next && diff--)
					pop_front();

				while (diff > static_cast<difference_type>(chunk_type::count)) {
					// destroy full chunks
					front = static_cast<chunk_type*>(d_data->end.next);
					unsigned size = front->size();
					diff -= size;
					d_data->size -= size;
					if (front->used != full)
						remove_free_node(front);
					destroy_node_elements(front);
					remove_node(front);
					d_data->deallocate_chunk(front);
				}

				// finish
				while (diff--)
					pop_front();
			}
		}

		/// @brief Replaces the contents of the container.
		/// @tparam Iter LegacyInputIterator type
		/// @param first range to copy the elements from
		/// @param last range to copy the elements from
		/// Basic exception guarantee. 
		template<class Iter>
		void assign(Iter first, Iter last)
		{
			if (SEQ_UNLIKELY(!d_data)) 
				d_data = make_data(get_allocator());
			assign_cat(first, last, typename std::iterator_traits<Iter>::iterator_category());
		}

		/// @brief Replaces the contents of the container.
		/// @param lst	initializer list to copy the values from
		/// Basic exception guarantee. 
		void assign(const std::initializer_list<T>& lst)
		{
			if (SEQ_UNLIKELY(!d_data)) 
				d_data = make_data(get_allocator());
			assign_cat(lst.begin(), lst.end(), std::random_access_iterator_tag());
		}

		/// @brief Replaces the contents with new_size copies of value \a value
		/// @param new_size the new size of the container
		/// @param value the value to initialize elements of the container with
		/// Basic exception guarantee. 
		void assign(size_type new_size, const T& value)
		{
			if (SEQ_UNLIKELY(!d_data))
				d_data = make_data(get_allocator());
			assign_cat(cvalue_iterator<T>(0, value), cvalue_iterator<T>(new_size, value), std::random_access_iterator_tag());
		}

		/// @brief Pack the sequence to remove empty slots and release unused memory.
		/// All empty slots created by calls to #erase() are filled by moving each element toward the beginning.
		/// This function might deallocate unused buckets created by the shrinking operation, but
		/// never allocates memory.
		/// Invalidates all references and iterators.
		/// Basic exception guarantee. 
		void shrink_to_fit() 
		{
			if(d_data)
				d_data->shrink_to_fit();
		}

		/// @brief Increase the capacity of the vector to a value that's greater or equal to new_cap.
		/// @param new_cap new capacity of the sequence
		/// If new_cap is greater than the current capacity(), new storage is allocated, otherwise the function does nothing.
		/// reserve() does not change the size of the sequence.
		/// Note that reserve() only works with \a OptimizeForSpeed flag.
		/// Basic exception guarantee. 
		void reserve(size_t new_cap)
		{
			if (SEQ_UNLIKELY(!d_data)) 
				d_data = make_data(get_allocator());
			if (new_cap > d_data->size) {
				size_t chunks = new_cap / count + (new_cap % count ? 1 : 0);
				d_data->resize(chunks);
			}
		}

		/// @brief Removes the first element of the container.
		/// Calling pop_front on an empty container results in undefined behavior.
		/// Iterators and references to the first element are invalidated.
		/// Other iterators and references remain valid.
		void pop_front() noexcept
		{
			SEQ_ASSERT_DEBUG(size() > 0, "pop_front() on an empty container");
			T* ptr = front_ptr();
			chunk_type* node = static_cast<chunk_type*>(d_data->end.next);

			if (SEQ_UNLIKELY(node->used == full))
				add_free_node(node);

			node->used &= ~(1ULL << static_cast<std::uint64_t>(node->start));
			destroy_ptr(ptr);
			if (SEQ_UNLIKELY(node->used == 0ULL)) 
				pop_front_remove_chunk(node);
			else {
				++node->start;
				if (!(node->used & (1ULL << static_cast<std::uint64_t>(node->start))))
					node->start = static_cast<int>(bit_scan_forward_64(node->used));
			}
			--d_data->size;
		}

		/// @brief Removes the last element of the container.
		/// Calling pop_back on an empty container results in undefined behavior.
		/// Iterators and references to the last element are invalidated.
		/// Other iterators and references remain valid.
		void pop_back() noexcept
		{
			SEQ_ASSERT_DEBUG(size() > 0, "pop_back() on an empty container");
			T* ptr = back_ptr();
			chunk_type* node = static_cast<chunk_type*>(d_data->end.prev);

			if (SEQ_UNLIKELY(node->used == full))
				add_free_node(node);

			node->used &= ~(1ULL << static_cast<std::uint64_t>(ptr - node->buffer()));
			destroy_ptr(ptr);
			if (SEQ_UNLIKELY(node->used == 0ULL)) 
				pop_back_remove_chunk(node);
			else {
				if(!(node->used & (1ULL << static_cast<std::uint64_t>(--node->end -1))))
					node->end = static_cast<int>(bit_scan_reverse_64(node->used)) + 1;
			}
			--d_data->size;
		}

		/// @brief Returns an iterator to given position.
		/// @param pos position in the sequence
		/// While sequence is not a random access container, operations on iterators are still faster
		/// than for a conventional std::list.
		/// This function is faster than using begin()+pos as it might start from the end to reach
		/// the required position.
		SEQ_ALWAYS_INLINE auto iterator_at(size_type pos) noexcept -> iterator
		{
			return d_data ? d_data->iterator_at(pos) : end();
		}

		/// @brief Returns a const_iterator to given position.
		/// @param pos position in the sequence
		/// While sequence is not a random access container, operations on iterators are still faster
		/// than for a conventional std::list.
		/// This function is faster than using begin()+pos as it might start from the end to reach
		/// the required position.
		SEQ_ALWAYS_INLINE auto iterator_at(size_type pos) const noexcept -> const_iterator
		{
			return d_data ?  d_data->iterator_at(pos) : end();
		}
	
		/// @brief Erases the specified element from the container.
		/// @param it iterator to the element to remove
		/// @return Iterator following the last removed element
		/// This function performs in O(1).
		/// Iterators and references to the erased element are invalidated.
		/// Iterators and references to other elements in the sequence remain valid.
		auto erase(const_iterator it) noexcept -> iterator
		{
			SEQ_ASSERT_DEBUG(size() > 0, "erase() on an empty container");
			SEQ_ASSERT_DEBUG(it.node->used & (1ULL << (it.pos)), "invalide erase position");
			SEQ_ASSERT_DEBUG(it != end(), "erasing at the end");

			T* ptr = it.node->buffer() + it.pos;

			iterator res = it;
			++res;

			destroy_ptr(ptr);

			if (SEQ_UNLIKELY(it.node->used == full))
				add_free_node(it.node);

			it.node->used &= ~(1ULL << (it.pos));

			
			if(SEQ_LIKELY(it.node->used != 0)) 
			{
				if (it.pos == it.node->start)
					it.node->start = static_cast<int>(bit_scan_forward_64(it.node->used));
				if (it.pos == it.node->end - 1)
					it.node->end = static_cast<int>(bit_scan_reverse_64(it.node->used)) + 1;
			}
			else
				erase_remove_chunk(it.node);

			--d_data->size;
			return res;
		}

		/// @brief Erases the specified elements from the container.
		/// @param first iterator to the element to remove
		/// @param last iterator to the element to remove
		/// @return Iterator following the last removed element
		/// This function performs in O(1).
		/// Iterators and references to the erased elements are invalidated.
		/// Iterators and references to other elements in the sequence remain valid.
		auto erase(const_iterator first, const_iterator last) noexcept -> iterator
		{
			SEQ_ASSERT_DEBUG(first <= last, "invalid erase range");
			if (first == last)
				return last;
			if (first == begin() && last == end()) {
				clear();
				return end();
			}

			iterator res = last;

			chunk_type* node = first.node;
			bool was_full = first.node->used == chunk_type::full;

			while (first != last) {
				destroy_ptr(&(*first));
				first.node->used &= ~(1ULL << static_cast<std::uint64_t>(first.pos));
				++first;
				--d_data->size;
				if (node != first.node) {
					// We just changed the node
					if (node->used == 0ULL) {
						if(!was_full)
							remove_free_node(node);
						remove_node(node);
						d_data->deallocate_chunk(node);
					}
					else {
						node->start = static_cast<int>(bit_scan_forward_64(node->used));
						node->end = static_cast<int>(bit_scan_reverse_64(node->used))+1;
						if (was_full && node->used != full)
							add_free_node(node);
					}
					node = first.node;
					was_full = first.node->used == chunk_type::full;
				}
			}
			if (node != &d_data->end) {
				node->start = static_cast<int>(bit_scan_forward_64(node->used));
				node->end = static_cast<int>(bit_scan_reverse_64(node->used)) + 1;
				if (was_full && node->used != full)
					add_free_node(node);
			}
			return res;

		}

		/// @brief Sort the sequence.
		/// The sequence is sorted using the std::less<value_type> comparator.
		/// sort() relies on the <a href="https://github.com/orlp/pdqsort">pdqsort</a> implementation from Orson Peters, and should be as fast as sorting a std::deque.
		/// This invalidates all iterators and references.
		void sort()
		{
			sort(std::less<T>());
		}

		/// @brief Sort the sequence using given comparator.
		/// sort() relies on the <a href="https://github.com/orlp/pdqsort">pdqsort</a> implementation from Orson Peters, and should be as fast as sorting a std::deque.
		/// This invalidates all iterators and references.
		template<class Less >
		void sort(Less less)
		{
			if (empty())
				return;

			using iter = detail::sequence_ra_iterator< sequence<T, Allocator, layout> >;
			using data = typename iter::Data;

			data d;
			d.size = size();
			d.end = d_data->endNode();
			d_data->shrink_to_fit(&d.chunks);

			iter begin(&d, static_cast<chunk_type*>(d_data->endNode()->next));
			iter end(&d, static_cast<chunk_type*>(d_data->endNode()), d_data->endNode()->start, static_cast<difference_type>(size()));
			pdqsort(begin, end, less);
		}

		/// @brief Sort the sequence.
		/// The sequence is sorted using the std::less<value_type> comparator.
		/// stable_sort() relies on std::stable_sort.
		/// This invalidates all iterators and references.
		void stable_sort()
		{
			stable_sort(std::less<T>());
		}

		/// @brief Sort the sequence using given comparator.
		/// The sequence is sorted using the std::less<value_type> comparator.
		/// stable_sort() relies on std::stable_sort.
		/// This invalidates all iterators and references.
		template<class Less >
		void stable_sort(Less less)
		{
			if (empty())
				return;

			using iter = detail::sequence_ra_iterator< this_type >;
			using data = typename iter::Data;

			data d;
			d.size = size();
			d.end = d_data->endNode();
			d_data->shrink_to_fit(&d.chunks);

			iter begin(&d, static_cast<chunk_type*>(d_data->endNode()->next));
			iter end(&d, static_cast<chunk_type*>(d_data->endNode()), d_data->endNode()->start, size());
			std::stable_sort(begin, end, less);
		}

		/// @brief Returns an iterator to the first element of the sequence.
		auto begin() noexcept -> iterator { return iterator(d_data ? static_cast<chunk_type*>(d_data->end.next) : nullptr); }
		/// @brief Returns an iterator to the element following the last element of the sequence.
		auto end() noexcept -> iterator { return iterator(d_data ? static_cast<chunk_type*>(&d_data->end) : nullptr, 0); }
		/// @brief Returns an iterator to the first element of the sequence.
		auto begin() const noexcept -> const_iterator { return const_iterator(d_data ? static_cast<chunk_type*>(d_data->end.next) : nullptr); }
		/// @brief Returns an iterator to the element following the last element of the sequence.
		auto end() const noexcept -> const_iterator { return const_iterator(d_data ? static_cast<chunk_type*>(&d_data->end) : nullptr, 0); }
		/// @brief Returns an iterator to the first element of the sequence.
		auto cbegin() const noexcept -> const_iterator { return const_iterator(d_data ? static_cast<chunk_type*>(d_data->end.next) : nullptr); }
		/// @brief Returns an iterator to the element following the last element of the sequence.
		auto cend() const noexcept -> const_iterator { return const_iterator(d_data ? static_cast<chunk_type*>(&d_data->end) : nullptr, 0); }
		/// @brief Returns a reverse iterator to the first element of the reversed list.
		auto rbegin() noexcept -> reverse_iterator { return reverse_iterator(end()); }
		/// @brief Returns a reverse iterator to the first element of the reversed list.
		auto rbegin() const noexcept -> const_reverse_iterator { return const_reverse_iterator(end()); }
		/// @brief Returns a reverse iterator to the element following the last element of the reversed list.
		auto rend() noexcept -> reverse_iterator { return reverse_iterator(begin()); }
		/// @brief Returns a reverse iterator to the element following the last element of the reversed list.
		auto rend() const noexcept -> const_reverse_iterator { return const_reverse_iterator(begin()); }
		/// @brief Returns a reverse iterator to the first element of the reversed list.
		auto crbegin() const noexcept -> const_reverse_iterator { return rbegin(); }
		/// @brief Returns a reverse iterator to the element following the last element of the reversed list.
		auto crend() const noexcept -> const_reverse_iterator { return rend(); }
	};


	/// @brief Specialization of is_relocatable for sequence type.
	template<class T, class Al, LayoutManagement L, bool A>
	struct is_relocatable<sequence<T,Al,L,A> > : is_relocatable<Al> {};


}//end namespace seq



#endif
