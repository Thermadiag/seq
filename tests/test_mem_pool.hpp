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

#include <thread>
#include <vector>
#include <iostream>

#include "testing.hpp"
//#include "mimalloc.h"
#include "memory.hpp"

using namespace seq;

template<class T>
struct StdPool
{
	using unique_ptr = std::unique_ptr<T>;
	using shared_ptr = std::shared_ptr<T>;
	using value_type = T;
	T* allocate(size_t size=1) { return static_cast<T*>(malloc(sizeof(T)*size)); }
	void deallocate(T* ptr, size_t =1) { free(ptr); }
	void release_unused_memory_all() {}
	size_t release_unused_memory() { return 0; }
	void clear_all() {}
	void reserve(size_t){}
	template<class U>
	std::unique_ptr<T> make_unique(const U& v) {
		//return std::unique_ptr<T>(new T(v));
		return std::make_unique<T>(v);
	}
	template<class U>
	std::shared_ptr<T> make_shared(const U& v) {
		return std::make_shared<T>(v);
	}
	void dump_statistics(object_pool_stats& stats) {}
};
/*template<class T>
struct MimallocPool
{
	using value_type = T;
	T* allocate(size_t size=1) { return (T*)mi_malloc(sizeof(T)*size); }
	void deallocate(T* ptr, size_t=1) { mi_free(ptr); }
	void release_unused_memory_all() {}
	size_t release_unused_memory() { return 0; }
	void clear_all() {}
	void reserve(size_t) {}
	void dump_statistics(object_pool_stats& stats) {}
};

struct MimallocExternal
{
	static void* allocate(size_t bytes) { return mi_malloc(bytes); }
	static void deallocate(void* p, size_t bytes) { (void)bytes; mi_free(p); }
};
*/

static constexpr unsigned MY_RAND_MAX = (1U << 16U) - 1U;
static inline size_t get_count(int reps, int step) {
	static std::vector<size_t> counts;
	if (static_cast<int>(counts.size()) != reps) {
		counts.resize(reps);
		srand(0);
		for (int i = 0; i < reps; ++i)
			counts[i] = (rand() & MY_RAND_MAX);
	}
	return counts[step];
}

template<class PoolType>
void test_mem_pool_release_thread(PoolType* pool, bool* finish)
{
	//using value_type = typename PoolType::value_type;

	// always clear
	while (!*finish) {
		size_t c = pool->release_unused_memory();
		if (c)
			std::cout << "released " << c << std::endl;
		std::this_thread::yield();
		//std::this_thread::sleep_for(std::chrono::microseconds(1));
	}
}




template<class PoolType>
void __test_mem_pool_object(PoolType* pool, int repetitions, int * res)
{
	*res = 0;
	int alloc_count = 0;
	using value_type = typename PoolType::value_type;
	for (int p = 0; p < repetitions; ++p) {
		
		std::vector<typename PoolType::value_type*> vec(get_count(repetitions,p) * 2);

		//Allocate lots of elements
		for (size_t i = 0; i < vec.size()/2; ++i) {
			vec[i] = pool->allocate(1);
			alloc_count++;
			memset(vec[i], 0, sizeof(value_type));
			//*res += read_32(vec[i]);
		}
		//deallocate 20%
		for (size_t i = 0; i < vec.size()/2; i += 5) {
			pool->deallocate(vec[i],1);
			vec[i] = NULL;
		}

		//allocate remaining
		for (size_t i = vec.size()/2; i < vec.size(); ++i) {
			vec[i] = pool->allocate(1);
			alloc_count++;
			memset(vec[i], 0, sizeof(value_type));
			//*res += read_32(vec[i]);
		}
		//deallocate remaining
		for (size_t i = 0; i < vec.size() ; i ++) {
			
			if(vec[i]) pool->deallocate(vec[i],1);
		}
		
	}
	
}
template<class PoolType>
int __test_mem_pool_type(PoolType & pool, int nthreads, int repetitions)
{
	int res = 0;
	std::vector<std::thread*> threads(nthreads);

	for (int i = 0; i < nthreads; ++i) {
		threads[i] = new std::thread(std::bind(__test_mem_pool_object<PoolType>, &pool, repetitions, &res));
	}

	for (int i = 0; i < nthreads; ++i) {
		threads[i]->join();
		delete threads[i];
	}
	return res;
}
/**
* \internal
* Test multithreaded allocation/deallocation.
* Pairs of allocation/deallocation are made in the same thread.
*/
template<class T>
void test_mem_pool_separate_threads(int nthreads, int repetitions)
{
	std::cout << "test alloc/dealloc in separate threads (" << nthreads << ") with the same pool" << std::endl;;

	get_count(repetitions, 0);
	size_t mem, start, el;

	reset_memory_usage();
	mem = get_memory_usage();
	start = detail::msecs_since_epoch();
	StdPool<T> p;
	__test_mem_pool_type<StdPool<T> >(p,nthreads, repetitions);
	el = detail::msecs_since_epoch() - start;
	mem = get_memory_usage() - mem;
	std::cout << "malloc/free: "<<el << " ms  " << (mem / (1024 * 1024)) << " MO" << std::endl;
	
	{
		reset_memory_usage();
		mem = get_memory_usage();
		start = detail::msecs_since_epoch();
		parallel_object_pool<T, std::allocator<T>, 0, linear_object_allocation<1>,true> pp;
		pp.set_reclaim_memory(true);
		__test_mem_pool_type(pp, nthreads, repetitions);
		
		pp.clear();
		el = detail::msecs_since_epoch() - start;
		mem = get_memory_usage() - mem;
		std::cout << "parallel_object_pool: "<< el <<" ms  "<< (mem / (1024 * 1024)) <<" MO " << std::endl;
	}
	
	
/*#ifndef __MINGW32__
	reset_memory_usage();
	//mem = get_memory_usage();
	start = detail::msecs_since_epoch();
	MimallocPool<T> mp;
	r=test_mem_pool_type(mp, nthreads, repetitions);
	el = detail::msecs_since_epoch() - start;
	//mem = get_memory_usage() - mem;
	std::cout<<"mi_malloc/mi_free " << << " threads: " << << " ms  " << << " MO   " << << "\n", nthreads, (int)el, (int)(mem / (1024 * 1024)),r);
#endif
	*/
	fflush(stdout);
}






template<class PoolType>
void __test_allocate_one_thread(PoolType* pool, std::atomic<void*> * vec, int size)
{
	for (int i = 0; i < size; ++i) {
		vec[i] =pool->allocate(1);
	}
}
template<class PoolType>
void __test_deallocate_one_thread(PoolType* pool, std::atomic<void*>* vec, int size)
{
	using value_type = typename PoolType::value_type;
	for (int i = 0; i < size; ++i) {
		while (!vec[i])
			;
		pool->deallocate(static_cast<value_type*>(static_cast<void*>(vec[i])),1);
	}
}
template<class PoolType>
int __test_alloc_dealloc_separate_threads(PoolType& pool, int nthreads ,int count)
{
	std::vector< std::atomic<void*>* > nar(nthreads);
	for (int i = 0; i < nthreads; ++i)
	{
		nar[i] = new std::atomic<void*>[count];
		std::atomic<void*>* tmp = nar[i];
		for (int j = 0; j < count; ++j)
			tmp[j] = NULL;
	}

	std::vector <std::thread*> threads(nthreads * 2);

	for (int i = 0; i < nthreads; ++i) {
		
		threads[i *2 +1] = new std::thread(__test_deallocate_one_thread<PoolType>, &pool, nar[i], count); 
		threads[i * 2] = new std::thread(__test_allocate_one_thread<PoolType>, &pool, nar[i], count);
	}
	//bool finished = false;
	//std::thread * clear_thread = new std::thread(std::bind(test_mem_pool_release_thread<PoolType>, &pool, &finished));

	for (int i = 0; i < nthreads; ++i) {
		threads[i * 2]->join();
		threads[i * 2 + 1]->join();
		delete threads[i * 2];
		delete threads[i * 2 + 1];
	}
	//finished = true;
	//clear_thread->join();
	//delete clear_thread;

	for (int i = 0; i < nthreads; ++i)
		delete[] nar[i];
	return 0;
}

template<class T>
void test_alloc_dealloc_separate_threads(int nthreads, int count)
{
	std::cout << "test alloc in one thread, deallocate in another thread (" << nthreads << ") with the same pool"<<std::endl;
	size_t mem, start, el;
	
	reset_memory_usage();
	mem = get_memory_usage();
	start = detail::msecs_since_epoch();
	StdPool<T> p;
	__test_alloc_dealloc_separate_threads<StdPool<T> >(p, nthreads, count);
	el = detail::msecs_since_epoch() - start;
	mem = get_memory_usage() - mem;
	std::cout << "malloc/free: " << el << " ms  " << (mem / (1024 * 1024)) << " MO" << std::endl;
	

	
	{
		reset_memory_usage();
		mem = get_memory_usage();
		start = detail::msecs_since_epoch();
		parallel_object_pool<T, std::allocator<T>, 0> pp;
		pp.set_reclaim_memory(true);
		__test_alloc_dealloc_separate_threads(pp, nthreads, count);
		pp.clear();
		el = detail::msecs_since_epoch() - start;
		mem = get_memory_usage() - mem;
		std::cout << "parallel_object_pool: " << el << " ms  " <<(mem / (1024 * 1024)) << " MO" << std::endl;
	}
	
	/*
#ifndef __MINGW32__
	mem = get_memory_usage();
	start = detail::msecs_since_epoch();
	MimallocPool<T> mp;
	r = test_alloc_dealloc_separate_threads(mp, nthreads, count);
	el = detail::msecs_since_epoch() - start;
	mem = get_memory_usage() - mem;
	std::cout<<"mi_malloc/mi_free " << << " threads: " << << " ms  " << << " MO   " << << "\n", nthreads, (int)el, (int)(mem / (1024 * 1024)), r);
#endif*/
}


template<class T>
void test_monothread_alloc_only(int count)
{
	std::cout << "test allocation/deallocation of " << count << " object of size " << sizeof(T) << " one by one" << std::endl;
	std::vector<T*> vec(count);
	size_t start, el;

	start = detail::msecs_since_epoch();
	for (int i = 0; i < count; ++i) {
		vec[i] = static_cast<T*>(malloc(sizeof(T)));
		memset(vec[i], 0, sizeof(T));
	}
	for (int i = 0; i < count; ++i)
		free(vec[i]);
	el = detail::msecs_since_epoch() - start;
	std::cout << "malloc/free: " << el << " ms" << std::endl;

	using pool_type = object_pool < T, std::allocator<T>,0,linear_object_allocation< 1>,false,false > ;
	{
		pool_type pa;
		pa.set_reclaim_memory(false);

		start = detail::msecs_since_epoch();
		for (int i = 0; i < count; ++i) {
			vec[i] = pa.allocate(1);
			memset(vec[i], 0, sizeof(T));
		}
		for (int i = 0; i < count; ++i)
			pa.deallocate(vec[i],1);
		el = detail::msecs_since_epoch() - start;
		std::cout << "object_pool: " << el << " ms" << std::endl;

		
		start = detail::msecs_since_epoch();
		for (int i = 0; i < count; ++i) {
			vec[i] = pa.allocate(1);
			memset(vec[i], 0, sizeof(T));
		}
		for (int i = 0; i < count; ++i)
			pa.deallocate(vec[i], 1);
		el = detail::msecs_since_epoch() - start;
		std::cout << "object_pool preallocated: " << el << " ms" << std::endl;

		object_pool < T, std::allocator<T>, 0, linear_object_allocation< 1>,true, false > pa2;
		start = detail::msecs_since_epoch();
		for (int i = 0; i < count; ++i) {
			vec[i] = pa2.allocate(1);
			memset(vec[i], 0, sizeof(T));
		}
		for (int i = 0; i < count; ++i)
			pa2.deallocate(vec[i], 1);
		el = detail::msecs_since_epoch() - start;
		std::cout << "object_pool enable unique_ptr: " << el << " ms" << std::endl;
	}
	
	{
		parallel_object_pool<T, std::allocator<T>, 0> pa;
		start = detail::msecs_since_epoch();
		for (int i = 0; i < count; ++i) {
			vec[i] = pa.allocate(1);
			memset(vec[i], 0, sizeof(T));
		}
		for (int i = 0; i < count; ++i)
			pa.deallocate(vec[i], 1);
		el = detail::msecs_since_epoch() - start;
		std::cout << "parallel_object_pool: " << el << " ms" << std::endl;
	}
	
#ifndef __MINGW32__
	/*start = detail::msecs_since_epoch();
	for (int i = 0; i < count; ++i) {
		vec[i] = (T*)mi_malloc(sizeof(T));
		memset(vec[i], 0, sizeof(T));
	}
	for (int i = 0; i < count; ++i)
		mi_free(vec[i]);
	el = detail::msecs_since_epoch() - start;
	std::cout<<"mi_malloc/mi_free: " << << " ms\n", (int)el); fflush(stdout);*/
#endif
	std::cout << std::endl;
}













template<class PoolType>
void __test_mem_pool_random_pattern(PoolType* pool, int count)
{
	using value_type = typename PoolType::value_type;
	std::vector<value_type*> vec(static_cast<size_t>(MY_RAND_MAX +1U),NULL);

	for (int i = 0; i < count; ++i) {
		int index = (rand() & MY_RAND_MAX);
		if (vec[index]) {
			pool->deallocate(vec[index],1);
			vec[index] = NULL;
		}
		else {
			vec[index] = pool->allocate(1);
			memset(vec[index], 0, sizeof(value_type));
		}
	}
}

template<class PoolType>
int __test_mem_pool_random(PoolType& pool, int nthreads, int count)
{
	int res = 0;
	std::vector<std::thread*> threads(nthreads);

	for (int i = 0; i < nthreads; ++i) {
		threads[i] = new std::thread(std::bind(__test_mem_pool_random_pattern<PoolType>, &pool, count));
	}

	for (int i = 0; i < nthreads; ++i) {
		threads[i]->join();
		delete threads[i];
	}
	return res;
}

/**
* \internal
* Test multithreaded allocation/deallocation.
* Pairs of allocation/deallocation are made in the same thread.
*/
template<class T>
void test_mem_pool_random_patterns(int nthreads, int repetitions)
{
	std::cout << "test randomly mixing alloc/dealloc in " << nthreads << " separate threads with the same pool" << std::endl;
	size_t mem, start, el;

	reset_memory_usage();
	mem = get_memory_usage();
	start = detail::msecs_since_epoch();
	StdPool<T> p;
	__test_mem_pool_random<StdPool<T> >(p, nthreads, repetitions);
	el = detail::msecs_since_epoch() - start;
	mem = get_memory_usage() - mem;
	std::cout << "malloc/free: " << el << " ms  " << mem / (1024 * 1024) << " MO" << std::endl;

	
	{
		reset_memory_usage();
		mem = get_memory_usage();
		start = detail::msecs_since_epoch();
		parallel_object_pool<T, std::allocator<T>, 0> pp;
		pp.set_reclaim_memory(false);
		__test_mem_pool_random(pp, nthreads, repetitions);
		pp.clear();
		el = detail::msecs_since_epoch() - start;
		mem = get_memory_usage() - mem;
		std::cout << "parallel_object_pool: " << el << " ms  " << mem / (1024 * 1024) << " MO" << std::endl;
	}
	/*
#ifndef __MINGW32__
	reset_memory_usage();
	mem = get_memory_usage();
	start = detail::msecs_since_epoch();
	MimallocPool<T> mp;
	r = test_mem_pool_random(mp, nthreads, repetitions);
	el = detail::msecs_since_epoch() - start;
	mem = get_memory_usage() - mem;
	std::cout<<"mi_malloc/mi_free " << << " threads: " << << " ms  " << << " MO   " << << "\n", nthreads, (int)el, (int)(mem / (1024 * 1024)), r);
#endif*/
}










template<class PoolType>
void __test_mem_pool_random_pattern_random_size(PoolType* pool,int /*th_index*/, std::vector<size_t> * sizes)
{
	using value_type = typename PoolType::value_type;
	using pair = std::pair<value_type*, size_t>;
	std::vector<pair> vec(static_cast<size_t>(MY_RAND_MAX + 1U), pair(NULL,0));

	for (size_t i = 0; i < sizes->size(); ++i) {
		int index = (rand() & MY_RAND_MAX);
		if (vec[index].first) {
			pool->deallocate(vec[index].first, vec[index].second);
			vec[index].first = NULL;
		}
		else {
			vec[index] = pair( pool->allocate((*sizes)[i]), (*sizes)[i]);
			memset(vec[index].first, 0, sizeof(value_type)* (*sizes)[i]);
		}
	}
	for (size_t i = 0; i < vec.size(); ++i)
		if(vec[i].first)
			pool->deallocate(vec[i].first, vec[i].second);
}

template<size_t MaxSize, class PoolType>
int __test_mem_pool_random_size(PoolType& pool, int nthreads, int count)
{
	int res = 0;
	std::vector<std::thread*> threads(nthreads);
	std::vector<size_t> sizes(count);
	for (int i = 0; i < count; ++i)
		sizes[i] = (rand() & MY_RAND_MAX) % (MaxSize - 1) + 1;

	for (int i = 0; i < nthreads; ++i) {
		threads[i] = new std::thread(std::bind(__test_mem_pool_random_pattern_random_size<PoolType>, &pool,i, &sizes));
	}

	for (int i = 0; i < nthreads; ++i) {
		threads[i]->join();
		delete threads[i];
	}
	return res;
}

/**
* \internal
* Test multithreaded allocation/deallocation.
* Pairs of allocation/deallocation are made in the same thread.
*/
template<size_t MaxSize,class T>
void test_mem_pool_random_patterns_random_size(int nthreads, int repetitions)
{
	std::cout << "test randomly mixing alloc/dealloc of random size (up to " << MaxSize << ") in " << nthreads << " separate threads with the same pool" << std::endl;

	size_t mem, start, el;

	reset_memory_usage();
	mem = get_memory_usage();
	start = detail::msecs_since_epoch();
	StdPool<T> p;
	__test_mem_pool_random_size<MaxSize,StdPool<T> >(p, nthreads, repetitions);
	el = detail::msecs_since_epoch() - start;
	mem = get_memory_usage() - mem;
	std::cout << "malloc/free: " << el << " ms  " << mem / (1024 * 1024) << " MO" << std::endl;


	{
		reset_memory_usage();
		mem = get_memory_usage();
		start = detail::msecs_since_epoch();
		parallel_object_pool<T, std::allocator<T>, 0,linear_object_allocation<MaxSize> > pp;
		pp.set_reclaim_memory(false);
		__test_mem_pool_random_size < MaxSize>(pp, nthreads, repetitions);
		pp.clear();
		el = detail::msecs_since_epoch() - start;
		mem = get_memory_usage() - mem;
		std::cout << "parallel_object_pool: " << el << " ms  " << mem / (1024 * 1024) << " MO" << std::endl;
	}

#ifndef __MINGW32__
	/*reset_memory_usage();
	mem = get_memory_usage();
	start = detail::msecs_since_epoch();
	MimallocPool<T> mp;
	r = test_mem_pool_random_size < MaxSize>(mp, nthreads, repetitions);
	el = detail::msecs_since_epoch() - start;
	mem = get_memory_usage() - mem;
	std::cout<<"mi_malloc/mi_free " << << " threads: " << << " ms  " << << " MO   " << << "\n", nthreads, (int)el, (int)(mem / (1024 * 1024)), r);*/
#endif
	fflush(stdout);
}









template<class PoolType>
void __test_mem_pool_interrupt_clear_thread(PoolType* pool, int count)
{
	using value_type = typename PoolType::value_type;
	std::vector<value_type*> vec(count, NULL);
	// allocate and never deallocate
	for (int i = 0; i < count; ++i) {
		//std::cout<<"alloc\n");
		(vec)[i] = pool->allocate(1);
		//std::cout<<"end\n");
		//memset((vec)[i], 0, sizeof(value_type));
	}
}
template<class PoolType>
void __test_mem_pool_clear_thread(PoolType* pool, bool* finish)
{
	//using value_type = typename PoolType::value_type;

	// always clear
	while (!*finish) {
		pool->clear();

		//std::this_thread::yield();
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}
template<class PoolType>
int __test_mem_pool_interrupt_clear(PoolType& pool, int nthreads, int count)
{
	int res = 0;

	std::thread *clear_thread;
	std::vector<std::thread*> threads(nthreads);

	for (int i = 0; i < nthreads; ++i) {
		threads[i] = new std::thread(std::bind(__test_mem_pool_interrupt_clear_thread<PoolType>, &pool, count));
	}
	bool finished = false;
	clear_thread = new std::thread(std::bind(__test_mem_pool_clear_thread<PoolType>, &pool, &finished));

	for (int i = 0; i < nthreads; ++i) {
		threads[i]->join();
		delete threads[i];
	}
	finished = true;
	clear_thread->join();
	delete clear_thread;
	return res;
}
template<class T>
void test_mem_pool_interrupt_clear(int nthreads, int count)
{
	std::cout << "test allocating in " << nthreads << " threads while calling clear() every ms in another thread" << std::endl;
	size_t mem, start, el;
	
	{
		reset_memory_usage();
		mem = get_memory_usage();
		start = detail::msecs_since_epoch();
		parallel_object_pool<T, std::allocator<T>, 0> pp;
		pp.set_reclaim_memory(true);
		__test_mem_pool_interrupt_clear(pp, nthreads, count);
		pp.clear();
		el = detail::msecs_since_epoch() - start;
		mem = get_memory_usage() - mem;
		std::cout << "parallel_object_pool: " << el << " ms  " << mem / (1024 * 1024) << " MO" << std::endl;
	}

	fflush(stdout);
}








template<class PoolType>
void __test_mem_pool_reset_thread(PoolType* pool, bool* finish)
{
	//using value_type = typename PoolType::value_type;

	// always clear
	while (!*finish) {
		pool->reset();

		//std::this_thread::yield();
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
}
template<class PoolType>
void __test_mem_pool_interrupt_reset_thread(PoolType* pool, int count)
{
	using value_type = typename PoolType::value_type;
	std::vector<value_type*> vec(count, NULL);
	// allocate and never deallocate
	for (int i = 0; i < count; ++i) {
		(vec)[i] = pool->allocate(1);
		//memset((vec)[i], 0, sizeof(value_type));
	}
}

template<class PoolType>
int __test_mem_pool_interrupt_reset(PoolType& pool, int nthreads, int count)
{
	int res = 0;

	std::thread* clear_thread;
	std::vector<std::thread*> threads(nthreads);

	for (int i = 0; i < nthreads; ++i) {
		threads[i] = new std::thread(std::bind(__test_mem_pool_interrupt_reset_thread<PoolType>, &pool, count));
	}
	bool finished = false;
	clear_thread = new std::thread(std::bind(__test_mem_pool_reset_thread<PoolType>, &pool, &finished));

	for (int i = 0; i < nthreads; ++i) {
		threads[i]->join();
		delete threads[i];
	}
	finished = true;
	clear_thread->join();
	delete clear_thread;
	return res;
}
template<class T>
void test_mem_pool_interrupt_reset(int nthreads, int count)
{
	std::cout << "test allocating in " << nthreads << " threads while calling reset() every ms in another thread" << std::endl;
	size_t mem, start, el;

	{
		reset_memory_usage();
		mem = get_memory_usage();
		start = detail::msecs_since_epoch();
		parallel_object_pool<T, std::allocator<T>, 0> pp;
		pp.set_reclaim_memory(true);
		__test_mem_pool_interrupt_reset(pp, nthreads, count);
		pp.clear();
		el = detail::msecs_since_epoch() - start;
		mem = get_memory_usage() - mem;
		std::cout << "parallel_object_pool " << nthreads << " threads: " << el << " ms  " << mem / (1024 * 1024) << " MO" << std::endl;
	}

}






template<class PoolType>
int _test_unique_ptr(PoolType& pool, int count)
{
	std::vector<typename PoolType::unique_ptr> vec(count);
	for (int i = 0; i < count; ++i) {
		vec[i] = pool.make_unique(0);
	} 
	return 0;
}

template<class T>
void test_mem_pool_unique_ptr(int count)
{
	std::cout << "test allocate/deallocate " << count << " unique_ptr of size " << sizeof(T) << std::endl;
	size_t mem, start, el;
	{
		reset_memory_usage();
		mem = get_memory_usage();
		start = detail::msecs_since_epoch();
		{
			StdPool<T> pp;
			_test_unique_ptr(pp ,count);
		}
		el = detail::msecs_since_epoch() - start;
		mem = get_memory_usage() - mem;
		std::cout << "malloc: " << el << " ms  " << mem / (1024 * 1024) << " MO" << std::endl;
	}
	{
		reset_memory_usage();
		mem = get_memory_usage();
		start = detail::msecs_since_epoch();
		{
			object_pool<T, std::allocator<T>, 0, linear_object_allocation<>, true> pp;
			_test_unique_ptr(pp, count);
		}
		el = detail::msecs_since_epoch() - start;
		mem = get_memory_usage() - mem;
		std::cout << "object_pool : " << el << " ms  " << mem / (1024 * 1024) << " MO" << std::endl;
	}
	{
		reset_memory_usage();
		mem = get_memory_usage();
		start = detail::msecs_since_epoch();
		{
			parallel_object_pool<T> pp;
			_test_unique_ptr(pp, count);
		}
		el = detail::msecs_since_epoch() - start;
		mem = get_memory_usage() - mem;
		std::cout << "parallel_object_pool: " << el << " ms  " << mem / (1024 * 1024) << " MO" << std::endl;
	}
}
















template<class T, size_t MaxSize>
void test_multipl_size_monthread(int count)
{
	using pair = std::pair<T*, size_t>;
	std::vector<pair> vec(count);
	std::vector<size_t> sizes(count);
	size_t start, el;

	srand(detail::msecs_since_epoch());
	for (int i = 0; i < count; ++i)
		sizes[i] = (rand() & MY_RAND_MAX) % (MaxSize - 1) + 1;

	start = detail::msecs_since_epoch();
	for (int i = 0; i < count; ++i) {
		vec[i] =pair(static_cast<T*>(malloc(sizeof(T) * sizes[i])), sizes[i]);
		memset(vec[i].first, 0, sizeof(T)* sizes[i]);
	}
	for (int i = 0; i < count; ++i)
		free(vec[i].first);
	el = detail::msecs_since_epoch() - start;
	std::cout << "malloc: " << el << " ms"<< el << std::endl;


	{
		{
			object_pool < T, std::allocator<T>, 0, block_object_allocation< MaxSize,8> > pa;
			pa.set_reclaim_memory(false);
			start = detail::msecs_since_epoch();
			for (int i = 0; i < count; ++i) {
				vec[i] = pair(pa.allocate(sizes[i]), sizes[i]);
				memset(vec[i].first, 0, sizeof(T) * sizes[i]);
			}
			for (int i = 0; i < count; ++i)
				pa.deallocate(vec[i].first, vec[i].second);
			el = detail::msecs_since_epoch() - start;
			std::cout << "object_pool: " << el << " ms" << std::endl;

			start = detail::msecs_since_epoch();
			for (int i = 0; i < count; ++i) {
				vec[i] = pair(pa.allocate(sizes[i]), sizes[i]);
				memset(vec[i].first, 0, sizeof(T) * sizes[i]);
			}
			for (int i = 0; i < count; ++i)
				pa.deallocate(vec[i].first, vec[i].second);
			el = detail::msecs_since_epoch() - start;
			std::cout << "pool_allocator2: " << el << " ms" << std::endl;
		}
	}

	{
		parallel_object_pool < T, std::allocator<T>, 0, block_object_allocation< MaxSize, 8> > pa;
		pa.set_reclaim_memory(false);
		start = detail::msecs_since_epoch();
		for (int i = 0; i < count; ++i) {
			vec[i] = pair(pa.allocate(sizes[i]), sizes[i]);
			memset(vec[i].first, 0, sizeof(T) * sizes[i]);
		}
		for (int i = 0; i < count; ++i)
			pa.deallocate(vec[i].first, vec[i].second);
		el = detail::msecs_since_epoch() - start;
		std::cout << "parallel_object_pool: " << el << " ms" << std::endl;

		start = detail::msecs_since_epoch();
		for (int i = 0; i < count; ++i) {
			vec[i] = pair(pa.allocate(sizes[i]), sizes[i]);
			memset(vec[i].first, 0, sizeof(T) * sizes[i]);
		}
		for (int i = 0; i < count; ++i)
			pa.deallocate(vec[i].first, vec[i].second);
		el = detail::msecs_since_epoch() - start;
		std::cout<<"parallel_pool_allocator2: " << el<< " ms"<< std::endl;
	}


	
}












inline void test_pow2_allocation(int count)
{
	size_t mem, mem_alloc, mem_dealloc, start, el;
	reset_memory_usage();
	//size_t start_mem = get_memory_usage();

	size_t max_size = 1024;
	std::vector<size_t> sizes(count);
	size_t total = 0;
	for (int i = 0; i < count; ++i) {
		sizes[i] = (rand() & MY_RAND_MAX) % max_size + 1;
		total += sizes[i];
	}
	std::cout << "theoric size: " << total / (1024 * 1024) << std::endl;

	std::vector <char*> std_pool(count);



	reset_memory_usage();
	mem = get_memory_usage();
	start = detail::msecs_since_epoch();
	object_pool<char, std::allocator<char>, 0, pow_object_allocation< 1024,16, 4> > pool;
	for (int i = 0; i < count; ++i)
		std_pool[i] = pool.allocate(sizes[i]);
	el = detail::msecs_since_epoch() - start;
	object_pool_stats stats;
	pool.dump_statistics(stats);
	//mem_alloc = get_memory_usage() - mem;
	for (int i = 0; i < count; ++i)
		pool.deallocate(std_pool[i], sizes[i]);
	mem_dealloc = get_memory_usage() - mem;
	std::cout << "object_pool: " << el << " ms  " << stats.memory / (1024 * 1024) << " MO and " << (mem_dealloc / (1024 * 1024)) << " MO" << std::endl;



	reset_memory_usage();
	mem = get_memory_usage();
	start = detail::msecs_since_epoch();
	for (int i = 0; i < count; ++i)
		std_pool[i] = static_cast<char*>(malloc(sizes[i]));
	el = detail::msecs_since_epoch() - start;
	mem_alloc = get_memory_usage() - mem;
	for (int i = 0; i < count; ++i)
		free(std_pool[i]);
	mem_dealloc = get_memory_usage() - mem;
	std::cout << "malloc/free: " << el << " ms  " << (mem_alloc / (1024 * 1024)) << " MO and " << (mem_dealloc / (1024 * 1024)) << " MO" << std::endl;

#ifndef __MINGW32__
	/*reset_memory_usage();
	mem = get_memory_usage();
	start = detail::msecs_since_epoch();
	for (int i = 0; i < count; ++i)
		std_pool[i] = (char*)mi_malloc(sizes[i]);
	el = detail::msecs_since_epoch() - start;
	mem_alloc = get_memory_usage() - mem;
	for (int i = 0; i < count; ++i)
		mi_free(std_pool[i]);
	mem_dealloc = get_memory_usage() - mem;
	std::cout<<"mi_malloc/mi_free: " << << " ms  " << << " MO and " << << " MO\n", (int)el, (int)(mem_alloc / (1024 * 1024)), (int)(mem_dealloc / (1024 * 1024)));
	*/

#endif
}



void test_object_pool(int rep)
{
	std::cout << std::endl;
	/* {
		disable_ostream d(std::cout);
		test_monothread_alloc_only<size_t>(rep);
		test_mem_pool_unique_ptr<size_t>(rep);
	}*/
	for (int nthreads = 15; nthreads < 16; ++nthreads) 
	{
		std::cout << "test parallel_object_pool for " << nthreads << " thread(s)" << std::endl;
		{
			disable_ostream d(std::cout);
			test_mem_pool_separate_threads<size_t>(nthreads, 50);
			//printf("tested separate_threads\n"); fflush(stdout);
			test_alloc_dealloc_separate_threads< size_t>(nthreads, rep);
			//printf("tested dealloc_separate_threads\n"); fflush(stdout);
			test_mem_pool_random_patterns<size_t>(nthreads, rep);
			//printf("tested random_patterns\n"); fflush(stdout);
			test_mem_pool_random_patterns_random_size<32, size_t >(nthreads, rep);
			//printf("tested random_patterns_random_size\n"); fflush(stdout);
			test_mem_pool_interrupt_clear<size_t>(nthreads, rep);
			//printf("tested interrupt_clear\n"); fflush(stdout);
			test_mem_pool_interrupt_reset<size_t>(nthreads, rep);
			//printf("tested interrupt_reset\n"); fflush(stdout);
		}
		
	}
}