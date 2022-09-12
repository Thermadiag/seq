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

#ifndef SEQ_DEQUE_ALLOCATOR_HPP
#define SEQ_DEQUE_ALLOCATOR_HPP




#include "bits.hpp"
#include "utils.hpp"

#include <vector>

namespace seq
{

	namespace detail
	{


		/// Allocate tiered_vector buckets by continuous blocks
		template< class T, class Allocator>
		struct deque_chunk_pool
		{
			template< class U>
			using RebindAlloc = typename std::allocator_traits<Allocator>::template rebind_alloc<U>;

			size_t count;					//full capacity (number of list_chunk<T>)
			size_t used;					//number of used list_chunk<T>
			size_t elems_per_chunk;			// number of T elements per chunk
			size_t tail;					//index of first free chunk in case they are all full
			size_t total_T;
			Allocator allocator;			//usually std::allocator< list_chunk<T> >
			T* chunks;			//pointer to the chunk array
			T* first_free;		//first free chunk

			void set_next(T* p, T* next) {
				//copy address
				memcpy(p, &next, sizeof(void*));
			}
			auto next(T* p) -> T* {
				T* res;
				memcpy(&res, p, sizeof(void*));
				return res;
			}

			auto T_per_chunk() const noexcept -> size_t {
				return elems_per_chunk;
			}
			auto chunkAt(size_t i) noexcept -> T* {
				return chunks + i * T_per_chunk();
			}
			auto indexOfChunk(const T* ch) const noexcept -> size_t {
				return (ch - chunks) / T_per_chunk();
			}
			explicit deque_chunk_pool(const Allocator& al) noexcept
				: count(0), used(0), elems_per_chunk(0), tail(0), total_T(0), allocator(al), chunks(NULL), first_free(NULL) {}
			deque_chunk_pool(const Allocator& al, size_t elems, size_t elems_per_chunk)
				: count(0), used(0), elems_per_chunk(0), tail(0), total_T(0), allocator(al), chunks(NULL), first_free(NULL) {
				//resize(elems,elems_per_chunk);
				if (elems) {
					this->elems_per_chunk = elems_per_chunk;
					chunks = allocator.allocate(elems * T_per_chunk());
					count = elems;
					total_T = count * T_per_chunk();
					set_next(chunks, NULL);
				}
			}
			//disable copy, only move semantic allowed
			deque_chunk_pool(const deque_chunk_pool& other) = delete;
			auto operator=(const deque_chunk_pool& other) -> deque_chunk_pool& = delete;
			~deque_chunk_pool() noexcept {
				if (chunks) {
					//free array of dchunk<T>
					allocator.deallocate(chunks, total_T);
				}
			}
			deque_chunk_pool(deque_chunk_pool&& other) noexcept :count(0), used(0), elems_per_chunk(0), chunks(NULL), first_free(NULL) {
				std::swap(count, other.count);
				std::swap(used, other.used);
				std::swap(elems_per_chunk, other.elems_per_chunk);
				std::swap(tail, other.tail);
				std::swap(total_T, other.total_T);
				std::swap(allocator, other.allocator);
				std::swap(chunks, other.chunks);
				std::swap(first_free, other.first_free);
			}
			auto operator=(deque_chunk_pool&& other) noexcept -> deque_chunk_pool& {
				std::swap(count, other.count);
				std::swap(used, other.used);
				std::swap(elems_per_chunk, other.elems_per_chunk);
				std::swap(tail, other.tail);
				std::swap(total_T, other.total_T);
				std::swap(allocator, other.allocator);
				std::swap(chunks, other.chunks);
				std::swap(first_free, other.first_free);
				return *this;
			}

			auto rebind_for(size_t elems_per_chunk) -> bool
			{
				size_t required_T = elems_per_chunk;
				if (required_T > total_T)
					return false;
				count = total_T / required_T;
				this->elems_per_chunk = elems_per_chunk;
				//create chain of free
				set_next(chunks, NULL);
				first_free = NULL;
				tail = 0;
				used = 0;
				return true;
			}

			// check if pointer belong to internal array of chunks
			SEQ_ALWAYS_INLINE auto is_inside(void* ptr) const noexcept -> bool {
				return ptr >= chunks && ptr < (chunks + count * T_per_chunk());
			}

			//allocate one chunk
			auto allocate() noexcept -> T* {
				if (first_free) {
					//reuse previously deallocated chunk
					T* res = first_free;
					first_free = next(first_free);
					used++;
					return res;
				}
				else if (tail != count) {
					T* res = chunkAt(tail);
					tail++;
					used++;
					return res;
				}
				return NULL;
			}
			//deallocate one chunk
			void deallocate(T* ptr) {
				SEQ_ASSERT_DEBUG(is_inside(ptr), "chunk does not belong to this deque_chunk_pool");
				if (--used == 0) {
					first_free = NULL;
					tail = 0;
					set_next(chunks, NULL);
				}
				else {
					set_next(ptr, first_free);
					first_free = ptr;
				}
			}
		};



		/// Class used to allocate/deallocate list_chunk<T> objects.
		/// Internally uses a list of growing deque_chunk_pool objects.
		/// Uses the global SEQ_GROW_FACTOR value
		///
		template< class T, class Allocator = std::allocator<T> >
		struct deque_chunk_pool_alloc
		{
			std::vector< deque_chunk_pool<T, Allocator> > pools; //vector of deque_chunk_pool
			Allocator allocator;
			size_t capacity; //full capacity in terms of T
			size_t objects; //number of allocated T
			size_t elems_per_chunks;
		public:
			deque_chunk_pool_alloc() noexcept :capacity(0), objects(0), elems_per_chunks(0) {}
			explicit deque_chunk_pool_alloc(const Allocator& alloc) noexcept :allocator(alloc), capacity(0), objects(0), elems_per_chunks(0) {}
			deque_chunk_pool_alloc(const Allocator& alloc, size_t elem_count, size_t elems_per_chunks) :allocator(alloc), capacity(0), objects(0), elems_per_chunks(0) {
				resize(elem_count, elems_per_chunks);
			}
			deque_chunk_pool_alloc(deque_chunk_pool_alloc&& other) noexcept
				:pools(std::move(other.pools)), allocator(other.allocator), capacity(other.capacity), objects(other.objects), elems_per_chunks(other.elems_per_chunks) {
				other.capacity = other.objects = 0;
			}
			deque_chunk_pool_alloc(deque_chunk_pool_alloc&& other, const Allocator& alloc) noexcept
				:pools(std::move(other.pools)), allocator(alloc), capacity(other.capacity), objects(other.objects) {
				other.capacity = other.objects = other.elems_per_chunks = 0;
			}
			// disable copy, only allox move semantic
			deque_chunk_pool_alloc(const deque_chunk_pool_alloc&) = delete;
			auto operator=(const deque_chunk_pool_alloc&) -> deque_chunk_pool_alloc& = delete;

			~deque_chunk_pool_alloc()
			{
				pools.clear();
			}

			auto operator=(deque_chunk_pool_alloc&& other) noexcept -> deque_chunk_pool_alloc& {
				pools = std::move(other.pools);
				allocator = std::move(other.allocator);
				capacity = (other.capacity);
				objects = (other.objects);
				elems_per_chunks = other.elems_per_chunks;
				other.capacity = other.objects = other.elems_per_chunks = 0;
				return *this;
			}
			auto get_allocator() const noexcept -> Allocator {
				return allocator;
			}
			auto get_allocator() noexcept -> Allocator& {
				return allocator;
			}
			// number of free list_chunk<T>
			auto free_count() const noexcept -> std::size_t { return capacity - objects; }

			// total memory footprint in bytes excluding sizeof(*this)
			auto memory_footprint() const noexcept -> std::size_t {
				return pools.size() * sizeof(deque_chunk_pool<T, Allocator>) + capacity * (elems_per_chunks * sizeof(T));
			}

			// extend the pool
			void resize(std::size_t count, size_t elems_per_chunks) {
				this->elems_per_chunks = elems_per_chunks;
				if (count > capacity) {
					size_t last_cap = pools.size() ? pools.back().count : 0;
					size_t extend = std::max(count - capacity, static_cast<size_t>(last_cap * SEQ_GROW_FACTOR));
					pools.push_back(deque_chunk_pool<T, Allocator>(allocator, extend, elems_per_chunks));
					capacity += extend;
				}
			}

			//allocate one chunk. returns the usable part as a T*
			auto allocate() -> T*
			{
				if (capacity == objects) {
					//full, create a new deque_chunk_pool
					if (pools.empty()) pools.push_back(deque_chunk_pool<T, Allocator>(allocator, 1, elems_per_chunks));
					else {
						size_t to_allocate = static_cast<size_t>( capacity * SEQ_GROW_FACTOR);
						if (!to_allocate) to_allocate = 1;
						pools.push_back(deque_chunk_pool<T, Allocator>(allocator, to_allocate, elems_per_chunks));
					}
					capacity += pools.back().count;
				}
				for (int i = static_cast<int>(pools.size()) - 1; i >= 0; --i) {
					//find a deque_chunk_pool starting from the last deque_chunk_pool
					T* res = pools[i].allocate();
					if (res) {
						++objects;
						return res;
					}
				}
				//throw if cannot get a chunk<T>
				throw std::bad_alloc();
				return NULL;
			}

			//deallocate a list_chunk<T>
			void deallocate(T* _ptr)
			{
				//get corresponding dchunk
				T* ptr = _ptr;
				for (auto it = pools.begin(); it != pools.end(); ++it) {
					if (it->is_inside(ptr)) {
						it->deallocate(ptr);
						--objects;
						if (it->used == 0) {
							capacity -= it->count;
							pools.erase(it);
						}
						return;
					}
				}

				//sanity check
				SEQ_ASSERT_DEBUG(false, "corrupted deque_chunk_pool_alloc");
			}

			void recyclate(deque_chunk_pool_alloc& other)
			{
				if (!other.pools.size())
					return;

				for (size_t i = 0; i < other.pools.size(); ++i) {
					if (other.pools[i].rebind_for(elems_per_chunks))
					{
						pools.push_back(std::move(other.pools[i]));
						capacity += pools.back().count;
						other.pools.erase(other.pools.begin() + i);
						--i;
					}
				}
			}
		};




		/// Bucket allocator/deallocator class for seq::tiered_vector
		template<class T, class Allocator, class Bucket>
		struct DequeBucketAllocator
		{
			using BucketType = Bucket;
			using MemPoolType = deque_chunk_pool_alloc<T, Allocator>;
			template< class U>
			using RebindAlloc = typename std::allocator_traits<Allocator>::template rebind_alloc<U>;
			static const size_t start_data_T = BucketType::start_data_T;
			Allocator allocator;
			int bucket_size;
			MemPoolType pool;

			explicit DequeBucketAllocator(const Allocator& al) :allocator(al), bucket_size(0), pool(al) {}
			explicit DequeBucketAllocator(Allocator&& al) :allocator(std::move(al)), bucket_size(0), pool(al) {}
			DequeBucketAllocator(DequeBucketAllocator&& other) noexcept : allocator(std::move(other.allocator)), bucket_size(other.bucket_size), pool(std::move(other.pool)) {}
			DequeBucketAllocator(DequeBucketAllocator&& other, const Allocator& al) noexcept : allocator(al), bucket_size(other.bucket_size), pool(std::move(other.pool)) {}
			~DequeBucketAllocator() {}

			auto operator=(DequeBucketAllocator&& other) noexcept -> DequeBucketAllocator& {
				allocator = (std::move(other.allocator));
				bucket_size = (other.bucket_size);
				pool = (std::move(other.pool));
				return *this;
			}

			auto alloc(int max_size) -> BucketType* {
				if (max_size != bucket_size) {
					//new max size, init
					pool = std::move(MemPoolType(allocator, 1, start_data_T + max_size));
					bucket_size = max_size;
				}
				//alloc, construct, return
				RebindAlloc< BucketType> alloc = allocator;
				BucketType* res = reinterpret_cast<BucketType*>(pool.allocate());
				construct_ptr(res, max_size);
				return res;
			}
			auto alloc(int max_size, const T& val) -> BucketType* {
				if (max_size != bucket_size) {
					//new max size, init
					pool = std::move(MemPoolType(allocator, 1, start_data_T + max_size));
					bucket_size = max_size;
				}
				//alloc, construct, return
				RebindAlloc< BucketType> alloc = allocator;
				BucketType* res = reinterpret_cast<BucketType*>(pool.allocate());
				construct_ptr(res, max_size, val, allocator);
				return res;
			}
			void dealloc(BucketType* buff) {
				//destroy and dealloc bucket
				buff->destroy(allocator);
				pool.deallocate(reinterpret_cast<T*>(buff));
			}
			template<class StoreBucket>
			void destroy_all(StoreBucket* bs, std::size_t count) {
				//destroy and dealloc all buckets
				RebindAlloc< BucketType> alloc = allocator;
				for (std::size_t i = 0; i < count; ++i) {
					bs[i].bucket->destroy(allocator);
				}
			}
			template<class StoreBucket>
			void destroy_all_no_destructor(StoreBucket* /*unused*/, std::size_t /*unused*/) {

			}
			void init(std::size_t bcount, int bsize)
			{
				//initialize the mem pool from a bucket count and bucket size
				pool = std::move(MemPoolType(allocator, bcount, start_data_T + bsize));
				bucket_size = bsize;
			}

			void recyclate(DequeBucketAllocator<T, Allocator, Bucket>& old, int new_bucket_size = 0)
			{
				if (new_bucket_size && pool.elems_per_chunks == 0) {
					//initialize pool first
					pool = std::move(MemPoolType(allocator, 1, start_data_T + new_bucket_size));
					bucket_size = new_bucket_size;
				}
				//try to steal some memory from another DequeBucketAllocator before it is destroyed
				pool.recyclate(old.pool);
			}
		};
	}//end namespace detail

}//end namespace seq
#endif
