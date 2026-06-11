#include "testing.hpp"
#include <seq/concurrent_queue.hpp>
#include <deque>
#include "concurrentqueue.h"
#include "MPMCQueue.h"
#include <seq/sequence.hpp>
#include <seq/concurrent_map.hpp>
#include <seq/devector.hpp>
using namespace rigtorp::mpmc;

template<class T>
using concurrent_set = seq::concurrent_set<T, seq::hasher<T>, std::equal_to<>, std::allocator<T>, seq::low_concurrency>;

template<class T>
struct queue
{
	using lock_type = seq::spinlock;//std::mutex;
	lock_type lock;
	seq::sequence<T> deq;

public:
	using value_type = T;
	queue() = default;

	template<class... Args>
	void emplace(Args&&... args)
	{
		std::scoped_lock<lock_type> guard(lock);
		deq.emplace_back(std::forward<Args>(args)...);
	}

	bool pop(T& val) noexcept
	{
		std::scoped_lock<lock_type> guard(lock);
		if (deq.empty())
			return false;
		val = std::move(deq.front());
		deq.pop_front();
		return true;
	}
	size_t size() const { return deq.size(); }

	void clear()
	{
		std::scoped_lock<lock_type> guard(lock);
		deq.clear();
	}
};

template<class Queue, class... Args>
void push(Queue& q, Args&&... t)
{
	q.emplace(std::forward<Args>(t)...);
}
template<class T, class... Args>
void push(moodycamel::ConcurrentQueue<T>& q, Args&&... t)
{
	q.enqueue(std::forward<Args>(t)...);
}

template<class T, class... Args>
void push(Queue<T>& q, Args&&... t)
{
	q.emplace(std::forward<Args>(t)...);
}
template<class T, class... Args>
void push(concurrent_set<T>& q, Args&&... t)
{
	q.emplace(std::forward<Args>(t)...);
}

template<class Queue, class T>
bool pop(Queue& q, T& v)
{
	return q.pop(v);
}
template<class T>
bool pop(moodycamel::ConcurrentQueue<T>& q, T& v)
{
	return q.try_dequeue(v);
}
template<class T>
bool pop(Queue<T>& q, T& v)
{
	return q.try_pop(v);
}
template<class T>
bool pop(concurrent_set<T>& q, T& v)
{

	q.visit_all([&](const auto& f) {
		v = f;
		return false;
	});
	q.erase(v);
	return true;
}

template<class Queue, class T>
void launch_push(Queue& q, const std::vector<T>& data, int threads)
{
#pragma omp parallel for num_threads(threads)
	for (int i = 0; i < (int)data.size(); ++i)
		push(q, data[i]);
}

template<class T, class Queue>
void launch_pop(Queue& q, int N, int threads)
{
#pragma omp parallel for num_threads(threads)
	for (int i = 0; i < N; ++i) {
		T v;
		pop(q, v);
	}
}

template<class T, class Queue>
void unbalanced(Queue& q, const std::vector<T>& data, int balance, int threads)
{
#pragma omp parallel for num_threads(threads)
	for (int i = 0; i < threads; ++i) {

		for (size_t j = 0; j < data.size(); ++j) {
			if (j % balance == 0) {
				T v;
				pop(q, v);
			}
			else {
				push(q, data[j]);
			}
		}
	}
}

template<class T>
class safe_counter
{
	std::atomic<T> d_cnts[16];

	unsigned thread_id() noexcept
	{
		static std::atomic<unsigned> cnt{ 0 };
		thread_local unsigned id = cnt.fetch_add(1) & 15;
		return id;
	}

public:
	safe_counter() noexcept { memset((void*)d_cnts, 0, sizeof(d_cnts)); }

	void add(T v = 1) noexcept { d_cnts[thread_id()].fetch_add(v); }
	void sub(T v = 1) noexcept { d_cnts[thread_id()].fetch_sub(v); }

	T value() const noexcept
	{
		T ret = 0;
		for (unsigned i = 0; i < 16; ++i)
			ret += d_cnts[i].load(std::memory_order_relaxed);
		return ret;
	}
};

template<class T, class Queue>
void push_thread(Queue& q, std::atomic<bool>& start, std::atomic<bool>& stop, safe_counter<size_t>& cnt)
{
	while (!start)
		; // std::this_thread::yield();
	T val = 0;
	while (!stop) {
		push(q, val++);
		cnt.add();
		//if (val % 1000000 == 0)
		//	printf("push %i\n", (int)cnt.value());
	}
}
template<class T, class Queue>
void pop_thread(Queue& q, std::atomic<bool>& start, std::atomic<bool>& stop, safe_counter<size_t>& cnt)
{
	while (!start)
		; // std::this_thread::yield();
	T val;
	size_t r = 0;
	while (!stop) {
		if (pop(q, val)) {
			++r;
			cnt.add();
			//if (r % 1000000 == 0)
			//	printf("pop %i\n", (int)cnt.value());
		}
	}
	
}

static uint64_t msecs()
{
	return (uint64_t) std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

template<class T, class Queue>
std::pair<size_t, size_t> test_queue(Queue& q, int threads)
{
	std::vector<std::pair<std::thread, std::thread>> all_threads((size_t)threads);
	std::atomic<bool> start_push{ false }, start_pop{ false }, stop{ false };
	safe_counter<size_t> push_cnt, pop_cnt;
	for (int i = 0; i < threads; ++i) {
		all_threads[(size_t)i].first = std::thread([&]() { push_thread<T>(q, start_push, stop, push_cnt); });
		all_threads[(size_t)i].second = std::thread([&]() { pop_thread<T>(q, start_pop, stop, pop_cnt); });
	}

	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	//for (int i = 0; i < 10000; ++i)
	//	push(q, T{});
	 
	start_push.store(true);
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	start_pop.store(true);

	auto st = msecs();
	while (msecs() - st < 2000) {
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	stop.store(true);

	for (size_t i = 0; i < (size_t)threads; ++i) {
		all_threads[i].first.join();
		all_threads[i].second.join();
	}

	return { push_cnt.value(), pop_cnt.value() };
}
template<class Queue, class T>
void test_queue_name(const char* name, int threads = 1)
{
	Queue q;
	auto p = test_queue<T>(q, threads);

	std::cout << name << ": " << p.first << " " << p.second << std::endl;
}

#include <seq/net_sort.hpp>

int bench_concurrent_queue(int, char** const)
{
	
	int threads = 16 ;
	int count = 1000000;  
	

	test_queue_name<queue<int>, int>("queue", threads);
	test_queue_name<seq::concurrent_queue<int>, int>("concurrent_queue", threads);
	test_queue_name<moodycamel::ConcurrentQueue<int>, int>("moodycamel", threads);

	struct Test
	{
		int val;
		char pad[32];

		Test& operator=(int v)
		{
			val = v;
			return *this;
		}
	};
	using queue_type = int;

	std::vector<queue_type> vals((size_t)count);
	for (size_t i = 0; i < vals.size(); ++i)
		vals[i] = (int)i;

	// Parallel push

	{

		queue<queue_type> q;

		seq::tick();
		launch_push(q, vals, threads);
		auto el = seq::tock_ms();
		std::cout << "push queue: " << el << " ms " << q.size() << std::endl;

		seq::concurrent_queue<queue_type> fifo;
			// fifo.reserve(vals.size());
			seq::tick();
			launch_push(fifo, vals, threads);
			el = seq::tock_ms();
			std::cout << "push fifo: " << el << " ms " << fifo.size() << std::endl;
		
		seq::tick();
		moodycamel::ConcurrentQueue<queue_type> mq;
		launch_push(mq, vals, threads);
		el = seq::tock_ms();
		std::cout << "push concurrent modycamel: " << el << " ms " << mq.size_approx() << std::endl;

		Queue<queue_type> mpq(vals.size());
		seq::tick();
		launch_push(mpq, vals, threads);
		el = seq::tock_ms();
		std::cout << "push concurrent MPMC: " << el << " ms " << mpq.size() << std::endl;

		/* seq::detail::TQueue<int> ts; //(vals.size());
		seq::tick();
		launch_push(ts, vals, threads);
		el = seq::tock_ms();
		std::cout << "push concurrent TQueue: " << el << " ms " << ts.size() << std::endl;
		*/

		// Parallel pop
		// threads = 1;

		seq::tick();
		launch_pop<queue_type>(q, count, threads);
		el = seq::tock_ms();
		std::cout << "pop queue: " << el << " ms " << q.size() << std::endl;

		seq::tick();
		launch_pop<queue_type>(fifo, count, threads);
		el = seq::tock_ms();
		std::cout << "pop fifo: " << el << " ms " << q.size() << std::endl;

		seq::tick();
		launch_pop<queue_type>(mq, count, threads);
		el = seq::tock_ms();
		std::cout << "pop concurrent modycamel: " << el << " ms " << mq.size_approx() << std::endl;

		seq::tick();
		launch_pop<queue_type>(mpq, count, threads);
		el = seq::tock_ms();
		std::cout << "pop concurrent MPMC: " << el << " ms " << mpq.size() << std::endl;

		/* seq::tick();
		launch_pop<int>(ts, count, threads);
		el = seq::tock_ms();
		std::cout << "pop concurrent TQueue: " << el << " ms " << ts.size() << std::endl;*/
	}

	// while (true)

	{
		int unbalance = 3;
		queue<queue_type> q;
		seq::concurrent_queue<queue_type> fifo;
		moodycamel::ConcurrentQueue<queue_type> mq;

		seq::tick();
		unbalanced<queue_type>(q, vals, unbalance, threads);
		auto el = seq::tock_ms();
		std::cout << "push/pop queue: " << el << " ms " << q.size() << std::endl;
		q.clear();

		seq::tick();
		unbalanced<queue_type>(fifo, vals, unbalance, threads);
		el = seq::tock_ms();
		std::cout << "push/pop concurrent fifo: " << el << " ms " << fifo.size() << std::endl;
		// cq.clear();

		seq::tick();
		unbalanced<queue_type>(mq, vals, unbalance, threads);
		el = seq::tock_ms();
		std::cout << "push/pop concurrent modycamel: " << el << " ms " << mq.size_approx() << std::endl;
	}

	return 0;
}