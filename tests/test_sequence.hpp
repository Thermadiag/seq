

#include "sequence.hpp"
#include "tiered_vector.hpp"
#include "memory.hpp"
#include "testing.hpp"
#include <list>
#include <vector>
#include <deque>


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




template<class T>
void test_sequence(int size = 50000000)
{
	const int count = size;
	

	typedef T type;
	std::vector<type> vec;
	typedef tiered_vector<type, std::allocator<T>, OptimizeForSpeed > deque_type;
	deque_type deq;
	typedef sequence<type , std::allocator<type>, OptimizeForSpeed> sequence_type;
	sequence_type seq;

	SEQ_TEST_ASSERT(seq.begin() == seq.end());
	SEQ_TEST_ASSERT(seq.size() == 0);
	
	seq.resize(10);
	SEQ_TEST_ASSERT(seq.size() == 10);
	seq.clear();
	SEQ_TEST_ASSERT(seq.size() == 0 && !seq.data());

	// Test push_back

	for (int i = 0; i < count; ++i)
		deq.push_back(i);

	vec.reserve(count);
	for (int i = 0; i < count; ++i)
		vec.push_back(i);

	for (int i = 0; i < count; ++i)
		seq.push_back(i);


	SEQ_TEST_ASSERT( equal_seq(deq, seq) )

	// Test resize lower
	deq.resize(deq.size() / 10);
	seq.resize(seq.size() / 10);
		
	SEQ_TEST_ASSERT(equal_seq(deq, seq));

	// Test resize upper
	deq.resize(count, 0);
	seq.resize(count, 0);
		
	SEQ_TEST_ASSERT(equal_seq(deq, seq));


	size_t i = 0; 
	for (auto it = seq.begin(); it != seq.end(); ++it)
		*it = (T)(i++);
	i = 0;
	for (auto it = deq.begin(); it != deq.end(); ++it)
		*it = (T)(i++);

	// Test resize front lower
	deq.resize_front(deq.size() / 10);
	seq.resize_front(seq.size() / 10);

	SEQ_TEST_ASSERT(equal_seq(deq, seq));

	// Test resize front upper
	deq.resize_front(count, 0);
	seq.resize_front(count, 0);
		
	SEQ_TEST_ASSERT(equal_seq(deq, seq));

	{
		// Test copy construct
		deque_type d2 = deq;
		sequence_type dd2 = seq;

		SEQ_TEST_ASSERT(equal_seq(d2, dd2));
	}

	SEQ_TEST_ASSERT(equal_seq(deq, seq));



	{
		{
			auto itd = deq.begin();
			auto its = seq.begin();
			int i = 0;
			while (itd != deq.end()) {
				*itd++ = i;
				*its++ = i;
				++i;
			}
		}
			
		SEQ_TEST_ASSERT(equal_seq(deq, seq));

		// Test erase range left side
		deq.erase(deq.begin() + deq.size() / 4, deq.begin() + deq.size() / 2);
		seq.erase(seq.begin() + seq.size() / 4, seq.begin() + seq.size() / 2);
		SEQ_TEST_ASSERT(equal_seq(deq, seq));

		deq.resize(count, 0);
		seq.resize(count, 0);

		// Test erase range right side
		deq.erase(deq.begin() + deq.size() / 2, deq.begin() + deq.size() * 3 / 4);
		seq.erase(seq.begin() + seq.size() / 2, seq.begin() + seq.size() * 3 / 4);
		SEQ_TEST_ASSERT(equal_seq(deq, seq));
	}

	{
		deq.resize(vec.size() / 2, 0);
		seq.resize(vec.size() / 2, 0);

		// Test assign grow
		deq.assign(vec.begin(), vec.end());
		seq.assign(vec.begin(), vec.end());
			
		SEQ_TEST_ASSERT(equal_seq(deq, seq));

		deq.resize(vec.size() * 2, 0);
		seq.resize(vec.size() * 2, 0);

		// Test assign shrink
		deq.assign(vec.begin(), vec.end());
		seq.assign(vec.begin(), vec.end());
			
		SEQ_TEST_ASSERT(equal_seq(deq, seq));
	}
	{
		std::list<type> lst;
		for (int i = 0; i < count; ++i)
			lst.push_back(i);

		deq.resize(lst.size() / 2, 0);
		seq.resize(lst.size() / 2, 0);

		// Test assign shrink with non random access iterator
		deq.assign(lst.begin(), lst.end());
		seq.assign(lst.begin(), lst.end());
			
		SEQ_TEST_ASSERT(equal_seq(deq, seq));
		deq.resize(lst.size() * 2, 0);
		seq.resize(lst.size() * 2, 0);

		// Test assign grow with non random access iterator
		deq.assign(lst.begin(), lst.end());
		seq.assign(lst.begin(), lst.end());
			
		SEQ_TEST_ASSERT(equal_seq(deq, seq));
	}

	deq.resize(count, 0);
	seq.resize(count, 0);
	SEQ_TEST_ASSERT(equal_seq(deq, seq));

	// Test shrink_to_fit
	seq.shrink_to_fit();

	SEQ_TEST_ASSERT(equal_seq(deq, seq));

	//fill again, backward
	{
		auto itd = deq.begin();
		auto its = seq.begin();
		int i = (int)deq.size()-1;
		while (itd != deq.end()) {
			*itd++ = i;
			*its++ = i;
			--i;
		}
	}

	SEQ_TEST_ASSERT(equal_seq(deq, seq));

	// Test pop_back
	while (deq.size() > 25)
		deq.pop_back();
	while (seq.size() > 25)
		seq.pop_back();
	SEQ_TEST_ASSERT(equal_seq(deq, seq));

	deq.resize(count, 0);
	seq.resize(count, 0);

	SEQ_TEST_ASSERT(equal_seq(deq, seq));

	//fill again, backward
	{
		auto itd = deq.begin();
		auto its = seq.begin();
		int i = (int)deq.size() - 1;
		while (itd != deq.end()) {
			*itd++ = i;
			*its++ = i;
			--i;
		}
	}



	SEQ_TEST_ASSERT(equal_seq(deq, seq));

	// Test pop_front
	while (deq.size() > 25) 
		deq.pop_front();
	while (seq.size() > 25)
		seq.pop_front();
	SEQ_TEST_ASSERT(equal_seq(deq, seq));


		
	//Test erase
	{
		sequence_type d;
		deque_type dd;
		d.resize(100, 0);
		dd.resize(100, 0);
		{
			auto itd = d.begin();
			auto its = dd.begin();
			int i = 0;
			while (itd != d.end()) {
				*itd++ = i;
				*its++ = i;
				++i;
			}
		}

		for (int i = 0; i < 50; ++i) {
			int pos = i % 5;
			pos = (int)d.size() * pos / 4;
			if (pos == (int)d.size()) --pos;
			dd.erase(dd.begin() + pos);
			d.erase(d.begin() + pos);
			SEQ_TEST_ASSERT(equal_seq(d, dd));
		}
	}



	deq.resize(count, 0);
	seq.resize(count, 0);

	seq.shrink_to_fit();

	//fill again, backward
	{
		auto itd = deq.begin();
		auto its = seq.begin();
		int i = (int)deq.size() - 1;;
		while (itd != deq.end()) {
			*itd++ = i;
			*its++ = i;
			--i;
		}
	}

	SEQ_TEST_ASSERT(equal_seq(deq, seq));

	


	seq.resize(count);
	deq.resize(count);
	vec.resize(count);
	for (int i = 0; i < (int)vec.size(); ++i)
		vec[i] = i;
	std::random_shuffle(vec.begin(), vec.end());
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
	int ssize = (int)vec.size();
	srand(0);
	for (int i = 0; i < /*100000*/count/10; ++i) {
		ran_pos.push_back(rand() % ssize--);
	}

	// Test erase random position
	for (int i = 0; i < (int)ran_pos.size(); ++i)
		deq.erase(deq.begin() + ran_pos[i]);
	
	auto tmp = seq.begin();
	for (int i = 0; i < (int)ran_pos.size(); ++i) {
		tmp = seq.erase(seq.iterator_at(ran_pos[i]));
	}
	
	SEQ_TEST_ASSERT(equal_seq(deq, seq));



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

	sequence_type seq2 = std::move(seq);
	deque_type deq2 = std::move(deq);
	SEQ_TEST_ASSERT(equal_seq(deq2, seq2) && seq2.size() > 0 && seq.size() == 0 && deq.size() == 0);

	deq = std::move(deq2);
	seq = std::move(seq2);
	SEQ_TEST_ASSERT(equal_seq(deq, seq) && seq.size() > 0 && seq2.size() == 0 && deq2.size() == 0);
}






