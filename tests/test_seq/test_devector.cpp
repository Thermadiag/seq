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



#include "seq/testing.hpp"
#include "seq/devector.hpp"
#include "tests.hpp"
#include <vector>

template<class V1, class V2>
bool vector_equals(const V1& v1, const V2& v2)
{
	if (v1.size() != v2.size())
		return false;
	return seq::equal(v1.begin(), v1.end(), v2.begin(), v2.end());
}



template<class T, class Alloc = std::allocator<T> >
void test_devector_logic(const Alloc & al = Alloc())
{
	using namespace seq;

	std::vector<T> v;
	devector<T, Alloc> dv(al);

	//test push_back
	for (size_t i = 0; i < 200; ++i)
		v.push_back(static_cast<T>(i));
	for (size_t i = 0; i < 100; ++i)
		dv.push_back(static_cast<T>(i));
	for (size_t i = 100; i < 200; ++i)
		dv.emplace_back(static_cast<T>(i));

	SEQ_TEST(vector_equals(v, dv));

	//test push_back after reserve
	v.clear();
	dv.clear();
	v.reserve(200);
	dv.reserve(200);
	for (size_t i = 0; i < 200; ++i)
		v.push_back(static_cast<T>(i));
	for (size_t i = 0; i < 100; ++i)
		dv.push_back(static_cast<T>(i));
	for (size_t i = 100; i < 200; ++i)
		dv.emplace_back(static_cast<T>(i));

	SEQ_TEST(vector_equals(v, dv));

	//test push_back after reserve back
	v.clear();
	dv.clear();
	v.reserve(200);
	dv.reserve_back(200);
	for (int i = 0; i < 200; ++i)
		v.push_back(static_cast<T>(i));
	for (int i = 0; i < 100; ++i)
		dv.push_back(static_cast<T>(i));
	for (int i = 100; i < 200; ++i)
		dv.emplace_back(static_cast<T>(i));

	SEQ_TEST(vector_equals(v, dv));

	//test push_back after reserve front
	v.clear();
	dv.clear();
	v.reserve(200);
	dv.reserve_front(200);
	for (int i = 0; i < 200; ++i)
		v.push_back(static_cast<T>(i));
	for (int i = 0; i < 100; ++i)
		dv.push_back(static_cast<T>(i));
	for (int i = 100; i < 200; ++i)
		dv.emplace_back(static_cast<T>(i));

	SEQ_TEST(vector_equals(v, dv));



	//test push front
	for (size_t i = 0; i < 200; ++i)
		v.insert(v.begin(), static_cast<T>(i));
	for (size_t i = 0; i < 100; ++i)
		dv.push_front(static_cast<T>(i));
	for (size_t i = 100; i < 200; ++i)
		dv.emplace_front(static_cast<T>(i));

	SEQ_TEST(vector_equals(v, dv));

	//test push_back after reserve
	v.clear();
	dv.clear();
	v.reserve(200);
	dv.reserve(200);
	for (size_t i = 0; i < 200; ++i)
		v.insert(v.begin(), static_cast<T>(i));
	for (size_t i = 0; i < 100; ++i)
		dv.push_front(static_cast<T>(i));
	for (size_t i = 100; i < 200; ++i)
		dv.emplace_front(static_cast<T>(i));

	SEQ_TEST(vector_equals(v, dv));

	//test push_back after reserve back
	v.clear();
	dv.clear();
	v.reserve(200);
	dv.reserve_back(200);
	for (size_t i = 0; i < 200; ++i)
		v.insert(v.begin(), static_cast<T>(i));
	for (size_t i = 0; i < 100; ++i)
		dv.push_front(static_cast<T>(i));
	for (size_t i = 100; i < 200; ++i)
		dv.emplace_front(static_cast<T>(i));

	SEQ_TEST(vector_equals(v, dv));

	//test push_back after reserve front
	v.clear();
	dv.clear();
	v.reserve(200);
	dv.reserve_front(200);
	for (size_t i = 0; i < 200; ++i)
		v.insert(v.begin(), static_cast<T>(i));
	for (size_t i = 0; i < 100; ++i)
		dv.push_front(static_cast<T>(i));
	for (size_t i = 100; i < 200; ++i)
		dv.emplace_front(static_cast<T>(i));

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
	for (size_t i = 0; i < v.size(); ++i) v[i] = static_cast<T>(i);
	for (size_t i = 0; i < dv.size(); ++i) dv[i] = static_cast<T>(i);
	SEQ_TEST(vector_equals(v, dv));

	//test shrink_to_fit
	v.shrink_to_fit();
	dv.shrink_to_fit();
	SEQ_TEST(vector_equals(v, dv));


	//test insertion
	std::ptrdiff_t pos[4] = { 
		rand() % static_cast<std::ptrdiff_t>(v.size()),
		rand() % static_cast<std::ptrdiff_t>(v.size()),
		rand() % static_cast<std::ptrdiff_t>(v.size()),
		rand() % static_cast<std::ptrdiff_t>(v.size()) };
	v.insert(v.begin() + pos[0], static_cast<T>(1234));
	v.insert(v.begin() + pos[0], static_cast<T>(1235));
	v.insert(v.begin() + pos[0], static_cast<T>(1236));
	v.insert(v.begin() + pos[0], static_cast<T>(1237));
	dv.insert(dv.begin() + pos[0], static_cast<T>(1234));
	dv.insert(dv.begin() + pos[0], static_cast<T>(1235));
	dv.insert(dv.begin() + pos[0], static_cast<T>(1236));
	dv.insert(dv.begin() + pos[0], static_cast<T>(1237));
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
	std::ptrdiff_t err[4] = { rand() % static_cast<std::ptrdiff_t>(v.size()),
		rand() % static_cast<std::ptrdiff_t>(v.size()),
		rand() % static_cast<std::ptrdiff_t>(v.size()),
		rand() % static_cast<std::ptrdiff_t>(v.size()) };
	for (int i = 0; i < 4; ++i) {
		if (err[i] > static_cast<std::ptrdiff_t>(v.size() - 200))
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
		devector<T, Alloc> dvv(dv,al);
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
		devector<T, Alloc> dvv (std::move(dv),al);
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
		devector<T, Alloc> dvv(v1.begin(), v1.end(), al);
		SEQ_TEST(vector_equals(vv, dvv));
	}
}

#include "tests.hpp"

SEQ_PROTOTYPE( int test_devector(int , char*[]))
{
	
	// Test devector and potential memory leak or wrong allocator propagation
	CountAlloc<size_t> al;
	SEQ_TEST_MODULE_RETURN(devector, 1, test_devector_logic<size_t,CountAlloc<size_t> >(al));
	SEQ_TEST(get_alloc_bytes(al) == 0);
	
	// Test devector and potential memory leak on object destruction
	SEQ_TEST_MODULE_RETURN(devector_destroy, 1, test_devector_logic<TestDestroy<size_t>>());
	SEQ_TEST(TestDestroy<size_t>::count() == 0);
	
	// Test devector and potential memory leak or wrong allocator propagation with non relocatable type
	CountAlloc<TestDestroy<size_t, false>> al2;
	SEQ_TEST_MODULE_RETURN(devector_destroy_no_relocatable, 1, test_devector_logic<TestDestroy<size_t,false>>(al2));
	SEQ_TEST(get_alloc_bytes(al2) == 0);
	SEQ_TEST(TestDestroy<size_t>::count() == 0);
	

	return 0;
}
