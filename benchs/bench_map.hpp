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

#pragma once



#include "flat_map.hpp"
#include "format.hpp"
#include "testing.hpp"

#include "phmap/btree.h"
#include "boost/container/flat_set.hpp"


#include <iostream>
#include <set>

using namespace seq;




template<class T>
inline size_t convert_to_size_t(const T& v)
{
	return static_cast<size_t>(v);
}
template<size_t S, class Al>
inline size_t convert_to_size_t(const tiny_string<S, Al>& v)
{
	return static_cast<size_t>(v.size());
}
inline size_t convert_to_size_t(const std::string& v)
{
	return static_cast<size_t>(v.size());
}

template<class C>
struct is_boost_map : std::false_type {};
template<class C>
struct is_boost_map<boost::container::flat_set<C> > : std::true_type {};

template<class C, class K>
struct find_val
{
	static bool find(const C& s,const K & key, size_t& out)
	{
		auto it = s.find(key);
		if (it != s.end()) {
			out += convert_to_size_t(*it);
			return true;
		}
		return false;
	}
};
template<class C, class K>
struct find_val<flat_set<C>,K>
{
	static bool find(const flat_set<C>& s, const K& key, size_t& out)
	{
		auto it = s.find_pos(key);
		if (it != s.size()) {
			out += convert_to_size_t(s.pos(it));
			return true;
		}
		return false;
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


template<class C>
void check_sorted(C& set)
{
	if (!std::is_sorted(set.begin(), set.end()))
		throw std::runtime_error("set class not sorted");
}

template<class T, class hash, class equal, class allocator>
void check_sorted(std::unordered_set<T,hash,equal,allocator>& )
{
}

template<class T, class hash, class equal, class allocator>
void check_sorted(seq::ordered_set<T, hash, equal, allocator>&)
{
}


template<class C, class U, class Format>
void test_set(const char * name,  const std::vector<U> & vec, Format f)
{
	using T = typename C::value_type;
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
#ifndef TEST_BOOST_INSERT_ERASE
		if (std::is_same< boost::container::flat_set<T>, C>::value) {
			insert = 1000000;
			insert_mem = 0;
		}
		else
#endif
		{
			tick();
			C s;
			for (size_t i = 0; i < vec.size() / 2; ++i)
				insert_value(s, vec[i]);
			insert = tock_ms();
			insert_mem = (get_memory_usage() - start_mem) / (1024 * 1024);

			check_sorted(s);
		}
	}
	
	reset_memory_usage();
	start_mem = get_memory_usage();

	//insert range
	tick();
	set.insert(vec.begin(), vec.begin() + vec.size() / 2);
	insert_range = tock_ms();
	insert_range_mem = (get_memory_usage() - start_mem) / (1024 * 1024);
	
	check_sorted(set);
	

	//insert fail
	tick();
	for (size_t i = 0; i < vec.size() / 2; ++i)
		insert_value(set, vec[i]);
	size_t insert_fail = tock_ms();
	size_t insert_fail_mem = (get_memory_usage() - start_mem) / (1024 * 1024);

	check_sorted(set);

	//find success
	tick();
	size_t sum = 0;
	for (size_t i = 0; i < success.size(); ++i)
		SEQ_TEST(find_val<C, U>::find(set, success[i], sum));
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
		sum += convert_to_size_t(*it);
	size_t iterate = tock_ms();
	print_null(sum);

	size_t erase = 0;
#ifndef TEST_BOOST_INSERT_ERASE
	if (std::is_same< boost::container::flat_set<T>, C>::value) {
		erase = 1000000;
	}
	else
#endif
	{
		tick();
		for (size_t i = 0; i < success.size(); ++i) {
			auto it = set.find(success[i]);
			if (it != set.end())
				set.erase(it);
		}
		/*for (auto it = set.begin(); it != set.end(); ++it)
		{
			it = set.erase(it);
			if (it == set.end())
				break;
		}*/
		erase = tock_ms();
		print_null(set.size());

		check_sorted(set);
	}
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

	test_set<flat_set<T>>("seq::flat_set", vec, f);
	test_set<phmap::btree_set<T> >("phmap::btree_set", vec, f);
	test_set<boost::container::flat_set<T> >("boost::flat_set<T>", vec,f);
	test_set<std::set<T> >("std::set", vec,f);
	test_set<std::unordered_set<T> >("std::unordered_set", vec,f);	
}



template<class Map, class Format>
void test_small_map_repeat(const char * name, int count, int repeat, Format f)
{
	using value_type = typename Map::value_type;
	std::vector< value_type> vec;
	for (int i = 0; i < count; ++i)
		vec.push_back(static_cast<value_type>(i));
	seq::random_shuffle(vec.begin(), vec.end());

	tick();
	for (int i = 0; i < repeat; ++i)
	{
		Map m ;
		for (size_t j = 0; j < vec.size()/2; ++j)
			m.insert(vec[j]);
		m.insert(vec.begin() + vec.size() / 2, vec.end());

		value_type sum = 0;
		for (size_t j = 0; j < vec.size(); ++j)
			sum += *m.find(vec[j]);

		print_null(sum);

		for (size_t j = 0; j < vec.size(); ++j)
			m.erase(vec[j]);

		print_null(m.size());
	}
	size_t el = tock_ms();
	std::cout << f(name, el) << std::endl;
}

template<class T>
void test_small_map(int count, int repeat)
{
	std::cout << std::endl;
	std::cout << "Test small sorted containers with type = " << typeid(T).name() << " and size = " << count  << std::endl;
	std::cout << std::endl;

	auto f = fmt(pos<0,2>(), 
		fmt("").l(30), "|",  //name
		fmt<size_t>().c(20), "|"); //time

	std::cout << fmt(fmt("Set name").l(30), "|", fmt("Tims (ms)").c(20), "|") << std::endl;
	std::cout << fmt(str().c(30).f('-'), "|", str().c(20).f('-'), "|") << std::endl;

	
	test_small_map_repeat<phmap::btree_set<T > >("phmap::btree_set", count, repeat, f);
	test_small_map_repeat < flat_set < T, std::less<T>, std::allocator<T> > >("seq::flat_set", count, repeat, f);
	test_small_map_repeat<boost::container::flat_set<T> >("boost::flat_set<T>", count, repeat, f);
	test_small_map_repeat<std::set<T> >("std::set", count, repeat, f);
}