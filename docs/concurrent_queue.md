# Concurrent queue

The new container `seq::concurrent_queue` was added in *seq v2.1*. As its name implies, it is a concurrent (thread safe) FIFO (First In First Out) designed for Multi-Producer Multi-Consumer (MPMC) scenarios.

I tried several concurrent queue implementations but they all had their own limitations:
-	[moodycamel::ConcurrentQueue](https://github.com/cameron314/concurrentqueue): very fast for pushing, but no guarantee on the dequeuing order which is a big stop for me.
-	[boost::lockfree::queue](https://www.boost.org/doc/libs/1_53_0/doc/html/lockfree.html): I did not manage to get any performances out of it, maybe I configured it wrongly. But the type limitation is also a big no.
-	[MPMCQueue](https://github.com/rigtorp/MPMCQueue): very good one, but sadly only work with preallocated storage.

In the end I decided to roll my own implentation with the following criteria:
-	Being faster than a regular std::mutex plus std::deque,
-	Having the possibility to preallocate memory up-front,
-	Always ensure FIFO behavior,
-	Not being to harsh on the type requirement,
-	Providing unsafe but usefull API (like iteration)

The resulting `seq::concurrent_queue` is not fully lock-free nor wait-free, but combines atomic-based operations with locks to provide a certain level of concurrency.
While I still need to properly formalize a benchmark, It is consistenly faster than a regular queue + mutex, and usually faster or as fast as aforementioned implementations.

Note that, if dequeuing order is not an issue for you and if you use a lot of threads (tipically more than 8 concurrent accesses), `moodycamel::ConcurrentQueue` is unbeatable.
