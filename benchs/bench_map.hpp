#pragma once



#include "flat_map.hpp"
#include "format.hpp"
#include "testing.hpp"

#include "phmap/btree.h"
#include "boost/container/flat_set.hpp"


#include <iostream>
#include <set>

using namespace seq;




template<class C>
struct is_boost_map : std::false_type {};
template<class C>
struct is_boost_map<boost::container::flat_set<C> > : std::true_type {};

template<class C, class K>
struct find_val
{
	static void find(const C& s,const K & key, size_t& out)
	{
		auto it = s.find(key);
		if (it != s.end())
			out += (size_t)(*it);
	}
};
template<class C, class K>
struct find_val<flat_set<C>,K>
{
	static void find(const flat_set<C>& s, const K& key, size_t& out)
	{
		auto it = s.find_pos(key);
		if (it != s.size())
			out += (size_t)(s.pos(it));
	}
};


template<class C, class K>
struct insert_val
{
	static void insert( C& s, const K& key)
	{
		s.insert(key);
	}
};
template<class C, class K>
struct insert_val<flat_set<C>, K>
{
	static void insert( flat_set<C>& s, const K& key)
	{
		s.insert_pos(key);
	}
};
template<class C, class K>
void insert_value(C& s, const K& key)
{
	insert_val<C, K>::insert(s, key);
}



template<class C, class U, class Format>
void test_set(const char * name,  const std::vector<U> & vec, Format f)
{
	C set;
	
	std::vector<U> success(vec.begin(), vec.begin()+vec.size()/2);
	std::vector<U> fail(vec.begin() + vec.size() / 2, vec.end());
	seq::random_shuffle(success.begin(), success.end());
	seq::random_shuffle(fail.begin(), fail.end());

	reset_memory_usage();
	size_t start_mem = get_memory_usage();
	size_t insert_range, insert_range_mem;
	size_t insert, insert_mem;


	{
		//insert
		tick();
		C s;
		for (size_t i = 0; i < vec.size() / 2; ++i)
			insert_value(s, vec[i]);
		insert = tock_ms();
		insert_mem = (get_memory_usage() - start_mem) / (1024 * 1024);

	}
	
	reset_memory_usage();
	start_mem = get_memory_usage();

	//insert range
	tick();
	set.insert(vec.begin(), vec.begin() + vec.size() / 2);
	insert_range = tock_ms();
	insert_range_mem = (get_memory_usage() - start_mem) / (1024 * 1024);
	
	

	//insert fail
	tick();
	for (size_t i = 0; i < vec.size() / 2; ++i)
		insert_value(set, vec[i]);
	size_t insert_fail = tock_ms();
	size_t insert_fail_mem = (get_memory_usage() - start_mem) / (1024 * 1024);

	//find success
	tick();
	size_t sum = 0;
	for (size_t i = 0; i < success.size(); ++i)
		find_val<C, U>::find(set, success[i], sum);
	size_t find = tock_ms();
	print_null(sum);

	//find fail
	tick();
	sum = 0;
	for (size_t i = 0; i < fail.size() ; ++i)
		find_val<C, U>::find(set, fail[i], sum);
	size_t find_fail = tock_ms();
	print_null(sum);

	//walk
	tick();
	sum = 0;
	for (auto it = set.begin(); it != set.end(); ++it)
		sum += (size_t)(*it);
	size_t iterate = tock_ms();
	print_null(sum);

	
	tick();
	for (size_t i = 0; i < success.size(); ++i) {
		auto it = set.find(success[i]);
		if (it != set.end())
			set.erase(it);
	}
	size_t erase = tock_ms();
	print_null(set.size());
	
	std::cout << f(name, fmt(insert_range, insert_range_mem), fmt(insert, insert_mem), insert_fail, find, find_fail, iterate, erase) << std::endl;
}



/// @brief Compare various operations on seq::flat_set, phmap::btree_set, boost::container::flat_set, std::set and std::unordered_set<T>
/// @tparam T 
/// @tparam Gen 
/// @param count 
/// @param gen 
template<class T, class Gen>
void test_map(size_t count, Gen gen)
{
	std::cout << std::endl;
	std::cout << "Test sorted containers with type = " << typeid(T).name() << " and count = " << count / 2 << std::endl;
	std::cout << std::endl;

	std::vector<T> vec(count*2);
	for (int i = 0; i < vec.size(); ++i) {
		vec[i] = (T)gen(i);
	}
	seq::random_shuffle(vec.begin(), vec.end());

	auto f = fmt(pos<0, 2, 4, 6, 8, 10, 12, 14>(),
		fmt("").l(30), "|",  //name
		fmt(pos<0, 2>(), fmt<size_t>(), " ms/", fmt<size_t>(), " MO").c(20), "|", //insert(range)
		fmt(pos<0, 2>(), fmt<size_t>(), " ms/", fmt<size_t>(), " MO").c(20), "|", //insert
		fmt(pos<0>(), fmt<size_t>(), " ms").c(15), "|", //insert(fail)
		fmt(pos<0>(), fmt<size_t>(), " ms").c(15), "|", //find
		fmt(pos<0>(), fmt<size_t>(), " ms").c(15), "|", //find(fail)
		fmt(pos<0>(), fmt<size_t>(), " ms").c(15), "|",  //iterate
		fmt(pos<0>(), fmt<size_t>(), " ms").c(15), "|"); //erase
	
	std::cout << fmt(fmt("Set name").l(30), "|", fmt("Insert(range)").c(20), "|", fmt("Insert").c(20), "|", fmt("Insert(failed)").c(15), "|", fmt("Find (success)").c(15), "|",
		fmt("Find (failed)").c(15), "|", fmt("Iterate").c(15), "|", fmt("Erase").c(15), "|") << std::endl;
	std::cout << fmt(str().c(30).f('-'), "|", str().c(20).f('-'), "|", str().c(20).f('-'), "|", str().c(15).f('-'), "|", str().c(15).f('-'), "|", str().c(15).f('-'), "|", str().c(15).f('-'), "|", str().c(15).f('-'), "|") << std::endl;

	test_set<flat_set<T> >("seq::flat_set", vec,f);
	test_set<phmap::btree_set<T> >("phmap::btree_set", vec,f);
	test_set<boost::container::flat_set<T> >("boost::flat_set<T>", vec,f);
	test_set<std::set<T> >("std::set", vec,f);
	test_set<std::unordered_set<T> >("std::unordered_set", vec,f);	
}
