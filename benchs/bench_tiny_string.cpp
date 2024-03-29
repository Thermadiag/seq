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


#include <seq/tiny_string.hpp>
#include <seq/testing.hpp>
#include <seq/format.hpp>
#include <seq/cvector.hpp>
#include <seq/pdqsort.hpp>

#include <unordered_set>
#include <set>

extern "C" {
#include <stdlib.h>
}

using namespace seq;


template<class String, class Char = typename String::value_type>
struct Convert
{
	static String apply(const char* value) {
		const size_t cSize = strlen(value) + 1;

		String wc;
		wc.resize(cSize);

		//size_t cSize1;
		//mbstowcs_s(&cSize1, (wchar_t*)&wc[0], cSize, value, cSize);
		mbstowcs((wchar_t*)&wc[0], value, cSize);

		wc.pop_back();

		return wc;
	}

};
template<class String>
struct Convert<String, char>
{
	static String apply(const char* value) {
		return String(value);
	}

};


struct Less
{
	template<class Char>
	bool operator()(const std::basic_string<Char,std::char_traits<Char>, std::allocator<Char>> & v1, const std::basic_string<Char, std::char_traits<Char>, std::allocator<Char>>& v2) const noexcept{
		
		return seq::detail::traits_string_inf< std::char_traits<Char> >(v1.data(), v1.size(), v2.data(), v2.size());
	}
	template<class Char, class Traits, class Al, size_t S>
	bool operator()(const tiny_string<Char,Traits,Al,S>& s1, const tiny_string<Char, Traits, Al, S>& s2) const noexcept {
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
	std::sort(copy.begin(), copy.end(), std::less<>{});
	return tock_ms();
}

/// @brief Test string sorting
/// Compare sorting a vector of std::string and seq::tstring using the native std::string::operator< and with tstring::operator<
/// @param count number of string to sort
template<class Char>
inline void test_sort_strings(size_t count = 1000000)
{
	std::cout << std::endl;
	std::cout << "Compare std::sort on vectors of small/big std::string and seq::tiny_string with char type '" <<typeid(Char).name()<<"'"<< std::endl;
	std::cout << std::endl;

	using std_string = std::basic_string<Char, std::char_traits<Char>, std::allocator<Char> >;
	using t_string = tiny_string<Char, std::char_traits<Char>, std::allocator<Char> >;

	std::vector<std_string> vec(count);
	std::vector<std_string> vec_w(count);
	
	for (size_t i = 0; i < vec.size(); ++i) {
		vec[i] = generate_random_string<std_string>(13/sizeof(Char), true);
		vec_w[i] = generate_random_string<std_string>(199, false);
	}
	std::vector<t_string> tvec(vec.begin(), vec.end());
	std::vector<t_string> tvec_w(vec_w.begin(), vec_w.end());
	

	std::cout << fmt(fmt("String name").l(30), "|", fmt("sort small (std::less)").c(30), "|", fmt("sort small (tstring::less)").c(30), "|", fmt("sort wide (std::less)").c(30), "|", fmt("sort wide (tstring::less)").c(30), "|") << std::endl;
	std::cout << fmt(str().c(30).f('-'), "|", str().c(30).f('-'), "|", str().c(30).f('-'), "|", str().c(30).f('-'), "|", str().c(30).f('-'), "|") << std::endl;

	std::cout << fmt("std::string").l(30)
		<< "|" << fmt(test_sort<std::less<std_string>>(vec)).c(30) 
		<< "|" << fmt(test_sort<Less>(vec)).c(30) 
		<< "|" << fmt(test_sort<std::less<std_string>>(vec_w)).c(30) 
		<< "|" << fmt(test_sort<Less>(vec_w)).c(30) << "|"
		<< std::endl;

	size_t s1 = test_sort<std::less<t_string>>(tvec);
	size_t s2 = test_sort<std::less<t_string>>(tvec_w);
	std::cout << fmt("seq::tiny_string").l(30)
		<< "|" << fmt(s1).c(30) 
		<< "|" << fmt(s1).c(30)
		<< "|" << fmt(s2).c(30) 
		<< "|" << fmt(s2).c(30) << "|"
		<< std::endl;

	

	cvector<t_string> cvec(vec.begin(), vec.end());
	cvector<t_string> cvec_w(vec_w.begin(), vec_w.end());

	std::cout << fmt(fmt("Compressed short string").c(40), "|", fmt("Compressed long string").c(40), "|") << std::endl;
	std::cout << fmt(str().c(40).f('-'), "|", str().c(40).f('-'), "|") << std::endl;
	s1 = test_sort2(cvec);
	s2 = test_sort2(cvec_w);
	std::cout << fmt(fmt(s1).c(40), "|", fmt(s2).c(40), "|") << std::endl;

	std::sort(tvec.begin(), tvec.end());
	std::sort(vec.begin(), vec.end());
	std::sort(tvec_w.begin(), tvec_w.end());
	std::sort(vec_w.begin(), vec_w.end());

	SEQ_TEST(seq::equal(tvec.begin(), tvec.end(), vec.begin(), vec.end()) == true);
	SEQ_TEST(seq::equal(tvec_w.begin(), tvec_w.end(), vec_w.begin(), vec_w.end()) == true);
}

#include <seq/devector.hpp>
#include <seq/flat_map.hpp>

void test_push_back_vector(size_t count)
{
	// In order to be truly representative, SEQ_GROW_FACTOR should be set to the one of std::vector on the considered STL implementation
	std::cout << "Test push back "<<count<< " small string in vector" << std::endl << std::endl;
	std::vector<std::string> vec(count);
	std::vector<tstring> tvec(count);
	for (size_t i = 0; i < count; ++i)
		tvec[i] = vec[i] = generate_random_string<std::string>(13, true);

	auto header = join("|", _str().c(20), _str().c(20), _str().c(20), "");
	std::cout << header("String type", "std::vector", "seq::devector") << std::endl;
	std::cout << header(fill('-'), fill('-'), fill('-')) << std::endl;

	auto line = join("|", _str().c(20), _u().c(20), _u().c(20), "");

	size_t vec_string, vec_tstring, de_string, de_tstring;

	std::vector<std::string> vs; 
	std::vector<tstring> vt; 
	seq::devector<std::string, std::allocator<std::string>, OptimizeForPushBack> ds;
	seq::devector<tstring, std::allocator<tstring>, OptimizeForPushBack> dt; 
	{
		tick();
		for (size_t i = 0; i < vec.size(); ++i)
			vs.push_back(vec[i]);
		vec_string = tock_ms();
	}
	{
		tick();
		for (size_t i = 0; i < tvec.size(); ++i)
			vt.push_back(tvec[i]);
		vec_tstring = tock_ms();
	}
	{
		tick();
		for (size_t i = 0; i < vec.size(); ++i)
			ds.push_back(vec[i]);
		de_string = tock_ms();
	}
	{
		tick();
		for (size_t i = 0; i < tvec.size(); ++i)
			dt.push_back(tvec[i]);
		de_tstring = tock_ms();
	}

	std::cout << line("std::string", vec_string, de_string) << std::endl;
	std::cout << line("seq::tstring", vec_tstring, de_tstring) << std::endl;
}



void test_insert_flat_map(size_t count)
{
	std::cout << "Test insert "<<count<< "small string in a seq::flat_set" << std::endl << std::endl;
	std::vector<std::string> vec(count);
	std::vector<tstring> tvec(count);
	for (size_t i = 0; i < count; ++i)
		tvec[i] = vec[i] = generate_random_string<std::string>(13, true);

	auto header = join("|", _str().c(20), _str().c(20),  "");
	std::cout << header("String type", "Insert (flat_set)") << std::endl;
	std::cout << header(fill('-'), fill('-')) << std::endl;

	auto line = join("|", _str().c(20), _u().c(20), "");

	size_t vec_string, vec_tstring;

	seq::flat_set<std::string> vs;
	seq::flat_set<tstring> vt;
	{
		tick();
		for (size_t i = 0; i < vec.size(); ++i)
			vs.insert(vec[i]);
		vec_string = tock_ms();
	}
	{
		tick();
		for (size_t i = 0; i < tvec.size(); ++i)
			vt.insert(tvec[i]);
		vec_tstring = tock_ms();
	}
	

	std::cout << line("std::string", vec_string) << std::endl;
	std::cout << line("seq::tstring", vec_tstring) << std::endl;
}

#ifdef SEQ_HAS_CPP_17
#include "gtl/btree.hpp"

template<class String>
void test_insert_map(const char * str_name,size_t count)
{
	std::vector<String> vec(count);
	for (size_t i = 0; i < count; ++i)
		 vec[i] = generate_random_string<String>(13, true);

	auto header = join("|", _str().l(20), _str().c(20), _str().c(20), _str().c(20) ,"");
	std::cout << header(str_name, "seq::flat_set","gtl::btree_set", "std::set") << std::endl;
	std::cout << header(fill('-'), fill('-'), fill('-'), fill('-')) << std::endl;

	auto line = join("|", _str().l(20), _fmt( _u(), " ms").c(20), _fmt(_u(), " ms").c(20), _fmt(_u(), " ms").c(20), "");

	size_t i_flat, i_ph, i_set, f_flat, f_ph, f_set;

	
	{
		tick();
		seq::flat_set<String> flat;
		for (size_t i = 0; i < vec.size(); ++i)
			flat.insert(vec[i]);
		i_flat = tock_ms();

		tick();
		size_t sum = 0;
		for (size_t i = 0; i < vec.size(); ++i)
			sum += flat.find_pos(vec[i]);
		f_flat = tock_ms();
		print_null(sum);
	}
	{
		tick();
		gtl::btree_set<String> ph;
		for (size_t i = 0; i < vec.size(); ++i)
			ph.insert(vec[i]);
		i_ph = tock_ms();

		tick();
		size_t sum = 0;
		for (size_t i = 0; i < vec.size(); ++i)
			sum += ph.find(vec[i]) != ph.end();
		f_ph = tock_ms();
		print_null(sum);
	}
	{
		tick();
		std::set<String> set;
		for (size_t i = 0; i < vec.size(); ++i)
			set.insert(vec[i]);
		i_set = tock_ms();

		tick();
		size_t sum = 0;
		for (size_t i = 0; i < vec.size(); ++i)
			sum += set.find(vec[i]) != set.end();
		f_set = tock_ms();
		print_null(sum);
	}


	std::cout << line("insert", i_flat, i_ph, i_set) << std::endl;
	std::cout << line("find", f_flat, f_ph, f_set) << std::endl;
	std::cout << std::endl;
}

#else


template<class String>
void test_insert_map(const char* str_name, size_t count)
{
	std::vector<String> vec(count);
	for (size_t i = 0; i < count; ++i)
		vec[i] = generate_random_string<String>(13, true);

	auto header = join("|", _str().l(20),  _str().c(20), _str().c(20), "");
	std::cout << header(str_name, "seq::flat_set",  "std::set") << std::endl;
	std::cout << header(fill('-'),  fill('-'), fill('-')) << std::endl;

	auto line = join("|", _str().l(20), _fmt(_u(), " ms").c(20), _fmt(_u(), " ms").c(20), "");

	size_t i_flat, i_set, f_flat, f_set;


	{
		tick();
		seq::flat_set<String> flat;
		for (size_t i = 0; i < vec.size(); ++i)
			flat.insert(vec[i]);
		i_flat = tock_ms();

		tick();
		size_t sum = 0;
		for (size_t i = 0; i < vec.size(); ++i)
			sum += flat.find_pos(vec[i]);
		f_flat = tock_ms();
		print_null(sum);
	}
	
	{
		tick();
		std::set<String> set;
		for (size_t i = 0; i < vec.size(); ++i)
			set.insert(vec[i]);
		i_set = tock_ms();

		tick();
		size_t sum = 0;
		for (size_t i = 0; i < vec.size(); ++i)
			sum += set.find(vec[i]) != set.end();
		f_set = tock_ms();
		print_null(sum);
	}


	std::cout << line("insert", i_flat, i_set) << std::endl;
	std::cout << line("find", f_flat, f_set) << std::endl;
	std::cout << std::endl;
}

#endif



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
template<class Char, size_t MaxStaticSize = 0>
void test_tstring_operators(size_t count = 5000000, size_t string_size = 13)
{
	using std_string = std::basic_string<Char, std::char_traits<Char>, std::allocator<Char> >;
	using t_string = tiny_string<Char, std::char_traits<Char>, std::allocator<Char>, MaxStaticSize>;


	std::cout << std::endl;
	std::cout << "Compare std::string and seq::tiny_string operators with " << count << " elements of size " << string_size<< " and type '" <<typeid(Char).name() <<"'"<<std::endl;
	std::cout << std::endl;

	std::cout << fmt(fmt("method").l(30), "|", fmt("std::string").c(20), "|", fmt("tstring").c(20), "|") << std::endl;
	std::cout << fmt(str().l(30).f('-'), "|", str().c(20).f('-'), "|", str().c(20).f('-'), "|") << std::endl;

	auto f = fmt(pos<0, 2, 4>(), str().l(30), "|", fmt(pos<0>(), size_t(), " ms").c(20), "|", fmt(pos<0>(), size_t(), " ms").c(20), "|");


	// Test operators
	{
		std::vector<std_string> a(count);
		std::vector<t_string> b(count);
		for (size_t i = 0; i < count; ++i) {
			a[i] = generate_random_string<std_string>((int)string_size, true);
		}
		std::sort(a.begin(), a.end());
		seq::random_shuffle(a.begin(), a.end());
		std::copy(a.begin(), a.end(), b.begin());


		tick();
		size_t sum = 0;
		std_string middle = a[a.size() / 2];
		for (size_t i = 0; i < a.size(); ++i) {
			sum += a[i] == middle;
		}
		size_t std_t = tock_ms();


		tick();
		size_t sum2 = 0;
		t_string middle2 = b[b.size() / 2];
		for (size_t i = 0; i < b.size(); ++i) {
			sum2 += b[i] == middle2;
		}
		size_t tstr_t = tock_ms();

		SEQ_TEST(sum == sum2);
		std::cout << f("operator== (fail)", std_t, tstr_t) << std::endl;



		tick();
		sum = 0;
		std::vector<std_string> c = a;
		for (size_t i = 1; i < a.size(); ++i) {
			sum += a[i] == c[i];
			sum += a[i] == c[i-1];
		}
		std_t = tock_ms();

		tick();
		sum2 = 0;
		std::vector<t_string> d = b;
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
			const std_string& v1 = a[i];
			sum += v1 <= middle;
		}
		std_t = tock_ms();

		tick();
		sum2 = 0;
		middle2 = b[b.size() / 2];
		for (size_t i = 0; i < b.size(); ++i) {
			const t_string& v1 = b[i];
			sum2 += v1 <= middle2;
		}
		tstr_t = tock_ms();

		SEQ_TEST(sum == sum2);
		std::cout << f("operator<=", std_t, tstr_t) << std::endl;


		tick();
		sum = 0;
		middle = a[a.size() / 2];
		for (size_t i = 0; i < a.size(); ++i) {
			const std_string& v1 = a[i];
			sum += v1 < middle;
		}
		std_t = tock_ms();

		tick();
		sum2 = 0;
		middle2 = b[b.size() / 2];
		for (size_t i = 0; i < b.size(); ++i) {
			const t_string& v1 = b[i];
			sum2 += v1 < middle2;
		}
		tstr_t = tock_ms();

		SEQ_TEST(sum == sum2);
		std::cout << f("operator<", std_t, tstr_t) << std::endl;

	}
}


/// @brief Compare some members of std::string and seq::tiny_string
template<class Char, size_t MaxStaticSize=0>
void test_tstring_members(size_t count = 5000000)
{
	using std_string = std::basic_string<Char, std::char_traits<Char>, std::allocator<Char> >;
	using tstring = tiny_string<Char,std::char_traits<Char>,std::allocator<Char>,MaxStaticSize>;

	std::cout << std::endl;
	std::cout << "Compare std::string and seq::tiny_string with " <<count <<" elements for type '"<<typeid(Char).name() <<"'"<< std::endl;
	std::cout << std::endl;

	std::cout << fmt(fmt("method").l(30), "|", fmt("std::string").c(20), "|", fmt("tstring").c(20), "|") << std::endl;
	std::cout << fmt(str().l(30).f('-'), "|", str().c(20).f('-'), "|", str().c(20).f('-'), "|") << std::endl;

	auto format = fmt(pos<0, 2, 4>(), str().l(30), "|", fmt(pos<0>(), size_t(), " ms").c(20), "|", fmt(pos<0>(), size_t(), " ms").c(20), "|");

	size_t std_t, tstr_t;

	//test consecutive append
	{
		std_string to_append = Convert<std_string>::apply("abcdefghi");

		std_string str;
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
		std::vector<std_string> vec(count);
		for (size_t i = 0; i < count; ++i)
			vec[i] = generate_random_string<std_string>(13, true);
		std::vector<tstring> tvec(count);
		std::copy(vec.begin(), vec.end(), tvec.begin());

		std::vector<tstring> ttmp(count);
		tick();
		std::copy(tvec.begin(), tvec.end(), ttmp.begin());
		tstr_t = tock_ms();

		std::vector<std_string> tmp(count);
		tick();
		std::copy(vec.begin(), vec.end(), tmp.begin());
		std_t = tock_ms();

		SEQ_TEST(seq::equal(tmp.begin(), tmp.end(), ttmp.begin()));
		std::cout << format("copy lots of small strings", std_t, tstr_t) << std::endl;

		std::vector<tstring> ttmp2(count);
		tick();
		std::move(tvec.begin(), tvec.end(), ttmp2.begin());
		tstr_t = tock_ms();

		std::vector<std_string> tmp2(count);
		tick();
		std::move(vec.begin(), vec.end(), tmp2.begin());
		std_t = tock_ms();

		SEQ_TEST(seq::equal(tmp2.begin(), tmp2.end(), ttmp2.begin()));
		std::cout << format("move lots of small strings", std_t, tstr_t) << std::endl;
	}
	{
		std::vector<std_string> vec(count);
		for (size_t i = 0; i < count; ++i)
			vec[i] = generate_random_string<std_string>(MaxStaticSize == 0 ? 24 : MaxStaticSize + 10, true);
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

		std::vector<std_string> tmp2(count);
		tick();
		std::move(vec.begin(), vec.end(), tmp2.begin());
		std_t = tock_ms();

		SEQ_TEST(seq::equal(tmp2.begin(), tmp2.end(), ttmp2.begin()));
		std::cout << format("move lots of big strings", std_t, tstr_t) << std::endl;
	}

	{
		size_t std_t, tstr_t;
		std_string str;
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
		std_string find1 = Convert<std_string>::apply("abcdefghijklmnop"); //does exists
		std_string find2 = Convert<std_string>::apply("kdpohdsifgugcvbfd"); //does not exists
		
		tick();
		for (int i = 0; i < 10; ++i) {
			pos = tstr.find((i & 1) ? find1 : find2);
			f += pos;
			if (pos == std_string::npos) pos = 0;
			else pos++;
		}
		tstr_t = tock_ms();

		size_t f2 = 0;
		size_t pos2 = 0;
		tick();
		for (int i = 0; i < 10; ++i) {
			pos2 = str.find((i & 1) ? find1 : find2);
			f2 += pos2;
			if (pos2 == std_string::npos) pos2 = 0;
			else pos2++;
		}
		std_t = tock_ms();

		SEQ_TEST(f == f2 && pos == pos2);
		std::cout << format("find", std_t, tstr_t) << std::endl;


		f = 0;
		pos = std_string::npos;
		tick();
		for (int i = 0; i < 10; ++i) {
			pos = tstr.rfind((i & 1) ? find1 : find2);
			f += pos;
			if (pos != std_string::npos) --pos;
		}
		tstr_t = tock_ms();

		f2 = 0;
		pos2 = std_string::npos;
		tick();
		for (int i = 0; i < 10; ++i) {
			pos2 = str.rfind((i & 1) ? find1 : find2);
			f2 += pos2;
			if (pos2 != std_string::npos) --pos2;
		}
		std_t = tock_ms();
		SEQ_TEST(f == f2 && pos == pos2);
		std::cout << format("rfind", std_t, tstr_t) << std::endl;


		// reset left half to 1
		memset(tstr.data(), 1, tstr.size() / 2);
		memset((char*)str.data(), 1, str.size() / 2);
		SEQ_TEST( tstr == str);


		tstring tfirst_of = Convert<tstring>::apply("lqhgsdsfhg");
		std_string first_of = Convert<std_string>::apply("lqhgsdsfhg");
		f = 0;
		pos = 0;
		tick();
		for (int i = 0; i < 10; ++i) {
			pos = tstr.find_first_of(tfirst_of, pos);
			f += pos;
			if (pos == std_string::npos) pos = 0;
			else pos++;
		}
		tstr_t = tock_ms();

		f2 = 0;
		pos2 = 0;
		tick();
		for (int i = 0; i < 10; ++i) {
			pos2 = str.find_first_of(first_of, pos2);
			f2 += pos2;
			if (pos2 == std_string::npos) pos2 = 0;
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
		SEQ_TEST(str == tstr);
		//reset right half to 0
		memset(tstr.data() + tstr.size() / 2, 0, tstr.size() - tstr.size() / 2);
		memset((Char*)str.data() + str.size() / 2, 0, str.size() - str.size() / 2);

		SEQ_TEST(str == tstr);

		f = 0;
		pos = std_string::npos;
		tick();
		for (int i = 0; i < 10; ++i) {
			pos = tstr.find_last_of(tfirst_of, pos);
			f += pos;
			if (pos != std_string::npos) --pos;
		}
		tstr_t = tock_ms();

		f2 = 0;
		pos2 = std_string::npos;
		tick();
		for (int i = 0; i < 10; ++i) {
			pos2 = str.find_last_of(first_of, pos2);
			f2 += pos2;
			if (pos2 != std_string::npos) --pos2;
		}
		std_t = tock_ms();
		SEQ_TEST(f == f2 && pos == pos2);
		std::cout << format("find_last_of", std_t, tstr_t) << std::endl;


		size_t len = count - find1.size();

		SEQ_TEST(str == tstr);

		f = 0;
		tick();
		for (size_t i = 0; i < len; ++i) {
			int c= tstr.compare(i, find1.size(), find1);
			f += c;
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
		for (int i = 0; i < count/2; ++i)
			tstr.pop_back();
		tstr_t = tock_ms();

		tick();
		for (int i = 0; i < count/2; ++i)
			str.pop_back();
		std_t = tock_ms();
		SEQ_TEST(str.size() == tstr.size());
		SEQ_TEST(str == tstr);
		std::cout << format("pop_back", std_t, tstr_t) << std::endl;
	}
}


int bench_tiny_string(int, char** const)
{
	
	/*using Char = char32_t;
	using string = tiny_string<Char>;
	using string1 = tiny_string<Char,std::char_traits<Char>,std::allocator<Char>, 0>;
	using string2 = tiny_string<Char, std::char_traits<Char>, std::allocator<Char>, 14>;
	using string3 = tiny_string<Char, std::char_traits<Char>, std::allocator<Char>, 15>;
	using string4 = tiny_string<Char, std::char_traits<Char>, std::allocator<Char>, 28>;

	string s0; int _s0 = string::max_static_size; int of0 = sizeof(string);
	string1 s1; int _s1 = string1::max_static_size; int of1 = sizeof(string1);
	string2 s2; int _s2 = string2::max_static_size; int of2 = sizeof(string2);
	string3 s3; int _s3 = string3::max_static_size; int of3 = sizeof(string3);
	string4 s4; int _s4 = string4::max_static_size; int of4 = sizeof(string4);*/

	test_insert_map<std::string>("std::string",500000);
	test_insert_map<seq::tstring>("seq::tstring",500000);
	
	test_push_back_vector(10000000);
	test_insert_flat_map(1000000);

	test_sort_strings<char>(2000000);
	test_sort_strings<wchar_t>(2000000);
	test_tstring_members<char>(20000000);
	test_tstring_members<wchar_t>(20000000);
	test_tstring_operators<char,0>(5000000, 13);
	test_tstring_operators<wchar_t, 0>(5000000, 5);
	return 0;
}