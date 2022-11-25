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

#include "tiny_string.hpp"
#include "testing.hpp"
#include <unordered_set>
#include <set>




template<class S1, class S2>
bool string_equals(const S1& s1, const S2& s2)
{
	if (s1.size() != s2.size())
		return false;
	return seq::equal(s1.begin(), s1.end(), s2.begin(), s2.end());
}

template<size_t MaxStaticSize = 0>
void test_tstring_logic()
{
	using namespace seq;

	std::string v;
	seq::tiny_string<MaxStaticSize> dv;

	//test push_back
	for (int i = 0; i < 200; ++i)
		v.push_back(static_cast<char>(i));
	for (int i = 0; i < 200; ++i)
		dv.push_back(static_cast<char>(i));
	
	SEQ_TEST(string_equals(v, dv));

	

	//test push front
	for (int i = 0; i < 200; ++i)
		v.insert(v.begin(), static_cast<char>(i));
	for (int i = 0; i < 200; ++i)
		dv.insert(dv.begin(), static_cast<char>(i));
	
	SEQ_TEST(string_equals(v, dv));



	// test resize;
	v.resize(1000);
	dv.resize(1000);
	SEQ_TEST(string_equals(v, dv));
	v.resize(2000, 12);
	dv.resize(2000, 12);
	SEQ_TEST(string_equals(v, dv));

	// test iterators
	std::string v1(v.size(),0), v2(v.size(),0);

	std::copy(v.begin(), v.end(), v1.begin());
	std::copy(dv.begin(), dv.end(), v2.begin());
	SEQ_TEST(string_equals(v1, v2));

	std::copy(v.rbegin(), v.rend(), v1.begin());
	std::copy(dv.rbegin(), dv.rend(), v2.begin());
	SEQ_TEST(string_equals(v1, v2));

	// test operator[]
	for (size_t i = 0; i < v.size(); ++i) v[i] = static_cast<char>(i);
	for (size_t i = 0; i < dv.size(); ++i) dv[i] = static_cast<char>(i);
	SEQ_TEST(string_equals(v, dv));

	//test shrink_to_fit
	v.shrink_to_fit();
	dv.shrink_to_fit();
	SEQ_TEST(string_equals(v, dv));


	//test insertion
	std::ptrdiff_t pos[4] = { rand() % static_cast<std::ptrdiff_t>(v.size()),
		rand() % static_cast<std::ptrdiff_t>(v.size()),
		rand() % static_cast<std::ptrdiff_t>(v.size()),
		rand() % static_cast<std::ptrdiff_t>(v.size()) };
	v.insert(v.begin() + pos[0], static_cast<char>(-46));
	v.insert(v.begin() + pos[0], static_cast < char>(-45));
	v.insert(v.begin() + pos[0], static_cast < char>(-44));
	v.insert(v.begin() + pos[0], static_cast < char>(-43));
	dv.insert(dv.begin() + pos[0], static_cast < char>(-46));
	dv.insert(dv.begin() + pos[0], static_cast < char>(-45));
	dv.insert(dv.begin() + pos[0], static_cast < char>(-44));
	dv.insert(dv.begin() + pos[0], static_cast < char>(-43));
	SEQ_TEST(string_equals(v, dv));

	//test range insertion
	v.insert(v.begin() + pos[0], v1.begin(), v1.end());
	v.insert(v.begin() + pos[1], v1.begin(), v1.end());
	v.insert(v.begin() + pos[2], v1.begin(), v1.end());
	v.insert(v.begin() + pos[3], v1.begin(), v1.end());
	dv.insert(dv.begin() + pos[0], v1.begin(), v1.end());
	dv.insert(dv.begin() + pos[1], v1.begin(), v1.end());
	dv.insert(dv.begin() + pos[2], v1.begin(), v1.end());
	dv.insert(dv.begin() + pos[3], v1.begin(), v1.end());
	SEQ_TEST(string_equals(v, dv));

	// test erase
	std::ptrdiff_t err[4] = { rand() % static_cast<std::ptrdiff_t>(v.size()),
		rand() % static_cast<std::ptrdiff_t>(v.size()),
		rand() % static_cast<std::ptrdiff_t>(v.size()),
		rand() % static_cast<std::ptrdiff_t>(v.size()) };
	for (int i = 0; i < 4; ++i) {
		if (err[i] > static_cast<std::ptrdiff_t>(v.size()) - 200)
			err[i] -= 200;
	}
	v.erase(v.begin() + err[0]);
	v.erase(v.begin() + err[1]);
	v.erase(v.begin() + err[2]);
	v.erase(v.begin() + err[3]);
	dv.erase(dv.begin() + err[0]);
	dv.erase(dv.begin() + err[1]);
	dv.erase(dv.begin() + err[2]);
	dv.erase(dv.begin() + err[3]);
	SEQ_TEST(string_equals(v, dv));

	//test erase range
	for (int i = 0; i < 4; ++i)
		v.erase(v.begin() + err[i], v.begin() + err[i] + 10);
	for (int i = 0; i < 4; ++i)
		dv.erase(dv.begin() + err[i], dv.begin() + err[i] + 10);
	SEQ_TEST(string_equals(v, dv));

	//test assign
	v.assign(v1.begin(), v1.end());
	dv.assign(v1.begin(), v1.end());
	SEQ_TEST(string_equals(v, dv));

	//test copy
	{
		std::string vv = v;
		seq::tiny_string<MaxStaticSize> dvv = dv;
		SEQ_TEST(string_equals(vv, dvv));

		vv.clear();
		dvv.clear();
		vv = v;
		dvv = dv;
		SEQ_TEST(string_equals(vv, dvv));
	}
	//test move
	{
		std::string vv = std::move(v);
		seq::tiny_string<MaxStaticSize> dvv = std::move(dv);
		SEQ_TEST(string_equals(vv, dvv));
		SEQ_TEST(string_equals(v, dv));

		v = std::move(vv);
		dv = std::move(dvv);
		SEQ_TEST(string_equals(vv, dvv));
		SEQ_TEST(string_equals(v, dv));

		//swap
		std::swap(dv, dvv);
		std::swap(v, vv);
		SEQ_TEST(string_equals(vv, dvv));
		SEQ_TEST(string_equals(v, dv));
	}

	//range construct
	{
		std::string vv(v1.begin(), v1.end());
		seq::tiny_string<MaxStaticSize> dvv(v1.begin(), v1.end());
		SEQ_TEST(string_equals(vv, dvv));
	}


	//test sorting
	{
		std::vector<std::string> vec(100000);
		for (size_t i = 0; i < vec.size(); ++i)
			vec[i] = generate_random_string<std::string>(32);
		std::vector<tiny_string<MaxStaticSize>> vec2(vec.begin(), vec.end());
		SEQ_TEST(string_equals(vec, vec2));

		std::sort(vec.begin(), vec.end());
		std::sort(vec2.begin(), vec2.end());

		SEQ_TEST(string_equals(vec, vec2));
	}


	const unsigned count = 100000;

	//test consecutive append
	{
		std::string to_append = "abcdefghi";

		std::string str;
		tiny_string<MaxStaticSize> tstr;
		size_t _count = count;

		for (size_t i = 0; i < _count; ++i)
			tstr.append(to_append.c_str());
		
		for (size_t i = 0; i < _count; ++i)
			str.append(to_append.c_str());
		
		SEQ_TEST(string_equals(str, tstr));
	}

	{


		{
			//TEST sort SSO
			std::vector<std::string> data(count);
			std::vector<tiny_string<MaxStaticSize>> tdata(count);
			for (unsigned i = 0; i < count; ++i) {
				data[i] = generate_random_string<std::string>(14, true);
				tdata[i] = data[i];
			}

			std::sort(data.begin(), data.end());
			std::sort(tdata.begin(), tdata.end());
			
			SEQ_TEST(seq::equal(data.begin(), data.end(), tdata.begin(), tdata.end()));
			
			std::vector<std::string> tmp;
			for (unsigned i = 0; i < count; ++i)
				tmp.push_back(generate_random_string<std::string>(63, true));

			data.clear();
			tdata.clear();
			data.resize(count);
			tdata.resize(count);

			// test copy using operator[]
			for (unsigned i = 0; i < count; ++i)
			{
				data[i] = tmp[i];
			}
			
			for (unsigned i = 0; i < count; ++i)
			{
				tdata[i] = tmp[i];
			}

			SEQ_TEST(string_equals(data, tdata));
			
			//test sorting on wide string
			std::sort(data.begin(), data.end());
			std::sort(tdata.begin(), tdata.end());
			SEQ_TEST(string_equals(data, tdata));
			

		}


	}
	{
		
		//test push back
		tiny_string<MaxStaticSize> tstr;
		for (unsigned i = 0; i < count; ++i)
			tstr.push_back(std::max(static_cast< char>(i), static_cast <  char>(1)));
		
		std::string str;
		for (unsigned i = 0; i < count; ++i)
			str.push_back(std::max(static_cast< char>(i), static_cast <  char>(1)));
		
		SEQ_TEST(string_equals(str, tstr));


		int sum1 = 0;
		for (unsigned i = 0; i < count; ++i)
			sum1 += tstr[i];
		
		int sum2 = 0;
		for (unsigned i = 0; i < count; ++i)
			sum2 += static_cast<char>(str[i]);

		SEQ_TEST(sum1==sum2);

		// test find
		size_t f = 0;
		size_t pos1 = 0;
		std::string find1 = "abcdefghijklmnop"; //does exists
		std::string find2 = "kdpohdsifgugcvbfd"; //does not exists

		for (int i = 0; i < 10; ++i) {
			pos1 = tstr.find((i & 1) ? find1 : find2);
			f += pos1;
			if (pos1 == std::string::npos) pos1 = 0;
			else pos1++;
		}
		
		size_t f2 = 0;
		size_t pos2 = 0;
		for (int i = 0; i < 10; ++i) {
			pos2 = str.find((i & 1) ? find1 : find2);
			f2 += pos2;
			if (pos2 == std::string::npos) pos2 = 0;
			else pos2++;
		}
		
		SEQ_TEST(f == f2);
		SEQ_TEST(pos1 == pos2);

		// test rfind
		f = 0;
		pos1 = std::string::npos;
		for (int i = 0; i < 10; ++i) {
			pos1 = tstr.rfind((i & 1) ? find1 : find2);
			f += pos1;
			if (pos1 != std::string::npos) --pos1;
		}
		
		f2 = 0;
		pos2 = std::string::npos;
		for (int i = 0; i < 10; ++i) {
			pos2 = str.rfind((i & 1) ? find1 : find2);
			f2 += pos2;
			if (pos2 != std::string::npos) --pos2;
		}
		SEQ_TEST(f == f2);
		SEQ_TEST(pos1 == pos2);



		memset(tstr.data(), 1, tstr.size() / 2);
		memset(const_cast<char*>(str.data()), 1, str.size() / 2);

		SEQ_TEST(str == tstr);

		tiny_string<MaxStaticSize> tfirst_of = "lqhgsdsfhg";
		std::string first_of = "lqhgsdsfhg";
		f = 0;
		pos1 = 0;
		for (int i = 0; i < 10; ++i) {
			pos1 = tstr.find_first_of(tfirst_of, pos1);
			f += pos1;
			if (pos1 == std::string::npos) pos1 = 0;
			else pos1++;
		}
		
		f2 = 0;
		pos2 = 0;
		for (int i = 0; i < 10; ++i) {
			pos2 = str.find_first_of(first_of, pos2);
			f2 += pos2;
			if (pos2 == std::string::npos) pos2 = 0;
			else pos2++;
		}
		SEQ_TEST(f == f2);
		SEQ_TEST(pos1 == pos2);



		for (unsigned i = 0; i < count; ++i) {
			tstr[i] = (std::max(static_cast< char>(i), static_cast <  char>(1)));
			str[i] = (std::max(static_cast< char>(i), static_cast <  char>(1)));
		}
		memset(tstr.data() + tstr.size() / 2, 0, tstr.size() - tstr.size() / 2);
		memset(const_cast<char*>(str.data()) + str.size() / 2, 0, str.size() - str.size() / 2);


		f = 0;
		pos1 = std::string::npos;
		for (int i = 0; i < 10; ++i) {
			pos1 = tstr.find_last_of(tfirst_of, pos1);
			f += pos1;
			if (pos1 != std::string::npos) --pos1;
		}
		
		f2 = 0;
		pos2 = std::string::npos;
		for (int i = 0; i < 10; ++i) {
			pos2 = str.find_last_of(first_of, pos2);
			f2 += pos2;
			if (pos2 != std::string::npos) --pos2;
		}
		SEQ_TEST(f == f2);
		SEQ_TEST(pos1 == pos2);
		
		//test compare
		unsigned len = count - static_cast<unsigned>(find1.size());
		f = 0;
		for (unsigned i = 0; i < len; ++i) {
			int c = tstr.compare(i, find1.size(), find1);
			f += static_cast<size_t>(c < 0 ? -1 : (c > 0 ? 1 : 0));
		}

		f2 = 0;
		for (unsigned i = 0; i < len; ++i) {
			int c = str.compare(i, find1.size(), find1);
			f2 += static_cast<size_t>(c < 0 ? -1 : (c > 0 ? 1 : 0));
		}
		SEQ_TEST(f == f2);

		//test pop back
		for (unsigned i = 0; i < count; ++i)
			tstr.pop_back();
		
		for (unsigned i = 0; i < count; ++i)
			str.pop_back();
		SEQ_TEST(str==tstr);
	}
}

