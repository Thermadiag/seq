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

#ifndef SEQ_SEQUENCE_HPP
#define SEQ_SEQUENCE_HPP

/** @file */

#include <climits>
#include <vector>
#include <utility>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>

#include "type_traits.hpp"
#include "internal/utils.hpp"
#include "net_sort.hpp"

namespace seq
{
	namespace detail
	{
		template<std::uint64_t Count>
		constexpr std::uint64_t shift_left()
		{
			if constexpr (Count == 64)
				return 0;
			else
				return 1ULL << Count;
		}

		template<class T>
		constexpr std::uint64_t chunk_count()
		{
			return sizeof(T) <= 8 ? 64 : sizeof(T) <= 16 ? 32 : sizeof(T) <= 32 ? 16 : sizeof(T) <= 64 ? 8 : 4;
		}

		

		// Contiguous storage for up to 64 objects
		template<class T, bool Aligned, class Derived>
		struct base_list_chunk
		{
			using derived_type = Derived;

			// Max number of elements
			static constexpr std::uint64_t count = chunk_count<T>();
			// log2(count)
			static constexpr std::uint64_t count_bits = sizeof(T) <= 8 ? 6 : sizeof(T) <= 16 ? 5 : sizeof(T) <= 32 ? 4 : sizeof(T) <= 64 ? 3 : 2;
			// Mask value when full
			static constexpr std::uint64_t full = (count == 64ULL ? static_cast<std::uint64_t>(-1) : (shift_left<count>() - 1ULL));

			static constexpr bool aligned = Aligned;

			// Previous node
			base_list_chunk* prev;
			// Next node
			base_list_chunk* next;
			// Previous node with at least one free element
			base_list_chunk* prev_free;
			// Next node with at least one free element
			base_list_chunk* next_free;
			// Mask (bit 1 for allocated, 0 for free)
			std::uint64_t used;

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

			SEQ_ALWAYS_INLINE auto derived() noexcept -> derived_type* { return static_cast<derived_type*>(this); }
			SEQ_ALWAYS_INLINE auto derived() const noexcept -> const derived_type* { return static_cast<const derived_type*>(this); }

			// Raw buffer access
			template<class Int>
			SEQ_ALWAYS_INLINE T* raw_slot(Int i) noexcept
			{
				return derived()->storage.raw_slot((size_t)i);
			}
			template<class Int>
			SEQ_ALWAYS_INLINE T* live_slot(Int i) noexcept
			{
				return derived()->storage.live_slot((size_t)i);
			}
			template<class Int>
			SEQ_ALWAYS_INLINE const T* raw_slot(Int i) const noexcept
			{
				return derived()->storage.raw_slot((size_t)i);
			}
			template<class Int>
			SEQ_ALWAYS_INLINE const T* live_slot(Int i) const noexcept
			{
				return derived()->storage.live_slot((size_t)i);
			}

			SEQ_ALWAYS_INLINE T& front() { return *live_slot(start); }
			SEQ_ALWAYS_INLINE const T& front() const { return *live_slot(start); }
			SEQ_ALWAYS_INLINE T& back() { return *live_slot(end-1); }
			SEQ_ALWAYS_INLINE const T& back() const {return *live_slot(end - 1); }
		};

		template<class T, bool Aligned>
		struct find_chunk_alignment
		{
			static constexpr size_t aligned = std::max(chunk_count<T>(), alignof(std::max_align_t));
			static constexpr size_t non_aligned = std::max(alignof(T), alignof(std::max_align_t));
			static constexpr size_t value = Aligned ? aligned : non_aligned;
		};

		// Actual chunk class, store up to 64 objects
		// Its size is a multiple of 64 bytes
		template<class T, bool Aligned>
		struct alignas(find_chunk_alignment<T, Aligned>::value) list_chunk : public base_list_chunk<T, Aligned, list_chunk<T, Aligned>>
		{
			using base = base_list_chunk<T, Aligned, list_chunk<T, Aligned>>;

			// Storage for values
			RawStorage<T, base::count> storage;
		};

		//
		// const iterator for sequence object
		//
		template<class List>
		class sequence_const_iterator
		{
		public:
			using iterator_category = std::bidirectional_iterator_tag;
			using value_type = typename List::value_type;
			using difference_type = typename List::difference_type;
			using size_type = size_t;
			using pointer = typename List::const_pointer;
			using reference = const value_type&;
			using list_data = typename List::Data;
			using node_type = typename List::node_type;
#ifdef _MSC_VER
			using pos_type = int;
#else
			using pos_type = difference_type;
#endif

			static constexpr int count = node_type::count;

			node_type* node = nullptr;
			pos_type pos = 0;

			SEQ_ALWAYS_INLINE sequence_const_iterator() noexcept {}
			SEQ_ALWAYS_INLINE sequence_const_iterator(const node_type* n) noexcept
			  : node(const_cast<node_type*>(n))
			  , pos(n ? static_cast<pos_type>(n->start) : 0)
			{
			}
			SEQ_ALWAYS_INLINE sequence_const_iterator(const node_type* n, pos_type p) noexcept
			  : node(const_cast<node_type*>(n))
			  , pos(p)
			{
			}
			sequence_const_iterator(const sequence_const_iterator&) noexcept = default;

			SEQ_ALWAYS_INLINE std::uintptr_t as_uint() const noexcept
			{
				static_assert(node_type::aligned);
				return (reinterpret_cast<std::uintptr_t>(node)) | static_cast<std::uintptr_t>(pos);
			}
			SEQ_ALWAYS_INLINE void from_uint(std::uintptr_t p) noexcept
			{
				static_assert(node_type::aligned);
				node = reinterpret_cast<node_type*>(p & ~(node_type::count - 1));
				pos = static_cast<pos_type>(p & (node_type::count - 1));
			}

			SEQ_ALWAYS_INLINE auto operator*() const noexcept -> reference
			{
				SEQ_ASSERT_DEBUG(pos >= node->start && pos < node->end, "invalid iterator position");
				return *node->live_slot(pos);
			}
			SEQ_ALWAYS_INLINE auto operator->() const noexcept -> pointer { return std::pointer_traits<pointer>::pointer_to(**this); }
			void update_incr_pos() noexcept
			{
				if (pos == node->end) {
					node = static_cast<node_type*>(node->next);
					pos = static_cast<pos_type>(node->start);
				}
				else {
					pos = static_cast<pos_type>(bit_scan_forward_64(node->used >> pos)) + pos; 
				}
			}
			SEQ_ALWAYS_INLINE auto operator++() noexcept -> sequence_const_iterator&
			{
				++pos;
				if SEQ_LIKELY (node->used == node_type::full) {
					if SEQ_UNLIKELY (pos == count) {
						node = static_cast<node_type*>(node->next);
						pos = node->start;
					}
					return *this;
				}
				if SEQ_UNLIKELY (pos == count || !((node->used & (1ULL << pos)))) {
					update_incr_pos();
				}
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator++(int) noexcept -> sequence_const_iterator
			{
				sequence_const_iterator _Tmp = *this;
				++(*this);
				return _Tmp;
			}
			void update_decr_pos() noexcept
			{
				if (pos < node->start) {
					node = static_cast<node_type*>(node->prev);
					pos = (node->end - 1);
				}
				else {
					pos = static_cast<pos_type>(bit_scan_reverse_64(node->used & ((1ULL << pos) - 1ULL))); 
				}
			}
			SEQ_ALWAYS_INLINE auto operator--() noexcept -> sequence_const_iterator&
			{
				--pos;
				if SEQ_LIKELY (node->used == node_type::full) {
					if SEQ_UNLIKELY (pos == -1) {
						node = static_cast<node_type*>(node->prev);
						pos = (node->end - 1);
					}
					return *this;
				}
				if SEQ_UNLIKELY (pos == -1 || !((node->used & (1ULL << pos))) /*ptr == node->end*/) {
					update_decr_pos();
				}
				// SEQ_ASSERT_DEBUG(pos >= 0 && pos < node->end, "invalid iterator position");
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator--(int) noexcept -> sequence_const_iterator
			{
				sequence_const_iterator _Tmp = *this;
				--(*this);
				return _Tmp;
			}
		};

		//
		// iterator for sequence object
		//
		template<class List>
		class sequence_iterator : public sequence_const_iterator<List>
		{
		public:
			using base_type = sequence_const_iterator<List>;
			using iterator_category = std::bidirectional_iterator_tag;
			using value_type = typename List::value_type;
			using difference_type = typename List::difference_type;
			using size_type = size_t;
			using pointer = typename List::pointer;
			using reference = value_type&;

			SEQ_ALWAYS_INLINE sequence_iterator() noexcept {}
			SEQ_ALWAYS_INLINE sequence_iterator(const typename base_type::node_type* n) noexcept
			  : sequence_const_iterator<List>(n)
			{
			}
			SEQ_ALWAYS_INLINE sequence_iterator(const typename base_type::node_type* n, typename base_type::pos_type p) noexcept
			  : sequence_const_iterator<List>(n, p)
			{
			}
			SEQ_ALWAYS_INLINE sequence_iterator(const sequence_iterator& other) noexcept
			  : base_type(other)
			{
			}
			SEQ_ALWAYS_INLINE auto operator*() const noexcept -> reference { return const_cast<reference>(base_type::operator*()); }
			SEQ_ALWAYS_INLINE auto operator->() const noexcept -> pointer { return std::pointer_traits<pointer>::pointer_to(**this); }
			SEQ_ALWAYS_INLINE auto operator++() noexcept -> sequence_iterator&
			{
				base_type::operator++();
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator++(int) noexcept -> sequence_iterator
			{
				sequence_iterator _Tmp = *this;
				base_type::operator++();
				return _Tmp;
			}
			SEQ_ALWAYS_INLINE auto operator--() noexcept -> sequence_iterator&
			{
				base_type::operator--();
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator--(int) noexcept -> sequence_iterator
			{
				sequence_iterator _Tmp = *this;
				base_type::operator--();
				return _Tmp;
			}
		};

		template<class List>
		SEQ_ALWAYS_INLINE bool operator==(const sequence_const_iterator<List>& a, const sequence_const_iterator<List>& b) noexcept
		{
			return a.node == b.node && a.pos == b.pos;
		}
		template<class List>
		SEQ_ALWAYS_INLINE bool operator!=(const sequence_const_iterator<List>& a, const sequence_const_iterator<List>& b) noexcept
		{
			return a.node != b.node || a.pos != b.pos;
		}

		template<class List>
		struct sequence_ra_iterator
		{
			using node_type = typename List::node_type;
			struct Data
			{
				std::vector<node_type*> chunks{};
				node_type* end;
				size_t size{};
			};
			using iterator_category = std::random_access_iterator_tag;
			using value_type = typename List::value_type;
			using difference_type = typename List::difference_type;
			using pointer = typename List::pointer;
			using reference = value_type&;
			using pos_type = difference_type;

			static constexpr int count = node_type::count;

			Data* data = nullptr;
			node_type* node = nullptr;
			difference_type abs_pos = 0;
			pos_type pos = 0;

			SEQ_ALWAYS_INLINE sequence_ra_iterator() noexcept {}
			SEQ_ALWAYS_INLINE sequence_ra_iterator(const Data* d, const node_type* n) noexcept
			  : data(const_cast<Data*>(d))
			  , node(const_cast<node_type*>(n))
			  , abs_pos(0)
			  , pos(n->start)
			{
			}
			SEQ_ALWAYS_INLINE sequence_ra_iterator(const Data* d, const node_type* n, pos_type p, difference_type _abs_pos) noexcept
			  : data(const_cast<Data*>(d))
			  , node(const_cast<node_type*>(n))
			  , abs_pos(_abs_pos)
			  , pos(p)
			{
			}

			SEQ_ALWAYS_INLINE auto absolutePos() const noexcept -> size_t { return static_cast<size_t>(abs_pos); }
			SEQ_ALWAYS_INLINE void setAbsolutePos(std::size_t _abs_pos) noexcept
			{
				SEQ_ASSERT_DEBUG(_abs_pos <= (data->size), "invalid iterator position");
				if SEQ_UNLIKELY (_abs_pos == data->size) {
					node = data->end;
					pos = node->start;
				}
				else {

					size_t front_size = static_cast<size_t>(data->chunks.front()->end - data->chunks.front()->start);
					size_t bucket = (_abs_pos + (node_type::count - front_size)) >> node_type::count_bits;
					node = data->chunks[bucket];
					pos = node->start + static_cast<int>((_abs_pos - (_abs_pos < front_size ? 0 : front_size)) & (node_type::count - 1));
				}
				this->abs_pos = static_cast<difference_type>(_abs_pos);
			}
			SEQ_ALWAYS_INLINE auto operator[](difference_type offset) const noexcept -> reference
			{
				difference_type new_pos = pos + offset;
				if (new_pos >= 0 && new_pos < node->end)
					return *node->live_slot(new_pos);
				return *((*this) + offset);
			}
			SEQ_ALWAYS_INLINE auto operator*() const noexcept -> reference
			{

				SEQ_ASSERT_DEBUG(pos >= node->start && pos < node->end, "invalid iterator position");
				return *node->live_slot(pos);
			}
			SEQ_ALWAYS_INLINE auto operator->() noexcept -> pointer { return std::pointer_traits<pointer>::pointer_to(**this); }
			void update_incr() noexcept
			{
				if (pos == node->end) {
					node = static_cast<node_type*>(node->next);
					pos = node->start;
				}
				else {
					pos = static_cast<pos_type>(bit_scan_forward_64(node->used >> pos) + pos);
				}
			}
			SEQ_ALWAYS_INLINE auto operator++() noexcept -> sequence_ra_iterator&
			{
				SEQ_ASSERT_DEBUG(abs_pos < static_cast<difference_type>(data->size), "invalid iterator position");
				++abs_pos;
				if (++pos >= node->end)
					update_incr();
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator++(int) noexcept -> sequence_ra_iterator
			{
				sequence_ra_iterator _Tmp = *this;
				++(*this);
				return _Tmp;
			}
			void update_decr() noexcept
			{
				if (pos < node->start) {
					node = static_cast<node_type*>(node->prev);
					pos = (node->end - 1);
				}
				else {
					pos = static_cast<pos_type>(bit_scan_reverse_64(node->used & ((1ULL << pos) - 1))); 
				}
			}
			SEQ_ALWAYS_INLINE auto operator--() noexcept -> sequence_ra_iterator&
			{
				SEQ_ASSERT_DEBUG(abs_pos > 0, "invalid iterator position");
				--abs_pos;
				if (--pos < node->start)
					update_decr();
				SEQ_ASSERT_DEBUG(pos >= 0 && pos < node->end, "invalid iterator position");
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator--(int) noexcept -> sequence_ra_iterator
			{
				sequence_ra_iterator _Tmp = *this;
				--(*this);
				return _Tmp;
			}
			SEQ_ALWAYS_INLINE auto operator+=(difference_type diff) noexcept -> sequence_ra_iterator&
			{
				setAbsolutePos(static_cast<size_t>(abs_pos + diff));
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator-=(difference_type diff) noexcept -> sequence_ra_iterator&
			{
				setAbsolutePos(static_cast<size_t>(abs_pos - diff));
				return *this;
			}

			SEQ_ALWAYS_INLINE bool operator==(const sequence_ra_iterator& other) const noexcept { return abs_pos == other.abs_pos; }
			SEQ_ALWAYS_INLINE bool operator!=(const sequence_ra_iterator& other) const noexcept { return abs_pos != other.abs_pos; }
			SEQ_ALWAYS_INLINE bool operator<(const sequence_ra_iterator& other) const noexcept { return abs_pos < other.abs_pos; }
			SEQ_ALWAYS_INLINE bool operator>(const sequence_ra_iterator& other) const noexcept { return abs_pos > other.abs_pos; }
			SEQ_ALWAYS_INLINE bool operator<=(const sequence_ra_iterator& other) const noexcept { return abs_pos <= other.abs_pos; }
			SEQ_ALWAYS_INLINE bool operator>=(const sequence_ra_iterator& other) const noexcept { return abs_pos >= other.abs_pos; }
			SEQ_ALWAYS_INLINE auto operator+(difference_type diff) const noexcept -> sequence_ra_iterator
			{
				sequence_ra_iterator tmp = *this;
				tmp += diff;
				return tmp;
			}
			SEQ_ALWAYS_INLINE auto operator-(difference_type diff) const noexcept -> sequence_ra_iterator
			{
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
	/// In addition, its sort() and shrink_to_fit() members do not preserve reference and iterator stability.
	///
	/// Its main advantages other std::list (and other containers) are:
	///		-	Insertion at both ends is faster than a std::deque or std::vector (without reserve)
	///		-	Walking through the sequence with iterators is usually faster than walking through a std::deque
	///		-	Sorting a sequence is usually as fast as sorting a std::deque.
	///			Note that #sequence::sort() uses seq::net_sort().
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
	template<class T, class Allocator = std::allocator<T>, bool Aligned = false>
	class sequence : private Allocator
	{
		using this_type = sequence<T, Allocator, Aligned>;

	public:
		using value_type = T;
		using pointer = T*;
		using reference = T&;
		using const_pointer = const T*;
		using allocator_type = Allocator;
		using difference_type = typename std::allocator_traits<Allocator>::difference_type;
		using size_type = typename std::allocator_traits<Allocator>::size_type;
		using iterator = detail::sequence_iterator<this_type>;
		using const_iterator = detail::sequence_const_iterator<this_type>;
		using reverse_iterator = std::reverse_iterator<iterator>;
		using const_reverse_iterator = std::reverse_iterator<const_iterator>;
		using chunk_type = detail::list_chunk<T, Aligned>;
		using node_type = detail::base_list_chunk<T, Aligned, chunk_type>;

	private:
		friend class detail::sequence_const_iterator<this_type>; // iterator has access to internal Data object
		friend class detail::sequence_iterator<this_type>;	 // iterator has access to internal Data object
		template<class U>
		using rebind_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<U>;

		static constexpr std::uint64_t count = node_type::count;
		static constexpr std::uint64_t count1 = node_type::count - 1;
		static constexpr std::uint64_t count_bits = node_type::count_bits;
		static constexpr std::uint64_t full = node_type::full;

		static_assert(std::is_nothrow_copy_constructible_v<Allocator>, "sequence only supports nothrow copy constructible allocators");
		static_assert(std::is_pointer_v<typename std::allocator_traits<Allocator>::pointer>, "sequence requires allocators with raw pointer types");

		/// Internal data
		/// We use a pointer to Data internally as it fasten the move copy and assignment, and simplifies th iterator implemention.
		/// In addition, it reduces the size of empty sequences.
		///
		struct Data : private Allocator
		{
			using difference_type = typename std::allocator_traits<Allocator>::difference_type;

			std::size_t size = 0;// full size
			std::size_t max_size = 0;
			std::size_t free_elements = 0;// total number of free values (max count * 2)
			std::size_t total_slots = 0;
			node_type* end_node;// end chunk
			node_type end_free;// end of free chunks

			Data(const Allocator& al, node_type* end) noexcept(std::is_nothrow_copy_constructible_v<Allocator>)
			  : Allocator(al)
			  , end_node(end)
			  , size(0)
			{
				end_free.prev = end_free.next = &end_free;
			}

			~Data() noexcept { clear(); }

			static void destroy_node_elements(node_type* node) noexcept
			{
				// Destroy all valid (constructed) elements of a node
				if (!std::is_trivially_destructible_v<T> && node->used) {
					for (int i = node->start; i < node->end; ++i)
						if (node->used & (1ULL << static_cast<std::uint64_t>(i)))
							destroy_ptr(node->live_slot(i));
				}
				node->start = node->end = 0;
				node->used = 0;
			}

			node_type* allocate_node()
			{
				auto* ret = new (allocate_from<chunk_type>(get_allocator())) chunk_type;
				total_slots += node_type::count;
				return ret;
			}
			void deallocate_node(node_type* n) noexcept
			{
				deallocate_from(get_allocator(), n->derived());
				total_slots -= node_type::count;
			}

			void clear() noexcept
			{
				// Remove free chunks
				while (node_type* c = pop_free()) {
					deallocate_node( c);
				}

				node_type* node = end_node->next;
				while (node != end_node) {
					// Destroy node content only if needed
					if (!std::is_trivially_destructible_v<T> && node->used)
						destroy_node_elements(node);
					// Deallocate chunk
					node_type* next = node->next;
					deallocate_node(node);
					node = next;
				}

				end_node->prev = end_node->next = end_node;
				end_node->prev_free = end_node->next_free = end_node;
				end_free.prev = end_free.next = &end_free;
			}

			void rebind_end(node_type* replacement) noexcept
			{
				node_type* old = end_node;

				if (old->next == old) {
					replacement->next = replacement->prev = replacement;
				}
				else {
					replacement->next = old->next;
					replacement->prev = old->prev;
					replacement->next->prev = replacement;
					replacement->prev->next = replacement;
				}

				if (old->next_free == old) {
					replacement->next_free = replacement->prev_free = replacement;
				}
				else {
					replacement->next_free = old->next_free;
					replacement->prev_free = old->prev_free;
					replacement->next_free->prev_free = replacement;
					replacement->prev_free->next_free = replacement;
				}

				replacement->used = full;
				replacement->start = replacement->end = 0;

				old->prev = old->next = old;
				old->prev_free = old->next_free = old;
				old->used = full;
				old->start = old->end = 0;

				end_node = replacement;
			}

			auto get_allocator() const -> Allocator { return static_cast<const Allocator&>(*this); }

			auto pop_free() noexcept -> node_type*
			{
				// Remove from list of free chunks
				auto node = end_free.next;
				if (node == &end_free)
					return nullptr;
				node->prev->next = node->next;
				node->next->prev = node->prev;
				free_elements -= count;
				return (node);
			}
			void add_free(node_type* node) noexcept
			{
				node->next = end_free.next;
				node->prev = &end_free;
				node->next->prev = node->prev->next = node;
				free_elements += count;
			}

			
			// Returns a const_iterator at given position
			SEQ_ALWAYS_INLINE auto iterator_at(size_t pos) const noexcept -> const_iterator
			{
				SEQ_ASSERT_DEBUG(pos <= size, "sequence::iterator_at: invalid position");
				if (pos == size)
					return const_iterator(end_node, 0);
				else if (pos < this->size / 2)
					return std::next(const_iterator((end_node->next), end_node->next->start), static_cast<difference_type>(pos));
				else
					return std::prev(const_iterator((end_node), 0), static_cast<difference_type>(this->size - pos));
			}
			// Returns an iterator at given position
			SEQ_ALWAYS_INLINE auto iterator_at(size_t pos) noexcept -> iterator
			{
				SEQ_ASSERT_DEBUG(pos <= size, "sequence::iterator_at: invalid position");
				if (pos == size)
					return iterator(end_node, 0);
				else if (pos < this->size / 2)
					return std::next(iterator((end_node->next), end_node->next->start), static_cast<difference_type>(pos));
				else
					return std::prev(iterator((end_node), 0), static_cast<difference_type>(this->size - pos));
			}
		};

		// Allocate and build a chunk with uninitialized storage
		auto make_chunk(node_type* prev, node_type* next) -> node_type*
		{
			// Allocate, might throw
			node_type* ptr = d_data->pop_free();
			if (!ptr)
				ptr = d_data->allocate_node();

			// Set the previous and next chunks
			ptr->prev = prev;
			ptr->next = next;
			prev->next = next->prev = ptr;

			ptr->start = ptr->end = 0;
			ptr->used = 0;

			// Insert this chunk in the list of partially free chunks
			ptr->prev_free = d_data->end_node;
			ptr->next_free = d_end.next_free;
			d_end.next_free = d_end.next_free->prev_free = ptr;

			return ptr;
		}

		void destroy_node(node_type* c)
		{
			if (d_data->free_elements < count * 2)
				d_data->add_free(c);
			else
				d_data->deallocate_node(c);
		}

		void remove_free_node(node_type* node) noexcept
		{
			// Remove from list of free chunks
			node->prev_free->next_free = node->next_free;
			node->next_free->prev_free = node->prev_free;
			node->next_free = node->prev_free = d_data->end_node;
		}
		void add_free_node(node_type* node) noexcept
		{
			// Add to list of free node
			node->next_free = d_end.next_free;
			node->prev_free = d_data->end_node;
			node->next_free->prev_free = node->prev_free->next_free = node;
		}
		void remove_node(node_type* node) noexcept
		{
			// Remove from list of free chunks
			node->prev->next = node->next;
			node->next->prev = node->prev;
			node->next = node->prev = d_data->end_node;
		}

		template<class... Args>
		SEQ_ALWAYS_INLINE auto emplace_anywhere(Args&&... args) -> iterator
		{
			node_type* node = d_end.next_free;
			std::uint64_t index = static_cast<std::uint64_t>(node->start != 0 ? node->start - 1 : (node->end != count ? node->end : static_cast<int>(node->firstFree())));
			T* res = node->raw_slot(index);
			// Construct first as it might throw
			construct_ptr(res, std::forward<Args>(args)...);

			// Remove from list of free chunks if necessary
			node->used |= (1ULL << index);
			if (node->used == full)
				remove_free_node(node);

			// Update boundaries
			if (static_cast<int>(index) == node->end)
				node->end++;
			else if (static_cast<int>(index) < node->start)
				node->start = static_cast<int>(index);

			++d_data->size;
			return iterator(node, static_cast<int>(index));
		}

		// Returns pointer to back value
		SEQ_ALWAYS_INLINE auto back_ptr() const noexcept -> const T*
		{
			SEQ_ASSERT_DEBUG(d_data->size > 0, "empty container");
			return &(d_end.prev->back());
		}
		// Returns pointer to front value
		SEQ_ALWAYS_INLINE auto front_ptr() const noexcept -> const T*
		{
			SEQ_ASSERT_DEBUG(d_data->size > 0, "empty container");
			return &(d_end.next->front());
		}
		// Returns pointer to back value
		SEQ_ALWAYS_INLINE auto back_ptr() noexcept -> T*
		{
			SEQ_ASSERT_DEBUG(d_data->size > 0, "empty container");
			return &(d_end.prev->back());
		}
		// Returns pointer to front value
		SEQ_ALWAYS_INLINE auto front_ptr() noexcept -> T*
		{
			SEQ_ASSERT_DEBUG(d_data->size > 0, "empty container");
			return &(d_end.next->front());
		}

		// Assign  range for non random access iterator
		template<class Iter, class Cat>
		void assign_cat(Iter first, Iter last, Cat)
		{
			if (first == last) {
				clear();
				return;
			}
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
		void assign_cat(Iter first, Iter last, std::random_access_iterator_tag)
		{
			if (first == last) {
				clear();
				return;
			}
			if (first > last)
				throw std::invalid_argument("sequence::assign: invalid iterator range");

			size_type new_count = static_cast<size_t>(last - first);
			resize(new_count);
			std::copy(first, last, begin());
		}

		// Insert back creating a new chunk
		template<class... Args>
		SEQ_NOINLINE(T&)
		emplace_back_new_chunk(node_type* last, Args&&... args)
		{
			// Build chunk, might throw (which is fine)
			last = make_chunk(last, d_data->end_node); // specify chunk index if not dirty
			try {
				// construct object, might throw
				construct_ptr(last->raw_slot(last->start), std::forward<Args>(args)...);
			}
			catch (...) {
				// delete chunk
				remove_node(last);
				remove_free_node(last);
				destroy_node(last);
				throw;
			}

			// Finish
			last->used = 1ULL;
			last->end = last->start + 1;
			++d_data->size;
			return last->front();
		}

		template<class... Args>
		SEQ_NOINLINE(T&)
		emplace_front_new_chunk(node_type* first, Args&&... args)
		{
			// Build chunk, migh throw (which is fine)
			first = make_chunk(d_data->end_node, first);
			first->end = count;

			try {
				// construct object, might throw
				construct_ptr(first->raw_slot(first->end-1), std::forward<Args>(args)...);
			}
			catch (...) {
				// delete chunk
				remove_node(first);
				remove_free_node(first);
				destroy_node(first);
				throw;
			}

			first->used = (1ULL << (count - 1));
			first->start = first->end - 1;
			++d_data->size;
			return first->front();
		}

		SEQ_ALWAYS_INLINE void make_data_if_null()
		{
			if SEQ_UNLIKELY (!d_data) {
				d_data.reset(make_data(get_allocator(), &d_end));
				d_data->max_size = max_size();
			}
		}

		template<class Alloc, bool Align>
		void import(const sequence<T, Alloc, Align>& other)
		{
			make_data_if_null();

			// Assign another sequence

			// Check self assignment
			if (this == &other)
				return;

			size_t osize = other.size();

			if (osize == size()) {
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
				node_type* last = d_data->end_node;

				// First, fill last chunk
				if (size()) {

					// Fill back last chunk
					last = d_end.prev;
					if (last->end != node_type::count) {
						while (last->end != node_type::count && diff) {
							// Might throw, fine
							construct_ptr(last->raw_slot(last->end), *other_it);
							last->used |= 1ULL << last->end;
							++last->end;
							++d_data->size;
							++other_it;
							--diff;
						}
						if (last->used == full)
							remove_free_node(last);
					}

					if (diff == 0)
						return; // Finished!
				}

				// Add chunks
				size_type chunks = diff / node_type::count;
				size_type rem = diff % node_type::count;

				while (chunks--) {
					last = make_chunk(last, d_data->end_node);
					remove_free_node(last);

					try {
						// Fill last chunk, might throw
						while (last->end != node_type::count) {
							construct_ptr(last->raw_slot( last->end), *other_it);
							last->used |= 1ull << (uint64_t)last->end;
							++last->end;
							++other_it;
						}
					}
					catch (...) {
						// In case of exception, remove full chunk
						remove_node(last);
						Data::destroy_node_elements(last);
						destroy_node(last);
						throw;
					}
					d_data->size += node_type::count;
				}
				// Add remaining
				if (rem) {
					// Might throw, ok
					last = make_chunk(last, d_data->end_node);

					try {
						// Fill last chunk, might throw
						while (last->end != static_cast<int>(rem)) {
							construct_ptr(last->raw_slot(last->end), *other_it);
							last->used |= 1ull << (uint64_t)last->end;
							++last->end;
							++other_it;
						}
					}
					catch (...) {
						// In case of exception, remove full chunk
						remove_node(last);
						Data::destroy_node_elements(last);
						remove_free_node(last);
						destroy_node(last);
						throw;
					}
					d_data->size += rem;
				}
			}
			else {
				// Assign a smaller sequence

				// Copy first part
				std::copy(other.begin(), other.end(), begin());

				node_type* last = d_end.prev;
				difference_type diff = static_cast<difference_type>(size() - osize);

				// empty last chunk
				while (last == d_end.prev && diff) {
					pop_back();
					--diff;
				}

				while (diff > static_cast<difference_type>(node_type::count)) {
					// Destroy full chunks
					last = d_end.prev;
					unsigned size = last->size();
					diff -= size;
					d_data->size -= size;
					if (last->used != full)
						remove_free_node(last);
					Data::destroy_node_elements(last);
					remove_node(last);
					destroy_node(last);
				}

				// Finish
				while (diff--)
					pop_back();
			}
		}

		void pop_front_remove_chunk(node_type* node) noexcept
		{
			// Remove chunk due to pop_front() call

			// remove from list
			remove_node(node);
			// remove from free list
			remove_free_node(node);
			destroy_node(node);
		}

		void pop_back_remove_chunk(node_type* node) noexcept
		{
			// Remove chunk due to pop_front() call

			// remove from list
			remove_node(node);
			// remove from free list
			remove_free_node(node);
			destroy_node(node);
		}

		void erase_remove_chunk(node_type* node) noexcept
		{
			// remove from list
			remove_node(node);
			// remove from free list
			remove_free_node(node);
			destroy_node(node);
		}

		void shrink_to_fit_internal(std::vector<node_type*>* vec_chunk = nullptr)
		{
			if (empty()) {
				// Remove free chunks
				if (d_data) {
					while (node_type* c = d_data->pop_free()) 
						d_data->deallocate_node(c);
				}
				return;
			}

			if (vec_chunk) {
				vec_chunk->clear();
				vec_chunk->reserve((size() + node_type::count - 1) / node_type::count);
			}

			auto al = get_allocator();
			sequence<T, Allocator, Aligned> tmp(al);
			tmp.make_data_if_null();

			// Loop through full nodes
			auto start = d_end.next;
			for (;;) {
				if (start == d_data->end_node) {
					// We reach the end without meeting holes, nothing to do except populating vec_chunk
					if (vec_chunk) {
						start = d_end.next;
						while (start != d_data->end_node) {
							vec_chunk->push_back(start);
							start = start->next;
						}
					}
					// Deallocate free nodes
					while (node_type* c = d_data->pop_free())
						d_data->deallocate_node(c);
					return; 
				}
				auto mask = start->used;
				if (mask != full) {
					if (start->next != d_data->end_node)
						break; // Holes
					else {
						// This is the last chunk holes on the right side are allowed
						// mask + 1 should be a power of 2
						if ((mask & (mask + 1)) != 0)
							break;
					}
				}
				start = start->next;
			}


			// If we reach that point, we have holes

			
			// Transfer free chunks
			while (node_type* c = d_data->pop_free()) {
				tmp.d_data->add_free(c);
				d_data->total_slots -= count;
				tmp.d_data->total_slots += count;
			}


			// Reserve enough buckets
			tmp.reserve(size());
			iterator dst;
			for (auto it = begin(); it != end(); ++it) {
				dst = tmp.emplace_back_iter(std::move_if_noexcept(*it));

				if (vec_chunk) {
					// Add node
					if (tmp.size() % node_type::count == 0)
						vec_chunk->push_back(dst.node);
				}
			}
			if (vec_chunk) {
				// Add last node
				if (vec_chunk->empty() || vec_chunk->back() != dst.node)
					vec_chunk->push_back(dst.node);
			}

			// Commit
			d_data.reset(tmp.d_data.release());
			d_data->rebind_end(&d_end);
			
			// Deallocate free nodes
			while (node_type* c = d_data->pop_free())
				d_data->deallocate_node(c);
		}


		struct DataDestroy
		{
			void operator()(Data* d) const noexcept
			{
				if (!d)
					return;

				auto al = d->get_allocator();
				destroy_ptr(d);
				deallocate_from(al, d);
			}
		};

		struct alignas(alignof(chunk_type)) sentinel_type : node_type
		{
			sentinel_type() noexcept { reset(); }

			void reset() noexcept
			{
				this->prev = this->next = this;
				this->prev_free = this->next_free = this;
				this->used = full;
				this->start = this->end = 0;
			}
		};

		// Declare before d_data so d_data is destroyed first.
		sentinel_type d_end;

		// Sequence object internal data
		std::unique_ptr<Data, DataDestroy> d_data;

		auto make_data(const Allocator& al, node_type * end) -> Data*
		{
			Data* p = allocate_from<Data>(al);
			try {
				::new (static_cast<void*>(p)) Data(al, end);
				return p;
			}
			catch (...) {
				deallocate_from(al, p);
				throw;
			}
		}

		template<class... Args>
		SEQ_NOINLINE(auto)
		emplace_back_iter_noinline(Args&&... args) -> iterator
		{
			emplace_back(std::forward<Args>(args)...);
			return iterator(d_end.prev, d_end.prev->end -1);
		}

		SEQ_ALWAYS_INLINE auto allocator_ref() noexcept -> Allocator& { return static_cast<Allocator&>(*this); }
		SEQ_ALWAYS_INLINE auto allocator_ref() const noexcept -> const Allocator& { return static_cast<const Allocator&>(*this); }

	public:
		/// @brief Default constructor, initialize internal data
		sequence() noexcept(std::is_nothrow_default_constructible_v<Allocator>)
		  : Allocator()
		{
		}
		/// @brief Constructor from an allocator object
		/// @param al allocator object
		explicit sequence(const Allocator& al) noexcept(std::is_nothrow_copy_constructible_v<Allocator>)
		  : Allocator(al)
		{
		}
		/// @brief Construct with an initial size and a fill value
		/// @param count initial size
		/// @param value fill value
		/// @param al allocator object
		sequence(size_type count, const T& value, const Allocator& al = Allocator())
		  : Allocator(al)
		{
			resize(count, value);
		}
		/// @brief Construct with an initial size. Objects will be value initialize.
		/// @param count initial size
		/// @param al allocator object
		explicit sequence(size_type count, const Allocator& al = Allocator())
		  : Allocator(al)
		{
			resize(count);
		}
		/// @brief Copy constructor
		/// @param other input sequence to copy
		sequence(const sequence& other)
		  : Allocator(copy_allocator(other.get_allocator()))
		{
			if (other.size())
				import(other);
		}
		/// @brief Copy constructor
		/// @param other input sequence to copy
		/// @param al allocator object
		sequence(const sequence& other, const Allocator& al)
		  : Allocator(al)
		{
			if (other.size())
				import(other);
		}
		/// @brief Move constructor
		sequence(sequence&& other) noexcept(std::is_nothrow_move_constructible_v<Allocator>)
		  : Allocator(std::move(other.allocator_ref()))
		  , d_data(std::move(other.d_data))
		{
			if (d_data)
				d_data->rebind_end(&d_end);
		}
		/// @brief  Allocator-extended move constructor. Using alloc as the allocator for the new container, moving the contents from other; if alloc != other.get_allocator(), this results in
		/// an element-wise move.
		/// @param other another container to be used as source to initialize the elements of the container with
		/// @param alloc allocator object
		sequence(sequence&& other, const Allocator& alloc)
		  : Allocator(alloc)
		{
			if (alloc == other.get_allocator()) {
				d_data = std::move(other.d_data);
				if (d_data)
					d_data->rebind_end(&d_end);
			}
			else if (!other.empty()) {
				reserve(other.size());
				for (auto& v : other)
					emplace_back(std::move(v));
			}
		}
		/// @brief Constructs the sequence with the contents of the initializer list \a lst
		/// @param lst initializer list
		/// @param al allocator object
		sequence(const std::initializer_list<T>& lst, const Allocator& al = Allocator())
		  : sequence(lst.begin(), lst.end(), al)
		{
		}
		/// @brief Constructs the sequence with the contents of the range [first, last).
		/// @tparam Iter iterator type
		/// @param first begin iterator
		/// @param last end iterator
		/// @param al allocator object
		template<class Iter, std::enable_if_t<is_iterator<Iter>::value, int> = 0>
		sequence(Iter first, Iter last, const Allocator& al = Allocator())
		  : Allocator(al)
		{
			assign(first, last);
		}
		/// @brief Destructor
		~sequence() noexcept = default;

		/// @brief Copy operator, basic exception guarantee
		/// @param other input sequence object
		/// @return reference to this
		auto operator=(const sequence& other) -> sequence&
		{
			if (this == std::addressof(other))
				return *this;

			using traits = std::allocator_traits<Allocator>;

			clear();
			// Copy allocator, might throw
			if constexpr (traits::propagate_on_container_copy_assignment::value)
				allocator_ref() = other.get_allocator();

			if (other.size())
				import(other);

			return *this;
		}

		/// @brief Move assignment operator
		/// @param other input sequence object
		/// @return reference to this
		auto operator=(sequence&& other) noexcept(std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value ? std::is_nothrow_move_assignable_v<Allocator>
																	  : std::allocator_traits<Allocator>::is_always_equal::value)
		  -> sequence&
		{
			if (this == std::addressof(other))
				return *this;

			using traits = std::allocator_traits<Allocator>;

			if constexpr (traits::propagate_on_container_move_assignment::value) {

				// Reset this container
				clear();

				// Move allocator, might throw
				static_cast<Allocator&>(*this) = std::move(static_cast<Allocator&>(other));

				d_data = std::move(other.d_data);
				if (d_data)
					d_data->rebind_end(&d_end);
			}
			else {
				if (get_allocator() == other.get_allocator()) {
					clear();
					d_data = std::move(other.d_data);
					if (d_data)
						d_data->rebind_end(&d_end);
				}
				else {
					clear();
					reserve(other.size());
					for (auto& v : other)
						emplace_back(std::move(v));
				}
			}

			return *this;
		}

		/// @brief Exchanges the contents of the container with those of other. Does not invoke any move, copy, or swap operations on individual elements.
		/// @param other other sequence to swap with
		/// All iterators and references remain valid.
		/// Note that iterator holding the past-the-end value will remain attached to their original container.
		void swap(sequence& other) noexcept(!std::allocator_traits<Allocator>::propagate_on_container_swap::value || std::is_nothrow_swappable_v<Allocator>)
		{
			if (this == std::addressof(other))
				return;

			using traits = std::allocator_traits<Allocator>;

			// Do the only potentially throwing operation before changing links.
			if constexpr (traits::propagate_on_container_swap::value) {
				using std::swap;
				swap(allocator_ref(), other.allocator_ref());
			}
			else {
				SEQ_ASSERT_DEBUG(get_allocator() == other.get_allocator(), "swap requires equal non-propagating allocators");
			}

			// Temporarily detach both Data objects from their owning sequences.
			// This is necessary because directly attaching one Data object to the
			// other's sentinel would overwrite links still used by the other Data.
			sentinel_type this_temporary_end;
			sentinel_type other_temporary_end;

			if (d_data)
				d_data->rebind_end(&this_temporary_end);

			if (other.d_data)
				other.d_data->rebind_end(&other_temporary_end);

			d_data.swap(other.d_data);

			// Attach the exchanged Data objects to their new owners.
			if (d_data)
				d_data->rebind_end(&d_end);
			else
				d_end.reset();

			if (other.d_data)
				other.d_data->rebind_end(&other.d_end);
			else
				other.d_end.reset();
		}

		/// @brief Returns the sequence internal data. Internal use only.
		SEQ_ALWAYS_INLINE auto data() noexcept -> Data* { return d_data.get(); }
		SEQ_ALWAYS_INLINE auto data() const noexcept -> const Data* { return d_data.get(); }

		/// @brief Returns the allocator associated with the container.
		SEQ_ALWAYS_INLINE auto get_allocator() const noexcept -> Allocator { return static_cast<const Allocator&>(*this); }

		/// @brief Returns the sequence maximum size.
		SEQ_ALWAYS_INLINE auto max_size() const noexcept -> size_type
		{
			using chunk_allocator = detail::RebindAllocator<Allocator, chunk_type>;
			using chunk_traits = std::allocator_traits<chunk_allocator>;

			const size_type max_chunks = chunk_traits::max_size(chunk_allocator{ allocator_ref() });
			constexpr size_type numeric_max = std::numeric_limits<size_type>::max();

			if (max_chunks > numeric_max / node_type::count)
				return numeric_max;
			return max_chunks * node_type::count;
		}

		/// @brief Returns thenumber of elements in this sequence.
		SEQ_ALWAYS_INLINE auto size() const noexcept -> size_type { return d_data ? d_data->size : 0; }

		SEQ_ALWAYS_INLINE auto empty() const noexcept -> bool { return !d_data || d_data->size == 0; }

		/// @brief Returns the number of elements that the container has currently allocated space for.
		SEQ_ALWAYS_INLINE auto capacity() const noexcept -> size_t { return d_data ? d_data->total_slots : 0; }

		/// @brief Returns the back sequence value.
		SEQ_ALWAYS_INLINE auto back() const noexcept -> const T&
		{
			SEQ_ASSERT_DEBUG(!empty(), "empty container");
			return d_end.prev->back();
		}

		/// @brief Returns the back sequence value.
		SEQ_ALWAYS_INLINE auto back() noexcept -> T&
		{
			SEQ_ASSERT_DEBUG(!empty(), "empty container");
			return d_end.prev->back();
		}

		/// @brief Returns the front sequence value.
		SEQ_ALWAYS_INLINE auto front() const noexcept -> const T&
		{
			SEQ_ASSERT_DEBUG(!empty(), "empty container");
			return d_end.next->front();
		}

		/// @brief Returns the front sequence value.
		SEQ_ALWAYS_INLINE auto front() noexcept -> T&
		{
			SEQ_ASSERT_DEBUG(!empty(), "empty container");
			return d_end.next->front();
		}

		/// @brief Clears the contents.
		/// Erases all elements from the container. After this call, size() returns zero.
		/// Invalidates any references, pointers, or iterators referring to contained elements.
		/// Any past-the-end iterator remains valid.
		void clear() noexcept
		{
			d_data.reset(); // Data::~Data() destroys chunks
			d_end.reset();	// same address as before
		}

		/// @brief Constructs an element in-place at the end
		/// @return reference to the newly constructed object
		/// No iterators or references are invalidated.
		/// Strong exception guarantee.
		template<class... Args>
		SEQ_ALWAYS_INLINE auto emplace_back(Args&&... args) -> T&
		{
			make_data_if_null();

			if SEQ_UNLIKELY (d_data->size == d_data->max_size)
				throw std::length_error("sequence::emplace_back");

			node_type* last = d_end.prev;
			if SEQ_UNLIKELY (last->used & (1ULL << (count - 1ULL)))
				return emplace_back_new_chunk(last, std::forward<Args>(args)...);

			// Might throw which is fine
			construct_ptr(last->raw_slot(last->end), std::forward<Args>(args)...);
			last->used |= (1ULL << (last->end));
			if SEQ_UNLIKELY (last->used == full)
				remove_free_node(last);
			++d_data->size;
			return *(last->live_slot( last->end++));
		}

		/// @brief Constructs an element in-place at the end and returns an iterator pointing to this element.
		/// @return iterator to the inserted element
		///
		/// No iterators or references are invalidated.
		/// Strong exception guarantee.
		template<class... Args>
		SEQ_ALWAYS_INLINE auto emplace_back_iter(Args&&... args) -> iterator
		{
			emplace_back(std::forward<Args>(args)...);
			return iterator(d_end.prev, d_end.prev->end -1);
		}

		/// @brief Appends the given element value to the end of the sequence.
		/// @param value
		/// No iterators or references are invalidated.
		/// Strong exception guarantee.
		SEQ_ALWAYS_INLINE void push_back(const T& value) { emplace_back(value); }

		/// @brief Appends the given element value to the end of the sequence. value is moved into the new element.
		SEQ_ALWAYS_INLINE void push_back(T&& value) { emplace_back(std::move(value)); }

		/// @brief Constructs an element in-place at the beginning
		/// @return reference to the newly constructed object
		/// No iterators or references are invalidated.
		/// Strong exception guarantee.
		template<class... Args>
		SEQ_ALWAYS_INLINE auto emplace_front(Args&&... args) -> T&
		{
			make_data_if_null();

			if SEQ_UNLIKELY (d_data->size == d_data->max_size)
				throw std::length_error("sequence::emplace_front");

			node_type* first = d_end.next;
			if SEQ_UNLIKELY (first->used & 1)
				return emplace_front_new_chunk(first, std::forward<Args>(args)...);
			// Construct, might throw (which is ok)
			construct_ptr(first->raw_slot( first->start - 1), std::forward<Args>(args)...);
			first->used |= (1ULL << (--first->start));
			if (first->used == full)
				remove_free_node(first);
			++d_data->size;
			return first->front();
		}

		/// @brief Constructs an element in-place at the beginning and returns an iterator pointing to this element.
		/// @return iterator to the inserted element
		/// No iterators or references are invalidated.
		/// Strong exception guarantee.
		template<class... Args>
		SEQ_ALWAYS_INLINE auto emplace_front_iter(Args&&... args) -> iterator
		{
			emplace_front(std::forward<Args>(args)...);
			return iterator(d_end.next, d_end.next->start);
		}

		/// @brief Prepends the given element value to the beginning of the sequence.
		/// @param value
		/// No iterators or references are invalidated.
		/// Strong exception guarantee.
		SEQ_ALWAYS_INLINE void push_front(const T& value) { emplace_front(value); }

		/// @brief Prepends the given element value to the beginning of the sequence. value is moved into the new element.
		/// @param value
		/// No iterators or references are invalidated.
		/// Strong exception guarantee.
		SEQ_ALWAYS_INLINE void push_front(T&& value) { emplace_front(std::move(value)); }

		/// @brief Constructs an element in-place anywhere into the sequence.
		/// @return iterator to newly inserted element
		///
		/// The element could be inserted anywhere inside the sequence, including at the back or front.
		/// This function tries to recyclate free slots after calls to erase().
		/// You should favor this function if you don't care about the sequence ordering.
		/// No iterators or references are invalidated.
		/// Strong exception guarantee.
		template<class... Args>
		SEQ_ALWAYS_INLINE auto emplace(Args&&... args) -> iterator
		{
			make_data_if_null();

			if SEQ_UNLIKELY (d_data->size == d_data->max_size)
				throw std::length_error("sequence::emplace");

			if SEQ_UNLIKELY (d_end.next_free == d_data->end_node)
				// If no free slot, default to emplace_back
				return emplace_back_iter_noinline(std::forward<Args>(args)...);

			return emplace_anywhere(std::forward<Args>(args)...);
		}

		/// @brief Insert the given element into the sequence.
		/// @return iterator to newly inserted element
		/// The element could be inserted anywhere inside the sequence, including at the back or front.
		/// This function tries to recyclate free slots after calls to erase().
		/// You should favor this function if you don't care about the sequence ordering.
		/// No iterators or references are invalidated.
		/// Strong exception guarantee.
		SEQ_ALWAYS_INLINE auto insert(const T& value) -> iterator { return emplace(value); }

		/// @brief Insert the given element into the sequence. value is moved into the new element.
		/// @tparam ...Args
		/// @param ...args
		/// @return iterator to newly inserted element
		/// The element could be inserted anywhere inside the sequence, including at the back or front.
		/// This function tries to recyclate free slots after calls to erase().
		/// You should favor this function if you don't care about the sequence ordering.
		/// No iterators or references are invalidated.
		/// Strong exception guarantee.
		SEQ_ALWAYS_INLINE auto insert(T&& value) -> iterator { return emplace(std::move(value)); }

		/// @brief Resizes the container to contain count elements.
		/// @param new_size new size of the container
		/// @param value the value to initialize the new elements with
		/// If the current size is greater than count, the container is reduced to its first count elements.
		/// If the current size is less than count, additional copies of value are appended.
		/// Basic exception guarantee.
		template<class... U>
		void resize(size_type new_size, const U&... value)
		{
			if (new_size == size())
				// No-op
				return;

			if (new_size == 0) {
				clear();
				return;
			}

			if SEQ_UNLIKELY (new_size > max_size())
				throw std::length_error("sequence::resize");

			make_data_if_null();

			if (new_size > size()) {

				auto helper = detail::resize_helper<T>(std::forward<const U&>(value)...);

				reserve(new_size);
				size_type diff = new_size - size();
				node_type* last = d_data->end_node;

				// First, fill last chunk
				if (size()) {

					// Fill back last chunk
					last = d_end.prev;
					if (last->end != node_type::count) {
						while (last->end != node_type::count && diff) {
							// Might throw, fine
							helper.construct(last->raw_slot(last->end));
							last->used |= 1ULL << last->end;
							++last->end;
							++d_data->size;
							--diff;
						}
						if (last->used == full)
							remove_free_node(last);
					}

					if (diff == 0)
						return; // Finished!
				}

				// Add chunks
				size_type chunks = diff / node_type::count;
				size_type rem = diff % node_type::count;

				while (chunks--) {
					last = make_chunk(last, d_data->end_node);
					remove_free_node(last);

					try {
						// Fill last chunk, might throw
						while (last->end != node_type::count) {
							helper.construct(last->raw_slot( last->end));
							last->used |= 1ull << (uint64_t)last->end;
							++last->end;
						}
					}
					catch (...) {
						// In case of exception, remove full chunk
						Data::destroy_node_elements(last);
						remove_node(last);
						destroy_node(last);
						throw;
					}
					d_data->size += node_type::count;
				}
				// Add remaining
				if (rem) {
					// Might throw, ok
					last = make_chunk(last, d_data->end_node);

					try {
						// Fill last chunk, might throw
						while (last->end != static_cast<int>(rem)) {
							helper.construct(last->raw_slot( last->end));
							last->used |= 1ull << (uint64_t)last->end;
							++last->end;
						}
					}
					catch (...) {
						// In case of exception, remove full chunk
						Data::destroy_node_elements(last);
						remove_free_node(last);
						remove_node(last);
						destroy_node(last);
						throw;
					}
					d_data->size += rem;
				}
			}
			else {
				node_type* last = d_end.prev;
				difference_type diff = static_cast<difference_type>(size() - new_size);

				// empty last chunk
				while (last == d_end.prev && diff) {
					pop_back();
					--diff;
				}

				while (diff > static_cast<difference_type>(node_type::count)) {
					// destroy full chunks
					last = d_end.prev;
					unsigned size = last->size();
					diff -= size;
					d_data->size -= size;
					if (last->used != full)
						remove_free_node(last);
					Data::destroy_node_elements(last);
					remove_node(last);
					destroy_node(last);
				}

				// finish
				while (diff--)
					pop_back();
			}
		}

		/// @brief Resizes the container to contain count elements.
		/// @param new_size new size of the container
		/// @param value the value to initialize the new elements with
		/// If the current size is greater than count, the container is reduced to its last count elements.
		/// If the current size is less than count, additional copies of value are prepended.
		/// Basic exception guarantee.
		template<class... U>
		void resize_front(size_type new_size, const U&... value)
		{
			if (new_size == size())
				// No-op
				return;

			if (new_size == 0) {
				clear();
				return;
			}

			if SEQ_UNLIKELY (new_size > max_size())
				throw std::length_error("sequence::resize");

			make_data_if_null();

			if (new_size > size()) {
				auto helper = detail::resize_helper<T>(std::forward<const U&>(value)...);

				reserve(new_size);
				size_type diff = new_size - size();
				node_type* front = d_data->end_node;
				if (size()) {

					// Fill front first chunk
					front = d_end.next;
					if (front->start != 0) {
						while (front->start != 0 && diff) {
							// Might throw, ok
							helper.construct(front->raw_slot( front->start - 1));
							--front->start;
							front->used |= 1ULL << front->start;
							++d_data->size;
							--diff;
						}
						if (front->used == full)
							remove_free_node(front);
					}

					if (diff == 0)
						return; // Finished!
				}

				// Add chunks
				size_type chunks = diff / node_type::count;
				size_type rem = diff % node_type::count;

				while (chunks--) {
					// Might throw, ok
					front = make_chunk(d_data->end_node, front);
					remove_free_node(front);
					front->start = front->end = node_type::count;

					try {
						while (front->start != 0) {
							// Might throw, ok
							helper.construct(front->raw_slot( front->start - 1));
							--front->start;
							front->used |= 1ull << (uint64_t)front->start;
						}
					}
					catch (...) {
						// In case of exception, remove front chunk
						Data::destroy_node_elements(front);
						remove_node(front);
						destroy_node(front);
						throw;
					}
					d_data->size += node_type::count;
				}
				// Add remaining
				if (rem) {
					// might throw, ok
					front = make_chunk(d_data->end_node, front);
					front->start = front->end = node_type::count;
					size_type target = node_type::count - rem;
					try {
						while (front->start != static_cast<int>(target)) {
							helper.construct(front->raw_slot( front->start - 1));
							--front->start;
							front->used |= 1ull << (uint64_t)front->start;
						}
					}
					catch (...) {
						// In case of exception, remove front chunk
						Data::destroy_node_elements(front);
						remove_free_node(front);
						remove_node(front);
						destroy_node(front);
						throw;
					}
					d_data->size += rem;
				}
			}
			else {

				node_type* front = d_end.next;
				difference_type diff = static_cast<difference_type>(size() - new_size);

				// empty last chunk
				while (front == d_end.next && diff) {
					pop_front();
					--diff;
				}

				while (diff > static_cast<difference_type>(node_type::count)) {
					// destroy full chunks
					front = d_end.next;
					unsigned size = front->size();
					diff -= size;
					d_data->size -= size;
					if (front->used != full)
						remove_free_node(front);
					Data::destroy_node_elements(front);
					remove_node(front);
					destroy_node(front);
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
			make_data_if_null();
			assign_cat(first, last, typename std::iterator_traits<Iter>::iterator_category());
		}

		/// @brief Replaces the contents of the container.
		/// @param lst	initializer list to copy the values from
		/// Basic exception guarantee.
		void assign(const std::initializer_list<T>& lst) { assign(lst.begin(), lst.end()); }

		/// @brief Replaces the contents with new_size copies of value \a value
		/// @param new_size the new size of the container
		/// @param value the value to initialize elements of the container with
		/// Basic exception guarantee.
		void assign(size_type new_size, const T& value)
		{
			if SEQ_UNLIKELY (new_size > max_size())
				throw std::length_error("sequence::assign");

			make_data_if_null();
			assign_cat(cvalue_iterator<T>(0, value), cvalue_iterator<T>(new_size, value), std::random_access_iterator_tag());
		}

		/// @brief Pack the sequence to remove empty slots and release unused memory.
		/// All empty slots created by calls to #erase() are filled by moving each element toward the beginning.
		/// This function might deallocate unused buckets created by the shrinking operation, but
		/// Invalidates all references and iterators.
		/// Strong exception guarantee if T if nothrow move constructible, basic exception guarantee otherwise.
		void shrink_to_fit()
		{
			shrink_to_fit_internal();
		}

		/// @brief Increase the capacity of the vector to a value that's greater or equal to new_cap.
		/// @param new_cap new capacity of the sequence
		/// If new_cap is greater than the current capacity(), new storage is allocated, otherwise the function does nothing.
		/// reserve() does not change the size of the sequence.
		/// Note that reserve() only works with \a OptimizeForSpeed flag.
		/// Basic exception guarantee.
		void reserve(size_t new_cap)
		{
			if SEQ_UNLIKELY (new_cap > max_size())
				throw std::length_error("sequence::reserve");

			make_data_if_null();

			if (new_cap > size()) {
				while (d_data->total_slots < new_cap)
					d_data->add_free(d_data->allocate_node());
			}
		}

		/// @brief Removes the first element of the container.
		/// Calling pop_front on an empty container results in undefined behavior.
		/// Iterators and references to the first element are invalidated.
		/// Other iterators and references remain valid.
		SEQ_ALWAYS_INLINE void pop_front() noexcept
		{
			SEQ_ASSERT_DEBUG(size() > 0, "pop_front() on an empty container");
			T* ptr = front_ptr();
			node_type* node = d_end.next;

			if SEQ_UNLIKELY (node->used == full)
				add_free_node(node);

			node->used &= ~(1ULL << static_cast<std::uint64_t>(node->start));
			destroy_ptr(ptr);
			if SEQ_UNLIKELY (node->used == 0ULL)
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
		SEQ_ALWAYS_INLINE void pop_back() noexcept
		{
			SEQ_ASSERT_DEBUG(size() > 0, "pop_back() on an empty container");
			T* ptr = back_ptr();
			node_type* node = d_end.prev;

			if SEQ_UNLIKELY (node->used == full)
				add_free_node(node);

			node->used &= ~(1ULL << static_cast<std::uint64_t>(node->end-1));
			destroy_ptr(ptr);
			if SEQ_UNLIKELY (node->used == 0ULL)
				pop_back_remove_chunk(node);
			else {
				if (!(node->used & (1ULL << static_cast<std::uint64_t>(--node->end - 1))))
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
		SEQ_ALWAYS_INLINE auto iterator_at(size_type pos) noexcept -> iterator { return d_data ? d_data->iterator_at(pos) : end(); }

		/// @brief Returns a const_iterator to given position.
		/// @param pos position in the sequence
		/// While sequence is not a random access container, operations on iterators are still faster
		/// than for a conventional std::list.
		/// This function is faster than using begin()+pos as it might start from the end to reach
		/// the required position.
		SEQ_ALWAYS_INLINE auto iterator_at(size_type pos) const noexcept -> const_iterator { return d_data ? d_data->iterator_at(pos) : end(); }

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

			T* ptr = it.node->live_slot(it.pos);

			iterator res(it.node, it.pos);
			++res;

			destroy_ptr(ptr);

			if SEQ_UNLIKELY (it.node->used == full)
				add_free_node(it.node);

			it.node->used &= ~(1ULL << (it.pos));

			if SEQ_LIKELY (it.node->used != 0) {
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
		/// Iterators and references to the erased elements are invalidated.
		/// Iterators and references to other elements in the sequence remain valid.
		auto erase(const_iterator first, const_iterator last) noexcept -> iterator
		{
			if (first == last)
				return iterator(last.node,last.pos);
			if (first == begin() && last == end()) {
				clear();
				return end();
			}

			iterator res(last.node, last.pos);

			node_type* node = first.node;
			bool was_full = first.node->used == node_type::full;

			while (first != last) {
				destroy_ptr(&(*first));
				first.node->used &= ~(1ULL << static_cast<std::uint64_t>(first.pos));
				++first;
				--d_data->size;
				if (node != first.node) {
					// We just changed the node
					if (node->used == 0ULL) {
						if (!was_full)
							remove_free_node(node);
						remove_node(node);
						destroy_node(node);
					}
					else {
						node->start = static_cast<int>(bit_scan_forward_64(node->used));
						node->end = static_cast<int>(bit_scan_reverse_64(node->used)) + 1;
						if (was_full && node->used != full)
							add_free_node(node);
					}
					node = first.node;
					was_full = first.node->used == node_type::full;
				}
			}
			if (node != d_data->end_node) {
				node->start = static_cast<int>(bit_scan_forward_64(node->used));
				node->end = static_cast<int>(bit_scan_reverse_64(node->used)) + 1;
				if (was_full && node->used != full)
					add_free_node(node);
			}
			return res;
		}

		/// @brief Sort the sequence inplace and in a stable way.
		/// The sequence is sorted using the std::less<value_type> comparator.
		/// This invalidates all iterators and references, and requires that T is default constructible.
		void sort() { sort(std::less<T>()); }

		template<class Less>
		void sort(Less less)
		{
			std::vector<T> buf(size() / 2);
			sort(less, buffer<T*>{ buf.data(), buf.size() });
		}

		/// @brief Sort the sequence using given comparator.
		/// sort() relies on seq::net_sort() and is stable.
		/// This invalidates all iterators and references.
		template<class Less, class Buffer>
		void sort(Less less, Buffer buf)
		{
			if (empty())
				return;

			using iter = detail::sequence_ra_iterator<sequence<T, Allocator, Aligned>>;
			using data = typename iter::Data;

			data d;
			d.size = size();
			
			shrink_to_fit_internal(&d.chunks);
			d.end = d_data->end_node;

			iter begin(&d, d_end.next);

			std::vector<iter> iters(d.chunks.size() + 1);
			iters[0] = begin;
			size_t abs_pos = 0;
			for (size_t i = 0; i < d.chunks.size(); ++i) {
				auto csize = d.chunks[i]->end - d.chunks[i]->start;
				abs_pos += csize;
				iters[i + 1] = begin + abs_pos;
				net_sort_size(d.chunks[i]->live_slot(d.chunks[i]->start), csize, less, buf);
			}
			if (iters.size() > 2 && iters.back() == iters[iters.size() - 2])
				iters.pop_back();
			inplace_merge(iters.data(), iters.size(), less, buf);
		}

		/// @brief Returns an iterator to the first element of the sequence.
		SEQ_ALWAYS_INLINE auto begin() noexcept -> iterator { return iterator(d_end.next); }
		/// @brief Returns an iterator to the element following the last element of the sequence.
		SEQ_ALWAYS_INLINE auto end() noexcept -> iterator { return iterator(&d_end, 0); }
		/// @brief Returns an iterator to the first element of the sequence.
		SEQ_ALWAYS_INLINE auto begin() const noexcept -> const_iterator { return const_iterator(d_end.next); }
		/// @brief Returns an iterator to the element following the last element of the sequence.
		SEQ_ALWAYS_INLINE auto end() const noexcept -> const_iterator { return const_iterator(&d_end, 0); }
		/// @brief Returns an iterator to the first element of the sequence.
		SEQ_ALWAYS_INLINE auto cbegin() const noexcept -> const_iterator { return const_iterator(d_end.next); }
		/// @brief Returns an iterator to the element following the last element of the sequence.
		SEQ_ALWAYS_INLINE auto cend() const noexcept -> const_iterator { return const_iterator(&d_end); }
		/// @brief Returns a reverse iterator to the first element of the reversed list.
		SEQ_ALWAYS_INLINE auto rbegin() noexcept -> reverse_iterator { return reverse_iterator(end()); }
		/// @brief Returns a reverse iterator to the first element of the reversed list.
		SEQ_ALWAYS_INLINE auto rbegin() const noexcept -> const_reverse_iterator { return const_reverse_iterator(end()); }
		/// @brief Returns a reverse iterator to the element following the last element of the reversed list.
		SEQ_ALWAYS_INLINE auto rend() noexcept -> reverse_iterator { return reverse_iterator(begin()); }
		/// @brief Returns a reverse iterator to the element following the last element of the reversed list.
		SEQ_ALWAYS_INLINE auto rend() const noexcept -> const_reverse_iterator { return const_reverse_iterator(begin()); }
		/// @brief Returns a reverse iterator to the first element of the reversed list.
		SEQ_ALWAYS_INLINE auto crbegin() const noexcept -> const_reverse_iterator { return rbegin(); }
		/// @brief Returns a reverse iterator to the element following the last element of the reversed list.
		SEQ_ALWAYS_INLINE auto crend() const noexcept -> const_reverse_iterator { return rend(); }
	};


} // end namespace seq

#endif
