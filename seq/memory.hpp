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

#ifndef SEQ_MEMORY_HPP
#define SEQ_MEMORY_HPP


/** @file */

/**\defgroup memory Memory: collection of tools for memory management

The memory module provides several classes to help for containers memory management.

Allocators
----------

The memory module provides several types of STL conforming allocators:
	-	seq::aligned_allocator: allocator returning aligned memory
	-	seq::external_allocator: allocator using external allocation/deallocation functions
	-	seq::object_allocator: allocate memory using an object pool (see next section)

See class documentation for more details.

Memory pools
------------

The memory module provides 2 types of memory pool classes:
	-	seq::object_pool: standard memory pool for fast allocation of 1 or more objects of the same type, as well as std::unique_ptr. Not thread safe.
	-	seq::parallel_object_pool: memory pool for fast allocation of 1 or more objects in a multi-threaded context. Uses an almost lock-free approach.

See class documentation for more details.


Miscellaneous
-------------

The class seq::tagged_pointer stores a pointer and tag value on 4-8 bytes (depending on the architecture).
*/

#include <thread>
#include <list>
#include <vector>
#include <climits>
#include <mutex>

#include "bits.hpp"
#include "utils.hpp"
#include "lock.hpp"

#undef min
#undef max


namespace seq
{
	
	namespace detail
	{
		struct MallocFree
		{
			static auto allocate(size_t bytes) -> void* { return malloc(bytes); }
			static void deallocate(void* p, size_t bytes) { (void)bytes; free(p); }
		} ;


	}


	/// @brief Constants used by #object_pool, #parallel_object_pool and #object_allocator
	enum {
		//EnoughForSharedPtr = INT_MAX, ///! MaxObjectsAllocation value for #object_pool and #parallel_object_pool to enable shared_ptr building
		DefaultAlignment = 0 ///! Default alignment value for #object_pool, #parallel_object_pool and #object_allocator
	};



	/// @brief Allocator class with custom alignment.
	/// @tparam T object type to allocate
	/// @tparam Allocator underlying allocator
	/// 
	/// aligned_allocator is a stl conforming allocator class supporting custom alignment.
	/// It relies on an underlying allocator class to perform the actual memory allocation.
	/// aligned_allocator will allocate more memory than required for over aligned types.
	/// 
	/// If \a Align is 0, the default system alignment is used.
	/// 
	/// If provided Allocator class is std::allocator<T>, aligned_allocator is specialized to
	/// use seq::aligned_malloc and seq::aligned_free.
	/// 
	template< class T, class Allocator = std::allocator<T>, size_t Align = DefaultAlignment>
	class aligned_allocator 
	{

		template< class U>
		using rebind_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<U>;

		Allocator d_alloc;
	public:
		static_assert(Align == 0 || ((Align - 1) & Align) == 0, "wrong alignment value (must be a power of 2)");
		static constexpr size_t alignment = (Align == 0 || Align < SEQ_DEFAULT_ALIGNMENT) ? SEQ_DEFAULT_ALIGNMENT : Align;

		using value_type = T;
		using pointer = T*;
		using const_pointer = const T*;
		using reference = T&;
		using const_reference = const T&;
		using size_type = size_t;
		using difference_type = std::ptrdiff_t;
		using propagate_on_container_swap = typename std::allocator_traits<Allocator>::propagate_on_container_swap;
		using propagate_on_container_copy_assignment = typename std::allocator_traits<Allocator>::propagate_on_container_copy_assignment;
		using propagate_on_container_move_assignment = typename std::allocator_traits<Allocator>::propagate_on_container_move_assignment;
		using is_always_equal = typename std::allocator_traits<Allocator>::is_always_equal;

		template< class U > struct rebind { using other = aligned_allocator<U, Allocator, Align>; };

		auto select_on_container_copy_construction() const noexcept -> aligned_allocator { return *this; }

		aligned_allocator(const Allocator al)
			:d_alloc(al) {}
		aligned_allocator(const aligned_allocator& other)
			:d_alloc(other.d_alloc) {}
		aligned_allocator(aligned_allocator&& other) noexcept 			
			:d_alloc(std::move(other.d_alloc)) {}
		template<class U>
		aligned_allocator(const aligned_allocator<U, Allocator, Align>& other)
			: d_alloc(other.get_allocator()) {}
		~aligned_allocator() {}

		auto get_allocator() noexcept -> Allocator& { return d_alloc; }
		auto get_allocator() const noexcept -> Allocator { return d_alloc; }

		auto operator=(const aligned_allocator& other) -> aligned_allocator& {
			d_alloc = other.d_alloc;
			return *this;
		}
		auto operator=( aligned_allocator&& other) noexcept -> aligned_allocator&   {
			d_alloc = std::move(other.d_alloc);
			return *this;
		}
		auto operator == (const aligned_allocator& other) const noexcept -> bool { return d_alloc == other.d_alloc; }
		auto operator != (const aligned_allocator& other) const noexcept -> bool { return d_alloc != other.d_alloc; }
		auto address(reference x) const noexcept -> pointer {
			return static_cast<std::allocator<T>*>(this)->address(x);
		}
		auto address(const_reference x) const noexcept -> const_pointer {
			return static_cast<std::allocator<T>*>(this)->address(x);
		}
		auto allocate(size_t n, const void* /*unused*/) -> T* {
			return allocate(n);
		}
		auto allocate(size_t n) -> T*
		{
			if (alignment == SEQ_DEFAULT_ALIGNMENT) {
				rebind_alloc<T> al = get_allocator();
				return al.allocate(n);
			}

			rebind_alloc<std::uint8_t> al = get_allocator();
			void* ptr;
			size_t align = alignment - 1;
			size_t size = n * sizeof(T);

			// Room for padding and extra pointer stored in front of allocated area 
			size_t overhead = align + sizeof(void*);

			// Avoid integer overflow 
			if (size > (SIZE_MAX - overhead))
				return nullptr;

			std::uint8_t* mem = al.allocate(size + overhead);

			// Use the fact that align + 1U is a power of 2 
			size_t offset = ((align ^ (reinterpret_cast<size_t>(mem + sizeof(void*)) & align)) + 1U) & align;
			ptr = (mem + sizeof(void*) + offset);
			(reinterpret_cast<void**>(ptr))[-1] = mem;

			return static_cast<T*>(ptr);
		}
		void deallocate(T* p, size_t n)
		{
			if (alignment == SEQ_DEFAULT_ALIGNMENT) {
				rebind_alloc<T> al = get_allocator();
				return al.deallocate(p, n);
			}
			if (p != nullptr) {
				rebind_alloc<std::uint8_t> al = get_allocator();
				al.deallocate(reinterpret_cast<std::uint8_t*>((reinterpret_cast<void**>(p))[-1]), n * sizeof(T) + alignment - 1ULL + sizeof(void*));
			}
		}
		template< class... Args >
		void construct(T* p, Args&&... args) {
			//((std::allocator<T>*)(this))->construct(p, std::forward<Args>(args)...);
			construct_ptr(p, std::forward<Args>(args)...);
		}
		void destroy(T* p) {
			destroy_ptr(p);
		}
	};



	template< class T, size_t Align >
	class aligned_allocator<T, std::allocator<T>, Align> : public std::allocator<T>
	{
		using Allocator = std::allocator<T>;
		template< class U>
		using rebind_alloc = typename std::allocator_traits<std::allocator<T>>::template rebind_alloc<U>;

	public:
		static_assert(Align == 0 || ((Align - 1) & Align) == 0, "wrong alignment value (must be a power of 2)");
		static constexpr size_t alignment = (Align == 0 || Align < SEQ_DEFAULT_ALIGNMENT) ? SEQ_DEFAULT_ALIGNMENT : Align;

		using value_type = T;
		using pointer = T*;
		using const_pointer = const T*;
		using reference = T&;
		using const_reference = const T&;
		using size_type = size_t;
		using difference_type = std::ptrdiff_t;
		using propagate_on_container_swap = typename std::allocator_traits<Allocator>::propagate_on_container_swap;
		using propagate_on_container_copy_assignment = typename std::allocator_traits<Allocator>::propagate_on_container_copy_assignment;
		using propagate_on_container_move_assignment = typename std::allocator_traits<Allocator>::propagate_on_container_move_assignment;
		template< class U > struct rebind { using other = aligned_allocator<U, std::allocator<U>, Align>; };

		auto select_on_container_copy_construction() const noexcept -> aligned_allocator { return *this; }

		aligned_allocator() {}
		aligned_allocator(const Allocator& al ) : std::allocator<T>(al) { }
		aligned_allocator(const aligned_allocator& al) : std::allocator<T>(al) {}
		aligned_allocator(aligned_allocator&& al) noexcept : std::allocator<T>(std::move(al))  {}
		template<class U, class V>
		aligned_allocator(const aligned_allocator<U, std::allocator<V>, Align>& /*unused*/) {}

		auto get_allocator() noexcept-> Allocator& { return *this; }
		auto get_allocator() const noexcept -> Allocator { return *this; }

		auto operator=(const aligned_allocator& /*unused*/) -> aligned_allocator& { return *this; }
		auto operator == (const aligned_allocator& /*unused*/) const noexcept -> bool { return true; }
		auto operator != (const aligned_allocator& /*unused*/) const noexcept -> bool { return false; }
		auto address(reference x) const noexcept -> pointer {
			return static_cast<std::allocator<T>*>(this)->address(x);
		}
		auto address(const_reference x) const noexcept -> const_pointer {
			return static_cast<std::allocator<T>*>(this)->address(x);
		}
		auto allocate(size_t n, const void* /*unused*/) -> T* {
			return allocate(n);
		}
		auto allocate(size_t n) -> T*
		{
			return static_cast<T*>(aligned_malloc(n * sizeof(T), alignment));
		}
		void deallocate(T* p, size_t /*unused*/)
		{
			aligned_free(p);
		}
		template< class... Args >
		void construct(T* p, Args&&... args) {
			construct_ptr(p, std::forward<Args>(args)...);
		}
		void destroy(T* p) {
			destroy_ptr(static_cast<T*>(p));
		}
	};

	/// @brief Stl conforming allocator wrapper using an external class to perform the allocation.
	/// @tparam T object type to allocate
	/// @tparam External class used to perform the actual allocation/deallocation.
	/// 
	/// external_allocator is a stl conforming allocator that performs its allocation/deallocation based on 
	/// the \a External class. External class must have the following signature:
	/// \code
	/// struct external
	/// {
	/// 	static void* allocate(size_t bytes) ;
	/// 	static void deallocate(void* p, size_t bytes);
	/// };
	/// \endcode
	/// 
	/// It is mainly used to test memory allocation libraries (like TCMalloc, jemalloc...) with stl like containers.
	/// 
	template< class T, class External = detail::MallocFree>
	class external_allocator 
	{
	public:

		using value_type = T;
		using pointer = T*;
		using const_pointer = const T*;
		using reference = T&;
		using const_reference = const T&;
		using size_type = size_t;
		using difference_type = std::ptrdiff_t;
		using propagate_on_container_swap = std::true_type;
		using propagate_on_container_copy_assignment = std::true_type;
		using propagate_on_container_move_assignment = std::true_type;
		template< class U > struct rebind { using other = external_allocator<U, External>; };

		auto select_on_container_copy_construction() const noexcept -> external_allocator { return *this; }

		external_allocator(){}
		external_allocator(const external_allocator&  /*unused*/) {}
		template<class U>
		external_allocator(const external_allocator<U, External>&  /*unused*/){}

		auto operator == (const external_allocator& other) const noexcept -> bool { return true; }
		auto operator != (const external_allocator& other) const noexcept -> bool { return true; }
		auto address(reference x) const noexcept -> pointer {
			return std::allocator<T>{}.address(x);
		}
		auto address(const_reference x) const noexcept -> const_pointer {
			return std::allocator<T>{}.address(x);
		}
		auto allocate(size_t n, const void* /*unused*/) -> T* {
			return External::allocate(n * sizeof(T));
		}
		auto allocate(size_t n) -> T*{
			return static_cast<T*>(External::allocate(n * sizeof(T)));
		}
		void deallocate(T* p, size_t n){
			External::deallocate(p, n * sizeof(T));
		}
		template<class... Args >
		void construct(T* p, Args&&... args) {
			construct_ptr(static_cast<T*>(p), std::forward<Args>(args)...);
		}
		void destroy(T* p) {
			destroy_ptr(static_cast<T*>(p));
		}
	};


	template<class T>
	struct is_object_pool : std::false_type {};



	namespace detail
	{
		struct virtual_mem_pool
		{
			// Virtual memory pool interface
			virtual ~virtual_mem_pool() = default;
			virtual auto allocate_N(size_t n) -> void* = 0;
			virtual void deallocate_N(void* p, size_t n) = 0;
			virtual auto reclaim_memory() const -> bool = 0;
			virtual void set_reclaim_memory(bool) = 0;
			virtual void release_unused_memory() = 0;
			virtual auto memory_footprint() const -> size_t = 0;
		} ;
		template<class Pool>
		struct impl_mem_pool : public virtual_mem_pool
		{
			// Implementation of virtual_mem_pool for any Pool type
			using value_type = typename Pool::value_type;
			Pool pool;
			template<class Alloc>
			impl_mem_pool(const Alloc& al)
				:pool(al) {}
			~impl_mem_pool()  {
			}
			auto allocate_N(size_t n) -> void* { return pool.allocate(n); }
			void deallocate_N(void* p, size_t n)  { pool.deallocate(static_cast<value_type*>(p), n); }
			auto reclaim_memory() const  -> bool {return pool.reclaim_memory();}
			void set_reclaim_memory(bool reclaim)  { pool.set_reclaim_memory(reclaim); }
			void release_unused_memory()  { pool.release_unused_memory(); }
			auto memory_footprint() const  -> size_t { return pool.memory_footprint(); }
		};

		/// Create a unique identifier for given type using its size and alignment
		template<class T>
		static constexpr auto make_type_key() -> size_t {
			//No need to use the type alignment
			return sizeof(T)/*| (((size_t)alignof(T)) << (size_t)(sizeof(size_t) * 4))*/;
		}

		// Select reference count type
		template<bool IsThreaded>
		struct select_ref_cnt_type
		{
			using type = int;
		};
		template<>
		struct select_ref_cnt_type<true>
		{
			using type = std::atomic<int>;
		} ;

		template<class Pool, class Allocator, class RealAllocator>
		void deallocate_virtual_pool(Allocator & al, virtual_mem_pool * pool)
		{
			// Deallocate a virtual_mem_pool object using an allocator.
			// This function is provided as dellocating class from its base pointer is not supported with allcoators (while delete works fine).
			RealAllocator alloc = al;
			alloc.deallocate(static_cast<Pool*>(pool), 1);
		}

		/// Internal data structure shared among object_allocator copies
		template<class Allocator, bool IsThreaded>
		struct allocator_data
		{
			struct VectorData
			{
				using deallocate_fun = void (*)(Allocator &, virtual_mem_pool *);

				virtual_mem_pool* pool;
				size_t key;
				// using an allocator on a virtual_mem_pool pointer just crash, we need to 
				// store a function pointer that uses an allocator on the real pool type
				deallocate_fun ptr;

				VectorData(virtual_mem_pool* p, size_t k, deallocate_fun f)
					:pool(p), key(k), ptr(f) {}
			};
			template< class U>
			using rebind_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<U>;
			
			using ref_cnt_type = typename select_ref_cnt_type<IsThreaded>::type;

			// Ref cnt
			ref_cnt_type ref_cnt;
			// Allocator object
			Allocator allocator;
			// vector of virtual_mem_pool, one for each type the allocator rebound to.
			std::vector <VectorData , rebind_alloc<VectorData > > data;
			// lock used by allocator to find the propert type
			spinlock lock;
			
			allocator_data(const Allocator& al = Allocator())
				:ref_cnt{ 1 }, allocator(al), data(al) {}
			~allocator_data() {
				rebind_alloc< virtual_mem_pool> al = allocator;
				for (auto it = data.begin(); it != data.end(); ++it) {
					
					// using an allocator on a virtual_mem_pool pointer just crash, we need to 
					// store a function pointer that uses an allocator on the real pool type
					it->pool->~virtual_mem_pool();
					it->ptr(allocator, it->pool);
				}
			}
			auto ref() -> allocator_data* {
				++ref_cnt;
				return this;
			}
			auto unref() -> int {
				return ref_cnt--;
			}
			void decref() {
				if (unref() == 1) {
					rebind_alloc< allocator_data> al = allocator;
					allocator_data* _this = this;
					destroy_ptr(_this);
					al.deallocate(_this, 1);
				}
			}
			// Find virtual_mem_pool with given key
			auto find(size_t k) noexcept -> virtual_mem_pool* {
				for (size_t i = 0; i < data.size(); ++i)
					if (data[i].key == k)
						return data[i].pool;
				return nullptr;
			}
			// Find virtual_mem_pool with given key
			auto find(size_t k) const noexcept -> const virtual_mem_pool* {
				for (size_t i = 0; i < data.size(); ++i)
					if (data[i].key == k)
						return data[i].pool;
				return nullptr;
			}
			// Push back a memory pool for type U
			template<class PoolType>
			auto emplace_back() -> virtual_mem_pool* {
				using pool_type = impl_mem_pool< PoolType >;
				using value_type = typename PoolType::value_type;
				rebind_alloc< pool_type > al = allocator;

				//reserve before to avoid a potential std::bad_alloc after
				data.reserve(data.size() + 1);

				pool_type* tmp= al.allocate(1);
				try {
					construct_ptr(tmp, allocator);
				}
				catch (...) {
					al.deallocate(tmp, 1);
					throw;
				}
				data.push_back(VectorData(tmp, make_type_key<value_type>(), deallocate_virtual_pool<pool_type,Allocator, rebind_alloc< pool_type > >));
				return tmp;
			}
		};

		
		using lock_type = spinlock;
		
	}


	
	/// @brief Stl conforming allocator based on an object pool class.
	/// @tparam Pool object pool type, either #object_pool or #parallel_object_pool
	/// 
	/// seq::object_allocator is a stl compliant allocator dedicated to node based containers like 
	/// std::set, std::map, std::list, std::deque, std::multimap, std::multiset... object_allocator also
	/// works for other containers like std::vector, but does not provide any benefits in such cases.
	/// 
	/// It provides faster allocation/deallocation time as well as reduced memory footprint and reduced memory fragmentation
	/// (see seq::object_pool class for more details).
	/// Since the seq::object_pool class uses chunk based allocation, containers using object_allocator are usually more cache friendly,
	/// which fasten most operations(like iterating, sorting, ...).
	/// 
	/// Note that object_allocator does not work with std::list::sort() on gcc.
	/// (https://stackoverflow.com/questions/63716394/list-sort-fails-with-abort-when-list-is-created-with-stateful-allocator-when-com)
	/// 
	/// Usage example for std::list:
	/// 
	/// \code{.cpp}
	/// using allocator = object_allocator< object_pool<int> >;
	/// 
	/// std::list<int, allocator > my_list;
	/// //....
	/// 
	/// \endcode
	/// 
	template< class Pool>
	class object_allocator
	{
		using type = typename Pool::value_type;
		using InternAllocator = typename Pool::allocator_type;
		template< class U>
		using rebind_alloc = typename std::allocator_traits<InternAllocator>::template rebind_alloc<U>;
		template< class U>
		using rebind_pool = typename Pool::template rebind<U>::type;

		using data_type = detail::allocator_data<InternAllocator, !is_object_pool<Pool>::value>;
		template<class U>
		friend class object_allocator;

		data_type * d_data;
		detail::virtual_mem_pool* d_allocator;

		void ensure_valid() {
			std::unique_lock<spinlock> lock(d_data->lock);
			if (!d_allocator) {
				if (!(d_allocator = d_data->find(detail::make_type_key<value_type>()))) {
					d_allocator = d_data->template emplace_back< rebind_pool<value_type> >();
				}
			}
			SEQ_ASSERT_DEBUG(d_allocator, "");
		}

		/*auto find_pool() -> detail::virtual_mem_pool* {
			std::unique_lock<spinlock> lock(d_data->lock);
			detail::virtual_mem_pool* pool = d_data->find(detail::make_type_key<value_type>());
			if (!pool)
				pool = d_data->template emplace_back< rebind_pool<value_type> >();
			return pool;
		}*/

		auto make_data(const InternAllocator& alloc) -> data_type* {
			rebind_alloc<data_type> al = alloc;
			data_type* d = al.allocate(1);
			try {
				construct_ptr(d, alloc);
			}
			catch(...) {
				al.deallocate(d, 1);
				throw;
			}
			return d;
		}

	public:
		
		using pool_type = Pool;
		using internal_allocator_type = InternAllocator;
		using value_type = type;
		using pointer = type*;
		using const_pointer = const type*;
		using reference = type&;
		using const_reference = const type&;
		using size_type = size_t;
		using difference_type = std::ptrdiff_t;
		using propagate_on_container_swap = std::true_type;
		using propagate_on_container_copy_assignment = std::true_type;
		using propagate_on_container_move_assignment = std::true_type;
		using is_always_equal  = std::false_type;
		template< class U > struct rebind { using other = object_allocator<rebind_pool<U>>; };
		static constexpr size_t max_objects_per_allocation = Pool::max_objects;

		auto select_on_container_copy_construction() const noexcept -> object_allocator {
			return *this;
		}
		
		object_allocator()
			: d_data(make_data(InternAllocator())), d_allocator(nullptr) {
			ensure_valid();
		}
		object_allocator(const internal_allocator_type &al )
			: d_data(make_data(al)), d_allocator(nullptr) {
			ensure_valid();
		}
		object_allocator(const object_allocator& other) noexcept
			:d_data(other.d_data->ref()), d_allocator(other.d_allocator) {}
		template<class U>
		object_allocator(const object_allocator<U>& other) noexcept
			: d_data(other.d_data->ref()), d_allocator(nullptr) {
			ensure_valid();
		}
		~object_allocator() {
			d_data->decref();
		}

		// TODO(VM213788): add function set_reclaim_memory, reclaim_memory, memory_footprint...

		auto get_internal_allocator() noexcept -> internal_allocator_type& { return d_data->allocator; }
		auto get_internal_allocator() const noexcept -> const internal_allocator_type& { return d_data->allocator; }

		auto get_pool() -> pool_type* {
			return static_cast<pool_type*>(d_allocator);
		}
		auto operator == (const object_allocator& other) const noexcept -> bool {
			return d_data == other.d_data ;
		}
		auto operator != (const object_allocator& other) const noexcept -> bool {
			return !operator ==(other);
		}
		auto address(reference x) const noexcept -> pointer {
			return std::allocator<value_type>{}.address(x);
		}
		auto address(const_reference x) const noexcept -> const_pointer {
			return std::allocator<value_type>{}.address(x);
		}
		auto allocate(size_t n, const void* /*unused*/) -> value_type* {
			return allocate(n);
		}
		auto allocate(size_t n) -> value_type* {
			return static_cast<value_type*>(d_allocator->allocate_N(n));
		}
		void deallocate(value_type* p, size_t n) {
			d_allocator->deallocate_N(p,n);
		}
		template<  class... Args >
		void construct(type* p, Args&&... args) {
			construct_ptr(static_cast<value_type*>(p), std::forward<Args>(args)...);
		}
		void destroy(type* p) {
			destroy_ptr(static_cast<value_type*>(p));
		}
	};


	

	namespace detail
	{

		/// @brief Lock a boolean value
		struct bool_locker {
			bool* value;
			bool_locker(bool* v) : value(v) { *v = true; }
			~bool_locker() { *value = false; }
		};
		struct atomic_bool_locker {
			std::atomic<bool>* value;
			atomic_bool_locker(std::atomic<bool>* v) : value(v) { *v = true; }
			~atomic_bool_locker() { *value = false; }
		};


		template< bool Threaded>
		class thread_data
		{
			// Additional data used for thread safe memory pool

			void*							d_deffered_free;
			unsigned						d_deffered_count;
			std::thread::id				d_id;
			detail::lock_type				d_lock;
			
		public:
			using lock_type = detail::lock_type;
			explicit thread_data(std::thread::id _id)noexcept
				:d_deffered_free(nullptr), d_deffered_count(0), d_id(_id) {}

			SEQ_ALWAYS_INLINE auto deffered_free() noexcept -> void* { return d_deffered_free; }
			SEQ_ALWAYS_INLINE auto deffered_count() const noexcept -> unsigned { return d_deffered_count; }
			SEQ_ALWAYS_INLINE auto thread_id() const noexcept -> std::thread::id { return d_id; }
			SEQ_ALWAYS_INLINE auto lock() noexcept -> lock_type* { return &d_lock; }
			SEQ_ALWAYS_INLINE void set_deffered_free(void* d) noexcept { d_deffered_free = d; }
			SEQ_ALWAYS_INLINE void set_deffered_count(unsigned c) noexcept { d_deffered_count = c; }
			SEQ_ALWAYS_INLINE void set_thread_id(std::thread::id id) noexcept { d_id = id; }
			SEQ_ALWAYS_INLINE void reset_thread_data() {
				d_deffered_free = nullptr;
				d_deffered_count = 0;
			}
		};

		template<>
		class thread_data<false>
		{
			// Specialization for thread unsafe pool: contains nothing, does nothing
		public:
			using lock_type = null_lock;
			explicit thread_data(std::thread::id /*unused*/)noexcept {}
			thread_data() noexcept = default;
			static auto deffered_free() noexcept -> void* { return nullptr; }
			static auto deffered_count() noexcept -> unsigned { return 0; }
			static auto thread_id() noexcept -> std::thread::id { return {}; }
			static auto lock() noexcept -> lock_type* { return nullptr; }
			void set_deffered_free(void*  /*unused*/) noexcept {  }
			void set_deffered_count(unsigned  /*unused*/) noexcept {  }
			void set_thread_id(std::thread::id  /*unused*/) noexcept {}
			void reset_thread_data() {}
		};


		template<bool TemporalStats>
		struct stats_data
		{
			// Gather statistics on total number of created/deleted objects

			size_t cum_created;
			size_t cum_freed;
			stats_data() noexcept : cum_created(0), cum_freed(0) {}
			void reset_statistics() noexcept {
				cum_created = cum_freed = 0;
			}
			auto get_cum_created() const noexcept -> size_t { return cum_created; }
			auto get_cum_freed() const noexcept -> size_t { return cum_freed; }
			void increment_created()noexcept { ++cum_created; }
			void increment_freed()noexcept { ++cum_freed; }

			void add_to(stats_data & other) noexcept {
				other.cum_created += cum_created;
				other.cum_freed += cum_freed;
				reset_statistics();
			}
		};
		
		template<>
		struct stats_data<false>
		{
			// Specialization for when no temporal statistics are required: contains nothing, does nothing

			stats_data() noexcept  = default;
			void reset_statistics() noexcept {}
			static auto get_cum_created() noexcept -> size_t { return 0; }
			static auto get_cum_freed() noexcept -> size_t { return 0; }
			void increment_created()noexcept {}
			void increment_freed()noexcept {}
			void add_to(stats_data &  /*unused*/) noexcept {}
		} ;

		/// Contiguous  block of memory
		/// Contiguous block of memory used as building blocks for object_pool and parallel object pool
		template<
			class Allocator,	//allocator class
			size_t Align,		//allocation alignment
			bool Threaded,		// support multi-threading or not
			bool GenerateStats,	// support temporal statistics or not
			bool StoreHeader = true // store a short header for each allocation, only used when generating unique_ptr or shared_ptr
		>
		struct block_pool : public thread_data<Threaded>, public stats_data<GenerateStats>
		{
			static constexpr size_t alignment = (Align == 0 || Align < SEQ_DEFAULT_ALIGNMENT) ? SEQ_DEFAULT_ALIGNMENT : Align;

			template< class U>
			using rebind_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<U>;

			size_t capacity;				//full capacity (number of elements of size elem_size)
			size_t objects;					//number of objects 
			size_t tail;					//first unused block address
			size_t chunk_bytes;				//chunk actual size in bytes
			char* chunks;					//pointer to the chunk array
			char* first_free;				//first free chunk
			unsigned elem_size;
			aligned_allocator<char, Allocator, Align> allocator; //usually std::allocator< T >
			
			// For multithreading
			using lock_type = typename thread_data<Threaded>::lock_type;

			// Returns the memory footprint of this block_pool
			SEQ_ALWAYS_INLINE auto bytes() const noexcept -> size_t {
				return sizeof(*this) + chunk_bytes;
			}
			// linked list of free slots
			SEQ_ALWAYS_INLINE void set_next(char* o, char* next) {
				memcpy(o, &next, sizeof(next));
			}
			// linked list of free slots
			SEQ_ALWAYS_INLINE auto next(char* o) -> char* {
				static constexpr size_t sizeof_pointer = sizeof(char*);
				char* res;
				memcpy(&res, o, sizeof_pointer);
				return res;
			}

			// initialize for a given type size (lik sizeof(int) for object_pool<int>)
			auto init(unsigned _elem_size) noexcept -> bool
			{
				// Re-init pool for another element size
				elem_size = (_elem_size / alignment) * alignment + (_elem_size % alignment ? alignment : 0) + alignment;
				capacity = chunk_bytes / elem_size;
				if (capacity == 0)
					return false;
				reset();
				memset(chunks, 0, chunk_bytes);
				return true;
			}

			void reset()
			{
				objects = tail = 0;
				//create chain of free slots
				set_next(chunks, nullptr);
				first_free = nullptr;
				thread_data<Threaded>::reset_thread_data();
			}

			/*SEQ_NOINLINE(void) interrupt()
			{
				// Check for thread interruption durring an allocation or deallocation
				this->set_in_alloc (false);
				this->lock()->lock();
				this->set_in_alloc(true);
				this->lock()->unlock();
			}*/

			static unsigned elem_size_for_size(unsigned _elem_size) noexcept {
				return (_elem_size / alignment) * alignment + (_elem_size % alignment ? alignment : 0) + alignment;
			}
			
			// Only one constructor provided, use an object count, the size of an object and an allcoator
			block_pool(size_t elems, unsigned _elem_size, const Allocator& alloc)
				: thread_data<Threaded>(std::this_thread::get_id()), stats_data<GenerateStats>(), 
				capacity(0), objects(0), tail(0), chunk_bytes(0), chunks(nullptr), first_free(nullptr), elem_size(_elem_size), allocator(alloc)
			{

				if (elems) {
					// Element size, including header
					elem_size = (elem_size / alignment) * alignment + (elem_size % alignment ? alignment : 0) + alignment;
					// Might throw, fine
					chunk_bytes = elems * (elem_size);
					chunks = allocator.allocate(chunk_bytes);
/*#ifdef SEQ_DEBUG_MEM_POOL
					// Debug mode, set the memory block to SEQ_DEBUG_UNINIT
					memset(chunks, SEQ_DEBUG_UNINIT, chunk_bytes);
#endif*/
					// reset block to 0 as this is mandatory for concurrent_map
					memset(chunks, 0, chunk_bytes);

					capacity = elems;
					//create chain of free 
					set_next(chunks, nullptr);
					first_free = nullptr;
				}
			}
			//disable copy and move semantic
			block_pool(const block_pool& other) = delete;
			auto operator=(const block_pool& other) -> block_pool& = delete;

			~block_pool()
			{
				if (chunks) {
					//free chunk
					allocator.deallocate(chunks, chunk_bytes);
				}
			}

			SEQ_ALWAYS_INLINE auto get_allocator() const noexcept -> Allocator {
				return allocator.get_allocator();
			}
			SEQ_ALWAYS_INLINE auto get_allocator() noexcept -> Allocator& {
				return allocator.get_allocator();
			}
			// memory footprint excluding sizeof(*this)
			SEQ_ALWAYS_INLINE auto memory_footprint() const noexcept -> size_t {
				return chunk_bytes;// capacity* (elem_size);
			}
			// check if pointer belong to internal array of chunks
			SEQ_ALWAYS_INLINE auto is_inside(void* ptr) const noexcept -> bool {
				return ptr >= chunks && ptr < (chunks + capacity * elem_size);
			}
			// cheeck if full
			SEQ_ALWAYS_INLINE auto is_full() const noexcept -> bool {
				return objects == capacity;
			}
			// number of VALID object within the pool
			SEQ_ALWAYS_INLINE auto objects_minus_deffered() const noexcept -> size_t {
				return objects - this->deffered_count();
			}
			// only if StoreHeader is true, retrieve the parent block_pool from an allocated object
			static SEQ_ALWAYS_INLINE auto from_ptr(void* ptr)  noexcept -> block_pool* {
				char* o = (static_cast<char*>(ptr) - alignment);
				block_pool* res;
				// The address is written SEQ_DEFAULT_ALIGNMENT bytes before object itself (usually 8 bytes on 64 bits platform)
				memcpy(&res, o + (alignment - SEQ_DEFAULT_ALIGNMENT), sizeof(void*));
				return res;
			}

			// allocate one object
			auto allocate() noexcept -> void*
			{
				static constexpr size_t min_for_deffered = 4U;// < MaxObjects ? 4U : MaxObjects;
				// Delete deffered object if necesary
				if (Threaded ) {
					if (this->deffered_count() >= min_for_deffered) {
						delete_deffered_locked();
					}
				}

				char* res;
				if (first_free) {
					//reuse previously deallocated chunk
					res = first_free;
					first_free = next(first_free);
				}
				else if (tail != capacity) {
					//use new chunk
					res = chunks + tail * (elem_size);
					tail++;
				}
				else {
					return nullptr;
				}
					

				this->increment_created();
				objects++;
				res += alignment;
				// The address is written SEQ_DEFAULT_ALIGNMENT bytes before object itself (usually 8 bytes on 64 bits platform)
				*reinterpret_cast<std::uintptr_t*>(res - SEQ_DEFAULT_ALIGNMENT) = reinterpret_cast<std::uintptr_t>(this);

				return res;

			}

			void deallocate_internal(void* ptr)
			{
				assert(objects > 0);

				char* o = (static_cast<char*>(ptr) - alignment);
				if (SEQ_UNLIKELY(--objects == 0)) {
					// no more objects: reset tail to restart from scratch
					set_next(chunks, nullptr);
					first_free = nullptr;
					tail = 0;
				}
				else {
					set_next(o, first_free);
					first_free = o;
				}
				this->increment_freed();
			}

			SEQ_NOINLINE(void) add_deffered_delete(void* ptr)
			{
				std::unique_lock<lock_type> l(*this->lock());

				//write address of next deffered_free directly inside the object
				void* d = this->deffered_free();
				memcpy(ptr, &d, sizeof(void*));
				this->set_deffered_free(ptr);
				this->set_deffered_count(this->deffered_count() + 1);

				/*if (this->deffered_count() > 2) {
					// Too much, do it now!

					this->set_wait_requested(true);
					// Wait for outside 
					int count = 0;
					while (this->in_alloc()) {
						std::this_thread::yield();
						if (++count == 1000000)
							printf("deadlocked!\n");
					}
					// Delete deffered elements
					delete_deffered();
					this->set_wait_requested(false);
				}*/
			}

			void delete_deffered()
			{
				if (this->deffered_count() == 0) {
					this->set_deffered_free(nullptr);
					return;
				}
				while (this->deffered_free()) {
					//read next
					void* next;
					memcpy(&next, this->deffered_free(), sizeof(void*));

					deallocate_internal(this->deffered_free());
					this->set_deffered_free(next);
				}
				this->set_deffered_count(0);
			}
			SEQ_NOINLINE(void) delete_deffered_locked()
			{
				// Faster with SEQ_NOINLINE (?)
				std::unique_lock<lock_type> l(*this->lock());
				delete_deffered();
			}

			SEQ_NOINLINE(void) deallocate(void* ptr)
			{
				deallocate_ptr(ptr, std::thread::id());
			}

			SEQ_ALWAYS_INLINE auto deallocate_ptr_no_thread(void* ptr, std::thread::id current_id) -> bool
			{
				(void)current_id;
				SEQ_ASSERT_DEBUG(is_inside(ptr), "chunk does not belong to this block_pool");
				deallocate_internal(ptr);
				return true;
			}
			//deallocate one Obj
			auto deallocate_ptr(void* ptr, std::thread::id current_id) -> bool
			{
				SEQ_ASSERT_DEBUG(is_inside(ptr), "chunk does not belong to this block_pool");
#ifdef SEQ_DEBUG_MEM_POOL
				//memset(ptr, SEQ_DEBUG_FREED, sizeof(Obj));
#endif
				if (Threaded) {
					if (this->thread_id() != current_id)
					{
						add_deffered_delete(ptr);
						return false;
					}

					if (this->deffered_free()) {
						delete_deffered_locked();
					}
				}

				deallocate_internal(ptr);
				return true;
			}
		};


		/// @brief Specialization of block_pool for non multi-threaded pool
		template<
			class Allocator,
			size_t Align,
			bool GenerateStats
		>
		struct block_pool<Allocator, Align, false, GenerateStats, false> : public thread_data<false>, public stats_data<GenerateStats>
		{
			static constexpr size_t alignment = (Align == 0 || Align < SEQ_DEFAULT_ALIGNMENT) ? SEQ_DEFAULT_ALIGNMENT : Align;

			template< class U>
			using rebind_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<U>;

			size_t capacity;				//full capacity (number of elements of size elem_size)
			size_t objects;					//number of objects Obj
			size_t tail;					//first unused block address
			size_t chunk_bytes;
			char* chunks;					//pointer to the chunk array
			char* first_free;				//first free chunk
			aligned_allocator<char, Allocator, Align> allocator;
			unsigned elem_size;

			SEQ_ALWAYS_INLINE auto bytes() const noexcept -> size_t {
				return sizeof(*this) + chunk_bytes;
			}
			SEQ_ALWAYS_INLINE void set_next(char* o, char* next) {
				memcpy(o, &next, sizeof(next));
			}
			SEQ_ALWAYS_INLINE auto next(char* o) -> char* {
				char* res = nullptr;
				memcpy(&res, o, sizeof(std::uintptr_t));
				return res;
			}

			auto init(unsigned _elem_size) -> bool
			{
				// Re-init pool for another element size
				elem_size = (_elem_size / alignment) * alignment + (_elem_size % alignment ? alignment : 0);
				capacity = chunk_bytes / elem_size;
				if (capacity == 0)
					return false;
				objects = tail = 0;
				//create chain of free 
				set_next(chunks, nullptr);
				first_free = nullptr;
				return true;
			}

			void reset()
			{
				objects = tail = 0;
				//create chain of free 
				set_next(chunks, nullptr);
				first_free = nullptr;
			}

			// Only one constructor provided
			block_pool(size_t elems, unsigned _elem_size, const Allocator& alloc)
				: thread_data<false>(), stats_data<GenerateStats>(), capacity(0), objects(0), tail(0), chunk_bytes(0), chunks(nullptr), first_free(nullptr), allocator(alloc), elem_size(_elem_size)
			{

				if (elems) {
					elem_size = (elem_size / alignment) * alignment + (elem_size % alignment ? alignment : 0);
					// Might throw, fine
					chunk_bytes = elems * (elem_size);
					chunks = allocator.allocate(chunk_bytes);
#ifdef SEQ_DEBUG_MEM_POOL
					// Debug mode, set the memory block to SEQ_DEBUG_UNINIT
					memset(chunks, SEQ_DEBUG_UNINIT, chunk_bytes);
#endif
					capacity = elems;
					//create chain of free 
					set_next(chunks, nullptr);
					first_free = nullptr;
				}
			}
			//disable copy and move semantic
			block_pool(const block_pool& other) = delete;
			auto operator=(const block_pool& other) -> block_pool& = delete;

			~block_pool()
			{
				if (chunks) {
					//free chunk
					allocator.deallocate(chunks, chunk_bytes);
				}
			}

			SEQ_ALWAYS_INLINE auto get_allocator() const noexcept -> Allocator {
				return allocator.get_allocator();
			}
			SEQ_ALWAYS_INLINE auto get_allocator() noexcept -> Allocator& {
				return allocator.get_allocator();
			}
			SEQ_ALWAYS_INLINE auto memory_footprint() const noexcept -> size_t {
				return chunk_bytes;// capacity* (elem_size);
			}
			// check if pointer belong to internal array of chunks
			SEQ_ALWAYS_INLINE auto is_inside(void* ptr) const noexcept -> bool {
				return ptr >= chunks && ptr < (chunks + capacity * elem_size);
			}
			SEQ_ALWAYS_INLINE auto is_full() const noexcept -> bool {
				return objects == capacity;
			}
			SEQ_ALWAYS_INLINE auto objects_minus_deffered() const noexcept -> size_t {
				return objects ;
			}
			static SEQ_ALWAYS_INLINE auto from_ptr(void*  /*unused*/)  noexcept -> block_pool* {
				return nullptr;
			}

			// allocate one Obj
			auto allocate() noexcept -> void*
			{
				char* res;
				if (first_free) {
					//reuse previously deallocated chunk
					res = first_free;
					first_free = next(first_free);
				}
				else if (tail != capacity) {
					//use new chunk
					res = chunks + tail * (elem_size);
					tail++;
				}
				else
					return nullptr;

				this->increment_created();
				objects++;
				return res;

			}

			void add_deffered_delete(void* /*unused*/)
			{
			}
			void delete_deffered()
			{
			}
			void delete_deffered_locked()
			{
			}
			auto deallocate_ptr(void* ptr, std::thread::id /*unused*/) -> bool
			{
				deallocate(ptr);
				return true;
			}
			auto deallocate_ptr_no_thread(void* ptr, std::thread::id /*unused*/) -> bool
			{
				deallocate(ptr);
				return true;
			}
			void deallocate(void* ptr)
			{
				if (SEQ_UNLIKELY(--objects == 0)) {
					// no more objects: reset tail to restart from scratch
					set_next(chunks, nullptr);
					first_free = nullptr;
					tail = 0;
				}
				else {
					set_next(static_cast<char*>(ptr), first_free);
					first_free = static_cast<char*>(ptr);
				}
				this->increment_freed();
			}
		};

		using _pool_shared_lock = shared_spinlock;



		// Iterator other blocks
		template<class block_type>
		struct block_it
		{
			// left and right block for internal pool walk
			block_type* left;
			block_type* right;

			// only used for parallel pool in order to iterate through the pool
			std::shared_ptr<_pool_shared_lock> lock;
			block_it<block_type>* prev_block;
			block_it<block_type>* next_block;
			

			block_it() : left(nullptr), right(nullptr), prev_block(nullptr), next_block(nullptr) {}

			void remove_for_iteration()
			{
				if (next_block) {
					std::unique_lock<_pool_shared_lock> l(*lock);
					if (next_block) {
						this->prev_block->next_block = this->next_block;
						this->next_block->prev_block = this->prev_block;
						this->prev_block = this->next_block = nullptr;
					}
				}
			}
			void add_for_iteration(block_it<block_type>* other)
			{
				if (other->next_block && !this->next_block) {
					
					std::unique_lock<_pool_shared_lock> l(*other->lock);
					if (other->next_block && !this->next_block) {
						this->prev_block = other;
						this->next_block = other->next_block;
						other->next_block = this;
						this->next_block->prev_block = this;
					}
				}
			}
		};
		// Base class for object pool
		template<class T>
		struct base_object_pool
		{
			virtual ~base_object_pool() {}
			virtual auto allocate(size_t N) -> T* = 0;
			virtual void deallocate(T* p, size_t N) = 0;
		};

		template<class T>
		struct virtual_block
		{
			virtual ~virtual_block() {}
			virtual void remove() = 0;
			virtual void ref() = 0;
			virtual void unref() = 0;
			virtual void remove_and_unref() = 0;
			virtual void deallocate(T* p, size_t n) = 0;
			virtual auto parent() const -> base_object_pool<T>* = 0;
		};

		// Block structure, contains a pool, a ref count and the parent thread_data (might be nullptr)
		template<class T, class Allocator, class block_pool_type, class derived>
		struct base_block : public block_it<derived>, public virtual_block<T>
		{
			template< class U>
			using rebind_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<U>;
			using value_type = T;

			block_pool_type pool;
			void* th_data; //parent thread_data or object_pool
			std::atomic<size_t> ref_cnt; //reference count

			base_block(void* th, size_t elems, unsigned elem_size, const Allocator& al)
				:block_it<derived>(), pool(elems, elem_size, al), th_data(th), ref_cnt(1)
			{}
			void insert(derived* l, derived* r)
			{
				this->left = l;
				this->right = r;
				l->right = r->left = static_cast<derived*>(this);
			}
			virtual void remove() override
			{
				this->left->right = this->right;
				this->right->left = this->left;
				this->left = this->right = nullptr;
				this->remove_for_iteration();
			}
			virtual void remove_keep_iteration() 
			{
				this->left->right = this->right;
				this->right->left = this->left;
				this->left = this->right = nullptr;
			}
			virtual void ref() override
			{
				++ref_cnt;
			}
			virtual void remove_and_unref() override
			{
				this->left->right = this->right;
				this->right->left = this->left;
				this->left = this->right = nullptr;
				unref();
			}
			virtual void unref() override
			{
				if (ref_cnt.fetch_sub(static_cast<size_t>(1)) == static_cast<size_t>(1)) {
					if (this->left && this->right)
						remove();
					rebind_alloc< derived> al = pool.get_allocator();
					destroy_ptr(static_cast<derived*>(this));
					al.deallocate(static_cast<derived*>(this), 1);
				}
			}
			virtual void deallocate(value_type* p, size_t /*unused*/) override
			{
				pool.deallocate_ptr(p, std::this_thread::get_id());
			}


		};


		template<class T>
		auto get_virtual_block(T* p) -> virtual_block<T>*
		{
			// Assuming p has been allocated with a memory pool of maximum allocation 1,
			// returns the virtual_block corresponding to p.

			// Dummy structure, just to get the pool offset compared to parent block
			struct dummy_block : public block_it<dummy_block>, public virtual_block<T>
			{
				size_t pool;
			};

			// The block_pool address is stored SEQ_DEFAULT_ALIGNMENT bytes before the object itself
			std::uintptr_t addr = *reinterpret_cast<std::uintptr_t*>(reinterpret_cast<char*>(p) - SEQ_DEFAULT_ALIGNMENT);
			void* pool = reinterpret_cast<void*>(addr);
			// The block itself is before the block_pool
			dummy_block* block = reinterpret_cast<dummy_block*>(reinterpret_cast<char*>(pool) - SEQ_OFFSETOF(dummy_block, pool));
			return static_cast<virtual_block<T>*>(block);
		}

	}// end namespace detail



	/// @brief Deleter class for std::unique_ptr when used with #object_pool or #parallel_object_pool
	/// @tparam T std::unique_ptr template type
	template<class T>
	struct unique_ptr_deleter
	{
		void operator()(T* p) const noexcept
		{
			if (!p) return;
			p->~T();
			// Deallocate p which was allocated with a memory pool.
			detail::virtual_block<T>* v = detail::get_virtual_block(p);
			v->deallocate(p, 1);
			// Note: unref() will never deallocate the block as long as it belong to a valid memory pool
			v->unref();
		}
	};

	/// @brief Deallocate pointer previously held by a std::unqiue_ptr<T,unique_ptr_deleter<T> > 
	/// @param p pointer
	template<class T>
	void unique_ptr_delete(T* p)
	{
		unique_ptr_deleter<T>{}(p);
	}




	namespace detail
	{
		/// Custom allocator class to use with std::allocate_shared
		template<class T, class  Pool>
		class allocator_for_shared_ptr //: public std::allocator<T>
		{
			Pool* d_pool;
			using pool_type = typename Pool::value_type;
			
			template<class U, class P>
			friend class allocator_for_shared_ptr;


		public:
			using value_type = T;
			using pointer = T*;
			using const_pointer = const T*;
			using reference = T&;
			using const_reference = const T&;
			using size_type = size_t;
			using difference_type = std::ptrdiff_t;
			using propagate_on_container_swap = std::true_type;
			using propagate_on_container_copy_assignment = std::true_type;
			using propagate_on_container_move_assignment = std::true_type;
			template< class U > struct rebind { using other = allocator_for_shared_ptr<U, Pool>; };

			auto select_on_container_copy_construction() const noexcept -> allocator_for_shared_ptr { return *this; }
			allocator_for_shared_ptr() noexcept : d_pool(nullptr) {}
			allocator_for_shared_ptr(Pool* p ) noexcept : d_pool(p) {}
			allocator_for_shared_ptr(const allocator_for_shared_ptr& other)noexcept :d_pool(other.d_pool) {}
			template<class U>
			allocator_for_shared_ptr(const allocator_for_shared_ptr<U, Pool>& other)noexcept : d_pool(other.d_pool) {}
			~allocator_for_shared_ptr()noexcept {}

			auto operator == (const allocator_for_shared_ptr& other) const noexcept -> bool { return d_pool == other.d_pool; }
			auto operator != (const allocator_for_shared_ptr& other) const noexcept -> bool { return d_pool != other.d_pool; }
			auto address(reference x) const noexcept -> pointer { return std::allocator<T>{}.address(x); }
			auto address(const_reference x) const noexcept -> const_pointer { return std::allocator<T>{}.address(x); }
			auto allocate(size_t n, const void* /*unused*/) -> T* { return allocate(n); }
			auto allocate(size_t n) -> T* {
				static_assert(sizeof(T) <= sizeof(pool_type) * Pool::max_objects,"max objects too low to allocate shared_ptr");
				static constexpr size_t alloc_count = sizeof(T) / sizeof(pool_type) + (sizeof(T) % sizeof(pool_type) ? 1 : 0);
				if (n == 1) 
					return reinterpret_cast<T*>(d_pool->allocate_for_shared(alloc_count));
				else
					return std::allocator<T>{}.allocate(n);
			}
			void deallocate(T* p, size_t n) {
				if (n == 1)
				{
					static constexpr size_t alloc_count = sizeof(T) / sizeof(pool_type) + (sizeof(T) % sizeof(pool_type) ? 1 : 0);
					// Deallocate p which was allocated with a memory pool.
					virtual_block<pool_type>* v = get_virtual_block(reinterpret_cast<pool_type*>(p));
					v->deallocate(reinterpret_cast<pool_type*>(p), alloc_count);
					// Note: unref() will never deallocate the block as long as it belong to a valid memory pool
					v->unref();
				}
				else
					std::allocator<T>{}.deallocate(p, n);
			}
			template< class U, class... Args >
			void construct(U* p, Args&&... args) { 
				construct_ptr(static_cast<T*>(p), std::forward<Args>(args)...);
			}
			template< class U >
			void destroy(U* p) {
				destroy_ptr(static_cast<T*>(p));
			}
		};
	}

	
	/// @brief Class gathering statistics for seq::object_pool or seq::parallel_object_pool
	struct object_pool_stats
	{
		size_t memory{0};			/// current memory footprint in bytes
		size_t peak_memory{0};		/// highest recorded memory footprint
		size_t objects{0};			/// current number of allocated objects
		size_t total_created{0};	/// total number of objects that has been allocated (GenerateStats must be true)
		size_t total_freed{0};		/// total number of object that has been deallocated (GenerateStats must be true)
		size_t thread_count{0};	/// total number of threads using this pool (always 0 for object_pool)
		object_pool_stats() {}
	} ;

	/// @brief Allocate up to MaxSize objects by step of 1
	template<size_t MaxSize = 1U, size_t MinCapacity = 4U >
	struct linear_object_allocation
	{
		static constexpr size_t count = MaxSize;
		static constexpr size_t min_capacity = MinCapacity;
		static constexpr size_t max_objects = MaxSize;
		static constexpr auto fits(size_t size) -> bool { return size <= MaxSize; }
		static constexpr auto size_to_idx(size_t size) -> size_t { return size == 0 ? 0 : size - 1; }
		static constexpr auto idx_to_size(size_t idx) -> size_t { return idx + 1; }
	} ;

	/// @brief Allocate up to MaxSize objects by step of 1
	template<size_t MinCapacity = 4U >
	struct one_object_allocation
	{
		static constexpr size_t count = 1;
		static constexpr size_t min_capacity = MinCapacity;
		static constexpr size_t max_objects = 1;
		static constexpr auto fits(size_t ) -> bool { return true; }
		static constexpr auto size_to_idx(size_t ) -> size_t { return 0; }
		static constexpr auto idx_to_size(size_t ) -> size_t { return 1; }
	};

	/// @brief Allocate up to MaxSize objects by step of BlockSize
	template<size_t MaxSize , size_t BlockSize, size_t MinCapacity = 4U >
	struct block_object_allocation
	{
		static_assert((MaxSize > BlockSize) && (MaxSize% BlockSize) == 0, "MaxSize  must be a multiple of BlockSize");
		static constexpr size_t count = MaxSize/ BlockSize;
		static constexpr size_t min_capacity = MinCapacity;
		static constexpr size_t max_objects = MaxSize;
		static constexpr auto fits(size_t size) -> bool { return size <= MaxSize; }
		static constexpr auto size_to_idx(size_t size) -> size_t { return (size / BlockSize + (size % BlockSize ? 1 : 0)) - 1; }
		static constexpr auto idx_to_size(size_t idx) -> size_t { return (idx+1) * BlockSize; }
	};

	/// @brief Allocate up to MaxSize objects using power of 2 steps
	template<size_t MaxSize , size_t MinSize = 1U, size_t MinCapacity = 4U >
	struct pow_object_allocation
	{
		static_assert((MinSize& (MinSize - static_cast<size_t>(1))) == static_cast<size_t>(0), "Minimum size must be a power of 2");
		static_assert((MaxSize& (MaxSize - static_cast<size_t>(1))) == static_cast<size_t>(0), "Maximum size must be a power of 2");
		static constexpr size_t max_objects = MaxSize;
		static constexpr size_t min_capacity = MinCapacity;
		static constexpr size_t count = seq::static_bit_scan_reverse<MaxSize>::value - seq::static_bit_scan_reverse<MinSize>::value + 1U;
		static constexpr auto fits(size_t size) -> bool { return size <= MaxSize; }
		static constexpr auto nonPow2(size_t size) -> size_t { return (size & (size - 1)) != 0; }
		static SEQ_ALWAYS_INLINE auto size_to_idx(size_t size) -> size_t { 
			if (size < MinSize) return 0;
			size_t log_2 = bit_scan_reverse(size);
			log_2 += nonPow2(size);
			log_2 -= seq::static_bit_scan_reverse<MinSize>::value;
			return log_2;
		}
		static constexpr auto idx_to_size(size_t idx) -> size_t { return 1ULL << (idx + seq::static_bit_scan_reverse<MinSize>::value); }
	};

	struct shared_ptr_allocation : linear_object_allocation<64>{} ;



	

	/// @brief Object pool class used to allocate objects of type T.
	/// @tparam T object type 
	/// @tparam Allocator allocator type used by this object_pool
	/// @tparam object_allocation allocation pattern
	/// @tparam EnableUniquePtr allow creating std::unique_ptr with this object_pool
	/// @tparam GenerateStats maintains temporal statistics
	/// 
	/// seq::object_pool is a memory pool class used to allocate objects of type T. It does so by managing internally 
	/// contiguous memory blocks of increasing size, based on the growth factor SEQ_GROW_FACTOR (default to 1.6).
	/// 
	/// seq::object_pool has the following properties:
	///		-	It is fast for allocating single objects, typically 10 times faster than the default malloc/free implementation.
	///		-	It reduces memory fragmentation for small data types.
	///		-	It can allocate more than one object at a time based on the provided object_allocation template parameter.
	///		-	It provides statistics: memory footprint, peak memory footprint, number of objects, total number of allocated/deallocated objects (if GenerateStats is true).
	///		-	The memory blocks are released on destruction or on calls to clear() or release_unused_memory().
	///		-	It can generate std::unique_ptr object (if EnableUniquePtr is true) that outlive the object_pool object.
	/// 
	/// object_pool is NOT thread safe. For a thread safe alternative, see seq::parallel_object_pool.
	/// 
	/// Allocation
	/// ----------
	/// 
	/// Use object_pool::allocate() to allocate one or more objects. If the requested number of objects is lower or equal to object_allocation::max_objects,
	/// the pool will look for a memory block able to allocate the requested size. If no such block exists, a new block is created based on provided allocator.
	/// If the object count is greater than object_allocation::max_objects, object_pool will directly use the supplied allocator object. 
	/// 
	/// Each memory block is a contiguous memory chunk, where new objects are extracted by incrementing a head pointer (this is a simplification as the provided
	/// alignment must be taken into account). On deallocation, the created free slot is added to a monodirectional linked list of free slots and ready to be 
	/// used for further allocations. The allocation process always performs in O(1), except when a new memory block must be created (in which case the behavior
	/// depends on the provided allocator).
	/// 
	/// object_pool can potentially allocate more than one object using the internal block mechanism. This depends on the provided object_allocation type,
	/// which defines the maximum number of objects a user can request, and the allocation pattern. The default type is linear_object_allocation<1> that can
	/// only allocate 1 object per call using the block mechanism. The possible object_allocation types are:
	///		-	linear_object_allocation<MaxSize>: can allocate up to MaxSize contiguous objects at once. For instance, when using MaxObject == 2,
	///			the object_pool will manage 2 types of memory blocks: those than can allocate 1 object, and those that can allocate 2 objects. These types of block
	///			are managed independently, and grow independently based on user requests. This is, in fact, very similar to managing 2 different object_pool, except
	///			that a free memory block used initially to allocate 1 object can be reused later to allocate 2 objects.
	///		-	block_object_allocation<MaxSize,BlockSize>: can allocate up to MaxSize contiguous objects by step of BlockSize. For instance with block_object_allocation<12,4>,
	///			if the user ask for 10 objects, these objects will be extracted from a block managing 12 objects.
	///		-	pow_object_allocation<MaxSize>: can allocate up to MaxSize contiguous objects by steps which are power of 2. For pow_object_allocation<16>, the
	///			object_pool manages 5 types of memory block, which can allocate 1, 2, 4, 8 or 16 objects.
	/// 
	/// As explained above, object_pool::allocate() never fails (except with a potential std::bad_alloc), and will fallback to the supplied allocator if requested number of objects 
	/// is not managed by the provided object_allocation type.
	/// 
	/// Usage example:
	/// 
	/// \code{.cpp}
	/// 
	/// seq::object_pool<int, std::allocator<int>, seq::DefaultAlignment, seq::linear_object_allocation<2> > pool;
	/// 
	/// int *p1 = pool.allocate(1);	// use memory blocks dedicated to allocate 1 object
	/// int *p2 = pool.allocate(2); // use memory blocks dedicated to allocate 2 object2
	/// int *p3 = pool.allocate(3); // directly call std::allocator<int>::allocate(3)
	/// 
	/// // deallocate all
	/// pool.deallocate(p1,1);
	/// pool.deallocate(p2,2);
	/// pool.deallocate(p3,3);	// directly call std::allocator<int>::deallocate(p3,3)
	/// 
	/// \endcode
	/// 
	/// 
	/// Deallocation
	/// ------------
	/// 
	/// Use object_pool::deallocate() to deallocate a memory region previously allocated with object_pool::allocate().
	/// A deallocation can trigger 2 types of mechanism:
	///		-	If the number of objects to deallocate is greater than object_allocation::max_objects, the supplied allocator is directly used for the deallocation.
	///		-	Otherwise:
	///			-# The memory block to which belong the objects is found,
	///			-# The objects are marked as free slots and added to a free list
	///			-# If the memory block is empty (no more objects inside), it is either:
	///				-	Deallocated if reclaim_memory() is true
	///				-	Kept internally for further reuse if reclaim_memory() is false.
	/// 
	/// Memory blocks do not outlive the object_pool. On destruction or calls to clear() or release_unused_memory(), all memory blocks are freed (this invalidates pointers referencing these blocks). 
	/// Note, however, that the memory allocated with supplied allocator when requested object count is greater than object_allocation::max_objects is not automatically deallocated.
	/// 
	/// Another exception is when dealing with std::unique_ptr (see next section).
	/// 
	/// 
	/// Unique pointers
	/// ---------------
	/// 
	/// An object_pool can create std::unique_ptr objects only if EnableUniquePtr is true. In this case the unique_ptr type is given by
	/// the typedef object_pool<T>::unique_ptr. This is basically a std::unique_ptr with a custom deleter class.
	/// 
	/// Use object_pool::make_unique() to create unique_ptr objects. These objects will outlive the object_pool.
	/// When destroying the object_pool or on calls to object_pool::clear() or release_unused_memory(), all memory blocks that contain at least
	/// one object stored in a unique_ptr are not destroyed. These blocks contain a thread-safe reference counter
	/// that is decremented whenever a unique_ptr is destroyed, and is eventually deallocated when the last unique_ptr referencing it is destroyed.
	/// 
	/// You should never call object_pool::deallocate() on a pointer held by a unique_ptr, this will lead to a segfault on unique_ptr destruction.
	/// 
	/// You can still release the pointer using std::unique_ptr::release(), but the pointer must then be destroyed with seq::unique_ptr_delete(),
	/// which calls the object destructor and deallocate the memory if needed.
	/// 
	/// Usage example:
	/// 
	/// \code{.cpp}
	/// 
	/// using pool_type = seq::object_pool<int, std::allocator<int>, seq::DefaultAlignment, seq::linear_object_allocation<1>, true >;
	/// using unique_ptr = typename pool_type::unique_ptr;
	/// 
	/// unique_ptr p1, p2;
	/// {
	/// pool_type pool;
	/// 
	/// p1 = pool.make_unique(1);	//unique ptr containing int(1)
	/// p2 = pool.make_unique(2);	//unique ptr containing int(2)
	/// }
	/// 
	/// // p1 and p2 are still valid
	/// 
	/// int * ptr = p1.release();
	/// 
	/// //ptr is still valid, but must now be release with seq::unique_ptr_delete()
	/// 
	/// seq::unique_ptr_delete(ptr);
	/// 
	/// \endcode
	/// 
	/// The unique_ptr generation is not enabled by default as it makes the allocation process slightly slower and require more memory (one additional pointer per allocation).
	/// 
	/// Statistics
	/// ----------
	/// 
	/// At any point, the total memory footprint of an object_pool can be retrieved with object_pool::memory_footprint(). The peak memory footprint
	/// can be retrieved with peak_memory_footprint().
	/// 
	/// To access to more detailed statistics, the function object_pool::dump_statistics() will provide the following information through a object_pool_stats object:
	///		-	memory: current memory footprint in bytes
	///		-	peak_memory: highest recorded memory footprint
	///		-	objects: current number of allocated objects
	///		-	total_created: total number of objects that has been allocated (GenerateStats must be true)
	///		-	total_freed: total number of object that has been deallocated (GenerateStats must be true)
	///		-	thread_count: total number of threads using this pool (always 0 for object_pool)
	/// 
	/// The peak_memory, total_created and total_freed can be reseted using object_pool::reset_statistics().
	/// The peak_memory will be set to the current memory footprint, total_created and total_freed will be set to 0. 
	///
	/// 
	template<
		class T, //data type
		class Allocator = std::allocator<T>,
		size_t Align = DefaultAlignment, //allocation alignment
		class object_allocation = linear_object_allocation<1>, //how much objects can be queried by a single allocation
		bool EnableUniquePtr = false,
		bool GenerateStats = false
	>
	class object_pool : public detail::base_object_pool<T>,  private detail::stats_data<GenerateStats>, private Allocator
	{
		using chunk_type = detail::block_pool<  Allocator,Align, false, GenerateStats, EnableUniquePtr>;
		template< class U>
		using rebind_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<U>;
		using this_type = object_pool<T, Allocator, Align, object_allocation, EnableUniquePtr, GenerateStats>;

		static constexpr size_t min_objects = 4U;// MaxObjectsAllocation * 4U;
		static constexpr size_t dword_4 = sizeof(void*) * 4; // Size of 4 pointers
		static constexpr size_t slots = (!std::is_same<shared_ptr_allocation, object_allocation>::value) ?
			object_allocation::count :
			(1 + dword_4 / sizeof(T) + ((dword_4 % sizeof(T)) != 0u ? 1 : 0));


		template<class U, class Pool>
		friend class detail::allocator_for_shared_ptr;

		struct block : public detail::base_block<T, Allocator, chunk_type, block>
		{
			block(void* data, size_t elems, unsigned elem_size, const Allocator& al)
				: detail::base_block<T,Allocator, chunk_type, block>(data, elems, elem_size, al)
			{}

			virtual auto parent() const -> detail::base_object_pool<T>* override {
				return static_cast<this_type*>(this->th_data);
			}
		};
		using block_iterator = detail::block_it<block>;

		block_iterator d_free;
		block_iterator d_pools[slots];
		block * d_last[slots];
		size_t d_capacity[slots];
		size_t d_bytes;
		size_t d_peak_memory : sizeof(size_t)*8 -1;
		size_t d_reclaim_memory :1;
		
		// Add a new chunk_mem_pool of given capacity
		auto add(size_t idx, size_t chunk_capacity) -> block* 
		{
			// Check minimum size
			if (chunk_capacity < object_allocation::min_capacity)
				chunk_capacity = object_allocation::min_capacity;

			// Allocate chunk_mem_pool
			rebind_alloc< block> al = get_allocator();//d_allocator;
			block* res = nullptr;
			try {
				res = al.allocate(1);
				construct_ptr(res, this, chunk_capacity, static_cast<unsigned>(object_allocation::idx_to_size(idx) * sizeof(T)), get_allocator());
			}
			catch (...) {
				// deallocate chunk_mem_pool in case of exception and rethrow
				if(res)
					al.deallocate(res, 1);
				throw;
			}
		
			res->insert(d_pools[idx].left, static_cast<block*>(&d_pools[idx]));
			d_capacity[idx] += chunk_capacity;
			d_bytes += sizeof(block) + res->pool.memory_footprint();
			d_peak_memory = std::max(static_cast<size_t>(d_peak_memory), static_cast<size_t>(d_bytes));
			return res;
		}

		SEQ_NOINLINE(auto) allocate_from_free_block(size_t idx) -> T*
		{
			block* bl = d_free.right;
			// Scan all free block until we find a big enough one
			while (bl != static_cast<block*>(&d_free)) {
				// Check if this block is big enough
				if (!bl->pool.init(static_cast<unsigned>(object_allocation::idx_to_size(idx) * sizeof(T)))) {
					bl = bl->right;
					continue;
				}
				bl->remove();
				bl->insert(d_pools[idx].left, static_cast<block*>(&d_pools[idx]));
				d_last[idx] = bl;
				d_capacity[idx] += bl->pool.capacity;
				return static_cast<T*>(bl->pool.allocate());
			}
			return nullptr;
		}

		SEQ_NOINLINE(auto) empty(size_t idx, block* bl) noexcept -> block*
		{
			// Handle empty block
			block* right = bl->right;
			d_capacity[idx] -= bl->pool.capacity;
			
			if (d_last[idx] == bl)
				d_last[idx] = nullptr;

			bl->pool.add_to(*this);
			if (d_reclaim_memory) {
				d_bytes -= sizeof(block) + bl->pool.memory_footprint();
				bl->remove_and_unref();
			}
			else {
				bl->remove();
				bl->insert(d_free.left, static_cast<block*>(&d_free));
			}
			return right;
		}

		void release_unused_memory_internal()
		{
			for (size_t i = 0; i < slots; ++i){
				block* it = d_pools[i].right;
				while (it != static_cast<block*>(&d_pools[i])) {
					block* next = it->right;
					if (it->pool.objects == 0) {
						d_capacity[i] -= it->pool.capacity;
						d_bytes -= sizeof(block) + it->pool.memory_footprint();
						it->pool.add_to(*this);
						it->remove_and_unref();
						d_last[i] = nullptr;
					}
					it = next;
				}
			}
			block* it = d_free.right;
			while (it != static_cast<block*>(&d_free)) {
				block* next = it->right;
				d_bytes -= sizeof(block) + it->pool.memory_footprint();
				it->pool.add_to(*this);
				it->remove_and_unref();
				it = next;
			}
		}

		SEQ_NOINLINE(auto) allocate_from_new_block(size_t idx) -> T*
		{
			size_t to_allocate = static_cast<size_t>(static_cast<double>(d_capacity[idx]) *  SEQ_GROW_FACTOR);
			if (to_allocate < object_allocation::min_capacity) {
				//for the start, try not to exceed objs_per_alloc T values
				to_allocate = object_allocation::min_capacity;
			}
			else if (to_allocate == d_capacity[idx]) to_allocate++;
			else {

			}
			//full, create a new block
			d_last[idx] = add(idx,to_allocate);
			return static_cast<T*>(d_last[idx]->pool.allocate());
		}

		SEQ_NOINLINE(auto) allocate_from_non_last(size_t idx) -> T*
		{
			if (d_free.right != static_cast<block*>(&d_free))
				if (T* res = allocate_from_free_block(idx))
					return res;

			block* it = d_pools[idx].right;
			while (it != static_cast<block*>(&d_pools[idx])) {
				// Empty block which is not the last one: due to unique_ptr destruction
				if (SEQ_UNLIKELY(it->pool.objects == 0 && it->right != static_cast<block*>(&d_pools[idx]))) {
					it = empty(idx, it);
				}
				if (it != d_last[idx]) {
					if (T* res = static_cast<T*>(it->pool.allocate())) {
						d_last[idx] = it;
						return res;
					}
				}
				it = it->right;
			}

			return allocate_from_new_block(idx);
		}

		auto allocate_big(size_t size) -> T*
		{
			//size too big to use the pools, use allocator
			T* res = nullptr;
			if (alignment <= SEQ_DEFAULT_ALIGNMENT) {
				rebind_alloc<T> al = get_allocator();
				res = al.allocate(size);
			}
			else {
				aligned_allocator<T, Allocator, alignment> al = get_allocator();
				res = al.allocate(size);
			}
			return res;
		}

		void deallocate_big(T* ptr, size_t size)
		{
			if (alignment <= SEQ_DEFAULT_ALIGNMENT) {
				rebind_alloc<T> al = get_allocator();
				al.deallocate(ptr,size);
			}
			else {
				aligned_allocator<T, Allocator, alignment> al = get_allocator();
				al.deallocate(ptr,size);
			}
		}


		// Allocate for usage through unique_ptr or shared_ptr.
		// This does incerase the ref count of the block.
		auto allocate_for_shared(size_t n) -> T*
		{
			T* p = allocate(n);
			d_last[object_allocation::size_to_idx(n)]->ref();
			return p;
		}



	public:
		static_assert(Align == 0 || ((Align & (Align - 1)) == 0), "alignment must be a power of 2");
		static_assert(Align == 0 || Align >= alignof(T), "alignment must be >= alignof(T)");

		/// @brief object type
		using value_type = T;
		/// @brief pointer type
		using pointer = T*;
		/// @brief supplied allocator type
		using allocator_type = Allocator;
		/// @brief unique_ptr type if EnableUniquePtr is true
		using unique_ptr = std::unique_ptr<T, unique_ptr_deleter<T> >;
		/// @brief shared_ptr type if shared_ptr_allocation is used with EnableUniquePtr
		using shared_ptr = std::shared_ptr<T >;
		/// @brief allocation pattern
		using allocation_type = object_allocation;
		/// @brief enable/disable unique_ptr support
		static constexpr bool enable_unique_ptr = EnableUniquePtr;
		/// @brief generate temporal statistics
		static constexpr bool generate_statistics = GenerateStats;
		/// @brief object alignment
		static constexpr size_t alignment = (Align == 0 || Align < SEQ_DEFAULT_ALIGNMENT) ? SEQ_DEFAULT_ALIGNMENT : Align;
		/// @brief maximum number of objects per allocation before going right through the allocator
		static constexpr size_t max_objects = object_allocation::max_objects;

		/// @brief Rebind struct, used for seq::object_allocator
		template<class U>
		struct rebind
		{
			using type = object_pool<U, Allocator, Align, object_allocation,EnableUniquePtr,GenerateStats>;
		};

		/// @brief Constructor
		/// @param al allocator object
		object_pool(const Allocator& al = Allocator()) noexcept
			: detail::stats_data<GenerateStats>(), Allocator(al), d_bytes(0), d_peak_memory(0), d_reclaim_memory(true)//, d_allocator(al)
		{
			for (size_t i = 0; i < slots; ++i) {
				d_pools[i].left = d_pools[i].right = static_cast<block*>(&d_pools[i]);
				d_last[i] = nullptr;
				d_capacity[i] = 0;
			}
			d_free.left = d_free.right = static_cast<block*>(&d_free);
		}
		object_pool(bool ReclaimMemory, const Allocator& al = Allocator()) noexcept
			:object_pool(al) {
			set_reclaim_memory(ReclaimMemory);
		}
		virtual ~object_pool() override
		{
			clear();
		}

		/// @brief Free all blocks
		///
		/// Free all memory blocks, except those managing at least one unique_ptr.
		/// This will invalidate all previously allocated pointers.
		/// 
		/// Note that the objects are not destroyed, only deallocated. 
		/// 
		void clear()
		{
			block* it = d_free.right;
			while (it != static_cast<block*>(&d_free)) {
				block* next = it->right;
				d_bytes -= sizeof(block) + it->pool.memory_footprint();
				it->pool.add_to(*this);
				it->remove_and_unref();
				it = next;
			}
			for (size_t i = 0; i < slots; ++i) {
				block* bl = d_pools[i].right;
				while (bl != static_cast<block*>(&d_pools[i])) {
					block* next = bl->right;
					bl->pool.add_to(*this);
					bl->remove_and_unref();
					bl = next;
				}
				d_capacity[i] = (0);
				d_last[i] = nullptr;
				d_bytes = 0;
				d_pools[i].left = d_pools[i].right = static_cast<block*>(&d_pools[i]);
			}
		}

		/// @brief Reset the object_pool
		///
		/// Reset this object by clearing all memory blocks and make them ready for new allocations.
		/// This effectively invalidates all previously allocated pointers, even if the underlying memory segment has not been deallocated.
		/// 
		/// Has no effect on block managing at least one unique_ptr.
		/// 
		void reset()
		{
			for (size_t i = 0; i < slots; ++i) {
				block* it = d_pools[i].right;
				while (it != static_cast<block*>(&d_pools[i])) {
					block* next = it->right;
					if (EnableUniquePtr && it->ref_cnt == 1)
					{
						// Do NOT reset blocks having at least one unique_ptr
						it->remove();
						it->insert(d_free.left, static_cast<block*>(&d_free));
						
						d_capacity[i] -= it->pool.capacity;
						it->pool.reset();
					}
					it = next;
				}
				d_last[i] = nullptr;
			}
		}

		/// @brief Reset statistics
		/// 
		///  Reset the following statistics:
		///		-	peak memory (reseted to the current memory footprint)
		///		-	total number of allocated object (reseted to 0, only meaningful if GenerateStats is true)
		///		-	total number of deallocated object (reseted to 0, only meaningful if GenerateStats is true)
		/// 
		void reset_statistics()noexcept
		{
			//reset peak memory
			d_peak_memory = d_bytes;

			//reset stats for each block pool
			for (size_t i = 0; i < slots; ++i) {
				block* it = d_pools[i].right;
				while (it != static_cast<block*>(&d_pools[i])) {
					it->pool.reset_statistics();
					it = it->right;
				}
			}
			this->detail::stats_data<GenerateStats>::reset_statistics();
		}

		/// @brief Dump object_pool statistics into a object_pool_stats
		void dump_statistics(object_pool_stats& stats)
		{
			stats.memory = d_bytes;
			stats.peak_memory = d_peak_memory;

			stats.objects = 0;
			stats.total_created = this->get_cum_created();
			stats.total_freed = this->get_cum_freed();

			//compute object count and total created/freed
			for (size_t i = 0; i < slots; ++i) {
				block* it = d_pools[i].right;
				while (it != static_cast<block*>(&d_pools[i])) {
					stats.objects += it->pool.objects;
					stats.total_created += it->pool.get_cum_created();
					stats.total_freed += it->pool.get_cum_freed();
					it = it->right;
				}
			}
		}

		/// @brief Returns the underlying allocator object
		auto get_allocator() const noexcept -> const Allocator& 
		{ 
			return static_cast<const Allocator&>(*this);
		}
		/// @brief Returns the underlying allocator object
		auto get_allocator()  noexcept -> Allocator& 
		{ 
			return static_cast<Allocator&>(*this); 
		}
		/// @brief Returns the object_pool memory footprint in bytes excluding sizeof(*this).
		auto memory_footprint() const noexcept -> std::size_t 
		{
			return d_bytes;
		}
		/// @brief Returns the object_pool peak memory footprint in bytes excluding sizeof(*this).
		auto peak_memory_footprint() const noexcept -> std::size_t 
		{
			return d_peak_memory;
		}
		/// @brief Returns true if the object_pool reclaim freed memory, false otherwise
		auto reclaim_memory() const noexcept -> bool 
		{
			return d_reclaim_memory;
		}

		/// @brief Set the reclaim_memory flag.
		/// 
		/// If false, the object_pool will not deallocate memory on calls to deallocate().
		/// Instead, free memory blocks will be added to an internal list and reuse on calls to allocate().
		/// This is the only way to move a memory block dedicated to an object count for another object count.
		/// 
		/// If true, calls to deallocate will deallocate any free block.
		/// 
		void set_reclaim_memory(bool reclaim) 
		{
			if (reclaim == d_reclaim_memory)
				return;
			d_reclaim_memory = reclaim;
			if (reclaim)
				release_unused_memory_internal();
		}

		/// @brief Deallocate all unused memory blocks.
		/// This only makes sense if #reclaim_memory() is false.
		void release_unused_memory()
		{
			release_unused_memory_internal();
		}

		/// @brief Allocate size objects.
		/// @param size number of objects to allocate
		/// @return pointer to allocated objects
		/// 
		/// If size <= object_allocation::max_object, use the memory block mechanism.
		/// If size > object_allocation::max_object, directly use the supplied allocator.
		/// 
		/// Might throw std::bad_alloc.
		/// 
		virtual auto allocate( size_t size) -> T* override
		{
			if (SEQ_UNLIKELY(!object_allocation::fits(size))) {
				//size too big to use the pools, use allocator
				return allocate_big(size);
			}
			size_t idx = object_allocation::size_to_idx(size);
			if (d_last[idx]) {
				if (T* res = static_cast<T*>(d_last[idx]->pool.allocate())) {
					return res;
				}
			}
			return allocate_from_non_last(idx);
		}
	
	
		/// @brief Deallocate size objects. Non POD objects must have been destroyed previously.
		/// @param ptr pointer to objects to deallocate
		/// @param size number of objects to deallocate
		/// 
		virtual void deallocate(T* ptr, size_t size) override
		{
			if (SEQ_UNLIKELY(!object_allocation::fits(size))) {
				//size too big to use the pools, use allocator
				return deallocate_big(ptr, size);
			}

			size_t idx = object_allocation::size_to_idx(size);
			if (EnableUniquePtr) {
				chunk_type* p = chunk_type::from_ptr(ptr);
				p->deallocate_ptr_no_thread(ptr, std::thread::id());
				block* b = reinterpret_cast<block*>(reinterpret_cast<char*>(p) - SEQ_OFFSETOF(block, pool));

				if (SEQ_UNLIKELY( p->objects == 0)) {
					empty(idx, b);
				}
				else
					d_last[idx] = b;
			}
			else {
				// Scan all block of given size until we find the right one
				block* b = d_pools[idx].right;
				while (b != static_cast<block*>(&d_pools[idx])) {
					if (b->pool.is_inside(ptr)) {
						b->pool.deallocate(ptr);
						if (SEQ_UNLIKELY( b->pool.objects == 0)) {
							empty(idx, b);
						}
						else
							d_last[idx] = b;
						return;
					}
					b = b->right;
				}
				SEQ_ASSERT_DEBUG(false, "corrupted memory pool!");
			}
		}
	
		/// @brief Extend the object_pool in order to hold at least count free slots for calls to allocate(alloc_size).
		/// @param alloc_size only extend the pool in order to allocate alloc_size objects per calls to allocate()
		/// @param count make sure that at least count calls to allocate(alloc_size) will not trigger a memory allocation
		/// 
		/// Might throw a std::bad_alloc
		/// 
		void reserve(size_t alloc_size, size_t count)
		{
			size_t idx = object_allocation::size_to_idx(alloc_size);
			if (count > d_capacity[idx]) {
				size_t to_allocate = static_cast<size_t>( static_cast<double>(d_capacity[idx]) * SEQ_GROW_FACTOR);
				if (to_allocate < object_allocation::min_capacity) 
					to_allocate = object_allocation::min_capacity;
				else if (to_allocate == d_capacity[idx]) 
					to_allocate++;
				size_t extend = std::max(size_t(count - d_capacity[idx]), to_allocate);
				d_last[idx] = add(idx,extend);
			}
		}

		/// @brief Returns a unique_ptr object constructed from the forwarded argument.
		/// 
		/// This function is similar to std::make_unique(), except that it relies on the object_pool to allocate the memory.
		/// The created unique_ptr will outlive the object_pool.
		/// Never deallocate a unique_ptr pointer with object_pool::deallocate().
		/// If unique_ptr::release() is called, the pointer must be destroyed with seq::unique_ptr_delete(), which calls the object destructor and release the memory if needed.
		template< class... Args >
		auto make_unique(Args&&... args) -> unique_ptr
		{
			static_assert(EnableUniquePtr, "this memory pool is not configured to create unique_ptr objects");
			T* p = allocate_for_shared(1);
			construct_ptr(p, std::forward<Args>(args)...);
			return unique_ptr(p);
		}

		/// @brief Experimental. Returns a shared_ptr object built with this object_pool.
		/*template< class... Args >
		auto make_shared(Args&&... args) -> std::shared_ptr<T>
		{
			static_assert(EnableUniquePtr, "this memory pool is not configured to create shared_ptr objects");
			static_assert(sizeof(T) <= 64, "creating shared_ptr with this memory pool would waste too much memory");

			using alloc_type = detail::allocator_for_shared_ptr<T, this_type >;
			return std::allocate_shared<T>(alloc_type(this), std::forward<Args>(args)...);
		}*/
	};


	/// @brief Tells if given type is an object pool
	template<
		class T,
		class Allocator,
		size_t Align,
		class object_allocation,
		bool EnableUniquePtr,
		bool GenerateStats
	>
	struct is_object_pool <object_pool<T, Allocator, Align, object_allocation, EnableUniquePtr, GenerateStats> > : std::true_type {};



	namespace detail
	{
		template<bool TemporalStats>
		struct parallel_stats_data
		{
			// Statistics for parallel_object_pool

			std::atomic<size_t> cum_created;
			std::atomic <size_t> cum_freed;
			parallel_stats_data() : cum_created{ 0 }, cum_freed{ 0 }{}
			auto get_cum_created() const noexcept -> size_t { return cum_created; }
			auto get_cum_freed() const noexcept -> size_t { return cum_freed; }
			template<class Other>
			void grab_from(Other& other) {
				cum_created += other.get_cum_created();
				cum_freed += other.get_cum_freed();
				other.reset_statistics();
			}
			void reset_statistics() {
				cum_created = 0;
				cum_freed = 0;
			}
		};
		template<>
		struct parallel_stats_data<false>
		{
			parallel_stats_data() = default;
			template<class Other>
			void grab_from(Other&  /*unused*/) {}
			static auto get_cum_created() noexcept -> size_t { return 0; }
			static auto get_cum_freed() noexcept -> size_t { return 0; }
			void reset_statistics() {}
		} ;



	} //end namespace detail




	/// @brief Lock-free parallel object pool
	/// @tparam T type of object to be allocated
	/// @tparam Allocator allocator type
	/// @tparam object_allocation object allocation pattern
	/// 
	/// seq::parallel_object_pool is very similar to seq::object_pool, but is a fully thread-safe alternative.
	/// 
	/// seq::parallel_object_pool relies on independant thread-local memory blocks. Allocating is quite straightforward,
	/// and is very similar to using a thread_local object_pool.
	/// 
	/// Deallocating is more complex, as a memory region can only be deallocated by the thread that created it (except if the thread
	/// was destroyed). Therefore, parallel_object_pool uses the notion of deffered deletion: 
	///		-	The deletion request is added to a list of requests
	///		-	On further calls to parallel_object_pool::allocate() or parallel_object_pool::deallocate(), all pending deletion are processed for this thread.
	/// 
	/// When a thread is destroyed, several options are checked:
	///		-	All memory blocks belonging to this thread are gathered
	///		-	Empty memory blocks are destroyed (reclaim_memory()==true) or added to a global list of free blocks (reclaim_memory()==false)
	///			that can be used later by any thread.
	///		-	Non empty blocks are added to a list of blocks to 'clean'. parallel_object_pool will try to reuse blocks from the clean list whenever they become empty.
	/// 
	/// parallel_object_pool is almost lock-free, and should be several times faster than the default malloc()/free() implementation in multi-threaded scenarios.
	/// 
	/// All members of parallel_object_pool are thread safe.
	/// 
	/// For more details, see the documentation of seq::object_pool.
	/// 
	template<
		class T, //data type
		class Allocator= std::allocator<T>,
		size_t Align = 0, //allocation alignment
		class object_allocation = linear_object_allocation<1>, //how much objects can be queried by a single allocation
		bool GenerateStats = false , // generate complete statistics
		bool HandleInterrupt = true // internal use only
	>
	class parallel_object_pool : public detail::base_object_pool<T>, private detail::parallel_stats_data<GenerateStats>
	{
		
		template< class U>
		using rebind_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<U>;
		using block_pool_type = detail::block_pool< Allocator, Align, true, GenerateStats, true>;
		using this_type = parallel_object_pool<T, Allocator, Align, object_allocation, GenerateStats, HandleInterrupt>;
	
		template<class U, class Pool>
		friend class detail::allocator_for_shared_ptr;

		static constexpr size_t dword_4 = sizeof(void*) * 4; // Size of 4 pointers
		static constexpr size_t _slots = (!std::is_same<shared_ptr_allocation, object_allocation>::value) ?
			object_allocation::count :
			(1 + dword_4 / sizeof(T) + ((dword_4 % sizeof(T)) != 0u ? 1 : 0));
		static constexpr size_t slots = _slots == 0 ? 1 : _slots;


		struct block : public detail::base_block<T,Allocator,block_pool_type, block>
		{
			// Memory block class

			using base_type = detail::base_block<T, Allocator, block_pool_type, block>;

			block(void* th, size_t elems, unsigned elem_size,  const Allocator& al)
				: base_type(th,elems, elem_size, al)
			{}
			auto clear(size_t idx) noexcept -> size_t
			{
				// Remove and unref block.
				// Returns its capacity.
				size_t cap = this->pool.capacity;
				if (thread_data* data = static_cast<thread_data*>(this->th_data)) {
					if (data->last[idx] == this)
						data->last[idx] = nullptr;
					data->capacity[idx] -= cap;
				}
				this->remove_and_unref();
				return cap;
			}
			virtual auto parent() const -> detail::base_object_pool<T>* override
			{
				return static_cast<thread_data*>(this->th_data)->parent;
			}
		};

		using block_iterator = detail::block_it<block>;
		using pool_lock = typename block_pool_type::lock_type;
		using lock_type = detail::lock_type;

		struct thread_data
		{
			block_iterator	pools[slots];
			block*			last[slots];
			size_t			capacity[slots];
			size_t			pool_count[slots];
			this_type*		parent;
			std::atomic<bool> wait_requested;
			bool			in_alloc;

			thread_data() noexcept
				:  wait_requested(false), in_alloc(false)
			{
			}

			void get_stats(object_pool_stats & stats) const noexcept
			{
				for (size_t i = 0; i < slots; ++i) {
					block* b = pools[i].right;
					while (b != static_cast<block*>(&pools[i])) {
						stats.total_created += b->pool.get_cum_created();
						stats.total_freed += b->pool.get_cum_freed();
						stats.objects += b->pool.objects;
						b = b->right;
					}
				}
			}

			void reset_statistics()
			{
				for (size_t i = 0; i < slots; ++i) {
					block* b = pools[i].right;
					while (b != static_cast<block*>(&pools[i])) {
						b->pool.reset_statistics();
						b = b->right;
					}
				}
			}

			void init(this_type * p) noexcept
			{
				parent = p;
				for (size_t i = 0; i < slots; ++i) {
					pools[i].left = pools[i].right = static_cast<block*>(&pools[i]);
					last[i] = nullptr;
					capacity[i] = 0;
					pool_count[i] = 0;
				}
			
			}
			auto begin(size_t idx) noexcept -> block* { return pools[idx].right; }
			auto end(size_t idx) noexcept -> block* { return static_cast<block*>(&pools[idx]); }
		
			template<class It, class U>
			static It find_data(It begin, It end, const U& val) noexcept {
				for (; begin != end; ++begin)
					if (*begin == val)
						return begin;
				return end;
			}
			~thread_data() 
			{
				// Called on TLS destruction
				if (!parent) {
					return;
				}
				std::unique_lock<lock_type> l(parent->d_lock);
				
				auto it = find_data(parent->d_pools.begin(), parent->d_pools.end(), this);
				if (it != parent->d_pools.end()) {

					for (size_t i = 0; i < slots; ++i) {
						// Move empty blocks to the shared free list.
						// Move non empty block 'other' list in order to be cleaned later.
						// In any case, set their th_data attribute to nullptr
						block* b = pools[i].right;
						while (static_cast<block_iterator*>(b) != /*static_cast<block*>*/(&pools[i])) {
							block* next = b->right;
							b->th_data = nullptr;

							//retrieve stats
							parent->grab_from(b->pool);

							// Check empty
							if (b->pool.objects == 0) {
								b->remove();
								//b->remove_for_iteration();
								if (!parent->d_reclaim_memory) {
									// Destroy if not shared
									parent->d_bytes -= sizeof(block) + b->pool.memory_footprint();
									b->unref();
								}
								else {
									// Add to shared free list
									b->insert(parent->d_chunks.left, static_cast<block*>(&parent->d_chunks));
								}
							}
							else {
								// Add to clean list
								b->remove_keep_iteration();
								b->insert(parent->d_clean.left, static_cast<block*>(&parent->d_clean));
							}

							b = next;
						}
					}

					parent->d_pools.erase(it);
				}
				parent->d_bytes -= sizeof(TLS) + sizeof(thread_data);
			}

		};

		// Thread local storage
		struct TLS
		{
			using list_type = std::list<thread_data, rebind_alloc<thread_data> >;

			// Null thread_data
			static auto null() noexcept -> thread_data* {
				static thread_data inst;
				return &inst;
			}
			thread_data* last;
			list_type data;
		
			explicit TLS(const Allocator& al)
				:last(null()), data(al) {}
		};


		auto register_this(TLS& tls) -> thread_data*
		{
			// Add new thread data
			// might throw std::bad_alloc

			tls.data.emplace_back();
			tls.data.back().init(this);
			std::unique_lock <lock_type> l(d_lock);
			d_pools.push_back(&tls.data.back());
			d_bytes += sizeof(TLS) + sizeof(thread_data);
			if (d_bytes > d_peak_memory) d_peak_memory.store( static_cast<size_t>( d_bytes ));
			return tls.last = d_pools.back();
		}

		SEQ_NOINLINE(auto) find_this(TLS & tls) -> thread_data*
		{
			// Find the thread_data for this parallel mem pool 
			auto it = tls.data.begin();
			while (it != tls.data.end()) {
				if (it->parent == this)
					return tls.last = &(*it);
				else if (it->parent == nullptr)
					it = tls.data.erase(it);
				else
					++it;
			}
			// Add new thread data
			// might throw std::bad_alloc
			return register_this(tls);
		}

		SEQ_ALWAYS_INLINE auto get_data() -> thread_data*
		{
			// Returns the thread_data for current thread
			// might throw std::bad_alloc
			static thread_local TLS tls(d_alloc);
			if (tls.last->parent == this)
				return tls.last;
			return find_this(tls);
		}

		SEQ_NOINLINE( void) lock_with_interrup(thread_data * data)
		{
			// Acquire d_lock while taking care of possible interrup request
			for (;;) {
				while (d_lock.is_locked()) {
					if (SEQ_UNLIKELY(data->wait_requested.load(std::memory_order_relaxed)))
						interrupt_thread(data);
					std::this_thread::yield();
				}
				if (d_lock.try_lock())
					return;
			}
		}

		SEQ_NOINLINE(auto ) extract_free_block(size_t idx, thread_data* data) noexcept -> block*
		{
			
			// Pop back a free block from the list of shared free blocks
			
			lock_with_interrup(data); 

			block* bl = d_chunks.right;
			while (bl != static_cast<block*>(&d_chunks)) {
				if (!bl->pool.init(static_cast<unsigned>(object_allocation::idx_to_size(idx) * sizeof(T)))) {
					bl = bl->right;
					continue;
				}
				data->last[idx] = bl;
				bl->remove();
				d_lock.unlock(); // Unlock
				bl->insert(data->pools[idx].left, static_cast<block*>(&data->pools[idx]));
				bl->pool.set_thread_id(std::this_thread::get_id());
				bl->pool.init(static_cast<unsigned>(object_allocation::idx_to_size(idx) * sizeof(T)));
				bl->th_data = data;
				data->capacity[idx] += bl->pool.capacity;
				data->pool_count[idx]++;
				return bl;
			}

			//Look in d_clean
			bl = d_clean.right;
			// get single element size
			const unsigned elem_size = block_pool_type::elem_size_for_size(static_cast<unsigned>(object_allocation::idx_to_size(idx) * sizeof(T)));
			while (bl != static_cast<block*>(&d_clean)) 
			{
				size_t objects = bl->pool.objects - bl->pool.deffered_count();
				
				if (objects == 0) {
					// Empty block: reuse it
					data->last[idx] = bl;
					bl->remove_keep_iteration();
					d_lock.unlock(); // Unlock
					bl->insert(data->pools[idx].left, static_cast<block*>(&data->pools[idx]));
					bl->pool.set_thread_id(std::this_thread::get_id());
					bl->pool.init(static_cast<unsigned>(object_allocation::idx_to_size(idx) * sizeof(T)));
					bl->th_data = data;
					data->capacity[idx] += bl->pool.capacity;
					data->pool_count[idx]++;
					return bl;
				}
				else if (objects < bl->pool.capacity / 2u && elem_size == bl->pool.elem_size) {
					// Non empty block of same element size: reuse it
					data->last[idx] = bl;
					bl->remove_keep_iteration();
					d_lock.unlock(); // Unlock
					bl->insert(data->pools[idx].left, static_cast<block*>(&data->pools[idx]));
					bl->pool.set_thread_id(std::this_thread::get_id());
					bl->th_data = data;
					data->capacity[idx] += bl->pool.capacity;
					data->pool_count[idx]++;
					return bl;
				}
				bl = bl->right;
			}

			d_lock.unlock(); // Unlock
			return nullptr;
		}

		SEQ_NOINLINE(auto) empty(size_t idx, thread_data* data, block* bl) noexcept -> block*
		{
			lock_with_interrup(data);
			// Handle empty block
			// Remove block from its list and delete it if d_reclaim_memory is true,
			// or add it to the shared free list/

			// Note that data might be nullptr if the block was created in a thread that does not exist anymore.
			block* right = bl->right;
			bl->remove();
			//bl->remove_for_iteration();
			bl->th_data = nullptr;

			//retrieve stats
			this->grab_from(bl->pool);

			if (data) {
				data->pool_count[idx]--;
				data->capacity[idx] -= bl->pool.capacity;
				if (data->last[idx] == bl)
					data->last[idx] = nullptr;
			}

			if (d_reclaim_memory) {
				d_bytes -= sizeof(block) + bl->pool.memory_footprint();
				bl->unref();
			}
			else {
				
				bl->insert(d_chunks.left, static_cast<block*>(&d_chunks));
				
			}
			d_lock.unlock(); // Unlock

			return right;
		}

		SEQ_NOINLINE(auto) add(size_t idx, thread_data* data) -> block*
		{
		
			// Creates a new block for given thread data
			// Uses the global SEQ_GROW_FACTOR
			// throw on error
			size_t to_allocate = static_cast<size_t>(static_cast<double>(data->capacity[idx]) * SEQ_GROW_FACTOR);
			if (to_allocate < object_allocation::min_capacity) {
				//for the start, try not to exceed objs_per_alloc T values
				to_allocate = object_allocation::min_capacity;
			}
			else if (to_allocate == data->capacity[idx]) to_allocate++;
			else {

			}

			block * res = nullptr;
			rebind_alloc<block> al = get_allocator();

			try {
				res = al.allocate(1);
				construct_ptr(res, data, to_allocate, static_cast<unsigned>(object_allocation::idx_to_size(idx) * sizeof(T)), d_alloc);
			}
			catch (...) {
				if (res)
					al.deallocate(res, 1);
				throw;
			}
			res->lock = d_iter.lock;
			d_bytes += sizeof(block) + res->pool.memory_footprint();
			if (d_bytes > d_peak_memory) d_peak_memory.store(static_cast<size_t>( d_bytes));
			return res;
		}

		SEQ_NOINLINE(void) interrupt_thread(thread_data* data)
		{
			// Check for thread interruption durring an allocation or deallocation
			data->in_alloc = false;
			d_lock.lock();
			data->in_alloc = true;
			d_lock.unlock();
		}


		SEQ_ALWAYS_INLINE void deallocate_same_thread(size_t idx, thread_data* data, block_pool_type* p, block * bl, std::thread::id id, T* ptr)
		{
			// Deallocate in the same thread as allocation
			data->in_alloc = true;
			// Check interrupt_thread
			if (HandleInterrupt) {
				if (SEQ_UNLIKELY(data->wait_requested.load(std::memory_order_relaxed)))
					interrupt_thread(data);
			}
			p->deallocate_ptr_no_thread(ptr, id);
			if (SEQ_UNLIKELY(p->objects == 0))
				empty(idx, data, bl);
			else if (SEQ_LIKELY(data))
				data->last[idx] = bl;
			data->in_alloc = false;

		}


		void pause_all()
		{
			// Trigger a pause for all allocation/deallocation threads and wait for all threads to be paused.
			// Lock must be held before

			// Send the wait_requested to all
			for (auto it = d_pools.begin(); it != d_pools.end(); ++it)
				(*it)->wait_requested = true;

			// Wait for all threads to be interrupted
			bool keep_going = true;
			auto it = d_pools.begin();

			
			while (keep_going) {
				keep_going = false;
				for (; it != d_pools.end(); ++it)
					if ((*it)->in_alloc) {
						keep_going = true;
						break;
					}
				// Leave some room to other threads to do their jobs
				std::this_thread::yield();
			}

		}

		void resume_all()
		{
			// Lock must be held before
			// Remove the wait_requested to all
			for (auto it = d_pools.begin(); it != d_pools.end(); ++it)
				(*it)->wait_requested = false;
		}

		auto release_unused_memory_internal() -> size_t
		{
			//Lock must be held before

			pause_all();

			// Walk all blocks and force deffered deletion
			size_t res = 0;
			for (auto it = d_pools.begin(); it != d_pools.end(); ++it) {
				for (size_t i = 0; i < slots; ++i) {
					block* p = (*it)->pools[i].right;
					while (p != static_cast<block*>(&(*it)->pools[i])) {
						block* next = p->right;

						// Delete deffered object if necesary
						if (p->pool.deffered_count()) {
							std::unique_lock<pool_lock> l(*p->pool.lock());
							p->pool.delete_deffered();
						}

						//retrieve stats
						this->grab_from(p->pool);

						if (p->pool.objects == 0) {
							// Remove this block, and make sure last is updated
							d_bytes -= sizeof(block) + p->pool.memory_footprint();
							res += p->clear(i);
						}
						p = next;
					}
				}
			}

			// Do the same for blocks to clean
			block* p = d_clean.right;
			while (p != static_cast<block*>(&d_clean)) {
				block* next = p->right;
				// Delete deffered object if necesary
				if (p->pool.deffered_count()) {
					std::unique_lock<pool_lock> l(*p->pool.lock());
					p->pool.delete_deffered();
				}
				//retrieve stats
				this->grab_from(p->pool);

				if (p->pool.objects == 0) {
					// Remove this block, and make sure last is updated
					d_bytes -= sizeof(block) + p->pool.memory_footprint();
					res += p->clear(0);
				}
				p = next;
			}

			// Clear free blocks
			p = d_chunks.right;
			while (p != static_cast<block*>(&d_chunks)) {
				block* next = p->right;
				d_bytes -= sizeof(block) + p->pool.memory_footprint();
				res += p->clear(0);
				p = next;
			}
			d_chunks.right = d_chunks.left = static_cast<block*>(&d_chunks);
			resume_all();
			return res;
		}

		auto contains_no_pause(void * val) -> bool
		{
			for (auto it = d_pools.begin(); it != d_pools.end(); ++it) {
				for (size_t i = 0; i < slots; ++i) {
					block* p = (*it)->begin(i);
					while (p != (*it)->end(i)) {
						block* next = p->right;
						if (p->pool.is_inside(val))
							return true;
						p = next;
					}
					
				}
			}
			
			// Check blocks to clean
			block* p = d_clean.right;
			while (p != static_cast<block*>(&d_clean)) {
				block* next = p->right;
				if (p->pool.is_inside(val))
					return true;
				p = next;
			}
			return false;
		}

		auto clear_no_pause(bool destroy = false) -> size_t
		{
			size_t res = 0;
			for (auto it = d_pools.begin(); it != d_pools.end(); ++it) {
				for (size_t i = 0; i < slots; ++i) {
					block* p = (*it)->begin(i);
					while (p != (*it)->end(i)) {
						block* next = p->right;
						res += p->pool.capacity;
						d_bytes -= sizeof(block) + p->pool.memory_footprint();
						//retrieve stats
						this->grab_from(p->pool);
						p->remove_and_unref();
						p = next;
					}
					if (destroy)
						(*it)->parent = nullptr;
					else
						(*it)->init(this);
				}
			}
			// Clear free blocks
			block* p = d_chunks.right;
			while (p != static_cast<block*>(&d_chunks)) {
				block* next = p->right;
				res += p->pool.capacity;
				d_bytes -= sizeof(block) + p->pool.memory_footprint();
				//retrieve stats
				this->grab_from(p->pool);
				p->remove_and_unref();
				p = next;
			}
			// Clear blocks to clean
			p = d_clean.right;
			while (p != static_cast<block*>(&d_clean) ){
				block* next = p->right;
				d_bytes -= sizeof(block) + p->pool.memory_footprint();
				p->remove_and_unref();
				p = next;
			}
			d_chunks.right = d_chunks.left = static_cast<block*>(&d_chunks);
			d_clean.right = d_clean.left = static_cast<block*>(&d_clean);
			return res;
		}


		auto reset_no_pause(bool destroy = false) -> size_t
		{
			size_t res = 0;
			for (auto it = d_pools.begin(); it != d_pools.end(); ++it) {
				for (size_t i = 0; i < slots; ++i) {
					block* p = (*it)->begin(i);
					while (p != (*it)->end(i)) {
						block* next = p->right;
						// Reset block and add it to the list of free chunks.
						// Except: if the block is used by unique_ptr objects
						if (p->ref_cnt == 1) {
							res += p->pool.objects;
							p->pool.reset();
							p->remove();
							p->insert(d_chunks.left, static_cast<block*>(&d_chunks));
						}
						p = next;
					}
					if (destroy)
						(*it)->parent = nullptr;
					else
						(*it)->init(this);
				}
			}
			
			// Do the same for blocks to clean
			block* p = d_clean.right;
			while (p != static_cast<block*>(&d_clean)) {
				block* next = p->right;
				if (p->ref_cnt == 1) {
					res += p->pool.objects;
					p->pool.reset();
					p->remove();
					p->insert(d_chunks.left, static_cast<block*>(&d_chunks));
				}
				p = next;
			}
			return res;
		}


		auto allocate_from_free(thread_data* data, size_t idx) -> T*
		{
			if ((data->last[idx] = extract_free_block(idx, data))) {
				data->last[idx]->add_for_iteration(&d_iter);
				return static_cast<T*>(data->last[idx]->pool.allocate());
			}
			return nullptr;
		}

		auto allocate_from_new_block(thread_data* data, size_t idx) -> T*
		{
			
			block* _bl = add(idx,data);

			// retry previous pools in case of deffered delete in-between
			block * it = data->begin(idx);
			while (it != data->end(idx)) {
				if (T* res = static_cast<T*>(it->pool.allocate())) {
					it->add_for_iteration(&d_iter);
					data->last[idx] = it;
					// Lock while taking care of interrupt requests
					lock_with_interrup(data);
					_bl->insert(d_chunks.left, static_cast<block*>(&d_chunks));
					d_lock.unlock(); // Unlock
					return res;
				}
				it = it->right;
			}

			_bl->add_for_iteration(&d_iter);
			_bl->insert(data->pools[idx].left, static_cast<block*>(&data->pools[idx]));
			data->capacity[idx] += _bl->pool.capacity;
			data->pool_count[idx]++;
			data->last[idx] = _bl;
			return static_cast<T*>(_bl->pool.allocate());
		}
		SEQ_ALWAYS_INLINE auto allocate_from_last( thread_data* data, size_t idx) -> T*
		{
			if (SEQ_LIKELY(data->last[idx])) {
				return static_cast<T*>(data->last[idx]->pool.allocate());
			}
			return nullptr;
		}

		auto allocate_from_other(thread_data* data, size_t idx ) -> T*
		{
			// Find a block with a slot
			block* it = data->begin(idx);
			while (it != data->end(idx)) {
				// Empty block which is not the last one: due to deffered deletion, add it to the free list
				if (SEQ_UNLIKELY(/*!d_reclaim_memory &&*/ it->right != data->end(idx) && it->pool.objects == 0)) {
					it = empty(idx, data, it);
				}
				
				if (T* res = static_cast<T*>(it->pool.allocate())) {
					it->add_for_iteration(&d_iter);
					data->last[idx] = it;
					return res;
				}

				it = it->right;
			}

			// Find a free block in the shared free list
			if (d_chunks.left != static_cast<block*>(&d_chunks) ||
				d_clean.left != static_cast<block*>(&d_clean)) {
				if (T* res = allocate_from_free(data, idx))
					return res;
			}

			// Create new block
			return allocate_from_new_block(data, idx);
		}

		SEQ_ALWAYS_INLINE auto allocate(thread_data* data, size_t idx ) -> T*
		{
			detail::bool_locker bl(&data->in_alloc);
			if SEQ_CONSTEXPR (HandleInterrupt) {
				if (SEQ_UNLIKELY(data->wait_requested.load(std::memory_order_relaxed)))
					interrupt_thread(data);
			}

			// Fastest path, use last used block
			T* res = allocate_from_last(data, idx);
			if(SEQ_LIKELY(res))
				return res;			
			return allocate_from_other(data, idx);
		}


		// Allocate for usage through unique_ptr or shared_ptr.
		// This does incerase the ref count of the block.
		SEQ_ALWAYS_INLINE auto allocate_for_shared(size_t size) -> T*
		{
			thread_data* data = get_data();
			size_t idx = object_allocation::size_to_idx(size);
			T* p = allocate(data, idx);
			data->last[idx]->ref();
			return p;
		}

		auto allocate_big(size_t size) -> T*
		{
			//size too big to use the pools, use allocator
			T* res = nullptr;
			if (alignment <= SEQ_DEFAULT_ALIGNMENT) {
				rebind_alloc<T> al = get_allocator();
				res = al.allocate(size);
			}
			else {
				aligned_allocator<T, Allocator, alignment> al = get_allocator();
				res = al.allocate(size);
			}
			return res;
		}

		void deallocate_big(T* ptr, size_t size)
		{
			if (alignment <= SEQ_DEFAULT_ALIGNMENT) {
				rebind_alloc<T> al = get_allocator();
				al.deallocate(ptr, size);
			}
			else {
				aligned_allocator<T, Allocator, alignment> al = get_allocator();
				al.deallocate(ptr, size);
			}
		}




		Allocator d_alloc;
		lock_type d_lock;
		bool d_reclaim_memory;
		// List of blocks released by the mem pools and that can be reused on allocation
		block_iterator d_chunks;
		// List of blocks released on thread_data destruction and non empty. Kept for cleaning.
		block_iterator d_clean;
		// REAL iterator other non empy blocks
		block_iterator d_iter;
		// List of pools
		std::vector<thread_data*, rebind_alloc<thread_data*> > d_pools;

		//statistics
		std::atomic<size_t> d_bytes;
		std::atomic<size_t> d_peak_memory;


	public:
		using value_type = T;
		using pointer = T*;
		using allocator_type = Allocator;
		using unique_ptr = std::unique_ptr<T, unique_ptr_deleter<T> >;
		using shared_ptr = std::shared_ptr<T >;
		using allocation_type = object_allocation;
		using block_type = block;
		static constexpr size_t max_objects = object_allocation::max_objects; //Maximum object that the pool can allocate before switching to allocator
		static constexpr size_t alignment = (Align == 0 || Align < SEQ_DEFAULT_ALIGNMENT) ? SEQ_DEFAULT_ALIGNMENT : Align;

		template<class U>
		struct rebind
		{
			using type = parallel_object_pool<U, Allocator, Align, object_allocation,GenerateStats>;
		};

		/// @brief Default constructor
		/// @param alloc allocator object
		parallel_object_pool(const Allocator& alloc = Allocator())
			:detail::parallel_stats_data<GenerateStats>(), d_alloc(alloc), d_reclaim_memory(true), d_pools(alloc), d_bytes{ 0 }, d_peak_memory{ 0 }
		{
			d_chunks.left = d_chunks.right = static_cast<block*>(&d_chunks);
			d_clean.left = d_clean.right = static_cast<block*>(&d_clean);
			d_iter.next_block = d_iter.prev_block = static_cast<block*>(&d_iter);
			d_iter.lock = std::allocate_shared<detail::_pool_shared_lock>(rebind_alloc<detail::_pool_shared_lock>(alloc));
		}

		parallel_object_pool(bool ReclaimMemory, const Allocator& alloc = Allocator())
			:parallel_object_pool(alloc) {
			set_reclaim_memory(ReclaimMemory);
		}

		~parallel_object_pool() override
		{
			d_iter.remove_for_iteration();
			std::unique_lock<lock_type> l(d_lock);
			clear_no_pause(true);
		}

		// disable copy
		parallel_object_pool(const parallel_object_pool&) = delete;
		auto operator=(const parallel_object_pool&) -> parallel_object_pool& = delete;

		/// @brief No-op, only provided for compatibility with object_pool.
		void reserve(size_t alloc_size, size_t count)
		{
			std::unique_lock<lock_type> ll(d_lock);

			// Compute current capacity for given size
			size_t idx = object_allocation::size_to_idx(alloc_size);
			size_t capacity = 0;
			for (auto it = d_pools.begin(); it != d_pools.end(); ++it) {
				capacity += (*it)->capacity[idx];
			}

			while (capacity < count)
			{
				size_t to_allocate = static_cast<size_t>(static_cast<double>(capacity) * SEQ_GROW_FACTOR);
				if (to_allocate < object_allocation::min_capacity)
					//for the start, try not to exceed objs_per_alloc T values
					to_allocate = object_allocation::min_capacity;
				else if (to_allocate == capacity) to_allocate++;

				block* res = nullptr;
				rebind_alloc<block> al = get_allocator();

				try {
					res = al.allocate(1);
					construct_ptr(res, nullptr, to_allocate, static_cast<unsigned>(object_allocation::idx_to_size(idx) * sizeof(T)), d_alloc);
				}
				catch (...) {
					if (res)
						al.deallocate(res, 1);
					throw;
				}
				res->lock = d_iter.lock;
				d_bytes += sizeof(block) + res->pool.memory_footprint();
				if (d_bytes > d_peak_memory) d_peak_memory.store(static_cast<size_t>(d_bytes));
				capacity += to_allocate;

				// add to the list of free chunks
				res->insert(d_chunks.left, static_cast<block*>(&d_chunks));
			}
		}

		/// @brief Returns the underlying allocator
		auto get_allocator() const noexcept -> Allocator 
		{
			return d_alloc;
		}
		/// @brief Returns the underlying allocator.
		auto get_allocator() noexcept -> Allocator& {
			return d_alloc;
		}
		/// @brief Returns the memory footprint in bytes excluding sizeof(*this).
		auto memory_footprint() const noexcept -> size_t 
		{
			return d_bytes;
		}
		/// @brief Returns the peak memory footprint in bytes excluding sizeof(*this).
		auto peak_memory_footprint() const noexcept -> size_t 
		{
			return d_peak_memory;
		}
		/// @brief Returns true if this pool reclaims freed memory, false otherwise
		auto reclaim_memory() const noexcept -> bool
		{
			return d_reclaim_memory;
		}

		/// @brief Set the reclaim_memory flag.
		/// 
		/// If false, the object_pool will not deallocate memory on calls to deallocate().
		/// Instead, free memory blocks will be added to an internal list and reuse on calls to allocate().
		/// This is the only way to move a memory block dedicated to an object count for another object count.
		/// 
		/// If true, calls to deallocate will deallocate any free block.
		/// 
		void set_reclaim_memory(bool reclaim)
		{
			std::unique_lock<lock_type> l(d_lock);
			d_reclaim_memory = reclaim;
			if (reclaim) {
				release_unused_memory_internal();
			}
		}

		/// @brief Dump object_pool statistics into a object_pool_stats
		void dump_statistics(object_pool_stats& stats)
		{
			memset(&stats, 0, sizeof(stats));
			stats.total_created = this->get_cum_created();
			stats.total_freed = this->get_cum_freed();

			std::unique_lock<lock_type> l(d_lock);
			pause_all();
			stats.thread_count = d_pools.size();
			stats.memory = d_bytes;
			stats.peak_memory = d_peak_memory;
			for (size_t i = 0; i < d_pools.size(); ++i)
				d_pools[i]->get_stats(stats);
			resume_all();
		}

		/// @brief Reset statistics
		/// 
		///  Reset the following statistics:
		///		-	peak memory (reseted to the current memory footprint)
		///		-	total number of allocated object (reseted to 0, only meaningful if GenerateStats is true)
		///		-	total number of deallocated object (reseted to 0, only meaningful if GenerateStats is true)
		/// 
		void reset_statistics(object_pool_stats& stats)
		{
			std::unique_lock<lock_type> l(d_lock);
			this->detail::parallel_stats_data<GenerateStats>::reset_statistics();
			pause_all();
			d_peak_memory = d_bytes;
			for (size_t i = 0; i < d_pools.size(); ++i)
				d_pools[i]->reset_statistics();

			resume_all();
		}

		bool contains(T * ptr)
		{
			std::unique_lock<lock_type> l(d_lock);
			pause_all();
			bool res = contains_no_pause(ptr);
			resume_all();
			return res;
		}
	
		/// @brief Free all blocks
		///
		/// Free all memory blocks, except those managing at least one unique_ptr.
		/// This will invalidate all previously allocated pointers.
		/// 
		/// Note that the objects are not destroyed, only deallocated. 
		/// 
		auto clear() -> size_t 
		{
			std::unique_lock<lock_type> l(d_lock);
			pause_all();
			size_t res = clear_no_pause();
			resume_all();
			return res;
		}

		/// @brief Reset the object_pool
		///
		/// Reset this object by clearing all memory blocks and make them ready for new allocations.
		/// This effectively invalidates all previously allocated pointers, even if the underlying memory segment has not been deallocated.
		/// 
		/// Has no effect on block managing at least one unique_ptr.
		/// 
		auto reset() -> size_t
		{
			std::unique_lock<lock_type> l(d_lock);
			pause_all();
			size_t res = reset_no_pause();
			resume_all();
			return res;
		}

		/// @brief Deallocate all unused memory blocks.
		/// This only makes sense if #reclaim_memory() is false.
		auto release_unused_memory() -> size_t
		{
			std::unique_lock<lock_type> l(d_lock);
			return release_unused_memory_internal();
		}

		/// @brief Allocate size objects.
		/// @param size number of objects to allocate
		/// @return pointer to allocated objects
		/// 
		/// If size <= object_allocation::max_object, use the memory block mechanism.
		/// If size > object_allocation::max_object, directly use the supplied allocator.
		/// 
		/// Might throw std::bad_alloc.
		/// 
		auto allocate(size_t size ) -> T* override
		{
			if (SEQ_UNLIKELY(!object_allocation::fits(size))) {
				//size too big to use the pools, use allocator
				return allocate_big(size);
			}
			return allocate(get_data(), object_allocation::size_to_idx(size));
		}


		/// @brief Deallocate size objects. Non POD objects must have been destroyed previously.
		/// @param ptr pointer to objects to deallocate
		/// @param size number of objects to deallocate
		/// 
		void deallocate(T* ptr, size_t size) override
		{
			if (SEQ_UNLIKELY(!object_allocation::fits(size))) {
				//size too big to use the pools, use allocator
				return deallocate_big(ptr, size);
			}
			size_t idx = object_allocation::size_to_idx(size);
			std::thread::id id = std::this_thread::get_id();
			block_pool_type* p = block_pool_type::from_ptr(ptr);
			block* bl = reinterpret_cast<block*>(reinterpret_cast<char*>(p) - SEQ_OFFSETOF(block, pool));
			thread_data* data = static_cast<thread_data*>(bl->th_data);

			// If we are in the right thread, check for interrupt.
			// If not, the problem might be that p->deallocate_ptr is called while clear() or release_unused_memory() is also called.
			// In practice this is not an issue as calling clear() in parallel to deallocate() will crash anyway, and calling
			// release_unused_memory() will not touch a block with deffered deletion (as objects > 0)

			// Note that data might be nullptr if the block was created in a thread that does not exist anymore.
			// In such case, the block is in the d_clean list.
			const bool check_interrupt = id == p->thread_id() && data;

			if (SEQ_LIKELY(check_interrupt)) 
				return deallocate_same_thread(idx, data, p, bl,id, ptr);

			if (SEQ_LIKELY(p->deallocate_ptr(ptr, id))){
				// Deallocation occured in this thread
				if (SEQ_UNLIKELY(p->objects == 0))
					empty(idx,data, bl);
				else if(SEQ_LIKELY(data))
					data->last[idx] = bl;
			}

			// TODO(VM): in case of no data (allocation thread is dead), use a custom locked deletetion without deffered.
			// If this make the block empty, remove from clean list and destroy
		}

		/// @brief Returns a unique_ptr object constructed from the forwarded argument.
		/// 
		/// This function is similar to std::make_unique(), except that it relies on the object_pool to allocate the memory.
		/// The created unique_ptr will outlive the object_pool.
		/// Never deallocate a unique_ptr pointer with object_pool::deallocate().
		/// If unique_ptr::release() is called, the pointer must be destroyed with seq::unique_ptr_delete(), which calls the object destructor and release the memory if needed.
		/// 
		template< class... Args >
		auto make_unique(Args&&... args) -> unique_ptr
		{
			T* p = allocate_for_shared(1);
			try {
				construct_ptr(p, std::forward<Args>(args)...);
			}
			catch (...) {
				detail::virtual_block<T>* v = detail::get_virtual_block(p);
				v->deallocate(p, 1);
				// Note: unref() will never deallocate the block as long as it belong to a valid memory pool
				v->unref();
			}
			return unique_ptr(p);
		}

		template< class... Args >
		SEQ_ALWAYS_INLINE auto make(Args&&... args) -> T*
		{
			T* p = allocate_for_shared(1);
			try {
				construct_ptr(p, std::forward<Args>(args)...);
			}
			catch (...) {
				detail::virtual_block<T>* v = detail::get_virtual_block(p);
				v->deallocate(p, 1);
				// Note: unref() will never deallocate the block as long as it belong to a valid memory pool
				v->unref();
			}
			return p;
		}

		block_iterator* end_block_iterator() noexcept {
			return &d_iter;
		}
		const block_iterator* end_block_iterator() const noexcept {
			return &d_iter;
		}

		/// @brief Experimental. Returns a shared_ptr object built with this object_pool.
		/*template< class... Args >
		auto make_shared(Args&&... args) -> std::shared_ptr<T>
		{
			static_assert(sizeof(T) <= 64, "creating shared_ptr with this memory pool would waste too much memory");
			using alloc_type = detail::allocator_for_shared_ptr<T, this_type >;
			return std::allocate_shared<T>(alloc_type(this), std::forward<Args>(args)...);
		}*/
	};



	template<class T>
	struct is_parallel_object_pool : std::false_type {};

	template<class T,class Al,size_t A,class O,bool G, bool H>
	struct	is_parallel_object_pool<parallel_object_pool<T, Al, A, O, G, H> > : std::true_type {};


}//end namespace seq

#endif
