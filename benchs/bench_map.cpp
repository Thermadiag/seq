/**
 * MIT License
 *
 * Copyright (c) 2026 Victor Moncada <vtr.moncada@gmail.com>
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
#include <seq/legacy/format.hpp>
#include <seq/any.hpp>
#include "gtl/btree.hpp"

#ifdef BOOST_FOUND
#include "boost/container/flat_map.hpp"
#endif

#include "art/radix_map.h"
#include "art/radix_set.h"

#include <iostream>
#include <map>
#include <algorithm>
#include <random>
#include <fstream>

#include "testing.hpp"

using namespace seq;

// For art::radix_map
namespace art
{
	// fixed size ascii string transform
	template<std::size_t MAX_SIZE>
	struct key_transform<seq::tstring, MAX_SIZE>
	{
		std::array<char, MAX_SIZE> operator()(const seq::tstring& key) const noexcept
		{
			// make sure to initialize the whole array, otherwise the suffix of
			// identical keys could be different
			std::array<char, MAX_SIZE> transformed{};
			std::memcpy(&transformed, key.c_str(), key.size() + 1);

			return transformed;
		}
	};

	template<>
	struct key_transform<double>
	{
		std::uint64_t operator()(double __val) const noexcept { return seq::byte_swap_64( seq::radix_detail::to_uint(__val)); }
	};

	template<>
	struct key_transform<std::tuple<unsigned,unsigned>>
	{
		std::uint64_t operator()(std::tuple<unsigned, unsigned> __val) const noexcept
		{
			std::uint64_t r = (std::uint64_t)std::get<0>(__val) << 32ull | std::get<1>(__val);
			return seq::byte_swap_64(r);
		}
	};
}

template<class T>
struct is_art : std::false_type{};

template<class K, class V>
struct is_art<art::radix_map<K,V>>: std::true_type{}; 

template<class T>
inline std::size_t convert_to_size_t(const T& v)
{
	return static_cast<std::size_t>(v);
}

template<class T>
inline std::size_t convert_to_size_t(const std::tuple<T, T>& v)
{
	return static_cast<std::size_t>(std::get<0>(v) + std::get<1>(v));
}

template<class Char, std::size_t S, class Al>
inline std::size_t convert_to_size_t(const tiny_string<Char, Al, S>& v)
{
	return static_cast<std::size_t>(v.size());
}
inline std::size_t convert_to_size_t(const std::string& v)
{
	return static_cast<std::size_t>(v.size());
}
inline std::size_t convert_to_size_t(const std::wstring& v)
{
	return static_cast<std::size_t>(v.size());
}
template<class Interface, std::size_t S, std::size_t A, bool R>
inline std::size_t convert_to_size_t(const seq::hold_any<Interface, S, A, R>& t)
{
	return reinterpret_cast<std::size_t>(t.data());
}
template<class T, class U>
inline std::size_t convert_to_size_t(const std::pair<T, U>& v)
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
	std::size_t dist = std::distance(set.begin(), set.end());
	SEQ_TEST(dist == set.size());
	SEQ_TEST(std::is_sorted(set.begin(), set.end(), less));
	SEQ_TEST(std::is_sorted(set.rbegin(), set.rend(), greater));
	for (auto it = set.begin(); it != set.end(); ++it)
		SEQ_TEST(set.find(it->first) != set.end());
}


template<class C, class U, bool Launch = true>
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
		std::size_t start_mem = get_memory_usage();
		std::size_t insert_range, insert_range_mem;
		std::size_t insert, insert_mem;

		{
			C s;
			reset_memory_usage();
			start_mem = get_memory_usage();

			// insert range
			tick();
			s.insert(success.begin(), success.end());
			insert_range = tock_ms();
			insert_range_mem = (get_memory_usage() - start_mem) / (1024 * 1024);

			// std::cout << name << " " << insert_range << " ms" << std::endl;
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
			for (std::size_t i = 0; i < success.size(); ++i) {
				SEQ_TEST(insert_value(set, success[i].first, success[i].second));
			}

			insert = tock_ms();
			insert_mem = (get_memory_usage() - start_mem) / (1024 * 1024);

			check_sorted(set);
		}

		// insert fail
		tick();
		for (std::size_t i = 0; i < success.size(); ++i)
			SEQ_TEST(!insert_value(set, success[i].first, success[i].second));
		std::size_t insert_fail = tock_ms();
		// std::size_t insert_fail_mem = (get_memory_usage() - start_mem) / (1024 * 1024);

		check_sorted(set);

		// find success
		tick();
		std::size_t sum = 0;
		for (std::size_t i = 0; i < success.size(); ++i)
			SEQ_TEST(find_val(set, success[i].first));
		std::size_t find = tock_ms();

		// lower_bound success
		tick();
		sum = 0;
		for (std::size_t i = 0; i < success.size(); ++i) 
			//SEQ_TEST(set.lower_bound(success[i].first) != set.end());
			//TEST: lower_bound on failed
			sum += (set.lower_bound(fail[i].first) != set.begin());
		std::size_t lower_bound = tock_ms();
		print_null(sum);

		// find fail
		tick();
		sum = 0;
		for (std::size_t i = 0; i < fail.size(); ++i)
			SEQ_TEST(!find_val(set, fail[i].first));
		// SEQ_TEST(set.lower_bound(fail[i]) != typename C::const_iterator());
		std::size_t find_fail = tock_ms();

		// walk
		tick();
		sum = 0;
		for (auto it = set.begin(); it != set.end(); ++it)
			sum += convert_to_size_t(*it);
		std::size_t iterate = tock_ms();
		print_null(sum);

		std::size_t erase = 0;

#ifndef TEST_BOOST_INSERT_ERASE
		if constexpr(is_boost_map<C>::value) {
			erase = 1000000;
		}
		else
#endif
		{
			erase = 0;
			// std::cout << "erase: " << success.size() / 2 << " elems" << std::endl;
			tick();
			using const_iterator = typename C::const_iterator;
			for (std::size_t i = 0; i < success.size() / 2; ++i) {
				auto it = set.find(success[i].first);
				SEQ_TEST(it != set.end());
				if (it != set.end()){ 
					if constexpr(!is_art<C>::value)
						set.erase(it);
					else
						set.erase(it->first);
				}	
			}

			erase = tock_ms();
			print_null(set.size());
			SEQ_TEST(set.size() == (success.size() / 2 + success.size() % 2));
			check_sorted(set);

			for (std::size_t i = 0; i < success.size() / 2; ++i)
				SEQ_TEST(set.find(success[i].first) == set.end());
			for (std::size_t i = success.size() / 2; i < success.size(); ++i)
				SEQ_TEST(set.find(success[i].first) != set.end());

			// reinsert
			for (std::size_t i = 0; i < success.size() / 2; ++i)
				SEQ_TEST(set.emplace(success[i].first, success[i].second).second);
			check_sorted(set);
			for (std::size_t i = 0; i < success.size(); ++i)
				SEQ_TEST(set.find(success[i].first) != set.end());
		}

		// clear
		//TEST: comment
		/*
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
		*/
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
void test_map(std::size_t count, Gen gen, Extract e = Extract())
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
	std::shuffle(vec.begin(), vec.end(), std::mt19937(1));
	//std::reverse(vec.begin(), vec.end());

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
	std::cout << header("Set name", "Insert(range)", "Insert", "Insert(failed)", "Find (success)", "LB (not equal)", "Find (failed)", "Iterate", "Erase") << std::endl;
	std::cout << header(fill('-'), fill('-'), fill('-'), fill('-'), fill('-'), fill('-'), fill('-'), fill('-'), fill('-')) << std::endl;

	// Warmup
	// test_set<flat_set<T>>("seq::flat_set", vec, f, false);

	test_set<flat_map<T, T>>("seq::flat_map", vec, f);
	test_set<gtl::btree_map<T, T>>("phmap::btree_map", vec, f);

	// TEST
#ifdef BOOST_FOUND
	test_set<boost::container::flat_map<T, T>>("boost::flat_map", vec, f);
#endif

	if constexpr (!is_hold_any<T>::value) {

		test_set<radix_map<T, T, Extract>>("seq::radix_map", vec, f);

		// https://github.com/philipbecker/cpp-art/tree/master
		test_set<art::radix_map<T, T>>("art::radix_map", vec, f);
	}

	test_set<std::map<T, T>>("std::map", vec, f);
}

/*
template<class Map, class Format>
void test_small_map_repeat(const char * name, int count, int repeat, Format f)
{
	using value_type = typename Map::value_type;
	using key_type = typename Map::key_type;
	using mapped_type = typename Map::mapped_type;
	std::vector<std::pair<key_type, mapped_type>> vec;
	for (int i = 0; i < count; ++i)
		vec.push_back({ (key_type)i, (mapped_type)i });
	seq::random_shuffle(vec.begin(), vec.end());

	tick();
	for (int i = 0; i < repeat; ++i)
	{
		Map m ;
		print_null(m.size());
		for (std::size_t j = 0; j < vec.size() / 2; ++j)
			m.insert(vec[j]);
		m.insert(vec.begin() + vec.size() / 2, vec.end());

		std::size_t sum = 0;
		for (std::size_t j = 0; j < vec.size(); ++j)
			sum += find_val(m, vec[j].first);

		print_null(sum);

		m.erase(m.begin(), std::next(m.begin() , m.size() / 2));

		for (std::size_t j = 0; j < vec.size(); ++j)
			m.erase(vec[j].first);

		print_null(m.size());
	}
	std::size_t el = tock_ms();
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
		fmt<std::size_t>().c(20), "|"); //time

	std::cout << fmt(fmt("Set name").l(30), "|", fmt("Tims (ms)").c(20), "|") << std::endl;
	std::cout << fmt(str().c(30).f('-'), "|", str().c(20).f('-'), "|") << std::endl;


	test_small_map_repeat<gtl::btree_map<T,T>>("gtl::btree_map", count, repeat, f);
	test_small_map_repeat<flat_map<T, T>>("seq::flat_map", count, repeat, f);
	test_small_map_repeat<boost::container::flat_map<T,T> >("boost::flat_map<T>", count, repeat, f);
	test_small_map_repeat<std::map<T, T>>("std::map", count, repeat, f);
}
*/

template<class String>
std::vector<String> load_dict(const char* filename)
{
	std::ifstream fin(filename);
	if (!fin)
		return {};
	std::vector<String> ret;
	while (true) {
		String r;
		fin >> r;
		if (fin)
			ret.push_back(std::move(r));
		else
			break;
	}
	return ret;
}

int bench_map(int, char** const)
{
	/* {
		std::vector<std::string> vec;
		for (std::size_t i = 0; i < 10000; ++i)
			vec.push_back(generate_random_string<std::string>(13, true));

		std::sort(vec.begin(), vec.end());
		auto it = std::unique(vec.begin(), vec.end());
		vec.resize(it - vec.begin());
		auto b = vec.back();
		vec.pop_back();
		std::shuffle(vec.begin(), vec.end(), std::mt19937(1));

		std::vector<std::string> success(vec.begin(), vec.begin() + vec.size() / 2);
		std::vector<std::string> fail(vec.begin() + vec.size() / 2, vec.end());
		fail.push_back(b);

		seq::radix_set<std::string> s1;
		art::radix_set<std::string> s2;
		s1.insert(success.begin(), success.end());
		s2.insert(success.begin(), success.end());

		for (auto& v : fail) {
			auto it = s2.lower_bound(v);
			auto tmp = s1.lower_bound(v);
			if (tmp == s1.end())
				bool stop = true;
			if (it == s2.end()) {
				
				auto& last = *(--s2.end());
				bool is_less = last < v;
				auto it1 = s1.lower_bound(v);
				bool ok = it1 == s1.end();
				if (!ok) {
					SEQ_TEST(v < *it1);
					if (it1 != s1.begin())
						SEQ_TEST(*(--it1) < v);
				}
				bool stop = true;
			}
		}
	}*/
	
	using string = seq::tstring;

	/* {
		auto vec = load_dict<string>("C:\\Users\\VM213788\\Documents\\src\\seq\\benchs\\words.txt");
		//seq::random_shuffle(vec.begin(), vec.end(), 1);
		test_map<string>(vec.size(), [&](std::size_t i) { return vec[i]; });
	}*/

	// test random tuple
	{
		std::random_device dev;
		std::mt19937 rngi(dev());
		std::uniform_int_distribution<unsigned> dist;
		test_map<std::tuple<unsigned, unsigned>>(2000000, [&](std::size_t i) { return std::make_tuple(dist(rngi), dist(rngi)); });
	}
	
	// test random integers
	{
		std::random_device dev;
		std::mt19937 rngi(dev());
		std::uniform_int_distribution<std::size_t> dist;
		test_map<std::uint64_t>(2000000, [&](std::size_t i) { return dist(rngi); });
	}

	// test random floating point values
	{
		// std::random_device rd;
		std::mt19937 e2(0);
		std::uniform_real_distribution<> dist;
		test_map<double>(2000000, [&](std::size_t i) { return dist(e2); });
	}

	// Random short strings
	test_map<string>(1000000, [](std::size_t i) { return generate_random_string<string>(13, true); });

	// random mix of short and long strings
	test_map<string>(1000000, [](std::size_t i) { return generate_random_string<string>(63, false); });
	// Test with seq::r_any
	test_map<seq::r_any>(2000000, [](std::size_t i) {
		std::size_t idx = i & 3U;
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
