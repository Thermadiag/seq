#include "testing.hpp"
#include <seq/concurrent_queue.hpp>
#include <deque>
#include <fstream>

#include "concurrentqueue.h"
#include "MPMCQueue.h"
#include <seq/sequence.hpp>
#include <seq/concurrent_map.hpp>
#include <seq/devector.hpp>
using namespace rigtorp::mpmc;
#if BOOST_FOUND
#include <boost/lockfree/queue.hpp>
template<class T>
using boost_queue = boost::lockfree::queue<T>;
#endif



template<class T>
using concurrent_set = seq::concurrent_set<T, seq::hasher<T>, std::equal_to<>, std::allocator<T>, seq::low_concurrency>;



template<class T>
struct queue
{
	using lock_type = std::mutex;
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
	std::size_t size() const { return deq.size(); }

	void clear()
	{
		std::scoped_lock<lock_type> guard(lock);
		deq.clear();
	}
};

template<class Queue, class... Args>
bool push(Queue& q, Args&&... t)
{
	q.emplace(std::forward<Args>(t)...);
	return true;
}
template<class T, class... Args>
bool push(moodycamel::ConcurrentQueue<T>& q, Args&&... t)
{
	q.enqueue(std::forward<Args>(t)...);
	return true;
}

#if BOOST_FOUND
template<class T, class... Args>
bool push(boost_queue<T>& q, Args&&... t)
{
	return (q.push(T{std::forward<Args>(t)...} ));
}
#endif


template<class T, class... Args>
bool push(Queue<T>& q, Args&&... t)
{
	q.emplace(std::forward<Args>(t)...);
	return true;
}
template<class T, class... Args>
bool push(concurrent_set<T>& q, Args&&... t)
{
	q.emplace(std::forward<Args>(t)...);
	return true;
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
#if BOOST_FOUND
template<class T>
bool pop(boost_queue<T>& q, T &v)
{
	return q.pop(v);
}
#endif
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

		for (std::size_t j = 0; j < data.size(); ++j) {
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
	static constexpr unsigned count = 32;
	static constexpr unsigned mask = count - 1;
	std::atomic<T> d_cnts[count];

	static unsigned global_id() noexcept
	{
		static std::atomic<unsigned> cnt{ 0 };
		return cnt.fetch_add(1);
	}
	SEQ_ALWAYS_INLINE unsigned thread_id() noexcept
	{
		thread_local unsigned id = global_id() & mask;
		return id;
	}

public:
	safe_counter() noexcept { memset((void*)d_cnts, 0, sizeof(d_cnts)); }

	SEQ_ALWAYS_INLINE void add(T v = 1) noexcept { d_cnts[thread_id()].fetch_add(v); }
	SEQ_ALWAYS_INLINE void sub(T v = 1) noexcept { d_cnts[thread_id()].fetch_sub(v); }

	SEQ_ALWAYS_INLINE T value() const noexcept
	{
		T ret = 0;
		for (unsigned i = 0; i < count; ++i)
			ret += d_cnts[i].load(std::memory_order_relaxed);
		return ret;
	}
};

template<class T, class Queue>
void push_thread(Queue& q, std::atomic<bool>& start, std::atomic<bool>& stop, safe_counter<std::size_t>& cnt)
{
	while (!start)
		; // std::this_thread::yield();
	T val = 0;
	std::size_t r = 0;
	while (!stop) {
		if(push(q, val++))
			if ((++r & 31) == 0)
				cnt.add(32);
	}
}


static constexpr std::size_t max_pop = 10000000;


template<class T, class Queue>
void pop_thread(Queue& q, std::atomic<bool>& start, std::atomic<bool>& stop, safe_counter<std::size_t>& cnt)
{
	while (!start)
		; // std::this_thread::yield();
	T val;
	std::size_t r = 0;
	while (!stop) {
		if (pop(q, val)) {
			cnt.add();
			if ((++r & 31) == 0)
				if SEQ_UNLIKELY(cnt.value() >= max_pop)
					stop.store(true);
		}
		else
			// Give time for additional push
			//std::this_thread::yield();
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}

static std::uint64_t msecs()
{
	return (std::uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

struct Ret
{
	std::uint64_t push_cnt = 0;
	std::uint64_t pop_cnt = 0;
	std::uint64_t elapsed_ms = 0;
	std::string name;
};

template<class T, class Queue>
Ret test_queue(Queue& q, int threads)
{
	std::vector<std::pair<std::thread, std::thread>> all_threads((std::size_t)threads);
	std::atomic<bool> start_push{ false }, start_pop{ false }, stop{ false };
	safe_counter<std::size_t> push_cnt, pop_cnt;
	for (int i = 0; i < threads; ++i) {
		all_threads[(std::size_t)i].first = std::thread([&]() { push_thread<T>(q, start_push, stop, push_cnt); });
		all_threads[(std::size_t)i].second = std::thread([&]() { pop_thread<T>(q, start_pop, stop, pop_cnt); });
	}

	

	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	// for (int i = 0; i < 10000; ++i)
	//	push(q, T{});

	start_push.store(true);
	//std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	start_pop.store(true);

	/* auto st = msecs();
	while (msecs() - st < 2000) {
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	stop.store(true);*/
	auto st = msecs();
	for (std::size_t i = 0; i < (std::size_t)threads; ++i) {
		all_threads[i].first.join();
		all_threads[i].second.join();
	}

	//T v;
	//while(pop(q,v))
	//	;
	auto el = msecs() - st;

	return { push_cnt.value(), pop_cnt.value() ,el};
}
template<class QueueType, class T>
Ret test_queue_name(const char* name, int threads = 1)
{
	Ret p;
	if constexpr (std::is_same_v<Queue<T>,QueueType>){
		QueueType q(max_pop * 2);
		p = test_queue<T>(q, threads);
	} 
#if BOOST_FOUND
	else if constexpr (std::is_same_v<boost_queue<T>, QueueType>) {
		QueueType q(0);
		p = test_queue<T>(q, threads);
	}
#endif
	else{ 
		QueueType q;
		p = test_queue<T>(q, threads);
	}	
	p.name = name;
	std::cout << name << ": " << p.push_cnt << " " << p.pop_cnt << " " << p.elapsed_ms << std::endl;
	return p;
}


bool to_csv(const std::vector < std::vector<Ret>>& res, const char* filename)
{
	std::ofstream fout(filename);
	if (!fout)
		return false;
	fout << "\"sep=\t\"" << std::endl;
	
    fout << "Threads\t";
	for (std::size_t i = 0; i < res[1].size(); ++i) {
		fout << res[1][i].name <<"\t";
	}	  
	fout << std::endl;

	for(std::size_t i = 1; i < res.size(); ++i){
		fout << i <<"\t";
		auto & vec = res[i]; 
		for (std::size_t j = 0; j < vec.size(); ++j) 
			fout << vec[j].elapsed_ms  <<"\t";
		fout << std::endl;

	}	
	fout << std::endl;

	return true;
}



int bench_concurrent_queue(int, char** const)
{
	// These benchmarks are not well formalized!

	int threads = 16;
	int count = 1000000;

	/*std::vector<std::vector<Ret>> rets(threads + 1);
	for(int th = 1; th <= threads; ++th){ 
		std::cout << th << " threads" << std::endl;
		rets[(std::size_t)th].push_back( test_queue_name<queue<int>, int>("std::deque", th));
		rets[(std::size_t)th].push_back( test_queue_name<seq::concurrent_queue<int>, int>("seq::concurrent_queue", th));
		//rets[(std::size_t)th].push_back( test_queue_name<rigtorp::mpmc::Queue<int>, int>("rigtorp::mpmc::Queue", th));
		rets[(std::size_t)th].push_back( test_queue_name<moodycamel::ConcurrentQueue<int>, int>("moodycamel::ConcurrentQueue", th));
#if BOOST_FOUND
		rets[(std::size_t)th].push_back( test_queue_name<boost_queue<int>, int>("boost::lockfree::queue", th));
#endif
	}
	to_csv(rets,"bench_concurrent_queue.csv");
	*/
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

	std::vector<queue_type> vals((std::size_t)count);
	for (std::size_t i = 0; i < vals.size(); ++i)
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

	#if BOOST_FOUND
		seq::tick();
		boost_queue<queue_type> bq{ 0 };
		bq.reserve_unsafe(vals.size());
		launch_push(bq, vals, threads);
		el = seq::tock_ms();
		std::cout << "push concurrent boost: " << el << " ms " << std::endl;
	#endif


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

	#if BOOST_FOUND
		seq::tick();
		launch_pop<queue_type>(bq, count, threads);
		el = seq::tock_ms();
		std::cout << "pop concurrent boost: " << el << " ms " << std::endl;
	#endif	

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

	#if BOOST_FOUND
		boost_queue<queue_type> bq{0};
		seq::tick();
		unbalanced<queue_type>(bq, vals, unbalance, threads);
		el = seq::tock_ms();
		std::cout << "push/pop concurrent boost: " << el << " ms " << std::endl;
	#endif	
	}

	return 0;
}