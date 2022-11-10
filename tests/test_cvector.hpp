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

#include <deque>
#include <list>
#include <vector>


#include "cvector.hpp"
#include "testing.hpp"


using namespace seq;



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

void test_cvector_algorithms(size_t count = 5000000)
{
	// Test algorithms on tiered_vector, some of them requiring random access iterators

	typedef size_t type;
	typedef cvector<type, std::allocator<type> > cvec_type;

	// Build with non unique random values
	cvec_type  cvec;
	std::deque<type> deq;
	srand(0);// time(NULL));
	for (size_t i = 0; i < count; ++i) {
		unsigned r = rand();// static_cast<unsigned>(count - i - 1);//rand() & ((1U << 16U) - 1U);
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
	deq.resize(it1 - deq.begin());
	cvec.resize(it2 - cvec.begin());

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



void test_cvector_move_only(size_t count)
{
	
	typedef cvector<std::unique_ptr<size_t> > cvec_type;

	std::deque<std::unique_ptr<size_t> > deq;
	cvec_type cvec;

	srand(0);
	for (size_t i = 0; i < count; ++i)
	{
		unsigned r = rand();
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
	cvec_type cvec2(cvec.size());

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
	SEQ_TEST(std::equal(deq.begin(), deq.end(), cvec.begin(), cvec.end(), make_comparator([](const auto& a, const auto& b) {return *a == *b; })));

	deq.resize(deq.size() * 2);
	cvec.resize(cvec.size() * 2);
	SEQ_TEST(std::equal(deq.begin(), deq.end(), cvec.begin(), cvec.end(), make_comparator([](const auto& a, const auto& b) {return a == b || *a == *b; })));

}



template<class T>
void test_cvector(size_t count = 5000000)
{

	// First, test some stl algorithms
	test_cvector_algorithms(count);
	test_cvector_move_only(count);

	typedef T type;
	std::deque<type > deq;
	typedef cvector<type, std::allocator<T> > cvec_type;
	cvec_type cvec;
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
		cvec_type dd2 = cvec;
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

	size_t stop = static_cast<size_t>(deq.size() * 0.75);
	// Test pop_front
	while (deq.size() > stop) {
		deq.pop_front(); 
	}
	while (cvec.size() > stop)
		cvec.erase(cvec.begin());
	SEQ_TEST(equal_cvec(deq, cvec));

	{
		// Test insert/erase single element
		cvec_type d;
		std::deque<type> dd;
		d.resize(128 * 3, 0);
		dd.resize(128 * 3, 0);
		for (size_t i = 0; i < d.size(); ++i) {
			d[i] = dd[i] = static_cast<T>(i);
		}
		SEQ_TEST(equal_cvec(d, dd));
		d.insert(d.begin() + 10, -1);
		dd.insert(dd.begin() + 10, -1);
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


	int insert_count = static_cast<int>(std::max(static_cast<size_t>(50), count / 100));
	std::vector<size_t> in_pos;
	int ss = static_cast<int>(deq.size());
	srand(0);
	for (int i = 0; i < insert_count; ++i)
		in_pos.push_back(rand() % ss++);

	// Test insert single value at random position
	for (int i = 0; i < insert_count; ++i) {
		deq.insert(deq.begin() + in_pos[i], static_cast<T>(i));
	}
	for (int i = 0; i < insert_count; ++i) {
		cvec.insert(cvec.begin() + in_pos[i], static_cast<T>(i));
	}
	SEQ_TEST(equal_cvec(deq, cvec));



	{
		// Test erase single value at random position
		cvec_type d;
		std::deque<type> dd;
		d.resize(100, 0);
		dd.resize(100, 0);
		for (size_t i = 0; i < d.size(); ++i) {
			d[i] = dd[i] = static_cast<T>(i);
		}

		for (int i = 0; i < 50; ++i) {
			int pos = i % 5;
			pos = static_cast<int>(d.size() * pos / 4);
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
	std::vector<size_t> er_pos;
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
	cvec_type tvec2 = std::move(cvec);
	SEQ_TEST(equal_cvec(deq2, tvec2) && tvec2.size() > 0 && deq.size() == 0 && cvec.size() == 0);

	deq = std::move(deq2);
	cvec = std::move(tvec2);
	SEQ_TEST(equal_cvec(deq, cvec) && cvec.size() > 0 && tvec2.size() == 0 && deq2.size() == 0);
}



