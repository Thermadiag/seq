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


#include "seq/testing.hpp"
#include "seq/concurrent_map.hpp"
#include "seq/tiny_string.hpp"

#include <fstream>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <iostream>
#include <algorithm>
#include <random>





template<class HMap1, class HMap2>
bool test_set_equals(const HMap1& h1, const HMap2& h2)
{
	if (h1.size() != h2.size())
		return false;


	for (auto it = h2.begin(); it != h2.end(); ++it) {
		if (!h1.visit(*it, [](const auto&) {}))
			return false;
	}
	return true;
}
template<class HMap1, class HMap2>
bool test_map_equals(const HMap1& h1, const HMap2& h2)
{
	if (h1.size() != h2.size())
		return false;

	
	for (auto it = h2.begin(); it != h2.end(); ++it) {
		bool eq = true;
		if (!h1.visit(it->first, [&](const auto& v) {eq = v.second == it->second ;}))
			return false;
		if (!eq)
			return false;
		
	}
	return true;
}


template<class T, unsigned Shards, class Alloc = std::allocator<T> >
inline void test_concurrent_set_logic(const Alloc & al )
{
	using namespace seq;
	using set_type = concurrent_set<T, std::hash<T>, std::equal_to<T>, Alloc, Shards>;
	{
		//test construct from initializer list
		set_type set ({ 1,9,2,8,3,7,4,6,5,2, 7 },al);
		std::unordered_set<T> uset = { 1,9,2,8,3,7,4,6,5, 2, 7 };
		SEQ_TEST(test_set_equals(set, uset));
		SEQ_TEST(!set.empty());
		SEQ_TEST(set.max_size() > 0);
	}
	{
		//construct from range
		std::vector<T> v = { 1,9,2,8,3,7,4,6,5,2, 7 };
		set_type set(v.begin(), v.end(),al);
		std::unordered_set<T> uset(v.begin(), v.end());
		SEQ_TEST(test_set_equals(set, uset));
	}
	{
		// push_front/back and sorted
		std::vector<T> v;
		for (size_t i = 0; i < 10000; ++i)
			v.push_back(static_cast<T>(i));
		seq::random_shuffle(v.begin(), v.end());

		set_type set(al);
		std::unordered_set<T> uset;
		for (size_t i = 0; i < v.size()/2; ++i) {
			uset.insert(v[i]);
			set.insert(v[i]);
		}

		//test various functions for compilation

		//add existing key
		set.emplace(v[0]);
		set.emplace(v[0]);

		set.insert(v.back());
		uset.insert(v.back());

		set.insert( v.back());
		uset.insert(uset.begin(), v.back());

		SEQ_TEST(set.count(v[0]) == 1);
		SEQ_TEST(set.count(v[v.size()-2]) == 0);
		SEQ_TEST(set.contains(v[0]) );
		SEQ_TEST(!set.contains(v[v.size() - 2]));
	
		// insert everything (half already in the set)
		set.insert(v.begin(), v.end());
		uset.insert(v.begin(), v.end());

		//erase
		{
			set.erase(v[0]);
			set.erase(v[1]);

			auto uit = uset.find(v[0]);
			uset.erase(uit); //erase iterator
			uset.erase(v[1]); //erase key
		}
		

		SEQ_TEST(test_set_equals(set, uset));
	}
	{
		//test rehash() with duplicate removal
		
		std::vector<T> v;
		for (size_t i = 0; i < 10000; ++i)
			v.push_back(static_cast<T>(i));
		for (size_t i = 0; i < 10000; ++i)
			v.push_back(static_cast<T>(i));
		seq::random_shuffle(v.begin(), v.end());

		set_type set(al);
		std::unordered_set<T> uset;

		uset.insert(v.begin(), v.end());
		set.insert(v.begin(), v.end());
		SEQ_TEST(test_set_equals(set, uset));

		
		//remove half
		for (size_t i = 0; i < v.size() / 2; ++i) {
			uset.erase(v[i]);
			set.erase(v[i]);
		}
		SEQ_TEST(test_set_equals(set, uset));
	}
	{
		//test swap/move
		set_type set(al), set2 ( { 1,9,2,8,3,7,4,6,5,2, 7 },al);
		std::unordered_set<T> uset, uset2 = { 1,9,2,8,3,7,4,6,5, 2, 7 };

		{
			//manual move
			set = std::move(set2);
			uset = std::move(uset2);
			SEQ_TEST(test_set_equals(set, uset));
			SEQ_TEST(test_set_equals(set2, uset2));
		}
		{
			set.swap(set2);
			uset.swap(uset2);
			SEQ_TEST(test_set_equals(set, uset));
			SEQ_TEST(test_set_equals(set2, uset2));
		}
		{
			std::swap(set, set2);
			std::swap(uset, uset2);
			SEQ_TEST(test_set_equals(set, uset));
			SEQ_TEST(test_set_equals(set2, uset2));
		}
	}
	{
		//test copy
		std::vector<T> v;
		for (size_t i = 0; i < 10000; ++i)
			v.push_back(static_cast<T>(i));
		seq::random_shuffle(v.begin(), v.end());

		set_type set(al);
		std::unordered_set<T> uset;
		uset.insert(v.begin(), v.end());
		set.insert(v.begin(), v.end());

		{
			//test copy construct
			set_type set2{ set,al };
			std::unordered_set<T> uset2 = uset;
			SEQ_TEST(test_set_equals(set2, uset2));
		}
		{
			//test copy operator
			set_type set2(al); set2= set;
			std::unordered_set<T> uset2; uset2 = uset;
			SEQ_TEST(test_set_equals(set2, uset2));

			//test equality
			SEQ_TEST(set == set2);
			SEQ_TEST(uset == uset2);
		}
		
	}

	{
		//test with non POD type
		std::vector<std::string> v;
		for (int i = 0; i < 10000; ++i)
			v.push_back(generate_random_string<std::string>(32));
		seq::random_shuffle(v.begin(), v.end());

		concurrent_set<std::string> set;
		std::unordered_set<std::string> uset;
		uset.insert(v.begin(), v.end());
		set.insert(v.begin(), v.end());
		SEQ_TEST(test_set_equals(set, uset));

		//erase half
		for (size_t i = 0; i < v.size(); i+=2) {
			set.erase(v[i]);
			uset.erase(v[i]);
		}
		SEQ_TEST(test_set_equals(set, uset));

		//reinsert all (half already exists)
		uset.insert(v.begin(), v.end());
		set.insert(v.begin(), v.end());
		SEQ_TEST(test_set_equals(set, uset));

		set.clear();
		uset.clear();
		SEQ_TEST(test_set_equals(set, uset));
	}
}




template<class T, unsigned Shards, class Alloc>
inline void test_concurrent_map_logic(const Alloc & al)
{
	using Al  = typename std::allocator_traits<Alloc>::template rebind_alloc<std::pair<T, T> >;

	using namespace seq;
	using map_type = concurrent_map<T, T,std::hash<T>, std::equal_to<T>, Al, Shards >;
	using umap_type = std::unordered_map<T, T>;
	{
		//test construct from initializer list
		map_type set ( { {1,1},{9,9},{2,2},{8,8},{3,3},{7,7},{4,4},{6,6},{5,5},{2,2}, {7,7} }, al);
		umap_type uset = { {1,1},{9,9},{2,2},{8,8},{3,3},{7,7},{4,4},{6,6},{5,5},{2,2}, {7,7} };
		SEQ_TEST(test_map_equals(set, uset));
		SEQ_TEST(!set.empty());
		SEQ_TEST(set.max_size() > 0);
	}
	{
		//construct from range
		std::vector<std::pair<T,T> > v = { {1,1},{9,9},{2,2},{8,8},{3,3},{7,7},{4,4},{6,6},{5,5},{2,2}, {7,7} };
		map_type set(v.begin(), v.end(),al);
		umap_type uset(v.begin(), v.end());
		SEQ_TEST(test_map_equals(set, uset));
	}
	{
		// push_front/back and sorted
		std::vector<T> v;
		for (size_t i = 0; i < 10000; ++i)
			v.push_back(static_cast<T>(i));
		seq::random_shuffle(v.begin(), v.end());

		map_type set(al);
		umap_type uset;
		for (size_t i = 0; i < v.size() / 2; ++i) {
			uset.emplace(v[i],v[i]);
			set.emplace(v[i],v[i]);
		}

		//test various functions for compilation

		//add existing key
		set.emplace(v[0], v[0]);
		set.emplace(std::pair<T, T>(v[0], v[0]));
		set.emplace(v[0], v[0]);
		set.insert(std::pair<T, T>(v[0], v[0]));
		set.insert(std::pair<T, T>(v[0], v[0]));
		set.emplace(std::pair<T, T>(v[0], v[0]));

		//replace keys
		set.insert_or_assign(v[0], v[0] * 2);
		set.insert_or_assign(v[0], v[0] * 2);
		uset[v[0]]= v[0] * 2;

		set.insert_or_assign(v[1], v[1] * 2);
		set.insert_or_assign(v[1], v[1] * 2);
		set.insert_or_assign(v[2], v[2] * 2);
		set.insert_or_assign(v[2], v[2] * 2);
		uset[v[1]]= v[1] * 2;
		uset[v[2]]= v[2] * 2;

		SEQ_TEST(test_map_equals(set, uset));

		set.visit(v[0], [&](auto& a) {a.second = v[0] * 2; });
		set.visit(v[0], [&](auto& a) {a.second = v[0] * 2; });
		

		//try_emplace
		set.try_emplace(v[0], v[0]); //fail
		set.try_emplace(v[v.size()/2], v[v.size() / 2]); //succeed
		set.try_emplace(v[0], v[0]); //fail
		set.try_emplace(v[v.size() / 2], v[v.size() / 2]); //fail

		set.try_emplace(v[0], v[0]); //fail
		set.try_emplace(v[v.size() / 2 +1], v[v.size() / 2 +1]); //succeed
		set.try_emplace(v[0], v[0]); //fail
		set.try_emplace(v[v.size() / 2], v[v.size() / 2]); //fail

		set.try_emplace(v[0], v[0]); //fail
		set.try_emplace(v[v.size() / 2 + 2], v[v.size() / 2 + 2]); //succeed
		set.try_emplace( v[0], v[0]); //fail
		set.try_emplace( v[v.size() / 2], v[v.size() / 2]); //fail

		uset.emplace(v[v.size() / 2], v[v.size() / 2]);
		uset.emplace(v[v.size() / 2+1], v[v.size() / 2+1]);
		uset.emplace(v[v.size() / 2+2], v[v.size() / 2+2]);


		SEQ_TEST(test_map_equals(set, uset));

		


		set.emplace(v.back(), v.back());
		uset.emplace(v.back(), v.back());

		SEQ_TEST(set.count(v[0]) == 1);
		SEQ_TEST(set.count(v[v.size() - 2]) == 0);
		SEQ_TEST(set.contains(v[0]));
		SEQ_TEST(!set.contains(v[v.size() - 2]));

		// insert everything (half already in the set)
		std::vector<std::pair<T, T> > vv;
		for (auto i = v.begin(); i != v.end(); ++i) vv.emplace_back(*i, *i);
		
		set.insert(vv.begin(), vv.end());
		uset.insert(vv.begin(), vv.end());
		

		//erase
		{
			set.erase(v[0]); //erase iterator
			set.erase(v[1]); //erase key

			auto uit = uset.find(v[0]);
			uset.erase(uit); //erase iterator
			uset.erase(v[1]); //erase key
		}


		SEQ_TEST(test_map_equals(set, uset));
	}
	{
		//test swap/move
		map_type set, set2( { {1,1},{9,9},{2,2},{8,8},{3,3},{7,7},{4,4},{6,6},{5,5},{2,2}, {7,7} }, al);
		umap_type uset, uset2 = { {1,1},{9,9},{2,2},{8,8},{3,3},{7,7},{4,4},{6,6},{5,5},{2,2}, {7,7} };

		{
			//manual move
			set = std::move(set2);
			uset = std::move(uset2);
			SEQ_TEST(test_map_equals(set, uset));
			SEQ_TEST(test_map_equals(set2, uset2));
		}
		{
			set.swap(set2);
			uset.swap(uset2);
			SEQ_TEST(test_map_equals(set, uset));
			SEQ_TEST(test_map_equals(set2, uset2));
		}
		{
			std::swap(set, set2);
			std::swap(uset, uset2);
			SEQ_TEST(test_map_equals(set, uset));
			SEQ_TEST(test_map_equals(set2, uset2));
		}
	}
	{
		//test copy
		std::vector<std::pair<T, T> > v;
		for (size_t i = 0; i < 10000; ++i)
			v.emplace_back(static_cast<T>(i), static_cast<T>(i));
		seq::random_shuffle(v.begin(), v.end());

		map_type set(al);
		umap_type uset;
		uset.insert(v.begin(), v.end());
		set.insert(v.begin(), v.end());

		{
			//test copy construct
			map_type set2 = set;
			umap_type uset2 = uset;
			SEQ_TEST(test_map_equals(set2, uset2));
		}
		{
			//test copy operator
			map_type set2; set2 = set;
			umap_type uset2; uset2 = uset;
			SEQ_TEST(test_map_equals(set2, uset2));

			//test equality
			SEQ_TEST(set == set2);
			SEQ_TEST(uset == uset2);
		}

	}

	{
		//test with non POD type
		std::vector< std::pair<std::string,std::string> > v;
		for (int i = 0; i < 10000; ++i)
			v.emplace_back(generate_random_string<std::string>(32), generate_random_string<std::string>(32));
		seq::random_shuffle(v.begin(), v.end());

		concurrent_map<std::string, std::string> set;
		std::unordered_map<std::string, std::string> uset;
		uset.insert(v.begin(), v.end());
		set.insert(v.begin(), v.end());
		SEQ_TEST(test_map_equals(set, uset));

		//erase half
		for (size_t i = 0; i < v.size(); i += 2) {
			set.erase(v[i].first);
			uset.erase(v[i].first);
		}
		SEQ_TEST(test_map_equals(set, uset));

		//reinsert all (half already exists)
		uset.insert(v.begin(), v.end());
		set.insert(v.begin(), v.end());
		SEQ_TEST(test_map_equals(set, uset));

		set.clear();
		uset.clear();
		SEQ_TEST(test_map_equals(set, uset));
	}
}


template<class T, class Hash, unsigned Shards, class Alloc>
void test_heavy_set(size_t count, const Alloc & al, unsigned seed = 0)
{
	using key_type = T;
	std::vector<key_type> keys(count);
	for (size_t i = 0; i < count; ++i)
		keys[i] = static_cast<key_type>(i);
	seq::random_shuffle(keys.begin(), keys.end(),seed);

	seq::concurrent_set<T,Hash, std::equal_to<>,Alloc, Shards> s(al);

	for(int k=0; k < 2; ++k)
	{
		//insert range
		s.insert(keys.begin(), keys.end());
		SEQ_TEST(s.size() == count);

		// find all
		for (size_t i = 0; i < count; ++i)
		{
			SEQ_TEST(s.visit(keys[i], [&](const auto& ) { }));
		}

		//failed find
		for (size_t i = 0; i < count; ++i)
		{
			size_t ke = i + count;
			SEQ_TEST(!s.visit(ke, [](const auto&) {}));
		}



		s.clear();
		SEQ_TEST(s.size() == 0);

		//insert all one by one
		for (size_t i = 0; i < count; ++i)
		{
			s.insert(keys[i]);
			// find while inserting
			for (size_t j = 0; j <= i; ++j)
			{
				SEQ_TEST(s.visit(keys[j], [&](const auto&) {}));
			}
			// failed find
			for (size_t j = i + 1; j < count; ++j)
			{
				SEQ_TEST(!s.visit(keys[j], [](const auto&) {}));
			}
		}
		SEQ_TEST(s.size() == count);

		//failed insertion
		for (size_t i = 0; i < count; ++i)
			s.insert(keys[i]);
		SEQ_TEST(s.size() == count);

		//failed range insertion
		s.insert(keys.begin(), keys.end());
		SEQ_TEST(s.size() == count);
		

		

		// find all
		for (size_t i = 0; i < count; ++i)
		{
			SEQ_TEST(s.visit(keys[i], [&](const auto&) {}));
		}

		//failed find
		for (size_t i = 0; i < count; ++i)
		{
			size_t ke = i + count;
			SEQ_TEST(!s.visit(ke, [](const auto&) {}));
		}

		//erase half
		size_t cc = (count / 2U) * 2U;
		for (size_t i = 0; i < cc; i += 2) {
			
			SEQ_TEST(s.erase(keys[i]));
		}
		SEQ_TEST(s.size() == count / 2);

		// find all
		for (size_t i = 1; i < count; i+=2)
		{
			if (i >= count)
				break;
			SEQ_TEST(s.visit(keys[i], [&](const auto&) {}));
			
		}

		//failed
		for (size_t i = 0; i < cc; i += 2)
		{
			SEQ_TEST(!s.visit(keys[i], [](const auto&) {}));
			
		}
	}

	//erase remaining keys
	for (size_t i = 0; i < count; ++i)
	{
		s.erase(keys[i]);
	}

	SEQ_TEST(s.size() == 0);
}

template<unsigned Shards>
void test_concurrent_map_members()
{
	using map_type = seq::concurrent_map<std::string, std::string, seq::hasher<std::string>, std::equal_to<>, std::allocator<std::pair<std::string, std::string>>, Shards >;

	{
		// construct and destroy empty map
		map_type map;
		map_type map2;
		SEQ_TEST(map == map2);
	}

	map_type map;
	map.emplace("toto", "tutu");

	// Constructors
	{
		map_type map2(12345);
		map2.emplace("toto", "tutu");
		SEQ_TEST(map2 == map);
	}
	{
		map_type map2(12345, seq::hasher<std::string>{}, std::equal_to<>{}, std::allocator<std::pair<std::string, std::string>>{});
		map2.emplace("toto", "tutu");
		SEQ_TEST(map2 == map);
	}
	{
		std::vector<std::pair<std::string, std::string>> vec = { {"toto","tutu"} };
		map_type map2(vec.begin(), vec.end(), 12345);
		SEQ_TEST(map2 == map);
	}
	{
		map_type map2(std::allocator<std::pair<std::string, std::string>>{});
		map2.emplace("toto", "tutu");
		SEQ_TEST(map2 == map);
	}
	{
		std::vector<std::pair<std::string, std::string>> vec = { {"toto","tutu"} };
		map_type map2(vec.begin(),vec.end(),std::allocator<std::pair<std::string, std::string>>{});
		SEQ_TEST(map2 == map);
	}
	{
		map_type map2 = map;
		SEQ_TEST(map2 == map);
	}
	{
		map_type map2(map, std::allocator<std::pair<std::string, std::string>>{});
		SEQ_TEST(map2 == map);
	}
	{
		map_type map2 = map;
		map_type map3 = std::move(map2);
		SEQ_TEST(map3 == map);
	}
	{
		map_type map2 = map;
		map_type map3 ( std::move(map2), std::allocator<std::pair<std::string, std::string>>{});
		SEQ_TEST(map3 == map);
	}
	{
		map_type map2 = { {"toto","tutu"} };
		SEQ_TEST(map2 == map);
	}
	{
		map_type map2 ({ {"toto","tutu"} }, 123);
		SEQ_TEST(map2 == map);
	}
	{
		map_type map2(123456, std::allocator<std::pair<std::string, std::string>>{});
		map2.emplace("toto", "tutu");
		SEQ_TEST(map2 == map);
	}
	{
		std::vector<std::pair<std::string, std::string>> vec = { {"toto","tutu"} };
		map_type map2(vec.begin(), vec.end(), 123456, std::allocator<std::pair<std::string, std::string>>{});
		SEQ_TEST(map2 == map);
	}
	{
		std::vector<std::pair<std::string, std::string>> vec = { {"toto","tutu"} };
		map_type map2(vec.begin(), vec.end(), 123456, seq::hasher<std::string>{}, std::allocator<std::pair<std::string, std::string>>{});
		SEQ_TEST(map2 == map);
	}
	{
		map_type map2({ {"toto","tutu"} }, std::allocator<std::pair<std::string, std::string>>{});
		SEQ_TEST(map2 == map);
	}
	{
		map_type map2({ {"toto","tutu"} }, 123456, std::allocator<std::pair<std::string, std::string>>{});
		SEQ_TEST(map2 == map);
	}
	{
		map_type map2({ {"toto","tutu"} }, 123456, seq::hasher<std::string>{}, std::allocator<std::pair<std::string, std::string>>{});
		SEQ_TEST(map2 == map);
	}

	// Assignment
	{
		map_type map2;
		map2 = map;
		SEQ_TEST(map2 == map);

		map_type map3;
		map3 = std::move(map2);
		SEQ_TEST(map3 == map);
	}
	
	// Getters
	{
		map_type map2 = map;
		SEQ_TEST(map2.size() == map.size());
		SEQ_TEST(map2.max_size() == map.max_size());
		SEQ_TEST(map2.empty() == map.empty());
		SEQ_TEST(map2.load_factor() == map.load_factor());
		SEQ_TEST(map2.max_load_factor() == map.max_load_factor());
		SEQ_TEST(map2.get_allocator() == map.get_allocator());
		auto h1 = map2.hash_function();
		auto h2 = map.hash_function();
		SEQ_TEST(sizeof(h1) == sizeof(h2));
		
		auto e1 = map2.key_eq();
		auto e2 = map.key_eq();
		SEQ_TEST(sizeof(e1) == sizeof(e2));
	}
	// Load factor
	{
		std::vector<std::pair<std::string, std::string>> vec;
		for (size_t i = 0; i < 1000; ++i) {
			vec.push_back(std::make_pair(seq::generate_random_string<std::string>(63), seq::generate_random_string<std::string>(63)));
		}

		map_type map1(vec.begin(), vec.end());
		map_type map2(vec.begin(), vec.end());
		SEQ_TEST(map1 == map2);

		map1.max_load_factor(0.2f);
		SEQ_TEST(map1 == map2);
		map1.max_load_factor(4.f);
		SEQ_TEST(map1 == map2);

		// rehash
		map2.rehash(16);
		SEQ_TEST(map1 == map2);

		// clear
		map1.clear();
		map2.clear();
		SEQ_TEST(map1 == map2);
	}
	{
		//Clear empty
		map_type map1;
		map1.clear();
	}
	{
		//Clear 1 value
		map_type map1;
		map1.emplace("toto", "tutu");
		map1.clear();
	}
	{
		// reserve and swap
		map_type map1;
		map1.reserve(10000);
		map1.emplace("toto", "tutu");
		SEQ_TEST(map1 == map);

		map_type map2;
		map2.emplace("gg", "hh");
		map_type map3 = map2;

		map1.swap(map2);
		SEQ_TEST(map2 == map);
		SEQ_TEST(map1 == map3);
	}

	// visit all
	{
		std::vector<std::pair<std::string, std::string>> vec;
		for (size_t i = 0; i < 1000; ++i) 
			vec.push_back(std::make_pair(seq::generate_random_string<std::string>(63), seq::generate_random_string<std::string>(63)));
		std::sort(vec.begin(), vec.end(),[](const auto& l, const auto& r) {return l.first < r.first; });
		auto it = std::unique(vec.begin(), vec.end(), [](const auto& l, const auto& r) {return l.first == r.first; });
		vec.erase(it, vec.end());

		map_type map1(vec.begin(), vec.end());
		size_t count = 0;
		map1.visit_all([&](const auto&) {++count; });
		SEQ_TEST(count == vec.size());
		count = 0;
		map1.cvisit_all([&](const auto&) {++count; });
		SEQ_TEST(count == vec.size());

		map1.visit_all([](auto& v) {v.second = std::string(); });
		count = 0;
		map1.visit_all([&](auto& v) {count += v.second.empty(); });
		SEQ_TEST(count == vec.size());
	}

#ifdef SEQ_HAS_CPP_17
	// visit all parallel
	{
		std::vector<std::pair<std::string, std::string>> vec;
		for (size_t i = 0; i < 1000; ++i)
			vec.push_back(std::make_pair(seq::generate_random_string<std::string>(63), seq::generate_random_string<std::string>(63)));
		std::sort(vec.begin(), vec.end(), [](const auto& l, const auto& r) {return l.first < r.first; });
		auto it = std::unique(vec.begin(), vec.end(), [](const auto& l, const auto& r) {return l.first == r.first; });
		vec.erase(it, vec.end());

		map_type map1(vec.begin(), vec.end());
		std::atomic<size_t> count{ 0 };
		map1.visit_all(std::execution::par, [&](const auto&) {++count; });
		SEQ_TEST(count == vec.size());
		count.store( 0);
		map1.cvisit_all(std::execution::par, [&](const auto&) {++count; });
		SEQ_TEST(count == vec.size());

		map1.visit_all(std::execution::par, [](auto& v) {v.second = std::string(); });
		count.store(0);
		map1.visit_all(std::execution::par, [&](auto& v) {count += v.second.empty(); });
		SEQ_TEST(count == vec.size());
	}
#endif
	
	// Single visit, count, contains
	{
		std::vector<std::pair<std::string, std::string>> vec;
		for (size_t i = 0; i < 1000; ++i)
			vec.push_back(std::make_pair(seq::generate_random_string<std::string>(63), seq::generate_random_string<std::string>(63)));
		std::sort(vec.begin(), vec.end(), [](const auto& l, const auto& r) {return l.first < r.first; });
		auto it = std::unique(vec.begin(), vec.end(), [](const auto& l, const auto& r) {return l.first == r.first; });
		vec.erase(it, vec.end());
		
		map_type map1(vec.begin(), vec.begin() + vec.size()/2);

		for (size_t i = 0; i < vec.size()/2; ++i) {
			
			SEQ_TEST(map1.cvisit(vec[i].first, [](const auto&) {}));
			SEQ_TEST(map1.cvisit(vec[i].first.c_str(), [](const auto&) {}));
			SEQ_TEST(map1.visit(vec[i].first, [](auto& v) {v.second.clear(); }));
			SEQ_TEST(map1.count(vec[i].first) == 1);
			SEQ_TEST(map1.count(vec[i].first.c_str()) == 1);
			SEQ_TEST(map1.contains(vec[i].first) == true);
			SEQ_TEST(map1.contains(vec[i].first.c_str()) == true);
		}
		for (size_t i = vec.size() / 2; i < vec.size(); ++i) {

			SEQ_TEST(!map1.cvisit(vec[i].first, [](const auto&) {}));
			SEQ_TEST(!map1.cvisit(vec[i].first.c_str(), [](const auto&) {}));
			SEQ_TEST(!map1.visit(vec[i].first, [](auto& ) { }));
			SEQ_TEST(map1.count(vec[i].first) == 0);
			SEQ_TEST(map1.count(vec[i].first.c_str()) == 0);
			SEQ_TEST(map1.contains(vec[i].first) == false);
			SEQ_TEST(map1.contains(vec[i].first.c_str()) == false);
		}
	}

	// Emplace
	{
		map_type map1;
		map1.emplace("toto", "tutu");
		SEQ_TEST(map1 == map);

		map_type map2;
		map2.emplace_or_cvisit( "toto", "tutu", [](const auto&) {});
		SEQ_TEST(map2 == map);

		map2.emplace_or_visit("toto", "tutu", [](auto& v) {v.second.clear(); });
		SEQ_TEST(map2 != map);
	}
	// Try emplace
	{
		map_type map1;
		map1.try_emplace("toto", "tutu");
		SEQ_TEST(map1 == map);
		map1.try_emplace(std::string("toto"), std::string("tutu"));
		SEQ_TEST(map1 == map);

		map_type map2;
		map2.try_emplace_or_cvisit( "toto", "tutu", [](const auto&) {});
		map2.try_emplace_or_cvisit( std::string("toto"), std::string("tutu"), [](const auto&) {});
		SEQ_TEST(map2 == map);

		map2.try_emplace_or_visit( "toto", "tutu", [](auto& v) {v.second.clear(); });
		SEQ_TEST(map2 != map);
	}
	// Insert
	{
		map_type map1;
		map1.insert(std::make_pair("toto", "tutu"));
		SEQ_TEST(map1 == map);

		map_type map2;
		auto p = std::make_pair("toto", "tutu");
		map2.insert(p);
		SEQ_TEST(map2 == map);

		map_type map3;
		map3.insert({ {"toto","tutu"} });
		SEQ_TEST(map3 == map);

		map_type map4;
		map4.insert_or_assign("toto", "tutu");
		SEQ_TEST(map4 == map);
		map4.insert_or_assign("toto", "");
		SEQ_TEST(map4 != map);
		map4.insert_or_assign(std::string("toto"), std::string(""));
		SEQ_TEST(map4 != map);
	}
	// emplace or visit
	{
		{
			// create histogram
			std::vector<size_t> vec;
			for (size_t i = 0; i < 100; ++i)
				for (size_t j = 0; j < 1000; ++j)
					vec.push_back(i);

			seq::random_shuffle(vec.begin(), vec.end());
			seq::concurrent_map<size_t, size_t> maph;
#ifdef SEQ_HAS_CPP_17
			std::for_each(std::execution::par, vec.begin(), vec.end(), [&](size_t v) {
				maph.emplace_or_visit( v, 1, [](auto& p) {p.second++; });
				}
			);
#else
			std::for_each(vec.begin(), vec.end(), [&](size_t v) {
				maph.emplace_or_visit( v, 1, [](auto& p) {p.second++; });
		}
			);
#endif

			std::vector<size_t> hist(100);
			maph.cvisit_all([&](const auto& p) {hist[p.first] = p.second; });
			for (size_t i = 0; i < hist.size(); ++i) 
				SEQ_TEST(hist[i] == 1000);
		}
	}
	// insert or visit
	{
		{
			// create histogram
			std::vector<size_t> vec;
			for (size_t i = 0; i < 100; ++i)
				for (size_t j = 0; j < 1000; ++j)
					vec.push_back(i);

			seq::random_shuffle(vec.begin(), vec.end());
			seq::concurrent_map<size_t, size_t> maph;

#ifdef SEQ_HAS_CPP_17
			std::for_each(std::execution::par, vec.begin(), vec.end(), [&](size_t v) {
				maph.insert_or_visit(std::make_pair(v, (size_t)1), [](auto& p) {p.second++; });
				}
			);
#else
			std::for_each(vec.begin(), vec.end(), [&](size_t v) {
				maph.insert_or_visit(std::make_pair(v, 1u), [](auto& p) {p.second++; });
		}
			);
#endif

			std::vector<size_t> hist(100);
			maph.cvisit_all([&](const auto& p) {hist[p.first] = p.second; });
			for (size_t i = 0; i < hist.size(); ++i) 
				SEQ_TEST(hist[i] == 1000);
		}
	}

	// Erase
	{
		map_type map1;
		map.erase("toto");
		map.erase(std::string("toto"));
		SEQ_TEST(map.empty());

		map1.emplace("toto", "tutu");
		map.erase("toto");
		SEQ_TEST(map.empty());
	}
	// Erase if
	 {
		map_type map1;
		map1.emplace("toto", "");
		map1.emplace("tutu", "");
		map1.emplace("ok", "");
		map1.emplace("no", "");
		map1.erase_if([](const auto& v) {return v.first[0] == 't'; });
		SEQ_TEST(map1.size() == 2);

		map1.erase_if("ok", [](const auto& v) {return v.second.empty(); });
		SEQ_TEST(map1.size() == 1);

		map1.erase_if("no", [](const auto& v) {return !v.second.empty(); });
		SEQ_TEST(map1.size() == 1);

		map1.erase_if("no", [](const auto& v) {return v.second.empty(); });
		SEQ_TEST(map1.size() == 0);
	}
	// merge
	{
		seq::concurrent_map<size_t, size_t> map1;
		for (size_t i = 0; i < 100000; ++i)
			map1.emplace(i, i);

		seq::concurrent_map<size_t, size_t> map2;
		SEQ_TEST(map2.merge(map1) == 100000);
		SEQ_TEST(map1.size() == 0);

#ifdef SEQ_HAS_CPP_17
		SEQ_TEST(map1.merge(std::execution::par, map2) == 100000);
		SEQ_TEST(map2.size() == 0);
#endif
	}
}



#include "tests.hpp"

/// @brief Hash function that provokes lots of collisions
struct DummyHash
{
	size_t operator()(size_t v) const noexcept
	{
		return (v * UINT64_C(0xff51afd7ed558ccd)) & 31U;
	}
	size_t operator()(const TestDestroy<size_t>& v) const noexcept
	{
		return (static_cast<size_t>(v) * UINT64_C(0xff51afd7ed558ccd)) & 31U;
	}
};

SEQ_PROTOTYPE( int test_concurrent_map(int , char*[]))
{
	SEQ_TEST_MODULE_RETURN(concurrent_map_members_low_concurrency, 1, test_concurrent_map_members<seq::low_concurrency>());
	SEQ_TEST_MODULE_RETURN(concurrent_map_members_medium_concurrency, 1, test_concurrent_map_members<seq::medium_concurrency>());
	SEQ_TEST_MODULE_RETURN(concurrent_map_members_high_concurrency, 1, test_concurrent_map_members<seq::high_concurrency>());
	SEQ_TEST_MODULE_RETURN(concurrent_map_members_no_concurrency, 1, test_concurrent_map_members<seq::no_concurrency>());
	
	// Test radix hash map and detect potential memory leak or wrong allocator propagation
	CountAlloc<double> al;
	SEQ_TEST_MODULE_RETURN(concurrent_map_low_concurrency, 1, test_concurrent_map_logic<double,seq::low_concurrency>(al));
	SEQ_TEST(get_alloc_bytes(al) == 0);
	SEQ_TEST_MODULE_RETURN(concurrent_map_medium_concurrency, 1, test_concurrent_map_logic<double, seq::medium_concurrency>(al));
	SEQ_TEST(get_alloc_bytes(al) == 0);
	SEQ_TEST_MODULE_RETURN(concurrent_map_high_concurrency, 1, test_concurrent_map_logic<double, seq::high_concurrency>(al));
	SEQ_TEST(get_alloc_bytes(al) == 0);
	SEQ_TEST_MODULE_RETURN(concurrent_map_no_concurrency, 1, test_concurrent_map_logic<double, seq::no_concurrency>(al));
	SEQ_TEST(get_alloc_bytes(al) == 0);

	SEQ_TEST_MODULE_RETURN(concurrent_set_low_concurrency,1, test_concurrent_set_logic<double, seq::low_concurrency>(al));
	SEQ_TEST(get_alloc_bytes(al) == 0);
	SEQ_TEST_MODULE_RETURN(concurrent_set_medium_concurrency, 1, test_concurrent_set_logic<double, seq::medium_concurrency>(al));
	SEQ_TEST(get_alloc_bytes(al) == 0);
	SEQ_TEST_MODULE_RETURN(concurrent_set_high_concurrency, 1, test_concurrent_set_logic<double, seq::high_concurrency>(al));
	SEQ_TEST(get_alloc_bytes(al) == 0);
	SEQ_TEST_MODULE_RETURN(concurrent_set_no_concurrency, 1, test_concurrent_set_logic<double, seq::no_concurrency>(al));
	SEQ_TEST(get_alloc_bytes(al) == 0);

	CountAlloc<size_t> alu;
	SEQ_TEST_MODULE_RETURN(heavy_concurrent_set_low_concurrency,1, test_heavy_set<size_t, seq::hasher<size_t>, seq::low_concurrency>(10000, alu));
	SEQ_TEST(get_alloc_bytes(alu) == 0);
	SEQ_TEST_MODULE_RETURN(heavy_concurrent_set_medium_concurrency, 1, test_heavy_set<size_t, seq::hasher<size_t>, seq::medium_concurrency>(10000, alu));
	SEQ_TEST(get_alloc_bytes(alu) == 0);
	SEQ_TEST_MODULE_RETURN(heavy_concurrent_set_high_concurrency, 1, test_heavy_set<size_t, seq::hasher<size_t>, seq::high_concurrency>(10000, alu));
	SEQ_TEST(get_alloc_bytes(alu) == 0);
	SEQ_TEST_MODULE_RETURN(heavy_concurrent_set_no_concurrency, 1, test_heavy_set<size_t, seq::hasher<size_t>, seq::no_concurrency>(10000, alu));
	SEQ_TEST(get_alloc_bytes(alu) == 0);

	SEQ_TEST_MODULE_RETURN(heavy_concurrent_set_linear_low_concurrency,1, test_heavy_set<size_t, DummyHash, seq::low_concurrency> (5000, alu));
	SEQ_TEST(get_alloc_bytes(alu) == 0);
	SEQ_TEST_MODULE_RETURN(heavy_concurrent_set_linear_medium_concurrency, 1, test_heavy_set<size_t, DummyHash, seq::medium_concurrency>(5000, alu));
	SEQ_TEST(get_alloc_bytes(alu) == 0);
	SEQ_TEST_MODULE_RETURN(heavy_concurrent_set_linear_high_concurrency, 1, test_heavy_set<size_t, DummyHash, seq::high_concurrency>(5000, alu));
	SEQ_TEST(get_alloc_bytes(alu) == 0);
	SEQ_TEST_MODULE_RETURN(heavy_concurrent_set_linear_no_concurrency, 1, test_heavy_set<size_t, DummyHash, seq::no_concurrency>(5000, alu));
	SEQ_TEST(get_alloc_bytes(alu) == 0);


	CountAlloc<TestDestroy<double>> al2;

	// Test radix hash map and detect potential memory leak (allocations and non destroyed objects) or wrong allocator propagation
	SEQ_TEST_MODULE_RETURN(concurrent_map_destroy_medium_concurrency, 1, test_concurrent_map_logic<TestDestroy<double>,seq::medium_concurrency>(al2));
	SEQ_TEST(TestDestroy<double>::count() == 0);
	SEQ_TEST(get_alloc_bytes(al2) == 0);
	SEQ_TEST_MODULE_RETURN(concurrent_map_destroy_no_concurrency, 1, test_concurrent_map_logic<TestDestroy<double>, seq::no_concurrency>(al2));
	SEQ_TEST(TestDestroy<double>::count() == 0);
	SEQ_TEST(get_alloc_bytes(al2) == 0);

	
	SEQ_TEST_MODULE_RETURN(concurrent_set_destroy_medium_concurrency, 1, test_concurrent_set_logic<TestDestroy<double>, seq::medium_concurrency>(al2));
	SEQ_TEST(TestDestroy<double>::count() == 0);
	SEQ_TEST(get_alloc_bytes(al2) == 0);
	SEQ_TEST_MODULE_RETURN(concurrent_set_destroy_no_concurrency, 1, test_concurrent_set_logic<TestDestroy<double>, seq::no_concurrency>(al2));
	SEQ_TEST(TestDestroy<double>::count() == 0);
	SEQ_TEST(get_alloc_bytes(al2) == 0);


	CountAlloc<TestDestroy<size_t>> al3;
	SEQ_TEST_MODULE_RETURN(heavy_concurrent_set_destroy_no_concurrency, 1, test_heavy_set<TestDestroy<size_t>, std::hash<TestDestroy<size_t>>, seq::no_concurrency >(10000,al3));
	SEQ_TEST(TestDestroy<size_t>::count() == 0);
	SEQ_TEST(get_alloc_bytes(al3) == 0);
	SEQ_TEST_MODULE_RETURN(heavy_concurrent_set_destroy_medium_concurrency, 1, test_heavy_set<TestDestroy<size_t>, std::hash<TestDestroy<size_t>>, seq::medium_concurrency >(10000, al3));
	SEQ_TEST(TestDestroy<size_t>::count() == 0);
	SEQ_TEST(get_alloc_bytes(al3) == 0);

	SEQ_TEST_MODULE_RETURN(heavy_concurrent_set_linear_destroy_no_concurrency, 1, test_heavy_set<TestDestroy<size_t>, DummyHash, seq::no_concurrency >(10000, al3));
	SEQ_TEST(TestDestroy<size_t>::count() == 0);
	SEQ_TEST(get_alloc_bytes(al3) == 0);
	SEQ_TEST_MODULE_RETURN(heavy_concurrent_set_linear_destroy_medium_concurrency, 1, test_heavy_set<TestDestroy<size_t>, DummyHash, seq::medium_concurrency >(10000, al3));
	SEQ_TEST(TestDestroy<size_t>::count() == 0);
	SEQ_TEST(get_alloc_bytes(al3) == 0);

	

	return 0;
}
