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


#include "seq/concurrent_map.hpp"
#include <libcuckoo/cuckoohash_map.hh>
#include <iostream>
#include <thread>
#include <vector>
#include <functional>
#include <unordered_map>
#include <thread>


#include <seq/testing.hpp> 
#include <seq/ordered_map.hpp>
#include <seq/ordered_map.hpp>
#include <seq/radix_hash_map.hpp>
#include <seq/format.hpp>
#include <seq/any.hpp>
#include <seq/tiny_string.hpp>

#ifdef BOOST_CONCURRENT_MAP_FOUND
#include <boost/unordered/concurrent_flat_map.hpp>
#endif

#ifdef SEQ_HAS_CPP_17
#include <gtl/phmap.hpp>
#endif

#ifdef NDEBUG
#ifdef TBB_FOUND
#include <tbb/concurrent_hash_map.h>
#include <tbb/concurrent_unordered_map.h>
#endif
#endif

#ifdef _MSC_VER
#include <concurrent_unordered_map.h>
#endif

#include <seq/radix_hash_map.hpp>


using namespace seq;


inline bool get_res(bool r) { return r; }
template<class A>
inline bool get_res(const std::pair<A, bool>& p) { return p.second; }

#ifdef SEQ_HAS_CPP_17
template<class K, class V, class H, class E, class A, size_t T, class M>
SEQ_ALWAYS_INLINE bool insert(gtl::parallel_flat_hash_map<K, V, H, E, A, T,M>& s, const K& k)
{
	return s.emplace(k, 0).second;
}
#endif

#ifdef BOOST_CONCURRENT_MAP_FOUND
template<class K, class V, class H, class P, class A>
SEQ_ALWAYS_INLINE bool insert(boost::concurrent_flat_map<K, V, H,P,A>& s, const K& k)
{
	return s.emplace(k, 0);
}
#endif
template<class Key, class V, class H, class E, class A, size_t S, class K>
SEQ_ALWAYS_INLINE bool insert(libcuckoo::cuckoohash_map<Key, V,H,E,A,S>& s, const K& k)
{
	return s.insert(k, 0);
}
#ifdef _MSC_VER
template<class K, class V, class H>
SEQ_ALWAYS_INLINE bool insert(Concurrency::concurrent_unordered_map<K, V,H>& s, const K& k)
{
	return s.insert(std::pair<K,int>(k, 0)).second;
}
#endif

template<class S, class K>
SEQ_ALWAYS_INLINE bool insert(S& s, const K& k)
{
	return get_res(s.emplace(k, 0));

}


template<class K, class Set>
void concurrent_insert(const std::vector<K>& keys, Set& s, size_t start, size_t end, std::atomic<bool> * start_compute)
{
	while (!(*start_compute)) {}

	for (size_t i = start; i < end; ++i)
	{
		 insert(s, keys[i]);
	}

}


template<class Set>
size_t walk_map(const Set& s)
{
	size_t count = 0;
	for (auto it = s.begin(); it != s.end(); ++it, ++count)
	{
	}
	return count;
}
template<class K, class V, class H, class E, class A, size_t S>
size_t walk_map(const libcuckoo::cuckoohash_map<K, V, H,E,A,S>& s)
{
	return s.size();
}


template<class K, class V, class H, class E, class A, unsigned T>
size_t walk_map(const concurrent_map<K, V, H, E, A, T>& s)
{
	size_t size = 0;
	s.cvisit_all([&size](const auto&) {++size; });
	return size;
}

#ifdef SEQ_HAS_CPP_17
template<class K, class V, class H, class E, class A, size_t T, class M>
size_t walk_map(const gtl::parallel_flat_hash_map<K, V, H, E, A, T, M>& s)
{
	size_t size = 0;
	s.for_each([&size](const auto&) {++size; });
	return size;
}
#endif

#ifdef BOOST_CONCURRENT_MAP_FOUND
template<class K, class V, class H, class P, class A>
size_t walk_map(const boost::concurrent_flat_map<K, V, H, P, A>& s)
{
	size_t size = 0;
	s.cvisit_all([&size](const auto&) {++size; });
	return size;
}
#endif

template<class Set>
void concurrent_walk(Set& s, std::atomic<bool>* start_compute, size_t * count)
{
	while (!(*start_compute)) {}

	while (*start_compute) {
		*count = walk_map(s);
	}
	size_t size = s.size();
	*count = walk_map(s);
	SEQ_TEST(*count == size);
}


template<class S, class K>
SEQ_ALWAYS_INLINE bool find_map(const S& s, const K& key)
{
	return s.contains(key);
}
#ifdef NDEBUG
#ifdef TBB_FOUND
template<class K, class V, class H>
SEQ_ALWAYS_INLINE bool find_map(const tbb::concurrent_hash_map<K, V, H>& s, const K& key)
{
	//tbb::concurrent_hash_map<K, V, H>::accessor a;
	//return s.find(a,key);
	return s.count(key) == 1;
}
template<class K, class V, class H>
SEQ_ALWAYS_INLINE bool find_map(const tbb::concurrent_unordered_map <K, V, H>& s, const K& key)
{
	//tbb::concurrent_hash_map<K, V, H>::accessor a;
	//return s.find(a,key);
	return s.count(key) == 1;
}
#endif
#endif
template<class K, class V, class H, class E, class A, size_t S>
SEQ_ALWAYS_INLINE bool find_map(const libcuckoo::cuckoohash_map<K, V, H,E,A,S>& s, const K& key)
{
	return s.contains(key);
}

#ifdef BOOST_CONCURRENT_MAP_FOUND
template<class K, class V, class H, class P, class A>
SEQ_ALWAYS_INLINE bool find_map(const boost::concurrent_flat_map<K, V, H, P, A>& s, const K& key)
{
	return s.contains(key);
}
#endif



template<class Map>
void reserve(Map& m, size_t size)
{
	m.reserve(size);
}
#ifdef NDEBUG
#ifdef TBB_FOUND
template<class K, class V, class H>
void reserve( tbb::concurrent_hash_map<K, V, H>& s, size_t size)
{
	s.rehash(size);
}
template<class K, class V, class H>
void reserve( tbb::concurrent_unordered_map <K, V, H>& s, size_t size)
{
	s.reserve(size);
}
#endif
#endif

template<class K, class Set>
void concurrent_find(const std::vector<K>& keys, Set& s, std::atomic<bool>* start_compute, size_t* count)
{
	while (!(*start_compute)) {}

	while (*start_compute) {
		*count = 0;
		for (size_t i = 0; i < keys.size(); ++i)
			if (find_map(s,keys[i]))
				++(*count);

	}
	*count = 0;
	for (size_t i = 0; i < keys.size(); ++i) {
		if (find_map(s, keys[i]))
			++(*count);
		else {
			//TEST
			//bool stop = true;
			//find_map(s, keys[i]);
		}
	}
}

template<class K, class Set>
void concurrent_find_once(const std::vector<K>& keys, Set& s, std::atomic<bool>* start_compute, size_t* count)
{
	while (!(*start_compute)) {}

	
	*count = 0;
	for (size_t i = 0; i < keys.size(); ++i)
		if (find_map(s, keys[i]))
			++(*count);
	
}



template<class S, class K>
void erase(S& s, const K& k)
{
	s.erase(k);
}

#ifdef NDEBUG
#ifdef TBB_FOUND
template<class K, class V>
void erase(tbb::concurrent_hash_map<K, V>& s, const K& k)
{
	s.erase(k);
}
template<class K, class V>
void erase(tbb::concurrent_unordered_map <K, V>& s, const K& k)
{
	s.unsafe_erase(k);
}
#endif
#endif

#ifdef _MSC_VER
template<class K, class V, class H>
void erase(Concurrency::concurrent_unordered_map<K, V, H>& s, const K& k)
{
	s.unsafe_erase(k);
}
#endif


template<class K, class Set>
void concurrent_erase(const std::vector<K>& keys, Set& s, size_t start, size_t end, std::atomic<bool>* start_compute)
{
	while (!(*start_compute)) {}

	for (size_t i = start; i < end; ++i)
	//for (size_t i = 0; i < keys.size(); ++i)
		erase( s,keys[i]);

}

template<class K, class Set>
void test_concurrent_map(const std::vector<K>& _keys, Set& s, size_t numthreads , size_t & el_insert, size_t & el_walk, size_t & el_find, size_t & el_find_fail, size_t &el_erase, bool test_walk, bool test_find)
{
	el_erase = el_find_fail = el_find = el_insert = el_walk = 0;

	std::vector<K> keys(_keys.begin(), _keys.begin() + _keys.size() / 2);
	std::vector<K> keys_find = keys;
	seq::random_shuffle(keys_find.begin(), keys_find.end(),1);
	std::vector<K> keys_not_found(_keys.begin() + _keys.size() / 2, _keys.end());
	seq::random_shuffle(keys_not_found.begin(), keys_not_found.end(), 1);

	size_t chunk_size = keys.size() / numthreads;

	std::atomic<bool> start_compute{ false };
	std::vector<std::thread*> threads(numthreads);
	for (size_t i = 0; i < numthreads; ++i) {
		size_t start = i * chunk_size;
		size_t end = i == numthreads-1 ? keys.size() : start + chunk_size;
		threads[i] = new std::thread(std::bind(concurrent_insert<K,Set>, std::cref(keys), std::ref(s), start, end, &start_compute));
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(10));

	//launch walk
	size_t walked = 0;
	std::thread* walk;
	if(test_walk)
		walk = new std::thread(std::bind(concurrent_walk<Set>, std::ref(s), &start_compute, &walked));

	//launch find
	size_t found = 0;
	std::thread* find;
	if(test_find)
		find = new std::thread(std::bind(concurrent_find<K, Set>, std::cref(keys_find), std::ref(s), &start_compute, &found));


	seq::tick();
	start_compute = true;

	for (size_t i = 0; i < numthreads; ++i) {
		threads[i]->join();
		delete threads[i];
	}
	size_t ss = s.size();
	if (ss != keys.size()) {
		printf("insert error: %i\n",(int)ss);
	}
	SEQ_TEST(keys.size() == ss);

	start_compute = false;
	if (test_walk) {
		walk->join();
		delete walk;
	}
	if (test_find) {
		find->join();
		delete find;
	}
	el_insert = seq::tock_ms();

	if (test_walk) SEQ_TEST(walked == s.size());
	if (test_find) SEQ_TEST(found == s.size());


	seq::tick();
	size_t count =  walk_map(s);
	el_walk = seq::tock_ms();
	if (count != s.size()) {
		printf("walk error, %i missing\n",(int)(s.size() - count));
	}
	SEQ_TEST(count == s.size());
	

	// parallel find
	start_compute = false;
	std::vector<size_t> counts(numthreads, 0);
	for (size_t i = 0; i < numthreads; ++i) {
		auto k = keys_find;
		seq::random_shuffle(k.begin(), k.end(),(unsigned) i);
		threads[i] = new std::thread(std::bind(concurrent_find_once<K, Set>, k, std::ref(s),  &start_compute,&counts[i]));
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(10));

	seq::tick();
	start_compute = true;

	for (size_t i = 0; i < numthreads; ++i) {
		threads[i]->join();
		delete threads[i];
		SEQ_TEST(s.size() == counts[i]);
	}
	el_find = seq::tock_ms();
	


	// parallel find failed
	start_compute = false;
	for (size_t i = 0; i < numthreads; ++i) {
		threads[i] = new std::thread(std::bind(concurrent_find_once<K, Set>, keys_not_found, std::ref(s), &start_compute, &counts[i]));
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(10));

	seq::tick();
	start_compute = true;

	for (size_t i = 0; i < numthreads; ++i) {
		threads[i]->join();
		delete threads[i];
		SEQ_TEST(0 == counts[i]);
	}
	el_find_fail = seq::tock_ms();

	


	//parallel erase

	
	for (size_t i = 0; i < numthreads; ++i) {
		size_t start = i * chunk_size;
		size_t end = i == numthreads - 1 ? keys.size() : start + chunk_size;
		threads[i] = new std::thread(std::bind(concurrent_erase<K, Set>, std::cref(keys), std::ref(s), start, end, &start_compute));
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	
	//launch walk
	walked = 0;
	if (test_walk)
		walk = new std::thread(std::bind(concurrent_walk<Set>, std::ref(s), &start_compute, &walked));

	//launch find
	found = 0;
	if (test_find)
		find = new std::thread(std::bind(concurrent_find<K, Set>, std::cref(keys_find), std::ref(s), &start_compute, &found));
	
	std::this_thread::sleep_for(std::chrono::milliseconds(10));

	seq::tick();
	start_compute = true;

	for (size_t i = 0; i < numthreads; ++i) {
		threads[i]->join();
		delete threads[i];
	}
	start_compute = false;
	if (test_walk) {
		walk->join();
		delete walk;
	}
	if (test_find) {
		find->join();
		delete find;
	}
	
	el_erase = seq::tock_ms();

	count = s.size();
	SEQ_TEST(0 == count);
	
}

template<class K, class Map, class Rng>
void test_concurrent_map(size_t count, const char * name, Rng rng)
{
	std::vector<K> keys;
	for (size_t i = 0; i < count; ++i)
		keys.push_back(rng(i));
	std::sort(keys.begin(), keys.end());
	auto it = std::unique(keys.begin(), keys.end());
	keys.erase(it, keys.end());
	seq::random_shuffle(keys.begin(), keys.end());

	//std::reverse(keys)
	//seq::random_shuffle(keys.begin(), keys.end(),1);

	count = keys.size();

	std::cout << std::endl;
	std::cout << "Test concurrent insert type = " << typeid(K).name() << " and count = " << count<< std::endl;
	std::cout << std::endl;


	auto f = join("|",
		_s().l(30),  //name
		_u().c(10), //threads
		_fmt(_u(), " ms").c(20),  //insert
		_fmt(_u(), " ms").c(20),  //walk
		_fmt(_u(), " ms").c(20),  //find
		_fmt(_u(), " ms").c(20),  //find fail
		_fmt(_u(), " ms").c(20),  //erase
		"");

	auto header = join("|", _s().l(30), _s().c(10),  _s().c(20), _s().c(20), _s().c(20), _s().c(20), _s().c(20), "");
	std::cout << header("Hash table name","Threads", "Insert", "Walk", "Find","Find fail", "Erase") << std::endl;
	std::cout << header(fill('-'), fill('-'), fill('-'), fill('-'), fill('-'), fill('-'), fill('-')) << std::endl;

	unsigned max_loop = 1;
	for (unsigned seed = 0; seed < max_loop; ++seed) {
		//if(seed)
			seq::random_shuffle(keys.begin(), keys.end(), seed);
		//if (seed < 4)
		//	continue;
		for (size_t threads = 1; threads < 20; ++threads)
		{
			
			Map set;
			//reserve(set,keys.size());
			size_t el_insert, el_walk, el_find, el_find_fail, el_erase;
			test_concurrent_map(keys, set, threads, el_insert, el_walk, el_find, el_find_fail, el_erase, false, false);

			std::cout << f(name, threads, el_insert, el_walk, el_find, el_find_fail, el_erase) << std::endl;
			
		} 
	}
	
}


template <typename Key>
class my_tbb_hash_compare {
public:
	std::size_t hash(const Key& a) const { return seq::hasher<Key>{}(a); }
	bool equal(const Key& a, const Key& b) const { return a == b; }
};

template<class K, class Gen>
void test_concurrent_hash_maps(size_t count, const Gen & gen)
{ 
	test_concurrent_map<K, concurrent_map < K, size_t, seq::hasher<K>, std::equal_to<K>, std::allocator<std::pair<K, size_t> > > >(count, "seq::concurrent_map", gen);
#ifdef BOOST_CONCURRENT_MAP_FOUND
	test_concurrent_map<K, boost::concurrent_flat_map<K, size_t, seq::hasher<K>, std::equal_to<K> > >(count, "boost::concurrent_flat_map", gen);
#endif
	
	test_concurrent_map<K, libcuckoo::cuckoohash_map<K, size_t, seq::hasher<K>, std::equal_to<K>> >(count, "libcuckoo::cuckoohash_map", gen);

#ifdef TBB_FOUND
	test_concurrent_map<K, tbb::concurrent_hash_map<K, size_t, my_tbb_hash_compare<K> >  >(count, "tbb::concurrent_hash_map", gen); //safe erase

	//test_concurrent_map<K, tbb::concurrent_unordered_map <K, size_t>  >(count, "tbb::concurrent_unordered_map ", gen); //safe iteration
#endif

#ifdef SEQ_HAS_CPP_17
	test_concurrent_map < K, gtl::parallel_flat_hash_map<K, size_t, seq::hasher<K>, std::equal_to<K>, std::allocator<std::pair<K, size_t>>, 5, std::shared_mutex > >(count, "gtl::parallel_flat_hash_map", gen);
#endif
}


int bench_concurrent_hash(int, char** const)
{
	 {
		size_t count = 20000000;
		using K = size_t;
		auto gen = [](size_t i) {return i; };
		test_concurrent_hash_maps<K>(count, gen);
	}
	
	 {
		size_t count = 10000000;
		using K = tstring;
		std::vector<tstring> strs;
		for (size_t i = 0; i < count; ++i)
			strs.push_back(generate_random_string<tstring>(33, false));
		auto gen = [&](size_t i) {return strs[i]; };
		test_concurrent_hash_maps<K>(count, gen);
	}
	

	return 0;
}