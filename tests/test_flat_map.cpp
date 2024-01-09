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


#include <map>
#include <unordered_map>
#include <set>

#include "seq/flat_map.hpp"
#include "seq/testing.hpp"
#include "tests.hpp"
#include <iostream>



template<class T, class U>
bool set_equals(const T& s1, const U& s2)
{
	if (s1.size() != s2.size())
		return false;

	auto it1 = s1.begin();
	int cc = 0;
	for (auto it2 = s2.begin(); it2 != s2.end(); ++it2, ++it1,++cc)
		if (*it1 != *it2)
			return false;
	return true;
}
template<class T, class U>
bool map_equals(const T& s1, const U& s2)
{
	if (s1.size() != s2.size())
		return false;

	auto it1 = s1.begin();
	int cc = 0;
	for (auto it2 = s2.begin(); it2 != s2.end(); ++it2, ++it1, ++cc)
		if (it1->first != it2->first || it1->second != it2->second)
			return false;
	return true;
}


template<class Alloc, class U>
using RebindAlloc = typename std::allocator_traits<Alloc>::template rebind_alloc<U>;


template<class Container, class U>
struct rebind
{
};
template<class T,class Al, class U>
struct rebind<seq::flat_set<T,std::less<T>,Al> ,U>
{
	using type = seq::flat_set<U, std::less<U>, RebindAlloc<Al,U> >;
};
template<class T, class Al, class U>
struct rebind<seq::flat_multiset<T, std::less<T>, Al>, U>
{
	using type = seq::flat_multiset<U, std::less<U>, RebindAlloc<Al, U>>;
};
template<class T, class U>
struct rebind<std::set<T>, U>
{
	using type = std::set<U>;
};
template<class T, class U>
struct rebind<std::multiset<T>, U>
{
	using type = std::multiset<U>;
};




template<class set_type, class std_set_type, bool Unique, class Alloc>
inline void test_flat_set_or_multi_logic(const Alloc& al = Alloc())
{
	using namespace seq;
	using value_type = typename set_type::value_type;
	{
		//test construct from initializer list
		set_type set ( { 1.,9.,2.,8.,3.,7.,4.,6.,5.,2., 7. },al);
		std_set_type uset = { 1.,9.,2.,8.,3.,7.,4.,6.,5.,2., 7. };
		SEQ_TEST(set_equals(set, uset));
		SEQ_TEST(!set.empty());
		SEQ_TEST(set.max_size() > 0);
	}
	{
		//construct from range
		std::vector<double> v = { 1.,9.,2.,8.,3.,7.,4.,6.,5.,2., 7. };
		set_type set(v.begin(), v.end(),al);
		std_set_type uset(v.begin(), v.end());
		SEQ_TEST(set_equals(set, uset));
	}
	{
		// insert/emplace
		std::vector<double> v;
		for (size_t i = 0; i < 10000; ++i)
			v.push_back(static_cast<double>(i));
		seq::random_shuffle(v.begin(), v.end());

		set_type set(al);
		std_set_type uset;
		for (size_t i = 0; i < v.size() / 2; ++i) {
			uset.insert(v[i]);
			if ((i & 1) == 0)
				set.insert(v[i]);
			else
				set.emplace(v[i]);
		}
		SEQ_TEST(set_equals(set, uset));

		//test various functions for compilation

		//add already existing key
		set.emplace(v[0]);
		uset.emplace(v[0]);

		set.emplace_pos(v[0]);
		uset.emplace(v[0]);

		set.insert(set.begin(),v[0]);
		uset.insert(uset.begin(), v[0]);

		set.emplace_hint(set.begin(), v[0]);
		uset.emplace_hint(uset.begin(), v[0]);

		//add new keys
		set.insert(v.back());
		uset.insert(v.back());

		set.insert(set.begin(), v.back());
		uset.insert(uset.begin(), v.back());

		SEQ_TEST(set_equals(set, uset));
		if (Unique)
		{
			SEQ_TEST(set.count(v[0]) == 1);
			SEQ_TEST(set.count(v[v.size() - 2]) == 0);
			SEQ_TEST(set.contains(v[0]));
			SEQ_TEST(!set.contains(v[v.size() - 2]));
		}

		// insert everything (half already in the set)
		set.insert(v.begin(), v.end());
		uset.insert(v.begin(), v.end());

		SEQ_TEST(set_equals(set, uset));

		//erase
		{
			auto it = set.find(v[0]);
			set.erase(it); //erase iterator
			set.erase(v[1]); //erase key

			auto uit = uset.find(v[0]);
			uset.erase(uit); //erase iterator
			uset.erase(v[1]); //erase key
		}

		SEQ_TEST(set_equals(set, uset));

		//test push front
		for (int i = -1; i > -10000; --i)
		{
			set.emplace(i);
			uset.emplace(i);
		}
		SEQ_TEST(set_equals(set, uset));

		//test push back
		for(int i=10000; i < 20000; ++i)
		{
			set.emplace(i);
			uset.emplace(i);
		}
		SEQ_TEST(set_equals(set, uset));
	}
	
	{
		//test swap/move
		set_type set(al), set2({ 1.,9.,2.,8.,3.,7.,4.,6.,5.,2., 7. },al);
		std_set_type uset, uset2 = { 1.,9.,2.,8.,3.,7.,4.,6.,5.,2., 7. };

		{
			//manual move
			set = std::move(set2);
			uset = std::move(uset2);
			SEQ_TEST(set_equals(set, uset));
			SEQ_TEST(set_equals(set2, uset2));
		}
		{
			set.swap(set2);
			uset.swap(uset2);
			SEQ_TEST(set_equals(set, uset));
			SEQ_TEST(set_equals(set2, uset2));
		}
		{
			std::swap(set, set2);
			std::swap(uset, uset2);
			SEQ_TEST(set_equals(set, uset));
			SEQ_TEST(set_equals(set2, uset2));
		}
	}
	{
		//test copy
		std::vector<double> v;
		for (size_t i = 0; i < 10000; ++i)
			v.push_back(static_cast<double>(i));
		seq::random_shuffle(v.begin(), v.end());

		set_type set(al);
		std_set_type uset;
		uset.insert(v.begin(), v.end());
		set.insert(v.begin(), v.end());

		{
			//test copy construct
			set_type set2 (set,al);
			std_set_type uset2 = uset;
			SEQ_TEST(set_equals(set2, uset2));
		}
		{
			//test copy operator
			set_type set2(al); set2 = set;
			std_set_type uset2; uset2 = uset;
			SEQ_TEST(set_equals(set2, uset2));

			//test equality
			SEQ_TEST(set == set2);
			SEQ_TEST(uset == uset2);
		}

		uset.insert(v.begin(), v.end());
		set.insert(v.begin(), v.end());

		//randomly shuffle thinks
		seq::random_shuffle(set.tvector().begin(), set.tvector().end());
		set.sort();
		SEQ_TEST(set_equals(set, uset));
	}

	{
		//test with non POD type
		std::vector<std::string> v;
		for (int i = 0; i < 10000; ++i)
			v.push_back(generate_random_string<std::string>(32));
		seq::random_shuffle(v.begin(), v.end());

		using rebind_set_type = typename rebind<set_type, std::string>::type;
		rebind_set_type set{ RebindAlloc<Alloc,std::string>(al) };
		typename rebind<std_set_type, std::string>::type uset;
		uset.insert(v.begin(), v.end());
		set.insert(v.begin(), v.end());
		SEQ_TEST(set_equals(set, uset));

		//erase half
		for (size_t i = 0; i < v.size(); i += 2) {
			/*int c1 = set.count(v[i]);
			int c2 = uset.count(v[i]);
			if (i == 28)
				bool stop = true;

			typename rebind<std_set_type, std::string>::type before(set.begin(), set.end());*/
			set.erase(v[i]);
			//typename rebind<std_set_type, std::string>::type after(set.begin(), set.end());

			uset.erase(v[i]);
			//SEQ_TEST(set_equals(set, uset));
		}
		SEQ_TEST(set_equals(set, uset));

		//reinsert all (half already exists)
		uset.insert(v.begin(), v.end());
		set.insert(v.begin(), v.end());
		SEQ_TEST(set_equals(set, uset));

		set.clear();
		uset.clear();
		SEQ_TEST(set_equals(set, uset));
	}


	{
		std::vector<value_type> vals;
		for (int i = 0; i < 100000; ++i)
			vals.push_back(static_cast<value_type>(i));
		seq::random_shuffle(vals.begin(), vals.end());

		std_set_type ref;
		ref.insert(vals.begin(), vals.begin() + vals.size() / 2);

		
		set_type set(al);
		set.insert(vals.begin(), vals.begin() + vals.size() / 2);

		//compare flat_set with std::set
		SEQ_TEST(seq::equal(set.begin(), set.end(), ref.begin(), ref.end()));

		// add already existing values
		set.insert(vals.begin(), vals.begin() + vals.size() / 2);
		ref.insert(vals.begin(), vals.begin() + vals.size() / 2);
		SEQ_TEST(seq::equal(set.begin(), set.end(), ref.begin(), ref.end()));

		// add already existing values one by one
		for (size_t i = 0; i < vals.size() / 2; ++i)
		{
			set.insert(vals[i]);
			ref.insert(vals[i]);
		}
		SEQ_TEST(seq::equal(set.begin(), set.end(), ref.begin(), ref.end()));

		// test find_pos
		for (size_t i = 0; i < vals.size() / 2; ++i)
		{
			SEQ_TEST(set.find_pos(vals[i]) != set.size());
		}
		for (size_t i = vals.size() / 2; i < vals.size() ; ++i)
			SEQ_TEST(set.find_pos(vals[i]) == set.size());
	}

	{
		// do the same as above with sorted values
		std::vector<value_type> vals;
		for (int i = 0; i < 100000; ++i)
			vals.push_back(static_cast<value_type>(i));

		std_set_type ref;
		ref.insert(vals.begin(), vals.begin() + vals.size() / 2);

		set_type set(al);
		set.insert(vals.begin(), vals.begin() + vals.size() / 2);

		//compare flat_set with std::set
		SEQ_TEST(seq::equal(set.begin(), set.end(), ref.begin(), ref.end()));

		// add already existing values one by one
		for (size_t i = 0; i < vals.size() / 2; ++i)
		{
			set.insert(vals[i]);
			ref.insert(vals[i]);
		}
		SEQ_TEST(seq::equal(set.begin(), set.end(), ref.begin(), ref.end()));

		// test find_pos
		for (size_t i = 0; i < vals.size() / 2; ++i)
			SEQ_TEST(set.find_pos(vals[i]) != set.size());
		for (size_t i = vals.size() / 2; i < vals.size() ; ++i)
			SEQ_TEST(set.find_pos(vals[i]) == set.size());
	}
	{
		// do the same as above with sorted values abnd insert one by one
		std::vector<value_type> vals;
		for (int i = 0; i < 100000; ++i)
			vals.push_back(static_cast<value_type>(i));

		std_set_type ref;
		ref.insert(vals.begin(), vals.begin() + vals.size() / 2);

		set_type set(al);
		for (size_t i = 0; i < vals.size() / 2; ++i)
			set.insert(vals[i]);

		//compare flat_set with std::set
		SEQ_TEST(seq::equal(set.begin(), set.end(), ref.begin(), ref.end()));

		// add already existing values one by one
		for (size_t i = 0; i < vals.size() / 2; ++i)
		{
			set.insert(vals[i]);
			ref.insert(vals[i]);
		}
		SEQ_TEST(seq::equal(set.begin(), set.end(), ref.begin(), ref.end()));

		// test find_pos
		for (size_t i = 0; i < vals.size() / 2; ++i)
			SEQ_TEST(set.find_pos(vals[i]) != set.size());
		for (size_t i = vals.size() / 2; i < vals.size() ; ++i)
			SEQ_TEST(set.find_pos(vals[i]) == set.size());
	}
	{
		// do the same as above with random values and insert one by one
		std::vector<value_type> vals;
		for (int i = 0; i < 100000; ++i)
			vals.push_back(static_cast<value_type>(i));
		seq::random_shuffle(vals.begin(), vals.end());

		std_set_type ref;
		ref.insert(vals.begin(), vals.begin() + vals.size() / 2);

		set_type set(al);
		for (size_t i = 0; i < vals.size() / 2; ++i)
			set.insert(vals[i]);

		//compare flat_set with std::set
		SEQ_TEST(seq::equal(set.begin(), set.end(), ref.begin(), ref.end()));

		// add already existing values one by one
		for (size_t i = 0; i < vals.size() / 2; ++i)
		{
			set.insert(vals[i]);
			ref.insert(vals[i]);
		}
		SEQ_TEST(seq::equal(set.begin(), set.end(), ref.begin(), ref.end()));

		// test find_pos
		for (size_t i = 0; i < vals.size() / 2; ++i)
			SEQ_TEST(set.find_pos(vals[i]) != set.size());
		for (size_t i = vals.size() / 2; i < vals.size() ; ++i)
			SEQ_TEST(set.find_pos(vals[i]) == set.size());
	}
}

template<class T, class Alloc = std::allocator<T> >
inline void test_flat_set_logic(const Alloc & al = Alloc())
{
	using namespace seq;
	test_flat_set_or_multi_logic < flat_set < T, std::less<T>, Alloc > , std::set<T>, true , Alloc> (al);
}
template<class T, class Alloc = std::allocator<T> >
inline void test_flat_multiset_logic(const Alloc& al = Alloc())
{
	using namespace seq;
	test_flat_set_or_multi_logic<flat_multiset<T, std::less<T>, Alloc>, std::multiset<T>,  false, Alloc>(al);
}




template<class map_type, class umap_type, bool Unique>
inline void test_flat_map_or_multi_logic()
{
	using pair_type = typename map_type::value_type;
	using namespace seq;
	{
		//test construct from initializer list
		map_type set = { {1.,1.},{9.,9.},{2.,2.},{8.,8.},{3.,3.},{7.,7.},{4.,4.},{6.,6.},{5.,5.},{2.,2.}, {7.,7.} };
		umap_type uset = { {1.,1.},{9.,9.},{2.,2.},{8.,8.},{3.,3.},{7.,7.},{4.,4.},{6.,6.},{5.,5.},{2.,2.}, {7.,7.} };
		SEQ_TEST(map_equals(set, uset));
		SEQ_TEST(!set.empty());
		SEQ_TEST(set.max_size() > 0);
	}
	{
		//construct from range
		std::vector<pair_type > v = { {1.,1.},{9.,9.},{2.,2.},{8.,8.},{3.,3.},{7.,7.},{4.,4.},{6.,6.},{5.,5.},{2.,2.}, {7.,7.} };
		map_type set(v.begin(), v.end());
		umap_type uset(v.begin(), v.end());
		SEQ_TEST(map_equals(set, uset));
	}
	{
		// push_front/back and sorted
		std::vector<double> v;
		for (size_t i = 0; i < 10000; ++i)
			v.push_back(static_cast<double>(i));
		seq::random_shuffle(v.begin(), v.end());

		map_type set;
		umap_type uset;
		for (size_t i = 0; i < v.size() / 2; ++i) {
			uset.emplace(v[i], v[i]);
			if ((i & 1) == 0)
				set.emplace(v[i], v[i]);
			else
				set.try_emplace(v[i], v[i]);
		}

		//test various functions for compilation

		//add existing key
		set.emplace(v[0], v[0]);
		uset.emplace(v[0], v[0]);

		set.emplace(pair_type(v[0], v[0]));
		uset.emplace(pair_type(v[0], v[0]));

		set.emplace_hint(set.begin(), v[0], v[0]);
		uset.emplace_hint(uset.begin(), v[0], v[0]);

		set.insert(pair_type(v[0], v[0]));
		uset.insert(pair_type(v[0], v[0]));

		set.insert(set.begin(), pair_type(v[0], v[0]));
		uset.insert(uset.begin(), pair_type(v[0], v[0]));

		
		set.insert_or_assign(v[0], v[0]);
		set.insert_or_assign_pos(v[0], v[0]);
		set.emplace_hint(set.begin(), pair_type(v[0], v[0]));

		//replace keys
		set.insert_or_assign(v[0], v[0] * 2);
		set.insert_or_assign(set.begin(), v[0], v[0] * 2);
		uset[v[0]] = v[0] * 2;



		set.insert_or_assign(v[1], v[1] * 2);
		set.insert_or_assign(set.begin(), v[1], v[1] * 2);
		set.insert_or_assign_pos(v[2], v[2] * 2);
		set.insert_or_assign(set.begin(), v[2], v[2] * 2);
		
		uset[v[1]] = v[1] * 2;
		uset[v[2]] = v[2] * 2;

		SEQ_TEST(map_equals(set, uset));

		//try_emplace
		set.try_emplace(v[0], v[0]); //fail
		set.try_emplace(v[v.size() / 2], v[v.size() / 2]); //succeed
		set.try_emplace(set.begin(), v[0], v[0]); //fail
		set.try_emplace(set.begin(), v[v.size() / 2], v[v.size() / 2]); //fail

		set.try_emplace(v[0], v[0]); //fail
		set.try_emplace(v[v.size() / 2 + 1], v[v.size() / 2 + 1]); //succeed
		set.try_emplace(set.begin(), v[0], v[0]); //fail
		set.try_emplace(set.begin(), v[v.size() / 2], v[v.size() / 2]); //fail

		set.try_emplace_pos(v[0], v[0]); //fail
		set.try_emplace_pos(v[v.size() / 2 + 2], v[v.size() / 2 + 2]); //succeed
		set.try_emplace(set.begin(), v[0], v[0]); //fail
		set.try_emplace(set.begin(), v[v.size() / 2], v[v.size() / 2]); //fail

		uset.emplace(v[v.size() / 2], v[v.size() / 2]);
		uset.emplace(v[v.size() / 2 + 1], v[v.size() / 2 + 1]);
		uset.emplace(v[v.size() / 2 + 2], v[v.size() / 2 + 2]);


		SEQ_TEST(map_equals(set, uset));

		//randomly shuffle thinks
		seq::random_shuffle(set.tvector().begin(), set.tvector().end());
		set.sort();
		SEQ_TEST(map_equals(set, uset));

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
		std::vector<pair_type > vv;
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


		SEQ_TEST(map_equals(set, uset));
		
	}
	{
		//test rehash() with duplicate removal

		std::vector<pair_type > v;
		for (size_t i = 0; i < 10000; ++i)
			v.emplace_back(static_cast<double>(i), static_cast<double>(i));
		for (size_t i = 0; i < 10000; ++i)
			v.emplace_back(static_cast<double>(i), static_cast<double>(i));
		seq::random_shuffle(v.begin(), v.end());

		map_type set;
		umap_type uset;

		uset.insert(v.begin(), v.end());
		set.insert(v.begin(), v.end());
		SEQ_TEST(map_equals(set, uset));

		uset.clear();
		set.clear();

		uset.insert(v.begin(), v.end());
		set.insert(v.begin(), v.end());
		SEQ_TEST(map_equals(set, uset));

		//remove half
		for (size_t i = 0; i < v.size() / 2; ++i) {
			uset.erase(v[i].first);
			set.erase(v[i].first);
		}
		SEQ_TEST(map_equals(set, uset));
	}
	{
		//test swap/move
		map_type set, set2 = { {1.,1.},{9.,9.},{2.,2.},{8.,8.},{3.,3.},{7.,7.},{4.,4.},{6.,6.},{5.,5.},{2.,2.}, {7.,7.} };
		umap_type uset, uset2 = { {1.,1.},{9.,9.},{2.,2.},{8.,8.},{3.,3.},{7.,7.},{4.,4.},{6.,6.},{5.,5.},{2.,2.}, {7.,7.} };

		{
			//manual move
			set = std::move(set2);
			uset = std::move(uset2);
			SEQ_TEST(map_equals(set, uset));
			SEQ_TEST(map_equals(set2, uset2));
		}
		{
			set.swap(set2);
			uset.swap(uset2);
			SEQ_TEST(map_equals(set, uset));
			SEQ_TEST(map_equals(set2, uset2));
		}
		{
			std::swap(set, set2);
			std::swap(uset, uset2);
			SEQ_TEST(map_equals(set, uset));
			SEQ_TEST(map_equals(set2, uset2));
		}
	}
	{
		//test copy
		std::vector<pair_type > v;
		for (size_t i = 0; i < 10000; ++i)
			v.emplace_back(static_cast<double>(i), static_cast<double>(i));
		seq::random_shuffle(v.begin(), v.end());

		map_type set;
		umap_type uset;
		uset.insert(v.begin(), v.end());
		set.insert(v.begin(), v.end());

		{
			//test copy construct
			map_type set2 = set;
			umap_type uset2 = uset;
			SEQ_TEST(map_equals(set2, uset2));
		}
		{
			//test copy operator
			map_type set2; set2 = set;
			umap_type uset2; uset2 = uset;
			SEQ_TEST(map_equals(set2, uset2));

			//test equality
			SEQ_TEST(set == set2);
			SEQ_TEST(uset == uset2);
		}

	}

	
}







template<class map_type, class umap_type>
inline void test_flat_multimap_logic()
{
	using pair_type = typename map_type::value_type;
	using namespace seq;
	{
		//test construct from initializer list
		map_type set = { {1.,1.},{9.,9.},{2.,2.},{8.,8.},{3.,3.},{7.,7.},{4.,4.},{6.,6.},{5.,5.},{2.,2.}, {7.,7.} };
		umap_type uset = { {1.,1.},{9.,9.},{2.,2.},{8.,8.},{3.,3.},{7.,7.},{4.,4.},{6.,6.},{5.,5.},{2.,2.}, {7.,7.} };
		SEQ_TEST(map_equals(set, uset));
		SEQ_TEST(!set.empty());
		SEQ_TEST(set.max_size() > 0);
	}
	{
		//construct from range
		std::vector<pair_type > v = { {1.,1.},{9.,9.},{2.,2.},{8.,8.},{3.,3.},{7.,7.},{4.,4.},{6.,6.},{5.,5.},{2.,2.}, {7.,7.} };
		map_type set(v.begin(), v.end());
		umap_type uset(v.begin(), v.end());
		SEQ_TEST(map_equals(set, uset));
	}
	{
		// push_front/back and sorted
		std::vector<double> v;
		for (size_t i = 0; i < 10000; ++i)
			v.push_back(static_cast<double>(i));
		seq::random_shuffle(v.begin(), v.end());

		map_type set;
		umap_type uset;
		for (size_t i = 0; i < v.size() / 2; ++i) {
			uset.emplace(v[i], v[i]);
			if ((i & 1) == 0)
				set.emplace(v[i], v[i]);
			else
				set.try_emplace(v[i], v[i]);
		}

		//test various functions for compilation

		//add existing key
		set.emplace(v[0], v[0]);
		uset.emplace(v[0], v[0]);

		set.emplace(pair_type(v[0], v[0]));
		uset.emplace(pair_type(v[0], v[0]));

		set.emplace_hint(set.begin(), v[0], v[0]);
		uset.emplace_hint(uset.begin(), v[0], v[0]);

		set.insert(pair_type(v[0], v[0]));
		uset.insert(pair_type(v[0], v[0]));

		set.insert(set.begin(), pair_type(v[0], v[0]));
		uset.insert(uset.begin(), pair_type(v[0], v[0]));

		SEQ_TEST(map_equals(set, uset));

		//randomly shuffle thinks
		seq::random_shuffle(set.tvector().begin(), set.tvector().end());
		set.sort();
		SEQ_TEST(map_equals(set, uset));

		set.emplace(v.back(), v.back());
		uset.emplace(v.back(), v.back());

		
		// insert everything (half already in the set)
		std::vector<pair_type > vv;
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


		SEQ_TEST(map_equals(set, uset));

	}
	{
		//test rehash() with duplicate removal

		std::vector<pair_type > v;
		for (size_t i = 0; i < 10000; ++i)
			v.emplace_back(static_cast<double>(i), static_cast<double>(i));
		for (size_t i = 0; i < 10000; ++i)
			v.emplace_back(static_cast<double>(i), static_cast<double>(i));
		seq::random_shuffle(v.begin(), v.end());

		map_type set;
		umap_type uset;

		uset.insert(v.begin(), v.end());
		set.insert(v.begin(), v.end());
		SEQ_TEST(map_equals(set, uset));

		uset.clear();
		set.clear();

		uset.insert(v.begin(), v.end());
		set.insert(v.begin(), v.end());
		SEQ_TEST(map_equals(set, uset));

		//remove half
		for (size_t i = 0; i < v.size() / 2; ++i) {
			uset.erase(v[i].first);
			set.erase(v[i].first);
		}
		SEQ_TEST(map_equals(set, uset));
	}
	{
		//test swap/move
		map_type set, set2 = { {1.,1.},{9.,9.},{2.,2.},{8.,8.},{3.,3.},{7.,7.},{4.,4.},{6.,6.},{5.,5.},{2.,2.}, {7.,7.} };
		umap_type uset, uset2 = { {1.,1.},{9.,9.},{2.,2.},{8.,8.},{3.,3.},{7.,7.},{4.,4.},{6.,6.},{5.,5.},{2.,2.}, {7.,7.} };

		{
			//manual move
			set = std::move(set2);
			uset = std::move(uset2);
			SEQ_TEST(map_equals(set, uset));
			SEQ_TEST(map_equals(set2, uset2));
		}
		{
			set.swap(set2);
			uset.swap(uset2);
			SEQ_TEST(map_equals(set, uset));
			SEQ_TEST(map_equals(set2, uset2));
		}
		{
			std::swap(set, set2);
			std::swap(uset, uset2);
			SEQ_TEST(map_equals(set, uset));
			SEQ_TEST(map_equals(set2, uset2));
		}
	}
	{
		//test copy
		std::vector<pair_type > v;
		for (size_t i = 0; i < 10000; ++i)
			v.emplace_back(static_cast<double>(i), static_cast<double>(i));
		seq::random_shuffle(v.begin(), v.end());

		map_type set;
		umap_type uset;
		uset.insert(v.begin(), v.end());
		set.insert(v.begin(), v.end());

		{
			//test copy construct
			map_type set2 = set;
			umap_type uset2 = uset;
			SEQ_TEST(map_equals(set2, uset2));
		}
		{
			//test copy operator
			map_type set2; set2 = set;
			umap_type uset2; uset2 = uset;
			SEQ_TEST(map_equals(set2, uset2));

			//test equality
			SEQ_TEST(set == set2);
			SEQ_TEST(uset == uset2);
		}

	}


}


template<class T>
inline void test_flat_map_logic()
{
	using namespace seq;
	test_flat_map_or_multi_logic<flat_map<T,T>, std::map<T,T>, true>();
}
template<class T>
inline void test_flat_multimap_logic()
{
	using namespace seq;
	test_flat_multimap_logic < flat_multimap<T, T>, std::multimap<T, T >  > ();
}






template<class Set>
void test_heavy_set(size_t count)
{
	using key_type = typename Set::value_type;
	std::vector<key_type> keys(count);
	for (size_t i = 0; i < count; ++i)
		keys[i] = i;
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




SEQ_PROTOTYPE( int test_flat_map(int , char*[]))
{
	// Test various map.multimap functions and potential memory leak or wrong allocator propagation
	CountAlloc<double> al;
	SEQ_TEST_MODULE_RETURN(heavy_flat_set,1, test_heavy_set<seq::flat_set<size_t> >(10000));
	SEQ_TEST_MODULE_RETURN(flat_map,1,test_flat_map_logic<double>());
	SEQ_TEST_MODULE_RETURN(flat_multimap,1,test_flat_multimap_logic<double>());
	SEQ_TEST_MODULE_RETURN(flat_set,1,test_flat_set_logic<double>(al));
	SEQ_TEST(get_alloc_bytes(al) == 0);
	SEQ_TEST_MODULE_RETURN(flat_multiset,1,test_flat_multiset_logic<double>(al));
	SEQ_TEST(get_alloc_bytes(al) == 0);
	
	// Test various map.multimap functions and potential memory leak
	SEQ_TEST_MODULE_RETURN(heavy_flat_set_destroy, 1, test_heavy_set<seq::flat_set<TestDestroy<size_t>> >(10000));
	SEQ_TEST(TestDestroy<size_t>::count() == 0);
	SEQ_TEST_MODULE_RETURN(flat_map_destroy, 1, test_flat_map_logic< TestDestroy<double>>());
	SEQ_TEST(TestDestroy<double>::count() == 0);
	SEQ_TEST_MODULE_RETURN(flat_multimap_destroy, 1, test_flat_multimap_logic<TestDestroy<double>>());
	SEQ_TEST(TestDestroy<double>::count() == 0);
	SEQ_TEST_MODULE_RETURN(flat_set_destroy, 1, test_flat_set_logic<TestDestroy<double>>());
	SEQ_TEST(TestDestroy<double>::count() == 0);
	SEQ_TEST_MODULE_RETURN(flat_multiset_destroy, 1, test_flat_multiset_logic<TestDestroy<double>>());
	SEQ_TEST(TestDestroy<double>::count() == 0);

	// Test various map.multimap functions and potential memory leak with non relocatable type
	CountAlloc<TestDestroy<double, false>> al2;
	SEQ_TEST_MODULE_RETURN(heavy_flat_set_destroy_no_relocatable, 1, test_heavy_set<seq::flat_set<TestDestroy<size_t,false>> >(10000));
	SEQ_TEST(TestDestroy<size_t>::count() == 0);
	SEQ_TEST_MODULE_RETURN(flat_map_destroy_no_relocatable, 1, test_flat_map_logic< TestDestroy<double, false>>());
	SEQ_TEST(TestDestroy<double>::count() == 0);
	SEQ_TEST_MODULE_RETURN(flat_multimap_destroy_no_relocatable, 1, test_flat_multimap_logic<TestDestroy<double, false>>());
	SEQ_TEST(TestDestroy<double>::count() == 0);
	SEQ_TEST_MODULE_RETURN(flat_set_destroy_no_relocatable, 1, test_flat_set_logic<TestDestroy<double, false>>(al2));
	SEQ_TEST(TestDestroy<double>::count() == 0);
	SEQ_TEST(get_alloc_bytes(al2) == 0);
	SEQ_TEST_MODULE_RETURN(flat_multiset_destroy_no_relocatable, 1, test_flat_multiset_logic<TestDestroy<double, false>>());
	SEQ_TEST(TestDestroy<double>::count() == 0);
	return 0;
}
