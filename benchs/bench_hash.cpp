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


#include <unordered_set>
#include <iostream>
#include <fstream>

#ifdef _MSC_VER
#include <concurrent_unordered_set.h>
#endif

#include "robin_hood/robin_hood.h"



#ifdef BOOST_CONCURRENT_MAP_FOUND
#include <boost/unordered/unordered_flat_set.hpp>
#endif


#include <seq/testing.hpp>
#include <seq/ordered_map.hpp>
#include <seq/radix_hash_map.hpp>
#include <seq/radix_map.hpp>
#include <seq/format.hpp>
#include <seq/any.hpp>
#include <seq/tiny_string.hpp>
#include <seq/concurrent_map.hpp>

#ifdef SEQ_HAS_CPP_17
#include "ankerl/unordered_dense.h"
#include <gtl/phmap.hpp>
#endif

using namespace seq; 



template<class C, class Iter>
void erase(C& set, Iter it)
{
	set.erase(it);
}
template<class C>
void reserve(C& set, size_t count)
{
	set.reserve(count);
}


template<class T>
inline size_t to_size_t(const T& t) {
	return static_cast<size_t>(t);
}
inline size_t to_size_t(const std::string& t) {
	return t.size();
}
template<class Char, class Traits, size_t S, class A>
inline size_t to_size_t(const tiny_string<Char,Traits,A,S>& t) {
	return t.size();
}
template<class Interface, size_t S, size_t A, bool R>
inline size_t to_size_t(const seq::hold_any<Interface,S,A,R>& t) {
	return reinterpret_cast<size_t>(t.data());
}

template<class Iter>
size_t count_iter(Iter start, Iter end)
{
	size_t count = 0;
	while (start != end) {
		++count;
		++start;
	}
	return count;
}


template<class C>
size_t walk_set(C& set)
{
	size_t sum = 0;
	auto end = set.end();
	for (auto it = set.begin(); it != end; ++it) 
		sum += to_size_t(*it);
	return sum;
}

template<
	class Key,
	class Hash ,
	class Equal ,
	class Allocator,
	unsigned Shards
>
size_t walk_set(concurrent_set<Key, Hash, Equal, Allocator, Shards>& set)
{
	size_t sum = 0;
	set.cvisit_all([&](const auto& v) {sum += to_size_t(v); });
	return sum;
}


inline bool test_insert(bool v) { return v; }
template<class T>
inline bool test_insert(const std::pair<T,bool> & v) { return v.second; }

template<class C>
struct is_radix_set : std::false_type {};

template<class T, class Extract, class Al>
struct is_radix_set<seq::radix_hash_set<T, Extract, Al> > : std::true_type {};


template<class C, class T, bool Launch = !(is_hold_any<T>::value&& is_radix_set<C>::value)>
struct LaunchTest
{
	template<class Format>
	static void test(const char* name, C& set, const std::vector<T>& keys, Format f, bool write = true)
	{
		std::vector<T> success, failed;
		{
			success = std::vector<T>(keys.begin(), keys.begin() + keys.size() / 2);
			failed = std::vector<T>(keys.begin() + keys.size() / 2, keys.end());
		}

		//seq::random_shuffle(success.begin(), success.end(),1);
		//seq::random_shuffle(failed.begin(), failed.end(),1);

		reset_memory_usage();
		size_t start_mem = get_memory_usage();
		size_t sum = 0;


		// insert with reserve
		size_t insert_reserve = 0;
		size_t insert_reserve_mem = 0;

		{
			reset_memory_usage();
			start_mem = get_memory_usage();
			C s;
			tick();
			//s.reserve(success.size());
			reserve(s, success.size());
			for (int i = 0; i < success.size(); ++i) {
				
				s.insert(success[i]);
			}
			insert_reserve = tock_ms();
			insert_reserve_mem = ((get_memory_usage() - start_mem) / (1024 * 1024));
		}

		reset_memory_usage();
		start_mem = get_memory_usage();

		// insert no reserve
		tick();
		for (size_t i = 0; i < success.size(); ++i)
		{
			set.insert(success[i]);

		}
		size_t insert = tock_ms();
		size_t insert_mem = ((get_memory_usage() - start_mem) / (1024 * 1024));


		//std::cout << "A" << std::endl;
		//std::cout << std::endl << set.size() << std::endl;


		size_t prev_size = set.size();

		reset_memory_usage();
		start_mem = get_memory_usage();

		// insert failed
		tick();
		for (size_t i = 0; i < success.size(); ++i) {

			if (test_insert(set.insert(success[i])))
				SEQ_TEST(false);
		}
		size_t insert_fail = tock_ms();
		size_t insert_fail_mem = ((get_memory_usage() - start_mem) / (1024 * 1024));

		SEQ_TEST(prev_size == set.size());

		//std::cout << "sorted: " << std::is_sorted(set.begin(), set.end()) << std::endl;

		seq::random_shuffle(success.begin(), success.end(), 1);

		// successfull lookups
		tick();
		for (size_t i = 0; i < success.size(); ++i) {

			/*auto it = set.find(success[i]);
			if (it != set.end())
				sum += to_size_t(*it);
			else
				SEQ_TEST(false);*/
			if (set.count(success[i]))
				++sum;
			else {
				//set.count(success[i]);
				SEQ_TEST(false);
			}

		}
		size_t find = tock_ms(); print_null(sum);

		//std::cout << "B" << std::endl;

		// failed lookups
		tick();
		sum = 0;
		for (size_t i = 0; i < failed.size(); ++i) {
			/*auto it = set.find(failed[i]);
			if (it != set.end()) {
				SEQ_TEST(false);
				sum += to_size_t(*it);
			}*/
			if (set.count(failed[i])) {
				SEQ_TEST(false);
			}
			else
				++sum;
		}
		size_t failed_ = tock_ms(); print_null(sum);

		//std::cout << "C" << std::endl;

		// walk through using iterators
		tick();
		sum = walk_set(set);
		
		size_t walk = tock_ms(); print_null(sum);

		//std::cout << "D" << std::endl;

		// remove half keys

		//std::vector<T> to_erase(success);
		//seq::random_shuffle(to_erase.begin(), to_erase.end(), 1);

		size_t eraset = 0;
		size_t erase_mem = 0, find_again = 0;
		tick();
		int i = 0;
		size_t target = set.size() / 2;
		while (set.size() > target) {
			if (set.erase(success[i]) != 1)
				SEQ_TEST(false);
			i++;
		}
		eraset = tock_ms();
		erase_mem = (get_memory_usage() - start_mem) / (1024 * 1024);


		//sum = 0;
		//for (auto it = set.begin(); it != set.end(); ++it)
		//	sum += to_size_t(*it);

		// mixed failed/successfull lookups
		tick();
		sum = 0;
		for (int i = 0; i < success.size(); ++i) {

			sum += set.count(success[i]);
		}
		find_again = tock_ms(); print_null(sum);
		//std::cout << sum << std::endl;
		
		
		if (write)
			std::cout << f(name, fmt(insert, insert_mem), fmt(insert_fail, insert_fail_mem), fmt(insert_reserve, insert_reserve_mem), find, failed_, walk, fmt(eraset, erase_mem), find_again) << std::endl;
	}
};




template<class C, class T, class Format>
void test_hash_set(const char* name, C& set, const std::vector<T>& keys, Format f, bool write = true)
{
	LaunchTest<C, T>::test(name, set, keys, f, write);
}

/// @brief Compare various operations on seq::ordered_set, phmap::node_hash_set, robin_hood::unordered_node_set, ska::unordered_set and std::unordered_set
template<class T,class Hash, class Gen>
void test_hash(int count, Gen gen,bool save_keys=false)
{
	std::cout << std::endl;
	std::cout << "Test hash table implementations with type = " << typeid(T).name() << " and count = " << count/2  << std::endl;
	std::cout << std::endl;
	
	
	auto f = join("|",
		_s().l(30),  //name
		_fmt(_u(), " ms/", _u(), " MO").c(20),  //insert(range)
		_fmt(_u(), " ms/", _u(), " MO").c(20),  //insert
		_fmt(_u(), " ms/", _u(), " MO").c(20),  //insert
		_fmt(_u(), " ms").c(15),  //find
		_fmt(_u(), " ms").c(15), //find(fail)
		_fmt(_u(), " ms").c(15), //iterate
		_fmt(_u(), " ms/", _u(), " MO").c(20),  //erase
		_fmt(_u(), " ms").c(15),   //find again
		 "");
	
	auto header = join("|", _s().l(30), _s().c(20), _s().c(20), _s().c(20), _s().c(15), _s().c(15), _s().c(15), _s().c(20), _s().c(15), "");
	std::cout << header("Hash table name", "Insert", "Insert(failed)", "Insert(reserve)", "Find(success)", "Find(failed)", "Iterate", "Erase", "Find again") << std::endl;
	std::cout << header(fill('-'), fill('-'), fill('-'), fill('-'), fill('-'), fill('-'), fill('-'), fill('-'), fill('-')) << std::endl;
	
	// Generate inputs
	std::vector<T> keys(count);
	for (size_t i = 0; i < keys.size(); ++i)
		keys[i] = (T)gen(i);
	{
		std::sort(keys.begin(), keys.end());
		size_t size = std::unique(keys.begin(), keys.end()) - keys.begin();
		keys.resize(size);
		seq::random_shuffle(keys.begin(), keys.end(), 1);
	}
	
	
	// warmup
	 {
		ordered_set<T, Hash, seq::equal_to<>, std::allocator<T>> set;
		test_hash_set("seq::ordered_set", set, keys, f, false);
	}

	
	{
		ordered_set<T, Hash, seq::equal_to<> > set;
		test_hash_set("seq::ordered_set", set, keys, f);
	}
#ifdef SEQ_HAS_CPP_17
	{
		ankerl::unordered_dense::set<T, Hash, seq::equal_to<> > set;
		test_hash_set("ankerl::unordered_dense::set", set, keys, f);
	}
#endif
	 {
		concurrent_set<T, Hash, seq::equal_to<>, std::allocator<T>, seq::no_concurrency > set;
		test_hash_set("seq::concurrent_set", set, keys, f);
	}
	
	
	
	 {
		 radix_hash_set<T, Hash, seq::equal_to<> > set;
		 //radix_set<T> set;
		  test_hash_set("seq::radix_hash_set", set, keys, f);
	 } 
	 {
		 robin_hood::unordered_flat_set<T, Hash, seq::equal_to<> > set;
		 test_hash_set("robin_hood::unordered_flat_set", set, keys, f);
	 }
#ifdef SEQ_HAS_CPP_17
	  {
		 gtl::flat_hash_set<T, Hash, seq::equal_to<> > set;
		 test_hash_set("gtl::flat_hash_set", set, keys, f);
	 }
#endif
#ifdef BOOST_CONCURRENT_MAP_FOUND
	  {
		 boost::unordered_flat_set<T, Hash, seq::equal_to<> > set;
		  test_hash_set("boost::unordered_flat_set", set, keys, f);
	  }
#endif
	 
	  {
		std::unordered_set <T, Hash, seq::equal_to<> > set;
		 test_hash_set("std::unordered_set", set, keys, f);
	}
}




int bench_hash(int, char** const)
{
	seq::concurrent_set<size_t, seq::hasher<size_t>, std::equal_to<>, std::allocator<size_t>, seq::no_concurrency> ss;
	ss.emplace(2);
	
	test_hash<int, seq::hasher<int> >(8000000, [](size_t i) { return (i); });
	test_hash<size_t, seq::hasher<size_t> >(8000000, [](size_t i) { return (i); });

	random_float_genertor<double> rng;
	test_hash<double, seq::hasher<double> >(8000000, [&rng](size_t i) { return rng(); });
	
	test_hash<tstring, seq::hasher<tstring > >(2500000, [](size_t i) { return generate_random_string<tstring >(63, false); });

	test_hash<tstring, seq::hasher<tstring> >(4000000, [](size_t i) { return generate_random_string<tstring>(1, true) + (tstring(12, ' ') + generate_random_string<tstring>(13, true)); });

	test_hash<tstring, seq::hasher<tstring> >(4000000, [](size_t i) { return generate_random_string<tstring>(13, true); });
	

	test_hash<seq::r_any, seq::hasher<seq::r_any> >(2500000, [](size_t i)
		{
			size_t idx = i & 3U;
			switch (idx) {
			case 0:return seq::r_any(i * UINT64_C(0xc4ceb9fe1a85ec53));
			case 1:return seq::r_any((double)(i * UINT64_C(0xc4ceb9fe1a85ec53)));
			default:return seq::r_any(generate_random_string<tstring>(63, false));
			}
		}
	);
	
	return 0;
}