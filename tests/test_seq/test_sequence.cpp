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



#include "seq/sequence.hpp"
#include "seq/tiered_vector.hpp"
#include "seq/testing.hpp"
#include "tests.hpp"
#include <list>
#include <vector>
#include <deque>


template<class Alloc, class U>
using RebindAlloc = typename std::allocator_traits<Alloc>::template rebind_alloc<U>;


template<class Deq1, class Deq2>
bool equal_seq(const Deq1& d1, const Deq2& d2)
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

template<class Deq>
bool is_sorted(Deq& d)
{
	if (d.empty())
		return true;
	auto first = d.begin();
	auto it = first; ++it;
	for (; it != d.end(); ++it, ++first)
	{
		if (*it < *first)
			return false;
	}
	return true;
}

struct WideType
{
	size_t data[16];
	WideType(size_t v = 0) {
		data[0] = v;
	}
	bool operator<(const WideType& other) const { return data[0] < other.data[0]; }
	bool operator==(const WideType& other) const { return data[0] == other.data[0]; }
	bool operator!=(const WideType& other) const { return data[0] != other.data[0]; }
};
inline bool operator==(const WideType& a, size_t b)  { return a.data[0] == b; }
inline bool operator!=(const WideType& a, size_t b)  { return a.data[0] != b; }
inline bool operator==(const size_t& a, WideType b) { return a == b.data[0]; }
inline bool operator!=(const size_t& a, WideType b) { return a != b.data[0]; }


template<class T, seq::LayoutManagement lay, class Alloc = std::allocator<T> >
void test_sequence(unsigned size = 50000000U, const Alloc & al = Alloc())
{
	using namespace seq;
	const unsigned count = size;

	{

		//test different memory layout with different data size
		using Al = RebindAlloc<Alloc, size_t>;
		Al a(al);

		using small_slow = sequence<size_t, Al, OptimizeForMemory>;
		using small_fast = sequence<size_t, Al, OptimizeForSpeed>;
		using big_slow = sequence<WideType, Al, OptimizeForMemory>;
		using big_fast = sequence<WideType, Al, OptimizeForSpeed>;

		small_slow ss(a);
		small_fast sf(a);
		big_slow bs(a);
		big_fast bf(a);

		size_t c =static_cast<size_t>( size / 10);

		for (size_t i = 0; i < c; ++i)
		{
			size_t v = static_cast<size_t>(i);
			ss.push_back(v);
			sf.push_back(v);
			bs.push_back(v);
			bf.push_back(v);
		}

		SEQ_TEST(equal_seq(ss,bs));
		SEQ_TEST(equal_seq(sf, bf));
		SEQ_TEST(equal_seq(ss, bf));

		std::vector<std::ptrdiff_t> erase_pos(c / 10);
		for (size_t i = 0; i < erase_pos.size(); ++i) {
			erase_pos[i] = static_cast<std::ptrdiff_t>(i);
		}
		seq::random_shuffle(erase_pos.begin(), erase_pos.end());
		seq::random_shuffle(erase_pos.begin(), erase_pos.end());
		seq::random_shuffle(erase_pos.begin(), erase_pos.end());

		//erase values
		for (size_t i = 0; i < erase_pos.size(); ++i) {
			ss.erase(ss.begin() + erase_pos[i]);
			sf.erase(sf.begin() + erase_pos[i]);
			bs.erase(bs.begin() + erase_pos[i]);
			bf.erase(bf.begin() + erase_pos[i]);
		}
		SEQ_TEST(equal_seq(ss, bs));
		SEQ_TEST(equal_seq(sf, bf));
		SEQ_TEST(equal_seq(ss, bf));

		//std::cout << "about to sort " << ss.size() << " values"<< std::endl;
		//sort 
		ss.sort();
		//std::cout << "sorted1" << std::endl;
		sf.sort();
		//std::cout << "sorted2" << std::endl;
		bs.sort();
		//std::cout << "sorted3" << std::endl;
		bf.sort();
		//std::cout << "sorted4" << std::endl;

		SEQ_TEST(is_sorted(ss));
		SEQ_TEST(is_sorted(sf));
		SEQ_TEST(is_sorted(bs));
		SEQ_TEST(is_sorted(bf));

		SEQ_TEST(equal_seq(ss, bs));
		SEQ_TEST(equal_seq(sf, bf));
		SEQ_TEST(equal_seq(ss, bf));
	}
	SEQ_TEST(get_alloc_bytes(al) == 0);

	typedef T type;
	std::vector<type> vec;
	typedef tiered_vector<type, std::allocator<T> > deque_type;
	deque_type deq;
	typedef sequence<type , Alloc ,lay> sequence_type;
	sequence_type seq(al);

	SEQ_TEST(seq.begin() == seq.end());
	SEQ_TEST(seq.size() == 0);
	
	seq.resize(10);
	SEQ_TEST(seq.size() == 10);
	seq.clear();
	SEQ_TEST(seq.size() == 0 && !seq.data());
	SEQ_TEST(get_alloc_bytes(al) == 0);
	// Test push_back

	for (unsigned i = 0; i < count; ++i)
		deq.push_back(static_cast<type>(i));

	vec.reserve(static_cast<size_t>(count));
	for (unsigned i = 0; i < count; ++i)
		vec.push_back(static_cast<type>(i));

	for (unsigned i = 0; i < count; ++i)
		seq.push_back(static_cast<type>(i));


	SEQ_TEST(equal_seq(deq, seq));

	// Test resize lower
	deq.resize(deq.size() / 10);
	seq.resize(seq.size() / 10);
		
	SEQ_TEST(equal_seq(deq, seq));

	// Test resize upper
	deq.resize(static_cast<size_t>(count), 0);
	seq.resize(static_cast<size_t>(count), 0);
		
	SEQ_TEST(equal_seq(deq, seq));


	size_t i = 0; 
	for (auto it = seq.begin(); it != seq.end(); ++it)
		*it = static_cast<T>(i++);
	i = 0;
	for (auto it = deq.begin(); it != deq.end(); ++it)
		*it = static_cast<T>(i++);

	// Test resize front lower
	deq.resize_front(deq.size() / 10);
	seq.resize_front(seq.size() / 10);

	SEQ_TEST(equal_seq(deq, seq));

	// Test resize front upper
	deq.resize_front(static_cast<size_t>(count), 0);
	seq.resize_front(static_cast<size_t>(count), 0);
		
	SEQ_TEST(equal_seq(deq, seq));

	{
		// Test copy construct
		deque_type d2 = deq;
		sequence_type dd2( seq,al);

		SEQ_TEST(equal_seq(d2, dd2));
	}

	SEQ_TEST(equal_seq(deq, seq));



	{
		{
			auto itd = deq.begin();
			auto its = seq.begin();
			type j = 0;
			while (itd != deq.end()) {
				*itd++ = j;
				*its++ = j;
				++j;
			}
		}
			
		SEQ_TEST(equal_seq(deq, seq));

		// Test erase range left side
		deq.erase(deq.begin() + deq.size() / 4, deq.begin() + deq.size() / 2);
		seq.erase(seq.begin() + seq.size() / 4, seq.begin() + seq.size() / 2);
		SEQ_TEST(equal_seq(deq, seq));

		deq.resize(static_cast<size_t>(count), 0);
		seq.resize(static_cast<size_t>(count), 0);

		// Test erase range right side
		deq.erase(deq.begin() + deq.size() / 2, deq.begin() + deq.size() * 3 / 4);
		seq.erase(seq.begin() + seq.size() / 2, seq.begin() + seq.size() * 3 / 4);
		SEQ_TEST(equal_seq(deq, seq));
	}

	{
		deq.resize(vec.size() / 2, 0);
		seq.resize(vec.size() / 2, 0);

		// Test assign grow
		deq.assign(vec.begin(), vec.end());
		seq.assign(vec.begin(), vec.end());
			
		SEQ_TEST(equal_seq(deq, seq));

		deq.resize(vec.size() * 2, 0);
		seq.resize(vec.size() * 2, 0);

		// Test assign shrink
		deq.assign(vec.begin(), vec.end());
		seq.assign(vec.begin(), vec.end());
			
		SEQ_TEST(equal_seq(deq, seq));
	}

	{
		std::list<type> lst;
		for (unsigned j = 0; j < count; ++j)
			lst.push_back(static_cast<type>(j));

		deq.resize(lst.size() / 2, 0);
		seq.resize(lst.size() / 2, 0);

		// Test assign shrink with non random access iterator
		deq.assign(lst.begin(), lst.end());
		seq.assign(lst.begin(), lst.end());
			
		SEQ_TEST(equal_seq(deq, seq));
		deq.resize(lst.size() * 2, 0);
		seq.resize(lst.size() * 2, 0);

		// Test assign grow with non random access iterator
		deq.assign(lst.begin(), lst.end());
		seq.assign(lst.begin(), lst.end());
			
		SEQ_TEST(equal_seq(deq, seq));
	}


	deq.resize(count, 0);
	seq.resize(count, 0);
	SEQ_TEST(equal_seq(deq, seq));

	// Test shrink_to_fit
	seq.shrink_to_fit();

	SEQ_TEST(equal_seq(deq, seq));

	//fill again, backward
	{
		auto itd = deq.begin();
		auto its = seq.begin();
		type j = static_cast<type>(deq.size())-1;
		while (itd != deq.end()) {
			*itd++ = j;
			*its++ = j;
			--j;
		}
	}

	SEQ_TEST(equal_seq(deq, seq));

	// Test pop_back
	while (deq.size() > 25)
		deq.pop_back();
	while (seq.size() > 25)
		seq.pop_back();
	SEQ_TEST(equal_seq(deq, seq));

	deq.resize(count, 0);
	seq.resize(count, 0);

	SEQ_TEST(equal_seq(deq, seq));

	//fill again, backward
	{
		auto itd = deq.begin();
		auto its = seq.begin();
		type j = static_cast<type>(deq.size()) - 1;
		while (itd != deq.end()) {
			*itd++ = j;
			*its++ = j;
			--j;
		}
	}



	SEQ_TEST(equal_seq(deq, seq));

	// Test pop_front
	while (deq.size() > 25) 
		deq.pop_front();
	while (seq.size() > 25)
		seq.pop_front();
	SEQ_TEST(equal_seq(deq, seq));


		
	//Test erase
	{
		sequence_type d(al);
		deque_type dd;
		d.resize(100, 0);
		dd.resize(100, 0);
		{
			auto itd = d.begin();
			auto its = dd.begin();
			type j = 0;
			while (itd != d.end()) {
				*itd++ = j;
				*its++ = j;
				++j;
			}
		}

		for (int j = 0; j < 50; ++j) {
			int pos = j % 5;
			pos = static_cast<int>(d.size()) * pos / 4;
			if (pos == static_cast<int>(d.size())) --pos;
			dd.erase(dd.begin() + pos);
			d.erase(d.begin() + pos);
			SEQ_TEST(equal_seq(d, dd));
		}
	}



	deq.resize(count, 0);
	seq.resize(count, 0);

	seq.shrink_to_fit();

	//fill again, backward
	{
		auto itd = deq.begin();
		auto its = seq.begin();
		type j = static_cast<type>(deq.size()) - 1;
		while (itd != deq.end()) {
			*itd++ = j;
			*its++ = j;
			--j;
		}
	}

	SEQ_TEST(equal_seq(deq, seq));

	


	seq.resize(count);
	deq.resize(count);
	vec.resize(count);
	for (size_t j = 0; j < vec.size(); ++j)
		vec[j] = static_cast<type>(j);
	seq::random_shuffle(vec.begin(), vec.end());
	{
		auto itd = deq.begin();
		auto its = seq.begin();
		auto itv = vec.begin();
		while (itd != deq.end()) {
			*itd = *itv;
			*its = *itv;
			++itd; ++its; ++itv;
		}
	}
	std::vector<int> ran_pos;
	int ssize = static_cast<int>(vec.size());
	srand(0);
	for (unsigned j = 0; j < /*100000*/count/10; ++j) {
		ran_pos.push_back(rand() % ssize--);
	}

	// Test erase random position
	for (size_t j = 0; j < ran_pos.size(); ++j)
		deq.erase(deq.begin() + ran_pos[j]);
	
	auto tmp = seq.begin();
	for (size_t j = 0; j < ran_pos.size(); ++j) {
		tmp = seq.erase(seq.iterator_at(static_cast<size_t>(ran_pos[j])));
	}
	
	SEQ_TEST(equal_seq(deq, seq));



	seq.resize(count);
	deq.resize(count);
	{
		auto itd = deq.begin();
		auto its = seq.begin();
		auto itv = vec.begin();
		while (itd != deq.end()) {
			*itd = *itv;
			*its = *itv;
			++itd; ++its; ++itv;
		}
	}

	// Test move assign and move copy

	sequence_type seq2 ( std::move(seq),al);
	deque_type deq2 = std::move(deq);
	SEQ_TEST(equal_seq(deq2, seq2) && seq2.size() > 0 && seq.size() == 0 && deq.size() == 0);

	deq = std::move(deq2);
	seq = std::move(seq2);
	SEQ_TEST(equal_seq(deq, seq) && seq.size() > 0 && seq2.size() == 0 && deq2.size() == 0);
}





SEQ_PROTOTYPE( int test_sequence(int , char*[]))
{
	// Test sequence and detect potential memory leak or wrong allocator propagation
	CountAlloc<size_t> al;
	SEQ_TEST_MODULE_RETURN(sequence<OptimizeForMemory>,1, test_sequence<size_t,seq::OptimizeForMemory>(1000000,al));
	SEQ_TEST(get_alloc_bytes(al) == 0);
	SEQ_TEST_MODULE_RETURN(sequence<OptimizeForSpeed>,1, test_sequence<size_t, seq::OptimizeForSpeed>(1000000,al));
	SEQ_TEST(get_alloc_bytes(al) == 0);

	// Test sequence and detect potential memory leak (including non destroyed objects) or wrong allocator propagation
	SEQ_TEST_MODULE_RETURN(sequence<OptimizeForMemory>destroy, 1, test_sequence<TestDestroy<size_t>, seq::OptimizeForMemory>(1000000));
	SEQ_TEST(TestDestroy<size_t>::count() == 0);
	SEQ_TEST_MODULE_RETURN(sequence<OptimizeForSpeed>destroy, 1, test_sequence<TestDestroy<size_t>, seq::OptimizeForSpeed>(1000000));
	SEQ_TEST(TestDestroy<size_t>::count() == 0);

	return 0;
}

