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


#include <unordered_set>
#include <iostream>

#include "robin_hood/robin_hood.h"
#include "ska/flat_hash_map.hpp"
#include "ska/unordered_map.hpp"
#include "boost/unordered_set.hpp"
#include "phmap/phmap.h"

#include "testing.hpp"
#include "ordered_map.hpp"
#include "format.hpp"

using namespace seq;




template<class C, class T, class Format>
void test_hash_set(const char* name, C& set, const std::vector<T>& keys, Format f)
{
	static constexpr size_t width = 12;

	std::vector<T> success(keys.begin(), keys.begin() + keys.size() / 2);
	seq::random_shuffle(success.begin(), success.end());
	std::vector<T> failed(keys.begin() + keys.size() / 2, keys.end());
	seq::random_shuffle(failed.begin(), failed.end());

	reset_memory_usage();
	size_t start_mem = get_memory_usage();
	size_t sum = 0;


	// insert with reserve
	size_t insert_reserve;
	size_t insert_reserve_mem;
	{
		reset_memory_usage();
		start_mem = get_memory_usage();
		C s;
		tick();
		s.reserve(keys.size() / 2);
		for (int i = 0; i < keys.size() / 2; ++i) {
			s.insert(keys[i]);
		}
		insert_reserve = tock_ms();
		insert_reserve_mem = ((get_memory_usage() - start_mem) / (1024 * 1024));
	}

	reset_memory_usage();
	start_mem = get_memory_usage();

	// insert no reserve
	tick();
	for (int i = 0; i < keys.size() / 2; ++i) {
		set.insert(keys[i]);
	}
	size_t insert = tock_ms();
	size_t insert_mem = ((get_memory_usage() - start_mem) / (1024 * 1024));


	// successfull lookups
	tick();
	for (int i = 0; i < success.size(); ++i) {
		
		auto it = set.find(success[i]);
		if (it != set.end())
			sum += (int)(*it);
			
	}
	size_t find = tock_ms(); print_null(sum);

	// failed lookups
	tick();
	sum = 0;
	for (int i = 0; i < failed.size(); ++i) {
		auto it = set.find(failed[i]);
		if (it != set.end())
			sum+= (size_t)(*it);

	}
	size_t failed_ = tock_ms(); print_null(sum);

	// walk through using iterators
	tick();
	sum = 0;
	auto end = set.end();
	for (auto it = set.begin(); it != end; ++it) {
		sum += (size_t)(*it);
	}
	size_t walk = tock_ms(); print_null(sum);
		
	// remove half keys
	tick();
	int i = 0;
	size_t target = set.size() / 2;
	while (set.size() > target) {
		auto it = set.find(success[i++]);
		if (it != set.end())
			set.erase(it);
	}
	size_t erase = tock_ms();
	size_t erase_mem = (get_memory_usage() - start_mem) / (1024 * 1024);

	// mixed failed/successfull lookups
	tick();
	sum = 0;
	for (int i = 0; i < success.size(); ++i) {
		auto it = set.find(keys[i]);
		if (it != set.end())
			sum += (size_t)(*it);
	}
	size_t find_again = tock_ms(); print_null(sum);
	
	std::cout << f(name, fmt(insert, insert_mem), fmt(insert_reserve, insert_reserve_mem), find, failed_, walk, fmt(erase, erase_mem), find_again) << std::endl;
}


/// @brief Compare various operations on seq::ordered_set, phmap::node_hash_set, robin_hood::unordered_node_set, ska::unordered_set and std::unordered_set
template<class T,class Hash, class Gen>
void test_hash(int count, Gen gen)
{
	std::cout << std::endl;
	std::cout << "Test hash table implementations with type = " << typeid(T).name() << " and count = " << count / 2 << std::endl;
	std::cout << std::endl;

	std::cout << fmt(fmt("Hash table name").l(30), "|", fmt("Insert").c(20), "|", fmt("Insert(reserve)").c(20), "|", fmt("Find (success)").c(15), "|",
		fmt("Find (failed)").c(15), "|", fmt("Iterate").c(15), "|", fmt("Erase").c(20), "|", fmt("Find again").c(15), "|") << std::endl;
	std::cout << fmt(str().c(30).f('-'), "|", str().c(20).f('-'), "|", str().c(20).f('-'), "|", str().c(15).f('-'), "|", str().c(15).f('-'), "|", str().c(15).f('-'), "|", str().c(20).f('-'), "|", str().c(15).f('-'), "|") << std::endl;


	// Create formatter object for results
	auto f = fmt(pos<0, 2, 4, 6, 8, 10, 12, 14>(),
		fmt("").l(30), "|",  //name
		fmt(pos<0, 2>(), fmt<size_t>(), " ms/", fmt<size_t>(), " MO").c(20), "|", //insert
		fmt(pos<0, 2>(), fmt<size_t>(), " ms/", fmt<size_t>(), " MO").c(20), "|", //insert(reserve)
		fmt(pos<0>(), fmt<size_t>(), " ms").c(15), "|", //find
		fmt(pos<0>(), fmt<size_t>(), " ms").c(15), "|", //find(fail)
		fmt(pos<0>(), fmt<size_t>(), " ms").c(15), "|", //iterate
		fmt(pos<0, 2>(), fmt<size_t>(), " ms/", fmt<size_t>(), " MO").c(20), "|",  //erase
		fmt(pos<0>(), fmt<size_t>(), " ms").c(15), "|"); //find again

	// Generate inputs
	std::vector<T> keys(count);
	for (size_t i = 0; i < keys.size(); ++i)
		keys[i] = (T)gen(i);
	
	// Test different tables
	
	{
#ifdef SEQ_NO_COMPRESSED_PTR
		std::cout << "SEQ_NO_COMPRESSED_PTR" << std::endl;
#endif
		ordered_set<T, Hash, std::equal_to<T>, std::allocator<T>> set;
		test_hash_set("seq::ordered_set", set, keys,f);
	}
	{
		phmap::node_hash_set<T, Hash, std::equal_to<T>> set;
		test_hash_set("phmap::node_hash_set", set, keys,f);
	}
	{
		robin_hood::unordered_node_set<T, Hash, std::equal_to<T>> set;
		test_hash_set("robin_hood::unordered_node_set", set, keys, f);
	}
	{
		ska::unordered_set<T, Hash, std::equal_to<T>> set;
		test_hash_set("ska::unordered_set", set, keys, f);
	}
	{
		boost::unordered_set<T, Hash, std::equal_to<T>> set;
		test_hash_set("boost::unordered_set", set, keys, f);
	}
	{
		std::unordered_set<T, Hash> set;
		//set.reserve(keys.size() / 2);
		test_hash_set("std::unordered_set", set, keys, f);
	}
}

