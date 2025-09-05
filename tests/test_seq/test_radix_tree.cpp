/**
 * MIT License
 *
 * Copyright (c) 2025 Victor Moncada <vtr.moncada@gmail.com>
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

// Silence warnings on deprecated functions in C++17
#if (defined(_MSC_VER) && !defined(__clang__))
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#endif

#include "seq/radix_map.hpp"
#include "seq/testing.hpp"
#include "seq/tiny_string.hpp"

#include <memory>
#include <set>
#include <map>
#include <string>

#include "tests.hpp"

template<class Alloc, class U>
using RebindAlloc = typename std::allocator_traits<Alloc>::template rebind_alloc<U>;

template<class Val>
struct TestDestroyCount
{
	template<class T>
	static void test(T)
	{
	}
};
template<class Val, bool R>
struct TestDestroyCount<TestDestroy<Val, R>>
{
	template<class T>
	static void test(T count)
	{
		auto c = TestDestroy<Val, R>::count();
		if (static_cast<std::int64_t>(count) == c)
			return;
		throw std::runtime_error("");
	}
};
template<class Val, class T>
void test_destroy(T count)
{
	TestDestroyCount<Val>::test(count);
}

// #undef SEQ_TEST
// #define SEQ_TEST(...) if(!(__VA_ARGS__)) throw std::runtime_error("");

/// @brief Allocator that allows only one allocation
template<class T>
class dummy_alloc : public std::allocator<T>
{
public:
	static int& get_int()
	{
		static int c = 0;
		return c;
	}
	template<class U>
	struct rebind
	{
		using other = dummy_alloc<U>;
	};
	auto select_on_container_copy_construction() const noexcept -> dummy_alloc { return *this; }

	dummy_alloc()
	  : std::allocator<T>()
	{
	}
	dummy_alloc(const dummy_alloc& other)
	  : std::allocator<T>(other)
	{
	}
	template<class U>
	dummy_alloc(const dummy_alloc<U>& other)
	  : std::allocator<T>(other)
	{
	}
	dummy_alloc& operator=(const dummy_alloc&) = default;
	auto operator==(const dummy_alloc&) const noexcept -> bool { return true; }
	auto operator!=(const dummy_alloc&) const noexcept -> bool { return false; }
	auto allocate(size_t n, const void* /*unused*/) -> T* { return allocate(n); }
	auto allocate(size_t n) -> T*
	{
		if (get_int()++ == 0)
			return std::allocator<T>{}.allocate(n);
		throw std::runtime_error("error");
	}
	void deallocate(T* p, size_t n) { std::allocator<T>{}.deallocate(p, n); }
};

template<class T, class U>
bool set_equals(const T& s1, const U& s2)
{
	if (s1.size() != s2.size())
		return false;

	auto it1 = s1.begin();
	int cc = 0;
	for (auto it2 = s2.begin(); it2 != s2.end(); ++it2, ++it1, ++cc)
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
struct rebind<std::set<T>, U>
{
	using type = std::set<U>;
};

template<class T, class Extract, class Al, class U>
struct rebind<seq::radix_set<T, Extract, Al>, U>
{
	using type = seq::radix_set<U, seq::default_key, RebindAlloc<Al, U>>;
};

template<class C>
void check_sorted(C& set)
{

	size_t dist = static_cast<size_t>(std::distance(set.begin(), set.end()));
	SEQ_TEST(dist == set.size());
	SEQ_TEST(std::is_sorted(set.begin(), set.end()));
	SEQ_TEST(std::is_sorted(set.rbegin(), set.rend(), std::greater<>{}));

	// build keys
	std::vector<typename C::value_type> vec;
	for (auto it = set.begin(); it != set.end(); ++it)
		vec.push_back(*it);

	for (size_t i = 0; i < vec.size(); ++i) {
		SEQ_TEST(set.find(vec[i]) != set.end());
	}
}

inline std::wstring from_string(const std::string& str)
{
	std::wstring res(str.size(), (wchar_t)0);
	for (size_t i = 0; i < str.size(); ++i)
		res[i] = (wchar_t)str[i];
	return res;
}

inline void test_radix_set_common()
{
	{
		// Test heterogeneous lookup on custom type
		struct Extract
		{
			int operator()(const std::shared_ptr<int>& p) const { return *p; }
			int operator()(int c) const { return c; }
		};
		seq::radix_set<std::shared_ptr<int>, Extract> set;
		set.insert(std::make_shared<int>(2));
		SEQ_TEST(set.find(2) != set.end());
	}
	{
		// Test heterogeneous lookup on string type
		dummy_alloc<char>::get_int() = 0;
		seq::radix_set<seq::tiny_string<char, std::char_traits<char>, dummy_alloc<char>, 0>> set;
		set.insert("this is a very very long string");
		SEQ_TEST(set.find("this is a very very long string") != set.end());
	}
	{
		// Test heterogeneous lookup on arithmetic type
		seq::radix_set<int> set;
		set.insert(2);
		SEQ_TEST(set.find(2.2) != set.end() && *set.find(2.2) == 2);
		SEQ_TEST(set.find(2) != set.end() && *set.find(2) == 2);
	}
	// TODO
	/* {
		// Test composite key
		struct Point
		{
			short x;
			int y;
			//bool operator==(const Point& other) const { return x == other.x && y == other.y; }
			bool operator<(const Point& other) const {
				return x < other.x ? true :
					x == other.x ? y < other.y : false;
			}
			bool operator>(const Point& other) const {
				return x > other.x ? true :
					x == other.x ? y > other.y : false;
			}
			bool operator==(const Point& pt) const
			{
				return x == pt.x && y == pt.y;
			}
			operator size_t() const { return static_cast<size_t>(x); }
		};
		struct Extract
		{
			std::tuple<short, int> operator()(const Point& p) const {
				return std::make_tuple(p.x, p.y);
			}

		};

		std::mt19937 e2;
		e2.seed();
		std::uniform_int_distribution<int> uniform_dist;

		std::vector<Point> vec;
		for (size_t i = 0; i < 500000; ++i)
		{
			vec.push_back(Point{ static_cast<short>(uniform_dist(e2)), static_cast<int>(uniform_dist(e2)) });
		}

		std::set<Point> set1;
		seq::radix_set<Point,Extract> set2;

		for (size_t i = 0; i < vec.size(); ++i)
		{
			set1.insert(vec[i]);
			set2.insert(vec[i]);
		}

		SEQ_TEST(set1.size() == set2.size());
		SEQ_TEST(seq::equal(set1.begin(), set1.end(), set2.begin()));
		for (size_t i = 0; i < vec.size(); ++i)
			SEQ_TEST(set2.find(vec[i]) != set2.end());
	}*/
	{
		// Test wide string
		std::vector<std::string> vec;
		for (size_t i = 0; i < 100000; ++i)
			vec.push_back(seq::generate_random_string<std::string>(63, false));

		std::vector<std::wstring> wvec;
		for (size_t i = 0; i < vec.size(); ++i)
			wvec.push_back(from_string(vec[i]));

		std::set<std::wstring> set1;
		seq::radix_set<std::wstring> set2;

		for (size_t i = 0; i < wvec.size(); ++i) {
			set1.insert(wvec[i]);
			set2.insert(wvec[i]);
		}

		SEQ_TEST(set1.size() == set2.size());
		SEQ_TEST(seq::equal(set1.begin(), set1.end(), set2.begin()));
		for (size_t i = 0; i < wvec.size(); ++i)
			SEQ_TEST(set2.find(wvec[i]) != set2.end());
	}
	{
		// test lower_bound
		size_t mul = 100;
		seq::radix_set<size_t> set;
		size_t count = 0;

		for (size_t i = 1000 * mul; i < 2000 * mul; i += 5, ++count) {

			set.insert(i);
		}

		// make sure the set is valid
		SEQ_TEST(std::distance(set.begin(), set.end()) == static_cast<std::ptrdiff_t>(set.size()));
		SEQ_TEST(std::is_sorted(set.begin(), set.end()));

		// look for existing values
		for (size_t i = 1000 * mul; i < 2000 * mul; i += 5) {
			auto it = set.lower_bound(i);
			// test bit position
			SEQ_TEST(it.iter.bit_pos == it.iter.get_bit_pos(it.iter.dir));
			// test iterator validity
			SEQ_TEST(it != set.end() && *it == i);
		}

		{
			// verify with find
			std::vector<size_t> vec;
			for (size_t i : set)
				vec.push_back(i);
			for (size_t i : vec) {
				SEQ_TEST(set.find(i) != set.end());
			}
		}

		// TODO: test lower_bound with vector nodes
		// TODO: test prefix search with vector nodes, leaf nodes, and multiple directories with prefix_len

		// mix existing/non existing values
		for (size_t i = 0; i < 4000 * mul; i++) {

			auto it = set.lower_bound(i);

			// test bit position
			if (it != set.end())
				SEQ_TEST(it.iter.bit_pos == it.iter.get_bit_pos(it.iter.dir));

			if (i < 1000 * mul) {
				// values < first value
				SEQ_TEST(it == set.begin());
			}
			else if (i > (2000 * mul) - 5) {
				// values > last value
				SEQ_TEST(it == set.end());
			}
			else if (i % 5 == 0) {
				// existing value
				SEQ_TEST(it != set.end() && *it == i);
			}
			else {

				// non existing value within [first,last]
				size_t v = *it;
				SEQ_TEST(it != set.end() && v > i && (v - i) < 5);
			}
		}
	}
}

inline void test_radix_map_common()
{
	{
		// Test heterogeneous lookup on custom type
		struct Extract
		{
			int operator()(const std::shared_ptr<int>& p) const { return *p; }
			// int operator()(int c) const { return c; }
		};
		seq::radix_map<std::shared_ptr<int>, int, Extract> set;
		set.emplace(std::make_shared<int>(2), 2);
		SEQ_TEST(set.find(2) != set.end());
	}
	{
		// Test heterogeneous lookup on string type
		dummy_alloc<char>::get_int() = 0;
		seq::radix_map<seq::tiny_string<char, std::char_traits<char>, dummy_alloc<char>, 0>, int> set;
		set.emplace("this is a very very long string", 2);
		SEQ_TEST(set.find("this is a very very long string") != set.end());
	}
	{
		// Test heterogeneous lookup on arithmetic type
		seq::radix_map<int, int> set;
		set.emplace(2, 2);
		SEQ_TEST(set.find(2.2) != set.end() && set.find(2.2)->first == 2);
		SEQ_TEST(set.find(2) != set.end() && set.find(2)->first == 2);
	}
	/* {
		// Test composite key
		struct Point
		{
			short x;
			int y;
			//bool operator==(const Point& other) const { return x == other.x && y == other.y; }
			bool operator<(const Point& other) const {
				return x < other.x ? true :
					x == other.x ? y < other.y : false;
			}
			bool operator>(const Point& other) const {
				return x > other.x ? true :
					x == other.x ? y > other.y : false;
			}
			bool operator==(const Point& pt) const
			{
				return x == pt.x && y == pt.y;
			}
			operator size_t() const { return static_cast<size_t>(x); }
		};
		struct Extract
		{
			std::tuple<short, int> operator()(const Point& p) const {
				return std::make_tuple(p.x, p.y);
			}

		};

		std::mt19937 e2;
		e2.seed();
		std::uniform_int_distribution<int> uniform_dist;

		std::vector<Point> vec;
		for (size_t i = 0; i < 500000; ++i)
		{
			vec.push_back(Point{ static_cast<short>(uniform_dist(e2)), static_cast<int>(uniform_dist(e2)) });
		}

		std::map<Point, int> set1;
		seq::radix_map<Point, int, Extract> set2;

		for (size_t i = 0; i < vec.size(); ++i)
		{
			set1.emplace(vec[i],1);
			set2.emplace(vec[i],1);
		}

		SEQ_TEST(set1.size() == set2.size());
		SEQ_TEST(seq::equal(set1.begin(), set1.end(), set2.begin(), set2.end(), [](const std::pair<Point, int>& l, const std::pair<const Point, int>& r) {return r.first == l.first && r.second
	== l.second; })); for (size_t i = 0; i < vec.size(); ++i) SEQ_TEST(set2.find(vec[i]) != set2.end());
	}*/
	{
		// Test wide string
		std::vector<std::string> vec;
		for (size_t i = 0; i < 100000; ++i)
			vec.push_back(seq::generate_random_string<std::string>(63, false));

		std::vector<std::wstring> wvec;
		for (size_t i = 0; i < vec.size(); ++i)
			wvec.push_back(from_string(vec[i]));

		std::map<std::wstring, int> set1;
		seq::radix_map<std::wstring, int> set2;

		for (size_t i = 0; i < wvec.size(); ++i) {
			set1.emplace(wvec[i], 1);
			set2.emplace(wvec[i], 1);
		}

		SEQ_TEST(set1.size() == set2.size());
		SEQ_TEST(seq::equal(set1.begin(), set1.end(), set2.begin(), set2.end(), [](const std::pair<std::wstring, int>& l, const std::pair<const std::wstring, int>& r) {
			return r.first == l.first && r.second == l.second;
		}));
		for (size_t i = 0; i < wvec.size(); ++i)
			SEQ_TEST(set2.find(wvec[i]) != set2.end());
	}
	{
		// test lower_bound
		size_t mul = 100;
		seq::radix_map<size_t, int> set;

		for (size_t i = 1000 * mul; i < 2000 * mul; i += 5)
			set.emplace(i, 1);

		// make sure the set is valid
		SEQ_TEST(std::distance(set.begin(), set.end()) == static_cast<std::ptrdiff_t>(set.size()));

		// look for existing values
		for (size_t i = 1000 * mul; i < 2000 * mul; i += 5) {
			auto it = set.lower_bound(i);
			// test bit position
			SEQ_TEST(it.iter.bit_pos == it.iter.get_bit_pos(it.iter.dir));
			// test iterator validity
			SEQ_TEST(it != set.end() && it->first == i);
		}

		// mix existing/non existing values
		for (size_t i = 0; i < 4000 * mul; i++) {
			auto it = set.lower_bound(i);

			// test bit position
			if (it != set.end())
				SEQ_TEST(it.iter.bit_pos == it.iter.get_bit_pos(it.iter.dir));

			if (i < 1000 * mul) {
				// values < first value
				SEQ_TEST(it == set.begin());
			}
			else if (i > (2000 * mul) - 5) {
				// values > last value
				SEQ_TEST(it == set.end());
			}
			else if (i % 5 == 0) {
				// existing value
				SEQ_TEST(it != set.end() && it->first == i);
			}
			else {

				// non existing value within [first,last]
				size_t v = it->first;
				SEQ_TEST(it != set.end() && v > i && (v - i) < 5);
			}
		}
	}
}

template<class set_type, class std_set_type, class Alloc>
inline void test_radix_set_logic(const Alloc& al)
{
	using namespace seq;
	using value_type = typename set_type::value_type;
	{
		// test construct from initializer list
		set_type set({ 1., 9., 2., 8., 3., 7., 4., 6., 5., 2., 7. }, al);
		std_set_type uset = { 1., 9., 2., 8., 3., 7., 4., 6., 5., 2., 7. };
		SEQ_TEST(set_equals(set, uset));
		SEQ_TEST(!set.empty());
		SEQ_TEST(set.max_size() > 0);
	}
	test_destroy<value_type>(0);
	SEQ_TEST(get_alloc_bytes(al) == 0);
	{
		// construct from range
		std::vector<value_type> v = { 1., 9., 2., 8., 3., 7., 4., 6., 5., 2., 7. };
		set_type set(v.begin(), v.end(), al);
		std_set_type uset(v.begin(), v.end());
		SEQ_TEST(set_equals(set, uset));
	}
	test_destroy<value_type>(0);
	SEQ_TEST(get_alloc_bytes(al) == 0);
	{
		// insert/emplace
		std::vector<value_type> v;
		for (size_t i = 0; i < 10000; ++i)
			v.push_back(static_cast<value_type>(i));
		seq::random_shuffle(v.begin(), v.end());

		test_destroy<value_type>(v.size());

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

		test_destroy<value_type>(v.size() + v.size());

		// test various functions for compilation

		// add already existing key
		set.emplace(v[0]);
		uset.emplace(v[0]);

		set.emplace(v[0]);
		uset.emplace(v[0]);

		set.insert(set.begin(), v[0]);
		uset.insert(uset.begin(), v[0]);

		set.emplace_hint(set.begin(), v[0]);
		uset.emplace_hint(uset.begin(), v[0]);

		// add new keys
		set.insert(v.back());
		uset.insert(v.back());

		set.insert(set.begin(), v.back());
		uset.insert(uset.begin(), v.back());

		SEQ_TEST(set_equals(set, uset));

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

		// erase
		{
			auto it = set.find(v[0]);
			set.erase(it);	 // erase iterator
			set.erase(v[1]); // erase key

			auto uit = uset.find(v[0]);
			uset.erase(uit);  // erase iterator
			uset.erase(v[1]); // erase key
		}

		SEQ_TEST(set_equals(set, uset));

		// test push front
		for (int i = -1; i > -10000; --i) {
			set.emplace(i);
			uset.emplace(i);
		}
		SEQ_TEST(set_equals(set, uset));

		// test push back
		for (int i = 10000; i < 20000; ++i) {
			set.emplace(i);
			uset.emplace(i);
		}
		SEQ_TEST(set_equals(set, uset));

		check_sorted(set);
	}
	test_destroy<value_type>(0);
	SEQ_TEST(get_alloc_bytes(al) == 0);
	{
		// test swap/move
		set_type set(al), set2({ 1., 9., 2., 8., 3., 7., 4., 6., 5., 2., 7. }, al);
		std_set_type uset, uset2 = { 1., 9., 2., 8., 3., 7., 4., 6., 5., 2., 7. };

		{
			// manual move
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
	test_destroy<value_type>(0);
	SEQ_TEST(get_alloc_bytes(al) == 0);
	{
		// test copy
		std::vector<value_type> v;
		for (size_t i = 0; i < 10000; ++i)
			v.push_back(static_cast<value_type>(i));
		seq::random_shuffle(v.begin(), v.end());

		set_type set(al);
		std_set_type uset;
		uset.insert(v.begin(), v.end());
		set.insert(v.begin(), v.end());

		{
			// test copy construct
			set_type set2(set, al);
			std_set_type uset2 = uset;
			SEQ_TEST(set_equals(set2, uset2));
		}
		{
			// test copy operator
			set_type set2(al);
			set2 = set;
			std_set_type uset2;
			uset2 = uset;
			SEQ_TEST(set_equals(set2, uset2));

			// test equality
			SEQ_TEST(set == set2);
			SEQ_TEST(uset == uset2);
		}

		uset.insert(v.begin(), v.end());
		set.insert(v.begin(), v.end());

		SEQ_TEST(set_equals(set, uset));
		check_sorted(set);
	}
	test_destroy<value_type>(0);
	SEQ_TEST(get_alloc_bytes(al) == 0);
	{
		// test with non POD type
		std::vector<std::string> v;
		for (int i = 0; i < 10000; ++i)
			v.push_back(generate_random_string<std::string>(32));
		seq::random_shuffle(v.begin(), v.end());

		typename rebind<set_type, std::string>::type set{ RebindAlloc<Alloc, std::string>(al) };
		typename rebind<std_set_type, std::string>::type uset;
		uset.insert(v.begin(), v.end());
		set.insert(v.begin(), v.end());
		SEQ_TEST(set_equals(set, uset));

		// erase half
		for (size_t i = 0; i < v.size(); i += 2) {

			set.erase(v[i]);
			uset.erase(v[i]);
			// SEQ_TEST(set_equals(set, uset));
		}
		SEQ_TEST(set_equals(set, uset));

		// reinsert all (half already exists)
		uset.insert(v.begin(), v.end());
		set.insert(v.begin(), v.end());
		SEQ_TEST(set_equals(set, uset));

		set.clear();
		uset.clear();
		SEQ_TEST(set_equals(set, uset));
		check_sorted(set);
	}
	test_destroy<value_type>(0);
	SEQ_TEST(get_alloc_bytes(al) == 0);
	{
		std::vector<value_type> vals;
		for (int i = 0; i < 100000; ++i)
			vals.push_back(static_cast<value_type>(i));
		seq::random_shuffle(vals.begin(), vals.end());

		std_set_type ref;
		ref.insert(vals.begin(), vals.begin() + vals.size() / 2);

		set_type set(al);
		set.insert(vals.begin(), vals.begin() + vals.size() / 2);

		// compare flat_set with std::set
		SEQ_TEST(seq::equal(set.begin(), set.end(), ref.begin(), ref.end()));

		// add already existing values
		set.insert(vals.begin(), vals.begin() + vals.size() / 2);
		ref.insert(vals.begin(), vals.begin() + vals.size() / 2);
		SEQ_TEST(seq::equal(set.begin(), set.end(), ref.begin(), ref.end()));

		// add already existing values one by one
		for (size_t i = 0; i < vals.size() / 2; ++i) {
			set.insert(vals[i]);
			ref.insert(vals[i]);
		}
		SEQ_TEST(seq::equal(set.begin(), set.end(), ref.begin(), ref.end()));
		check_sorted(set);
	}
	test_destroy<value_type>(0);
	SEQ_TEST(get_alloc_bytes(al) == 0);
	{
		// do the same as above with sorted values
		std::vector<value_type> vals;
		for (int i = 0; i < 100000; ++i)
			vals.push_back(static_cast<value_type>(i));

		std_set_type ref;
		ref.insert(vals.begin(), vals.begin() + vals.size() / 2);

		set_type set(al);
		set.insert(vals.begin(), vals.begin() + vals.size() / 2);

		// compare flat_set with std::set
		SEQ_TEST(seq::equal(set.begin(), set.end(), ref.begin(), ref.end()));

		// add already existing values one by one
		for (size_t i = 0; i < vals.size() / 2; ++i) {
			set.insert(vals[i]);
			ref.insert(vals[i]);
		}
		SEQ_TEST(seq::equal(set.begin(), set.end(), ref.begin(), ref.end()));
		check_sorted(set);
	}
	test_destroy<value_type>(0);
	SEQ_TEST(get_alloc_bytes(al) == 0);
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

		// compare flat_set with std::set
		SEQ_TEST(seq::equal(set.begin(), set.end(), ref.begin(), ref.end()));

		// add already existing values one by one
		for (size_t i = 0; i < vals.size() / 2; ++i) {
			set.insert(vals[i]);
			ref.insert(vals[i]);
		}
		SEQ_TEST(seq::equal(set.begin(), set.end(), ref.begin(), ref.end()));
		check_sorted(set);
	}
	test_destroy<value_type>(0);
	SEQ_TEST(get_alloc_bytes(al) == 0);
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

		// compare flat_set with std::set
		SEQ_TEST(seq::equal(set.begin(), set.end(), ref.begin(), ref.end()));

		// add already existing values one by one
		for (size_t i = 0; i < vals.size() / 2; ++i) {
			set.insert(vals[i]);
			ref.insert(vals[i]);
		}
		SEQ_TEST(seq::equal(set.begin(), set.end(), ref.begin(), ref.end()));
		check_sorted(set);
	}
	test_destroy<value_type>(0);
	SEQ_TEST(get_alloc_bytes(al) == 0);
}

template<class map_type, class umap_type>
inline void test_radix_map_logic()
{
	using namespace seq;
	using pair_type = typename map_type::value_type;
	using key_type = typename map_type::key_type;
	{
		// test construct from initializer list
		map_type set = { { 1., 1. }, { 9., 9. }, { 2., 2. }, { 8., 8. }, { 3., 3. }, { 7., 7. }, { 4., 4. }, { 6., 6. }, { 5., 5. }, { 2., 2. }, { 7., 7. } };
		umap_type uset = { { 1., 1. }, { 9., 9. }, { 2., 2. }, { 8., 8. }, { 3., 3. }, { 7., 7. }, { 4., 4. }, { 6., 6. }, { 5., 5. }, { 2., 2. }, { 7., 7. } };
		SEQ_TEST(map_equals(set, uset));
		SEQ_TEST(!set.empty());
		SEQ_TEST(set.max_size() > 0);
	}
	{
		// construct from range
		std::vector<pair_type> v = { { 1., 1. }, { 9., 9. }, { 2., 2. }, { 8., 8. }, { 3., 3. }, { 7., 7. }, { 4., 4. }, { 6., 6. }, { 5., 5. }, { 2., 2. }, { 7., 7. } };
		map_type set(v.begin(), v.end());
		umap_type uset(v.begin(), v.end());
		SEQ_TEST(map_equals(set, uset));
	}
	{
		// push_front/back and sorted
		std::vector<key_type> v;
		for (size_t i = 0; i < 10000; ++i)
			v.push_back(static_cast<key_type>(i));
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

		// test various functions for compilation

		// add existing key
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
		set.insert_or_assign(v[0], v[0]);
		set.emplace_hint(set.begin(), pair_type(v[0], v[0]));

		// replace keys
		set.insert_or_assign(v[0], v[0] * 2);
		set.insert_or_assign(set.begin(), v[0], v[0] * 2);
		uset[v[0]] = v[0] * 2;

		set.insert_or_assign(v[1], v[1] * 2);
		set.insert_or_assign(set.begin(), v[1], v[1] * 2);
		set.insert_or_assign(v[2], v[2] * 2);
		set.insert_or_assign(set.begin(), v[2], v[2] * 2);

		uset[v[1]] = v[1] * 2;
		uset[v[2]] = v[2] * 2;

		SEQ_TEST(map_equals(set, uset));

		// try_emplace
		set.try_emplace(v[0], v[0]);					// fail
		set.try_emplace(v[v.size() / 2], v[v.size() / 2]);		// succeed
		set.try_emplace(set.begin(), v[0], v[0]);			// fail
		set.try_emplace(set.begin(), v[v.size() / 2], v[v.size() / 2]); // fail

		set.try_emplace(v[0], v[0]);					// fail
		set.try_emplace(v[v.size() / 2 + 1], v[v.size() / 2 + 1]);	// succeed
		set.try_emplace(set.begin(), v[0], v[0]);			// fail
		set.try_emplace(set.begin(), v[v.size() / 2], v[v.size() / 2]); // fail

		set.try_emplace(v[0], v[0]);					// fail
		set.try_emplace(v[v.size() / 2 + 2], v[v.size() / 2 + 2]);	// succeed
		set.try_emplace(set.begin(), v[0], v[0]);			// fail
		set.try_emplace(set.begin(), v[v.size() / 2], v[v.size() / 2]); // fail

		uset.emplace(v[v.size() / 2], v[v.size() / 2]);
		uset.emplace(v[v.size() / 2 + 1], v[v.size() / 2 + 1]);
		uset.emplace(v[v.size() / 2 + 2], v[v.size() / 2 + 2]);

		SEQ_TEST(map_equals(set, uset));

		// test at() and operator[]
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
		std::vector<pair_type> vv;
		for (auto i = v.begin(); i != v.end(); ++i)
			vv.emplace_back(*i, *i);

		set.insert(vv.begin(), vv.end());
		uset.insert(vv.begin(), vv.end());

		// erase
		{
			auto it = set.find(v[0]);
			set.erase(it);	 // erase iterator
			set.erase(v[1]); // erase key

			auto uit = uset.find(v[0]);
			uset.erase(uit);  // erase iterator
			uset.erase(v[1]); // erase key
		}

		SEQ_TEST(map_equals(set, uset));
	}
	{
		// test rehash() with duplicate removal

		std::vector<pair_type> v;
		for (size_t i = 0; i < 10000; ++i)
			v.emplace_back(static_cast<key_type>(i), static_cast<key_type>(i));
		for (size_t i = 0; i < 10000; ++i)
			v.emplace_back(static_cast<key_type>(i), static_cast<key_type>(i));
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

		// remove half
		for (size_t i = 0; i < v.size() / 2; ++i) {
			uset.erase(v[i].first);
			set.erase(v[i].first);
		}
		SEQ_TEST(map_equals(set, uset));
	}
	{
		// test swap/move
		map_type set, set2 = { { 1., 1. }, { 9., 9. }, { 2., 2. }, { 8., 8. }, { 3., 3. }, { 7., 7. }, { 4., 4. }, { 6., 6. }, { 5., 5. }, { 2., 2. }, { 7., 7. } };
		umap_type uset, uset2 = { { 1., 1. }, { 9., 9. }, { 2., 2. }, { 8., 8. }, { 3., 3. }, { 7., 7. }, { 4., 4. }, { 6., 6. }, { 5., 5. }, { 2., 2. }, { 7., 7. } };

		{
			// manual move
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
		// test copy
		std::vector<pair_type> v;
		for (size_t i = 0; i < 10000; ++i)
			v.emplace_back(static_cast<key_type>(i), static_cast<key_type>(i));
		seq::random_shuffle(v.begin(), v.end());

		map_type set;
		umap_type uset;
		uset.insert(v.begin(), v.end());
		set.insert(v.begin(), v.end());

		{
			// test copy construct
			map_type set2 = set;
			umap_type uset2 = uset;
			SEQ_TEST(map_equals(set2, uset2));
		}
		{
			// test copy operator
			map_type set2;
			set2 = set;
			umap_type uset2;
			uset2 = uset;
			SEQ_TEST(map_equals(set2, uset2));

			// test equality
			SEQ_TEST(set == set2);
			SEQ_TEST(uset == uset2);
		}
	}
}

template<class T, class Extract = seq::default_key, class Alloc = std::allocator<T>>
inline void test_radix_set(const Alloc& al = Alloc())
{
	test_radix_set_common();
	test_radix_set_logic<seq::radix_set<T, Extract, Alloc>, std::set<T>>(al);
}
template<class T, class Extract = seq::default_key>
inline void test_radix_map()
{
	test_radix_map_common();
	test_radix_map_logic<seq::radix_map<T, T, Extract>, std::map<T, T>>();
}

template<class Set>
void test_heavy_set(size_t count)
{
	using key_type = typename Set::value_type;
	std::vector<key_type> keys(count);
	for (size_t i = 0; i < count; ++i)
		keys[i] = static_cast<key_type>(i);
	seq::random_shuffle(keys.begin(), keys.end());

	Set s;

	for (int k = 0; k < 2; ++k) {
		// insert range
		s.insert(keys.begin(), keys.end());
		// for (auto& k : keys)
		//	s.insert(k);
		SEQ_TEST(s.size() == count);

		// find all
		for (size_t i = 0; i < count; ++i) {
			auto it = s.find(keys[i]);
			// TEST
			if (it == s.end() || *it != keys[i]) {
				s.find(keys[i]);
			}
			SEQ_TEST(it != s.end());
			SEQ_TEST(*it == keys[i]);
		}

		// failed find
		for (size_t i = 0; i < count; ++i) {
			size_t ke = i + count;
			auto it = s.find(ke);
			SEQ_TEST(it == s.end());
		}

		s.clear();
		SEQ_TEST(s.size() == 0);

		// insert all one by one
		for (size_t i = 0; i < count; ++i) {
			s.insert(keys[i]);
			// find while inserting
			for (size_t j = 0; j <= i; ++j) {
				auto it = s.find(keys[j]);
				SEQ_TEST(it != s.end());
				SEQ_TEST(*it == keys[j]);
			}
			// failed find
			for (size_t j = i + 1; j < count; ++j) {
				auto it = s.find(keys[j]);
				SEQ_TEST(it == s.end());
			}
		}
		SEQ_TEST(s.size() == count);

		// failed insertion
		for (size_t i = 0; i < count; ++i)
			s.insert(keys[i]);
		SEQ_TEST(s.size() == count);

		// failed range insertion
		s.insert(keys.begin(), keys.end());
		SEQ_TEST(s.size() == count);

		// find all
		for (size_t i = 0; i < count; ++i) {
			auto it = s.find(keys[i]);
			SEQ_TEST(it != s.end());
			SEQ_TEST(*it == keys[i]);
		}

		// failed find
		for (size_t i = 0; i < count; ++i) {
			size_t ke = i + count;
			auto it = s.find(ke);
			SEQ_TEST(it == s.end());
		}

		// erase half
		size_t cc = (count / 2U) * 2U;
		for (size_t i = 0; i < cc; i += 2) {
			auto it = s.find(keys[i]);
			s.erase(it);
		}
		SEQ_TEST(s.size() == count / 2);

		// find all
		for (size_t i = 1; i < count; i += 2) {
			if (i >= count)
				break;

			auto it = s.find(keys[i]);
			SEQ_TEST(it != s.end());
			SEQ_TEST(*it == keys[i]);
		}

		// failed
		for (size_t i = 0; i < cc; i += 2) {
			auto it = s.find(keys[i]);
			SEQ_TEST(it == s.end());
		}
	}

	// erase remaining keys
	for (size_t i = 0; i < count; ++i) {

		auto it = s.find(keys[i]);
		if (it != s.end())
			s.erase(it);
	}

	SEQ_TEST(s.size() == 0);
}

static void test_string_key()
{
	std::vector<std::string> vec(1000);
	for (size_t i = 0; i < 1000; ++i)
		vec[i] = i == 0 ? std::string() : seq::generate_random_string<std::string>(static_cast<int>(i), true);

	std::vector<std::string> sorted = vec;
	std::sort(sorted.begin(), sorted.end());

	seq::radix_set<std::string> set;
	for (size_t i = 0; i < vec.size(); ++i)
		set.insert(vec[i]);

	check_sorted(set);
	SEQ_TEST(set_equals(sorted, set));
}
static void test_worst_string_key(char c)
{
	std::vector<std::string> vec(1000);
	std::string str;
	for (size_t i = 0; i < 1000; ++i) {
		vec[i] = str;
		str.push_back(c);
	}

	std::vector<std::string> sorted = vec;
	std::sort(sorted.begin(), sorted.end());

	seq::radix_set<std::string> set;
	for (size_t i = 0; i < vec.size(); ++i)
		set.insert(vec[i]);

	check_sorted(set);
	SEQ_TEST(set_equals(sorted, set));
}

template<class T>
struct Extract
{
	T operator()(const T& v) const { return v; }
	T operator()(const TestDestroy<T>& v) const { return static_cast<T>(v); }
};

SEQ_PROTOTYPE(int test_radix_tree(int, char*[]))
{

	SEQ_TEST_MODULE_RETURN(radix_set_string, 1, test_string_key());
	SEQ_TEST_MODULE_RETURN(test_worst_string_key_a, 1, test_worst_string_key('a'));
	SEQ_TEST_MODULE_RETURN(test_worst_string_key_0, 1, test_worst_string_key(static_cast<char>(0)));

	// Test radix map and detect potential memory leak or wrong allocator propagation
	CountAlloc<double> al;
	test_radix_set<double>(al);
	SEQ_TEST_MODULE_RETURN(radix_set, 1, test_radix_set<double>(al));
	SEQ_TEST(get_alloc_bytes(al) == 0);
	SEQ_TEST_MODULE_RETURN(radix_map, 1, test_radix_map<double>());
	SEQ_TEST_MODULE_RETURN(heavy_radix_set, 1, test_heavy_set<seq::radix_set<size_t>>(10000));

	// Test radix map and detect potential memory leak (allocations and non destroyed objects) or wrong allocator propagation
	SEQ_TEST_MODULE_RETURN(radix_set_destroy, 1, test_radix_set<TestDestroy<double>, Extract<double>>());
	SEQ_TEST(TestDestroy<double>::count() == 0);
	SEQ_TEST_MODULE_RETURN(radix_map_destroy, 1, test_radix_map<TestDestroy<double>, Extract<double>>());
	SEQ_TEST(TestDestroy<double>::count() == 0);
	SEQ_TEST_MODULE_RETURN(heavy_radix_set_destroy, 1, test_heavy_set<seq::radix_set<TestDestroy<size_t>, Extract<size_t>>>(10000));
	SEQ_TEST(TestDestroy<size_t>::count() == 0);

	// Test radix map and detect potential memory leak (allocations and non destroyed objects) or wrong allocator propagation with non relocatable type
	CountAlloc<TestDestroy<double, false>> al2;
	SEQ_TEST_MODULE_RETURN(radix_set_destroy_no_relocatable, 1, test_radix_set<TestDestroy<double, false>, Extract<double>>(al2));
	SEQ_TEST(TestDestroy<double>::count() == 0);
	SEQ_TEST(get_alloc_bytes(al2) == 0);
	SEQ_TEST_MODULE_RETURN(radix_map_destroy_no_relocatable, 1, test_radix_map<TestDestroy<double, false>, Extract<double>>());
	SEQ_TEST(TestDestroy<double>::count() == 0);
	SEQ_TEST_MODULE_RETURN(heavy_radix_set_destroy_no_relocatable, 1, test_heavy_set<seq::radix_set<TestDestroy<size_t, false>, Extract<size_t>>>(10000));
	SEQ_TEST(TestDestroy<size_t>::count() == 0);

	return 0;
}
