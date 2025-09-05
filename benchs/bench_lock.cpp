
#include <seq/testing.hpp>
#include <seq/tiny_lock.hpp>

#include <mutex>
#include <thread>
#include <vector>

template<class Lock>
static void incr_var(std::atomic<bool>* start, std::atomic<int>* ready, Lock * lock, size_t* var, size_t count)
{
	ready->fetch_add(1);
	while (!start->load())
		std::this_thread::yield();

	for (size_t i = 0; i < count; ++i) {
		std::lock_guard<Lock> ll(*lock);
		(*var) = seq::hash_bytes_murmur64(var, sizeof(size_t));
	}
}

template<class Lock>
static std::pair<size_t,size_t> test_lock(size_t threads, size_t count)
{
	struct pad
	{
		Lock l;
		char unused[63];
	};
	std::vector<std::thread> ths(threads);
	std::atomic<bool> start{ false };
	std::atomic<int> ready{ 0 };

	static constexpr int max_lock = 1;
	size_t var[max_lock];
	pad l[max_lock];
	memset(var, 0, sizeof(var));

	for (size_t i = 0; i < threads; ++i) {
		ths[i] = std::thread([&,i]() {
			
			int lock_idx = ((i / 6) & (max_lock - 1));
			incr_var<Lock>(&start, &ready, &(l + lock_idx)->l, var + lock_idx, count);
		});
	}

	while (ready.load() != (int)threads)
		std::this_thread::yield();

	seq::tick();
	start.store(true);
	for (size_t i = 0; i < threads; ++i)
		ths[i].join();

	size_t v = 0;
	for (int i = 0; i < max_lock; ++i)
		v += var[i];
	return { v, (size_t)seq::tock_ms() };
}


int bench_lock(int, char** const) 
{
	size_t threads = 1;
	size_t count = 1000000;

	auto f1 = [](int i) { return i + 2; };
	auto f2 = [threads](int i) { return i * threads; };
	auto f3 = [&](int i) { return count + i; };
	auto f4 = [&](int i) { return count + threads + i; };
	std::cout << sizeof(f1) << " " << sizeof(f2) << " " << sizeof(f3) << " " << sizeof(f4) << std::endl;

	// warmup
	test_lock<std::mutex>(threads, count);

	auto ms = test_lock<std::mutex>(threads, count);
	std::cout << "std::mutex " << ms.first << " " << ms.second << " ms" << std::endl;

	ms = test_lock<seq::tiny_mutex>(threads, count);
	std::cout << "seq::tiny_mutex " << ms.first << " " << ms.second << " ms" << std::endl;

	//ms = test_lock<seq::tiny_lock>(threads, count);
	//std::cout << "seq::tiny_lock "<<  ms.first << " " << ms.second << " ms" << std::endl;

	ms = test_lock<seq::spinlock>(threads, count);
	std::cout << "seq::spinlock "<< ms.first << " " << ms.second << " ms" << std::endl;

	

	
	return 0;
}