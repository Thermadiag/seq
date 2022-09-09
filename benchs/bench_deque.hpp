#pragma once

#include "tiered_vector.hpp"
#include "testing.hpp"
#include "format.hpp"

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
	if(d1.front() != d2.front())
		throw std::runtime_error("different front!");
	if (d1.back() != d2.back()) {
		auto v1 = &d1.back();
		auto v2 = &d2.back();
		throw std::runtime_error("different back!");
	}
	auto it1 = d1.begin();
	auto it2 = d2.begin();
	while (it1 != d1.end())
	{
		if (*it1 != *it2) {
			throw std::runtime_error("");
		}
		++it1;
		++it2;
	}
}


template< class T>
void test_deque_algorithms()
{
	//TEST sort
	{
		typedef T type;
		typedef tiered_vector<type, std::allocator<type> > deque_type;

		deque_type  tvec;
		std::deque<type> deq;
		std::vector<type> vec;
		srand(0);// time(NULL));
		int cc = 5000000;
		for (int i = 0; i < cc; ++i) {
			int r = rand();
			deq.push_back(r);
			tvec.push_back(r);
			vec.push_back(r);
		}

		std::cout << fmt(fmt("algorithm").c(20), "|", fmt("std::vector").c(20), "|", fmt("std::deque").c(20), "|", fmt("seq::tiered_vector").c(20), "|") << std::endl;
		std::cout << fmt(str().c(20).f('-'), "|", str().c(20).f('-'), "|", str().c(20).f('-'), "|", str().c(20).f('-'), "|");

		auto f = fmt(pos<0,2,4,6>(), str().c(20), "|", fmt(pos<0>(), size_t(), " ms").c(20), "|", fmt(pos<0>(), size_t(), " ms").c(20), "|", fmt(pos<0>(), size_t(), " ms").c(20), "|");

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
		std::cout << f("std::sort",vec_t, deq_t, tvec_t) << std::endl;

		
		tick();
		std::unique(vec.begin(), vec.end());
		size_t vec_t = tock_ms();

		tick();
		std::unique(deq.begin(), deq.end());
		size_t deq_t = tock_ms();

		tick();
		std::unique(tvec.begin(), tvec.end());
		size_t tvec_t = tock_ms();

		assert_equal(deq, tvec);
		std::cout << f("std::unique",vec_t, deq_t, tvec_t) << std::endl;

		for (int i = 0; i < cc; ++i)
			tvec[i] =deq[i] = vec[i] = rand();

		tick();
		std::rotate(vec.begin(), vec.begin() + vec.size() / 2, vec.end());
		size_t vec_t = tock_ms();

		tick();
		std::rotate(deq.begin(), deq.begin() + deq.size() / 2, deq.end());
		size_t deq_t = tock_ms();

		tick();
		std::rotate(tvec.begin(), tvec.begin() + tvec.size() / 2, tvec.end());
		size_t tvec_t = tock_ms();

		assert_equal(deq, tvec);
		std::cout << f("std::rotate",vec_t, deq_t, tvec_t) << std::endl;



		tick();
		std::reverse(vec.begin(), vec.end());
		size_t vec_t = tock_ms();

		tick();
		std::reverse(deq.begin(), deq.end());
		size_t deq_t = tock_ms();

		tick();
		std::reverse(tvec.begin(), tvec.end());
		size_t tvec_t = tock_ms();

		assert_equal(deq, tvec);
		std::cout << f("std::reverse",vec_t, deq_t, tvec_t) << std::endl;

		for (int i = 0; i < cc; ++i)
			tvec[i] = deq[i] = vec[i] = rand();

		tick();
		std::partial_sort(vec.begin(), vec.begin() + vec.size() / 2, vec.end());
		size_t vec_t = tock_ms();

		tick();
		std::partial_sort(deq.begin(), deq.begin() + deq.size() / 2, deq.end());
		size_t deq_t = tock_ms();

		tick();
		std::partial_sort(tvec.begin(), tvec.begin() + tvec.size() / 2, tvec.end());
		size_t tvec_t = tock_ms();

		assert_equal(deq, tvec);
		std::cout << f("std::partial_sort",vec_t, deq_t, tvec_t) << std::endl;

		for (int i = 0; i < cc; ++i)
			tvec[i] = deq[i] = vec[i] = rand();

		tick();
		std::nth_element(vec.begin(), vec.begin() + vec.size() / 2, vec.end());
		size_t vec_t = tock_ms();

		tick();
		std::nth_element(deq.begin(), deq.begin() + deq.size() / 2, deq.end());
		size_t deq_t = tock_ms();

		tick();
		std::nth_element(tvec.begin(), tvec.begin() + tvec.size() / 2, tvec.end());
		size_t tvec_t = tock_ms();

		assert_equal(deq, tvec);
		std::cout << f("std::nth_element", vec_t, deq_t, tvec_t) << std::endl;
	}
}





template<class T>
void test_deque()
{
	//for (size_t count = _ITERATIONS; count <= _ITERATIONS; count *= 10)
	size_t count = _ITERATIONS ;
	{
		_print("Start count %i\n", (int)count);

		typedef T type;
		std::vector<type> vec;
		std::deque<type/*, mem_pool_allocator<type>*/ > deq;
		typedef deque<type, std::allocator<T>, OptimizeForMemory > deque_type;
		deque_type deqq;

		
		long long st, el;


		st = msecsSinceEpoch();
		for (int i = 0; i < count; ++i)
			deq.push_back(i);
		el = msecsSinceEpoch() - st;
		_print("deq: %i ms\n", (int)el);

		st = msecsSinceEpoch();
		vec.reserve(count);
		for (int i = 0; i < count; ++i)
			vec.push_back(i);
		el = msecsSinceEpoch() - st;
		_print("vec: %i ms\n", (int)el);

		st = msecsSinceEpoch();
		for (int i = 0; i < count; ++i)
			deqq.push_back(i);
		el = msecsSinceEpoch() - st;
		_print("deqq: %i ms \n", (int)el);

		assert_equal(deq, deqq);

		_print("\n");

		int sum = 0;
		st = msecsSinceEpoch();
		for (int i = 0; i < count; ++i)
			sum += deq[i];
		el = msecsSinceEpoch() - st;
		_print("walk deq: %i ms   %i\n", (int)el, sum);

		sum = 0;
		st = msecsSinceEpoch();
		for (typename std::deque<type>::iterator it = deq.begin(); it != deq.end(); ++it)
			sum += *it;
		el = msecsSinceEpoch() - st;
		_print("walk deq it: %i ms   %i\n", (int)el, sum);


		st = msecsSinceEpoch();
		sum = 0;
		for (int i = 0; i < count; ++i)
			sum += vec[i];
		el = msecsSinceEpoch() - st;
		_print("walk vec: %i ms   %i\n", (int)el, sum);


		st = msecsSinceEpoch();
		sum = 0;
		for (int i = 0; i < count; ++i)
			sum += deqq[i];
		el = msecsSinceEpoch() - st;
		_print("walk deqq: %i ms  %i\n", (int)el, sum);


		st = msecsSinceEpoch();
		sum = 0;
		for (typename deque_type::const_iterator it = deqq.begin(); it != deqq.end(); ++it)
			sum += *it;
		el = msecsSinceEpoch() - st;
		_print("walk deqq it: %i ms  %i\n", (int)el, sum);

		st = msecsSinceEpoch();
		sum = 0;
		auto end = deqq.cend();
		for (auto it = deqq.cbegin(); it != end; ++it) {
			sum += *it;
		}
		el = msecsSinceEpoch() - st;
		_print("walk deqq it2: %i ms  %i\n", (int)el, sum);

		//return ;
		test_count;

		st = msecsSinceEpoch();
		deq.resize(deq.size() / 10);
		el = msecsSinceEpoch() - st;
		_print("deq resize lower: %i ms\n", (int)el);

		st = msecsSinceEpoch();
		deqq.resize(deqq.size() / 10);
		el = msecsSinceEpoch() - st;
		_print("deqq resize lower: %i ms\n", (int)el);

		test_count;
		assert_equal(deq, deqq);

		st = msecsSinceEpoch();
		deq.resize(count,0);
		el = msecsSinceEpoch() - st;
		_print("deq resize upper: %i ms\n", (int)el);

		st = msecsSinceEpoch();
		deqq.resize(count,0);
		el = msecsSinceEpoch() - st;
		_print("deqq resize upper: %i ms\n", (int)el);

		test_count;
		assert_equal(deq, deqq);

		{
			st = msecsSinceEpoch();
			std::deque<type/*, mem_pool_allocator<T> */> d2 = deq;
			el = msecsSinceEpoch() - st;
			_print("deq copy: %i ms\n", (int)el);

			st = msecsSinceEpoch();
			deque_type dd2 = deqq;
			el = msecsSinceEpoch() - st;
			_print("deqq copy: %i ms\n", (int)el);

			assert_equal(d2, dd2);
		}

		assert_equal(deq, deqq);

		{

			st = msecsSinceEpoch();
			deq.insert(deq.begin() + (deq.size() * 2) / 5, vec.begin(), vec.end());
			el = msecsSinceEpoch() - st;
			_print("deq insert range L: %i ms\n", (int)el);

			st = msecsSinceEpoch();
			deqq.insert(deqq.begin() + (deqq.size() * 2) / 5, vec.begin(), vec.end());
			//TODO: check resize_front provoque a crash after
			// put back iterator instead of iterator in erase and insert
			el = msecsSinceEpoch() - st;
			_print("deqq insert range L: %i ms\n", (int)el);

			assert_equal(deq, deqq);
			deq.resize(count);
			deqq.resize(count);
			assert_equal(deq, deqq);

			st = msecsSinceEpoch();
			deq.insert(deq.begin() + (deq.size() * 3) / 5, vec.begin(), vec.end());
			el = msecsSinceEpoch() - st;
			_print("deq insert range R: %i ms\n", (int)el);

			st = msecsSinceEpoch();
			deqq.insert(deqq.begin() + (deqq.size() * 3) / 5, vec.begin(), vec.end());
			el = msecsSinceEpoch() - st;
			_print("deqq insert range R: %i ms\n", (int)el);

			assert_equal(deq, deqq);
			deq.resize(count);
			deqq.resize(count);
		}


		{
			for (int i = 0; i < deq.size(); ++i) {
				deq[i] = deqq[i] = i;
			}
			assert_equal(deq, deqq);

			st = msecsSinceEpoch();
			deq.erase(deq.begin() + deq.size() / 4, deq.begin() + deq.size() / 2);
			el = msecsSinceEpoch() - st;
			_print("deq erase range L: %i ms\n", (int)el);

			st = msecsSinceEpoch();
			deqq.erase(deqq.size() / 4, deqq.size() / 2);
			el = msecsSinceEpoch() - st;
			_print("deqq erase range L: %i ms\n", (int)el);
			assert_equal(deq, deqq);

			deq.resize(count,0);
			deqq.resize(count,0);

			st = msecsSinceEpoch();
			deq.erase(deq.begin() + deq.size() / 2, deq.begin() + deq.size() * 3 / 4);
			el = msecsSinceEpoch() - st;
			_print("deq erase range R: %i ms\n", (int)el);

			st = msecsSinceEpoch();
			deqq.erase(deqq.size() / 2, deqq.size() * 3 / 4);
			el = msecsSinceEpoch() - st;
			_print("deqq erase range R: %i ms\n", (int)el);
			assert_equal(deq, deqq);
		}

		{
			deq.resize(vec.size() / 2,0);
			deqq.resize(vec.size() / 2,0);

			st = msecsSinceEpoch();
			deq.assign(vec.begin(), vec.end());
			el = msecsSinceEpoch() - st;
			_print("deq assign random grow: %i ms\n", (int)el);

			st = msecsSinceEpoch();
			deqq.assign(vec.begin(), vec.end());
			el = msecsSinceEpoch() - st;
			_print("deqq assign random grow: %i ms\n", (int)el);

			assert_equal(deq, deqq);

			deq.resize(vec.size() * 2,0);
			deqq.resize(vec.size() * 2,0);

			st = msecsSinceEpoch();
			deq.assign(vec.begin(), vec.end());
			el = msecsSinceEpoch() - st;
			_print("deq assign random shrink: %i ms\n", (int)el);

			st = msecsSinceEpoch();
			deqq.assign(vec.begin(), vec.end());
			el = msecsSinceEpoch() - st;
			_print("deqq assign random shrink: %i ms\n", (int)el);

			assert_equal(deq, deqq);
		}
		{
			std::list<type> lst;
			for (int i = 0; i < count; ++i)
				lst.push_back(i);

			deq.resize(lst.size() / 2,0);
			deqq.resize(lst.size() / 2,0);

			st = msecsSinceEpoch();
			deq.assign(lst.begin(), lst.end());
			el = msecsSinceEpoch() - st;
			_print("deq assign forward grow: %i ms\n", (int)el);

			st = msecsSinceEpoch();
			deqq.assign(lst.begin(), lst.end());
			el = msecsSinceEpoch() - st;
			_print("deqq assign forward grow: %i ms\n", (int)el);

			assert_equal(deq, deqq);
			deq.resize(lst.size() * 2,0);
			deqq.resize(lst.size() * 2,0);

			st = msecsSinceEpoch();
			deq.assign(lst.begin(), lst.end());
			el = msecsSinceEpoch() - st;
			_print("deq assign forward shrink: %i ms\n", (int)el);

			st = msecsSinceEpoch();
			deqq.assign(lst.begin(), lst.end());
			el = msecsSinceEpoch() - st;
			_print("deqq assign forward shrink: %i ms\n", (int)el);

			assert_equal(deq, deqq);
		}

		deq.resize(count,0);
		deqq.resize(count,0);
		assert_equal(deq, deqq);

		//fill again, backward
		for (int i = 0; i < deq.size(); ++i) {
			deq[i] = deq.size() - i - 1;
			deqq[i] = deqq.size() - i - 1;
		}


		st = msecsSinceEpoch();
		while (deq.size() > 25)
			deq.pop_back();
		el = msecsSinceEpoch() - st;
		_print("deq pop_back: %i ms\n", (int)el);

		st = msecsSinceEpoch();
		while (deqq.size() > 25)
			deqq.pop_back();
		el = msecsSinceEpoch() - st;
		_print("deqq pop_back: %i ms\n", (int)el);

		assert_equal(deq, deqq);

		deq.resize(count,0);
		deqq.resize(count,0);

		assert_equal(deq, deqq);

		//fill again, backward
		for (int i = 0; i < deq.size(); ++i) {
			deq[i] = deq.size() - i - 1;
			deqq[i] = deqq.size() - i - 1;
		}


		assert_equal(deq, deqq);

		st = msecsSinceEpoch();
		while (deq.size() > 25) {
			deq.pop_front();
		}
		el = msecsSinceEpoch() - st;
		_print("deq pop_front: %i ms\n", (int)el);

		st = msecsSinceEpoch();
		while (deqq.size() > 25)
			deqq.pop_front();
		el = msecsSinceEpoch() - st;
		_print("deqq pop_front: %i ms\n", (int)el);

		assert_equal(deq, deqq);


		/*st = msecsSinceEpoch();
		for (int i = 0; i < 110; ++i) {
			deq.insert(deq.begin() + deq.size()/2, i);
		}
		el = msecsSinceEpoch() - st;
		_print("deq insert: %i ms\n", (int)el);
		print_deque(deq);*/

		//TEST 
		{
			deque_type d;
			std::deque<int> dd;
			d.resize(128 * 3,0);
			dd.resize(128 * 3,0);
			for (int i = 0; i < d.size(); ++i) {
				d[i] = dd[i] = i;
			}
			assert_equal(d, dd);
			d.insert(10, -1);
			dd.insert(dd.begin() + 10, -1);
			assert_equal(d, dd);
			for (int i = 0; i < 128; ++i) {
				d.erase(0);
				dd.erase(dd.begin());
				assert_equal(d, dd);
			}
			assert_equal(d, dd);
			d.erase(0);
			dd.erase(dd.begin());
			assert_equal(d, dd);
		}


		int insert_count = std::max((size_t)50, count / 100);
		std::vector<size_t> in_pos;
		int ss=deq.size();
		srand(NULL);
		for(int i=0; i< insert_count; ++i)
			in_pos.push_back(rand() % ss++);


		st = msecsSinceEpoch();
		for (int i = 0; i < insert_count; ++i) {
			deq.insert(deq.begin() + in_pos[i], i);
		}
		el = msecsSinceEpoch() - st;
		_print("deq insert %i values: %i ms\n", insert_count, (int)el);


		st = msecsSinceEpoch();
		for (int i = 0; i < insert_count; ++i) {
			deqq.insert(in_pos[i], i);
			/*printf("insert at %i, %i\n", in_pos[i], i);
			if (i == 3)
				bool stop = true;
			deqq.insert(in_pos[i], i);
			for (int j = 0; j < deqq.size(); ++j) printf("%i ", deqq[j]);
			printf("\n");
			deq.insert(deq.begin() + in_pos[i], i);
			assert_equal(deq, deqq);*/
		}
		el = msecsSinceEpoch() - st;
		_print("deqq insert %i values: %i ms\n", insert_count, (int)el);

		assert_equal(deq, deqq);


		//TEST erase
		{
			deque_type d;
			std::deque<int> dd;
			d.resize(100,0);
			dd.resize(100,0);
			for (int i = 0; i < d.size(); ++i) {
				d[i] = dd[i] = i;
			}

			for (int i = 0; i < 50; ++i) {
				int pos = i % 5;
				pos = d.size() * pos / 4;
				if (pos == d.size()) --pos;
				dd.erase(dd.begin() + pos);
				d.erase(pos);
				assert_equal(d, dd);
			}
		}



		deq.resize(count,0);
		deqq.resize(count,0);

		st = msecsSinceEpoch();
		deq.shrink_to_fit();
		el = msecsSinceEpoch() - st;
		_print("deqq shrink_to_fit: %i ms\n", (int)el);

		//fill again, backward
		for (int i = 0; i < deq.size(); ++i) {
			deq[i] = deq.size() - i - 1;
			deqq[i] = deqq.size() - i - 1;
		}

		int erase_count = std::min((size_t)5000000, deq.size());
		std::vector<size_t> er_pos;
		int sss=count;
		srand(NULL);
		for(int i=0; i< erase_count; ++i)
			er_pos.push_back(rand() % sss--);


		st = msecsSinceEpoch();
		for (int i = 0; i < erase_count; ++i) {
			/*int pos = i % 5;
			pos = deq.size() * pos / 4;
			if (pos == deq.size()) --pos;
			else if (pos == 0) pos = 1;*/
			deq.erase(deq.begin() + er_pos[i]);
		}
		el = msecsSinceEpoch() - st;
		_print("deq erase %i values rem %i: %i ms\n", erase_count,(int)deq.size(),(int)el);

		st = msecsSinceEpoch();
		for (int i = 0; i < erase_count; ++i) {
			/*int pos = i % 5;
			pos = deqq.size() * pos / 4;
			if (pos == deqq.size()) --pos;
			else if (pos == 0) pos = 1;*/
			//_print("erase %i for %i\n", pos, (int)deqq.size());
			deqq.erase( er_pos[i]);
		}
		el = msecsSinceEpoch() - st;
		_print("deqq erase %i values rem %i: %i ms\n", erase_count, (int)deqq.size(),(int)el);

		assert_equal(deq, deqq);

	}


	int c = test_count;
	printf("\ncount: %i\n", c);
	bool stop = true;

}

















template<class Deq1, class Deq2>
void assert_equal_ptr(const Deq1& d1, const Deq2& d2)
{
	//#ifdef ENABLE_ASSERT
	if (d1.size() != d2.size())
		throw std::runtime_error("different size!");
	if (d1.size() == 0)
		return;
	if (d1.front() || d2.front()) {
		auto v1 = &d1.front();
		auto v2 = &d2.front();
		if (*d1.front() != *d2.front())
			throw std::runtime_error("different front!");
	}
	if (d1.back() || d2.back())
		if (*d1.back() != *d2.back())
			throw std::runtime_error("different back!");
	for (int i = 0; i < d1.size(); ++i) {
		auto v1 = &d1[i];
		auto v2 = &d2[i];
		if ((d1[i] || d2[i]) && (*d1[i] != *d2[i])) {
			char msg[100];
			sprintf(msg, "different value at %i!", i);
			throw std::runtime_error(msg);
		}
	}
	//#endif
}


#ifndef _MSC_VER
/*namespace std
{
template< class T, class... Args >
unique_ptr<T> make_unique( Args&&... args )
{
	return unique_ptr<T>(new T(std::forward<Args>(args)...));
}
}*/
#endif

template<class T>
void test_deque_ptr()
{
	for (size_t count = 50; count <= _ITERATIONS; count *= 10)
	//size_t count = _ITERATIONS;
	{
		_print("Start count %i\n", (int)count);

		typedef std::unique_ptr < T> type;
		std::vector<type> vec;
		std::deque<type> deq;
		typedef deque<type, std::allocator<type>, OptimizeForMemory > deque_type;
		deque_type deqq;

		long long st, el;


		st = msecsSinceEpoch();
		for (int i = 0; i < count; ++i)
			deq.push_back(std::make_unique<T>(i));
		el = msecsSinceEpoch() - st;
		_print("deq: %i ms\n", (int)el);

		st = msecsSinceEpoch();
		for (int i = 0; i < count; ++i)
			vec.push_back(std::make_unique<T>(i));
		el = msecsSinceEpoch() - st;
		_print("vec: %i ms\n", (int)el);

		st = msecsSinceEpoch();
		for (int i = 0; i < count; ++i)
			deqq.push_back(std::make_unique<T>(i));
		el = msecsSinceEpoch() - st;
		_print("deqq: %i ms \n", (int)el);

		assert_equal_ptr(deq, deqq);

		_print("\n");

		int sum = 0;
		st = msecsSinceEpoch();
		for (int i = 0; i < count; ++i)
			sum += *deq[i];
		el = msecsSinceEpoch() - st;
		_print("walk deq: %i ms   %i\n", (int)el, sum);

		sum = 0;
		st = msecsSinceEpoch();
		for (typename std::deque<type>::iterator it = deq.begin(); it != deq.end(); ++it)
			sum += *(*it);
		el = msecsSinceEpoch() - st;
		_print("walk deq it: %i ms   %i\n", (int)el, sum);


		st = msecsSinceEpoch();
		sum = 0;
		for (int i = 0; i < count; ++i)
			sum += *vec[i];
		el = msecsSinceEpoch() - st;
		_print("walk vec: %i ms   %i\n", (int)el, sum);


		st = msecsSinceEpoch();
		sum = 0;
		for (int i = 0; i < count; ++i)
			sum += *deqq[i];
		el = msecsSinceEpoch() - st;
		_print("walk deqq: %i ms  %i\n", (int)el, sum);


		st = msecsSinceEpoch();
		sum = 0;
		for (typename deque_type::const_iterator it = deqq.begin(); it != deqq.end(); ++it)
			sum += *(*it);
		el = msecsSinceEpoch() - st;
		_print("walk deqq it: %i ms  %i\n", (int)el, sum);

		st = msecsSinceEpoch();
		sum = 0;
		auto end = deqq.cend();
		for (auto it = deqq.cbegin(); it != deqq.cend(); ++it) {
			sum += *(*it);
		}
		el = msecsSinceEpoch() - st;
		_print("walk deqq it2: %i ms  %i\n", (int)el, sum);

		//return 0;

		st = msecsSinceEpoch();
		deq.resize(deq.size() / 10);
		el = msecsSinceEpoch() - st;
		_print("deq resize lower: %i ms\n", (int)el);

		st = msecsSinceEpoch();
		deqq.resize(deqq.size() / 10);
		el = msecsSinceEpoch() - st;
		_print("deqq resize lower: %i ms\n", (int)el);

		assert_equal_ptr(deq, deqq);

		st = msecsSinceEpoch();
		deq.resize(count);
		el = msecsSinceEpoch() - st;
		_print("deq resize upper: %i ms\n", (int)el);

		st = msecsSinceEpoch();
		deqq.resize(count);
		el = msecsSinceEpoch() - st;
		_print("deqq resize upper: %i ms\n", (int)el);

		assert_equal_ptr(deq, deqq);


		{
			std::vector<type> vec2(vec.size());
			for (int i = 0; i < vec.size(); ++i) {
				vec2[i] = std::make_unique<T>(i);
			}

			st = msecsSinceEpoch();
			deq.insert(deq.begin() + (deq.size() * 2) / 5, std::make_move_iterator(vec.begin()), std::make_move_iterator(vec.end()));
			el = msecsSinceEpoch() - st;
			_print("deq insert range L: %i ms\n", (int)el);

			st = msecsSinceEpoch();
			deqq.insert(deqq.begin() + (deqq.size() * 2) / 5, std::make_move_iterator( vec2.begin()), std::make_move_iterator(vec2.end()));
			//TODO: check resize_front provoque a crash after
			// put back iterator instead of iterator in erase and insert
			el = msecsSinceEpoch() - st;
			_print("deqq insert range L: %i ms\n", (int)el);

			assert_equal_ptr(deq, deqq);
			deq.resize(count);
			deqq.resize(count);

			for (int i = 0; i < vec.size(); ++i) {
				vec[i] = std::make_unique<T>(i);
				vec2[i] = std::make_unique<T>(i);
			}

			st = msecsSinceEpoch();
			deq.insert(deq.begin() + (deq.size() * 3) / 5, std::make_move_iterator(vec.begin()), std::make_move_iterator(vec.end()));
			el = msecsSinceEpoch() - st;
			_print("deq insert range R: %i ms\n", (int)el);

			st = msecsSinceEpoch();
			deqq.insert(deqq.begin() + (deqq.size() * 3) / 5, std::make_move_iterator(vec2.begin()), std::make_move_iterator(vec2.end()));
			el = msecsSinceEpoch() - st;
			_print("deqq insert range R: %i ms\n", (int)el);

			assert_equal_ptr(deq, deqq);
			deq.resize(count);
			deqq.resize(count);
		}


		{
			for (int i = 0; i < deq.size(); ++i) {
				deq[i] = std::make_unique<T>(i);
				deqq[i] = std::make_unique<T>(i);
			}
			assert_equal_ptr(deq, deqq);

			st = msecsSinceEpoch();
			deq.erase(deq.begin() + deq.size() / 4, deq.begin() + deq.size() / 2);
			el = msecsSinceEpoch() - st;
			_print("deq erase range L: %i ms\n", (int)el);

			st = msecsSinceEpoch();
			deqq.erase(deqq.size() / 4, deqq.size() / 2);
			el = msecsSinceEpoch() - st;
			_print("deqq erase range L: %i ms\n", (int)el);
			assert_equal_ptr(deq, deqq);

			deq.resize(count);
			deqq.resize(count);

			st = msecsSinceEpoch();
			deq.erase(deq.begin() + deq.size() / 2, deq.begin() + deq.size() * 3 / 4);
			el = msecsSinceEpoch() - st;
			_print("deq erase range R: %i ms\n", (int)el);

			st = msecsSinceEpoch();
			deqq.erase(deqq.size() / 2, deqq.size() * 3 / 4);
			el = msecsSinceEpoch() - st;
			_print("deqq erase range R: %i ms\n", (int)el);
			assert_equal_ptr(deq, deqq);
		}

		/*{
			deq.resize(vec.size() / 2);
			deqq.resize(vec.size() / 2);

			st = msecsSinceEpoch();
			deq.assign(vec.begin(), vec.end());
			el = msecsSinceEpoch() - st;
			_print("deq assign random grow: %i ms\n", (int)el);

			st = msecsSinceEpoch();
			deqq.assign(vec.begin(), vec.end());
			el = msecsSinceEpoch() - st;
			_print("deqq assign random grow: %i ms\n", (int)el);

			assert_equal_ptr(deq, deqq);
			deq.resize(vec.size() * 2);
			deqq.resize(vec.size() * 2);

			st = msecsSinceEpoch();
			deq.assign(vec.begin(), vec.end());
			el = msecsSinceEpoch() - st;
			_print("deq assign random shrink: %i ms\n", (int)el);

			st = msecsSinceEpoch();
			deqq.assign(vec.begin(), vec.end());
			el = msecsSinceEpoch() - st;
			_print("deqq assign random shrink: %i ms\n", (int)el);

			assert_equal_ptr(deq, deqq);
		}
		{
			std::list<type> lst;
			for (int i = 0; i < count; ++i)
				lst.push_back(std::make_unique<int>(i));

			deq.resize(lst.size() / 2);
			deqq.resize(lst.size() / 2);

			st = msecsSinceEpoch();
			deq.assign(lst.begin(), lst.end());
			el = msecsSinceEpoch() - st;
			_print("deq assign forward grow: %i ms\n", (int)el);

			st = msecsSinceEpoch();
			deqq.assign(lst.begin(), lst.end());
			el = msecsSinceEpoch() - st;
			_print("deqq assign forward grow: %i ms\n", (int)el);

			assert_equal_ptr(deq, deqq);
			deq.resize(lst.size() * 2);
			deqq.resize(lst.size() * 2);

			st = msecsSinceEpoch();
			deq.assign(lst.begin(), lst.end());
			el = msecsSinceEpoch() - st;
			_print("deq assign forward shrink: %i ms\n", (int)el);

			st = msecsSinceEpoch();
			deqq.assign(lst.begin(), lst.end());
			el = msecsSinceEpoch() - st;
			_print("deqq assign forward shrink: %i ms\n", (int)el);

			assert_equal_ptr(deq, deqq);
		}*/

		deq.resize(count);
		deqq.resize(count);
		assert_equal_ptr(deq, deqq);

		//fill again, backward
		for (int i = 0; i < deq.size(); ++i) {
			deq[i] = std::make_unique<T>(deq.size() - i - 1);
			deqq[i] = std::make_unique<T>(deqq.size() - i - 1);
		}


		st = msecsSinceEpoch();
		while (deq.size() > 25)
			deq.pop_back();
		el = msecsSinceEpoch() - st;
		_print("deq pop_back: %i ms\n", (int)el);

		st = msecsSinceEpoch();
		while (deqq.size() > 25)
			deqq.pop_back();
		el = msecsSinceEpoch() - st;
		_print("deqq pop_back: %i ms\n", (int)el);

		assert_equal_ptr(deq, deqq);

		deq.resize(count);
		deqq.resize(count);

		assert_equal_ptr(deq, deqq);

		//fill again, backward
		for (int i = 0; i < deq.size(); ++i) {
			deq[i] = std::make_unique<T>(deq.size() - i - 1);
			deqq[i] = std::make_unique<T>(deqq.size() - i - 1);
		}


		assert_equal_ptr(deq, deqq);

		st = msecsSinceEpoch();
		while (deq.size() > 25) {
			deq.pop_front();
		}
		el = msecsSinceEpoch() - st;
		_print("deq pop_front: %i ms\n", (int)el);

		st = msecsSinceEpoch();
		while (deqq.size() > 25)
			deqq.pop_front();
		el = msecsSinceEpoch() - st;
		_print("deqq pop_front: %i ms\n", (int)el);

		assert_equal_ptr(deq, deqq);


		/*st = msecsSinceEpoch();
		for (int i = 0; i < 110; ++i) {
			deq.insert(deq.begin() + deq.size()/2, i);
		}
		el = msecsSinceEpoch() - st;
		_print("deq insert: %i ms\n", (int)el);
		print_deque(deq);*/

		int insert_count = std::max((size_t)50, count / 1000);
		st = msecsSinceEpoch();
		for (int i = 0; i < insert_count; ++i) {
			int pos = i % 5;
			pos = deq.size() * pos / 4;
			deq.insert(deq.begin() + pos, std::make_unique<T>(i));
		}
		el = msecsSinceEpoch() - st;
		_print("deq insert %i values: %i ms\n", insert_count, (int)el);


		st = msecsSinceEpoch();
		for (int i = 0; i < insert_count; ++i) {
			int pos = i % 5;
			pos = deqq.size() * pos / 4;
			deqq.insert(pos, std::make_unique<T>(i));
		}
		el = msecsSinceEpoch() - st;
		_print("deqq insert %i values: %i ms\n", insert_count, (int)el);

		assert_equal_ptr(deq, deqq);

		deq.resize(count);
		deqq.resize(count);

		//fill again, backward
		for (int i = 0; i < deq.size(); ++i) {
			deq[i] = std::make_unique<T>(deq.size() - i - 1);
			deqq[i] = std::make_unique<T>(deqq.size() - i - 1);
		}

		int erase_count = std::min((size_t)100, deq.size());
		st = msecsSinceEpoch();
		for (int i = 0; i < erase_count; ++i) {
			int pos = i % 5;
			pos = deq.size() * pos / 4;
			if (pos == deq.size()) --pos;
			else if (pos == 0) pos = 1;
			deq.erase(deq.begin() + pos);
		}
		el = msecsSinceEpoch() - st;
		_print("deq erase %i values: %i ms\n", erase_count, (int)el);

		st = msecsSinceEpoch();
		for (int i = 0; i < erase_count; ++i) {
			int pos = i % 5;
			pos = deqq.size() * pos / 4;
			if (pos == deqq.size()) --pos;
			else if (pos == 0) pos = 1;
			//_print("erase %i for %i\n", pos, (int)deqq.size());
			deqq.erase(pos);
		}
		el = msecsSinceEpoch() - st;
		_print("deqq erase %i values: %i ms\n", erase_count, (int)el);

		assert_equal_ptr(deq, deqq);

	}

	int c = test_count;
	printf("\ncount: %i\n", c);
	bool stop = true;

}
