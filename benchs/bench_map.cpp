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


#include <seq/flat_map.hpp>
#include <seq/radix_map.hpp>
#include <seq/format.hpp>
#include <seq/testing.hpp>
#include <seq/any.hpp>
#include "gtl/btree.hpp"

#ifdef BOOST_FOUND
#include "boost/container/flat_map.hpp"
#endif

#include <iostream>
#include <map>
#include <algorithm>

using namespace seq;

template<class T>
inline size_t convert_to_size_t(const T& v)
{
	return static_cast<size_t>(v);
}

template<class T>
inline size_t convert_to_size_t(const std::tuple<T, T>& v)
{
	return static_cast<size_t>(std::get<0>(v) + std::get<1>(v));
}

template<class Char, class Traits, size_t S, class Al>
inline size_t convert_to_size_t(const tiny_string<Char, Traits, Al, S>& v)
{
	return static_cast<size_t>(v.size());
}
inline size_t convert_to_size_t(const std::string& v)
{
	return static_cast<size_t>(v.size());
}
inline size_t convert_to_size_t(const std::wstring& v)
{
	return static_cast<size_t>(v.size());
}
template<class Interface, size_t S, size_t A, bool R>
inline size_t convert_to_size_t(const seq::hold_any<Interface, S, A, R>& t)
{
	return reinterpret_cast<size_t>(t.data());
}
template<class T, class U>
inline size_t convert_to_size_t(const std::pair<T, U>& v)
{
	return convert_to_size_t(v.first);
}


template<class C>
struct is_boost_map : std::false_type
{
};
#ifdef BOOST_FOUND
template<class K, class V>
struct is_boost_map<boost::container::flat_map<K, V>> : std::true_type
{
};
#endif

template<class C, class K>
static SEQ_ALWAYS_INLINE bool find_val(const C& s, const K& key)
{
	return s.find(key) != s.end();
}

template<class C, class E, class K>
static SEQ_ALWAYS_INLINE bool find_val(const seq::radix_set<C, E>& s, const K& key)
{
	return s.contains(key);
}

template<class C, class V, class E, class K>
static SEQ_ALWAYS_INLINE bool find_val(const seq::radix_map<C, V, E>& s, const K& key)
{
	return s.contains(key);
}

template<class C, class K>
static SEQ_ALWAYS_INLINE bool find_val(const flat_set<C>& s, const K& key)
{
	return s.find_pos(key) != s.size();
}

template<class C, class V, class K>
static SEQ_ALWAYS_INLINE bool find_val(const flat_map<C, V>& s, const K& key)
{
	return s.find_pos(key) != s.size();
}

template<class C, class K, class V>
struct insert_val
{
	static bool insert(C& s, const K& key, const V& val) { return s.emplace(key, val).second; }
};
template<class K, class V>
struct insert_val<flat_map<K, V>, K, V>
{
	static bool insert(flat_map<K, V>& s, const K& key, const V& val) { return s.emplace_pos(key, val).second; }
};
template<class C, class K, class V>
bool insert_value(C& s, const K& key, const V& val)
{
	return insert_val<C, K, V>::insert(s, key, val);
}

template<class C>
void check_sorted(C& set)
{
	auto less = [](const auto& a, const auto& b) { return a.first < b.first; };
	auto greater = [](const auto& a, const auto& b) { return a.first > b.first; };
	size_t dist = std::distance(set.begin(), set.end());
	SEQ_TEST(dist == set.size());
	SEQ_TEST(std::is_sorted(set.begin(), set.end(), less));
	SEQ_TEST(std::is_sorted(set.rbegin(), set.rend(), greater));
	for (auto it = set.begin(); it != set.end(); ++it)
		SEQ_TEST(set.find(it->first) != set.end());
}

template<class C>
struct is_radix_set : std::false_type
{
};

template<class T, class Extract, class Al>
struct is_radix_set<radix_set<T, Extract, Al>> : std::true_type
{
};

template<class C>
struct is_radix_map : std::false_type
{
};

template<class T, class V, class Extract, class Al>
struct is_radix_map<radix_map<T, V, Extract, Al>> : std::true_type
{
};

template<class C, class U, bool Launch = !(is_hold_any<typename U::first_type>::value && (is_radix_set<C>::value || is_radix_map<C>::value))>
struct LaunchTest
{
	template<class Format>
	static void test(const char* name, const std::vector<U>& vec, Format f, bool write = true)
	{
		// using T = typename C::value_type;
		C set;

		std::vector<U> success(vec.begin(), vec.begin() + vec.size() / 2);
		std::vector<U> fail(vec.begin() + vec.size() / 2, vec.end());
		// seq::random_shuffle(success.begin(), success.end(),1);
		// seq::random_shuffle(fail.begin(), fail.end(),1);

		reset_memory_usage();
		size_t start_mem = get_memory_usage();
		size_t insert_range, insert_range_mem;
		size_t insert, insert_mem;

		{
			C s;
			reset_memory_usage();
			start_mem = get_memory_usage();

			// insert range
			tick();
			s.insert(success.begin(), success.end());
			insert_range = tock_ms();
			insert_range_mem = (get_memory_usage() - start_mem) / (1024 * 1024);

			//std::cout << name << " " << insert_range << " ms" << std::endl;
			check_sorted(s);
		}

		// insert
#ifndef TEST_BOOST_INSERT_ERASE
		if (is_boost_map<C>::value) {
			insert = 1000000;
			insert_mem = 0;
			set.insert(success.begin(), success.end());
		}
		else
#endif
		{
			reset_memory_usage();
			start_mem = get_memory_usage();

			tick();
			for (size_t i = 0; i < success.size(); ++i) {
				SEQ_TEST(insert_value(set, success[i].first, success[i].second));
			}

			insert = tock_ms();
			insert_mem = (get_memory_usage() - start_mem) / (1024 * 1024);

			check_sorted(set);
		}

		// insert fail
		tick();
		for (size_t i = 0; i < success.size(); ++i)
			SEQ_TEST(!insert_value(set, success[i].first, success[i].second));
		size_t insert_fail = tock_ms();
		// size_t insert_fail_mem = (get_memory_usage() - start_mem) / (1024 * 1024);

		check_sorted(set);

		// find success
		tick();
		size_t sum = 0;
		for (size_t i = 0; i < success.size(); ++i)
			SEQ_TEST(find_val(set, success[i].first));
		size_t find = tock_ms();

		// lower_bound success
		tick();
		sum = 0;
		for (size_t i = 0; i < success.size(); ++i)
			SEQ_TEST(set.lower_bound(success[i].first) != set.end());
		size_t lower_bound = tock_ms();

		// find fail
		tick();
		sum = 0;
		for (size_t i = 0; i < fail.size(); ++i)
			SEQ_TEST(!find_val(set, fail[i].first));
		// SEQ_TEST(set.lower_bound(fail[i]) != typename C::const_iterator());
		size_t find_fail = tock_ms();

		// walk
		tick();
		sum = 0;
		for (auto it = set.begin(); it != set.end(); ++it)
			sum += convert_to_size_t(*it);
		size_t iterate = tock_ms();
		print_null(sum);

		size_t erase = 0;

#ifndef TEST_BOOST_INSERT_ERASE
		if (is_boost_map<C>::value) {
			erase = 1000000;
		}
		else
#endif
		{
			erase = 0;
			// std::cout << "erase: " << success.size() / 2 << " elems" << std::endl;
			tick();
			using const_iterator = typename C::const_iterator;
			for (size_t i = 0; i < success.size() / 2; ++i) {
				auto it = set.find(success[i].first);
				SEQ_TEST(it != set.end());
				if (it != set.end())
					set.erase((const_iterator)it);
			}

			erase = tock_ms();
			print_null(set.size());
			SEQ_TEST(set.size() == (success.size() / 2 + success.size() % 2));
			check_sorted(set);

			for (size_t i = 0; i < success.size() / 2; ++i)
				SEQ_TEST(set.find(success[i].first) == set.end());
			for (size_t i = success.size() / 2; i < success.size(); ++i)
				SEQ_TEST(set.find(success[i].first) != set.end());

			// reinsert
			for (size_t i = 0; i < success.size() / 2; ++i)
				SEQ_TEST(set.emplace(success[i].first, success[i].second).second);
			check_sorted(set);
			for (size_t i = 0; i < success.size(); ++i)
				SEQ_TEST(set.find(success[i].first) != set.end());
		}

		// clear
#ifndef TEST_BOOST_INSERT_ERASE
		if (!is_boost_map<C>::value)
#endif
		{
			using const_iterator = typename C::const_iterator;
			auto beg = set.begin();
			int count = 0;
			while (beg != set.end()) {
				++count;
				beg = set.erase((const_iterator)beg);
			}
			SEQ_TEST(set.size() == 0);
		}
		if (write)
			std::cout << f(name, fmt(insert_range, insert_range_mem), fmt(insert, insert_mem), insert_fail, find, lower_bound, find_fail, iterate, erase) << std::endl;
	}
};

template<class C, class T>
struct LaunchTest<C, T, false>
{
	template<class Format>
	static void test(const char* name, const std::vector<T>& vec, Format f, bool write)
	{
	}
};

template<class C, class U, class Format>
void test_set(const char* name, const std::vector<U>& vec, Format f, bool write = true)
{
	LaunchTest<C, U>::test(name, vec, f, write);
}

/// @brief Compare various operations on seq::flat_set, phmap::btree_set, boost::container::flat_set, std::set and std::unordered_set<T>
/// @tparam T
/// @tparam Gen
/// @param count
/// @param gen
template<class T, class Gen, class Extract = default_key>
void test_map(size_t count, Gen gen, Extract e = Extract())
{
	std::cout << std::endl;
	std::cout << "Test sorted containers with type = " << typeid(T).name() << " and count = " << count / 2 << std::endl;
	std::cout << std::endl;

	std::vector<std::pair<T, T>> vec(count);
	for (int i = 0; i < vec.size(); ++i) {
		vec[i] = std::make_pair((T)gen(i), T());
	}

	auto less = [](const auto& a, const auto& b) { return a.first < b.first; };
	auto equal = [](const auto& a, const auto& b) { return a.first == b.first; };

	std::sort(vec.begin(), vec.end(), less);
	auto it = std::unique(vec.begin(), vec.end(), equal);
	vec.resize(it - vec.begin());
	seq::random_shuffle(vec.begin(), vec.end(), 1);
	// std::reverse(vec.begin(), vec.end());

	std::cout << "vector size: " << vec.size() << std::endl;

	auto f = join("|",
		      _s().l(30),			     // name
		      _fmt(_u(), " ms/", _u(), " MO").c(20), // insert(range)
		      _fmt(_u(), " ms/", _u(), " MO").c(20), // insert
		      _fmt(_u(), " ms").c(15),		     // insert(fail)
		      _fmt(_u(), " ms").c(15),		     // find
		      _fmt(_u(), " ms").c(15),		     // lower_bound
		      _fmt(_u(), " ms").c(15),		     // find(fail)
		      _fmt(_u(), " ms").c(15),		     // iterate
		      _fmt(_u(), " ms").c(15),
		      ""); // erase

	auto header = join("|", _s().l(30), _s().c(20), _s().c(20), _s().c(15), _s().c(15), _s().c(15), _s().c(15), _s().c(15), _s().c(15), "");
	std::cout << header("Set name", "Insert(range)", "Insert", "Insert(failed)", "Find (success)", "LB (success)", "Find (failed)", "Iterate", "Erase") << std::endl;
	std::cout << header(fill('-'), fill('-'), fill('-'), fill('-'), fill('-'), fill('-'), fill('-'), fill('-'), fill('-')) << std::endl;

	// Warmup
	// test_set<flat_set<T>>("seq::flat_set", vec, f, false);

	test_set<flat_map<T, T>>("seq::flat_map", vec, f);
	test_set<gtl::btree_map<T, T>>("phmap::btree_map", vec, f);


	// TEST
#ifdef BOOST_FOUND
		test_set<boost::container::flat_map<T,T> >("boost::flat_map", vec,f);
#endif
	test_set<radix_map<T, T, Extract>>("seq::radix_map", vec, f);
	
	test_set<std::map<T, T>>("std::map", vec, f);
}

int bench_map(int, char** const)
{
	
	using string = tstring;
	// test random IP addresses
	/* {

		srand(0);
		test_map<string>(4000000, [](size_t i) {
			string s = join(".", ch(rand() & 255), ch(rand() & 255), ch(rand() & 255), ch(rand() & 255));
			return s;
		});
	}*/

	// test random tuple
	{
		std::random_device dev;
		std::mt19937 rngi(dev());
		std::uniform_int_distribution<unsigned> dist;
		test_map<std::tuple<unsigned, unsigned>>(2000000, [&](size_t i) { return std::make_tuple(dist(rngi), dist(rngi)); });
	}

	// test random integers
	{
		std::random_device dev;
		std::mt19937 rngi(dev());
		std::uniform_int_distribution<size_t> dist;
		test_map<size_t>(2000000, [&](size_t i) { return dist(rngi); });
	}
	// test random floating point values
	{
		// std::random_device rd;
		std::mt19937 e2(0);
		std::uniform_real_distribution<> dist;
		test_map<double>(2000000, [&](size_t i) { return dist(e2); });
	}
	
	// Random short strings
	test_map<string>(1000000, [](size_t i) { return generate_random_string<string>(13, true); });
	
	// random mix of short and long strings
	test_map<string>(1000000, [](size_t i) { return generate_random_string<string>(63, false); });
	// Test with seq::r_any
	test_map<seq::r_any>(2000000, [](size_t i) {
		size_t idx = i & 3U;
		switch (idx) {
			case 0:
				return seq::r_any(i * UINT64_C(0xc4ceb9fe1a85ec53));
			case 1:
				return seq::r_any((double)i * (double)UINT64_C(0xc4ceb9fe1a85ec53));
			default:
				return seq::r_any(generate_random_string<seq::tstring>(13, true));
		}
	});

	return 0;
}

/*template<class Map, class Format>
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
		print_null(m.size());
		for (size_t j = 0; j < vec.size() / 2; ++j)
			m.insert(vec[j]);
		m.insert(vec.begin() + vec.size() / 2, vec.end());

		size_t sum = 0;
		for (size_t j = 0; j < vec.size(); ++j)
			//sum += *(size_t)m.find(vec[j]);
			find_val<Map,value_type>::find(m,vec[j],sum);

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
}*/