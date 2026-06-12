 /*
 * Copyright (c) 2026 Victor Moncada <vtr.moncada@gmail.com>
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
#include "seq/concurrent_queue.hpp"
#include <random>
#include <chrono>
#include <thread>
#include <vector>

template<class Queue>
void test_empty_queue(Queue& q)
{
	SEQ_TEST(q.size() == 0);
	SEQ_TEST(q.empty());
	SEQ_TEST(std::distance(q.begin(), q.end()) == 0);
	SEQ_TEST(std::distance(q.cbegin(), q.cend()) == 0);
	SEQ_TEST(std::distance(q.rbegin(), q.rend()) == 0);
	SEQ_TEST(std::distance(q.crbegin(), q.crend()) == 0);
 }

template<class Queue, class T>
void queue_thread(Queue& q, const std::vector<T> & vec, std::atomic<bool> & start)
{
	thread_local int loc = 0;
	std::uniform_int_distribution<int> uniform_dist(0, (int)vec.size());
	std::mt19937 e2((unsigned)(uintptr_t)&loc);
	while (!start)
		;

	for (size_t i = 0; i < vec.size(); ++i) {
		auto v = uniform_dist(e2);
		if (v % 2 == 0)
			q.push(vec[i]);
		else
			q.pop();
	}
}

template<class T, class Al>
static void test_queue(const std::vector<T>& vals, const Al& al)
{
	using queue_type = seq::concurrent_queue<T,Al>;

	{
		queue_type q(al);
		test_empty_queue(q);
	}
	{
		queue_type q(100000, al);
		test_empty_queue(q);
	}
	{
		queue_type q(al);
		for (auto& v : vals)
			q.push(v);
		SEQ_TEST(std::distance(q.begin(), q.end()) == vals.size());
		SEQ_TEST(std::distance(q.cbegin(), q.cend()) == vals.size());
		SEQ_TEST(std::distance(q.rbegin(), q.rend()) == vals.size());
		SEQ_TEST(std::distance(q.crbegin(), q.crend()) == vals.size());
		for (auto& v : vals)
			q.pop();
		test_empty_queue(q);
		for (auto& v : vals)
			q.emplace(v);
		for (auto& v : vals) {
			T unused;
			q.pop(unused);
			SEQ_TEST(unused == v);
		}
		q.shrink_to_fit();
		test_empty_queue(q);
	}
	{
		queue_type q(al);
		for (auto& v : vals)
			q.push(v);
		q.clear();
		test_empty_queue(q);
	}
	{
		queue_type q(al);
		for (auto& v : vals)
			q.push(v);
	}

	queue_type q(al);

	std::vector<std::thread> threads(16);
	std::atomic<bool> start{ false };
	for (size_t i = 0; i < threads.size(); ++i)
		threads[i] = std::thread([&]() { queue_thread(q, vals, start); });

	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	start.store(true);
	for (size_t i = 0; i < threads.size(); ++i)
		threads[i].join();

}

#include "tests.hpp"

struct Int
{

};

SEQ_PROTOTYPE(int test_concurrent_queue(int, char*[])) 
{
	
	std::vector<TestDestroy<double>> vals(1000000);
	for (size_t i = 0; i < vals.size(); ++i)
		vals[i] = TestDestroy<double> ((double)i);

	CountAlloc<TestDestroy<double>> al;
	auto prev = TestDestroy<double>::count();
	test_queue(vals, al);
	auto current = TestDestroy<double>::count();
	SEQ_TEST(current == prev);
	SEQ_TEST(get_alloc_bytes(al) == 0);

	return 0;

} 