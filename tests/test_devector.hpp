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

#include "testing.hpp"
#include "devector.hpp"
#include <vector>

template<class V1, class V2>
bool vector_equals(const V1& v1, const V2& v2)
{
	if (v1.size() != v2.size())
		return false;
	return seq::equal(v1.begin(), v1.end(), v2.begin(), v2.end());
}

using namespace seq;

template<class T, DEVectorFlag flag>
void test_devector_logic()
{
	std::vector<T> v;
	devector<T, std::allocator<T>, flag> dv;

	//test push_back
	for (int i = 0; i < 200; ++i)
		v.push_back(i);
	for (int i = 0; i < 100; ++i)
		dv.push_back(i);
	for (int i = 100; i < 200; ++i)
		dv.emplace_back(i);

	SEQ_TEST(vector_equals(v, dv));

	//test push_back after reserve
	v.clear();
	dv.clear();
	v.reserve(200);
	dv.reserve(200);
	for (int i = 0; i < 200; ++i)
		v.push_back(i);
	for (int i = 0; i < 100; ++i)
		dv.push_back(i);
	for (int i = 100; i < 200; ++i)
		dv.emplace_back(i);

	SEQ_TEST(vector_equals(v, dv));

	//test push_back after reserve back
	v.clear();
	dv.clear();
	v.reserve(200);
	dv.reserve_back(200);
	for (int i = 0; i < 200; ++i)
		v.push_back(i);
	for (int i = 0; i < 100; ++i)
		dv.push_back(i);
	for (int i = 100; i < 200; ++i)
		dv.emplace_back(i);

	SEQ_TEST(vector_equals(v, dv));

	//test push_back after reserve front
	v.clear();
	dv.clear();
	v.reserve(200);
	dv.reserve_front(200);
	for (int i = 0; i < 200; ++i)
		v.push_back(i);
	for (int i = 0; i < 100; ++i)
		dv.push_back(i);
	for (int i = 100; i < 200; ++i)
		dv.emplace_back(i);

	SEQ_TEST(vector_equals(v, dv));



	//test push front
	for (int i = 0; i < 200; ++i)
		v.insert(v.begin(),i);
	for (int i = 0; i < 100; ++i)
		dv.push_front(i);
	for (int i = 100; i < 200; ++i)
		dv.emplace_front(i);

	SEQ_TEST(vector_equals(v, dv));

	//test push_back after reserve
	v.clear();
	dv.clear();
	v.reserve(200);
	dv.reserve(200);
	for (int i = 0; i < 200; ++i)
		v.insert(v.begin(), i);
	for (int i = 0; i < 100; ++i)
		dv.push_front(i);
	for (int i = 100; i < 200; ++i)
		dv.emplace_front(i);

	SEQ_TEST(vector_equals(v, dv));

	//test push_back after reserve back
	v.clear();
	dv.clear();
	v.reserve(200);
	dv.reserve_back(200);
	for (int i = 0; i < 200; ++i)
		v.insert(v.begin(), i);
	for (int i = 0; i < 100; ++i)
		dv.push_front(i);
	for (int i = 100; i < 200; ++i)
		dv.emplace_front(i);

	SEQ_TEST(vector_equals(v, dv));

	//test push_back after reserve front
	v.clear();
	dv.clear();
	v.reserve(200);
	dv.reserve_front(200);
	for (int i = 0; i < 200; ++i)
		v.insert(v.begin(), i);
	for (int i = 0; i < 100; ++i)
		dv.push_front(i);
	for (int i = 100; i < 200; ++i)
		dv.emplace_front(i);

	SEQ_TEST(vector_equals(v, dv));



	// test resize;
	v.resize(1000);
	dv.resize(1000);
	SEQ_TEST(vector_equals(v, dv));
	v.resize(2000, 12);
	dv.resize(2000, 12);
	SEQ_TEST(vector_equals(v, dv));

	// test iterators
	std::vector<T> v1(v.size()), v2(v.size());

	std::copy(v.begin(), v.end(), v1.begin());
	std::copy(dv.begin(), dv.end(), v2.begin());
	SEQ_TEST(vector_equals(v1, v2));

	std::copy(v.rbegin(), v.rend(), v1.begin());
	std::copy(dv.rbegin(), dv.rend(), v2.begin());
	SEQ_TEST(vector_equals(v1, v2));

	// test operator[]
	for (size_t i = 0; i < v.size(); ++i) v[i] = i;
	for (size_t i = 0; i < dv.size(); ++i) dv[i] = i;
	SEQ_TEST(vector_equals(v, dv));

	//test shrink_to_fit
	v.shrink_to_fit();
	dv.shrink_to_fit();
	SEQ_TEST(vector_equals(v, dv));


	//test insertion
	size_t pos[4] = { rand() % v.size(),rand() % v.size(),rand() % v.size(),rand() % v.size() };
	v.insert(v.begin() + pos[0], 1234);
	v.insert(v.begin() + pos[0], 1235);
	v.insert(v.begin() + pos[0], 1236);
	v.insert(v.begin() + pos[0], 1237);
	dv.insert(dv.begin() + pos[0], 1234);
	dv.insert(dv.begin() + pos[0], 1235);
	dv.insert(dv.begin() + pos[0], 1236);
	dv.insert(dv.begin() + pos[0], 1237);
	SEQ_TEST(vector_equals(v, dv));

	//test range insertion
	v.insert(v.begin() + pos[0], v1.begin(), v1.end());
	v.insert(v.begin() + pos[1], v1.begin(), v1.end());
	v.insert(v.begin() + pos[2], v1.begin(), v1.end());
	v.insert(v.begin() + pos[3], v1.begin(), v1.end());
	dv.insert(dv.begin() + pos[0], v1.begin(), v1.end());
	dv.insert(dv.begin() + pos[1], v1.begin(), v1.end());
	dv.insert(dv.begin() + pos[2], v1.begin(), v1.end());
	dv.insert(dv.begin() + pos[3], v1.begin(), v1.end());
	SEQ_TEST(vector_equals(v, dv));

	// test erase
	size_t err[4] = { rand() % v.size(),rand() % v.size(),rand() % v.size(),rand() % v.size() };
	for (int i = 0; i < 4; ++i) {
		if (err[i] > v.size() - 200)
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
	SEQ_TEST(vector_equals(v, dv));

	//test erase range
	for(int i=0; i < 4;++i)
		v.erase( v.begin() + err[i], v.begin() + err[i]+ 10);
	for (int i = 0; i < 4; ++i)
		dv.erase( dv.begin() + err[i], dv.begin() + err[i] + 10);
	SEQ_TEST(vector_equals(v, dv));

	//test assign
	v.assign(v1.begin(), v1.end());
	dv.assign(v1.begin(), v1.end());
	SEQ_TEST(vector_equals(v, dv));

	//test copy
	{
		std::vector<T> vv = v;
		devector<T, std::allocator<T>, flag> dvv = dv;
		SEQ_TEST(vector_equals(vv, dvv));

		vv.clear();
		dvv.clear();
		vv = v;
		dvv = dv;
		SEQ_TEST(vector_equals(vv, dvv));
	}
	//test move
	{
		std::vector<T> vv = std::move(v);
		devector<T, std::allocator<T>, flag> dvv = std::move(dv);
		SEQ_TEST(vector_equals(vv, dvv));
		SEQ_TEST(vector_equals(v, dv));

		v = std::move(vv);
		dv = std::move(dvv);
		SEQ_TEST(vector_equals(vv, dvv));
		SEQ_TEST(vector_equals(v, dv));

		//swap
		std::swap(dv, dvv);
		std::swap(v, vv);
		SEQ_TEST(vector_equals(vv, dvv));
		SEQ_TEST(vector_equals(v, dv));
	}
	
	//range construct
	{
		std::vector<T> vv(v1.begin(), v1.end());
		devector<T, std::allocator<T>, flag> dvv(v1.begin(), v1.end());
		SEQ_TEST(vector_equals(vv, dvv));
	}
}