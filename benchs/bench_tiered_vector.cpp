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


#include <seq/tiered_vector.hpp>
#include <seq/devector.hpp>
#include <seq/testing.hpp>
#include <seq/format.hpp>

#include <list>
#include <deque> 
#include <algorithm>
#include <cstdio>
#include <vector>
#include <iostream>

using namespace seq;


template<class Deq1, class Deq2>
void assert_equal(const Deq1& d1, const Deq2& d2)
{
	if (d1.size() != d2.size())
		throw std::runtime_error("different size!");
	if (d1.size() == 0)
		return;
	if (d1.front() != d2.front())
		throw std::runtime_error("different front!");
	if (d1.back() != d2.back()) {
		throw std::runtime_error("different back!");
	}
	auto it1 = d1.begin();
	auto it2 = d2.begin();
	int i = 0;
	while (it1 != d1.end())
	{
		if (*it1 != *it2) {
			throw std::runtime_error("");
		}
		++it1;
		++it2;
		++i;
	}
}


/// @brief Compare performances of std::vector, std::deque, seq::tiered_vector for some stl algorithms
template< class T>
void test_tiered_vector_algorithms(size_t count = 5000000)
{
	std::cout << std::endl;
	std::cout << "Compare performances of std::vector, std::deque, seq::tiered_vector for some stl algorithms" << std::endl;
	std::cout << std::endl;


	typedef T type;
	typedef tiered_vector<type, std::allocator<type> > deque_type;

	deque_type  tvec;
	std::deque<type> deq;
	std::vector<type> vec;
	srand(0);// time(NULL));
	for (size_t i = 0; i < count; ++i) {
		T r = (T)rand();
		deq.push_back(r);
		tvec.push_back(r);
		vec.push_back(r);
	}

	std::cout << fmt(fmt("algorithm").l(20), "|", fmt("std::vector").c(20), "|", fmt("std::deque").c(20), "|", fmt("seq::tiered_vector").c(20), "|") << std::endl;
	std::cout << fmt(rep('-',20), "|", rep('-',20), "|", rep('-', 20), "|", rep('-', 20), "|") << std::endl;

	auto f = fmt(pos<0, 2, 4, 6>(), str().l(20), "|", fmt(pos<0>(), size_t(), " ms").c(20), "|", fmt(pos<0>(), size_t(), " ms").c(20), "|", fmt(pos<0>(), size_t(), " ms").c(20), "|");

	tick(); 
	std::sort(vec.begin(), vec.end());
	size_t vec_t = tock_ms();

	tick();
	std::sort(deq.begin(), deq.end());
	size_t deq_t = tock_ms();

	tick();
	std::sort(tvec.begin(), tvec.end());
	size_t tvec_t = tock_ms();

	assert_equal(deq, tvec);
	std::cout << f("std::sort", vec_t, deq_t, tvec_t) << std::endl;

	tick();
	auto it1 = std::unique(vec.begin(), vec.end());
	(void)it1;
	vec_t = tock_ms();

	tick();
	auto it2 = std::unique(deq.begin(), deq.end());
	(void)it2;
	deq_t = tock_ms();

	tick();
	auto it3 = std::unique(tvec.begin(), tvec.end());
	(void)it3;
	tvec_t = tock_ms();

	assert_equal(deq, tvec);
	std::cout << f("std::unique", vec_t, deq_t, tvec_t) << std::endl;

	for (size_t i = 0; i < count; ++i)
		tvec[i] = deq[i] = vec[i] =  rand();


	tick();
	std::rotate(vec.begin(), vec.begin() + vec.size() / 2, vec.end());
	vec_t = tock_ms();

	tick();
	std::rotate(deq.begin(), deq.begin() + deq.size() / 2, deq.end());
	deq_t = tock_ms();

	tick();
	std::rotate(tvec.begin(), tvec.begin() + tvec.size() / 2, tvec.end());
	tvec_t = tock_ms();

	assert_equal(deq, tvec);
	std::cout << f("std::rotate", vec_t, deq_t, tvec_t) << std::endl;
	


	tick();
	std::reverse(vec.begin(), vec.end());
	vec_t = tock_ms();

	tick();
	std::reverse(deq.begin(), deq.end());
	deq_t = tock_ms();

	tick();
	std::reverse(tvec.begin(), tvec.end());
	tvec_t = tock_ms();

	assert_equal(deq, tvec);
	std::cout << f("std::reverse", vec_t, deq_t, tvec_t) << std::endl;

	for (size_t i = 0; i < count; ++i)
		tvec[i] = deq[i] = vec[i] =  rand();

	tick();
	std::partial_sort(vec.begin(), vec.begin() + vec.size() / 2, vec.end());
	vec_t = tock_ms();

	tick();
	std::partial_sort(deq.begin(), deq.begin() + deq.size() / 2, deq.end());
	deq_t = tock_ms();

	tick();
	std::partial_sort(tvec.begin(), tvec.begin() + tvec.size() / 2, tvec.end());
	tvec_t = tock_ms();

	assert_equal(deq, tvec);
	std::cout << f("std::partial_sort", vec_t, deq_t, tvec_t) << std::endl;

	for (size_t i = 0; i < count; ++i)
		tvec[i] = deq[i] = vec[i] = rand();

	tick();
	std::nth_element(vec.begin(), vec.begin() + vec.size() / 2, vec.end());
	vec_t = tock_ms();

	tick();
	std::nth_element(deq.begin(), deq.begin() + deq.size() / 2, deq.end());
	deq_t = tock_ms();

	tick();
	std::nth_element(tvec.begin(), tvec.begin() + tvec.size() / 2, tvec.end());
	tvec_t = tock_ms();

	assert_equal(deq, tvec);
	std::cout << f("std::nth_element", vec_t, deq_t, tvec_t) << std::endl;
	
}


//#include "segmented_tree/include/boost/segmented_tree/seq.hpp"

/// @brief Compare performances of std::vector, std::deque, seq::tiered_vector and seq::devector
/// A value of 1000000000 means that the container has not been tested against a particular operation because too slow (for instance pop front on a std::vector).
template<class T>
void test_tiered_vector(size_t count = 10000000)
{
	std::cout << std::endl;
	std::cout << "Compare performances of std::vector, std::deque, seq::tiered_vector, seq::devector and seq::cvector" << std::endl;
	std::cout << std::endl;


	std::cout << fmt(fmt("method").l(30), "|", fmt("std::vector").c(20), "|", fmt("std::deque").c(20), "|", fmt("seq::tiered_vector").c(20), "|", fmt("seq::devector").c(20), "|") << std::endl;
	std::cout << fmt(rep('-',30), "|", rep('-',20), "|", rep('-', 20), "|", rep('-', 20), "|", rep('-', 20), "|", rep('-', 20), "|") << std::endl;

	auto f = fmt( _str().l(30), "|", _fmt(_u(), " ms").c(20), "|", _fmt(_u(), " ms").c(20), "|", _fmt(_u(), " ms").c(20), "|", _fmt(_u(), " ms").c(20), "|");


	{
		typedef T type;
		std::vector<type> vec;
		std::deque<type > deq;
		seq::devector<type> devec;

		using deque_type =  tiered_vector<type, std::allocator<T> > ;
		deque_type tvec;


		size_t vec_t, deq_t, tvec_t, devec_t, cvec_t;

		tick();
		for (size_t i = 0; i < count; ++i)
			deq.push_back(i);
		deq_t = tock_ms();

		tick();
		for (size_t i = 0; i < count; ++i)
			vec.push_back(i);
		vec_t = tock_ms();

		tick();
		for (size_t i = 0; i < count; ++i)
			devec.push_back(i);
		devec_t = tock_ms();

		tick();
		for (size_t i = 0; i < count; ++i)
			tvec.push_back(i);
		tvec_t = tock_ms();


		assert_equal(deq, tvec);
		assert_equal(deq, devec);
		std::cout << f("push_back", vec_t, deq_t, tvec_t, devec_t) << std::endl;

		

		deq = std::deque<type>{};
		vec = std::vector<type>{};
		tvec = deque_type{};
		devec = devector<type>{};
		
		tick();
		for (size_t i = 0; i < count; ++i)
			deq.push_front(i);
		deq_t = tock_ms();

		vec.assign(deq.begin(), deq.end());

		tick();
		for (size_t i = 0; i < count; ++i)
			devec.push_front(i);
		devec_t = tock_ms();

		tick();
		for (size_t i = 0; i < count; ++i)
			tvec.push_front(i);
		tvec_t = tock_ms();

		assert_equal(deq, tvec);
		assert_equal(deq, devec);
		std::cout << f("push_front", 1000000000, deq_t, tvec_t, devec_t) << std::endl;


		deq = std::deque<type>{};
		vec = std::vector<type>{};
		tvec = deque_type{};
		devec = devector<type>{};
		
		for (size_t i = 0; i < count; ++i)
		{
			deq.push_back(i);
			vec.push_back(i);
			devec.push_back(i);
			tvec.push_back(i);
			
		}

		size_t sum = 0, sum2=0, sum3=0;
		tick();
		for (size_t i = 0; i < count; ++i)
			sum += deq[i];
		deq_t = tock_ms(); print_null(sum);

		tick();
		sum = 0;
		for (size_t i = 0; i < count; ++i)
			sum += vec[i];
		vec_t = tock_ms(); print_null(sum);

		tick();
		sum = 0;
		for (size_t i = 0; i < count; ++i)
			sum += devec[i];
		devec_t = tock_ms(); print_null(sum);

		tick();
		sum2 = 0;
		for (size_t i = 0; i < count; ++i)
			sum2 += tvec[i];
		tvec_t = tock_ms(); print_null(sum2);

		SEQ_TEST(sum == sum2);
		std::cout << f("iterate operator[]", vec_t, deq_t, tvec_t, devec_t) << std::endl;


		sum = 0;
		tick();
		for (typename std::deque<type>::iterator it = deq.begin(); it != deq.end(); ++it)
			sum += *it;
		deq_t = tock_ms(); print_null(sum);

		tick();
		sum = 0;
		for (auto it = vec.begin(); it != vec.end(); ++it)
			sum += *it;
		vec_t = tock_ms(); print_null(sum);

		tick();
		sum = 0;
		for (auto it = devec.begin(); it != devec.end(); ++it)
			sum += *it;
		devec_t = tock_ms(); print_null(sum);

		tick();
		sum2 = 0;
		auto end = tvec.cend();
		for (auto it = tvec.cbegin(); it != end; ++it) {
			sum2 += *it;
		}
		tvec_t = tock_ms(); print_null(sum2);

		SEQ_TEST(sum == sum2);
		std::cout << f("iterate iterators", vec_t, deq_t, tvec_t, devec_t) << std::endl;



		tick();
		deq.resize(deq.size() / 10);
		deq_t = tock_ms(); 

		tick();
		vec.resize(vec.size() / 10);
		vec_t = tock_ms();

		tick();
		devec.resize(devec.size() / 10);
		devec_t = tock_ms();

		tick();
		tvec.resize(tvec.size() / 10);
		tvec_t = tock_ms(); 

		assert_equal(deq, tvec);
		assert_equal(deq, devec);
		std::cout << f("resize to lower", vec_t, deq_t, tvec_t, devec_t) << std::endl;


		tick();
		deq.resize(count, 0);
		deq_t = tock_ms();

		tick();
		vec.resize(count, 0);
		vec_t = tock_ms();

		tick();
		devec.resize(count, 0);
		devec_t = tock_ms();

		tick();
		tvec.resize(count, 0);
		tvec_t = tock_ms();

		assert_equal(deq, tvec);
		assert_equal(deq, devec);
		std::cout << f("resize to upper", vec_t, deq_t, tvec_t, devec_t) << std::endl;

		{
			tick();
			std::deque<type> d2 = deq;
			deq_t = tock_ms();

			tick();
			std::vector<type> v2 = vec;
			vec_t = tock_ms();

			tick();
			seq::devector<type> de2 = devec;
			devec_t = tock_ms();

			tick();
			deque_type dd2 = tvec;
			tvec_t = tock_ms();

			assert_equal(d2, dd2);
			assert_equal(d2, de2);
			std::cout << f("copy construct", vec_t, deq_t, tvec_t, devec_t) << std::endl;
		}

		assert_equal(deq, tvec);
		
		{
			std::vector<type> tmp = vec;

			tick();
			deq.insert(deq.begin() + (deq.size() * 2) / 5, tmp.begin(), tmp.end());
			deq_t = tock_ms();

			tick();
			vec.insert(vec.begin() + (vec.size() * 2) / 5, tmp.begin(), tmp.end());
			vec_t = tock_ms();

			tick();
			devec.insert(devec.begin() + (devec.size() * 2) / 5, tmp.begin(), tmp.end());
			devec_t = tock_ms();

			tick();
			tvec.insert(tvec.begin() + (tvec.size() * 2) / 5, tmp.begin(), tmp.end());
			tvec_t = tock_ms();

			assert_equal(deq, tvec);
			assert_equal(deq, devec);
			std::cout << f("insert range left side", vec_t, deq_t, tvec_t, devec_t) << std::endl;

			
			deq.resize(count);
			tvec.resize(count);
			vec.resize(count);
			devec.resize(count);
			
			assert_equal(deq, tvec);
			assert_equal(deq, devec);
			
			//TODO

			tick();
			deq.insert(deq.begin() + (deq.size() * 3) / 5, tmp.begin(), tmp.end());
			deq_t = tock_ms();

			tick();
			vec.insert(vec.begin() + (vec.size() * 3) / 5, tmp.begin(), tmp.end());
			vec_t = tock_ms();

			tick();
			devec.insert(devec.begin() + (devec.size() * 3) / 5, tmp.begin(), tmp.end());
			devec_t = tock_ms();

			tick();
			tvec.insert(tvec.begin() + (tvec.size() * 3) / 5, tmp.begin(), tmp.end());
			tvec_t = tock_ms();

			assert_equal(deq, tvec);
			assert_equal(deq, devec);
			std::cout << f("insert range right side", vec_t, deq_t, tvec_t, devec_t) << std::endl;

			deq.resize(count);
			vec.resize(count);
			tvec.resize(count);
			devec.resize(count);
		}



		{
			for (size_t i = 0; i < deq.size(); ++i) {
				deq[i] = vec[i] = tvec[i] = devec[i] =i;
			}
			assert_equal(deq, tvec);
			assert_equal(deq, devec);
			
			tick();
			deq.erase(deq.begin() + deq.size() / 4, deq.begin() + deq.size() / 2);
			deq_t = tock_ms();

			tick();
			vec.erase(vec.begin() + vec.size() / 4, vec.begin() + vec.size() / 2);
			vec_t = tock_ms();

			tick();
			devec.erase(devec.begin() + devec.size() / 4, devec.begin() + devec.size() / 2);
			devec_t = tock_ms();

			tick();
			tvec.erase(tvec.begin() + tvec.size() / 4, tvec.begin() + tvec.size() / 2);
			tvec_t = tock_ms();

			assert_equal(deq, tvec);
			assert_equal(deq, devec);
			std::cout << f("erase range left side", vec_t, deq_t, tvec_t, devec_t) << std::endl;

			deq.resize(count, 0);
			vec.resize(count, 0);
			tvec.resize(count, 0);
			devec.resize(count, 0);
			

			tick();
			deq.erase(deq.begin() + deq.size() / 2, deq.begin() + deq.size() * 3 / 4);
			deq_t = tock_ms();

			tick();
			vec.erase(vec.begin() + vec.size() / 2, vec.begin() + vec.size() * 3 / 4);
			vec_t = tock_ms();

			tick();
			devec.erase(devec.begin() + devec.size() / 2, devec.begin() + devec.size() * 3 / 4);
			devec_t = tock_ms();

			tick();
			tvec.erase(tvec.begin() + tvec.size() / 2, tvec.begin() + tvec.size() * 3 / 4);
			tvec_t = tock_ms();

			assert_equal(deq, devec);
			assert_equal(deq, tvec);
			std::cout << f("erase range right side", vec_t, deq_t, tvec_t, devec_t) << std::endl;
		}

		{
			std::vector<type> tmp(count);
			for (size_t i = 0; i < tmp.size(); ++i) tmp[i] = i;

			deq.resize(count / 2, 0);
			vec.resize(count / 2, 0);
			tvec.resize(count / 2, 0);
			devec.resize(count / 2, 0);
			
			tick();
			deq.assign(tmp.begin(), tmp.end());
			deq_t = tock_ms();

			tick();
			vec.assign(tmp.begin(), tmp.end());
			vec_t = tock_ms();

			tick();
			devec.assign(tmp.begin(), tmp.end());
			devec_t = tock_ms();

			tick();
			tvec.assign(tmp.begin(), tmp.end());
			tvec_t = tock_ms();

			assert_equal(deq, tvec);
			assert_equal(deq, devec);
			std::cout << f("assign grow random access", vec_t, deq_t, tvec_t, devec_t) << std::endl;

			deq.resize(count * 2, 0);
			vec.resize(count * 2, 0);
			tvec.resize(count * 2, 0);
			devec.resize(count * 2, 0);
			
			tick();
			deq.assign(tmp.begin(), tmp.end());
			deq_t = tock_ms();

			tick();
			vec.assign(tmp.begin(), tmp.end());
			vec_t = tock_ms();

			tick();
			devec.assign(tmp.begin(), tmp.end());
			devec_t = tock_ms();

			tick();
			tvec.assign(tmp.begin(), tmp.end());
			tvec_t = tock_ms();

			assert_equal(deq, tvec);
			assert_equal(deq, devec);
			std::cout << f("assign shrink random access", vec_t, deq_t, tvec_t, devec_t) << std::endl;
		}
		{
			std::list<type> lst;
			for (size_t i = 0; i < count; ++i)
				lst.push_back(i);

			deq.resize(lst.size() / 2, 0);
			vec.resize(lst.size() / 2, 0);
			tvec.resize(lst.size() / 2, 0);
			devec.resize(lst.size() / 2, 0);
			
			tick();
			deq.assign(lst.begin(), lst.end());
			deq_t = tock_ms();

			tick();
			vec.assign(lst.begin(), lst.end());
			vec_t = tock_ms();

			tick();
			devec.assign(lst.begin(), lst.end());
			devec_t = tock_ms();

			tick();
			tvec.assign(lst.begin(), lst.end());
			tvec_t = tock_ms();

			assert_equal(deq, tvec);
			assert_equal(deq, devec);
			std::cout << f("assign grow forward iterator", vec_t, deq_t, tvec_t, devec_t) << std::endl;

			deq.resize(lst.size() * 2, 0);
			vec.resize(lst.size() * 2, 0);
			tvec.resize(lst.size() * 2, 0);
			devec.resize(lst.size() * 2, 0);
			
			tick();
			deq.assign(lst.begin(), lst.end());
			deq_t = tock_ms();

			tick();
			vec.assign(lst.begin(), lst.end());
			vec_t = tock_ms();

			tick();
			devec.assign(lst.begin(), lst.end());
			devec_t = tock_ms();

			tick();
			tvec.assign(lst.begin(), lst.end());
			tvec_t = tock_ms();

			assert_equal(deq, tvec);
			assert_equal(deq, devec);
			std::cout << f("assign shrink forward iterator", vec_t, deq_t, tvec_t, devec_t) << std::endl;
		}


		deq.resize(count, 0);
		vec.resize(count, 0);
		tvec.resize(count, 0);
		devec.resize(count, 0);
		assert_equal(deq, tvec);
		assert_equal(deq, devec);
		
		//fill again, backward
		for (size_t i = 0; i < deq.size(); ++i) {
			deq[i] = deq.size() - i - 1;
			vec[i] = vec.size() - i - 1;
			tvec[i] = tvec.size() - i - 1;
			devec[i] = tvec.size() - i - 1;
		}


		tick();
		while (deq.size() > 25)
			deq.pop_back();
		deq_t = tock_ms();

		tick();
		while (vec.size() > 25)
			vec.pop_back();
		vec_t = tock_ms();

		tick();
		while (devec.size() > 25)
			devec.pop_back();
		devec_t = tock_ms();

		tick();
		while (tvec.size() > 25)
			tvec.pop_back();
		tvec_t = tock_ms();

		assert_equal(deq, tvec);
		assert_equal(deq, devec);
		std::cout << f("pop_back", vec_t, deq_t, tvec_t, devec_t) << std::endl;



		deq.resize(count, 0);
		tvec.resize(count, 0);
		vec.resize(count, 0);
		devec.resize(count, 0);
		assert_equal(deq, tvec);
		assert_equal(deq, devec);
		
		//fill again, backward
		for (size_t i = 0; i < deq.size(); ++i) {
			deq[i] = deq.size() - i - 1;
			vec[i] = vec.size() - i - 1;
			tvec[i] = tvec.size() - i - 1;
			devec[i] = tvec.size() - i - 1;
		}


		assert_equal(deq, tvec);
		assert_equal(deq, devec);
		
		tick();
		while (deq.size() > count/2) {
			deq.pop_front();
		}
		deq_t = tock_ms();

		tick();
		while (devec.size() > count / 2) {
			devec.pop_front();
		}
		devec_t = tock_ms();

		tick();
		while (tvec.size() > count / 2)
			tvec.pop_front();
		tvec_t = tock_ms();

		assert_equal(deq, tvec);
		assert_equal(deq, devec);
		std::cout << f("pop_front", 1000000000ULL, deq_t, tvec_t,devec_t) << std::endl;




		size_t insert_count = std::max((size_t)50, count / 100);
		std::vector<size_t> in_pos;
		size_t ss = deq.size();
		srand(0);
		for (size_t i = 0; i < insert_count; ++i)
			in_pos.push_back((size_t)rand() % ss++);


		tick();
		for (size_t i = 0; i < insert_count; ++i) {
			deq.insert(deq.begin() + in_pos[i], i);
		}
		deq_t = tock_ms();

		tick();
		for (size_t i = 0; i < insert_count; ++i) {
			devec.insert(devec.begin() + in_pos[i], i);
		}
		devec_t = tock_ms();

		tick();
		for (size_t i = 0; i < insert_count; ++i) {
			tvec.insert(tvec.begin() + in_pos[i], i);
		}
		tvec_t = tock_ms();

		assert_equal(deq, tvec);
		assert_equal(deq, devec);
		std::cout << f("insert random position", 1000000000ULL, deq_t, tvec_t, devec_t) << std::endl;




		deq.resize(count, 0);
		tvec.resize(count, 0);
		devec.resize(count, 0);
		
		//fill again, backward
		for (size_t i = 0; i < deq.size(); ++i) {
			deq[i] = deq.size() - i - 1;
			tvec[i] = tvec.size() - i - 1;
			devec[i] = tvec.size() - i - 1;
		}

		size_t erase_count = deq.size() / 20;//std::min((size_t)5000000, deq.size());
		std::vector<size_t> er_pos;
		size_t sss = count;
		srand(0);
		for (size_t i = 0; i < erase_count; ++i)
			er_pos.push_back((size_t)rand() % sss--);


		tick();
		for (size_t i = 0; i < erase_count; ++i) {
			deq.erase(deq.begin() + er_pos[i]);
		}
		deq_t = tock_ms();

		tick();
		for (size_t i = 0; i < erase_count; ++i) {
			devec.erase(devec.begin() + er_pos[i]);
		}
		devec_t = tock_ms();

		tick();
		for (size_t i = 0; i < erase_count; ++i) {
			tvec.erase(tvec.begin() + er_pos[i]);
		}
		tvec_t = tock_ms();

		assert_equal(deq, tvec);
		assert_equal(deq, devec);
		std::cout << f("erase random position", 1000000000ULL, deq_t, tvec_t, devec_t) << std::endl;
	}


}



int bench_tiered_vector(int, char** const)
{
	tick();
	size_t e = tock_ms();
	std::cout << e << std::endl;
	test_tiered_vector_algorithms<size_t>(5000000);
	test_tiered_vector<size_t>(10000000); 
	 
	return 0;
}




