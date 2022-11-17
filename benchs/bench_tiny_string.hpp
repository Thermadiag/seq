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

#pragma once

#include <seq/tiny_string.hpp>
#include <seq/flat_map.hpp>
#include <seq/testing.hpp>
#include <seq/format.hpp>
#include <seq/cvector.hpp>

#include <unordered_set>
#include <set>

struct Less
{
	bool operator()(const std::string& v1, const std::string& v2) const noexcept{
		return seq::detail::string_inf(v1.data(), v1.data()+v1.size(), v2.data(), v2.data()+v2.size());
	}
	bool operator()(const tstring& s1, const tstring& s2) const noexcept {
		return s1 < s2;
	}
};

using namespace seq;

template<class Less, class Vec>
size_t test_sort(const Vec& v)
{
	Vec copy = v;

	tick();
	std::sort(copy.begin(), copy.end(), Less{});
	return tock_ms();
}
template<class Vec>
size_t test_sort2(const Vec& v)
{
	Vec copy = v;

	tick();
	std::sort(copy.begin(), copy.end());
	return tock_ms();
}

/// @brief Test string sorting
/// Compare sorting a vector of std::string and seq::tstring using the native std::string::operator< and with tstring::operator<
/// @param count number of string to sort
inline void test_sort_strings(size_t count = 1000000)
{
	std::cout << std::endl;
	std::cout << "Compare std::sort on vectors of small/big std::string and seq::tiny_string " << std::endl;
	std::cout << std::endl;


	std::vector<std::string> vec(count);
	std::vector<std::string> vec_w(count);
	
	for (size_t i = 0; i < vec.size(); ++i) {
		vec[i] = generate_random_string<std::string>(14, true);
		vec_w[i] = generate_random_string<std::string>(200, true);
	}
	std::vector<tstring> tvec(vec.begin(), vec.end());
	std::vector<tstring> tvec_w(vec_w.begin(), vec_w.end());
	

	std::cout << fmt(fmt("String name").l(30), "|", fmt("sort small (std::less)").c(30), "|", fmt("sort small (tstring::less)").c(30), "|", fmt("sort wide (std::less)").c(30), "|", fmt("sort wide (tstring::less)").c(30), "|") << std::endl;
	std::cout << fmt(str().c(30).f('-'), "|", str().c(30).f('-'), "|", str().c(30).f('-'), "|", str().c(30).f('-'), "|", str().c(30).f('-'), "|") << std::endl;

	std::cout << fmt("std::string").l(30)
		<< "|" << fmt(test_sort<std::less<std::string>>(vec)).c(30) 
		<< "|" << fmt(test_sort<Less>(vec)).c(30) 
		<< "|" << fmt(test_sort<std::less<std::string>>(vec_w)).c(30) 
		<< "|" << fmt(test_sort<Less>(vec_w)).c(30) << "|"
		<< std::endl;

	size_t s1 = test_sort<std::less<tstring>>(tvec);
	size_t s2 = test_sort<std::less<tstring>>(tvec_w);
	std::cout << fmt("seq::tiny_string").l(30)
		<< "|" << fmt(s1).c(30) 
		<< "|" << fmt(s1).c(30)
		<< "|" << fmt(s2).c(30) 
		<< "|" << fmt(s2).c(30) << "|"
		<< std::endl;

	cvector<tstring> cvec(vec.begin(), vec.end());
	cvector<tstring> cvec_w(vec_w.begin(), vec_w.end());

	std::cout << fmt(fmt("Compressed short string").c(40), "|", fmt("Compressed long string").c(40), "|") << std::endl;
	std::cout << fmt(str().c(40).f('-'), "|", str().c(40).f('-'), "|") << std::endl;
	s1 = test_sort2(cvec);
	s2 = test_sort2(cvec_w);
	std::cout << fmt(fmt(s1).c(40), "|", fmt(s2).c(40), "|") << std::endl;
}





template<class S1, class S2>
bool string_equals(const S1& s1, const S2& s2)
{
	if (s1.size() != s2.size())
		return false;
	return memcmp(s1.data(), s2.data(), s1.size()) == 0;
}


/// @brief Test std::string and tstring operators <, == and <=
/// @param count number of string in the vector
/// @param string_size size of each string. Default to 14 to keep SSO for bosth tstring and std::string
template<size_t MaxStaticSize = 0>
void test_tstring_operators(size_t count = 5000000, size_t string_size = 14)
{
	using tstring = tiny_string<MaxStaticSize>;

	std::cout << std::endl;
	std::cout << "Compare std::string and seq::tiny_string operators with " << count << " elements of size " << string_size<< std::endl;
	std::cout << std::endl;

	std::cout << fmt(fmt("method").l(30), "|", fmt("std::string").c(20), "|", fmt("tstring").c(20), "|") << std::endl;
	std::cout << fmt(str().l(30).f('-'), "|", str().c(20).f('-'), "|", str().c(20).f('-'), "|") << std::endl;

	auto f = fmt(pos<0, 2, 4>(), str().l(30), "|", fmt(pos<0>(), size_t(), " ms").c(20), "|", fmt(pos<0>(), size_t(), " ms").c(20), "|");


	// Test operators
	{
		std::vector<std::string> a(count);
		std::vector<tstring> b(count);
		for (size_t i = 0; i < count; ++i) {
			a[i] = generate_random_string<std::string>((int)string_size, true);
		}
		std::sort(a.begin(), a.end());
		seq::random_shuffle(a.begin(), a.end());
		std::copy(a.begin(), a.end(), b.begin());


		tick();
		size_t sum = 0;
		std::string middle = a[a.size() / 2];
		for (size_t i = 0; i < a.size(); ++i) {
			sum += a[i] == middle;
		}
		size_t std_t = tock_ms();


		tick();
		size_t sum2 = 0;
		tstring middle2 = b[b.size() / 2];
		for (size_t i = 0; i < b.size(); ++i) {
			sum2 += b[i] == middle2;
		}
		size_t tstr_t = tock_ms();

		SEQ_TEST(sum == sum2);
		std::cout << f("operator== (fail)", std_t, tstr_t) << std::endl;



		tick();
		sum = 0;
		std::vector<std::string> c = a;
		for (size_t i = 1; i < a.size(); ++i) {
			sum += a[i] == c[i];
			sum += a[i] == c[i-1];
		}
		std_t = tock_ms();

		tick();
		sum2 = 0;
		std::vector<tstring> d = b;
		for (size_t i = 1; i < b.size(); ++i) {
			sum2 += b[i] == d[i];
			sum2 += b[i] == d[i-1];
		}
		tstr_t = tock_ms();

		SEQ_TEST(sum == sum2);
		std::cout << f("operator== (half)", std_t, tstr_t) << std::endl;


		tick();
		sum = 0;
		middle = a[a.size() / 2];
		for (size_t i = 0; i < a.size(); ++i) {
			const std::string& v1 = a[i];
			sum += v1 <= middle;
		}
		std_t = tock_ms();

		tick();
		sum2 = 0;
		middle2 = b[b.size() / 2];
		for (size_t i = 0; i < b.size(); ++i) {
			const tstring& v1 = b[i];
			sum2 += v1 <= middle2;
		}
		tstr_t = tock_ms();

		SEQ_TEST(sum == sum2);
		std::cout << f("operator<=", std_t, tstr_t) << std::endl;


		tick();
		sum = 0;
		middle = a[a.size() / 2];
		for (size_t i = 0; i < a.size(); ++i) {
			const std::string& v1 = a[i];
			sum += v1 < middle;
		}
		std_t = tock_ms();

		tick();
		sum2 = 0;
		middle2 = b[b.size() / 2];
		for (size_t i = 0; i < b.size(); ++i) {
			const tstring& v1 = b[i];
			sum2 += v1 < middle2;
		}
		tstr_t = tock_ms();

		SEQ_TEST(sum == sum2);
		std::cout << f("operator<", std_t, tstr_t) << std::endl;

	}
}


/// @brief Compare some members of std::string and seq::tiny_string
template<size_t MaxStaticSize=0>
void test_tstring_members(size_t count = 5000000)
{
	using tstring = tiny_string<MaxStaticSize>;

	std::cout << std::endl;
	std::cout << "Compare std::string and seq::tiny_string with " <<count <<" elements"<< std::endl;
	std::cout << std::endl;

	std::cout << fmt(fmt("method").l(30), "|", fmt("std::string").c(20), "|", fmt("tstring").c(20), "|") << std::endl;
	std::cout << fmt(str().l(30).f('-'), "|", str().c(20).f('-'), "|", str().c(20).f('-'), "|") << std::endl;

	auto format = fmt(pos<0, 2, 4>(), str().l(30), "|", fmt(pos<0>(), size_t(), " ms").c(20), "|", fmt(pos<0>(), size_t(), " ms").c(20), "|");

	size_t std_t, tstr_t;

	//test consecutive append
	{
		std::string to_append = "abcdefghi";

		std::string str;
		tstring tstr;
		size_t _count = count;
		

		tick();
		for (size_t i = 0; i < _count; ++i)
			tstr.append(to_append);
		tstr_t = tock_ms();

		tick();
		for (size_t i = 0; i < _count; ++i)
			str.append(to_append);
		std_t = tock_ms();

		SEQ_TEST(string_equals(str, tstr));
		std::cout << format("append string(9) lots of time", std_t, tstr_t) << std::endl;


		tick();
		tstring tstr2 = tstr;
		tstr_t = tock_ms();

		tick();
		tstring str2 = str;
		std_t = tock_ms();

		SEQ_TEST(string_equals(str2, tstr2));
		std::cout << format("copy construct", std_t, tstr_t) << std::endl;

	}

	{
		std::vector<std::string> vec(count);
		for (size_t i = 0; i < count; ++i)
			vec[i] = generate_random_string<std::string>(14, true);
		std::vector<tstring> tvec(count);
		std::copy(vec.begin(), vec.end(), tvec.begin());

		std::vector<tstring> ttmp(count);
		tick();
		std::copy(tvec.begin(), tvec.end(), ttmp.begin());
		tstr_t = tock_ms();

		std::vector<std::string> tmp(count);
		tick();
		std::copy(vec.begin(), vec.end(), tmp.begin());
		std_t = tock_ms();

		SEQ_TEST(seq::equal(tmp.begin(), tmp.end(), ttmp.begin()));
		std::cout << format("copy lots of small strings", std_t, tstr_t) << std::endl;

		std::vector<tstring> ttmp2(count);
		tick();
		std::move(tvec.begin(), tvec.end(), ttmp2.begin());
		tstr_t = tock_ms();

		std::vector<std::string> tmp2(count);
		tick();
		std::move(vec.begin(), vec.end(), tmp2.begin());
		std_t = tock_ms();

		SEQ_TEST(seq::equal(tmp2.begin(), tmp2.end(), ttmp2.begin()));
		std::cout << format("move lots of small strings", std_t, tstr_t) << std::endl;
	}
	{
		std::vector<std::string> vec(count);
		for (size_t i = 0; i < count; ++i)
			vec[i] = generate_random_string<std::string>(MaxStaticSize == 0 ? 24 : MaxStaticSize + 10, true);
		std::vector<tstring> tvec(count);
		std::copy(vec.begin(), vec.end(), tvec.begin());

		/*std::vector<tstring> ttmp(count);
		tick();
		std::copy(tvec.begin(), tvec.end(), ttmp.begin());
		tstr_t = tock_ms();

		std::vector<std::string> tmp(count);
		tick();
		std::copy(vec.begin(), vec.end(), tmp.begin());
		std_t = tock_ms();

		SEQ_TEST(seq::equal(tmp.begin(), tmp.end(), ttmp.begin()));
		std::cout << format("copy lots of big strings", std_t, tstr_t) << std::endl;
		*/
		std::vector<tstring> ttmp2(count);
		tick();
		std::move(tvec.begin(), tvec.end(), ttmp2.begin());
		tstr_t = tock_ms();

		std::vector<std::string> tmp2(count);
		tick();
		std::move(vec.begin(), vec.end(), tmp2.begin());
		std_t = tock_ms();

		SEQ_TEST(seq::equal(tmp2.begin(), tmp2.end(), ttmp2.begin()));
		std::cout << format("move lots of big strings", std_t, tstr_t) << std::endl;
	}

	{
		size_t std_t, tstr_t;
		std::string str;
		tstring tstr;

		tick();
		for (size_t i = 0; i < count; ++i)
		{
			tstr.push_back(std::max((unsigned char)i, (unsigned char)1));
		}
		tstr_t = tock_ms();

		tick();
		for (size_t i = 0; i < count; ++i)
			str.push_back(std::max((unsigned char)i, (unsigned char)1));
		std_t = tock_ms();

		SEQ_TEST(string_equals(str, tstr));
		std::cout << format("push_back", std_t, tstr_t) << std::endl;


		size_t sum = 0;
		tick();
		for (int i = 0; i < count; ++i)
			sum += (size_t)tstr[i];
		tstr_t = tock_ms();

		size_t sum2 = 0;
		tick();
		for (size_t i = 0; i < count; ++i)
			sum2 += (size_t)str[i];
		std_t = tock_ms();

		SEQ_TEST(sum == sum2);
		std::cout << format("operator[]", std_t, tstr_t) << std::endl;


		size_t f = 0;
		size_t pos = 0;
		std::string find1 = "abcdefghijklmnop"; //does exists
		std::string find2 = "kdpohdsifgugcvbfd"; //does not exists
		
		tick();
		for (int i = 0; i < 10; ++i) {
			pos = tstr.find((i & 1) ? find1 : find2);
			f += pos;
			if (pos == std::string::npos) pos = 0;
			else pos++;
		}
		tstr_t = tock_ms();

		size_t f2 = 0;
		size_t pos2 = 0;
		tick();
		for (int i = 0; i < 10; ++i) {
			pos2 = str.find((i & 1) ? find1 : find2);
			f2 += pos2;
			if (pos2 == std::string::npos) pos2 = 0;
			else pos2++;
		}
		std_t = tock_ms();

		SEQ_TEST(f == f2 && pos == pos2);
		std::cout << format("find", std_t, tstr_t) << std::endl;


		f = 0;
		pos = std::string::npos;
		tick();
		for (int i = 0; i < 10; ++i) {
			pos = tstr.rfind((i & 1) ? find1 : find2);
			f += pos;
			if (pos != std::string::npos) --pos;
		}
		tstr_t = tock_ms();

		f2 = 0;
		pos2 = std::string::npos;
		tick();
		for (int i = 0; i < 10; ++i) {
			pos2 = str.rfind((i & 1) ? find1 : find2);
			f2 += pos2;
			if (pos2 != std::string::npos) --pos2;
		}
		std_t = tock_ms();
		SEQ_TEST(f == f2 && pos == pos2);
		std::cout << format("rfind", std_t, tstr_t) << std::endl;


		// reset left half to 1
		memset(tstr.data(), 1, tstr.size() / 2);
		memset((char*)str.data(), 1, str.size() / 2);
		SEQ_TEST( tstr == str);


		tstring tfirst_of = "lqhgsdsfhg";
		std::string first_of = "lqhgsdsfhg";
		f = 0;
		pos = 0;
		tick();
		for (int i = 0; i < 10; ++i) {
			pos = tstr.find_first_of(tfirst_of, pos);
			f += pos;
			if (pos == std::string::npos) pos = 0;
			else pos++;
		}
		tstr_t = tock_ms();

		f2 = 0;
		pos2 = 0;
		tick();
		for (int i = 0; i < 10; ++i) {
			pos2 = str.find_first_of(first_of, pos2);
			f2 += pos2;
			if (pos2 == std::string::npos) pos2 = 0;
			else pos2++;
		}
		std_t = tock_ms();
		SEQ_TEST(f == f2 && pos == pos2);
		std::cout << format("find_first_of", std_t, tstr_t) << std::endl;


		// reset both strings
		for (int i = 0; i < count; ++i) {
			tstr[i] = (std::max((unsigned char)i, (unsigned char)1));
			str[i] = (std::max((unsigned char)i, (unsigned char)1));
		}
		//reset right half to 0
		memset(tstr.data() + tstr.size() / 2, 0, tstr.size() - tstr.size() / 2);
		memset((char*)str.data() + str.size() / 2, 0, str.size() - str.size() / 2);


		f = 0;
		pos = std::string::npos;
		tick();
		for (int i = 0; i < 10; ++i) {
			pos = tstr.find_last_of(tfirst_of, pos);
			f += pos;
			if (pos != std::string::npos) --pos;
		}
		tstr_t = tock_ms();

		f2 = 0;
		pos2 = std::string::npos;
		tick();
		for (int i = 0; i < 10; ++i) {
			pos2 = str.find_last_of(first_of, pos2);
			f2 += pos2;
			if (pos2 != std::string::npos) --pos2;
		}
		std_t = tock_ms();
		SEQ_TEST(f == f2 && pos == pos2);
		std::cout << format("find_last_of", std_t, tstr_t) << std::endl;


		size_t len = count - find1.size();

		f = 0;
		tick();
		for (size_t i = 0; i < len; ++i) {
			f += tstr.compare(i, find1.size(), find1);
		}
		tstr_t = tock_ms();

		f2 = 0;
		tick();
		for (size_t i = 0; i < len; ++i)
		{
			int c = str.compare(i, find1.size(), find1);
			if (c < 0) c = -1;
			else if (c > 0) c = 1;
			f2 += c;
		}
		std_t = tock_ms();
		SEQ_TEST(f == f2 && str == tstr);
		std::cout << format("compare", std_t, tstr_t) << std::endl;


		tick();
		for (int i = 0; i < count; ++i)
			tstr.pop_back();
		tstr_t = tock_ms();

		tick();
		for (int i = 0; i < count; ++i)
			str.pop_back();
		std_t = tock_ms();
		SEQ_TEST(str.size() == tstr.size());
		std::cout << format("pop_back", std_t, tstr_t) << std::endl;
	}
}