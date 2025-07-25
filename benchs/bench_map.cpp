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

#define SEQ_FORMAT_USE_STD_TO_CHARS

#include <seq/flat_map.hpp>
#include <seq/radix_map.hpp>
#include <seq/format.hpp>
#include <seq/testing.hpp>
#include <seq/any.hpp>
#include <seq/sequence.hpp>
#define SEQ_FORMAT_USE_STD_TO_CHARS
#ifdef SEQ_HAS_CPP_17
#include "gtl/btree.hpp"
#endif

#ifdef BOOST_FOUND
#include "boost/container/flat_set.hpp"
#endif

#include <iostream>
#include <set>
#include <map>
#include <fstream>
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
			for (size_t i = 0; i < success.size() / 2; ++i) {
				auto it = set.find(success[i].first);
				SEQ_TEST(it != set.end());
				if (it != set.end())
					set.erase(it);
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
			auto beg = set.begin();
			int count = 0;
			while (beg != set.end()) {
				++count;
				beg = set.erase(beg);
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
#ifdef SEQ_HAS_CPP_17
	test_set<gtl::btree_map<T, T>>("phmap::btree_map", vec, f);
#endif
	// TEST
	// #ifdef BOOST_FOUND
	//	test_set<boost::container::flat_set<T> >("boost::flat_set<T>", vec,f);
	// #endif
	test_set<radix_map<T, T, Extract>>("seq::radix_map", vec, f);
	test_set<std::map<T, T>>("std::map", vec, f);
	// test_set<std::unordered_set<T> >("std::unordered_set", vec,f);
}

#include <unordered_map>
#include <vector>
#include <random>

#include "ska_sort.hpp"
#include "timsort.hpp"
#include <boost/sort/spreadsort/spreadsort.hpp>
#include <boost/sort/spinsort/spinsort.hpp>
#include <boost/sort/flat_stable_sort/flat_stable_sort.hpp>

template<class T>
void indisort(T start, T end, size_t size)
{
	using value_type = typename std::iterator_traits<T>::value_type;

	std::vector<uintptr_t> buffer(size * 3u);
	uintptr_t* sort_array = buffer.data();
	uintptr_t* indexes = buffer.data() + size;
	value_type** ptrs = (value_type**)(buffer.data() + size * 2);
	size_t index = 0;
	for (auto it = start; it != end; ++it, ++index) {

		sort_array[index] = (index);
		indexes[index] = (index);
		ptrs[index] = (&(*it));
	}
	std::sort(sort_array, sort_array + size, [&ptrs](size_t l, size_t r) { return *ptrs[l] < *ptrs[r]; });

	index = 0;

	for (auto current_index = sort_array; current_index != sort_array + size; ++current_index, ++index) {

		size_t idx = indexes[*current_index];
		while (idx != indexes[idx])
			idx = indexes[idx];
		value_type tmp = std::move(*ptrs[index]);
		*ptrs[index] = std::move(*ptrs[idx]);
		*ptrs[idx] = std::move(tmp);

		indexes[index] = idx;
	}
}

template<class T, class Cmp = std::less<>>
bool sorted(const T& c, Cmp l = Cmp())
{
	return std::is_sorted(c.begin(), c.end(), l);
}
template<class T, class Extr>
bool equals(const T& c1, const T& c2, Extr extr)
{
	auto eq = [](const auto& l, const auto& r) { return Extr{}(l) == Extr{}(r); };
	return std::equal(c1.begin(), c1.end(), c2.begin(), c2.end(), eq);
}

#define BENCH_SORT(vec, sorted, extr, function)                                                                                                                                                        \
	{                                                                                                                                                                                              \
		auto v = vec;                                                                                                                                                                          \
		seq::tick();                                                                                                                                                                           \
		function(v.begin(), v.end(), extr);                                                                                                                                                    \
		size_t el = seq::tock_ms();                                                                                                                                                            \
		std::cout << #function << " " << el << " " << std::equal(sorted.begin(), sorted.end(), v.begin()) << std::endl;                                                                        \
	}
#define BENCH_SORT2(vec, sorted, name, ...)                                                                                                                                                            \
	{                                                                                                                                                                                              \
		auto v = vec;                                                                                                                                                                          \
		seq::tick();                                                                                                                                                                           \
		__VA_ARGS__;                                                                                                                                                                           \
		size_t el = seq::tock_ms();                                                                                                                                                            \
		std::cout << #name << " " << el << " " << (v == sorted) << std::endl;                                                                                                                  \
	}

template<class T>
struct Iter
{
	using iterator_category = std::bidirectional_iterator_tag;
	using value_type = T;
	using pointer = T*;
	using reference = T&;
	using difference_type = std::ptrdiff_t;
	T* p;

	Iter& operator++() noexcept
	{
		++p;
		return *this;
	}
	Iter operator++(int) noexcept
	{
		Iter it = *this;
		++(*this);
		return it;
	}
	Iter& operator--() noexcept
	{
		--p;
		return *this;
	}
	Iter operator--(int) noexcept
	{
		Iter it = *this;
		--(*this);
		return it;
	}
	bool operator==(Iter o) const noexcept { return o.p == p; }
	bool operator!=(Iter o) const noexcept { return o.p != p; }
	T& operator*() const noexcept { return const_cast<T&>(*p); }
	T* operator->() const noexcept { return const_cast<T*>(p); }
};

template<class T>
void ska_sort_stable(T begin, T end)
{
	using type = typename std::iterator_traits<T>::value_type;
	using tuple_type = std::tuple<type&, size_t>;
	std::vector<char> mem((end - begin) * sizeof(size_t) * 2);
	tuple_type* data = reinterpret_cast<tuple_type*>(mem.data());
	size_t i = 0;
	for (auto it = begin; it != end; ++it, ++i)
		data[i] = std::tie(*it, i);

	ska_sort(data, data + (end - begin));
}

template<class Iter>
void wave_sort(Iter first, Iter last, size_t longest_run)
{
	std::mt19937 rng(0);
	std::uniform_int_distribution<size_t> dist(0, std::numeric_limits<size_t>::max());
	auto it = first;
	while (it != last) {
		size_t len = dist(rng) % longest_run;
		if (last - it < len)
			len = last - it;
		std::sort(it, it + len);
		it = it + len;
	}
}

#include <seq/algorithm.hpp>
#include <deque>
#include <seq/radix_hash_map.hpp>


int bench_map2(int, char** const)
{
	{
		seq::radix_set<std::string> set;
		
		set.emplace("tutu", 4);
		set.emplace("toto");
		set.emplace(std::string("tata"));
	}
	
	{
		seq::flat_set<std::tuple<int, int>> s;
		s.emplace(0, 0);
	}
	/*{
		std::mt19937 rngi(0);
		std::uniform_int_distribution<size_t> dist;
		std::vector<size_t> vec(1000000);
		for (size_t i = 0; i < vec.size(); ++i) {
			vec[i] = dist(rngi);
		}
		std::cout << "start" << std::endl;
		getchar();
		getchar();
		std::cout << "insert" << std::endl;
		seq::radix_hash_set<size_t> set;
		for (size_t i : vec)
			set.insert(i);

		std::cout << "erase" << std::endl;
		while (set.size()) {
			if (set.size() == 1)
				bool stop = true;
			set.erase(set.begin());
		}

		std::cout << "finish" << std::endl;
		vec.clear();
		vec.shrink_to_fit();
		getchar();
	}*/
	using string = tstring;
	// test random IP addresses
	/* {

		srand(0);
		test_map<string>(4000000, [](size_t i) {
			string s = join(".", (rand() & 255), (rand() & 255), (rand() & 255), (rand() & 255));
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

	// random_float_genertor<double> rng;
	// test_map<double>(4000000, [&rng](size_t i) { return rng(); });

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

int bench_map(int, char** const)
{

	{
		struct Test
		{
			unsigned i;
			// std::string t;
			char data[4];
			SEQ_ALWAYS_INLINE Test(unsigned ii = 0)
			  : i(ii)
			{
			}
			SEQ_ALWAYS_INLINE Test(const Test&) = default;
			SEQ_ALWAYS_INLINE Test(Test&&) noexcept = default;
			SEQ_ALWAYS_INLINE Test& operator=(const Test&) = default;
			SEQ_ALWAYS_INLINE Test& operator=(Test&&) noexcept = default;
			SEQ_ALWAYS_INLINE bool operator==(const Test& o) const noexcept { return i == o.i; }
			SEQ_ALWAYS_INLINE bool operator<(const Test& r) const noexcept { return i < r.i; }
		};
		// wave pattern

		std::vector<size_t> vec;
		std::random_device dev;
		std::mt19937 rng(dev());
		std::uniform_int_distribution<size_t> dist(0, std::numeric_limits<size_t>::max());
		for (size_t i = 0; i < 32000; ++i) {
			auto start = dist(rng);
			for (size_t j = 0; j < 256; ++j)
				vec.push_back(start + j);
			start = dist(rng);
			for (int j = 253; j >= 0; --j)
				vec.push_back((size_t)j + start);
		}
		// std::sort(vec.begin(), vec.begin() + vec.size() / 2, std::less<>{});
		// std::shuffle(vec.begin(), vec.end(), std::random_device());

		/* std::mt19937 rng(0);
		std::uniform_int_distribution<unsigned> dist(0, std::numeric_limits<unsigned>::max());
		//std::uniform_real_distribution<float> dist(0, std::numeric_limits<float>::max());
		std::vector<Test> vec(3200000);
		for (size_t i = 0; i < vec.size(); ++i) {
			//auto t = dist(rng);
			vec[i] = Test(dist(rng));
			//vec[i] = dist(rng) * (float)(i&1 ? 1.f : -1.f);
			//vec[i] = std::tuple<short, short>(t >> 16u, t & 65535u);
			//vec[i] = (double)t * ((i & 1) ? 1 : -1);
		}*/

		/* std::vector<unsigned> tmp(vec.size());
		vart::radix_internal(vec.begin(), vec.end(), tmp.begin(), 3);
		vart::radix_internal(tmp.begin(), tmp.end(), vec.begin(), 2);
		vart::radix_internal(vec.begin(), vec.end(), tmp.begin(), 1);
		vart::radix_internal(tmp.begin(), tmp.end(), vec.begin(), 0);
		bool ok = std::is_sorted(vec.begin(), vec.end());
		bool stop = true;*/

		// wave_sort(vec.begin(), vec.end(), vec.size()/100);
		// vec[i] = dist(rng) *((i & 1) ? 1 : -1);
		// std::nth_element(vec.begin(), vec.begin() + vec.size() / 2, vec.end(), std::less<>{});
		// std::sort(vec.begin(), vec.begin() + vec.size() , std::less<>{});
		// std::swap(vec[0], vec[100]);

		// binary
		/* std::vector<size_t> vec(10000000);
		for (size_t i = 0; i < vec.size(); ++i)
			vec[i] = size_t(i % 1000);
		std::shuffle(vec.begin(), vec.end(), std::random_device());
		*/
		// Optimize for this one
		/*std::random_device dev;
		std::mt19937 rng(dev());
		std::uniform_int_distribution<size_t> dist(0, std::numeric_limits<size_t>::max());
		std::vector<size_t> tmp(100);
		for (size_t i = 0; i < tmp.size(); ++i)
			tmp[i] = i;
		//dist(rng);
		std::vector<size_t> vec;
		for (int i = 0; i < 200000; ++i) {
			//vec.insert(vec.end(), tmp.begin(), tmp.end());
			auto begin = tmp.begin();
			auto end = tmp.begin() + (dist(rng) % tmp.size());
			if (i&1)
				vec.insert(vec.end(), begin,end);
			else vec.insert(vec.end(), std::make_reverse_iterator(end), std::make_reverse_iterator(begin));
		}*/
		// std::shuffle(vec.begin(), vec.end(), std::random_device());
		// std::sort(vec.begin(), vec.begin() + vec.size()/2, std::greater<>{});
		// std::swap(vec[0], vec[100]);

		/* std::vector<std::vector<void*>> vec;
		std::random_device dev;
		std::mt19937 rng(0);
		std::uniform_int_distribution<size_t> dist(0, std::numeric_limits<size_t>::max());
		for (int i = 0; i < 1000000; ++i) {
			std::vector<void*> tmp(dist(rng) % 8 );
			for (size_t j = 0; j < tmp.size(); ++j)
				tmp[j] = reinterpret_cast<void*>(dist(rng) );
			vec.emplace_back(std::move(tmp));
		}*/

		/* std::vector<std::string> vec;
		std::ifstream fin("C:\\src\\seq\\build\\words.txt");
		while (fin) {
			std::string line;
			fin >> line;
			vec.push_back(line);
		}
		//vec.resize(vec.size() / 10);
		std::mt19937 mt(0);
		std::shuffle(vec.begin(), vec.end(), mt);
		*/
		/* std::vector<std::string> vec(1000000);
		for (size_t i = 0; i < vec.size(); ++i)
			vec[i] = (generate_random_string<std::string>(63, false));
		*/

		// std::nth_element(vec.begin(), vec.begin() + vec.size()/2, vec.end(), std::less<>{});
		// std::sort(vec.begin(), vec.begin()+vec.size()/2, std::less<>{});
		// std::swap(vec[10], vec[10000]);

		/* std::string str;
		std::vector<std::string> vec;
		for (size_t i = 0; i < 10000; ++i) {
			vec.push_back(str);
			str += (char)i;
		}
		std::shuffle(vec.begin(), vec.end(), std::random_device());
		*/

		/* std::vector<std::string> tmp(1000);
		for (size_t i = 0; i < tmp.size(); ++i)
			tmp[i] = generate_random_string<std::string>(15, true);
		std::vector<std::string> vec;
		for (int i = 0; i < 2000; ++i) {
			vec.insert(vec.end(),tmp.begin(), tmp.end());
		}*/
		// std::shuffle(vec.begin(), vec.end(), std::random_device());

		auto inf = [](const auto& l, const auto& r) { return l < r; }; //
		auto extr = [](const auto& t) -> const auto& { return t; };

		// auto extr = [](const auto& t) [[msvc::forceinline]]->auto { return t.i; };
		// auto inf = [extr](const auto& l, const auto& r) { return extr(l) < extr(r); };

		/* auto extr = [](const auto& t) -> auto
		{
			const char* p = (const char*)&t;
			return std::string_view(p, 4);
			//return std::tuple<unsigned char, unsigned char, unsigned char, unsigned char>{ t >> 16u, t & 65535u };
		};*/
		// auto inf = [extr](const auto& l, const auto& r) { return extr(l) < extr(r); }; //

		// auto inf = [](const Test& l, const Test& r) { return l.val < r.val; };
		// auto extr = [](const Test& t) -> const std::string& { return t.val; };

		auto sorted = vec;
		std::stable_sort(sorted.begin(), sorted.end(), inf);
		using type = decltype(vec)::value_type;
		/* {
			seq::sequence<type> v(vec.begin(), vec.end());
			seq::tick();
			v.sort();
			size_t el = seq::tock_ms();
			std::cout << "list " << el << " " << std::is_sorted(v.begin(), v.end()) << std::endl;
		}
		{
			seq::sequence<type> v(vec.begin(), vec.end());
			seq::tick();
			seq::merge_sort_size(v.begin(), v.size());
			size_t el = seq::tock_ms();
			std::cout << "merge_sort_size " << el << " " << std::is_sorted(v.begin(), v.end()) << std::endl;
		}*/
		/* {
			std::list<type> v(vec.begin(), vec.end());
			seq::tick();
			v.sort();
			size_t el = seq::tock_ms();
			std::cout << "sequence " << el << " " << std::is_sorted(v.begin(), v.end()) << std::endl;
		}
		{
			std::list<type> v(vec.begin(), vec.end());
			seq::tick();
			vart::merge_sort_size(v.begin(), v.size());
			size_t el = seq::tock_ms();
			std::cout << "sequence merge_sort_size " << el << " " << std::is_sorted(v.begin(), v.end()) << std::endl;
		}*/

		BENCH_SORT(vec, sorted, inf, std::stable_sort);
		BENCH_SORT(vec, sorted, inf, std::sort);
		BENCH_SORT(vec, sorted, inf, seq::merge_sort);
		BENCH_SORT(vec, sorted, inf, seq::merge_sort_stack);

		BENCH_SORT(vec, sorted, inf, gfx::timsort);
		BENCH_SORT(vec, sorted, inf, boost::sort::spinsort);
		BENCH_SORT(vec, sorted, inf, boost::sort::flat_stable_sort);
		// BENCH_SORT2(vec, sorted, spreadsort, boost::sort::spreadsort::spreadsort(v.begin(),v.end()));
		BENCH_SORT(vec, sorted, inf, boost::sort::pdqsort);
		// BENCH_SORT2(vec, sorted, quadsort, quadsort_uint64(v.data(), v.size(),nullptr));
		// BENCH_SORT(vec, sorted, extr, vart::radix_sort);
		// BENCH_SORT(vec, sorted, extr, ska_sort);

		return 0;
	}

	/*
	using string = tstring;

	// Test list or words and uuid if available
	  {
		std::ifstream fin("words.txt");
		std::vector<string> vec;
		while (true) {
			string s;
			fin >> s;
			if (fin)vec.push_back(s);
			else
				break;
		}
		{
			std::ifstream fin("uuid.txt");
			while (true) {
				string s;
				fin >> s;
				if (fin)vec.push_back(s);
				else
					break;
			}
		}
		//TEST
		vec.clear();

		std::vector<string> vec2 = vec;
		seq::random_shuffle(vec2.begin(), vec2.end());
		vec.reserve(vec.size() * 2);
		for (size_t i = 0; i < vec2.size(); ++i)
			vec.push_back(vec[i] + " " + vec2[i]);

		seq::random_shuffle(vec.begin(), vec.end());

		std::vector<string> vec3 = vec;
		seq::random_shuffle(vec3.begin(), vec3.end());
		vec.reserve(vec.size() * 2);
		for (size_t i = 0; i < vec3.size(); ++i)
			vec.push_back(vec[i] + " " + vec3[i]);

		test_map<string>(vec.size(), [&vec](size_t i) {return vec[i]; });


	}

	  // test random IP addresses
	{

		srand(0);
		test_map<string>(4000000, [](size_t i) {
			string s= join(".", (rand() & 255), (rand() & 255), (rand() & 255), (rand() & 255));
			return s;
			 });
	}
	// test random integers
	 {
		std::random_device dev;
		std::mt19937 rngi(dev());
		std::uniform_int_distribution<size_t> dist;
		test_map<size_t>(4000000, [&](size_t i) { return dist(rngi); });
	}
	 // test random floating point values
	{
		std::random_device rd;
		std::mt19937 e2(rd());
		std::uniform_real_distribution<> dist;
		test_map<double>(4000000, [&](size_t i) { return dist(e2); });
	}

	//random_float_genertor<double> rng;
	//test_map<double>(4000000, [&rng](size_t i) { return rng(); });

	// Random short strings
	test_map<string>(4000000, [](size_t i) { return generate_random_string<string>(13, true); });
	// random mix of short and long strings
	test_map<string>(2000000, [](size_t i) { return generate_random_string<string>(63, false); });
	 // Test with seq::r_any
	test_map<seq::r_any>(4000000, [](size_t i)
		 {
			 size_t idx = i & 3U;
			 switch (idx) {
			 case 0:return seq::r_any(i * UINT64_C(0xc4ceb9fe1a85ec53));
			 case 1:return seq::r_any((double)i * (double)UINT64_C(0xc4ceb9fe1a85ec53));
			 default:return seq::r_any(generate_random_string<seq::tstring>(13, true));
			 }
		 }
	 );
	 */
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