#pragma once

#include <thread>
#include <vector>

#include "testing.hpp"
//#include "mimalloc.h"
#include "ordered_map.hpp"
#include "memory.hpp"

template<class T>
struct StdPool
{
	using unique_ptr = std::unique_ptr<T>;
	using shared_ptr = std::shared_ptr<T>;
	using value_type = T;
	T* allocate(size_t size=1) { return (T*)malloc(sizeof(T)*size); }
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

static inline size_t get_count(int reps, int step) {
	static std::vector<size_t> counts;
	if (counts.size() != reps) {
		counts.resize(reps);
		srand(NULL);
		for (int i = 0; i < reps; ++i)
			counts[i] = rand();
	}
	return counts[step];
}

template<class PoolType>
void test_mem_pool_release_thread(PoolType* pool, bool* finish)
{
	using value_type = typename PoolType::value_type;

	// always clear
	while (!*finish) {
		size_t c = pool->release_unused_memory();
		if(c)printf("released %i\n", (int)c);
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
		if (p == 1)
			bool stop = true;
		std::vector<typename PoolType::value_type*> vec(get_count(repetitions,p) * 2);

		//Allocate lots of elements
		for (int i = 0; i < vec.size()/2; ++i) {
			vec[i] = pool->allocate(1);
			alloc_count++;
			memset(vec[i], 0, sizeof(value_type));
			//*res += read_32(vec[i]);
		}
		//deallocate 20%
		for (int i = 0; i < vec.size()/2; i += 5) {
			pool->deallocate(vec[i]);
			vec[i] = NULL;
		}

		//allocate remaining
		for (int i = vec.size()/2; i < vec.size(); ++i) {
			vec[i] = pool->allocate(1);
			alloc_count++;
			memset(vec[i], 0, sizeof(value_type));
			//*res += read_32(vec[i]);
		}
		//deallocate remaining
		for (int i = 0; i < vec.size() ; i ++) {
			if (i == vec.size() - 1)
				bool stop = true;
			if(vec[i]) pool->deallocate(vec[i]);
		}
		
	}
	
	alloc_count;
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
	printf("test alloc/dealloc in separate threads (%i) with the same pool\n", nthreads);

	get_count(repetitions, 0);
	size_t mem, start, el;
	int r;

	reset_memory_usage();
	mem = get_memory_usage();
	start = msecsSinceEpoch();
	StdPool<T> p;
	__test_mem_pool_type<StdPool<T> >(p,nthreads, repetitions);
	el = msecsSinceEpoch() - start;
	mem = get_memory_usage() - mem;
	printf("malloc/free: %i ms  %i MO\n",(int)el, (int)(mem / (1024 * 1024)));
	
	{
		reset_memory_usage();
		mem = get_memory_usage();
		start = msecsSinceEpoch();
		parallel_object_pool<T, std::allocator<T>, 0, linear_object_allocation<1>,true> pp;
		pp.set_reclaim_memory(true);
		r = __test_mem_pool_type(pp, nthreads, repetitions);
		
		pp.clear();
		el = msecsSinceEpoch() - start;
		mem = get_memory_usage() - mem;
		printf("parallel_object_pool: %i ms  %i MO \n", (int)el, (int)(mem / (1024 * 1024)));
	}
	
	
/*#ifndef __MINGW32__
	reset_memory_usage();
	//mem = get_memory_usage();
	start = msecsSinceEpoch();
	MimallocPool<T> mp;
	r=test_mem_pool_type(mp, nthreads, repetitions);
	el = msecsSinceEpoch() - start;
	//mem = get_memory_usage() - mem;
	printf("mi_malloc/mi_free %i threads: %i ms  %i MO   %i\n", nthreads, (int)el, (int)(mem / (1024 * 1024)),r);
#endif
	*/
	fflush(stdout);
}






template<class PoolType>
void __test_allocate_one_thread(PoolType* pool, std::atomic<void*> * vec, int size)
{
	for (int i = 0; i < size; ++i) {
		vec[i] = (void*)pool->allocate(1);
	}
	bool end = true;
}
template<class PoolType>
void __test_deallocate_one_thread(PoolType* pool, std::atomic<void*>* vec, int size)
{
	using value_type = typename PoolType::value_type;
	for (int i = 0; i < size; ++i) {
		while (!vec[i])
			bool stop = true;
		pool->deallocate((value_type*)(void*)vec[i]);
	}
	bool end = true;
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
	printf("test alloc in one thread, deallocate in another thread (%i) with the same pool\n", nthreads);
	size_t mem, start, el;
	int r;
	
	reset_memory_usage();
	mem = get_memory_usage();
	start = msecsSinceEpoch();
	StdPool<T> p;
	__test_alloc_dealloc_separate_threads<StdPool<T> >(p, nthreads, count);
	el = msecsSinceEpoch() - start;
	mem = get_memory_usage() - mem;
	printf("malloc/free: %i ms  %i MO\n", (int)el, (int)(mem / (1024 * 1024)));
	

	
	{
		reset_memory_usage();
		mem = get_memory_usage();
		start = msecsSinceEpoch();
		parallel_object_pool<T, std::allocator<T>, 0> pp;
		pp.set_reclaim_memory(true);
		r = __test_alloc_dealloc_separate_threads(pp, nthreads, count);
		pp.clear();
		el = msecsSinceEpoch() - start;
		mem = get_memory_usage() - mem;
		printf("parallel_object_pool: %i ms  %i MO\n", (int)el, (int)(mem / (1024 * 1024)));
	}
	
	/*
#ifndef __MINGW32__
	mem = get_memory_usage();
	start = msecsSinceEpoch();
	MimallocPool<T> mp;
	r = test_alloc_dealloc_separate_threads(mp, nthreads, count);
	el = msecsSinceEpoch() - start;
	mem = get_memory_usage() - mem;
	printf("mi_malloc/mi_free %i threads: %i ms  %i MO   %i\n", nthreads, (int)el, (int)(mem / (1024 * 1024)), r);
#endif*/
}


template<class T>
void test_monothread_alloc_only(int count)
{
	printf("test allocation/deallocation of %i object of size %i one by one\n", count, sizeof(T));
	std::vector<T*> vec(count);
	size_t start, el;

	start = msecsSinceEpoch();
	for (int i = 0; i < count; ++i) {
		vec[i] = (T*)malloc(sizeof(T));
		memset(vec[i], 0, sizeof(T));
	}
	for (int i = 0; i < count; ++i)
		free(vec[i]);
	el = msecsSinceEpoch() - start;
	printf("malloc/free: %i ms\n", (int)el); fflush(stdout);

	using pool_type = object_pool < T, std::allocator<T>,0,linear_object_allocation< 1>,false,false > ;
	{
		pool_type pa;
		pa.set_reclaim_memory(false);

		start = msecsSinceEpoch();
		for (int i = 0; i < count; ++i) {
			vec[i] = pa.allocate(1);
			memset(vec[i], 0, sizeof(T));
		}
		for (int i = 0; i < count; ++i)
			pa.deallocate(vec[i],1);
		el = msecsSinceEpoch() - start;
		printf("object_pool: %i ms\n", (int)el); fflush(stdout);

		
		start = msecsSinceEpoch();
		for (int i = 0; i < count; ++i) {
			vec[i] = pa.allocate(1);
			memset(vec[i], 0, sizeof(T));
		}
		for (int i = 0; i < count; ++i)
			pa.deallocate(vec[i], 1);
		el = msecsSinceEpoch() - start;
		printf("object_pool preallocated: %i ms\n", (int)el); fflush(stdout);

		object_pool < T, std::allocator<T>, 0, linear_object_allocation< 1>,true, false > pa2;
		start = msecsSinceEpoch();
		for (int i = 0; i < count; ++i) {
			vec[i] = pa2.allocate(1);
			memset(vec[i], 0, sizeof(T));
		}
		for (int i = 0; i < count; ++i)
			pa2.deallocate(vec[i], 1);
		el = msecsSinceEpoch() - start;
		printf("object_pool enable unique_ptr: %i ms\n", (int)el); fflush(stdout);
	}
	
	{
		parallel_object_pool<T, std::allocator<T>, 0> pa;
		start = msecsSinceEpoch();
		for (int i = 0; i < count; ++i) {
			vec[i] = pa.allocate(1);
			memset(vec[i], 0, sizeof(T));
		}
		for (int i = 0; i < count; ++i)
			pa.deallocate(vec[i], 1);
		el = msecsSinceEpoch() - start;
		printf("parallel_object_pool: %i ms\n", (int)el); fflush(stdout);
	}
	
#ifndef __MINGW32__
	/*start = msecsSinceEpoch();
	for (int i = 0; i < count; ++i) {
		vec[i] = (T*)mi_malloc(sizeof(T));
		memset(vec[i], 0, sizeof(T));
	}
	for (int i = 0; i < count; ++i)
		mi_free(vec[i]);
	el = msecsSinceEpoch() - start;
	printf("mi_malloc/mi_free: %i ms\n", (int)el); fflush(stdout);*/
#endif
	printf("\n");
}













template<class PoolType>
void __test_mem_pool_random_pattern(PoolType* pool, int count)
{
	using value_type = typename PoolType::value_type;
	std::vector<value_type*> vec(RAND_MAX+1,NULL);

	for (int i = 0; i < count; ++i) {
		int index = rand();
		if (vec[index]) {
			pool->deallocate(vec[index]);
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
	printf("test randomly mixing alloc/dealloc in %i separate threads with the same pool\n", nthreads);
	size_t mem, start, el;
	int r;

	reset_memory_usage();
	mem = get_memory_usage();
	start = msecsSinceEpoch();
	StdPool<T> p;
	__test_mem_pool_random<StdPool<T> >(p, nthreads, repetitions);
	el = msecsSinceEpoch() - start;
	mem = get_memory_usage() - mem;
	printf("malloc/free: %i ms  %i MO\n", (int)el, (int)(mem / (1024 * 1024)));

	
	{
		reset_memory_usage();
		mem = get_memory_usage();
		start = msecsSinceEpoch();
		parallel_object_pool<T, std::allocator<T>, 0> pp;
		pp.set_reclaim_memory(false);
		r = __test_mem_pool_random(pp, nthreads, repetitions);
		pp.clear();
		el = msecsSinceEpoch() - start;
		mem = get_memory_usage() - mem;
		printf("parallel_object_pool: %i ms  %i MO\n", (int)el, (int)(mem / (1024 * 1024)));
	}
	/*
#ifndef __MINGW32__
	reset_memory_usage();
	mem = get_memory_usage();
	start = msecsSinceEpoch();
	MimallocPool<T> mp;
	r = test_mem_pool_random(mp, nthreads, repetitions);
	el = msecsSinceEpoch() - start;
	mem = get_memory_usage() - mem;
	printf("mi_malloc/mi_free %i threads: %i ms  %i MO   %i\n", nthreads, (int)el, (int)(mem / (1024 * 1024)), r);
#endif*/
	fflush(stdout);
}










template<class PoolType>
void __test_mem_pool_random_pattern_random_size(PoolType* pool,int th_index, std::vector<size_t> * sizes)
{
	using value_type = typename PoolType::value_type;
	using pair = std::pair<value_type*, size_t>;
	std::vector<pair> vec(RAND_MAX + 1, pair(NULL,0));

	for (int i = 0; i < sizes->size(); ++i) {
		int index = rand();
		if (vec[index].first) {
			pool->deallocate(vec[index].first, vec[index].second);
			vec[index].first = NULL;
		}
		else {
			vec[index] = pair( pool->allocate((*sizes)[i]), (*sizes)[i]);
			memset(vec[index].first, 0, sizeof(value_type)* (*sizes)[i]);
		}
	}
	for (int i = 0; i < vec.size(); ++i)
		if(vec[i].first)
			pool->deallocate(vec[i].first, vec[i].second);
}

template<size_t MaxSize, class PoolType>
int __test_mem_pool_random_size(PoolType& pool, int nthreads, int count)
{
	int res = 0;
	std::vector<std::thread*> threads(nthreads);
	std::vector<size_t> sizes(count);
	for (size_t i = 0; i < count; ++i)
		sizes[i] = rand() % (MaxSize - 1) + 1;

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
	printf("test randomly mixing alloc/dealloc of random size (up to %i) in %i separate threads with the same pool\n", (int)MaxSize, nthreads);

	size_t mem, start, el;
	int r;

	reset_memory_usage();
	mem = get_memory_usage();
	start = msecsSinceEpoch();
	StdPool<T> p;
	__test_mem_pool_random_size<MaxSize,StdPool<T> >(p, nthreads, repetitions);
	el = msecsSinceEpoch() - start;
	mem = get_memory_usage() - mem;
	printf("malloc/free: %i ms  %i MO\n", (int)el, (int)(mem / (1024 * 1024)));


	{
		reset_memory_usage();
		mem = get_memory_usage();
		start = msecsSinceEpoch();
		parallel_object_pool<T, std::allocator<T>, 0,linear_object_allocation<MaxSize> > pp;
		pp.set_reclaim_memory(false);
		r = __test_mem_pool_random_size < MaxSize>(pp, nthreads, repetitions);
		pp.clear();
		el = msecsSinceEpoch() - start;
		mem = get_memory_usage() - mem;
		printf("parallel_object_pool: %i ms  %i MO\n", (int)el, (int)(mem / (1024 * 1024)));
	}

#ifndef __MINGW32__
	/*reset_memory_usage();
	mem = get_memory_usage();
	start = msecsSinceEpoch();
	MimallocPool<T> mp;
	r = test_mem_pool_random_size < MaxSize>(mp, nthreads, repetitions);
	el = msecsSinceEpoch() - start;
	mem = get_memory_usage() - mem;
	printf("mi_malloc/mi_free %i threads: %i ms  %i MO   %i\n", nthreads, (int)el, (int)(mem / (1024 * 1024)), r);*/
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
		//printf("alloc\n");
		(vec)[i] = pool->allocate(1);
		//printf("end\n");
		//memset((vec)[i], 0, sizeof(value_type));
	}
}
template<class PoolType>
void __test_mem_pool_clear_thread(PoolType* pool, bool* finish)
{
	using value_type = typename PoolType::value_type;

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
	printf("test allocating in %i threads while calling clear() every ms in another thread\n", nthreads);
	size_t mem, start, el;
	
	{
		reset_memory_usage();
		//mem = get_memory_usage();
		start = msecsSinceEpoch();
		parallel_object_pool<T, std::allocator<T>, 0> pp;
		pp.set_reclaim_memory(true);
		__test_mem_pool_interrupt_clear(pp, nthreads, count);
		pp.clear();
		el = msecsSinceEpoch() - start;
		//mem = get_memory_usage() - mem;
		printf("parallel_object_pool: %i ms  %i MO \n", (int)el, (int)(mem / (1024 * 1024)));
	}

	fflush(stdout);
}








template<class PoolType>
void __test_mem_pool_reset_thread(PoolType* pool, bool* finish)
{
	using value_type = typename PoolType::value_type;

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
		//printf("alloc\n");
		(vec)[i] = pool->allocate(1);
		//printf("end\n");
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
	printf("test allocating in %i threads while calling reset() every ms in another thread\n", nthreads);
	size_t mem, start, el;

	{
		reset_memory_usage();
		mem = get_memory_usage();
		start = msecsSinceEpoch();
		parallel_object_pool<T, std::allocator<T>, 0> pp;
		pp.set_reclaim_memory(true);
		__test_mem_pool_interrupt_reset(pp, nthreads, count);
		pp.clear();
		el = msecsSinceEpoch() - start;
		mem = get_memory_usage() - mem;
		printf("parallel_object_pool %i threads: %i ms  %i MO \n", nthreads, (int)el, (int)(mem / (1024 * 1024)));
	}

	fflush(stdout);
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
	printf("test allocate/deallocate %i unique_ptr of size %i\n",count, sizeof(T));
	size_t mem, start, el;
	{
		reset_memory_usage();
		mem = get_memory_usage();
		start = msecsSinceEpoch();
		{
			StdPool<T> pp;
			_test_unique_ptr(pp ,count);
		}
		el = msecsSinceEpoch() - start;
		mem = get_memory_usage() - mem;
		printf("malloc: %i ms  %i MO \n", (int)el, (int)(mem / (1024 * 1024))); fflush(stdout);
	}
	{
		reset_memory_usage();
		mem = get_memory_usage();
		start = msecsSinceEpoch();
		{
			object_pool<T,std::allocator<T>,0,linear_object_allocation<>,true> pp;
			_test_unique_ptr(pp, count);
		}
		el = msecsSinceEpoch() - start;
		mem = get_memory_usage() - mem;
		printf("object_pool : %i ms  %i MO \n", (int)el, (int)(mem / (1024 * 1024))); fflush(stdout);
	}
	{
		reset_memory_usage();
		mem = get_memory_usage();
		start = msecsSinceEpoch();
		{
			parallel_object_pool<T> pp;
			_test_unique_ptr(pp, count);
		}
		el = msecsSinceEpoch() - start;
		mem = get_memory_usage() - mem;
		printf("parallel_object_pool: %i ms  %i MO \n", (int)el, (int)(mem / (1024 * 1024))); fflush(stdout);
	}
}
















template<class T, size_t MaxSize>
void test_multipl_size_monthread(int count)
{
	using pair = std::pair<T*, size_t>;
	std::vector<pair> vec(count);
	std::vector<size_t> sizes(count);
	size_t start, el;

	srand(msecsSinceEpoch());
	for (int i = 0; i < count; ++i)
		sizes[i] =  rand() % (MaxSize - 1) + 1;

	start = msecsSinceEpoch();
	for (int i = 0; i < count; ++i) {
		vec[i] =pair( (T*)malloc(sizeof(T) * sizes[i]), sizes[i]);
		memset(vec[i].first, 0, sizeof(T)* sizes[i]);
	}
	for (int i = 0; i < count; ++i)
		free(vec[i].first);
	el = msecsSinceEpoch() - start;
	printf("malloc: %i ms\n", (int)el); fflush(stdout);


	{
		{
			object_pool < T, std::allocator<T>, 0, block_object_allocation< MaxSize,8> > pa;
			pa.set_reclaim_memory(false);
			start = msecsSinceEpoch();
			for (int i = 0; i < count; ++i) {
				vec[i] = pair(pa.allocate(sizes[i]), sizes[i]);
				memset(vec[i].first, 0, sizeof(T) * sizes[i]);
			}
			for (int i = 0; i < count; ++i)
				pa.deallocate(vec[i].first, vec[i].second);
			el = msecsSinceEpoch() - start;
			printf("object_pool: %i ms\n", (int)el); fflush(stdout);

			start = msecsSinceEpoch();
			for (int i = 0; i < count; ++i) {
				vec[i] = pair(pa.allocate(sizes[i]), sizes[i]);
				memset(vec[i].first, 0, sizeof(T) * sizes[i]);
			}
			for (int i = 0; i < count; ++i)
				pa.deallocate(vec[i].first, vec[i].second);
			el = msecsSinceEpoch() - start;
			printf("pool_allocator2: %i ms\n", (int)el); fflush(stdout);
		}
	}

	{
		parallel_object_pool < T, std::allocator<T>, 0, block_object_allocation< MaxSize, 8> > pa;
		pa.set_reclaim_memory(false);
		start = msecsSinceEpoch();
		for (int i = 0; i < count; ++i) {
			vec[i] = pair(pa.allocate(sizes[i]), sizes[i]);
			memset(vec[i].first, 0, sizeof(T) * sizes[i]);
		}
		for (int i = 0; i < count; ++i)
			pa.deallocate(vec[i].first, vec[i].second);
		el = msecsSinceEpoch() - start;
		printf("parallel_object_pool: %i ms\n", (int)el); fflush(stdout);

		start = msecsSinceEpoch();
		for (int i = 0; i < count; ++i) {
			vec[i] = pair(pa.allocate(sizes[i]), sizes[i]);
			memset(vec[i].first, 0, sizeof(T) * sizes[i]);
		}
		for (int i = 0; i < count; ++i)
			pa.deallocate(vec[i].first, vec[i].second);
		el = msecsSinceEpoch() - start;
		printf("parallel_pool_allocator2: %i ms\n", (int)el); fflush(stdout);
	}


	/*start = msecsSinceEpoch();
	for (int i = 0; i < count; ++i) {
		vec[i] = pair((T*)mi_malloc(sizeof(T) * sizes[i]), sizes[i]);
		memset(vec[i].first, 0, sizeof(T) * sizes[i]);
	}
	for (int i = 0; i < count; ++i)
		mi_free(vec[i].first);
	el = msecsSinceEpoch() - start;
	printf("mi_malloc: %i ms\n", (int)el); fflush(stdout);

	start = msecsSinceEpoch();
	for (int i = 0; i < count; ++i) {
		vec[i] = pair((T*)mi_malloc(sizeof(T) * sizes[i]), sizes[i]);
		memset(vec[i].first, 0, sizeof(T) * sizes[i]);
	}
	for (int i = 0; i < count; ++i)
		mi_free(vec[i].first);
	el = msecsSinceEpoch() - start;
	printf("mi_malloc2: %i ms\n", (int)el); fflush(stdout);
	printf("\n"); fflush(stdout);
	*/
}










template<class mutex>
void test_mutex_thread(mutex* m, int count)
{
	for (int i = 0; i < count; ++i)
	{
		m->lock();
		std::this_thread::yield();//Sleep(1);
		m->unlock();
	}
}

template<class mutex>
void test_mutex_nthreads(mutex& m, int nthreads, int count)
{
	std::vector<std::thread*> threads(nthreads);
	for (int i = 0; i < nthreads; ++i)
		threads[i] = new std::thread(std::bind(test_mutex_thread<mutex>, &m, count));

	for (int i = 0; i < nthreads; ++i)
		threads[i]->join();

	for (int i = 0; i < nthreads; ++i)
		delete threads[i];
}

void test_mutex(int nthreads, int count)
{
	size_t st, el;

	st = msecsSinceEpoch();
	std::mutex m1;
	test_mutex_nthreads(m1, nthreads, count);
	el = msecsSinceEpoch() - st;
	printf("std::mutex: %i\n", (int)el);

	st = msecsSinceEpoch();
	spinlock m2;
	test_mutex_nthreads(m2, nthreads, count);
	el = msecsSinceEpoch() - st;
	printf("spinlock: %i\n", (int)el);

	st = msecsSinceEpoch();
	spin_mutex<> m3;
	test_mutex_nthreads(m3, nthreads, count);
	el = msecsSinceEpoch() - st;
	printf("adaptative mutex: %i\n", (int)el);

	
}





inline void test_pow2_allocation(int count)
{
	size_t mem, mem_alloc, mem_dealloc, start, el;
	int r;
	reset_memory_usage();
	//size_t start_mem = get_memory_usage();

	size_t max_size = 1024;
	std::vector<size_t> sizes(count);
	size_t total = 0;
	for (int i = 0; i < count; ++i) {
		sizes[i] = rand() % max_size + 1;
		total += sizes[i];
	}
	printf("theoric size: %i\n", (int)(total / (1024 * 1024)));

	std::vector <char*> std_pool(count);



	reset_memory_usage();
	mem = get_memory_usage();
	start = msecsSinceEpoch();
	object_pool<char, std::allocator<char>, 0, pow_object_allocation< 1024,16, 4> > pool;
	for (int i = 0; i < count; ++i)
		std_pool[i] = pool.allocate(sizes[i]);
	el = msecsSinceEpoch() - start;
	object_pool_stats stats;
	pool.dump_statistics(stats);
	mem_alloc = get_memory_usage() - mem;
	for (int i = 0; i < count; ++i)
		pool.deallocate(std_pool[i], sizes[i]);
	mem_dealloc = get_memory_usage() - mem;
	printf("object_pool: %i ms  %i MO and %i MO\n", (int)el, (int)(stats.memory / (1024 * 1024)), (int)(mem_dealloc / (1024 * 1024)));



	reset_memory_usage();
	mem = get_memory_usage();
	start = msecsSinceEpoch();
	for (int i = 0; i < count; ++i)
		std_pool[i] = (char*)malloc(sizes[i]);
	el = msecsSinceEpoch() - start;
	mem_alloc = get_memory_usage() - mem;
	for (int i = 0; i < count; ++i)
		free(std_pool[i]);
	mem_dealloc = get_memory_usage() - mem;
	printf("malloc/free: %i ms  %i MO and %i MO\n", (int)el, (int)(mem_alloc / (1024 * 1024)), (int)(mem_dealloc / (1024 * 1024)));

#ifndef __MINGW32__
	/*reset_memory_usage();
	mem = get_memory_usage();
	start = msecsSinceEpoch();
	for (int i = 0; i < count; ++i)
		std_pool[i] = (char*)mi_malloc(sizes[i]);
	el = msecsSinceEpoch() - start;
	mem_alloc = get_memory_usage() - mem;
	for (int i = 0; i < count; ++i)
		mi_free(std_pool[i]);
	mem_dealloc = get_memory_usage() - mem;
	printf("mi_malloc/mi_free: %i ms  %i MO and %i MO\n", (int)el, (int)(mem_alloc / (1024 * 1024)), (int)(mem_dealloc / (1024 * 1024)));
	*/

#endif
}
