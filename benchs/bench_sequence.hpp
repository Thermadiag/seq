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

#pragma once

#include <seq/sequence.hpp>
#include <seq/tiered_vector.hpp>
#include <seq/format.hpp>
#include <seq/testing.hpp>
#include <seq/memory.hpp>
#include "plf/plf_colony.hpp"

#include <list>


using namespace seq;

/// @brief Compare performances of seq::sequence, plf::colony and std::list
/// @tparam T 
/// @param count 
template<class T, LayoutManagement lay = seq::OptimizeForSpeed>
void test_sequence_vs_colony(size_t count)
{
	using seq_type = sequence<T, std::allocator<T>, lay>;
	using colony_type = plf::colony<T>;
	using list_type = std::list<T>;

	std::cout << std::endl;
	std::cout << "Compare performances of seq::sequence, plf::colony and std::list " << std::endl;
	std::cout << std::endl;


	std::cout << fmt(fmt("method").l(30), "|", fmt("plf::colony").c(20), "|", fmt("seq::sequence").c(20), "|", fmt("std::list").c(20), "|") << std::endl;
	std::cout << fmt(str().l(30).f('-'), "|", str().c(20).f('-'), "|", str().c(20).f('-'), "|", str().c(20).f('-'), "|") << std::endl;

	auto f = fmt(pos<0, 2, 4, 6>(), str().l(30), "|", fmt(pos<0>(), size_t(), " ms").c(20), "|", fmt(pos<0>(), size_t(), " ms").c(20), "|", fmt(pos<0>(), size_t(), " ms").c(20), "|");



	std::vector< T> shufle;

	colony_type col1;
	seq_type seq1;
	list_type lst;

	size_t col_t, seq_t, lst_t;

	for (size_t i = 0; i < count; ++i)
		shufle.push_back((T)i);
	std::random_shuffle(shufle.begin(), shufle.end());

	

	tick();
	col1.reserve(count);
	for (int i = 0; i < count; ++i)
		col1.insert((shufle[i]));
	col_t = tock_ms();

	tick();
	seq1.reserve(count);
	for (int i = 0; i < count; ++i)
		seq1.push_back((shufle[i]));
	seq_t = tock_ms();

	tick();
	for (int i = 0; i < count; ++i)
		lst.push_back((shufle[i]));
	lst_t = tock_ms();

	std::cout << f("insert(reserve)", col_t, seq_t, lst_t) << std::endl;

	col1 = colony_type{};
	seq1 = seq_type{};


	tick();
	for (int i = 0; i < count; ++i)
		col1.insert((shufle[i]));
	col_t = tock_ms();

	tick();
	for (int i = 0; i < count; ++i)
		seq1.insert((shufle[i]));
	seq_t = tock_ms();

	tick();
	for (int i = 0; i < count; ++i)
		lst.push_back((shufle[i]));
	lst_t = tock_ms();

	std::cout << f("insert", col_t, seq_t, lst_t) << std::endl;



	tick();
	col1.clear();
	col_t = tock_ms();

	tick();
	seq1.clear();
	seq_t = tock_ms();

	tick();
	lst.clear();
	lst_t = tock_ms();

	SEQ_TEST(seq1.size() == 0 && col1.size() == 0 && lst.size() == 0);
	std::cout << f("clear", col_t, seq_t, lst_t) << std::endl;

	for (int i = 0; i < count; ++i) {
		col1.insert((shufle[i]));
		seq1.insert((shufle[i]));
		lst.push_back((shufle[i]));
	}

	tick();
	col1.erase(col1.begin(), col1.end());
	col_t = tock_ms();

	tick();
	seq1.erase(seq1.begin(), seq1.end());
	seq_t = tock_ms();

	tick();
	lst.erase(lst.begin(), lst.end());
	lst_t = tock_ms();

	SEQ_TEST(seq1.size() == 0 && col1.size() == 0 && lst.size() == 0);
	std::cout << f("erase(begin(),end())", col_t, seq_t, lst_t) << std::endl;



	for (int i = 0; i < count; ++i) {
		col1.insert((shufle[i]));
		seq1.insert((shufle[i]));
		lst.push_back((shufle[i]));
	}
	T sum = 0;

	tick();
	for (auto it = col1.begin(); it != col1.end(); ++it)
		sum += *it;
	col_t = tock_ms();

	tick();
	for (auto it = seq1.begin(); it != seq1.end(); ++it)
		sum += *it;
	seq_t = tock_ms();

	tick();
	for (auto it = lst.begin(); it != lst.end(); ++it)
		sum += *it;
	lst_t = tock_ms();

	std::cout << f("iterate", col_t, seq_t, lst_t) << std::endl; print_null(sum);




	tick();
	for (auto it = col1.begin(); it != col1.end(); ++it) {
		it = col1.erase(it);
	}
	col_t = tock_ms();


	tick();
	for (auto it = seq1.begin(); it != seq1.end(); ++it) {
		it = seq1.erase(it);
	}
	seq_t = tock_ms();

	tick();
	for (auto it = lst.begin(); it != lst.end(); ++it) {
		it = lst.erase(it);
	}
	lst_t = tock_ms();

	SEQ_TEST(seq1.size() == col1.size()  && col1.size() == lst.size() );
	std::cout << f("erase half", col_t, seq_t, lst_t) << std::endl;


	tick();
	for (int i = 0; i < shufle.size(); ++i)
		col1.insert(shufle[i]);
	col_t = tock_ms();

	tick();
	for (int i = 0; i < shufle.size(); ++i)
		seq1.insert(shufle[i]);
	seq_t = tock_ms();

	tick();
	for (int i = 0; i < shufle.size(); ++i)
		lst.push_back(shufle[i]);
	lst_t = tock_ms();

	std::cout << f("insert again", col_t, seq_t, lst_t) << std::endl;



	col1.clear();
	seq1.clear();
	lst.clear();
	for (int i = 0; i < count; ++i) {
		seq1.push_back((shufle[i]));
		col1.insert((shufle[i]));
		lst.push_back((shufle[i]));
	}


	tick();
	col1.sort();
	col_t = tock_ms();

	tick();
	seq1.sort();
	seq_t = tock_ms();

	tick();
	lst.sort();
	lst_t = tock_ms();

	std::cout << f("sort", col_t, seq_t, lst_t) << std::endl;

}








