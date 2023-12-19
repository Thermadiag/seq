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

#ifndef SEQ_TIERED_VECTOR_HPP
#define SEQ_TIERED_VECTOR_HPP




#include "devector.hpp"
#include "pdqsort.hpp"
#include "utils.hpp"
#include "tiny_string.hpp"

#include <algorithm>
#include <string>
#include <cmath>

#ifndef SEQ_MIN_BUCKET_SIZE
/// @brief Minimum bucket size for given type
#define SEQ_MIN_BUCKET_SIZE(T) \
	(sizeof(T) <= 4 ? 64 : (sizeof(T) <= 8 ? 32 : (sizeof(T) <= 16 ? 16 : (sizeof(T) <= 64 ? 4 : 2))))

#endif

/// @brief Maximum bucket size (default to 1<<16)
#define SEQ_MAX_BUCKET_SIZE (1U<<16U) 


namespace seq
{

	namespace detail
	{
		




		using cbuffer_pos = int; //index type within circular buffer, must be signed

		// Deque const iterator
		template<class BucketMgr >
		struct deque_const_iterator
		{
			using bucket_manager = BucketMgr;
			using bucket_type = typename bucket_manager::StoreBucketType;
			using value_type = typename BucketMgr::value_type;
			using iterator_category = std::random_access_iterator_tag;
			using size_type = typename BucketMgr::size_type;
			using difference_type = typename BucketMgr::difference_type;
			using pointer = typename BucketMgr::const_pointer;
			using reference = const value_type&;

			bucket_manager* mgr;
			bucket_type* bucket;
			difference_type pos ;
			cbuffer_pos ptr ;
			cbuffer_pos begin_ptr ;
			cbuffer_pos first_stop ; 

			SEQ_ALWAYS_INLINE deque_const_iterator() noexcept {}
			SEQ_ALWAYS_INLINE deque_const_iterator(const bucket_manager* d) noexcept
				: mgr(const_cast<bucket_manager*>(d)),
				bucket(d ? const_cast<bucket_type*>(d->d_buckets.data() + d->d_buckets.size()) : nullptr),
				pos(d ? static_cast<difference_type>(d->size()) : 0),
				ptr(0),
				begin_ptr(0),
				first_stop(0)
			{} //end()
			SEQ_ALWAYS_INLINE deque_const_iterator(const bucket_manager* d, size_type /*unused*/) noexcept
				: mgr(const_cast<bucket_manager*>(d)),
				bucket(d ? const_cast<bucket_type*>(d->d_buckets.data()) : nullptr),
				pos(0),
				ptr(d ? (d->d_buckets.empty() ? 0 : d->d_buckets.front()->begin_index()) : 0)
			{
				begin_ptr = ptr;
				if(d) first_stop = d->d_buckets.empty() ? 0 : (*bucket)->first_stop();
			} //begin

			SEQ_ALWAYS_INLINE deque_const_iterator(const bucket_manager* d, size_type p, size_type /*unused*/) noexcept
				: mgr(const_cast<bucket_manager*>(d))
			{
				setPos(p);
			} //any pos

			SEQ_ALWAYS_INLINE auto absolutePos() const noexcept -> size_type {
				return static_cast<size_t>(pos);
			}

			SEQ_ALWAYS_INLINE void setPos(size_t new_pos) noexcept 
			{
				SEQ_ASSERT_DEBUG( new_pos <= static_cast<size_t>(mgr->d_size), "invalid iterator position");

				size_type front_size = static_cast<size_t>(mgr->d_buckets.front()->size);
				size_type bindex = (new_pos + (static_cast<size_t>(mgr->d_bucket_size) - front_size)) >> static_cast<size_t>(mgr->d_bucket_size_bits);
				size_type index_in_bucket = (new_pos - (new_pos < (front_size) ? 0 : front_size)) & static_cast<size_t>(mgr->d_bucket_size1);

				bucket = mgr->d_buckets.data() + bindex;

				if (SEQ_UNLIKELY(bindex == mgr->d_buckets.size())) {
					// end iterator
					ptr = begin_ptr = first_stop = 0;
				}
				else {
						
					ptr = static_cast<cbuffer_pos>(&(*bucket)->at(static_cast<cbuffer_pos>(index_in_bucket)) - (*bucket)->buffer());
					begin_ptr = (*bucket)->begin_index();
					first_stop = (ptr < begin_ptr) ? (*bucket)->second_stop() : (*bucket)->first_stop();
				}
				pos = static_cast<difference_type>(new_pos);
			}

			SEQ_ALWAYS_INLINE void offset(difference_type diff) noexcept
			{
				setPos(static_cast<size_t>(pos + diff));
			}
			SEQ_ALWAYS_INLINE void go_next() noexcept
			{
				const cbuffer_pos sstop = (*bucket)->second_stop();
				if (first_stop == sstop)
				{
					bucket++;
					if (pos != static_cast<difference_type>(mgr->size())) {
						begin_ptr = ptr = (*bucket)->begin_index();
						first_stop = (*bucket)->first_stop();
					}
					else
						ptr = begin_ptr = first_stop = 0;
				}
				else {
					first_stop = sstop;
					ptr = 0;
				}
			}
			SEQ_ALWAYS_INLINE void go_prev() noexcept
			{
				if (ptr == begin_ptr - 1)
				{
					bucket--;
					ptr = static_cast<cbuffer_pos>((*bucket)->last_ptr() - (*bucket)->buffer());
					begin_ptr = (*bucket)->begin_index();
					first_stop = (*bucket)->second_stop();
				}
				else if (ptr == -1) {
					first_stop = (*bucket)->first_stop();
					ptr = (*bucket)->max_size1;
				}
			}
			
			SEQ_ALWAYS_INLINE auto operator++() noexcept -> deque_const_iterator& 
			{
				SEQ_ASSERT_DEBUG(pos < static_cast<difference_type>(mgr->d_size), "increment iterator already at the end");
				++ptr;
				++pos;
				if (SEQ_UNLIKELY(ptr == first_stop )) {
					go_next();
				}
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator++(int) noexcept -> deque_const_iterator {
				deque_const_iterator _Tmp = *this;
				++(*this);
				return _Tmp;
			}

			SEQ_ALWAYS_INLINE auto operator--() noexcept -> deque_const_iterator& {
				SEQ_ASSERT_DEBUG(pos > 0, "increment iterator already at the end");
				--ptr;
				--pos;
				if(SEQ_UNLIKELY(ptr < begin_ptr )) {
					go_prev();
				}
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator--(int) noexcept -> deque_const_iterator {
				deque_const_iterator _Tmp = *this;
				--(*this);
				return _Tmp;
			}
			SEQ_ALWAYS_INLINE auto operator*() const noexcept -> reference {
				SEQ_ASSERT_DEBUG(pos >= 0 && pos < static_cast<difference_type>(mgr->d_size), "dereference invalid iterator");
				return (*bucket)->buffer()[ptr];
			}
			SEQ_ALWAYS_INLINE auto operator->() const noexcept -> pointer {
				return std::pointer_traits<pointer>::pointer_to(**this);
			}
			SEQ_ALWAYS_INLINE auto operator+=(difference_type diff) noexcept -> deque_const_iterator& {
				offset(diff);
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator-=(difference_type diff) noexcept -> deque_const_iterator& {
				offset(-diff);
				return *this;
			}
		};

		// Deque iterator class
		template<class BucketMgr >
		struct deque_iterator : public deque_const_iterator< BucketMgr>
		{
			using base_type = deque_const_iterator< BucketMgr>;
			using bucket_manager = BucketMgr;
			using bucket_type = typename bucket_manager::BucketType;
			using value_type = typename BucketMgr::value_type;
			using size_type = typename BucketMgr::size_type;
			using iterator_category = std::random_access_iterator_tag;
			using difference_type = typename BucketMgr::difference_type;
			using pointer = typename BucketMgr::pointer;
			using const_pointer = typename BucketMgr::const_pointer;
			using reference = value_type&;
			using const_reference = const value_type&;

			deque_iterator() noexcept {}
			deque_iterator(const bucket_manager* d) noexcept :base_type(d) {}
			deque_iterator(const bucket_manager* d, size_type b) noexcept :base_type(d, b) {}
			deque_iterator(const deque_const_iterator<BucketMgr>& other) noexcept : base_type(other) {}
			deque_iterator(const bucket_manager* d, size_type p, size_type unused) noexcept : base_type(d,p,unused){} //any pos
			
			SEQ_ALWAYS_INLINE auto operator*()  noexcept -> reference {
				return const_cast<reference>(base_type::operator*());
			}
			SEQ_ALWAYS_INLINE auto operator->()  noexcept -> pointer {
				return std::pointer_traits<pointer>::pointer_to(**this);
			}
			SEQ_ALWAYS_INLINE auto operator*() const noexcept -> const_reference {
				return base_type::operator*();
			}
			SEQ_ALWAYS_INLINE auto operator->() const noexcept -> const_pointer {
				return std::pointer_traits<const_pointer>::pointer_to(**this);
			}
			SEQ_ALWAYS_INLINE auto operator++() noexcept -> deque_iterator& {
				base_type::operator++();
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator++(int) noexcept -> deque_iterator {
				deque_iterator _Tmp = *this;
				base_type::operator++();
				return _Tmp;
			}
			SEQ_ALWAYS_INLINE auto operator--() noexcept -> deque_iterator& {
				base_type::operator--();
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator--(int) noexcept -> deque_iterator {
				deque_iterator _Tmp = *this;
				base_type::operator--();
				return _Tmp;
			}
			SEQ_ALWAYS_INLINE auto operator+=(difference_type diff)noexcept -> deque_iterator& {
				base_type::operator+=(diff);
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator-=(difference_type diff)noexcept -> deque_iterator& {
				base_type::operator-=(diff);
				return *this;
			}
		};

		template < class BucketMgr>
		SEQ_ALWAYS_INLINE auto operator+(const deque_const_iterator< BucketMgr>& it, typename deque_const_iterator< BucketMgr>::difference_type diff)noexcept -> deque_const_iterator< BucketMgr> {
			deque_const_iterator< BucketMgr> res = it;
			res += diff;
			return res;
		}
		template < class BucketMgr>
		SEQ_ALWAYS_INLINE auto operator+(const deque_iterator< BucketMgr>& it, typename deque_iterator< BucketMgr>::difference_type diff)noexcept -> deque_iterator< BucketMgr> {
			deque_iterator< BucketMgr> res = it;
			res += diff;
			return res;
		}
		template < class BucketMgr>
		SEQ_ALWAYS_INLINE auto operator-(const deque_const_iterator< BucketMgr>& it, typename deque_const_iterator< BucketMgr>::difference_type diff)noexcept -> deque_const_iterator< BucketMgr> {
			deque_const_iterator< BucketMgr> res = it;
			res -= diff;
			return res;
		}
		template < class BucketMgr>
		SEQ_ALWAYS_INLINE auto operator-(const deque_iterator< BucketMgr>& it, typename deque_iterator< BucketMgr>::difference_type diff)noexcept -> deque_iterator< BucketMgr> {
			deque_iterator< BucketMgr> res = it;
			res -= diff;
			return res;
		}
		template < class BucketMgr>
		SEQ_ALWAYS_INLINE auto operator-(const deque_const_iterator< BucketMgr>& it1, const deque_const_iterator< BucketMgr>& it2)noexcept -> typename deque_const_iterator< BucketMgr>::difference_type {
			return it1.pos - it2.pos;
		}
		template < class BucketMgr>
		SEQ_ALWAYS_INLINE auto operator==(const deque_const_iterator< BucketMgr>& it1, const deque_const_iterator< BucketMgr>& it2)noexcept -> bool {
			return it1.pos == it2.pos;
		}
		template < class BucketMgr>
		SEQ_ALWAYS_INLINE auto operator!=(const deque_const_iterator< BucketMgr>& it1, const deque_const_iterator< BucketMgr>& it2)noexcept -> bool {
			return  it1.pos != it2.pos;
		}
		template < class BucketMgr>
		SEQ_ALWAYS_INLINE auto operator<(const deque_const_iterator< BucketMgr>& it1, const deque_const_iterator< BucketMgr>& it2)noexcept -> bool {
			return (it1.pos) < (it2.pos);
		}
		template < class BucketMgr>
		SEQ_ALWAYS_INLINE auto operator>(const deque_const_iterator< BucketMgr>& it1, const deque_const_iterator< BucketMgr>& it2) noexcept -> bool {
			return (it1.pos) > (it2.pos);
		}
		template < class BucketMgr>
		SEQ_ALWAYS_INLINE auto operator<=(const deque_const_iterator< BucketMgr>& it1, const deque_const_iterator< BucketMgr>& it2)noexcept -> bool {
			return it1.pos <= it2.pos;
		}
		template < class BucketMgr>
		SEQ_ALWAYS_INLINE auto operator>=(const deque_const_iterator< BucketMgr>& it1, const deque_const_iterator< BucketMgr>& it2)noexcept -> bool {
			return it1.pos >= it2.pos;
		}




		template< class BucketMgr>
		struct tvector_ra_iterator
		{

			using iterator_category = std::random_access_iterator_tag;
			using value_type = typename BucketMgr::value_type;
			using difference_type = typename BucketMgr::difference_type;
			using pointer = typename BucketMgr::pointer;
			using reference = value_type&;
			using pos_type = difference_type;
			using chunk_type = typename BucketMgr::StoreBucketType;

			BucketMgr* data;
			chunk_type* node;
			int pos;

			tvector_ra_iterator() noexcept {}
			tvector_ra_iterator(const BucketMgr* d) noexcept //begin
				:data(const_cast<BucketMgr*>(d)), node(const_cast<chunk_type*>(d->d_buckets.data())), pos(0){}
			tvector_ra_iterator(const BucketMgr* d, size_t) noexcept //end
				:data(const_cast<BucketMgr*>(d)){
				if (data->d_buckets.back().bucket->size == data->d_bucket_size) {
					node = data->d_buckets.data() + data->d_buckets.size();
					pos = 0;
				}
				else {
					node = data->d_buckets.data() + data->d_buckets.size() - 1;
					pos = data->d_buckets.back().bucket->size;
				}
			}

			auto computeAbsolutePos() const noexcept -> difference_type {
				return ((node - data->d_buckets.data()) << data->d_bucket_size_bits) + pos;
			}
			void add(difference_type diff) noexcept{
				difference_type abs = computeAbsolutePos();
				abs += diff;
				this->node = data->d_buckets.data() + (abs >> data->d_bucket_size_bits);
				this->pos = static_cast<int>(abs & data->d_bucket_size1);
			}
			auto operator*() noexcept  -> reference {
				return node->bucket->buffer()[pos];
			}
			auto operator->() noexcept -> pointer {
				return std::pointer_traits<pointer>::pointer_to(**this);
			}
			auto operator++() noexcept -> tvector_ra_iterator& {
				++pos;
				if (SEQ_UNLIKELY(pos >= data->d_bucket_size)) {
					++node;
					pos = 0;
				}
				return *this;
			}
			auto operator++(int) noexcept -> tvector_ra_iterator {
				tvector_ra_iterator _Tmp = *this;
				++(*this);
				return _Tmp;
			}
			auto operator--() noexcept -> tvector_ra_iterator& {
				--pos;
				if (SEQ_UNLIKELY(pos < 0)) {
					--node;
					pos = data->d_bucket_size1;
				}
				return*this;
			}
			auto operator--(int) noexcept -> tvector_ra_iterator {
				tvector_ra_iterator _Tmp = *this;
				--(*this);
				return _Tmp;
			}
			auto operator+=(difference_type diff) noexcept -> tvector_ra_iterator& {
				add( diff);
				return *this;
			}
			auto operator-=(difference_type diff) noexcept -> tvector_ra_iterator& {
				add(-diff);
				return *this;
			}
			bool operator==(const tvector_ra_iterator& other) const noexcept { return node == other.node && pos == other.pos; }
			bool operator!=(const tvector_ra_iterator& other) const noexcept { return node != other.node || pos != other.pos; }
			bool operator<(const tvector_ra_iterator& other) const noexcept { return node < other.node || (node == other.node && pos < other.pos); }
			bool operator>(const tvector_ra_iterator& other) const noexcept { return node > other.node || (node == other.node && pos > other.pos); }
			bool operator<=(const tvector_ra_iterator& other) const noexcept { return !(*this > other); }
			bool operator>=(const tvector_ra_iterator& other) const noexcept { return !(other > *this); }
			auto operator+(difference_type diff) const noexcept -> tvector_ra_iterator {
				tvector_ra_iterator tmp = *this;
				tmp += diff;
				return tmp;
			}
			auto operator-(difference_type diff) const noexcept -> tvector_ra_iterator {
				tvector_ra_iterator tmp = *this;
				tmp -= diff;
				return tmp;
			}
			auto operator-(const tvector_ra_iterator& other) const noexcept -> difference_type {
				return ((node - other.node) << data->d_bucket_size_bits) + (pos - other.pos);
			}
		};

	} //end detail






	namespace detail
	{
		// Base class for circular buffer
		template<class T, class Allocator>
		struct alignas(alignof(T) > alignof(cbuffer_pos) ? alignof(T) : alignof(cbuffer_pos)) BaseCircularBuffer
		{
			cbuffer_pos	size;		//buffer size
			cbuffer_pos	max_size1;	//buffer max size -1
			cbuffer_pos max_size_;	//buffer max size
			cbuffer_pos	begin;		//begin index of data		

			BaseCircularBuffer(cbuffer_pos max_size) noexcept
				:size(0), max_size1(max_size - 1), max_size_(max_size), begin(0) {}
		};


		// Circular buffer class used internally by tiered_vector
		// Actuall data are located in the same memory block and starts at start_data_T
		template<class T, class Allocator>
		struct CircularBuffer : BaseCircularBuffer<T, Allocator>
		{
			static constexpr size_t size_BCB = sizeof(BaseCircularBuffer<T, Allocator>);
			// Start position for actual data in bytes
			static constexpr size_t start_data_T = (size_BCB > sizeof(T)) ?
				(size_BCB / sizeof(T) + ((size_BCB % sizeof(T)) != 0u ? 1 : 0)) :
				1;
			static constexpr size_t start_data = start_data_T * sizeof(T);
			static constexpr bool relocatable = is_relocatable<T>::value && (sizeof(T) >= sizeof(size_t));

			using BaseCircularBuffer<T, Allocator>::size;
			using BaseCircularBuffer<T, Allocator>::max_size1;
			using BaseCircularBuffer<T, Allocator>::max_size_;
			using BaseCircularBuffer<T, Allocator>::begin;

			// Initialize from a maximum size
			CircularBuffer(cbuffer_pos max_size) noexcept
				:BaseCircularBuffer<T, Allocator>(max_size) {
				//initialize empty buffer
			}
			// Initialize from a maximum size and a default value.
			// This will set the buffer size to its maximum.
			CircularBuffer(cbuffer_pos max_size, const T& val, Allocator&  /*unused*/) 
				:BaseCircularBuffer<T, Allocator>(max_size) {
				size = max_size;
				//initialize full buffer
				cbuffer_pos i = 0;
				try {
					for (; i < max_size; ++i)
						construct_ptr(buffer() + i, val);
				}
				catch (...) {
					for (cbuffer_pos j = 0; j < i; ++j)
						destroy_ptr(buffer() + j);
					size = 0;
					throw;
				}
			}

			// Destroy all values within buffer
			void destroy(Allocator&  /*unused*/) noexcept {
				if (!std::is_trivially_destructible<T>::value) {
					for (cbuffer_pos i = 0; i < size; ++i)
						destroy_ptr(&(*this)[i]);
				}
			}

			//Disable copy
			CircularBuffer(const CircularBuffer& other) = delete;
			auto operator=(const CircularBuffer& other) -> CircularBuffer& = delete;

			//Raw buffer access
			SEQ_ALWAYS_INLINE auto buffer() noexcept -> T* { return reinterpret_cast<T*>(reinterpret_cast<char*>(this) + start_data); }
			SEQ_ALWAYS_INLINE auto buffer() const noexcept -> const T* { return reinterpret_cast<const T*>(reinterpret_cast<const char*>(this) + start_data); }

			//Pointer on the first data
			SEQ_ALWAYS_INLINE auto begin_ptr() noexcept -> T* { return buffer() + ((begin)&max_size1); }
			SEQ_ALWAYS_INLINE auto begin_ptr() const noexcept -> const T* { return buffer() + ((begin)&max_size1); }
			//Pointer on the last data
			SEQ_ALWAYS_INLINE auto last_ptr() noexcept -> T* { return (buffer() + ((begin + size - 1) & max_size1)); }
			SEQ_ALWAYS_INLINE auto last_ptr() const noexcept -> const T* { return (buffer() + ((begin + size - 1) & max_size1)); }
			//Index of the first data
			SEQ_ALWAYS_INLINE auto begin_index() const noexcept -> cbuffer_pos { return  (begin)&max_size1; }
			// Index of the first stop (either at size or max size)
			SEQ_ALWAYS_INLINE auto first_stop() const noexcept -> cbuffer_pos {
				cbuffer_pos p = begin_index();
				return (p + size) > max_size() ? (max_size()) : (p + size);
			}
			// Index of the second stop (either at size or max size)
			SEQ_ALWAYS_INLINE auto second_stop() const noexcept -> cbuffer_pos {
				cbuffer_pos p = begin_index();
				return (p + size) > max_size() ? (((p + size) & max_size1)) : (p + size);
			}
			//Buffer maximum size
			SEQ_ALWAYS_INLINE auto max_size() const noexcept -> cbuffer_pos {
				return max_size_;
			}
			//Check if full
			auto isFull() const noexcept -> bool { return size == max_size(); }
			//Element access.
			SEQ_ALWAYS_INLINE auto operator[](cbuffer_pos index) noexcept -> T& {
				//SEQ_ASSERT_DEBUG(index >= 0, "Invalid index");
				SEQ_ASSERT_DEBUG(!(index >= max_size() && begin == 0), "Invalid index");
				return  buffer()[(begin == 0) ? index : ((begin + index) & (max_size1))];
			}
			SEQ_ALWAYS_INLINE auto operator[](cbuffer_pos index) const noexcept -> const T& {
				//SEQ_ASSERT_DEBUG(index >= 0, "Invalid index");
				SEQ_ASSERT_DEBUG(!(index >= max_size() && begin == 0), "Invalid index");
				return  buffer()[(begin == 0) ? index : ((begin + index) & (max_size1))];
			}
			SEQ_ALWAYS_INLINE auto at(cbuffer_pos index) noexcept -> T& {
				SEQ_ASSERT_DEBUG(index >= 0, "Invalid index");
				return buffer()[((begin + index) & (max_size1))];
			}
			SEQ_ALWAYS_INLINE auto at(cbuffer_pos index) const noexcept -> const T& {
				SEQ_ASSERT_DEBUG(index >= 0, "Invalid index");
				return buffer()[((begin + index) & (max_size1))];
			}

			// Front/back access
			SEQ_ALWAYS_INLINE auto front() noexcept -> T& { return buffer()[begin]; }
			SEQ_ALWAYS_INLINE auto front() const noexcept -> const T& { return buffer()[begin]; }
			SEQ_ALWAYS_INLINE auto back() noexcept -> T& { return (*this)[size - 1]; }
			SEQ_ALWAYS_INLINE auto back() const noexcept -> const T& { return (*this)[size - 1]; }

			// Initialize a front buffer
			SEQ_ALWAYS_INLINE void init_front() noexcept {
				//init members for front buffer
				begin = size = 0;
			}

			// Resize buffer
			void resize(cbuffer_pos s, Allocator&  /*unused*/)  {
				if (s < size) {
					if (!std::is_trivially_destructible<T>::value) {
						for (cbuffer_pos i = s; i < size; ++i)
							destroy_ptr(&(*this)[i]);
					}
				}
				else if (s > size) {
					cbuffer_pos i = size;
					// Make sure to destroy constructed values in case of exception
					try {
						for (; i < s; ++i)
							construct_ptr(&(*this)[i]);
					}
					catch (...) {
						for (cbuffer_pos j = size; j < i; ++j)
							destroy_ptr(&(*this)[j]);
						throw;
					}
				}
				size = s;
			}
			void resize(cbuffer_pos s, const T& value, Allocator&  /*unused*/)  {
				if (s < size) {
					if (!std::is_trivially_destructible<T>::value) {
						for (cbuffer_pos i = s; i < size; ++i)
							destroy_ptr(&at(i));
					}
				}
				else if (s > size) {
					cbuffer_pos i = size;
					// Make sure to destroy constructed values in case of exception
					try {
						for (; i < s; ++i)
							construct_ptr(&at(i), value);
					}
					catch (...) {
						for (cbuffer_pos j = size; j < i; ++j)
							destroy_ptr(&at(j));
						throw;
					}
				}
				size = s;
			}


			// Extend front buffer
			void grow_front(cbuffer_pos s) noexcept {
				begin += (s - size);
				size = s;
				if (begin < 0) begin += max_size();
				else begin &= max_size1;
			}

			template <class... Args>
			auto emplace_back(Allocator&  /*unused*/, Args&&... args) -> T* {
				//only works for non full array
				T* ptr = begin ? &at(size) : (buffer() + size);
				// Might throw, which is fine
				construct_ptr(ptr, std::forward<Args>(args)...);
				++size;
				return ptr;
			}
			SEQ_ALWAYS_INLINE auto push_back(Allocator& allocator, const T& value) -> T* {
				//only works for non full array
				return emplace_back(allocator, value);
			}
			SEQ_ALWAYS_INLINE auto push_back(Allocator& allocator, T&& value) -> T* {
				//only works for non full array
				return emplace_back(allocator, std::move(value));
			}

			template <class... Args>
			auto emplace_front(Allocator&  /*unused*/, Args&&... args) -> T* {
				//only works for non full array
				if (--begin < 0) begin = max_size1;
				try {
					construct_ptr(buffer() + begin, std::forward<Args>(args)...);
				}
				catch (...) {
					begin = (begin + 1) & max_size1;
					throw;
				}
				++size;
				return buffer() + begin;
			}
			SEQ_ALWAYS_INLINE auto push_front(Allocator& allocator, const T& value) -> T*  {
				//only works for non full array
				return emplace_front(allocator, value);
			}
			SEQ_ALWAYS_INLINE auto push_front(Allocator& allocator, T&& value) -> T*  {
				//only works for non full array
				return emplace_front(allocator, std::move(value));
			}

			// Pushing front while poping back value for relocatable types
			SEQ_ALWAYS_INLINE void push_front_pop_back_use_relocatable(T& inout) noexcept {
				typename std::aligned_storage<sizeof(T), alignof(T)>::type tmp;
				memcpy(static_cast<void*>(&tmp), static_cast<void*>( &back()), sizeof(T));
				if (--begin < 0) begin = max_size1;
				memcpy(static_cast<void*>(buffer() + begin), static_cast<void*>( &inout), sizeof(T));
				memcpy(static_cast<void*>(&inout), static_cast<void*>(&tmp), sizeof(T));
			}
			// Pushing front while poping back value for relocatable types, except arithmetic ones
			SEQ_ALWAYS_INLINE void push_front_pop_back_relocatable(T& inout)noexcept(std::is_nothrow_move_assignable<T>::value) {
				if (std::is_arithmetic<T>::value)
					inout = std::move(push_front_pop_back(std::move(inout)));
				else
					push_front_pop_back_use_relocatable(inout);
			}
			// Pushing front while poping back value
			// Might throw, but leave the buffer in a valid state
			SEQ_ALWAYS_INLINE auto push_front_pop_back(T&& value) noexcept(std::is_nothrow_move_assignable<T>::value) -> T {
				// Only works for filled array
				T res = std::move(back());
				if (--begin < 0) begin = max_size1;
				buffer()[begin] = std::move(value);
				return (res);
			}

			// Pushing front while poping back for relocatable types
			SEQ_ALWAYS_INLINE void push_back_pop_front_use_relocatable(T& inout) noexcept {
				typename std::aligned_storage<sizeof(T), alignof(T)>::type tmp;
				memcpy(static_cast<void*>(&tmp), static_cast<void*>( &front()), sizeof(T));
				begin = (begin + 1) & (max_size1);
				memcpy(static_cast<void*>(&(*this)[size - 1]), static_cast<void*>( &inout), sizeof(T));
				memcpy(static_cast<void*>(&inout), static_cast<void*>(&tmp), sizeof(T));
			}
			// Pushing front while poping back for relocatable types, except arithmetic ones
			SEQ_ALWAYS_INLINE void push_back_pop_front_relocatable(T& inout) noexcept(std::is_nothrow_move_assignable<T>::value) {
				if (std::is_arithmetic<T>::value)
					inout = std::move(push_back_pop_front(std::move(inout)));
				else
					push_back_pop_front_use_relocatable(inout);
			}
			// Pushing front while poping back
			// Might throw, but leave the buffer in a valid state since it is already full
			SEQ_ALWAYS_INLINE auto push_back_pop_front(T&& value) noexcept(std::is_nothrow_move_assignable<T>::value) -> T {
				// Only works for filled array
				T res = std::move(front());
				begin = (begin + 1) & (max_size1);
				(*this)[size - 1] = std::move(value);
				return (res);
			}

			// Pop back/front
			void pop_back(Allocator&  /*unused*/) noexcept {
				destroy_ptr(&back());
				--size;
			}
			void pop_front(Allocator&  /*unused*/) noexcept {
				destroy_ptr(buffer() + begin);
				begin = (begin + 1) & (max_size1);
				--size;
			}
			// Pop front n values
			void pop_front_n(Allocator& allocator, cbuffer_pos n) noexcept {
				for (cbuffer_pos i = 0; i != n; ++i)
					pop_front(allocator);
			}
			// Push front n copies of value
			template<class U>
			void push_front_n(Allocator& allocator, cbuffer_pos n, const U& value) {
				for (cbuffer_pos i = 0; i < n; ++i)
					push_front(allocator, value);
			}
			// Push front n values
			void push_front_n(Allocator&  /*unused*/, cbuffer_pos n) {
				try {
					for (cbuffer_pos i = 0; i < n; ++i)
					{
						if (--begin < 0) begin = max_size1;
						construct_ptr(buffer() + begin);
						++size;
					}
				}
				catch (...) {
					begin = (begin + 1) & max_size1;
					throw;
				}
			}

			// Move buffer content toward the right by 1 element
			// Might throw
			void move_right_1(int pos) noexcept(std::is_nothrow_move_assignable<T>::value || relocatable)
			{
				//starting from pos, move elements toward the end
				T* ptr1 = &at(size - 1);
				T* stop = &at(pos);
				if (stop > ptr1)
					stop = buffer();

				if (!relocatable) {
					while (ptr1 > stop) {
						ptr1[0] = std::move(ptr1[-1]);
						--ptr1;
					}
				}
				else {
					memmove(static_cast<void*>(stop + 1), static_cast<void*>(stop), static_cast<size_t>(ptr1 - stop) * sizeof(T)); ptr1 = stop;
				}

				if (ptr1 != &at(pos)) {
					if (!relocatable)
						*ptr1 = std::move(*(buffer() + max_size1));
					else
						memcpy(static_cast<void*>(ptr1), static_cast<void*>(buffer() + max_size1), sizeof(T));
					ptr1 = (buffer() + max_size1);
					stop = &at(pos);
					if (!relocatable) {
						while (ptr1 > stop) {
							ptr1[0] = std::move(ptr1[-1]);
							--ptr1;
						}
					}
					else {
						memmove(static_cast<void*>(stop + 1), static_cast<void*>(stop), static_cast<size_t>(ptr1 - stop) * sizeof(T));
					}
				}
			}
			// Move buffer content toward the left by 1 element
			// Might throw
			void move_left_1(int pos) noexcept(std::is_nothrow_move_assignable<T>::value || relocatable)
			{
				//starting from pos, move elements toward the beginning
				T* ptr1 = &at(0);
				//T* ptr2 = ptr1 + 1;
				T* stop = buffer() + ((begin + pos - 1) & max_size1);//&at(pos);
				if (stop < ptr1)
					stop = buffer() + max_size1;
				if (!relocatable) {
					while (ptr1 < stop) {
						*ptr1 = std::move(ptr1[1]);
						++ptr1;
					}
				}
				else {
					memmove(static_cast<void*>(ptr1), static_cast<void*>(ptr1 + 1), static_cast<size_t>(stop - ptr1) * sizeof(T)); ptr1 = stop;
				}
				if (ptr1 != buffer() + ((begin + pos - 1) & max_size1)) {
					if (!relocatable)
						*ptr1 = std::move(*(buffer()));
					else
						memcpy(static_cast<void*>(ptr1), static_cast<void*>(buffer()), sizeof(T));
					ptr1 = buffer();
					stop = &at(pos - 1);
					if (!relocatable) {
						while (ptr1 < stop) {
							*ptr1 = std::move(ptr1[1]);
							++ptr1;
						}
					}
					else {
						memmove(static_cast<void*>(ptr1), static_cast<void*>(ptr1 + 1), static_cast<size_t>(stop - ptr1) * sizeof(T));
					}
				}
			}

			// Move buffer content toward the left by 1 element
			// Might throw
			void move_right(Allocator&  /*unused*/, cbuffer_pos pos) noexcept((std::is_nothrow_move_assignable<T>::value&& std::is_nothrow_default_constructible<T>::value) || relocatable)
			{
				if (!relocatable)
					construct_ptr(&(*this)[size]);
				//move elems after pos
				++size;
				// Might throw, fine since no more values are constructed/destructed
				move_right_1(pos);
				// Non optimized way:
				//for (cbuffer_pos i = size - 1; i > pos; --i)
				//	(*this)[i] = std::move((*this)[i - 1]);
			}
			void move_left(Allocator&  /*unused*/, cbuffer_pos pos) noexcept((std::is_nothrow_move_assignable<T>::value&& std::is_nothrow_default_constructible<T>::value) || relocatable)
			{
				if (!relocatable)
					construct_ptr(&(*this)[begin ? -1 : max_size1]);
				//move elems before pos
				if (--begin < 0) begin = max_size1;
				++size;
				// Might throw, fine since no more values are constructed/destructed
				move_left_1(pos + 1);
				// Non optimized way:
				//for (cbuffer_pos i = 0; i < pos -1; ++i)
				//	(*this)[i] = std::move((*this)[i + 1]);
			}

			// Insert value at given position. Only works if the buffer is not full.
			template <class... Args>
			auto emplace(Allocator& allocator, cbuffer_pos pos, Args&&... args) -> T*
			{
				SEQ_ASSERT_DEBUG(size != max_size_, "cannot insert in a full circular buffer");
				if (pos > size / 2) {
					move_right(allocator, pos);
				}
				else {
					move_left(allocator, pos);
				}

				T* res = &(*this)[pos];
				if (relocatable) {
					try {
						construct_ptr(res, std::forward<Args>(args)...);
					}
					catch (...) {
						//No choice but to destroy all values after pos and reduce the size in order to leave the buffer in a valid state
						for (int i = pos + 1; i < size; ++i)
							destroy_ptr(&(*this)[i]);
						size = pos;
						throw;
					}
				}
				else {
					// For non relocatable types, use move assignment to provide basic exception guarantee
					*res = T(std::forward<Args>(args)...);
				}

				return res;
			}

			// Insert value at given position. Only works if the buffer is not full.
			auto insert(Allocator& allocator, cbuffer_pos pos, const T& value) -> T* {
				return emplace(allocator, pos, value);
			}
			auto insert(Allocator& allocator, cbuffer_pos pos, T&& value) -> T* {
				return emplace(allocator, pos, std::move(value));
			}

			// Insert at given position while poping back
			template <class... Args>
			auto insert_pop_back(Allocator&  /*unused*/, cbuffer_pos pos, Args&&... args) -> T
			{
				SEQ_ASSERT_DEBUG(pos != max_size(), "invalid insertion position");
				//move elems after pos
				T* p = &(*this)[size - 1];
				// Might throw, fine
				T res = std::move(*p);
				// For relocatable types, we must destroy the back value
				if (relocatable && !std::is_trivially_destructible<T>::value)
					destroy_ptr(p);

				if (pos > size / 2) {
					move_right_1(pos);
				}
				else {
					if (--begin < 0) begin = max_size1;
					move_left_1(pos + 1);

				}


				if (relocatable) {
					try {
						construct_ptr(&(*this)[pos], std::forward<Args>(args)...);
					}
					catch (...) {
						//No choice but to destroy all values after pos and reduce the size in order to leave the buffer in a valid state
						for (int i = pos + 1; i < size; ++i)
							destroy_ptr(&(*this)[i]);
						size = pos;
						throw;
					}
				}
				else {
					// For non relocatable types, use move assignment to provide basic exception guarantee
					(*this)[pos] = T(std::forward<Args>(args)...);
				}

				return res;
			}
			// Insert at given position while poping front
			template <class... Args>
			auto insert_pop_front(Allocator&  /*unused*/, cbuffer_pos pos, Args&&... args) -> T
			{
				SEQ_ASSERT_DEBUG(pos != 0, "invalid insertion position");
				//move elems after pos
				T* p = &(*this)[0];
				// Might throw, fine
				T res = std::move(*p);

				if (relocatable && !std::is_trivially_destructible<T>::value)
					destroy_ptr(p);

				if (pos < size / 2) {
					move_left_1(pos);
				}
				else {
					begin = (begin + 1) & (max_size1);
					move_right_1(pos - 1);
				}

				p = &(*this)[pos - 1];
				if (relocatable) {
					try {
						construct_ptr(p, std::forward<Args>(args)...);
					}
					catch (...) {
						//No choice but to destroy all values after pos and reduce the size in order to leave the buffer in a valid state
						for (int i = pos; i < size; ++i)
							destroy_ptr(&(*this)[i]);
						size = pos - 1;
						throw;
					}
				}
				else {
					// For non relocatable types, use move assignment to provide basic exception guarantee
					*p = T(std::forward<Args>(args)...);
				}

				return res;
			}

			void move_erase_right_1(int pos) noexcept(std::is_nothrow_move_assignable<T>::value || relocatable)
			{
				//starting from pos, move elements toward the end
				T* ptr1 = &at(pos);
				T* stop = &at(size);
				if (stop < ptr1)
					stop = buffer() + max_size1;
				if (!relocatable) {
					while (ptr1 < stop) {
						*ptr1 = std::move(ptr1[1]); ++ptr1;
					}
				}
				else { memmove(static_cast<void*>(ptr1), static_cast<void*>(ptr1 + 1), static_cast<size_t>(stop - ptr1) * sizeof(T)); ptr1 = stop; }
				if (ptr1 != &at(size)) {
					if (!relocatable)
						*ptr1 = std::move(*(buffer()));
					else
						memcpy(static_cast<void*>(ptr1), static_cast<void*>(buffer()), sizeof(T));
					ptr1 = buffer();
					stop = &at(size);
					if (!relocatable) {
						while (ptr1 < stop) {
							*ptr1 = std::move(ptr1[1]); ++ptr1;
						}
					}
					else { memmove(static_cast<void*>(ptr1), static_cast<void*>(ptr1 + 1), static_cast<size_t>(stop - ptr1) * sizeof(T)); }
				}
			}
			void move_erase_left_1(int pos) noexcept(std::is_nothrow_move_assignable<T>::value || relocatable)
			{
				//starting from pos, move elements toward the beginning
				T* ptr1 = &at(pos);
				T* stop = &at(0);
				if (stop > ptr1)
					stop = buffer();
				if (!relocatable) {
					while (ptr1 > stop) {
						*ptr1 = std::move(ptr1[-1]); --ptr1;
					}
				}
				else { memmove(static_cast<void*>(stop + 1), static_cast<void*>(stop), static_cast<size_t>(ptr1 - stop) * sizeof(T)); ptr1 = stop; }

				if (ptr1 != &at(0)) {
					if (!relocatable)
						*ptr1 = std::move(*(buffer() + max_size1));
					else
						memcpy(static_cast<void*>(ptr1), static_cast<void*> (buffer() + max_size1), sizeof(T));
					ptr1 = (buffer() + max_size1);
					stop = &at(0);
					if (!relocatable) {
						while (ptr1 > stop) {
							*ptr1 = std::move(ptr1[-1]); --ptr1;
						}
					}
					else { memmove(static_cast<void*>(stop + 1), static_cast<void*>(stop), static_cast<size_t>(ptr1 - stop) * sizeof(T)); }
				}
			}

			// Erase element at given position and push back value 
			// This function might throw, but always leave the buffer in a valid state
			void erase_push_back(Allocator&  /*unused*/, cbuffer_pos pos, T&& value)
			{
				//for relocatable type, destroy value at pos since it will be memcpied over
				if (relocatable)
					destroy_ptr(&(*this)[pos]);

				if (pos > size / 2) {
					//move elems after pos
					try {
						--size;
						move_erase_right_1(pos);
					}
					catch (...) {
						++size;
						throw;
					}

					if (!relocatable)
						destroy_ptr(&(*this)[size]);
				}
				else {
					//move elems after pos
					try {
						--size; //decrement size
						move_erase_left_1(pos);
					}
					catch (...) {
						++size;
						throw;
					}

					if (!relocatable)
						destroy_ptr(&(*this)[0]);
					begin = (begin + 1) & (max_size1); //increment begin
				}

				construct_ptr(&at(size), std::move(value));
				size++;

			}

			void erase_push_front(Allocator& allocator, cbuffer_pos pos, T&& value)
			{
				//for relocatable type, destroy value at pos since it will be memcpied over
				if (relocatable)
					destroy_ptr(&(*this)[pos]);

				if (pos > size / 2) {
					//move elems after pos
					try {
						--size;
						move_erase_right_1(pos);
					}
					catch (...) {
						++size;
						throw;
					}
					if (!relocatable)
						destroy_ptr(&(*this)[size]);
				}
				else {
					//move elems after pos
					try {
						--size; //decrement size
						move_erase_left_1(pos);
					}
					catch (...) {
						++size;
						throw;
					}
					if (!relocatable)
						destroy_ptr(&(*this)[0]);
					begin = (begin + 1) & (max_size1); //increment begin
				}
				// Might throw, fine
				push_front(allocator, std::move(value));
			}


			//Erase value at given position.
			void erase(Allocator&  /*unused*/, cbuffer_pos pos) 
			{
				//for relocatable type, destroy value at pos since it will be memcpied over
				if (relocatable)
					destroy_ptr(&(*this)[pos]);
				if (pos > size / 2) {
					//move elems after pos
					try {
						--size;
						move_erase_right_1(pos);
					}
					catch (...) {
						++size;
						throw;
					}
					if (!relocatable)
						destroy_ptr(&(*this)[size]);
				}
				else {
					//move elems after pos
					try {
						--size; //decrement size
						move_erase_left_1(pos);
					}
					catch (...) {
						++size;
						throw;
					}
					if (!relocatable)
						destroy_ptr(&(*this)[0]);
					begin = (begin + 1) & (max_size1); //increment begin
				}
			}
		};


		// Structure used to find the bucket size for a given tiered_vector size
		template<class T>
		struct FindBucketSize
		{
			auto  operator() (size_t size, cbuffer_pos MinBSize, cbuffer_pos MaxBSize) const noexcept -> cbuffer_pos {
				cbuffer_pos res;
				if (size < static_cast<size_t>(MinBSize)) 
					res = static_cast<cbuffer_pos>(MinBSize);
				else {
					if (size < 4096) {
						if (size < 8) res = 2;
						else if (size < 32) res = 4;
						else if (size < 128) res = 8;
						else if (size < 512) res = 16;
						else if (size < 1024) res = 32;
						else if (size < 2048) res = 64;
						else res = 128;
					}
					else {
						// For now, select bigger chunk size as moving objects inside is faster than moving objects between chunks
						//unsigned bits = (bit_scan_reverse(size) >> 1) + 2;// log2 / 2 +2
						//res = static_cast<cbuffer_pos>(std::max(MinBSize, std::min(MaxBSize, static_cast<cbuffer_pos>(1U << bits))));

						// for sizeof(T) == 16*8, 1 is better
						// for sizeof(T) == 32, 2 is better
						// for sizeof(T) == 8 or 16, 3 is better

						static constexpr unsigned offset = sizeof(T) <= 16 ? 3 : (sizeof(T) <= 64 ? 2 : 1);
						unsigned bits = bit_scan_reverse(static_cast<size_t>(std::sqrt(size))) + offset;
						res = static_cast<cbuffer_pos>(1U << bits);
					}
					res = static_cast<cbuffer_pos>(std::max(MinBSize, std::min(MaxBSize, res)));
				}
				return res;
			}
		};


		
		template<class KeyType, class = void>
		struct StorePlainKey
		{
			using key_type = KeyType;
			static constexpr bool value = (std::is_copy_constructible<KeyType>::value && 
				std::is_nothrow_copy_constructible<KeyType>::value && 
				sizeof(KeyType) <= 16);
		};
		/*template<class KeyType >
		struct StorePlainKey<KeyType, typename std::enable_if<is_string_class<KeyType>::value, void>::type>
		{
			using key_type = tstring_view;
			static constexpr bool value = true;
		};*/


		//Store a CircularBuffer pointer and an optional back value (pointer to the back value of the CircularBuffer)
		//Storing the back pointer is only used for sorted tiered_vector
		template< class T, class Allocator, class ValueCompare, bool StoreBackValues, bool IsArithmetic = StorePlainKey<typename ValueCompare::key_type>::value >
		struct StoreBucket 
		{
			// No back value storage
			CircularBuffer<T, Allocator>* bucket;

			StoreBucket()noexcept :bucket(nullptr){}
			StoreBucket(CircularBuffer<T, Allocator>* b) noexcept :bucket(b) {}
			void update()noexcept {}
			auto back() const noexcept -> const T& { return bucket->back(); }
			auto operator->() noexcept -> CircularBuffer<T, Allocator>* { return bucket; }
			auto operator->() const noexcept -> const CircularBuffer<T, Allocator>* { return bucket; }
		};

		template< class T, class Allocator, class ValueCompare>
		struct StoreBucket<T, Allocator, ValueCompare, true, false>
		{
			// Store back value as pointer
			using key_type = typename StorePlainKey<typename ValueCompare::key_type>::key_type;
			const key_type* back_value;
			CircularBuffer<T, Allocator>* bucket;

			StoreBucket()noexcept {}
			StoreBucket(CircularBuffer<T, Allocator>* b) noexcept
				:bucket(b) {}
			void update()noexcept {
				//Update back value
				back_value = &ValueCompare::key((bucket)->back());
			}
			auto back() const noexcept -> const typename ValueCompare::key_type& {
				return *back_value;//ValueCompare::key(*back_value); 
			}
			auto operator->() noexcept -> CircularBuffer<T, Allocator>* { return bucket; }
			auto operator->() const noexcept -> const CircularBuffer<T, Allocator>* { return bucket; }

		};

		
		template< class T, class Allocator, class ValueCompare>
		struct StoreBucket<T, Allocator, ValueCompare, true, true>
		{
			// Store plain back value
			using key_type = typename StorePlainKey<typename ValueCompare::key_type>::key_type;
			key_type back_value;
			CircularBuffer<T, Allocator>* bucket;

			StoreBucket()noexcept {}
			StoreBucket(CircularBuffer<T, Allocator>* b) noexcept
				:back_value(), bucket(b) {}
			StoreBucket(const StoreBucket& other) noexcept
				:back_value(other.back_value), bucket(other.bucket) {}
			StoreBucket(StoreBucket&& other) noexcept
				:back_value(std::move(other.back_value)), bucket(other.bucket) {}
			auto operator=(StoreBucket&& other) noexcept -> StoreBucket& {
				back_value = std::move(other.back_value);
				bucket = other.bucket;
				return *this;
			}
			void update()noexcept {
				//Update back value
				back_value = ValueCompare::key((bucket)->back());
			}
			auto back() const noexcept ->  typename ValueCompare::key_type {
				return back_value; 
			}
			auto operator->() noexcept -> CircularBuffer<T, Allocator>* { return bucket; }
			auto operator->() const noexcept -> const CircularBuffer<T, Allocator>* { return bucket; }
		};
		
	}

	// Specialization of is_relocatable for StoreBucket
	template< class T, class Allocator, class ValueCompare, bool StoreBackValues, bool IsArithmetic >
	struct is_relocatable<detail::StoreBucket<T,Allocator,ValueCompare, StoreBackValues, IsArithmetic> >
	{
		static constexpr bool value = true;
	};
	// Specialization of is_relocatable for StoreBucket
	template< class T, class Allocator, class ValueCompare >
	struct is_relocatable<detail::StoreBucket<T, Allocator, ValueCompare, true, true> > : is_relocatable<typename ValueCompare::key_type> {};
	

	namespace detail
	{
	

		// Bucket manager class, in charge of most tiered_vector operations
		template<class T, class Allocator, int MinBSize, int MaxBSize, bool StoreBackValues , class ValueCompare >
		struct BucketManager : private Allocator
		{
			template< class U>
			using RebindAlloc = typename std::allocator_traits<Allocator>::template rebind_alloc<U>;

			using BucketType = CircularBuffer<T, Allocator>;
			using StoreBucketType = StoreBucket<T, Allocator, ValueCompare, StoreBackValues>;
			using alloc_traits = std::allocator_traits<Allocator>;
			using this_type = BucketManager<T, Allocator, MinBSize, MaxBSize, StoreBackValues, ValueCompare >;
			using size_type = typename std::allocator_traits<Allocator>::size_type;
			using difference_type = typename std::allocator_traits<Allocator>::difference_type;
			using value_type = T;
			using pointer = typename std::allocator_traits<Allocator>::pointer;
			using const_pointer = typename std::allocator_traits<Allocator>::const_pointer;
			using reference = value_type&;
			using BucketVector = devector <StoreBucketType, RebindAlloc<StoreBucketType> > ;
			static constexpr int min_block_size = MinBSize;
			static constexpr int max_block_size = MaxBSize;

			BucketVector d_buckets;							//vector of buckets
			size_type	 d_size;							//full size
			cbuffer_pos  d_bucket_size;						//single bucket size
			cbuffer_pos  d_bucket_size1;					//used for fast random access, single bucket size - 1
			cbuffer_pos  d_bucket_size_bits;				//used for fast random access, single bucket size log2
			

			// Update back value for all buckets
			void update_all_back_values() noexcept {
				if (StoreBackValues) {
					for (size_t i = 0; i < d_buckets.size(); ++i)
						d_buckets[i].update();
				}
			}

		public:
			// Construct from bucket size and allocator
			BucketManager(cbuffer_pos bucket_size,  const Allocator & al = Allocator() ) noexcept(std::is_nothrow_copy_constructible<Allocator>::value)
				:Allocator(al),
				d_buckets(al),
				d_size(0),
				d_bucket_size((bucket_size)),
				d_bucket_size1((bucket_size - 1)),
				d_bucket_size_bits(static_cast<cbuffer_pos>(bit_scan_reverse(static_cast<size_t>(bucket_size))))
			{
			}
			// Move construct
			BucketManager(BucketManager&& other) noexcept(std::is_nothrow_move_constructible<Allocator>::value)
				:Allocator(std::move(other.get_allocator())),
				d_buckets(std::move(other.d_buckets)),
				d_size(other.d_size),
				d_bucket_size(other.d_bucket_size),
				d_bucket_size1(other.d_bucket_size1),
				d_bucket_size_bits(other.d_bucket_size_bits)
			{
				other.d_bucket_size = MinBSize;
				other.d_bucket_size1 = MinBSize - 1;
				other.d_bucket_size_bits = bit_scan_reverse(static_cast<size_t>(MinBSize));
				other.d_size = 0;
			}
			// Move construct
			BucketManager(BucketManager&& other, const Allocator& al) noexcept(alloc_traits::is_always_equal::value) // strengthened
				:Allocator(al),
				d_buckets(std::move(other.d_buckets), RebindAlloc<StoreBucketType>(al)),
				d_size(other.d_size),
				d_bucket_size(other.d_bucket_size),
				d_bucket_size1(other.d_bucket_size1),
				d_bucket_size_bits(other.d_bucket_size_bits)
			{
				other.d_bucket_size = MinBSize;
				other.d_bucket_size1 = MinBSize - 1;
				other.d_bucket_size_bits = bit_scan_reverse(MinBSize);
				other.d_size = 0;
			}
			// Construct by copying the content of other
			BucketManager(const this_type& other,
				cbuffer_pos new_bucket_size,
				size_type start = 0,
				size_type size = static_cast<size_t>( -1),
				const Allocator& al = Allocator())
				:Allocator(al),
				d_buckets(al),
				d_size(0),
				d_bucket_size(new_bucket_size),
				d_bucket_size1(new_bucket_size - 1),
				d_bucket_size_bits(static_cast<cbuffer_pos>(bit_scan_reverse(static_cast<size_t>(new_bucket_size))))
			{
				// Retrieve size 
				size_type full_size = size;
				if (size == static_cast<size_type>(-1))
					full_size = other.d_size;
				else if (size > other.d_size)
					full_size = other.d_size;

				SEQ_ASSERT_DEBUG(start < other.size(), "invalid start position");
				SEQ_ASSERT_DEBUG(start + full_size <= other.size(), "invalid end position");

				// Get new bucket count
				size_type bucket_count = full_size / static_cast<size_t>(new_bucket_size) + (full_size % static_cast<size_t>(new_bucket_size) ? 1 : 0);

				if (bucket_count == 1) {
					// There is only one bucket: just fill front buffer
					// Might throw, fine
					BucketType* current = create_back_bucket();
					for (size_type i = start; i < start + full_size; ++i, ++d_size) {
						current->push_back(get_allocator(), other.at(i));
					}
				}
				else {

					size_type pos = start;
					size_type end_pos = start + full_size;

					while (pos < end_pos) {

						// Create bucket, may throw
						BucketType* current = create_back_bucket();
						T* ptr = current->buffer();

						// Compute end
						size_type end = pos + static_cast<size_t>(new_bucket_size);
						if (end > end_pos)
							end = end_pos;

						// If this throw, d_size won't be updated (which is fine)
						try {
							other.for_each(pos, end, [&](const T& v) {construct_ptr(ptr++, v); });
						}
						catch (...) {
							// Destroy constructed elements
							--ptr;
							for (T* p = current->buffer(); p != ptr; ++p)
								destroy_ptr(p);
							this->remove_back_bucket();
							throw;
						}
						size_type copied = end - pos;
						pos += copied;
						d_size += copied;

						if (end <= end_pos || d_buckets.size() > 1) {
							// Fill back buffer
							current->size = static_cast<cbuffer_pos>(copied);
							current->resize(current->size, get_allocator());
						}
						else {
							// Fill front buffer
							current->grow_front(static_cast<cbuffer_pos>(copied));
						}
					}
				}

				// Update back values if necessary
				update_all_back_values();

			}


			// Construct by moving the content of other and destroying other's elements
			BucketManager(this_type&& other,
				cbuffer_pos new_bucket_size,
				size_type start = 0,
				size_type size = static_cast<size_t>(-1),
				const Allocator& al = Allocator())
				:Allocator(al),
				d_buckets(al),
				d_size(0),
				d_bucket_size(static_cast<cbuffer_pos>(new_bucket_size)),
				d_bucket_size1(static_cast<cbuffer_pos>(new_bucket_size - 1)),
				d_bucket_size_bits(static_cast<cbuffer_pos>(bit_scan_reverse(static_cast<size_t>(new_bucket_size))))
			{
				
				// Get size
				size_type full_size = size;
				if (size == static_cast<size_type>(-1))
					full_size = other.d_size;
				else if (size > other.d_size)
					full_size = other.d_size;

				//SEQ_ASSERT_DEBUG(start < other.size(), "invalid start position");
				SEQ_ASSERT_DEBUG(start + full_size <= other.size(), "invalid end position");


				size_type bucket_count = full_size / static_cast<size_type>(new_bucket_size) + ((full_size % static_cast<size_type>(new_bucket_size)) ? 1 : 0);
				if (bucket_count == 0)
					return;
				if (bucket_count == 1) {
					// Fill front buffer
					BucketType* current = create_front_bucket();

					// Might throw, fine
					for (size_type i = start; i < start + full_size; ++i, ++d_size) 
						current->push_back(get_allocator(), std::move(other.at(i)));
					
					// Destroy/deallocate the content of other
					other.destroy_all();
					
					// Update back values if necessary
					update_all_back_values();
					return;
				}
			

				size_type pos = start;
				size_type end_pos = start + full_size;

				size_t bindex = other.bucket_index(start);
				size_t front_size = static_cast<size_t>(other.d_buckets[0]->size);
				//destroy buckets before
				if (bindex) {
					for (size_t i = 0; i < bindex; ++i) {
						other.destroy_bucket(other.d_buckets[i].bucket);
						other.d_buckets[i].bucket = nullptr;
					}
				}
			
				while (pos < end_pos) {

					// Create bucket, may throw
					BucketType* current = create_back_bucket();
					T* ptr = current->buffer();
					// Compute end
					size_type end = pos + static_cast<size_t>(new_bucket_size);
					if (end > end_pos)
						end = end_pos;

					// If this throw, d_size won't be updated (which is fine)
					try {
						
						other.for_each_bucket(pos, end, front_size, [&ptr,&bindex,&other](size_t index, T& v) {
								construct_ptr(ptr++, std::move(v)); 
								if (index != bindex) {
									other.destroy_bucket(other.d_buckets[bindex].bucket);
									other.d_buckets[bindex].bucket = nullptr;
									bindex = index;
								}
								});
					}
					catch (...) {
						// Destroy constructed values
						--ptr;
						for (T* p = current->buffer(); p != ptr; ++p)
							destroy_ptr(p);
						this->remove_back_bucket();
						other.destroy_all();
						throw;
					}

					size_type copied = end - pos;
					pos += copied;
					d_size += copied;

					if (end <= end_pos || d_buckets.size() > 1) {
						// Fill back buffer
						current->size = static_cast<cbuffer_pos>(copied);
						current->resize(current->size, get_allocator());
					}
					else {
						// Fill front buffer
						current->grow_front(static_cast<cbuffer_pos>(copied));
					}
				}

				//destroy buckets after
				for (; bindex < other.d_buckets.size(); ++bindex) {
					if (other.d_buckets[bindex].bucket) {
						other.destroy_bucket(other.d_buckets[bindex].bucket);
						other.d_buckets[bindex].bucket = nullptr;
					}
				}
				other.d_buckets.clear();
				
				// Update back values if necessary
				update_all_back_values();
			}

			~BucketManager()
			{
				// Destroy all element, deallocate buckets
				destroy_all();
			}

			void destroy_bucket(BucketType* b )
			{
				if (!b) return;
				b->destroy(get_allocator());
				get_allocator().deallocate(reinterpret_cast<T*>(b), static_cast<size_t>(b->max_size()) + BucketType::start_data_T);
			}
			auto make_bucket(int max_size) -> BucketType* 
			{
				BucketType* res = reinterpret_cast<BucketType*>(get_allocator().allocate(BucketType::start_data_T + static_cast<size_t>(max_size)));
				// Never throw
				construct_ptr(res, max_size);
				return res;
			}
			auto make_bucket(int max_size, const T& val) -> BucketType*
			{
				BucketType* res = reinterpret_cast<BucketType*>(get_allocator().allocate(BucketType::start_data_T + static_cast<size_t>(max_size)));
				// Might throw
				try {
					construct_ptr(res, max_size, val, get_allocator());
				}
				catch (...) {
					get_allocator().deallocate(reinterpret_cast<T*>(res), BucketType::start_data_T + static_cast<size_t>(max_size));
					throw;
				}
				return res;
			}

			// Destroy all element, deallocate buckets 
			void destroy_all() 
			{
				for(auto it = d_buckets.begin(); it != d_buckets.end(); ++it){
					if (it->bucket)
						destroy_bucket(it->bucket);
				}
				d_buckets.clear();
			}
			
			// Move copy
			auto operator=(BucketManager&& other) noexcept(noexcept(swap_allocator(std::declval<Allocator&>(), std::declval<Allocator&>()))) -> BucketManager&
			{
				if (this != std::addressof(other)) {
					swap_allocator(get_allocator(), other.get_allocator());
					d_buckets.swap( other.d_buckets);
					std::swap(d_bucket_size, other.d_bucket_size);
					std::swap(d_bucket_size1, other.d_bucket_size1);
					std::swap(d_bucket_size_bits, other.d_bucket_size_bits);
					std::swap(d_size, other.d_size);
				}
				return *this;
			}
			// Get bucket vector
			BucketVector & buckets() noexcept {return d_buckets;}
			const BucketVector & buckets() const noexcept {return d_buckets;}
			// For each function, faster than using iterators
			template<class Functor>
			auto for_each(size_type start, size_type end, Functor fun) const -> Functor
			{
				size_type remaining = end - start;
				size_type bindex = bucket_index(start);
				size_type pos = static_cast<size_type>(bucket_pos(start));

				while (remaining) {
					const BucketType* cur = d_buckets[bindex].bucket;

					size_type offset = static_cast<size_t>(cur->begin) + pos;
					const T* b_end = cur->buffer() + cur->max_size();
					const T* _start = cur->buffer() + offset;
					if (_start > b_end)
						_start = cur->buffer() + (_start - b_end);
					size_type to_copy = std::min(remaining, static_cast<size_type>(cur->size) - pos );
					const T* _send = _start + to_copy;
					const T* _end = (_send > b_end) ? b_end : _send;
					while (_start < _end)
						fun(*_start++);

					if (_send > b_end) {
						_start = cur->buffer();
						_end = _start + to_copy - (static_cast<size_t>(cur->max_size()) - offset);
						while (_start < _end)
							fun(*_start++);
					}
					remaining -= to_copy;
					pos = 0;
					++bindex;
				}
				return fun;
			}
			// For each function, faster than using iterators
			template<class Functor>
			auto for_each(size_type start, size_type end, Functor fun) -> Functor
			{
				//Construct/fill out using values from start to end

				size_type remaining = end - start;
				size_type bindex = bucket_index(start);
				size_type pos = static_cast<size_type>(bucket_pos(start));

				while (remaining) {
					BucketType* cur = d_buckets[bindex].bucket;

					size_type offset = static_cast<size_t>(cur->begin) + pos;
					T* b_end = cur->buffer() + cur->max_size();
					T* _start = cur->buffer() + offset;
					if (_start > b_end)
						_start = cur->buffer() + (_start - b_end);
					size_type to_copy = std::min(remaining, static_cast<size_type>(cur->size) - pos );
					T* _send = _start + to_copy;
					T* _end = (_send > b_end) ? b_end : _send;
					while (_start < _end)
						fun(*_start++);

					if (_send > b_end) {
						_start = cur->buffer();
						_end = _start + to_copy - (static_cast<size_t>(cur->max_size()) - offset);
						while (_start < _end)
							fun(*_start++);
					}
					remaining -= to_copy;
					pos = 0;
					++bindex;
				}
				return fun;
			}

			// For each function, faster than using iterators
			template<class Functor>
			auto for_each_bucket(size_type start, size_type end, size_type front_size, Functor fun) -> Functor
			{
				//Construct/fill out using values from start to end
				size_type remaining = end - start;
				size_type bindex = bucket_index(start, front_size);
				size_type pos = static_cast<size_type>(bucket_pos(start, front_size));

				while (remaining) {
					BucketType* cur = d_buckets[bindex].bucket;

					size_type offset = static_cast<size_t>(cur->begin) + pos;
					T* b_end = cur->buffer() + cur->max_size();
					T* _start = cur->buffer() + offset;
					if (_start > b_end)
						_start = cur->buffer() + (_start - b_end);
					size_type to_copy = std::min(remaining, static_cast<size_type>(cur->size) - pos);
					T* _send = _start + to_copy;
					T* _end = (_send > b_end) ? b_end : _send;
					while (_start < _end)
						fun(bindex , *_start++);

					if (_send > b_end) {
						_start = cur->buffer();
						_end = _start + to_copy - (static_cast<size_t>(cur->max_size()) - offset);
						while (_start < _end)
							fun(bindex ,*_start++);
					}
					remaining -= to_copy;
					pos = 0;
					++bindex;
				}
				return fun;
			}


			// Returns true is tiered_vector size is even
			SEQ_ALWAYS_INLINE auto isPow2Size() const noexcept -> bool { return isPow2Size(d_size); }
			SEQ_ALWAYS_INLINE auto isPow2Size(size_t s) const noexcept -> bool { return ((s - 1) & s) == 0; }
			 // Returns allocator
			SEQ_ALWAYS_INLINE auto get_allocator() const noexcept -> const Allocator& { return *this; }
			SEQ_ALWAYS_INLINE auto get_allocator() noexcept -> Allocator& { return *this; }
			
			// Returns bucket at given position
			SEQ_ALWAYS_INLINE auto bucket(size_type pos) noexcept -> BucketType* { SEQ_ASSERT_DEBUG(pos >= 0 && pos < d_buckets.size(),"invalid bucket position"); return d_buckets[pos].bucket; }
			SEQ_ALWAYS_INLINE auto bucket(size_type pos) const noexcept -> const BucketType* { SEQ_ASSERT_DEBUG(pos >= 0 && pos < d_buckets.size(), "invalid bucket position"); return d_buckets[pos].bucket; }
			// Returns bucket index for global position in tiered_vector
			SEQ_ALWAYS_INLINE auto bucket_index(size_type pos) const noexcept -> size_type {
				SEQ_ASSERT_DEBUG(d_buckets.size() > 0, "invalid bucket position");
				return (pos + static_cast<size_type>(d_bucket_size - d_buckets.front()->size)) >> d_bucket_size_bits;
			}
			SEQ_ALWAYS_INLINE auto bucket_index(size_type pos, size_type front_size) const noexcept -> size_type {
				SEQ_ASSERT_DEBUG(d_buckets.size() > 0, "invalid bucket position");
				return (pos + static_cast<size_type>(d_bucket_size) - front_size) >> d_bucket_size_bits;
			}
			// Returns position within bucket for global position in tiered_vector
			SEQ_ALWAYS_INLINE auto bucket_pos(size_type pos) const noexcept -> int { 
				SEQ_ASSERT_DEBUG(d_buckets.size() > 0, "invalid bucket position");
				return static_cast<int>((pos - (pos < static_cast<size_type>(d_buckets.front()->size) ? 0 : static_cast<size_t>(d_buckets.front()->size))) & static_cast<size_t>(d_bucket_size1));
			}
			SEQ_ALWAYS_INLINE auto bucket_pos(size_type pos, size_type front_size) const noexcept -> int {
				SEQ_ASSERT_DEBUG(d_buckets.size() > 0, "invalid bucket position");
				return static_cast<int>((pos - (pos < front_size ? 0 : front_size)) & static_cast<size_t>(d_bucket_size1));
			}
			// Returns the bucket size (always power of 2)
			SEQ_ALWAYS_INLINE auto bucket_size() const noexcept -> int { return d_bucket_size; }
			// Returns number of buckets
			SEQ_ALWAYS_INLINE auto bucket_count() const noexcept -> size_type { return d_buckets.size(); }
			// Returns full size
			SEQ_ALWAYS_INLINE auto size() const noexcept -> size_type { return d_size; }
			// Returns bucket size at given position
			SEQ_ALWAYS_INLINE auto bucket_size(size_type pos) const noexcept -> int { return  (bucket(pos))->size; }
			// Retruns back value
			SEQ_ALWAYS_INLINE auto back() noexcept -> T & { SEQ_ASSERT_DEBUG(d_buckets.size() > 0, "empty container"); return d_buckets.back()->back();}
			SEQ_ALWAYS_INLINE auto back() const noexcept -> const T& { SEQ_ASSERT_DEBUG(d_buckets.size() > 0, "empty container"); return d_buckets.back()->back(); }
			// Retruns front value
			SEQ_ALWAYS_INLINE auto front() noexcept -> T& { SEQ_ASSERT_DEBUG(d_buckets.size() > 0, "empty container"); return d_buckets.front()->front();}
			SEQ_ALWAYS_INLINE auto front() const noexcept -> const T& { SEQ_ASSERT_DEBUG(d_buckets.size() > 0, "empty container"); return d_buckets.front()->front(); }

			// Returns element at global position
			auto at(size_type pos) noexcept -> T& {
				SEQ_ASSERT_DEBUG(d_buckets.size() > 0, "empty container");
				//It seems that using d_buckets.front()->size instead of d_first_bucket_size
				//and (d_bucket_size - d_buckets.front()->size) instead of d_first_bcket_rem is as fast
				size_t front_size = static_cast<size_t>(d_buckets.front()->size);
				size_t bucket = (pos + (static_cast<size_t>(d_bucket_size) - front_size)) >> static_cast<size_t>(d_bucket_size_bits);
				size_t index = (pos - (pos < front_size ? 0 : front_size)) & static_cast<size_t>(d_bucket_size1);
				return (*d_buckets[bucket].bucket)[static_cast<cbuffer_pos>(index)];

			}
			auto at(size_type pos) const noexcept -> const T& {
				SEQ_ASSERT_DEBUG(d_buckets.size() > 0, "empty container");
				size_t front_size = static_cast<size_t>(d_buckets.front()->size);
				size_t bucket = (pos + (static_cast<size_t>(d_bucket_size) - front_size)) >> static_cast<size_t>(d_bucket_size_bits);
				size_t index = (pos - (pos < front_size ? 0 : front_size)) & static_cast<size_t>(d_bucket_size1);
				return (*d_buckets[bucket].bucket)[static_cast<cbuffer_pos>(index)];
			}

			// Create bucket at the back
			auto create_back_bucket() -> BucketType* {
				//Bucket is full, create a new one
				// Might throw, fine
				BucketType* bucket = make_bucket(d_bucket_size);
				if ((d_buckets.empty())) {
					bucket->init_front();
				}
				try {
					d_buckets.push_back(bucket);
				}
				catch (...) {
					destroy_bucket(bucket);
					throw;
				}
				return bucket;
			}
			// Create bucket at the back filled with given value
			auto create_back_bucket(const T& val) -> BucketType* {
				//bucket is full, create a new one
				// Might throw, fine
				BucketType* bucket = make_bucket(d_bucket_size, val);
				try {
					d_buckets.push_back(bucket);
				}
				catch (...) {
					destroy_bucket(bucket);
					throw;
				}
				return bucket;
			}
			// Create front bucket
			auto create_front_bucket() -> BucketType* {
				//bucket is full, create a new one
				BucketType* bucket = make_bucket(d_bucket_size);
				bucket->init_front();
				try {
					d_buckets.insert(d_buckets.begin(), bucket);
				}
				catch (...) {
					destroy_bucket(bucket);
					throw;
				}
				return bucket;
			}
			// Create bucket at the front filled with given value
			auto create_front_bucket(const T & value) -> BucketType* {
				//bucket is full, create a new one
				BucketType* bucket = make_bucket(d_bucket_size, value);
				try {
					d_buckets.insert(d_buckets.begin(), bucket);
				}
				catch (...) {
					destroy_bucket(bucket);
					throw;
				}
				return bucket;
			}

			//Remove bucket, but d_buckets must still have one bucket
			void remove_back_bucket() noexcept {
				SEQ_ASSERT_DEBUG(d_buckets.size() > 1, "Cannot remove bucket");
				destroy_bucket(d_buckets.back().bucket);
				d_buckets.pop_back();
			}
			void remove_front_bucket() noexcept {
				SEQ_ASSERT_DEBUG(d_buckets.size() > 1, "Cannot remove bucket");
				destroy_bucket(d_buckets.front().bucket);
				d_buckets.erase(d_buckets.begin());
			}
			// Make sure at least one bucket is present
			SEQ_ALWAYS_INLINE void ensure_has_bucket() {
				if (d_buckets.empty())
					create_back_bucket();
			}

			// Back insertion

			template< class... Args >
			auto emplace_back(Args&&... args) -> T& {
				// All functions might throw, fine (strong guarantee)
				if (SEQ_UNLIKELY(d_buckets.empty()))
					create_back_bucket();
				BucketType* bucket = d_buckets.back().bucket;
				if (SEQ_UNLIKELY(bucket->size == d_bucket_size)) {
					bucket = create_back_bucket();
				}
				T* ptr =  bucket->emplace_back(get_allocator(), std::forward<Args>(args)...);
				if(StoreBackValues) d_buckets.back().update();
				d_size++;
				return *ptr;
			}
			void push_back(const T& value) {
				emplace_back(value);
			}
			void push_back(T&& value) {
				emplace_back(std::move(value));
			}

			// Front insertion
			template< class... Args >
			auto emplace_front(Args&&... args) -> T& {
				// All functions might throw, fine
				if (SEQ_UNLIKELY(d_buckets.empty()))
					create_back_bucket();
				BucketType* bucket = d_buckets.front().bucket;
				if (SEQ_UNLIKELY(bucket->size == d_bucket_size))
					bucket = create_front_bucket();

				T * res = bucket->emplace_front(get_allocator(), std::forward<Args>(args)...);
				if (StoreBackValues) d_buckets.front().update();
				d_size++;
				return *res;
			}
			void push_front(const T& value) {
				emplace_front(value);
			}
			void push_front(T&& value) {
				emplace_front(std::move(value));
			}

			// Insertion in the case where d_buckets.size() == 1
			template< class... Args >
			auto insert_one_bucket(size_type bucket_index, int index, Args&&... args) -> T*
			{
				T* res = nullptr;
				SEQ_ASSERT_DEBUG(bucket_index == 0, "Corrupted tiered_vector");
				if (d_buckets[0]->isFull()) {

					// All functions might throw, fine
					res = create_back_bucket()->push_back(get_allocator(), std::move(d_buckets[0]->insert_pop_back(get_allocator(), index, std::forward<Args>(args)...)));
					if (StoreBackValues) {
						d_buckets.back().update();
						d_buckets[0].update();
					}
				}
				else {
					// Might throw, fine
					res = d_buckets[0].bucket->emplace(get_allocator(), index, std::forward<Args>(args)...);
					if (StoreBackValues) d_buckets.front().update();
				}
				return res;
			}
			template< class... Args >
			auto insert_left(size_type pos, size_type bucket_index, int index, Args&&... args) -> T*
			{
				T* res = nullptr;
				//bucket is on the left side
				if (SEQ_UNLIKELY(index == 0)) {
					--bucket_index; //we work on the previous bucket
					index = bucket(bucket_index)->size;
				}
				if (SEQ_UNLIKELY(bucket(bucket_index)->size < d_bucket_size)) {
					//SEQ_ASSERT_DEBUG(bucket_index == 0, "Corrupted tiered_vector structure");
					// We insert into the first bucket which is not full
					// Might throw, fine
					res = bucket(bucket_index)->emplace(get_allocator(), index, std::forward<Args>(args)...);
					if (StoreBackValues) d_buckets[bucket_index].update();
				}
				else if (SEQ_UNLIKELY(bucket_index == 0)) {
					//insert into full first bucket
					// Might throw, fine
					T tmp = d_buckets[0]->insert_pop_front(get_allocator(), index, std::forward<Args>(args)...);
					try {
						res = create_front_bucket()->push_back(get_allocator(), std::move(tmp));
					}
					catch (...) {
						//check if front buffer is empty
						if (d_buckets[0]->size == 0)
							this->remove_front_bucket();
						throw;
					}
					if (StoreBackValues) {
						d_buckets[0].update();
						d_buckets[1].update();
					}
				}
				else {
					//we are going to loose value at index 0, save it
					size_t bindex = bucket_index;
					//T tmp = std::move((*bucket(bindex))[0]);
					//bucket(bucket_index)->insert<false>(index, value);

					// Might throw, fine
					T tmp = bucket(bucket_index)->insert_pop_front(get_allocator(), index, std::forward<Args>(args)...);
					if (StoreBackValues) d_buckets[bucket_index].update();

					//propagate down to left side
					while (--bindex > 0) {
						// Might throw, fine
						if (BucketType::relocatable)
							bucket(bindex)->push_back_pop_front_relocatable(tmp);
						else
							tmp = bucket(bindex)->push_back_pop_front(std::move(tmp));
						if (StoreBackValues) d_buckets[bindex].update();
					}
					//first bucket
					BucketType* bucket = d_buckets.front().bucket;
					if (!bucket->isFull()) {
						// Might throw, fine
						bucket->push_back(get_allocator(), std::move(tmp));
						if (StoreBackValues) d_buckets[0].update();
					}
					else {
						if (BucketType::relocatable)
							bucket->push_back_pop_front_relocatable(tmp);
						else
							tmp = bucket->push_back_pop_front(std::move(tmp));
						if (StoreBackValues) d_buckets[0].update();
						bucket = create_front_bucket();
						try {
							bucket->push_back(get_allocator(), std::move(tmp));
						}
						catch (...) {
							//remove front buffer
							this->remove_front_bucket();
							throw;
						}
						if (StoreBackValues) d_buckets[0].update();
					}
					res = &at(pos);
				}
				return res;
			}
			template< class... Args >
			auto insert_right(size_type pos, size_type bucket_index, int index, Args&&... args) -> T*
			{
				T* res = nullptr;
				if (SEQ_UNLIKELY(bucket(bucket_index)->size < d_bucket_size)) {
					SEQ_ASSERT_DEBUG(bucket_index == 0 || bucket_index == d_buckets.size() - 1, "Corrupted tiered_vector structure");
					// Might throw, fine
					res = bucket(bucket_index)->emplace(get_allocator(), index, std::forward<Args>(args)...);
					if (StoreBackValues) d_buckets[bucket_index].update();
				
				}
				else if (SEQ_UNLIKELY(bucket_index == d_buckets.size() - 1)) {
					// Inserting into last (full) bucket
					// Might throw, fine
					T tmp = bucket(bucket_index)->insert_pop_back(get_allocator(), index, std::forward<Args>(args)...);
					if (StoreBackValues) d_buckets[bucket_index].update();
					try {
						res = create_back_bucket()->push_back(get_allocator(), std::move(tmp));
					}
					catch (...) {
						// Remove empty back bucket
						if (d_buckets.back()->size == 0)
							remove_back_bucket();
						throw;
					}
					if (StoreBackValues) d_buckets.back().update();
				}
				else {
				
					size_type bindex = bucket_index;
					// Might throw, fine
					T tmp = bucket(bindex)->insert_pop_back(get_allocator(), index, std::forward<Args>(args)...);
					if (StoreBackValues) d_buckets[bindex].update();

					// Propagate to right buckets with successive push_front
					while (++bindex < static_cast<size_type>(d_buckets.size() - 1)) {
						//Might throw, fine
						if (BucketType::relocatable)
							bucket(bindex)->push_front_pop_back_relocatable(tmp);
						else
							tmp = bucket(bindex)->push_front_pop_back(std::move(tmp));
						if (StoreBackValues) d_buckets[bindex].update();
					}
				
					//last bucket
					BucketType* bucket = d_buckets.back().bucket;
					if (!bucket->isFull()) {
						// Might throw, fine
						bucket->push_front(get_allocator(), std::move(tmp));
						//if (StoreBackValues) d_buckets.back().update();
					}
					else {
						// Might throw, fine
						if (BucketType::relocatable)
							bucket->push_front_pop_back_relocatable(tmp);
						else
							tmp = bucket->push_front_pop_back(std::move(tmp));
						if (StoreBackValues) d_buckets.back().update();
						bucket = create_back_bucket();
						bucket->push_front(get_allocator(), std::move(tmp));
						if (StoreBackValues) d_buckets.back().update();
					}
					res = &at(pos);
				}
				return res;
			}

			// Insert value at a location which is not the front or back
			template< class... Args >
			auto insert_middle_fwd(size_type pos, Args&&... args) -> T *
			{
				T* res = nullptr;

				size_t front_size = static_cast<size_t>(d_buckets.front()->size);
				size_t bucket_index = (pos + (static_cast<size_t>(d_bucket_size) - front_size)) >> static_cast<size_t>(d_bucket_size_bits);
				int index = static_cast<int>((pos - (pos < front_size ? 0 : front_size)) & static_cast<size_t>(d_bucket_size1));

				if (SEQ_UNLIKELY(d_buckets.size() == 1)) {
					res = insert_one_bucket(bucket_index, index, std::forward<Args>(args)...);
				}
				else if (pos < d_size / 2) {
					//bucket is on the left side
					res = insert_left(pos, bucket_index, index, std::forward<Args>(args)...);
				}
				else {
					//bucket is on the right side
					res = insert_right(pos, bucket_index, index, std::forward<Args>(args)...);
				}

				d_size++;
				return res;
			}
		
			// Insert anywhere
			template<class... Args>
			auto insert(size_type pos, Args&&... args) -> T & {

				SEQ_ASSERT_DEBUG(pos <= size(), "invalid insert position");

				if(SEQ_LIKELY(pos != 0 && pos != size()))
					return *insert_middle_fwd(pos, std::forward<Args>(args)...);
				
				if (pos==0)
					return emplace_front(std::forward<Args>(args)...);
				
				return emplace_back(std::forward<Args>(args)...);
			}

			void pop_back() noexcept {
				//This the equivalent of above, but inlined
				SEQ_ASSERT_DEBUG(d_buckets.size(), "pop_back on an empty tiered_vector");
				SEQ_ASSERT_DEBUG(d_buckets.back()->size > 0, "pop_back on an empty tiered_vector");
				d_buckets.back()->pop_back(get_allocator());
				if (SEQ_UNLIKELY(d_buckets.back()->size == 0 && d_buckets.size() > 1))
					remove_back_bucket();
				else if (StoreBackValues) d_buckets.back().update();
				--d_size;
			}
			void pop_front() noexcept {
				SEQ_ASSERT_DEBUG(d_buckets.size(), "pop_front on an empty tiered_vector");
				BucketType* bucket = d_buckets.front().bucket;
				SEQ_ASSERT_DEBUG(bucket->size > 0, "pop_front on an empty tiered_vector");
				bucket->pop_front(get_allocator());
				if (SEQ_UNLIKELY(bucket->size == 0 && d_buckets.size() > 1)) {
					remove_front_bucket();
				}
				else if (StoreBackValues) d_buckets.front().update();
				--d_size;
			}

			void erase_extremity(size_type pos) noexcept
			{
				if (pos == 0)
					pop_front();
				else
					pop_back();
			}

			// Erase anywhere
			void erase(size_type pos) {
				SEQ_ASSERT_DEBUG(pos < d_size, "tiered_vector: erase at invalid position");
				SEQ_ASSERT_DEBUG(d_size > 0, "tiered_vector: erase element on an empty tiered_vector");
				if (SEQ_UNLIKELY(pos == 0 || pos == d_size - 1))
					return erase_extremity(pos);
				
				erase_middle(pos);
			}
			SEQ_ALWAYS_INLINE void erase_left(size_type bucket_index, int index)
			{
				//shift left values
				T tmp = std::move((d_buckets.front().bucket)->back());
				d_buckets.front().bucket->pop_back(get_allocator());
				if (StoreBackValues) d_buckets.front().update();

				for (size_type i = 1; i < bucket_index; ++i) {
					// Might throw, fine
					if (BucketType::relocatable)
						bucket(i)->push_front_pop_back_relocatable(tmp);
					else
						tmp = std::move(bucket(i)->push_front_pop_back(std::move(tmp)));
					if (StoreBackValues) d_buckets[i].update();
				}
			
				// Might throw, fine
				bucket(bucket_index)->erase_push_front(get_allocator(), index, std::move(tmp));

				if (StoreBackValues) 
					d_buckets[bucket_index].update();
				if (d_buckets.front()->size == 0 && d_buckets.size() > 1) {
					remove_front_bucket();
				}
			}
			SEQ_ALWAYS_INLINE void erase_right(size_type bucket_index, int index)
			{
				// Shift right values

				// Might throw, fine
				bucket(bucket_index)->erase_push_back(get_allocator(),index,std::move(bucket(bucket_index + 1)->front()));
				if (StoreBackValues) d_buckets[bucket_index].update();

				for (size_type i = bucket_index + 1; i < d_buckets.size() - 1; ++i) {
					// Might throw, fine
					bucket(i)->push_back_pop_front(std::move(bucket(i + 1)->front()));
					if (StoreBackValues) d_buckets[i].update();
				}
				d_buckets.back()->pop_front( get_allocator());
				if (d_buckets.back()->size == 0 && d_buckets.size() > 1) {
					remove_back_bucket();
				}
				else if (StoreBackValues)
					d_buckets.back().update();
			}
			void erase_middle(size_type pos)
			{
				//find bucket for pos
				size_type bucket_index = this->bucket_index(pos);
				//find index in bucket
				int index = this->bucket_pos(/*bucket_index,*/ pos);

				if (bucket_index == 0) {
					//remove from first bucket
					BucketType* bucket = d_buckets.front().bucket;
					SEQ_ASSERT_DEBUG(bucket->size > 0, "erase on an empty tiered_vector");
					// Might throw, fine
					bucket->erase(get_allocator(), index );
					if (bucket->size == 0 ) {
						remove_front_bucket();
					}
					else if (StoreBackValues) d_buckets.front().update();

				}
				else if (bucket_index == d_buckets.size() - 1) {
					//remove from last bucket
					BucketType* bucket = d_buckets.back().bucket;
					SEQ_ASSERT_DEBUG(bucket->size > 0, "erase on an empty tiered_vector");
					// Might throw, fine
					bucket->erase(get_allocator(), index);
					if (bucket->size == 0 && d_buckets.size() > 1) {
						remove_back_bucket();
					}
					else if (StoreBackValues) d_buckets.back().update();
				}
				else if (pos < d_size / 2) {
					erase_left(bucket_index, index);
				}
				else {
					erase_right(bucket_index, index);
				}

				d_size--;
			}

			void resize(size_type size)
			{
				if (size == d_size)
					return;

				if (size > d_size) {
					// Might throw, fine
					ensure_has_bucket();

					//add missing buckets
					size_type missing = static_cast<size_t>(size - d_size);
					size_type last_bucket_rem = static_cast<size_t>(bucket_size() - d_buckets.back()->size);

					if (missing < last_bucket_rem) {
						// The last bucket has enough space: just resize it
						// Might throw, the bucket remains in its original form, fine
						d_buckets.back()->resize(d_buckets.back()->size + static_cast<cbuffer_pos>(missing), get_allocator());

						d_size += missing;
					}
					else {
						// Resize last bucket
						// Might throw, the bucket remains in its original form, fine
						d_buckets.back()->resize(bucket_size(), get_allocator());

						missing -= last_bucket_rem;
						d_size += last_bucket_rem;

						size_type new_bucket_count = missing / static_cast<size_t>(bucket_size());
						int last_bucket_size = static_cast<int>(missing % static_cast<size_t>(bucket_size()));

						try {
							//add full buckets
							for (size_type i = 0; i < new_bucket_count; ++i) {
								BucketType* b = create_back_bucket();
								b->resize(bucket_size(), get_allocator());
								d_size += static_cast<size_t>(bucket_size());
						
							}
							//add last
							if (last_bucket_size) {
								create_back_bucket()->resize(last_bucket_size, get_allocator());
								d_size += static_cast<size_t>(last_bucket_size);
							}	
						}
						catch (...) {
							if(d_buckets.size() > 1 && d_buckets.back()->size == 0)
								remove_back_bucket();
							throw;
						}
					}

				}
				else {
					size_type to_remove = d_size - size;
					size_type last_bucket_size = static_cast<size_t>(bucket_size(bucket_count() - 1));

					if (last_bucket_size > to_remove) {
						//the last bucket has enough space: just resize it
						d_buckets.back()->resize(static_cast<cbuffer_pos>(last_bucket_size - to_remove), get_allocator());
					}
					else {
						//remove last bucket
						remove_back_bucket();

						to_remove -= last_bucket_size;

						//dealloc buckets starting to the last one
						while (to_remove >= static_cast<size_type>(bucket_size())) {
							to_remove -= static_cast<size_t>(bucket_size(bucket_count() - 1));
							remove_back_bucket();
						}

						//resize last bucket
						d_buckets.back()->resize(bucket_size(bucket_count() - 1) - static_cast<cbuffer_pos>(to_remove), get_allocator());
					}

					d_size = size;
				}

				update_all_back_values();
			}
			void resize(size_type size, const T& val)
			{
				if (size == d_size)
					return;

				if (size > d_size) {
					ensure_has_bucket();

					//add missing buckets
					size_type missing = size - d_size;
					size_type last_bucket_rem = static_cast<size_t>(bucket_size() - d_buckets.back()->size);

					if (missing < last_bucket_rem) {
						//the last bucket has enough space: just resize it
						d_buckets.back()->resize(d_buckets.back()->size + static_cast<cbuffer_pos>(missing), val, get_allocator());
						d_size += missing;
					}
					else {
						//resize last bucket
						d_buckets.back()->resize(bucket_size(), val, get_allocator());

						missing -= last_bucket_rem;
						d_size += last_bucket_rem;

						size_type new_bucket_count = missing / static_cast<size_t>(bucket_size());
						cbuffer_pos last_bucket_size = static_cast<cbuffer_pos>(missing % static_cast<size_t>(bucket_size()));

						try {
							//add full buckets
							for (size_type i = 0; i < new_bucket_count; ++i) {
								create_back_bucket(val);
								d_size += static_cast<size_t>(bucket_size());
							}
							//add last
							if (last_bucket_size) {
								create_back_bucket(val)->resize(last_bucket_size, get_allocator());
								d_size += static_cast<size_t>(last_bucket_size);
							}
						}
						catch (...) {
							if (d_buckets.size() > 1 && d_buckets.back()->size == 0)
								remove_back_bucket();
							throw;
						}
					}

				}
				else {
					size_type to_remove = d_size - size;
					size_type last_bucket_size = static_cast<size_t>(bucket_size(bucket_count() - 1));

					if (last_bucket_size > to_remove) {
						//the last bucket has enough space: just resize it
						d_buckets.back()->resize(static_cast<cbuffer_pos>(last_bucket_size - to_remove), get_allocator());
					}
					else {
						//remove last bucket
						remove_back_bucket();

						to_remove -= last_bucket_size;

						//dealloc buckets starting to the last one
						while (to_remove >= static_cast<size_type>(bucket_size())) {
							to_remove -= static_cast<size_t>(bucket_size(bucket_count() - 1));
							remove_back_bucket();
						}

						//resize last bucket
						d_buckets.back()->resize(bucket_size(bucket_count() - 1) - static_cast<cbuffer_pos>(to_remove), get_allocator());
					}
					d_size = size;
				}
				update_all_back_values();
			
			}

			void resize_front(size_type size)
			{
				if (size == d_size)
					return;

				if (size > d_size) {
					ensure_has_bucket();

					//add missing buckets
					size_type missing = size - d_size;
					size_type first_bucket_rem = static_cast<size_t>(bucket_size() - d_buckets.front()->size);

					if (missing < first_bucket_rem) {
						// The front bucket has enough space: just resize it
						// Might throw, fine
						d_buckets.front().bucket->push_front_n(get_allocator(), static_cast<cbuffer_pos>( missing));
						d_size += missing;
					}
					else {
						//resize front bucket
						d_buckets.front().bucket->push_front_n(get_allocator(), static_cast<cbuffer_pos>(first_bucket_rem));

						missing -= first_bucket_rem;
						d_size += first_bucket_rem;

						size_type new_bucket_count = missing / static_cast<size_t>(bucket_size());
						cbuffer_pos first_bucket_size = static_cast<cbuffer_pos>(missing % static_cast<size_t>(bucket_size()));

						try {
							//add full buckets
							for (size_type i = 0; i < new_bucket_count; ++i) {
								create_front_bucket()->resize(bucket_size(), get_allocator());
								d_size += static_cast<size_t>(bucket_size());

							}

							//add last
							if (first_bucket_size) {
								create_front_bucket()->push_front_n(get_allocator(), first_bucket_size);
								d_size += static_cast<size_t>(first_bucket_size);
							}
						}
						catch (...) {
							if (d_buckets.size() > 1 && d_buckets[0]->size == 0)
								remove_front_bucket();
							throw;
						}
					}

				}
				else {
					size_type to_remove = d_size - size;
					size_type first_bucket_size = static_cast<size_t>(bucket_size(0));

					if (first_bucket_size > to_remove) {
						//the last bucket has enough space: just resize it
						d_buckets.front().bucket->pop_front_n(get_allocator(), static_cast<cbuffer_pos>(to_remove));
						d_size -= to_remove;
					}
					else {
						//remove last bucket
						remove_front_bucket();

						to_remove -= first_bucket_size;
						d_size -= first_bucket_size;

						//dealloc buckets starting to the last one
						while (to_remove >= static_cast<size_type>(bucket_size())) {
							size_type s = static_cast<size_t>(bucket_size(0));
							to_remove -= s;
							remove_front_bucket();
							d_size -= s;
						}

						//resize front bucket
						d_buckets.front().bucket->pop_front_n(get_allocator(), static_cast<cbuffer_pos>(to_remove));
						d_size -= to_remove;
					}
				}

				update_all_back_values();
			}
			void resize_front(size_type size, const T& val)
			{
				if (size == d_size)
					return;

				if (size > d_size) {
					ensure_has_bucket();

					//add missing buckets
					size_type missing = size - d_size;
					size_type first_bucket_rem = static_cast<size_t>(bucket_size() - d_buckets.front()->size);

					if (missing < first_bucket_rem) {
						//the front bucket has enough space: just resize it
						d_buckets.front()->push_front_n(get_allocator(), static_cast<cbuffer_pos>(missing), val);
						d_size += missing;
					}
					else {
						//resize front bucket
						d_buckets.front()->push_front_n(get_allocator(), static_cast<cbuffer_pos>(first_bucket_rem), val);

						missing -= first_bucket_rem;
						d_size += first_bucket_rem;

						size_type new_bucket_count = missing / static_cast<size_t>(bucket_size());
						cbuffer_pos first_bucket_size = static_cast<cbuffer_pos>(missing % static_cast<size_t>(bucket_size()));

						try {
							//add full buckets
							for (size_type i = 0; i < new_bucket_count; ++i) {
								create_front_bucket(val);
								d_size += static_cast<size_t>(bucket_size());
							}

							//add last
							if (first_bucket_size) {
								create_front_bucket()->push_front_n(get_allocator(), first_bucket_size, val);
								d_size += static_cast<size_t>(first_bucket_size);
							}
						}
						catch (...) {
							if (d_buckets.size() > 1 && d_buckets[0]->size == 0)
								remove_front_bucket();
							throw;
						}
					}

				}
				else {
					size_type to_remove = d_size - size;
					size_type first_bucket_size = static_cast<size_t>(bucket_size(0));

					if (first_bucket_size > to_remove) {
						//the last bucket has enough space: just resize it
						d_buckets.front()->pop_front_n(get_allocator(), static_cast<cbuffer_pos>(to_remove));
						d_size -= to_remove;
					}
					else {
						//remove last bucket
						remove_front_bucket();

						to_remove -= first_bucket_size;
						d_size -= first_bucket_size;

						//dealloc buckets starting to the last one
						while (to_remove >= static_cast<size_type>(bucket_size())) {
							size_type s = static_cast<size_type>(bucket_size(0));
							to_remove -= s;
							remove_front_bucket();
							d_size -= s;
						}

						//resize front bucket
						d_buckets.front()->pop_front_n(get_allocator(), static_cast<cbuffer_pos>(to_remove));
						d_size -= to_remove;
					}
				}
				update_all_back_values();
			}

		};

		template<class T>
		struct NullValueCompare
		{
			using key_type = T;
			static auto key(T& v) noexcept -> T& { return v; }
			static auto key(const T& v) noexcept -> const T& { return v; }
		};
	} //end detail




	/// @brief seq::tiered_vector is a std::deque like container implemented as a tiered-vector.
	/// @tparam T value type
	/// @tparam Allocator allocator type
	/// @tparam MinBSize minimum bucket size, depends on the value_type size
	/// @tparam MaxBSize maximum bucket size
	/// @tparam FindBSize struct used to find the bucket size for a given tiered_vector size
	/// @tparam ValueCompare internal use only
	/// 
	/// 
	/// seq::tiered_vector is a random-access, bucket based container providing a similar interface to std::deque.
	/// Its internals are however very different as it is implemented as a <a href="http://cs.brown.edu/cgc/jdsl/papers/tiered-vector.pdf">tiered vector</a>.
	/// Instead of maintaining a vector of fixed size buckets, seq::tiered_vector uses a bucket size
	/// close to sqrt(size()). The bucket size is always a power of 2 for fast division and modulo.
	/// Furtheremore, the bucket is not a linear buffer but is instead implemented as a circular buffer.
	/// This allows a complexity of O(sqrt(N)) for insertion and deletion in the middle of the tiered_vector
	/// instead of O(N) for std::deque. Inserting and deleting elements at both ends is still O(1).
	/// 
	/// seq::tiered_vector internally uses seq::devector to store the buckets.
	/// seq::tiered_vector is used as the backend container for seq::flat_set, seq::flat_map, seq::flat_multiset and seq::flat_multimap.
	/// 
	/// 
	/// Interface
	/// ---------
	/// 
	/// seq::tiered_vector interface is the same as std::deque, with the additional members:
	///		-	for_each() providing a faster way to walk through the tiered_vector than iterators,
	///		-	resize_front() to resize the tiered_vector from the front instead of the back of the container,
	/// 
	/// 
	/// Bucket managment
	/// ----------------
	/// 
	/// The seq::tiered_vector maintains internally an array of circular buffers (or buckets). At any moment,
	/// all buckets have the same size which is a power of 2. At container initialization, the bucket size
	/// is given by template parameter \a MinBSize which is, by default, between 64 and 8 depending on value_type size.
	/// Whenever seq::tiered_vector grows (through #push_back(), #push_front(), #insert(), #resize() ...), the new bucket size 
	/// is computed using the template parameter \a FindBSize. \a FindBSize must provide the member
	/// \code{.cpp}
	///  unsigned  FindBSize::operator() (size_t size, unsigned MinBSize, unsigned MaxBSize) const noexcept ;
	/// \endcode
	/// returning the new bucket size based on the container size, the minimum and maximum bucket size. Default implementation
	/// returns a value close to sqrt(size()) rounded up to the next power of 2.
	/// 
	/// If the new bucket size is different than the current one, new buckets are created and elements from the old
	/// buckets are moved to the new ones. This has the practical consequence to <b> invalidate all iterators and references 
	/// on growing or shrinking </b>, as opposed to std::deque which maintains references when inserting/deleting at both ends.
	/// 
	/// 
	/// Inserting and deleting elements
	/// -------------------------------
	/// 
	/// Inserting or deleting elements at the back or the front behaves the same way as for std::deque, except if the bucket
	/// size is updated (as explained above).
	/// 
	/// Inerting an element in the middle of seq::tiered_vector follows these steps:
	///		- The bucket index and the element position within the bucket are first computed
	///		- The back element of the bucket is extracted and removed from the bucket
	///		- The new element is inserted at the right position. Since the bucket is implemented as a dense
	///		circular buffer, at most half of the bucket elements must be moved (toward the left or the right,
	///		whichever is the shortest path)
	///		- The back value that was previously removed is inserted at the front of the next bucket
	///		- The next bucket back value is extracted and inserted at the front of the following bucket
	///		- ....
	///		- And so forth until we reach the last bucket. 
	/// 
	/// This operation of <em>insert front/pop back</em> is very fast on a circular buffer as it involves only two element moves
	/// and shifting the begin index of the buffer. If the bucket size is exactly sqrt(size()), inserting an element in the middle
	/// performs in O(sqrt(N)) as it involves as many element moves within a single bucket than between buckets.
	/// 
	/// In practice the buckets size should be greater than sqrt(size()) as moving elements within a bucket is much faster than
	/// between buckets due to cache locality.
	/// 
	/// Note that, depending on the insertion location, elements can be shifted toward the front bucket instead of the back one
	/// if this is the shortest path. This practically divides by 2 (on average) the number of moved elements.
	/// 
	/// Erasing an element in the middle follows the exact same logic.
	/// 
	/// Note that inserting/removing relocatable types (where seq::is_relocatable<T>::value is true) is faster than for other types.
	/// 
	/// 
	/// Exception guarantee
	/// -------------------
	/// 
	/// All insertion/deletion operations on a seq::tiered_vector are much more complex than for a std::deque. Especially,
	/// each operation might change the bucket size, and therefore trigger the allocation of new buckets plus moving
	/// all elements within the new buckets.
	/// 
	/// Although possible, providing strong exception guarantee on seq::tiered_vector operations would have added a very complex layer
	/// hurting its performances. Therefore, all seq::tiered_vector operations only provide <b>basic exception guarantee</b>.
	/// 
	/// 
	/// Iterators and references invalidation
	/// -------------------------------------
	/// 
	/// As explained above, all seq::tiered_vector operations invalidate iterators and references, except for swapping two seq::tiered_vector.
	/// 
	/// The only exception is when providing a minimum bucket size (\a MinBSize) equals to the maximum bucket size (\a MaxBSize).
	/// In this case, inserting/deleting elements will <em>never</em> change the bucket size and move all elements within
	/// new buckets. This affects the members #emplace_back(), #push_back(), #emplace_front() and #push_front() that provide the
	/// same invalidation rules as for std::deque.
	/// 
	/// 
	/// Performances
	/// ------------
	/// 
	/// seq::tiered_vector was optimized to match libstdc++ std::deque performances as close as possible.
	/// My benchmarhs show that most members are actually faster than libstdc++ std::deque, except
	/// for #push_back(), #push_front(), #pop_back() and #pop_front() which are slightly slower 
	/// due to the need to move all elements when the bucket size changes. This can be alievated 
	/// by the \a OptimizeForSpeed flag that makes both operations as fast as their std::deque counterparts (see 'seq/seq/benchs/bench_tiered_vector.hpp' for more details).
	/// 
	/// Usually, iterating through a seq::tiered_vector is faster than through a std::deque, and the random-access
	/// #operator[](size_t) is also faster. Making a lot of random access based on iterators can be slightly slower with seq::tiered_vector 
	/// depending on the use case. For instance, sorting a seq::tiered_vector is slower than sorting a std::deque.
	/// 
	/// Inserting/deleting single elements in the middle of a seq::tiered_vector is several order of magnitudes faster than std::deque
	/// due to the tiered-vector implementation.
	/// 
	/// seq::tiered_vector is faster when working with relocatable types (where seq::is_relocatable<T>::value == true).
	/// 
	/// The standard conlusion is: you should always benchmark with your own use cases.
	/// 
	template<
		class T,											///! value type
		class Allocator = std::allocator<T>,				///! allocator type
		unsigned MinBSize = SEQ_MIN_BUCKET_SIZE(T),			///! minimum bucket size, depends on the value_type size
		unsigned MaxBSize = SEQ_MAX_BUCKET_SIZE,			///! maximum bucket size, fixed to SEQ_MAX_BUCKET_SIZE
		class FindBSize = detail::FindBucketSize<T>,		///! struct used to find the bucket size for a given tiered_vector size
		bool StoreBackValues = false,						///! Internal parameter used by flat_*map classes
		class ValueCompare = detail::NullValueCompare<T>	///! Internal parameter used by flat_*map classes
	>
	class tiered_vector : private Allocator
	{
		template< class U>
		using RebindAlloc = typename std::allocator_traits<Allocator>::template rebind_alloc<U>;

	public:
		 
		enum {
			min_block_size = static_cast<detail::cbuffer_pos>(MinBSize),
			max_block_size = MaxBSize > SEQ_MAX_BUCKET_SIZE ? static_cast<detail::cbuffer_pos>(SEQ_MAX_BUCKET_SIZE) : static_cast<detail::cbuffer_pos>(MaxBSize)
		};
		static_assert(((min_block_size - 1)& min_block_size) == 0, "Minimum block size must be a power of 2");
		static_assert(((max_block_size - 1)& max_block_size) == 0, "Maximum block size must be a power of 2");
		static_assert(min_block_size > 0, "Invalid min block size");
		static_assert(max_block_size >= min_block_size, "Invalid max block size");

		using bucket_manager = detail::BucketManager<T, Allocator, min_block_size, max_block_size, StoreBackValues, ValueCompare>;
		using alloc_traits = std::allocator_traits<Allocator>;
		using this_type = tiered_vector<T, Allocator, min_block_size, max_block_size, FindBSize, StoreBackValues, ValueCompare>;
		using find_bsize_type = FindBSize;

		using value_type = T;
		using allocator_type = Allocator;
		using size_type = typename alloc_traits::size_type;
		using difference_type = typename alloc_traits::difference_type;
		using pointer = typename alloc_traits::pointer;
		using const_pointer = typename alloc_traits::const_pointer;
		using reference = value_type&;
		using const_reference = const value_type&;
		using iterator = detail::deque_iterator<bucket_manager>;
		using const_iterator = detail::deque_const_iterator<bucket_manager>;
		using reverse_iterator = std::reverse_iterator<iterator>;
		using const_reverse_iterator = std::reverse_iterator<const_iterator>;
		using value_compare = ValueCompare;
	private:

		// Create a BucketManager object
		template< class... Args >
		auto make_manager(const Allocator& al, Args&&... args) -> bucket_manager* {
			RebindAlloc< bucket_manager> alloc = al;
			bucket_manager* res = alloc.allocate(1);
			try {
				construct_ptr(res, std::forward<Args>(args)...);
			}
			catch (...) {
				alloc.deallocate(res, 1);
			}
			return res;
		}
		// Destroy/deallocate a BucketManager object
		void destroy_manager(bucket_manager* manager) noexcept {
			if (manager) {
				RebindAlloc< bucket_manager> alloc = manager->get_allocator();
				destroy_ptr(manager);
				alloc.deallocate(manager, 1);
			}
		}

		// Make the manager a pointer to keep iterator validity in case of swap
		bucket_manager* d_manager;
	
		// Update the bucket size if necessary, used when inserting/deleting single elements
		SEQ_ALWAYS_INLINE void update_bucket_size() {
			static constexpr size_t minb = static_cast<size_t>(min_block_size);
			static constexpr size_t maxb = static_cast<size_t>(max_block_size);
			static constexpr size_t mul_factor = (minb < 8U) ? 8U : minb;
			static constexpr size_t mask = (mul_factor * mul_factor) - 1ULL;
			// Check the bucket size every (min_block_size * min_block_size) insertions/deletions
			if (SEQ_UNLIKELY(min_block_size != maxb && /*manager()->isPow2Size()*/(size() < 64U || (size() & mask)==0)))
				check_bucket_size();
		}
		// Find bucket size based on full tiered_vector size
		auto findBSize(size_type size) const noexcept -> detail::cbuffer_pos {
			return FindBSize()(size, static_cast<detail::cbuffer_pos>(min_block_size), static_cast<detail::cbuffer_pos>(max_block_size));
		}
		// Force the bucket size
		void set_bucket_size(int bsize) {
			if (bsize != manager()->bucket_size()) {
				bucket_manager* tmp = make_manager(get_allocator(), std::move(*manager()), bsize, 0U, static_cast<size_t>(-1), get_allocator());
				destroy_manager(d_manager);
				d_manager = tmp;
			}
		}
		// Check the bucket size, set the new one if necessary
		void check_bucket_size()
		{
			detail::cbuffer_pos bucket_size = (findBSize(size()));
			if (bucket_size != manager()->bucket_size()) {
				set_bucket_size(bucket_size);
			}
		}
	
		// Insert range for non random-access iterators
		template<class Iter, class Cat>
		void insert_cat(size_type pos, Iter first, Iter last, Cat /*unused*/) {
			SEQ_ASSERT_DEBUG(pos <= size(), "invalid insert position");
			if (first == last)
				return;

			if (pos < size() / 2) {
				//push front values
				size_type prev_size = size();
				// Might throw, fine
				for (; first != last; ++first)
					push_front(*first);

				size_type num = size() - prev_size;
				// Might throw, fine
				std::reverse(begin(), begin() + num); // flip new stuff in place
				std::rotate(begin(), begin() + num, begin() + (num + pos));
			}
			else {
				// push back, might throw
				size_type prev_size = size();
				for (; first != last; ++first)
					push_back(*first);
				// Might throw, fine
				std::rotate(begin() + pos, begin() + prev_size, end());
			}
		}
		// Insert range for random-access iterators
		template<class Iter>
		void insert_cat(size_type pos, Iter first, Iter last, std::random_access_iterator_tag  /*unused*/) {
			SEQ_ASSERT_DEBUG(pos <= size(), "invalid insert position");
			if (first == last)
				return;

			if (pos < size() / 2) {
				size_type to_insert = static_cast<size_t>(last - first);
				// Might throw, fine
				resize_front(size() + to_insert);
				iterator beg = begin();
				// Might throw, fine
				manager()->for_each(to_insert, to_insert + pos, [&](T& v) {*beg = std::move(v); ++beg; });
				manager()->for_each(pos, pos + to_insert, [&](T& v) {v = *first++; });
			}
			else {
				// Might throw, fine
				size_type to_insert = static_cast<size_t>(last - first);
				resize(size() + to_insert);
				std::move_backward(begin() + static_cast<difference_type>(pos), begin() + static_cast<difference_type>(size() - to_insert), end());
				std::copy(first, last, begin() + static_cast<difference_type>(pos));
			}
		}

		// Assign range for non random-access iterators
		template<class Iter, class Cat>
		void assign_cat(Iter first, Iter last, Cat /*unused*/) {
			iterator it = begin();
			iterator en = end();
			while (it != en && first != last) {
				*it = *first;
				++it;
				++first;
			}
			size_type count = it.absolutePos();
			while (first != last) {
				push_back(*first);
				++first;
				++count;
			}
			resize(count);
		}
		// Assign range for random-access iterators
		template<class Iter>
		void assign_cat(Iter first, Iter last, std::random_access_iterator_tag /*unused*/) {
			size_type count = static_cast<size_t>(last - first);
			if (size() != count && size() > 0 && findBSize(count) != manager()->bucket_size()) {
				// Create an empty manager, might throw
				bucket_manager* tmp = make_manager(get_allocator(), findBSize(count), get_allocator());
				// Destroy all objects from current manager
				d_manager->destroy_all();
				// Move, no throw
				*d_manager = std::move(*tmp);
				// Destroy tmp
				destroy_manager(tmp);
				// Resize, might throw, fine
				d_manager->resize(count);
			
			}
			else
				resize(count);
			manager()->for_each(0, size(), [&](T& v) {v = *first; ++first; });
		}

		SEQ_ALWAYS_INLINE void make_manager_if_null()
		{
			if (SEQ_UNLIKELY(!d_manager))
				d_manager = make_manager(get_allocator(), min_block_size, get_allocator());
		}

	public:

		/// @brief Default constructor, initialize the internal bucket manager.
		tiered_vector() noexcept(std::is_nothrow_default_constructible<Allocator>::value)
			:Allocator(), d_manager(nullptr)
		{
		}
		/// @brief Constructs an empty container with the given allocator alloc.
		/// @param alloc allocator object
		explicit tiered_vector(const Allocator& alloc)
			:Allocator(alloc), d_manager(make_manager(alloc, min_block_size, alloc))
		{
		}
		/// @brief Constructs the container with \a count copies of elements with value \a value.
		/// @param count new tiered_vector size
		/// @param value the value to initialize elements of the container with
		/// @param alloc allocator object 
		tiered_vector(size_type count,const T& value ,const Allocator& alloc = Allocator())
			:Allocator(alloc), d_manager(make_manager(alloc, min_block_size, alloc))
		{
			resize(count, value);
		}
		/// @brief Constructs the container with count default-inserted instances of T. No copies are made.
		/// @param count new tiered_vector size
		/// @param alloc allocator object
		explicit tiered_vector(size_type count, const Allocator& alloc = Allocator())
			:Allocator(alloc), d_manager(make_manager(alloc, min_block_size, alloc))
		{
			resize(count);
		}
		/// @brief Copy constructor. Constructs the container with the copy of the contents of other.
		/// @param other another container to be used as source to initialize the elements of the container with
		tiered_vector(const tiered_vector& other)
			:Allocator(copy_allocator(other.get_allocator())), d_manager(nullptr)
		{
			if (other.size())
				d_manager = make_manager(get_allocator(), *other.manager(), other.manager()->bucket_size(), 0U, static_cast<size_t>(-1), get_allocator());
		}
		/// @brief Constructs the container with the copy of the contents of other, using alloc as the allocator.
		/// @param other other another container to be used as source to initialize the elements of the container with
		/// @param alloc allcoator object
		tiered_vector(const tiered_vector& other, const Allocator & alloc)
			:Allocator(alloc), d_manager(nullptr)
		{
			if (other.size())
				d_manager = make_manager(get_allocator(), *other.manager(), other.manager()->bucket_size(), 0U, static_cast<size_t>(-1), get_allocator());
		}
		/// @brief Move constructor. Constructs the container with the contents of other using move semantics. Allocator is obtained by move-construction from the allocator belonging to other.
		/// @param other another container to be used as source to initialize the elements of the container with
		tiered_vector(tiered_vector&& other) noexcept(std::is_nothrow_move_constructible<Allocator>::value)
			:Allocator(std::move(other.get_allocator())), d_manager(other.manager())
		{
			other.d_manager = nullptr;
		}
		/// @brief  Allocator-extended move constructor. Using alloc as the allocator for the new container, moving the contents from other; if alloc != other.get_allocator(), this results in an element-wise move.
		/// @param other another container to be used as source to initialize the elements of the container with
		/// @param alloc allocator object
		tiered_vector(tiered_vector&& other, const Allocator & alloc) 
			:Allocator(alloc), d_manager(make_manager(alloc, min_block_size, alloc))
		{
			if (alloc == other.get_allocator()) {
				std::swap(d_manager, other.d_manager);
			}
			else {
				resize(other.size());
				std::move(other.begin(), other.end(), begin());
			}
		}
		/// @brief Constructs the container with the contents of the initializer list \a init.
		/// @param lst initializer list
		/// @param alloc allocator object
		tiered_vector(const std::initializer_list<T>& lst, const Allocator & alloc = Allocator())
			:Allocator(alloc), d_manager(make_manager(alloc, min_block_size, alloc))
		{
			assign(lst.begin(), lst.end());
		}
		/// @brief  Constructs the container with the contents of the range [first, last).
		/// @tparam Iter iterator type
		/// @param first first iterator of the range
		/// @param last last iterator of the range
		/// @param alloc allocator object
		template< class Iter>
		tiered_vector(Iter first, Iter last, const Allocator& alloc = Allocator())
			: Allocator(alloc), d_manager(make_manager(alloc, min_block_size, alloc))
		{
			assign(first, last);
		}

		/// @brief  Destructor
		~tiered_vector() 
		{
			destroy_manager(manager());
		}

		/// @brief Move assignment operator.
		/// @param other another container to use as data source
		/// @return reference to this
		auto operator=(tiered_vector&& other) noexcept(noexcept(std::declval< tiered_vector&>().swap(std::declval< tiered_vector&>()))) -> tiered_vector&
		{
			this->swap(other);
			return *this;
		}

		/// @brief Copy assignment operator.
		/// @param other another container to use as data source
		/// @return reference to this
		auto operator=(const tiered_vector& other) -> tiered_vector& 
		{
			if (this != std::addressof(other)) 
			{
				if SEQ_CONSTEXPR(assign_alloc<Allocator>::value)
				{
					if (get_allocator() != other.get_allocator()) {
						destroy_manager(d_manager);
						d_manager = nullptr;
					}
				}
				assign_allocator(get_allocator(), other.get_allocator());

				if (other.size() == 0)
					clear();
				else {
					bucket_manager* tmp = (make_manager(get_allocator(), *other.manager(), other.manager()->bucket_size(), 0U, static_cast<size_t>(-1), get_allocator()));
					destroy_manager(d_manager);
					d_manager = tmp;
				}
			}
			return *this;
		}


		/// @brief Returns the internal bucket manager object.
		auto manager() noexcept -> bucket_manager* { return d_manager; }
		/// @brief Returns the internal bucket manager object.
		auto manager() const noexcept -> const bucket_manager* { return d_manager; }


		/// @brief Returns the container size.
		auto size() const noexcept -> size_type
		{
			return d_manager ? d_manager->size() : 0;
		}
		/// @brief Returns the container maximum size.
		auto max_size() const noexcept -> size_type
		{
			return std::numeric_limits<difference_type>::max();
		}
		/// @brief Returns the number of buckets within tiered_vector.
		auto bucket_count() const noexcept -> size_type 
		{
			return d_manager ? manager()->buckets().size() : 0;
		}
		/// @brief Returns the size of a bucket within tiered_vector.
		auto bucket_size() const noexcept -> size_type 
		{ 
			return d_manager ? manager()->bucket_size() : 0; 
		}
		/// @brief Retruns true if the container is empty, false otherwise.
		auto empty() const noexcept -> bool 
		{
			return !d_manager || d_manager->size() == 0;
		}
		/// @brief Returns the allocator associated with the container.
		auto get_allocator() const noexcept -> const Allocator& 
		{
			return static_cast<const Allocator&>(*this);
		}
		/// @brief Returns the allocator associated with the container.
		auto get_allocator() noexcept -> Allocator&  
		{
			return static_cast<Allocator&>(*this);
		}
		/// @brief Exchanges the contents of the container with those of other. Does not invoke any move, copy, or swap operations on individual elements.
		/// @param other other sequence to swap with
		/// All iterators and references remain valid.
		/// An iterator holding the past-the-end value in this container will refer to the other container after the operation.
		void swap(tiered_vector& other) noexcept (noexcept(swap_allocator(std::declval<Allocator&>(), std::declval<Allocator&>())))
		{
			if (this != std::addressof(other)) {
				std::swap(d_manager, other.d_manager);
				swap_allocator(get_allocator(), other.get_allocator());
			}
		}
	
		/// @brief Resizes the container to contain count elements.
		/// @param count new size of the container
		/// If the current size is greater than count, the container is reduced to its first count elements.
		/// If the current size is less than count, additional default-inserted elements are appended.
		/// Basic exception guarantee.
		void resize(size_type count)
		{
			if (count == this->size())
				return; // No-op
			else if (count == 0)
				clear();
			else {
				make_manager_if_null();
				detail::cbuffer_pos bucket_size = findBSize(count);
				// Update bucket size if necessary
				if (bucket_size != manager()->bucket_size()) {
					bucket_manager* tmp = make_manager(get_allocator(), std::move(*manager()), bucket_size, 0U,std::min(count, this->size()), get_allocator());
					*d_manager = std::move(*tmp);
					destroy_manager(tmp);
				}
				manager()->resize(count);
			}
		}

		/// @brief Resizes the container to contain count elements.
		/// @param count new size of the container
		/// @param value the value to initialize the new elements with
		/// If the current size is greater than count, the container is reduced to its first count elements.
		/// If the current size is less than count, additional copies of value are appended.
		/// Basic exception guarantee.
		void resize(size_type count, const T & value)
		{
			if (count == this->size())
				return; // No-op
			else if (count == 0)
				clear();
			else {
				make_manager_if_null();
				int bucket_size = findBSize(count);
				// Update bucket size if necessary
				if (bucket_size != manager()->bucket_size()) {
					bucket_manager* tmp = make_manager(get_allocator(), std::move(*manager()), bucket_size, 0U, std::min(count,this->size()), get_allocator());
					*d_manager = std::move(*tmp);
					destroy_manager(tmp);
				}
				manager()->resize(count, value);
			}
		}

		/// @brief Resizes the container to contain count elements.
		/// @param count new size of the container
		/// If the current size is greater than count, the container is reduced to its last count elements.
		/// If the current size is less than count, additional default-inserted elements are prepended.
		/// Basic exception guarantee.
		void resize_front(size_type count)
		{
			if (count == this->size())
				return;// No-op
			else if (count == 0)
				clear();
			else {
				make_manager_if_null();
				detail::cbuffer_pos bucket_size = findBSize(count);
				// Update bucket size if necessary
				if (bucket_size != manager()->bucket_size()) {
					size_type fsize = std::min(count, this->size());
					size_type fstart = static_cast<size_t>(std::max(static_cast<difference_type>(this->size()) - static_cast<difference_type>(fsize), static_cast<difference_type>(0)));
					bucket_manager* tmp = make_manager(get_allocator(), std::move(*manager()), bucket_size, fstart, fsize, get_allocator());
					*d_manager = std::move(*tmp);
					destroy_manager(tmp);
				}
				manager()->resize_front(count);
			}
		}

		/// @brief Resizes the container to contain count elements.
		/// @param count new size of the container
		/// @param value the value to initialize the new elements with
		/// If the current size is greater than count, the container is reduced to its last count elements.
		/// If the current size is less than count, additional copies of value are prepended.
		/// Basic exception guarantee.
		void resize_front(size_type count, const T& value)
		{
			if (count == this->size())
				return;// No-op
			else if (count == 0)
				clear();
			else {
				make_manager_if_null();
				int bucket_size = findBSize(count);
				// Update bucket size if necessary
				if (bucket_size != manager()->bucket_size()) {
					size_type fsize = std::min(count, this->size());
					size_type fstart = static_cast<size_t>(std::max(static_cast<difference_type>(this->size()) - static_cast<difference_type>(fsize), static_cast<difference_type>(0)));
					bucket_manager* tmp = make_manager(get_allocator(), std::move(*manager()), bucket_size, fstart, fsize, get_allocator());
					*d_manager = std::move(*tmp);
					destroy_manager(tmp);
				}
				manager()->resize_front(count, value);
			}
		}

		/// @brief Clear the container.
		void clear() noexcept
		{
			if (size() == 0)
				return;

			destroy_manager(d_manager);
			d_manager = nullptr;
		}

		/// @brief Appends the given element value to the end of the container.
		/// @param value the value of the element to append
		/// Basic exception guarantee, except if <em>MinBSize == MaxBSize</em> (strong guarantee in this case).
		void push_back(const T& value) 
		{
			make_manager_if_null();
			update_bucket_size();
			manager()->push_back(value);
		}

		/// @brief Appends the given element value to the end of the container using move semantic.
		/// @param value the value of the element to append
		/// Basic exception guarantee, except if <em>MinBSize == MaxBSize</em> (strong guarantee in this case).
		void push_back(T&& value)
		{
			make_manager_if_null();
			update_bucket_size();
			manager()->push_back(std::move(value));
		}

		/// @brief Appends a new element to the end of the container
		/// @tparam ...Args 
		/// @param ...args T constructor arguments
		/// @return reference to inserted element
		/// Basic exception guarantee, except if <em>MinBSize == MaxBSize</em> (strong guarantee in this case).
		template< class... Args >
		auto emplace_back(Args&&... args) -> T&
		{
			make_manager_if_null();
			update_bucket_size();
			return manager()->emplace_back(std::forward<Args>(args)...);
		}

		/// @brief Appends the given element value to the beginning of the container.
		/// @param value the value of the element to append
		/// Basic exception guarantee, except if <em>MinBSize == MaxBSize</em> (strong guarantee in this case).
		void push_front(const T& value)
		{
			make_manager_if_null();
			update_bucket_size();
			manager()->push_front(value);
		}

		/// @brief Appends the given element value to the beginning of the container using move semantic.
		/// @param value the value of the element to append
		/// Basic exception guarantee, except if <em>MinBSize == MaxBSize</em> (strong guarantee in this case).
		void push_front(T&& value)
		{
			make_manager_if_null();
			update_bucket_size();
			manager()->push_front(std::move(value));
		}

		/// @brief Appends a new element to the beginning of the container
		/// @tparam ...Args 
		/// @param ...args T constructor arguments
		/// @return reference to inserted element
		/// Basic exception guarantee, except if <em>MinBSize == MaxBSize</em> (strong guarantee in this case).
		template< class... Args >
		auto emplace_front(Args&&... args) -> T&
		{
			make_manager_if_null();
			update_bucket_size();
			return manager()->emplace_front(std::forward<Args>(args)...);
		}

		/// @brief Insert \a value before \a pos
		/// @param pos absolute position within the tiered_vector
		/// @param value element to insert
		/// Basic exception guarantee.
		void insert(size_type pos, const T& value)
		{
			make_manager_if_null();
			update_bucket_size();
			manager()->insert(pos, value);
		}

		/// @brief Insert \a value before \a pos using move semantic.
		/// @param pos absolute position within the tiered_vector
		/// @param value element to insert
		/// Basic exception guarantee.
		void insert(size_type pos, T&& value)
		{
			make_manager_if_null();
			update_bucket_size();
			manager()->insert(pos, std::move(value));
		}

		/// @brief Insert \a value before \a it
		/// @param it iterator within the tiered_vector
		/// @param value element to insert
		/// Basic exception guarantee.
		auto insert(const_iterator it, const T& value) -> iterator
		{
			size_type pos = it.absolutePos();
			insert(pos, value);
			return iterator_at(pos);
		}

		/// @brief Insert \a value before \a it using move semantic.
		/// @param it iterator within the tiered_vector
		/// @param value element to insert
		/// Basic exception guarantee.
		auto insert(const_iterator it, T&& value) -> iterator
		{
			size_type pos = it.absolutePos();
			insert(pos, std::move(value));
			return iterator_at(pos);
		}

		/// @brief Inserts a new element into the container directly before \a pos.
		/// @tparam ...Args 
		/// @param pos absolute position within the tiered_vector
		/// @param ...args T constructor arguments
		/// @return reference to inserted element
		/// Basic exception guarantee.
		template<class... Args>
		auto emplace(size_type pos, Args&&... args) -> T& 
		{
			make_manager_if_null();
			update_bucket_size();
			return manager()->insert(pos, std::forward<Args>(args)...);
		}
	
		/// @brief Inserts a new element into the container directly before \a pos.
		/// @tparam ...Args 
		/// @param pos iterator within the tiered_vector
		/// @param ...args T constructor arguments
		/// @return reference to inserted element
		/// Basic exception guarantee.
		template<class... Args>
		auto emplace(const_iterator pos, Args&&... args) -> T&
		{
			make_manager_if_null();
			update_bucket_size();
			return manager()->insert(pos.absolutePos(), std::forward<Args>(args)...);
		}

		/// @brief Inserts elements from range [first, last) before pos.
		/// @tparam Iter iterator type
		/// @param pos absolute position within the tiered_vector
		/// @param first first iterator of the range
		/// @param last last iterator of the range
		/// Basic exception guarantee.
		template<class Iter>
		void insert(size_type pos, Iter first, Iter last) 
		{
			make_manager_if_null();
			insert_cat(pos, first, last, typename std::iterator_traits<Iter>::iterator_category());
		}

		/// @brief Inserts elements from range [first, last) before it.
		/// @tparam Iter iterator type
		/// @param it iterator within the tiered_vector
		/// @param first first iterator of the range
		/// @param last last iterator of the range
		/// @return Iterator pointing to the first element inserted, or it if first==last
		/// Basic exception guarantee.
		template<class Iter>
		auto insert(const_iterator it, Iter first, Iter last) -> iterator
		{
			size_type pos = it.absolutePos();
			make_manager_if_null();
			insert_cat(pos, first, last,typename std::iterator_traits<Iter>::iterator_category());
			return iterator_at(pos);
		}

		/// @brief Inserts elements from initializer list ilist before pos.
		/// @return Iterator pointing to the first element inserted, or it if first==last.
		/// Basic exception guarantee.
		void insert(size_type pos, std::initializer_list<T> ilist)
		{
			return insert(pos, ilist.begin(), ilist.end());
		}

		/// @brief Inserts elements from initializer list ilist before pos.
		/// @return Iterator pointing to the first element inserted, or it if first==last.
		/// Basic exception guarantee.
		auto insert(const_iterator pos, std::initializer_list<T> ilist) -> iterator
		{
			return insert(pos, ilist.begin(), ilist.end());
		}
	
		/// @brief Inserts count copies of the value before pos
		/// Basic exception guarantee. 	
		void insert(size_type pos, size_type count, const T& value)
		{
			insert(pos, cvalue_iterator<T>(0, value), cvalue_iterator<T>(count, value));
		}

		/// @brief Inserts count copies of the value before pos
		/// @return Iterator pointing to the first element inserted, or it if count==0
		/// Basic exception guarantee.
		auto insert(const_iterator pos, size_type count, const T& value) -> iterator
		{
			return insert(pos, cvalue_iterator<T>(0, value), cvalue_iterator<T>(count, value));
		}
	
		/// @brief Removes the last element of the container.
		/// Calling pop_back on an empty container results in undefined behavior.
		/// Basic exception guarantee, except if <em>MinBSize == MaxBSize</em> (strong guarantee in this case).
		void pop_back() 
		{
			make_manager_if_null();
			update_bucket_size();
			manager()->pop_back();
		}

		/// @brief Removes the first element of the container.
		/// Calling pop_front on an empty container results in undefined behavior.
		/// Basic exception guarantee, except if <em>MinBSize == MaxBSize</em> (strong guarantee in this case).
		void pop_front() 
		{
			make_manager_if_null();
			update_bucket_size();
			manager()->pop_front();
		}

		/// @brief Erase element at given position.
		/// @param pos absolute position of the element to erase
		/// Basic exception guarantee.
		void erase(size_type pos) 
		{
			SEQ_ASSERT_DEBUG(pos < size(), "erase: invalid position");
			update_bucket_size();
			manager()->erase(pos);
		}

		/// @brief Erase element at given position.
		/// @param it iterator to the element to remove
		/// @return iterator following the last removed element
		/// Basic exception guarantee.
		auto erase(const_iterator it) -> iterator
		{
			update_bucket_size();
			size_type pos = it.absolutePos();
			manager()->erase(it.absolutePos());
			return iterator_at(pos) ;
		}

		/// @brief Removes the elements in the range [first, last).
		/// @param first absolute position of the first element to erase
		/// @param last absolute position of the last (excluded) element to erase
		/// Basic exception guarantee.
		void erase(size_type first, size_type last)
		{
			SEQ_ASSERT_DEBUG(first <= last, "erase: invalid positions");
			SEQ_ASSERT_DEBUG(last <= size(), "erase: invalid last position");
			if (first == last) return;
			size_type space_before = first;
			size_type space_after = size() - last;

			if (space_before < space_after) {
				std::move_backward(begin(), begin() + static_cast<difference_type>(first), begin() + static_cast<difference_type>(last));
				resize_front(size() - static_cast<size_t>(last - first));
			}
			else {
				//std::move(begin() + last, end(), begin() + first);
				iterator it = begin() + static_cast<difference_type>(first);
				manager()->for_each(last, size(), [&](T& v) {*it = std::move(v); ++it; });
				resize(size() - static_cast<size_t>(last - first));
			}
		}

		/// @brief Removes the elements in the range [first, last).
		/// @param first iterator to the first element to erase
		/// @param last iterator to the last (excluded) element to erase
		/// @return Iterator following the last removed element.
		/// Basic exception guarantee.
		auto erase(const_iterator first, const_iterator last) -> iterator
		{
			size_type f = first.absolutePos();
			erase(f, last.absolutePos());
			return iterator_at(f);
		}

		/// @brief Replaces the contents with \a count copies of value \a value
		/// Basic exception guarantee.
		void assign(size_type count, const T& value)
		{
			assign(cvalue_iterator<T>(0, value), cvalue_iterator<T>(count, value));
		}

		/// @brief Replaces the contents with copies of those in the range [first, last). The behavior is undefined if either argument is an iterator into *this.
		/// Basic exception guarantee.
		template< class Iter >
		void assign(Iter first, Iter last)
		{
			make_manager_if_null();
			assign_cat(first,last, typename std::iterator_traits<Iter>::iterator_category());
		}

		/// @brief Replaces the contents with the elements from the initializer list ilist.
		/// Basic exception guarantee.
		void assign(std::initializer_list<T> ilist)
		{
			assign(ilist.begin(), ilist.end());
		}

		/// @brief Apply unary function \a fun to all elements within the range [first,last).
		/// @tparam Fun unary functor type 
		/// @param first first element of the range to apply the functor \a fun
		/// @param last end of range
		/// @param fun unary functor of lambda function
		/// @return the unary functor (usefull for statefull functors). 
		/// Using for_each is faster than walking through iterators as it uses the internal
		/// knowledge on bucket layout to fasten step increments.
		template< class Fun>
		auto for_each(size_type first, size_type last, Fun fun) -> Fun {
			if(d_manager)
				return manager()->for_each(first, last, fun);
			return fun;
		}

		/// @brief Apply unary function \a fun to all elements within the range [first,last).
		/// @tparam Fun unary functor type 
		/// @param first first element of the range to apply the functor \a fun
		/// @param last end of range
		/// @param fun unary functor of lambda function
		/// @return the unary functor (usefull for statefull functors). 
		/// Using for_each is faster than walking through iterators as it uses the internal
		/// knowledge on bucket layout to fasten step increments.
		template< class Fun>
		auto for_each(size_type first, size_type last, Fun fun) const -> Fun {
			if(d_manager)
				return manager()->for_each(first, last, fun);
			return fun;
		}

		/// @brief Returns a reference to the element at specified location pos, with bounds checking.
		SEQ_ALWAYS_INLINE auto at(size_type pos) const -> const T&  {
			//random access
			if (pos >= size()) throw std::out_of_range("");
			return (manager()->at(pos));
		}
		/// @brief Returns a reference to the element at specified location pos, with bounds checking.
		SEQ_ALWAYS_INLINE auto at(size_type pos) -> T& 
		{
			//random access
			if (pos >= size()) throw std::out_of_range("");
			return (manager()->at(pos));
		}
		/// @brief Returns a reference to the element at specified location pos, without bounds checking.
		SEQ_ALWAYS_INLINE auto operator[](size_type pos) const noexcept -> const T& {
			//random access
			return (manager()->at(pos));
		}
		/// @brief Returns a reference to the element at specified location pos, without bounds checking.
		SEQ_ALWAYS_INLINE auto operator[](size_type pos) noexcept -> T& {
			//random access
			return (manager()->at(pos));
		}

		/// @brief Returns a reference to the last element in the container.
		SEQ_ALWAYS_INLINE auto back() noexcept -> T& { return manager()->back(); }
		/// @brief Returns a reference to the last element in the container.
		SEQ_ALWAYS_INLINE auto back() const noexcept -> const T& { return manager()->back(); }
		/// @brief Returns a reference to the first element in the container.
		SEQ_ALWAYS_INLINE auto front() noexcept -> T&  { return manager()->front(); }
		/// @brief Returns a reference to the first element in the container.
		SEQ_ALWAYS_INLINE auto front() const noexcept -> const T& { return manager()->front(); }

		/// @brief Returns an iterator to the first element of the tiered_vector.
		SEQ_ALWAYS_INLINE auto begin() const noexcept -> const_iterator { return const_iterator(manager(), 0); }
		/// @brief Returns an iterator to the first element of the tiered_vector.
		SEQ_ALWAYS_INLINE auto begin() noexcept -> iterator { return iterator(manager(), 0); }
		/// @brief Returns an iterator to the element following the last element of the tiered_vector.
		SEQ_ALWAYS_INLINE auto end() const noexcept -> const_iterator { return const_iterator(manager()); }
		/// @brief Returns an iterator to the element following the last element of the tiered_vector.
		SEQ_ALWAYS_INLINE auto end() noexcept -> iterator { return iterator(manager()); }
		/// @brief Returns a reverse iterator to the first element of the reversed list.
		SEQ_ALWAYS_INLINE auto rbegin() noexcept -> reverse_iterator { return reverse_iterator(end()); }
		/// @brief Returns a reverse iterator to the first element of the reversed list.
		SEQ_ALWAYS_INLINE auto rbegin() const noexcept -> const_reverse_iterator { return const_reverse_iterator(end()); }
		/// @brief Returns a reverse iterator to the element following the last element of the reversed list.
		SEQ_ALWAYS_INLINE auto rend() noexcept -> reverse_iterator { return reverse_iterator(begin()); }
		/// @brief Returns a reverse iterator to the element following the last element of the reversed list.
		SEQ_ALWAYS_INLINE auto rend() const noexcept -> const_reverse_iterator { return const_reverse_iterator(begin()); }
		/// @brief Returns an iterator to the first element of the tiered_vector.
		SEQ_ALWAYS_INLINE auto cbegin() const noexcept -> const_iterator { return begin(); }
		/// @brief Returns an iterator to the element following the last element of the tiered_vector.
		SEQ_ALWAYS_INLINE auto cend() const noexcept -> const_iterator { return end(); }
		/// @brief Returns a reverse iterator to the first element of the reversed list.
		SEQ_ALWAYS_INLINE auto crbegin() const noexcept -> const_reverse_iterator { return rbegin(); }
		/// @brief Returns a reverse iterator to the element following the last element of the reversed list.
		SEQ_ALWAYS_INLINE auto crend() const noexcept -> const_reverse_iterator { return rend(); }

		/// @brief Returns an iterator to given absolute position.
		/// This is slightly faster than begin()+pos.
		SEQ_ALWAYS_INLINE auto iterator_at(size_t pos) noexcept -> iterator 
		{
			return pos == size() ? end() : iterator(manager(),pos,pos);
		}
		/// @brief Returns a const_iterator to given absolute position.
		/// This is slightly faster than begin()+pos.
		SEQ_ALWAYS_INLINE auto iterator_at(size_t pos) const noexcept -> const_iterator
		{
			return pos == size() ? end() : const_iterator(manager(), pos, pos);
		}
	};




} //end namespace seq
#endif
