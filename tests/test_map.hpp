

#include <map>
#include <unordered_map>
#include <set>

#include "flat_map.hpp"
#include <iostream>

using namespace seq;

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

template<class Container, class U>
struct rebind
{
};
template<class T, class U>
struct rebind<flat_set<T> ,U>
{
	using type = flat_set<U>;
};
template<class T, class U>
struct rebind<flat_multiset<T>, U>
{
	using type = flat_multiset<U>;
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




template<class set_type, class std_set_type, bool Unique>
inline void test_flat_set_or_multi_logic()
{
	
	{
		//test construct from initializer list
		set_type set = { 1,9,2,8,3,7,4,6,5,2, 7 };
		std_set_type uset = { 1,9,2,8,3,7,4,6,5, 2, 7 };
		SEQ_TEST_ASSERT(set_equals(set, uset));
		SEQ_TEST_ASSERT(!set.empty());
		SEQ_TEST_ASSERT(set.max_size() > 0);
	}
	{
		//construct from range
		std::vector<double> v = { 1,9,2,8,3,7,4,6,5,2, 7 };
		set_type set(v.begin(), v.end());
		std_set_type uset(v.begin(), v.end());
		SEQ_TEST_ASSERT(set_equals(set, uset));
	}
	{
		// insert/emplace
		std::vector<double> v;
		for (size_t i = 0; i < 10000; ++i)
			v.push_back((double)i);
		std::random_shuffle(v.begin(), v.end());

		set_type set;
		std_set_type uset;
		for (size_t i = 0; i < v.size() / 2; ++i) {
			uset.insert(v[i]);
			if ((i & 1) == 0)
				set.insert(v[i]);
			else
				set.emplace(v[i]);
		}
		SEQ_TEST_ASSERT(set_equals(set, uset));

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

		SEQ_TEST_ASSERT(set_equals(set, uset));
		if (Unique)
		{
			SEQ_TEST_ASSERT(set.count(v[0]) == 1);
			SEQ_TEST_ASSERT(set.count(v[v.size() - 2]) == 0);
			SEQ_TEST_ASSERT(set.contains(v[0]));
			SEQ_TEST_ASSERT(!set.contains(v[v.size() - 2]));
		}

		// insert everything (half already in the set)
		set.insert(v.begin(), v.end());
		uset.insert(v.begin(), v.end());

		SEQ_TEST_ASSERT(set_equals(set, uset));

		//erase
		{
			auto it = set.find(v[0]);
			set.erase(it); //erase iterator
			set.erase(v[1]); //erase key

			auto uit = uset.find(v[0]);
			uset.erase(uit); //erase iterator
			uset.erase(v[1]); //erase key
		}

		SEQ_TEST_ASSERT(set_equals(set, uset));

		//test push front
		for (int i = -1; i > -10000; --i)
		{
			set.emplace(i);
			uset.emplace(i);
		}
		SEQ_TEST_ASSERT(set_equals(set, uset));

		//test push back
		for(int i=10000; i < 20000; ++i)
		{
			set.emplace(i);
			uset.emplace(i);
		}
		SEQ_TEST_ASSERT(set_equals(set, uset));
	}
	
	{
		//test swap/move
		set_type set, set2 = { 1,9,2,8,3,7,4,6,5,2, 7 };
		std_set_type uset, uset2 = { 1,9,2,8,3,7,4,6,5, 2, 7 };

		{
			//manual move
			set = std::move(set2);
			uset = std::move(uset2);
			SEQ_TEST_ASSERT(set_equals(set, uset));
			SEQ_TEST_ASSERT(set_equals(set2, uset2));
		}
		{
			set.swap(set2);
			uset.swap(uset2);
			SEQ_TEST_ASSERT(set_equals(set, uset));
			SEQ_TEST_ASSERT(set_equals(set2, uset2));
		}
		{
			std::swap(set, set2);
			std::swap(uset, uset2);
			SEQ_TEST_ASSERT(set_equals(set, uset));
			SEQ_TEST_ASSERT(set_equals(set2, uset2));
		}
	}
	{
		//test copy
		std::vector<double> v;
		for (size_t i = 0; i < 10000; ++i)
			v.push_back((double)i);
		std::random_shuffle(v.begin(), v.end());

		set_type set;
		std_set_type uset;
		uset.insert(v.begin(), v.end());
		set.insert(v.begin(), v.end());

		{
			//test copy construct
			set_type set2 = set;
			std_set_type uset2 = uset;
			SEQ_TEST_ASSERT(set_equals(set2, uset2));
		}
		{
			//test copy operator
			set_type set2; set2 = set;
			std_set_type uset2; uset2 = uset;
			SEQ_TEST_ASSERT(set_equals(set2, uset2));

			//test equality
			SEQ_TEST_ASSERT(set == set2);
			SEQ_TEST_ASSERT(uset == uset2);
		}

		uset.insert(v.begin(), v.end());
		set.insert(v.begin(), v.end());

		//randomly shuffle thinks
		std::random_shuffle(set.tvector().begin(), set.tvector().end());
		set.sort();
		SEQ_TEST_ASSERT(set_equals(set, uset));
	}

	{
		//test with non POD type
		std::vector<std::string> v;
		for (int i = 0; i < 10000; ++i)
			v.push_back(generate_random_string<std::string>(32));
		std::random_shuffle(v.begin(), v.end());

		typename rebind<set_type,std::string>::type set;
		typename rebind<std_set_type, std::string>::type uset;
		uset.insert(v.begin(), v.end());
		set.insert(v.begin(), v.end());
		SEQ_TEST_ASSERT(set_equals(set, uset));

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
			//SEQ_TEST_ASSERT(set_equals(set, uset));
		}
		SEQ_TEST_ASSERT(set_equals(set, uset));

		//reinsert all (half already exists)
		uset.insert(v.begin(), v.end());
		set.insert(v.begin(), v.end());
		SEQ_TEST_ASSERT(set_equals(set, uset));

		set.clear();
		uset.clear();
		SEQ_TEST_ASSERT(set_equals(set, uset));
	}
}


inline void test_flat_set_logic()
{
	test_flat_set_or_multi_logic<flat_set<double>, std::set<double>, true>();
}
inline void test_flat_multiset_logic()
{
	test_flat_set_or_multi_logic<flat_multiset<double>, std::multiset<double>, false>();
}




template<class map_type, class umap_type, bool Unique>
inline void test_flat_map_or_multi_logic()
{
	{
		//test construct from initializer list
		map_type set = { {1,1},{9,9},{2,2},{8,8},{3,3},{7,7},{4,4},{6,6},{5,5},{2,2}, {7,7} };
		umap_type uset = { {1,1},{9,9},{2,2},{8,8},{3,3},{7,7},{4,4},{6,6},{5,5},{2,2}, {7,7} };
		SEQ_TEST_ASSERT(map_equals(set, uset));
		SEQ_TEST_ASSERT(!set.empty());
		SEQ_TEST_ASSERT(set.max_size() > 0);
	}
	{
		//construct from range
		std::vector<std::pair<double, double> > v = { {1,1},{9,9},{2,2},{8,8},{3,3},{7,7},{4,4},{6,6},{5,5},{2,2}, {7,7} };
		map_type set(v.begin(), v.end());
		umap_type uset(v.begin(), v.end());
		SEQ_TEST_ASSERT(map_equals(set, uset));
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

		set.emplace(std::pair<double, double>(v[0], v[0]));
		uset.emplace(std::pair<double, double>(v[0], v[0]));

		set.emplace_hint(set.begin(), v[0], v[0]);
		uset.emplace_hint(uset.begin(), v[0], v[0]);

		set.insert(std::pair<double, double>(v[0], v[0]));
		uset.insert(std::pair<double, double>(v[0], v[0]));

		set.insert(set.begin(), std::pair<double, double>(v[0], v[0]));
		uset.insert(uset.begin(), std::pair<double, double>(v[0], v[0]));

		
		set.insert_or_assign(v[0], v[0]);
		set.insert_or_assign_pos(v[0], v[0]);
		set.emplace_hint(set.begin(), std::pair<double, double>(v[0], v[0]));

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

		SEQ_TEST_ASSERT(map_equals(set, uset));

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


		SEQ_TEST_ASSERT(map_equals(set, uset));

		//randomly shuffle thinks
		std::random_shuffle(set.tvector().begin(), set.tvector().end());
		set.sort();
		SEQ_TEST_ASSERT(map_equals(set, uset));

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


		SEQ_TEST_ASSERT(map_equals(set, uset));
		
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
		SEQ_TEST_ASSERT(map_equals(set, uset));

		uset.clear();
		set.clear();

		uset.insert(v.begin(), v.end());
		set.insert(v.begin(), v.end());
		SEQ_TEST_ASSERT(map_equals(set, uset));

		//remove half
		for (size_t i = 0; i < v.size() / 2; ++i) {
			uset.erase(v[i].first);
			set.erase(v[i].first);
		}
		SEQ_TEST_ASSERT(map_equals(set, uset));
	}
	{
		//test swap/move
		map_type set, set2 = { {1,1},{9,9},{2,2},{8,8},{3,3},{7,7},{4,4},{6,6},{5,5},{2,2}, {7,7} };
		umap_type uset, uset2 = { {1,1},{9,9},{2,2},{8,8},{3,3},{7,7},{4,4},{6,6},{5,5},{2,2}, {7,7} };

		{
			//manual move
			set = std::move(set2);
			uset = std::move(uset2);
			SEQ_TEST_ASSERT(map_equals(set, uset));
			SEQ_TEST_ASSERT(map_equals(set2, uset2));
		}
		{
			set.swap(set2);
			uset.swap(uset2);
			SEQ_TEST_ASSERT(map_equals(set, uset));
			SEQ_TEST_ASSERT(map_equals(set2, uset2));
		}
		{
			std::swap(set, set2);
			std::swap(uset, uset2);
			SEQ_TEST_ASSERT(map_equals(set, uset));
			SEQ_TEST_ASSERT(map_equals(set2, uset2));
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
			SEQ_TEST_ASSERT(map_equals(set2, uset2));
		}
		{
			//test copy operator
			map_type set2; set2 = set;
			umap_type uset2; uset2 = uset;
			SEQ_TEST_ASSERT(map_equals(set2, uset2));

			//test equality
			SEQ_TEST_ASSERT(set == set2);
			SEQ_TEST_ASSERT(uset == uset2);
		}

	}

	
}







template<class map_type, class umap_type>
inline void test_flat_multimap_logic()
{
	{
		//test construct from initializer list
		map_type set = { {1,1},{9,9},{2,2},{8,8},{3,3},{7,7},{4,4},{6,6},{5,5},{2,2}, {7,7} };
		umap_type uset = { {1,1},{9,9},{2,2},{8,8},{3,3},{7,7},{4,4},{6,6},{5,5},{2,2}, {7,7} };
		SEQ_TEST_ASSERT(map_equals(set, uset));
		SEQ_TEST_ASSERT(!set.empty());
		SEQ_TEST_ASSERT(set.max_size() > 0);
	}
	{
		//construct from range
		std::vector<std::pair<double, double> > v = { {1,1},{9,9},{2,2},{8,8},{3,3},{7,7},{4,4},{6,6},{5,5},{2,2}, {7,7} };
		map_type set(v.begin(), v.end());
		umap_type uset(v.begin(), v.end());
		SEQ_TEST_ASSERT(map_equals(set, uset));
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

		set.emplace(std::pair<double, double>(v[0], v[0]));
		uset.emplace(std::pair<double, double>(v[0], v[0]));

		set.emplace_hint(set.begin(), v[0], v[0]);
		uset.emplace_hint(uset.begin(), v[0], v[0]);

		set.insert(std::pair<double, double>(v[0], v[0]));
		uset.insert(std::pair<double, double>(v[0], v[0]));

		set.insert(set.begin(), std::pair<double, double>(v[0], v[0]));
		uset.insert(uset.begin(), std::pair<double, double>(v[0], v[0]));

		SEQ_TEST_ASSERT(map_equals(set, uset));

		//randomly shuffle thinks
		std::random_shuffle(set.tvector().begin(), set.tvector().end());
		set.sort();
		SEQ_TEST_ASSERT(map_equals(set, uset));

		set.emplace(v.back(), v.back());
		uset.emplace(v.back(), v.back());

		
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


		SEQ_TEST_ASSERT(map_equals(set, uset));

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
		SEQ_TEST_ASSERT(map_equals(set, uset));

		uset.clear();
		set.clear();

		uset.insert(v.begin(), v.end());
		set.insert(v.begin(), v.end());
		SEQ_TEST_ASSERT(map_equals(set, uset));

		//remove half
		for (size_t i = 0; i < v.size() / 2; ++i) {
			uset.erase(v[i].first);
			set.erase(v[i].first);
		}
		SEQ_TEST_ASSERT(map_equals(set, uset));
	}
	{
		//test swap/move
		map_type set, set2 = { {1,1},{9,9},{2,2},{8,8},{3,3},{7,7},{4,4},{6,6},{5,5},{2,2}, {7,7} };
		umap_type uset, uset2 = { {1,1},{9,9},{2,2},{8,8},{3,3},{7,7},{4,4},{6,6},{5,5},{2,2}, {7,7} };

		{
			//manual move
			set = std::move(set2);
			uset = std::move(uset2);
			SEQ_TEST_ASSERT(map_equals(set, uset));
			SEQ_TEST_ASSERT(map_equals(set2, uset2));
		}
		{
			set.swap(set2);
			uset.swap(uset2);
			SEQ_TEST_ASSERT(map_equals(set, uset));
			SEQ_TEST_ASSERT(map_equals(set2, uset2));
		}
		{
			std::swap(set, set2);
			std::swap(uset, uset2);
			SEQ_TEST_ASSERT(map_equals(set, uset));
			SEQ_TEST_ASSERT(map_equals(set2, uset2));
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
			SEQ_TEST_ASSERT(map_equals(set2, uset2));
		}
		{
			//test copy operator
			map_type set2; set2 = set;
			umap_type uset2; uset2 = uset;
			SEQ_TEST_ASSERT(map_equals(set2, uset2));

			//test equality
			SEQ_TEST_ASSERT(set == set2);
			SEQ_TEST_ASSERT(uset == uset2);
		}

	}


}



inline void test_flat_map_logic()
{
	test_flat_map_or_multi_logic<flat_map<double,double>, std::map<double,double>, true>();
}
inline void test_flat_multimap_logic()
{
	test_flat_multimap_logic < flat_multimap<double, double>, std::multimap<double, double >  > ();
}



