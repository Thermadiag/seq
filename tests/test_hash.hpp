

#include "testing.hpp"
#include "ordered_map.hpp"
#include <fstream>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <iostream>


#ifndef _print
#define _print( ... ) \
	printf(__VA_ARGS__); fflush(stdout);
#endif




#include <random>
#include "tiny_string.hpp"

using namespace seq;


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


inline void test_ordered_set_logic()
{
	{
		//test construct from initializer list
		ordered_set<double> set = { 1,9,2,8,3,7,4,6,5,2, 7 };
		std::unordered_set<double> uset = { 1,9,2,8,3,7,4,6,5, 2, 7 };
		SEQ_TEST_ASSERT(hash_set_equals(set, uset));
		SEQ_TEST_ASSERT(!set.empty());
		SEQ_TEST_ASSERT(set.max_size() > 0);
	}
	{
		//construct from range
		std::vector<double> v = { 1,9,2,8,3,7,4,6,5,2, 7 };
		ordered_set<double> set(v.begin(), v.end());
		std::unordered_set<double> uset(v.begin(), v.end());
		SEQ_TEST_ASSERT(hash_set_equals(set, uset));
	}
	{
		// push_front/back and sorted
		std::vector<double> v;
		for (size_t i = 0; i < 10000; ++i)
			v.push_back((double)i);
		std::random_shuffle(v.begin(), v.end());

		ordered_set<double> set;
		std::unordered_set<double> uset;
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

		SEQ_TEST_ASSERT(set.count(v[0]) == 1);
		SEQ_TEST_ASSERT(set.count(v[v.size()-2]) == 0);
		SEQ_TEST_ASSERT(set.contains(v[0]) );
		SEQ_TEST_ASSERT(!set.contains(v[v.size() - 2]));
	
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
		

		SEQ_TEST_ASSERT(hash_set_equals(set, uset));
		set.sort();
		SEQ_TEST_ASSERT(hash_set_equals(set, uset));
		SEQ_TEST_ASSERT(hash_set_sorted(set));
	}
	{
		//test rehash() with duplicate removal
		
		std::vector<double> v;
		for (size_t i = 0; i < 10000; ++i)
			v.push_back((double)i);
		for (size_t i = 0; i < 10000; ++i)
			v.push_back((double)i);
		std::random_shuffle(v.begin(), v.end());

		ordered_set<double> set;
		std::unordered_set<double> uset;

		uset.insert(v.begin(), v.end());
		set.insert(v.begin(), v.end());
		SEQ_TEST_ASSERT(hash_set_equals(set, uset));

		uset.clear();
		set.clear();

		uset.insert(v.begin(), v.end());
		for (auto it = v.begin(); it != v.end(); ++it)
			set.sequence().insert(*it);
		set.rehash();
		SEQ_TEST_ASSERT(hash_set_equals(set, uset));

		//remove half
		for (size_t i = 0; i < v.size() / 2; ++i) {
			uset.erase(v[i]);
			set.erase(v[i]);
		}
		SEQ_TEST_ASSERT(hash_set_equals(set, uset));
		set.shrink_to_fit();
		SEQ_TEST_ASSERT(hash_set_equals(set, uset));
		set.sort();
		SEQ_TEST_ASSERT(hash_set_equals(set, uset));
		SEQ_TEST_ASSERT(hash_set_sorted(set));
	}
	{
		//test swap/move
		ordered_set<double> set, set2 = { 1,9,2,8,3,7,4,6,5,2, 7 };
		std::unordered_set<double> uset, uset2 = { 1,9,2,8,3,7,4,6,5, 2, 7 };

		{
			//manual move
			set = std::move(set2);
			uset = std::move(uset2);
			SEQ_TEST_ASSERT(hash_set_equals(set, uset));
			SEQ_TEST_ASSERT(hash_set_equals(set2, uset2));
		}
		{
			set.swap(set2);
			uset.swap(uset2);
			SEQ_TEST_ASSERT(hash_set_equals(set, uset));
			SEQ_TEST_ASSERT(hash_set_equals(set2, uset2));
		}
		{
			std::swap(set, set2);
			std::swap(uset, uset2);
			SEQ_TEST_ASSERT(hash_set_equals(set, uset));
			SEQ_TEST_ASSERT(hash_set_equals(set2, uset2));
		}
	}
	{
		//test copy
		std::vector<double> v;
		for (size_t i = 0; i < 10000; ++i)
			v.push_back((double)i);
		std::random_shuffle(v.begin(), v.end());

		ordered_set<double> set;
		std::unordered_set<double> uset;
		uset.insert(v.begin(), v.end());
		set.insert(v.begin(), v.end());

		{
			//test copy construct
			ordered_set<double> set2 = set;
			std::unordered_set<double> uset2 = uset;
			SEQ_TEST_ASSERT(hash_set_equals(set2, uset2));
		}
		{
			//test copy operator
			ordered_set<double> set2; set2= set;
			std::unordered_set<double> uset2; uset2 = uset;
			SEQ_TEST_ASSERT(hash_set_equals(set2, uset2));

			//test equality
			SEQ_TEST_ASSERT(set == set2);
			SEQ_TEST_ASSERT(uset == uset2);
		}
		
	}

	{
		//test with non POD type
		std::vector<std::string> v;
		for (int i = 0; i < 10000; ++i)
			v.push_back(generate_random_string<std::string>(32));
		std::random_shuffle(v.begin(), v.end());

		ordered_set<std::string> set;
		std::unordered_set<std::string> uset;
		uset.insert(v.begin(), v.end());
		set.insert(v.begin(), v.end());
		SEQ_TEST_ASSERT(hash_set_equals(set, uset));

		//erase half
		for (size_t i = 0; i < v.size(); i+=2) {
			set.erase(v[i]);
			uset.erase(v[i]);
		}
		SEQ_TEST_ASSERT(hash_set_equals(set, uset));

		//reinsert all (half already exists)
		uset.insert(v.begin(), v.end());
		set.insert(v.begin(), v.end());
		SEQ_TEST_ASSERT(hash_set_equals(set, uset));

		set.sort();
		SEQ_TEST_ASSERT(hash_set_equals(set, uset));
		SEQ_TEST_ASSERT(hash_set_sorted(set));

		set.clear();
		uset.clear();
		SEQ_TEST_ASSERT(hash_set_equals(set, uset));
	}
}





inline void test_ordered_map_logic()
{
	using map_type = ordered_map<double, double>;
	using umap_type = std::unordered_map<double, double>;
	{
		//test construct from initializer list
		map_type set = { {1,1},{9,9},{2,2},{8,8},{3,3},{7,7},{4,4},{6,6},{5,5},{2,2}, {7,7} };
		umap_type uset = { {1,1},{9,9},{2,2},{8,8},{3,3},{7,7},{4,4},{6,6},{5,5},{2,2}, {7,7} };
		SEQ_TEST_ASSERT(hash_map_equals(set, uset));
		SEQ_TEST_ASSERT(!set.empty());
		SEQ_TEST_ASSERT(set.max_size() > 0);
	}
	{
		//construct from range
		std::vector<std::pair<double,double> > v = { {1,1},{9,9},{2,2},{8,8},{3,3},{7,7},{4,4},{6,6},{5,5},{2,2}, {7,7} };
		map_type set(v.begin(), v.end());
		umap_type uset(v.begin(), v.end());
		SEQ_TEST_ASSERT(hash_map_equals(set, uset));
	}
	{
		// push_front/back and sorted
		std::vector<double> v;
		for (size_t i = 0; i < 10000; ++i)
			v.push_back((double)i);
		std::random_shuffle(v.begin(), v.end());

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
		set.emplace(std::pair<double, double>(v[0], v[0]));
		set.emplace_hint(set.begin(),v[0], v[0]);
		set.insert(std::pair<double, double>(v[0], v[0]));
		set.insert(set.begin(),std::pair<double, double>(v[0], v[0]));
		set.emplace_back(std::pair<double, double>(v[0], v[0]));
		set.emplace_front(std::pair<double, double>(v[0], v[0]));
		set.emplace_hint(set.begin(), std::pair<double, double>(v[0], v[0]));

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

		SEQ_TEST_ASSERT(hash_map_equals(set, uset));

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


		SEQ_TEST_ASSERT(hash_map_equals(set, uset));

		//test at() and operator[]
		for (size_t i = 0; i < v.size() / 2; ++i) {
			SEQ_TEST_ASSERT(set[v[i]] == uset[v[i]]);
			SEQ_TEST_ASSERT(set.at(v[i]) == uset.at(v[i]));
		}


		set.emplace(v.back(), v.back());
		uset.emplace(v.back(), v.back());

		SEQ_TEST_ASSERT(set.count(v[0]) == 1);
		SEQ_TEST_ASSERT(set.count(v[v.size() - 2]) == 0);
		SEQ_TEST_ASSERT(set.contains(v[0]));
		SEQ_TEST_ASSERT(!set.contains(v[v.size() - 2]));

		// insert everything (half already in the set)
		std::vector<std::pair<double, double> > vv;
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


		SEQ_TEST_ASSERT(hash_map_equals(set, uset));
		set.sort();
		SEQ_TEST_ASSERT(hash_map_equals(set, uset));
		SEQ_TEST_ASSERT(hash_map_sorted(set));
	}
	{
		//test rehash() with duplicate removal

		std::vector<std::pair<double, double> > v;
		for (size_t i = 0; i < 10000; ++i)
			v.emplace_back((double)i, (double)i);
		for (size_t i = 0; i < 10000; ++i)
			v.emplace_back((double)i, (double)i);
		std::random_shuffle(v.begin(), v.end());

		map_type set;
		umap_type uset;

		uset.insert(v.begin(), v.end());
		set.insert(v.begin(), v.end());
		SEQ_TEST_ASSERT(hash_map_equals(set, uset));

		uset.clear();
		set.clear();

		uset.insert(v.begin(), v.end());
		for (auto it = v.begin(); it != v.end(); ++it)
			set.sequence().insert(*it);
		set.rehash();
		SEQ_TEST_ASSERT(hash_map_equals(set, uset));

		//remove half
		for (size_t i = 0; i < v.size() / 2; ++i) {
			uset.erase(v[i].first);
			set.erase(v[i].first);
		}
		SEQ_TEST_ASSERT(hash_map_equals(set, uset));
		set.shrink_to_fit();
		SEQ_TEST_ASSERT(hash_map_equals(set, uset));
		set.sort();
		SEQ_TEST_ASSERT(hash_map_equals(set, uset));
		SEQ_TEST_ASSERT(hash_map_sorted(set));
	}
	{
		//test swap/move
		map_type set, set2 = { {1,1},{9,9},{2,2},{8,8},{3,3},{7,7},{4,4},{6,6},{5,5},{2,2}, {7,7} };
		umap_type uset, uset2 = { {1,1},{9,9},{2,2},{8,8},{3,3},{7,7},{4,4},{6,6},{5,5},{2,2}, {7,7} };

		{
			//manual move
			set = std::move(set2);
			uset = std::move(uset2);
			SEQ_TEST_ASSERT(hash_map_equals(set, uset));
			SEQ_TEST_ASSERT(hash_map_equals(set2, uset2));
		}
		{
			set.swap(set2);
			uset.swap(uset2);
			SEQ_TEST_ASSERT(hash_map_equals(set, uset));
			SEQ_TEST_ASSERT(hash_map_equals(set2, uset2));
		}
		{
			std::swap(set, set2);
			std::swap(uset, uset2);
			SEQ_TEST_ASSERT(hash_map_equals(set, uset));
			SEQ_TEST_ASSERT(hash_map_equals(set2, uset2));
		}
	}
	{
		//test copy
		std::vector<std::pair<double, double> > v;
		for (size_t i = 0; i < 10000; ++i)
			v.emplace_back((double)i, (double)i);
		std::random_shuffle(v.begin(), v.end());

		map_type set;
		umap_type uset;
		uset.insert(v.begin(), v.end());
		set.insert(v.begin(), v.end());

		{
			//test copy construct
			map_type set2 = set;
			umap_type uset2 = uset;
			SEQ_TEST_ASSERT(hash_map_equals(set2, uset2));
		}
		{
			//test copy operator
			map_type set2; set2 = set;
			umap_type uset2; uset2 = uset;
			SEQ_TEST_ASSERT(hash_map_equals(set2, uset2));

			//test equality
			SEQ_TEST_ASSERT(set == set2);
			SEQ_TEST_ASSERT(uset == uset2);
		}

	}

	{
		//test with non POD type
		std::vector< std::pair<std::string,std::string> > v;
		for (int i = 0; i < 10000; ++i)
			v.emplace_back(generate_random_string<std::string>(32), generate_random_string<std::string>(32));
		std::random_shuffle(v.begin(), v.end());

		ordered_map<std::string, std::string> set;
		std::unordered_map<std::string, std::string> uset;
		uset.insert(v.begin(), v.end());
		set.insert(v.begin(), v.end());
		SEQ_TEST_ASSERT(hash_map_equals(set, uset));

		//erase half
		for (size_t i = 0; i < v.size(); i += 2) {
			set.erase(v[i].first);
			uset.erase(v[i].first);
		}
		SEQ_TEST_ASSERT(hash_map_equals(set, uset));

		//reinsert all (half already exists)
		uset.insert(v.begin(), v.end());
		set.insert(v.begin(), v.end());
		SEQ_TEST_ASSERT(hash_map_equals(set, uset));

		set.sort();
		SEQ_TEST_ASSERT(hash_map_equals(set, uset));
		SEQ_TEST_ASSERT(hash_map_sorted(set));

		set.clear();
		uset.clear();
		SEQ_TEST_ASSERT(hash_map_equals(set, uset));
	}
}


