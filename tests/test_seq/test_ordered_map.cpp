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
#include "seq/ordered_map.hpp"
#include "seq/tiny_string.hpp"
#include "tests.hpp"

#include <fstream>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <iostream>
#include <algorithm>
#include <random>



template<class Alloc, class U>
using RebindAlloc = typename std::allocator_traits<Alloc>::template rebind_alloc<U>;




template<class HMap1, class HMap2>
bool hash_set_equals(const HMap1& h1, const HMap2& h2)
{
	if (h1.size() != h2.size())
		return false;


	for (auto it = h1.begin(); it != h1.end(); ++it) {
		auto found = h2.find(*it);
		if (found == h2.end())
			return false;
	}
	return true;
}
template<class HMap1, class HMap2>
bool hash_map_equals(const HMap1& h1, const HMap2& h2)
{
	if (h1.size() != h2.size())
		return false;

	
	for (auto it = h1.begin(); it != h1.end(); ++it) {
		auto found = h2.find(it->first);
		if (found == h2.end())
			return false;
		if (found->second != it->second)
			return false;
	}
	return true;
}

template<class HMap1>
bool hash_set_sorted(const HMap1& h)
{
	if (h.empty())
		return true;
	auto it = h.begin();
	auto val = *it++;
	for (; it != h.end(); ++it) {
		if (*it <= val) return false;
		val = *it;
	}
	return true;
}
template<class HMap1>
bool hash_map_sorted(const HMap1& h)
{
	if (h.empty())
		return true;
	auto it = h.begin();
	auto val = it->first;
	++it;
	for (; it != h.end(); ++it) {
		if (it->first <= val) return false;
		val = it->first;
	}
	return true;
}



template<class T, class Alloc = std::allocator<T> >
inline void test_ordered_set_logic(const Alloc & al = Alloc())
{
	using namespace seq;
	using set_type = ordered_set<T, std::hash<T>, std::equal_to<T>, Alloc>;
	{
		//test construct from initializer list
		set_type set ( { 1,9,2,8,3,7,4,6,5,2, 7 },al);
		std::unordered_set<T> uset = { 1,9,2,8,3,7,4,6,5, 2, 7 };
		SEQ_TEST(hash_set_equals(set, uset));
		SEQ_TEST(!set.empty());
		SEQ_TEST(set.max_size() > 0);
	}
	SEQ_TEST(get_alloc_bytes(al) == 0);
	{
		//construct from range
		std::vector<T> v = { 1,9,2,8,3,7,4,6,5,2, 7 };
		set_type set(v.begin(), v.end(),al);
		std::unordered_set<T> uset(v.begin(), v.end());
		SEQ_TEST(hash_set_equals(set, uset));
	}
	SEQ_TEST(get_alloc_bytes(al) == 0);
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
			if ((i & 1) == 0)
				set.push_back(v[i]);
			else
				set.push_front(v[i]);
		}

		//test various functions for compilation

		//add existing key
		set.emplace(v[0]);
		set.emplace_back(v[0]);
		set.emplace_front(v[0]);
		set.emplace_hint(set.begin(), v[0]);

		set.insert(v.back());
		uset.insert(v.back());

		set.insert(set.begin(), v.back());
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
			auto it = set.find(v[0]);
			set.erase(it); //erase iterator
			set.erase(v[1]); //erase key

			auto uit = uset.find(v[0]);
			uset.erase(uit); //erase iterator
			uset.erase(v[1]); //erase key
		}
		

		SEQ_TEST(hash_set_equals(set, uset));
		set.sort();
		SEQ_TEST(hash_set_equals(set, uset));
		SEQ_TEST(hash_set_sorted(set));
	}
	SEQ_TEST(get_alloc_bytes(al) == 0);
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
		SEQ_TEST(hash_set_equals(set, uset));

		uset.clear();
		set.clear();

		SEQ_TEST(get_alloc_bytes(al) == 0);

		uset.insert(v.begin(), v.end());
		for (auto it = v.begin(); it != v.end(); ++it)
			set.sequence().insert(*it);
		set.rehash();
		SEQ_TEST(hash_set_equals(set, uset));

		//remove half
		for (size_t i = 0; i < v.size() / 2; ++i) {
			uset.erase(v[i]);
			set.erase(v[i]);
		}
		SEQ_TEST(hash_set_equals(set, uset));
		set.shrink_to_fit();
		SEQ_TEST(hash_set_equals(set, uset));
		set.sort();
		SEQ_TEST(hash_set_equals(set, uset));
		SEQ_TEST(hash_set_sorted(set));
	}
	SEQ_TEST(get_alloc_bytes(al) == 0);
	{
		//test swap/move
		set_type set(al), set2 ( { 1,9,2,8,3,7,4,6,5,2, 7 },al);
		std::unordered_set<T> uset, uset2 = { 1,9,2,8,3,7,4,6,5, 2, 7 };

		{
			//manual move
			set = std::move(set2);
			uset = std::move(uset2);
			SEQ_TEST(hash_set_equals(set, uset));
			SEQ_TEST(hash_set_equals(set2, uset2));
		}
		{
			set.swap(set2);
			uset.swap(uset2);
			SEQ_TEST(hash_set_equals(set, uset));
			SEQ_TEST(hash_set_equals(set2, uset2));
		}
		{
			std::swap(set, set2);
			std::swap(uset, uset2);
			SEQ_TEST(hash_set_equals(set, uset));
			SEQ_TEST(hash_set_equals(set2, uset2));
		}
	}
	SEQ_TEST(get_alloc_bytes(al) == 0);
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
			set_type set2 (set,al);
			std::unordered_set<T> uset2 = uset;
			SEQ_TEST(hash_set_equals(set2, uset2));
		}
		{
			//test copy operator
			set_type set2(al); set2= set;
			std::unordered_set<T> uset2; uset2 = uset;
			SEQ_TEST(hash_set_equals(set2, uset2));

			//test equality
			SEQ_TEST(set == set2);
			SEQ_TEST(uset == uset2);
		}
		
	}
	SEQ_TEST(get_alloc_bytes(al) == 0);
	{
		//test with non POD type
		std::vector<std::string> v;
		for (int i = 0; i < 10000; ++i)
			v.push_back(generate_random_string<std::string>(32));
		seq::random_shuffle(v.begin(), v.end());

		using str_set_type = ordered_set<std::string, std::hash<std::string>, std::equal_to<std::string>, RebindAlloc<Alloc, std::string> >;
		str_set_type set{ RebindAlloc<Alloc, std::string>(al) };
		std::unordered_set<std::string> uset;
		uset.insert(v.begin(), v.end());
		set.insert(v.begin(), v.end());
		SEQ_TEST(hash_set_equals(set, uset));

		//erase half
		for (size_t i = 0; i < v.size(); i+=2) {
			set.erase(v[i]);
			uset.erase(v[i]);
		}
		SEQ_TEST(hash_set_equals(set, uset));

		//reinsert all (half already exists)
		uset.insert(v.begin(), v.end());
		set.insert(v.begin(), v.end());
		SEQ_TEST(hash_set_equals(set, uset));

		set.sort();
		SEQ_TEST(hash_set_equals(set, uset));
		SEQ_TEST(hash_set_sorted(set));

		set.clear();
		uset.clear();
		SEQ_TEST(hash_set_equals(set, uset));
	}
	SEQ_TEST(get_alloc_bytes(al) == 0);
}




template<class T>
inline void test_ordered_map_logic()
{
	using namespace seq;
	using map_type = ordered_map<T, T>;
	using umap_type = std::unordered_map<T, T>;
	{
		//test construct from initializer list
		map_type set = { {1,1},{9,9},{2,2},{8,8},{3,3},{7,7},{4,4},{6,6},{5,5},{2,2}, {7,7} };
		umap_type uset = { {1,1},{9,9},{2,2},{8,8},{3,3},{7,7},{4,4},{6,6},{5,5},{2,2}, {7,7} };
		SEQ_TEST(hash_map_equals(set, uset));
		SEQ_TEST(!set.empty());
		SEQ_TEST(set.max_size() > 0);
	}
	{
		//construct from range
		std::vector<std::pair<T, T> > v = { {1,1},{9,9},{2,2},{8,8},{3,3},{7,7},{4,4},{6,6},{5,5},{2,2}, {7,7} };
		map_type set(v.begin(), v.end());
		umap_type uset(v.begin(), v.end());
		SEQ_TEST(hash_map_equals(set, uset));
	}
	{
		// push_front/back and sorted
		std::vector<T> v;
		for (size_t i = 0; i < 10000; ++i)
			v.push_back(static_cast<T>(i));
		seq::random_shuffle(v.begin(), v.end());

		map_type set;
		umap_type uset;
		for (size_t i = 0; i < v.size() / 2; ++i) {
			uset.emplace(v[i],v[i]);
			if ((i & 1) == 0)
				set.emplace_back(v[i],v[i]);
			else
				set.emplace_front(v[i], v[i]);
		}

		//test various functions for compilation

		//add existing key
		set.emplace(v[0], v[0]);
		set.emplace(std::pair<T, T>(v[0], v[0]));
		set.emplace_hint(set.begin(),v[0], v[0]);
		set.insert(std::pair<T, T>(v[0], v[0]));
		set.insert(set.begin(),std::pair<T, T>(v[0], v[0]));
		set.emplace_back(std::pair<T, T>(v[0], v[0]));
		set.emplace_front(std::pair<T, T>(v[0], v[0]));
		set.emplace_hint(set.begin(), std::pair<T, T>(v[0], v[0]));

		//replace keys
		set.insert_or_assign(v[0], v[0] * 2);
		set.insert_or_assign(set.begin(),v[0], v[0] * 2);
		uset[v[0]]= v[0] * 2;

		set.push_back_or_assign(v[1], v[1] * 2);
		set.push_back_or_assign(set.begin(),v[1], v[1] * 2);
		set.push_front_or_assign(v[2], v[2] * 2);
		set.push_front_or_assign(set.begin(),v[2], v[2] * 2);
		uset[v[1]]= v[1] * 2;
		uset[v[2]]= v[2] * 2;

		SEQ_TEST(hash_map_equals(set, uset));

		//try_emplace
		set.try_emplace(v[0], v[0]); //fail
		set.try_emplace(v[v.size()/2], v[v.size() / 2]); //succeed
		set.try_emplace(set.begin(),v[0], v[0]); //fail
		set.try_emplace(set.begin(),v[v.size() / 2], v[v.size() / 2]); //fail

		set.try_emplace_back(v[0], v[0]); //fail
		set.try_emplace_back(v[v.size() / 2 +1], v[v.size() / 2 +1]); //succeed
		set.try_emplace_back(set.begin(), v[0], v[0]); //fail
		set.try_emplace_back(set.begin(), v[v.size() / 2], v[v.size() / 2]); //fail

		set.try_emplace_front(v[0], v[0]); //fail
		set.try_emplace_front(v[v.size() / 2 + 2], v[v.size() / 2 + 2]); //succeed
		set.try_emplace_front(set.begin(), v[0], v[0]); //fail
		set.try_emplace_front(set.begin(), v[v.size() / 2], v[v.size() / 2]); //fail

		uset.emplace(v[v.size() / 2], v[v.size() / 2]);
		uset.emplace(v[v.size() / 2+1], v[v.size() / 2+1]);
		uset.emplace(v[v.size() / 2+2], v[v.size() / 2+2]);


		SEQ_TEST(hash_map_equals(set, uset));

		//test at() and operator[]
		for (size_t i = 0; i < v.size() / 2; ++i) {
			SEQ_TEST(set[v[i]] == uset[v[i]]);
			SEQ_TEST(set.at(v[i]) == uset.at(v[i]));
		}


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
			auto it = set.find(v[0]);
			set.erase(it); //erase iterator
			set.erase(v[1]); //erase key

			auto uit = uset.find(v[0]);
			uset.erase(uit); //erase iterator
			uset.erase(v[1]); //erase key
		}


		SEQ_TEST(hash_map_equals(set, uset));
		set.sort();
		SEQ_TEST(hash_map_equals(set, uset));
		SEQ_TEST(hash_map_sorted(set));
	}
	{
		//test rehash() with duplicate removal

		std::vector<std::pair<T, T> > v;
		for (size_t i = 0; i < 10000; ++i)
			v.emplace_back(static_cast<T>(i), static_cast<T>(i));
		for (size_t i = 0; i < 10000; ++i)
			v.emplace_back(static_cast<T>(i), static_cast<T>(i));
		seq::random_shuffle(v.begin(), v.end());

		map_type set;
		umap_type uset;

		uset.insert(v.begin(), v.end());
		set.insert(v.begin(), v.end());
		SEQ_TEST(hash_map_equals(set, uset));

		uset.clear();
		set.clear();

		uset.insert(v.begin(), v.end());
		for (auto it = v.begin(); it != v.end(); ++it)
			set.sequence().insert(*it);
		set.rehash();
		SEQ_TEST(hash_map_equals(set, uset));

		//remove half
		for (size_t i = 0; i < v.size() / 2; ++i) {
			uset.erase(v[i].first);
			set.erase(v[i].first);
		}
		SEQ_TEST(hash_map_equals(set, uset));
		set.shrink_to_fit();
		SEQ_TEST(hash_map_equals(set, uset));
		set.sort();
		SEQ_TEST(hash_map_equals(set, uset));
		SEQ_TEST(hash_map_sorted(set));
	}
	{
		//test swap/move
		map_type set, set2 = { {1,1},{9,9},{2,2},{8,8},{3,3},{7,7},{4,4},{6,6},{5,5},{2,2}, {7,7} };
		umap_type uset, uset2 = { {1,1},{9,9},{2,2},{8,8},{3,3},{7,7},{4,4},{6,6},{5,5},{2,2}, {7,7} };

		{
			//manual move
			set = std::move(set2);
			uset = std::move(uset2);
			SEQ_TEST(hash_map_equals(set, uset));
			SEQ_TEST(hash_map_equals(set2, uset2));
		}
		{
			set.swap(set2);
			uset.swap(uset2);
			SEQ_TEST(hash_map_equals(set, uset));
			SEQ_TEST(hash_map_equals(set2, uset2));
		}
		{
			std::swap(set, set2);
			std::swap(uset, uset2);
			SEQ_TEST(hash_map_equals(set, uset));
			SEQ_TEST(hash_map_equals(set2, uset2));
		}
	}
	{
		//test copy
		std::vector<std::pair<T, T> > v;
		for (size_t i = 0; i < 10000; ++i)
			v.emplace_back(static_cast<T>(i), static_cast<T>(i));
		seq::random_shuffle(v.begin(), v.end());

		map_type set;
		umap_type uset;
		uset.insert(v.begin(), v.end());
		set.insert(v.begin(), v.end());

		{
			//test copy construct
			map_type set2 = set;
			umap_type uset2 = uset;
			SEQ_TEST(hash_map_equals(set2, uset2));
		}
		{
			//test copy operator
			map_type set2; set2 = set;
			umap_type uset2; uset2 = uset;
			SEQ_TEST(hash_map_equals(set2, uset2));

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

		ordered_map<std::string, std::string> set;
		std::unordered_map<std::string, std::string> uset;
		uset.insert(v.begin(), v.end());
		set.insert(v.begin(), v.end());
		SEQ_TEST(hash_map_equals(set, uset));

		//erase half
		for (size_t i = 0; i < v.size(); i += 2) {
			set.erase(v[i].first);
			uset.erase(v[i].first);
		}
		SEQ_TEST(hash_map_equals(set, uset));

		//reinsert all (half already exists)
		uset.insert(v.begin(), v.end());
		set.insert(v.begin(), v.end());
		SEQ_TEST(hash_map_equals(set, uset));

		set.sort();
		SEQ_TEST(hash_map_equals(set, uset));
		SEQ_TEST(hash_map_sorted(set));

		set.clear();
		uset.clear();
		SEQ_TEST(hash_map_equals(set, uset));
	}
}



template<class Set>
void test_heavy_set(size_t count)
{
	using key_type = typename Set::value_type;
	std::vector<key_type> keys(count);
	for (size_t i = 0; i < count; ++i)
		keys[i] = static_cast<key_type>( i);
	seq::random_shuffle(keys.begin(), keys.end());

	Set s; 

	for(int k=0; k < 2; ++k)
	{
		//insert range
		s.insert(keys.begin(), keys.end());
		SEQ_TEST(s.size() == count);

		// find all
		for (size_t i = 0; i < count; ++i)
		{
			auto it = s.find(keys[i]);
			SEQ_TEST(it != s.end());
			SEQ_TEST(*it == keys[i]);
		}

		//failed find
		for (size_t i = 0; i < count; ++i)
		{
			size_t ke = i + count;
			auto it = s.find(ke);
			SEQ_TEST(it == s.end());
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
				auto it = s.find(keys[j]);
				SEQ_TEST(it != s.end());
				SEQ_TEST(*it == keys[j]);
			}
			// failed find
			for (size_t j = i + 1; j < count; ++j)
			{
				auto it = s.find(keys[j]);
				SEQ_TEST(it == s.end());
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
			auto it = s.find(keys[i]);
			SEQ_TEST(it != s.end());
			SEQ_TEST(*it == keys[i]);
		}

		//failed find
		for (size_t i = 0; i < count; ++i)
		{
			size_t ke = i + count;
			auto it = s.find(ke);
			SEQ_TEST(it == s.end());
		}

		//erase half
		size_t cc = (count / 2U) * 2U;
		for (size_t i = 0; i < cc; i += 2) {
			auto it = s.find(keys[i]);
			s.erase(it);
		}
		SEQ_TEST(s.size() == count / 2);

		// find all
		for (size_t i = 1; i < count; i+=2)
		{
			if (i >= count)
				break;

			auto it = s.find(keys[i]);
			SEQ_TEST(it != s.end());
			SEQ_TEST(*it == keys[i]);
		}

		//failed
		for (size_t i = 0; i < cc; i += 2)
		{
			auto it = s.find(keys[i]);
			SEQ_TEST(it == s.end());
		}
	}

	//erase remaining keys
	for (size_t i = 0; i < count; ++i)
	{
		auto it = s.find(keys[i]);
		if (it != s.end())
			s.erase(it);
	}

	SEQ_TEST(s.size() == 0);
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



SEQ_PROTOTYPE( int test_ordered_map(int , char*[]))
{ 
	//Test ordered_map and detect potential memory leak or wrong allocator propagation
	SEQ_TEST_MODULE_RETURN(heavy_ordered_set,1, test_heavy_set<seq::ordered_set<size_t> >(10000));
	SEQ_TEST_MODULE_RETURN(heavy_ordered_set_linear,1, test_heavy_set<seq::ordered_set<size_t, DummyHash> >(3000));
	SEQ_TEST_MODULE_RETURN(ordered_map,1,test_ordered_map_logic<double>());
	CountAlloc<double> al;
	SEQ_TEST_MODULE_RETURN(ordered_set,1,test_ordered_set_logic<double>(al));
	SEQ_TEST(get_alloc_bytes(al) == 0);

	//Test ordered_map and detect potential memory leak or wrong allocator propagation
	SEQ_TEST_MODULE_RETURN(heavy_ordered_set_destroy, 1, test_heavy_set<seq::ordered_set<TestDestroy<size_t>> >(10000));
	SEQ_TEST(TestDestroy<size_t>::count() == 0);
	SEQ_TEST_MODULE_RETURN(heavy_ordered_set_linear_destroy, 1, test_heavy_set<seq::ordered_set<TestDestroy<size_t>, DummyHash> >(3000));
	SEQ_TEST(TestDestroy<size_t>::count() == 0);
	SEQ_TEST_MODULE_RETURN(ordered_map_destroy, 1, test_ordered_map_logic<TestDestroy<double>>());
	SEQ_TEST(TestDestroy<double>::count() == 0);
	CountAlloc<TestDestroy<double>> al2;
	SEQ_TEST_MODULE_RETURN(ordered_set_destroy, 1, test_ordered_set_logic<TestDestroy<double>>(al2));
	SEQ_TEST(TestDestroy<double>::count() == 0);
	SEQ_TEST(get_alloc_bytes(al2) == 0);

	return 0;
}
