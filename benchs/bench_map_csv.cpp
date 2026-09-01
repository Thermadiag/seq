
#include <seq/flat_map.hpp>
#include <seq/radix_map.hpp>
#include <seq/legacy/format.hpp>
#include <seq/any.hpp>
#include <seq/timer.hpp>
#include <seq/algorithm.hpp>
#include "gtl/btree.hpp"

#ifdef BOOST_FOUND
#include "boost/container/flat_set.hpp"
#endif

#include <iostream>
#include <set>
#include <algorithm>
#include <fstream>
#include <random>

#include "testing.hpp"

using namespace seq;

template<class C>
struct is_boost_set : std::false_type
{
};
#ifdef BOOST_FOUND
template<class K>
struct is_boost_set<boost::container::flat_set<K>> : std::true_type
{
};
#endif

using measure_type = std::vector<std::pair<std::size_t, std::size_t>>;

#define STEP 65536

template<class Set, class T>
measure_type bench_insert(Set& s, const std::vector<T>& vec)
{
	
	timer t;

	std::vector<std::pair<std::size_t, std::size_t>> elapsed;
	elapsed.reserve(vec.size() / STEP + 2);

	if (is_boost_set<Set>::value) {
		for (std::size_t i = 0; i < vec.size(); ++i) {
			if (((i & (STEP-1)) == 0 && i)|| i == 1000)  {
				elapsed.push_back({ i, 0 });
			}
		}
		return elapsed;
	}

	t.tick();

	for (std::size_t i = 0; i < vec.size(); ++i) {
		s.insert(vec[i]);
		if (((i & (STEP-1)) == 0 && i)|| i == 1000)  {
			auto el = t.tock();
			elapsed.push_back({ i, el / (std::size_t)(i == 1000 ? 1000 : STEP) });
			t.tick();
		}
	}
	return elapsed;
}

template<class Set, class T>
measure_type bench_erase(Set& s, const std::vector<T>& vec)
{
	timer t;

	std::vector<std::pair<std::size_t, std::size_t>> elapsed;
	elapsed.reserve(vec.size() / STEP + 2);

	if (is_boost_set<Set>::value) {
		for (std::size_t i = 0; i < vec.size(); ++i) {
			if (((i & (STEP-1)) == 0 && i)|| i == 1000)  {
				elapsed.push_back({ i, 0 });
			}
		}
		return elapsed;
	}

	std::vector<std::size_t> idx(vec.size());
	for (std::size_t i = 0; i < vec.size(); ++i) {
		s.insert(vec[i]);
		idx.push_back(i);
	}
	std::shuffle(idx.begin(), idx.end(), std::random_device{});

	t.tick();

	for (std::size_t i = 0; i < vec.size(); ++i) {
		s.erase(vec[idx[i]]);
		if (((i & (STEP-1)) == 0 && i)|| i == 1000)  {
			auto el = t.tock();
			elapsed.push_back({ vec.size() - i, el / (std::size_t)(i == 1000 ? 1000 : STEP) });
			t.tick();
		}
	}
	std::sort(elapsed.begin(), elapsed.end());
	return elapsed;
}

template<class Set, class T>
measure_type bench_find_success(Set& s, const std::vector<T>& vec)
{
	std::vector<std::pair<std::size_t, std::size_t>> elapsed;
	elapsed.reserve(vec.size() / STEP + 2);
	std::vector<std::size_t> idx;
	idx.reserve(vec.size());

	for (std::size_t i = 0; i < vec.size(); ++i) {
		s.insert(vec[i]);
		idx.push_back(i);
		if (((i & (STEP-1)) == 0 && i)|| i == 1000)  {

			auto tmp = idx;
			std::shuffle(tmp.begin(), tmp.end(), std::random_device{});
			timer t;
			t.tick();

			// perform look up of all inserted values
			for (auto id : tmp)
				SEQ_TEST(s.count(vec[id]) == 1);

			auto el = t.tock();
			elapsed.push_back({ i, el / tmp.size() });
		}
	}
	return elapsed;
}

template<class Set, class T>
measure_type bench_find_failed(Set& s, const std::vector<T>& vec, const std::vector<T>& failed)
{
	std::vector<std::pair<std::size_t, std::size_t>> elapsed;
	elapsed.reserve(vec.size() / STEP + 2);

	for (std::size_t i = 0; i < vec.size(); ++i) {
		s.insert(vec[i]);
		if (((i & (STEP-1)) == 0 && i)|| i == 1000)  {

			timer t;
			t.tick();

			// perform look up of all inserted values
			for (const auto& v : failed) {
				SEQ_TEST(s.count(v) == 0);
			}

			auto el = t.tock();
			elapsed.push_back({ i, el / failed.size() });
		}
	}
	return elapsed;
}

static void print_measure(const measure_type& m)
{
	for (std::size_t i = 0; i < m.size(); ++i)
		std::cout << m[i].first << " " << m[i].second << std::endl;
}

struct SetResults
{
	std::string name;
	measure_type insert;
	measure_type erase;
	measure_type find_success;
	measure_type find_fail;
};

bool to_csv(const std::vector < SetResults>& res, const char* filename)
{
	std::ofstream fout(filename);
	if (!fout)
		return false;
	fout << "\"sep=\t\"" << std::endl;
	
    fout << "\t";
	for (std::size_t i = 0; i < res.size(); ++i) {
		auto n = res[i].name;
		fout << n + "\t\t\t\t";
	}
	fout << std::endl;

	fout << "count\t";
	for (std::size_t i = 0; i < res.size(); ++i) {
		auto n = res[i].name;
		fout << "insert\t" <<"erase\t" <<"find\t" << "fail\t" ;
	}
	fout << std::endl;

	std::size_t rows = res[0].insert.size();
	for (std::size_t j = 0; j < rows; ++j) {
	
		fout << res[0].insert[j].first << "\t";
		for (std::size_t i = 0; i < res.size(); ++i) {
			auto & r = res[i];
			fout << r.insert[j].second << "\t" << r.erase[j].second << "\t" << r.find_success[j].second << "\t" << r.find_fail[j].second << "\t" ;
		}
		fout << std::endl;
	
	}
	return true;
}

template<class Set, class T>
SetResults bench_set(const char *name, const std::vector<T>& keys, const std::vector<T>& failed)
{
    std::cout <<"bench "<<name<<std::endl;
	SetResults r;
	r.name = name;
	{
		Set s;
		r.insert = bench_insert(s, keys);
	}
	{
		Set s;
		r.erase = bench_erase(s, keys);
	}
	{
		Set s;
		r.find_success = bench_find_success(s, keys);
	}
	{
		Set s;
		r.find_fail = bench_find_failed(s, keys, failed);
	}
	return r;
}



int bench_map_csv(int, char** const)
{

	/* {
		std::vector<std::size_t> keys(1024*1024 + 5000);
		for (std::size_t i = 0; i < keys.size(); ++i)
			keys[i] = i;

		std::shuffle(keys.begin(), keys.end(), std::random_device{});
		std::vector<std::size_t> failed(keys.begin() + keys.size() - 5000, keys.end());
		keys.erase(keys.begin() + keys.size() - failed.size(), keys.end());

		std::vector<SetResults> res;

//#ifdef BOOST_FOUND
//		res.push_back(bench_set<boost::container::flat_set<std::size_t>>("boost.flat_set", keys, failed));
//#endif
		res.push_back( bench_set<flat_set<std::size_t>>("flat_set", keys, failed));
		res.push_back(  bench_set<radix_set<std::size_t>>("radix_set", keys, failed));

		res.push_back(  bench_set<gtl::btree_set<std::size_t>>("gtl::btree_set", keys, failed));
		res.push_back(  bench_set<std::set<std::size_t>>("std::set" ,keys, failed));

		to_csv(res, "bench_map_size_t.csv");
	}*/

	{
		std::vector<tstring> keys(1024 * 1024 * 4 + 5000);
		for (std::size_t i = 0; i < keys.size(); ++i)
			keys[i] = generate_random_string<tstring>(63,true);
		auto it = seq::unique(keys.begin(), keys.end());
		keys.resize(it - keys.begin());

		std::shuffle(keys.begin(), keys.end(), std::random_device{});
		std::vector<tstring> failed(keys.begin() + keys.size() - 5000, keys.end());
		keys.erase(keys.begin() + keys.size() - failed.size(), keys.end());

		std::vector<SetResults> res;

		// #ifdef BOOST_FOUND
		//		res.push_back(bench_set<boost::container::flat_set<std::size_t>>("boost.flat_set", keys, failed));
		// #endif
		res.push_back(bench_set<flat_set<tstring>>("flat_set", keys, failed));
		res.push_back(bench_set<radix_set<tstring>>("radix_set", keys, failed));

		res.push_back(bench_set<gtl::btree_set<tstring>>("gtl::btree_set", keys, failed));
		res.push_back(bench_set<std::set<tstring>>("std::set", keys, failed));

		to_csv(res, "bench_map_short_tstring.csv");
	}

	return 0;
}
