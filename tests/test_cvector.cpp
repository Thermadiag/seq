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

#ifdef TEST_CVECTOR

#include "seq/cvector.hpp"

#include <deque>
#include <list>
#include <vector>
#include <algorithm>


#include "seq/any.hpp"
#include "seq/testing.hpp"
#include "tests.hpp"


template<class Alloc, class U>
using RebindAlloc = typename std::allocator_traits<Alloc>::template rebind_alloc<U>;




template<class Deq1, class Deq2>
bool equal_cvec(const Deq1& d1, const Deq2& d2)
{
	if (d1.size() != d2.size())
		return false;
	if (d1.size() == 0)
		return true;
	if (d1.front() != d2.front())
		return false;
	if (d1.back() != d2.back()) {
		return false;
	}
	auto it1 = d1.begin();
	auto it2 = d2.begin();
	int i = 0;
	while (it1 != d1.end())
	{
		if (*it1 != *it2) {
			return false;
		}
		++it1;
		++it2;
		++i;
	}
	return true;
}

template<class Alloc = std::allocator<size_t> >
inline void test_cvector_algorithms(size_t count = 5000000, const Alloc &al = Alloc())
{
	using namespace seq;

	// Test algorithms on tiered_vector, some of them requiring random access iterators

	typedef size_t type;
	typedef cvector<type, Alloc > cvec_type;

	// Build with non unique random values
	cvec_type  cvec(al);
	std::deque<type> deq;
	srand(0);// time(NULL));
	for (size_t i = 0; i < count; ++i) {
		unsigned r = static_cast<unsigned>(rand());// static_cast<unsigned>(count - i - 1);//rand() & ((1U << 16U) - 1U);
		deq.push_back(static_cast<type>(r));
		cvec.push_back(static_cast<type>(r));
	}

	SEQ_TEST(equal_cvec(deq, cvec));

	// Test sort
	std::sort(deq.begin(), deq.end());
	std::sort(cvec.begin(), cvec.end());

	//for (size_t i = 0; i < deq.size(); ++i)
	//	std::cout << cvec[i].get() << " ";
	//std::cout << std::endl;

	/*for (size_t i = 0; i < deq.size(); ++i)
		if (deq[i] != cvec[i]) {
			std::cout << "error at " << i << ": " << deq[i] << " and " << cvec[i].get() << std::endl;
			throw std::runtime_error("");
		}*/

	SEQ_TEST(equal_cvec(deq, cvec));

	// Test unique after sorting
	auto it1 = std::unique(deq.begin(), deq.end());
	auto it2 = std::unique(cvec.begin(), cvec.end());
	deq.resize(static_cast<size_t>(it1 - deq.begin()));
	cvec.resize(static_cast<size_t>(it2 - cvec.begin()));

	SEQ_TEST(equal_cvec(deq, cvec));

	// Reset values
	deq.resize(count);
	cvec.resize(count);

	for (size_t i = 0; i < count; ++i)
		cvec[i] = deq[i] = static_cast<type>(rand());

	// Test rotate
	std::rotate(deq.begin(), deq.begin() + deq.size() / 2, deq.end());
	std::rotate(cvec.begin(), cvec.begin() + cvec.size() / 2, cvec.end());

	SEQ_TEST(equal_cvec(deq, cvec));

	// Test reversing
	std::reverse(deq.begin(), deq.end());
	std::reverse(cvec.begin(), cvec.end());

	SEQ_TEST(equal_cvec(deq, cvec));

	// Reset values
	for (size_t i = 0; i < count; ++i)
		cvec[i] = deq[i] = static_cast<type>(rand());

	// Test partial sort
	std::partial_sort(deq.begin(), deq.begin() + deq.size() / 2, deq.end());
	std::partial_sort(cvec.begin(), cvec.begin() + cvec.size() / 2, cvec.end());
	SEQ_TEST(equal_cvec(deq, cvec));



	for (size_t i = 0; i < count; ++i)
		cvec[i] = deq[i] = static_cast<type>(rand());

	// Strangely, msvc implementation of std::nth_element produce a warning as it tries to modify the value of const iterator
	// Test nth_element
	std::nth_element(deq.begin(), deq.begin() + deq.size() / 2, deq.end());
	std::nth_element(cvec.begin(), cvec.begin() + cvec.size() / 2, cvec.end());
	SEQ_TEST(equal_cvec(deq, cvec));

}




template<class Alloc >
inline void test_cvector_move_only(size_t count, const Alloc & al = Alloc())
{
	using namespace seq;
	using Al = RebindAlloc<Alloc, std::unique_ptr<size_t>>;

	typedef cvector<std::unique_ptr<size_t>, Al > cvec_type;

	std::deque<std::unique_ptr<size_t> > deq;
	cvec_type cvec(al);

	srand(0);
	for (size_t i = 0; i < count; ++i)
	{
		unsigned r = static_cast<unsigned>(rand());
		deq.emplace_back(new size_t(r));
		cvec.emplace_back(new size_t(r));
	}

	for (size_t i = 0; i < count; ++i)
	{
		SEQ_TEST(*deq[i] == *cvec[i].get());
	}

	auto less = [](const std::unique_ptr<size_t>& a, const std::unique_ptr<size_t>& b) {return *a < *b; };
	std::sort(deq.begin(), deq.end(), less);
	std::sort(cvec.begin(), cvec.end(), make_comparator(less));

	for (size_t i = 0; i < count; ++i)
	{
		SEQ_TEST(*deq[i] == *cvec[i].get());
	}


	//test std::move and std::move_backward
	std::deque<std::unique_ptr<size_t> > deq2(deq.size());
	cvec_type cvec2(cvec.size(),al);

	std::move(deq.begin(), deq.end(), deq2.begin());
	std::move(cvec.begin(), cvec.end(), cvec2.begin());

	for (size_t i = 0; i < count; ++i)
	{
		SEQ_TEST(deq[i].get() == NULL);
		SEQ_TEST(cvec[i].get().get() == NULL);
		SEQ_TEST(*deq2[i] == *cvec2[i].get());
	}


	std::move_backward(deq2.begin(), deq2.end(), deq.end());
	std::move_backward(cvec2.begin(), cvec2.end(), cvec.end());

	for (size_t i = 0; i < count; ++i)
	{
		SEQ_TEST(deq2[i].get() == NULL);
		SEQ_TEST(cvec2[i].get().get() == NULL);
		SEQ_TEST(*deq[i] == *cvec[i].get());
	}

	deq.resize(deq.size() / 2);
	cvec.resize(cvec.size() / 2);
	SEQ_TEST(seq::equal(deq.begin(), deq.end(), cvec.begin(), cvec.end(), make_comparator([](const std::unique_ptr<size_t>& a, const std::unique_ptr<size_t>& b) {return *a == *b; })));

	deq.resize(deq.size() * 2);
	cvec.resize(cvec.size() * 2);
	SEQ_TEST(seq::equal(deq.begin(), deq.end(), cvec.begin(), cvec.end(), make_comparator([](const std::unique_ptr<size_t>& a, const std::unique_ptr<size_t>& b) {return a == b || *a == *b; })));

}



template<class CVec>
void from_const_wrapper(const CVec& vec)
{
	seq::r_any a = vec[0];
	SEQ_TEST(a == 2);
}

template<class Alloc,class U>
using RebindAlloc = typename std::allocator_traits<Alloc>::template rebind_alloc<U>;


template<class T, class Alloc = std::allocator<T> >
void test_cvector(size_t count = 5000000, const Alloc al = Alloc())
{
	using namespace seq;

	{
		using Al = RebindAlloc<Alloc, seq::r_any>;
		// Test cvector with seq::r_any
		seq::cvector<seq::r_any, Al > vec(al);

		vec.push_back(2);
		vec.push_back(std::unique_ptr<int>(new int(2)));

		from_const_wrapper(vec);

		seq::r_any a = std::move(vec[0]);
		SEQ_TEST(a == 2);

		vec[0] = a;
		seq::r_any b = (vec[0]);
		SEQ_TEST(b == 2);

		seq::r_any c;
		c = vec[0];
		SEQ_TEST(c == 2);

		seq::r_any d = std::move(vec[1]);
		SEQ_TEST(*d.cast<std::unique_ptr<int>&>() == 2);
		vec[1] = std::move(d);

		SEQ_TEST_THROW(std::bad_function_call, seq::r_any e = vec[1]);
	}

	// First, test some stl algorithms
	//std::cout << "Test cvector algorithms..." << std::endl;
	test_cvector_algorithms(count,al);
	//std::cout << "Test cvector move only..." << std::endl;
	test_cvector_move_only(count, al);

	typedef T type;
	std::deque<type > deq;
	typedef cvector<type, Alloc > cvec_type;
	cvec_type cvec(al);
	std::vector<type> vec;

	SEQ_TEST(cvec.begin() == cvec.end());
	SEQ_TEST(cvec.size() == 0);
	//SEQ_TEST(cvec.lower_bound(0) == 0);
	//SEQ_TEST(cvec.upper_bound(0) == 0);
	//SEQ_TEST(cvec.binary_search(0) == 0);

	cvec.resize(10);
	SEQ_TEST(cvec.size() == 10);
	cvec.clear();
	SEQ_TEST(cvec.size() == 0 );


	// Fill containers
	for (size_t i = 0; i < count; ++i)
		deq.push_back(static_cast<T>(i));
	for (size_t i = 0; i < count; ++i)
		cvec.push_back(static_cast<T>(i));
	for (size_t i = 0; i < count; ++i)
		vec.push_back(static_cast<T>(i));

	SEQ_TEST(equal_cvec(deq, cvec));


	// Test resiz lower
	deq.resize(deq.size() / 10);
	cvec.resize(cvec.size() / 10);
	SEQ_TEST(equal_cvec(deq, cvec));

	// Test resize upper
	deq.resize(count, 0);
	cvec.resize(count, 0);
	SEQ_TEST(equal_cvec(deq, cvec));

	{
		// Test copy contruct
		std::deque<type> d2 = deq;
		cvec_type dd2( cvec,al);
		SEQ_TEST(equal_cvec(d2, dd2));
	}

	{
		// Test insert range based on random access itertor, left side
		deq.insert(deq.begin() + (deq.size() * 2) / 5, vec.begin(), vec.end());
		cvec.insert(cvec.begin() + (cvec.size() * 2) / 5, vec.begin(), vec.end());
		SEQ_TEST(equal_cvec(deq, cvec));

		deq.resize(count);
		cvec.resize(count);
		SEQ_TEST(equal_cvec(deq, cvec));

		// Test insert range based on random access itertor, right side
		deq.insert(deq.begin() + (deq.size() * 3) / 5, vec.begin(), vec.end());
		cvec.insert(cvec.begin() + (cvec.size() * 3) / 5, vec.begin(), vec.end());

		SEQ_TEST(equal_cvec(deq, cvec));

		deq.resize(count);
		cvec.resize(count);

		SEQ_TEST(equal_cvec(deq, cvec));
	}


	{
		// Rest values
		for (size_t i = 0; i < deq.size(); ++i) {
			deq[i] = cvec[i] = static_cast<T>(i);
		}
		SEQ_TEST(equal_cvec(deq, cvec));

		// Test erase range, left side
		deq.erase(deq.begin() + deq.size() / 4, deq.begin() + deq.size() / 2);
		cvec.erase(cvec.begin() + cvec.size() / 4, cvec.begin() + cvec.size() / 2);
		SEQ_TEST(equal_cvec(deq, cvec));

		deq.resize(count, 0);
		cvec.resize(count, 0);

		// Test erase range, right side
		deq.erase(deq.begin() + deq.size() / 2, deq.begin() + deq.size() * 3 / 4);
		cvec.erase(cvec.begin() + cvec.size() / 2, cvec.begin() + cvec.size() * 3 / 4);
		SEQ_TEST(equal_cvec(deq, cvec));
	}

	{
		deq.resize(vec.size() / 2, 0);
		cvec.resize(vec.size() / 2, 0);

		// Test assign from lower size
		deq.assign(vec.begin(), vec.end());
		cvec.assign(vec.begin(), vec.end());
		SEQ_TEST(equal_cvec(deq, cvec));

		deq.resize(vec.size() * 2, 0);
		cvec.resize(vec.size() * 2, 0);

		// Test assign from greater size
		deq.assign(vec.begin(), vec.end());
		cvec.assign(vec.begin(), vec.end());
		SEQ_TEST(equal_cvec(deq, cvec));
	}
	{
		std::list<type> lst;
		for (size_t i = 0; i < count; ++i)
			lst.push_back(static_cast<T>(i));

		deq.resize(lst.size() / 2, 0);
		cvec.resize(lst.size() / 2, 0);

		// Test assign from forward iterators
		deq.assign(lst.begin(), lst.end());
		cvec.assign(lst.begin(), lst.end());
		SEQ_TEST(equal_cvec(deq, cvec));

		deq.resize(lst.size() * 2, 0);
		cvec.resize(lst.size() * 2, 0);

		// Test assign from forward iterators
		deq.assign(lst.begin(), lst.end());
		cvec.assign(lst.begin(), lst.end());
		SEQ_TEST(equal_cvec(deq, cvec));
	}

	deq.resize(count, 0);
	cvec.resize(count, 0);
	SEQ_TEST(equal_cvec(deq, cvec));

	//fill again, backward
	for (size_t i = 0; i < deq.size(); ++i) {
		deq[i] = static_cast<T>(deq.size() - i - 1);
		cvec[i] = static_cast<T>(cvec.size() - i - 1);
	}

	// Test pop_back
	while (deq.size() > 25)
		deq.pop_back();
	while (cvec.size() > 25)
		cvec.pop_back();
	SEQ_TEST(equal_cvec(deq, cvec));

	deq.resize(count, 0);
	cvec.resize(count, 0);

	SEQ_TEST(equal_cvec(deq, cvec));

	//fill again, backward
	for (size_t i = 0; i < deq.size(); ++i) {
		deq[i] = static_cast<T>(deq.size() - i - 1);
		cvec[i] = static_cast<T>(cvec.size() - i - 1);
	}


	SEQ_TEST(equal_cvec(deq, cvec));

	size_t stop = static_cast<size_t>(static_cast<double>(deq.size()) * 0.9);
	// Test pop_front
	while (deq.size() > stop) {
		deq.pop_front(); 
	}
	while (cvec.size() > stop)
		cvec.erase(cvec.begin());
	SEQ_TEST(equal_cvec(deq, cvec));

	{
		// Test insert/erase single element
		cvec_type d(al);
		std::deque<type> dd;
		d.resize(128 * 3, 0);
		dd.resize(128 * 3, 0);
		for (size_t i = 0; i < d.size(); ++i) {
			d[i] = dd[i] = static_cast<T>(i);
		}
		SEQ_TEST(equal_cvec(d, dd));
		d.insert(d.begin() + 10, static_cast<type>(-1));
		dd.insert(dd.begin() + 10, static_cast<type>(-1));
		SEQ_TEST(equal_cvec(d, dd));
		for (size_t i = 0; i < 128; ++i) {
			d.erase(d.begin());
			dd.erase(dd.begin());
			SEQ_TEST(equal_cvec(d, dd));
		}
		SEQ_TEST(equal_cvec(d, dd));
		d.erase(d.begin());
		dd.erase(dd.begin());
		SEQ_TEST(equal_cvec(d, dd));
	}


	unsigned insert_count = static_cast<unsigned>(std::max(static_cast<size_t>(50), count / 50));
	std::vector<std::ptrdiff_t> in_pos;
	int ss = static_cast<int>(deq.size());
	srand(0);
	for (unsigned i = 0; i < insert_count; ++i)
		in_pos.push_back(rand() % ss++);

	// Test insert single value at random position
	for (unsigned i = 0; i < insert_count; ++i) {
		deq.insert(deq.begin() + in_pos[i], static_cast<T>(i));
	}
	for (unsigned i = 0; i < insert_count; ++i) {
		cvec.insert(cvec.begin() + in_pos[i], static_cast<T>(i));
	}
	SEQ_TEST(equal_cvec(deq, cvec));



	{
		// Test erase single value at random position
		cvec_type d(al);
		std::deque<type> dd;
		d.resize(100, 0);
		dd.resize(100, 0);
		for (size_t i = 0; i < d.size(); ++i) {
			d[i] = dd[i] = static_cast<T>(i);
		}

		for (int i = 0; i < 50; ++i) {
			int pos = i % 5;
			pos = static_cast<int>(d.size()) * pos / 4;
			if (pos == static_cast<int>(d.size()))--pos;
			dd.erase(dd.begin() + pos);
			d.erase(d.begin() + pos);
			SEQ_TEST(equal_cvec(d, dd));
		}
	}


	deq.resize(count, 0);
	cvec.resize(count, 0);

	// Test shrink_to_fit
	deq.shrink_to_fit();
	SEQ_TEST(equal_cvec(deq, cvec));

	//fill again, backward
	for (size_t i = 0; i < deq.size(); ++i) {
		deq[i] = static_cast<T>(deq.size() - i - 1);
		cvec[i] = static_cast<T>(cvec.size() - i - 1);
	}

	// Test erase single values at random position
	size_t erase_count = deq.size() / 8;
	std::vector<std::ptrdiff_t> er_pos;
	size_t sss = count;
	srand(0);
	for (size_t i = 0; i < erase_count; ++i)
		er_pos.push_back(rand() % static_cast<int>(sss--));


	for (size_t i = 0; i < erase_count; ++i) {
		deq.erase(deq.begin() + er_pos[i]);
	}
	for (size_t i = 0; i < erase_count; ++i) {
		cvec.erase(cvec.begin() + er_pos[i]);
	}
	SEQ_TEST(equal_cvec(deq, cvec));


	cvec.resize(count);
	deq.resize(count);
	for (size_t i = 0; i < deq.size(); ++i) {
		deq[i] = cvec[i] = static_cast<T>(i);
	}

	// Test move assign and move copy

	std::deque<type> deq2 = std::move(deq);
	cvec_type tvec2 ( std::move(cvec),al);
	SEQ_TEST(equal_cvec(deq2, tvec2) && tvec2.size() > 0 && deq.size() == 0 && cvec.size() == 0);

	deq = std::move(deq2);
	cvec = std::move(tvec2);
	SEQ_TEST(equal_cvec(deq, cvec) && cvec.size() > 0 && tvec2.size() == 0 && deq2.size() == 0);


	
}

#endif


int test_cvector(int , char*[])
{
	CountAlloc<size_t> al;

#ifdef TEST_CVECTOR
	// Test cvector and potential memory leak or wrong allocator propagation
	SEQ_TEST_MODULE_RETURN(cvector, 1, test_cvector<size_t>(50000, al));
	SEQ_TEST(get_alloc_bytes(al) == 0);
	// Test cvector and potential memory leak on object destruction
	SEQ_TEST_MODULE_RETURN(cvector_destroy, 1, test_cvector<TestDestroy<size_t> >(50000));
	SEQ_TEST(TestDestroy<size_t>::count() == 0);
#endif
	return 0;
}
