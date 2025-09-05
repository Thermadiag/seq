#pragma once

#include "concurrent_map.hpp"

namespace seq
{

	namespace detail
	{
		struct BaseThreadData
		{
			BaseThreadData* left;
			BaseThreadData* right;

			BaseThreadData() noexcept { left = right = this; }

			bool is_empty() const noexcept { return right == this; }
			void erase() noexcept
			{
				// remove from linked list
				left->right = right;
				right->left = left;
				left = right = this;
			}
			void insert(BaseThreadData* _left, BaseThreadData* _right) noexcept
			{
				// insert in linked list
				this->left = _left;
				this->right = _right;
				_left->right = this;
				_right->left = this;
			}
		};
		struct ThreadData : BaseThreadData
		{
			std::atomic<int> acquire_lock{ 0 };
			std::mutex lock;
			std::condition_variable cond;

			static ThreadData* get() noexcept
			{
				thread_local ThreadData data;
				return &data;
			}
		};

		struct LockData
		{
			BaseThreadData end;
			std::mutex lock;

			void import(LockData&& other)
			{
				if (other.end.is_empty()) {
					end.left = end.right = &end;
				}
				else {
					end = other.end;
					end.left->right = end.right->left = &end;
				}
			}

			LockData() noexcept { end.left = end.right = &end; }
			LockData(LockData&& other) noexcept { import(std::move(other)); }
			LockData& operator=(LockData&& other) noexcept
			{
				import(std::move(other));
				return *this;
			}

			using LockMap = concurrent_map<void*, ThreadData*,seq::hasher<void*>, std::equal_to<>,std::allocator<std::pair<void*,ThreadData*>>, 0>;
			static SEQ_ALWAYS_INLINE LockMap& map() noexcept
			{
				static LockMap inst;
				return inst;
			}

			static SEQ_ALWAYS_INLINE bool try_lock(std::atomic<uint8_t>& lc) noexcept
			{
				uint8_t val = lc.load(std::memory_order_relaxed);
				return (val & 1) == 0 && lc.compare_exchange_strong(val, val | 1, std::memory_order_acquire);
			}

			static void lock_slow(std::atomic<uint8_t>& lc)
			{
				ThreadData* thread_data = ThreadData::get();
				
				// insert this thread at the end of the lock list
				map().insert_or_visit(std::make_pair((void*)&lc, thread_data), [&lc, thread_data](auto& val) {
					ThreadData* data = val.second;
					thread_data->insert(data->left, data);
					
				});

				if (try_lock(lc)) {
					goto end;
				}
				lc.fetch_or(2, std::memory_order_relaxed); // set bit for concurrent map
				{
					
					std::unique_lock<std::mutex> lock(thread_data->lock);
					// wait until this thread grabs the lock
					while (!thread_data->cond.wait_for(lock, std::chrono::milliseconds(1), [&lc]() { return try_lock(lc); }))
						;
				}
				

			end:
				// erase
				map().erase_if((void*)&lc, [thread_data, &lc](auto& val) {
					if (val.second == thread_data)
						val.second = static_cast<ThreadData*>(thread_data->right);
					thread_data->erase();
					if (val.second->is_empty()) {
						lc.fetch_and((uint8_t)~2u, std::memory_order_relaxed);
						// empty list of waiters
						return true;
					}
					return false;
				});
			}

			static void unlock_slow(std::atomic<uint8_t>& lc)
			{
				map().visit((void*)&lc, [&lc](auto& val) {
					// Check for already locked
					if (lc.load(std::memory_order_relaxed) & 1)
						return; 
					val.second->cond.notify_all();
				});
			}
		};
	}

	/// @brief One byte mutex class.
	///
	/// tiny_mutex is a one byte mutex class that
	/// should be used when a LOT of mutexes are
	/// required (as sizeof(std::mutex) could be
	/// relatively big) and c++20 is not available.
	///
	class tiny_lock
	{
		std::atomic<uint8_t> d_lock{ 0 };

		SEQ_ALWAYS_INLINE bool try_lock(uint8_t val) noexcept { return (val & 1) == 0 && d_lock.compare_exchange_strong(val, val | 1, std::memory_order_acquire); }

	public:
		constexpr tiny_lock() noexcept
		  : d_lock(0)
		{
		}
		tiny_lock(tiny_lock const&) = delete;
		tiny_lock& operator=(tiny_lock const&) = delete;

		SEQ_ALWAYS_INLINE bool is_locked() const noexcept { return d_lock.load(std::memory_order_relaxed) & 1; }
		SEQ_ALWAYS_INLINE bool try_lock() noexcept { return detail::LockData::try_lock(d_lock); }

		SEQ_ALWAYS_INLINE void unlock()
		{
			if (d_lock.fetch_and((uint8_t)(~1u), std::memory_order_release) > 1)
				detail::LockData::unlock_slow(d_lock);
		}
		void yield(unsigned c)
		{
			while (c--)
				std::this_thread::yield();
		}
		SEQ_ALWAYS_INLINE void lock()
		{
			uint8_t count = 0;
			uint8_t val = d_lock.load(std::memory_order_relaxed);
			while (++count < 40) {
				// Optimistically assume the lock is free on the first try
				if (try_lock(val))
					return;

				// Wait for lock to be released without generating cache misses
				val = d_lock.load(std::memory_order_relaxed);
				while (++count < 40 && (val & 1)) {
					// Issue X86 PAUSE or ARM YIELD instruction to reduce contention between
					// hyper-threads
					yield(count);
					val = d_lock.load(std::memory_order_relaxed);
				}
			}
			detail::LockData::lock_slow(d_lock);
		}
	};




	class tiny_mutex
	{
		std::atomic<uint8_t> d_lock{ 0 };

		struct Cond
		{
			std::condition_variable condition;
			std::mutex mutex;
		};
		SEQ_ALWAYS_INLINE Cond& this_condition()
		{
			static Cond cond[1024u];
			return cond[hasher<tiny_mutex*>{}(this) & 1023u];
		}
		SEQ_ALWAYS_INLINE bool try_lock(uint8_t val) noexcept { return (val & 1) == 0 && d_lock.compare_exchange_strong(val, val | 1, std::memory_order_acquire); }
		SEQ_ALWAYS_INLINE void yield(uint8_t c)
		{
			c = std::max(c, (uint8_t)16);
			while (c--) 
				std::this_thread::yield();
		}

		SEQ_NOINLINE(void) acquire()
		{
			static constexpr uint8_t max_value = 255;

			// increment waiter
			auto prev = d_lock.load(std::memory_order_relaxed);
			while ((prev & 1) && prev < (max_value - 1) && !d_lock.compare_exchange_weak(prev, prev + 2))
				; 

			const bool saturate = prev > (max_value - 2);
			if (try_lock(prev))
				// already unlocked: try to lock
				goto end;

			{
				auto& cond = this_condition();
				std::unique_lock<std::mutex> ll(cond.mutex);
				while (!cond.condition.wait_for(ll, std::chrono::milliseconds(1), [this]() { return this->try_lock(); }))
					;
			}

			end:
			if (!saturate)
				// decrement waiters
				d_lock.fetch_sub(2, std::memory_order_relaxed);
		}

	public:
		constexpr tiny_mutex() noexcept
		  : d_lock(0)
		{
		}
		tiny_mutex(tiny_mutex const&) = delete;
		tiny_mutex& operator=(tiny_mutex const&) = delete;

		SEQ_ALWAYS_INLINE bool is_locked() const noexcept { return d_lock.load(std::memory_order_relaxed) & 1; }
		SEQ_ALWAYS_INLINE bool try_lock() noexcept
		{
			return try_lock(d_lock.load(std::memory_order_relaxed));
		}
		
		SEQ_ALWAYS_INLINE bool try_lock_shared() noexcept { return try_lock(); }
		SEQ_ALWAYS_INLINE void unlock()
		{
			if (d_lock.fetch_and((uint8_t)(~1u), std::memory_order_release) > 1)
				this_condition().condition.notify_all();
		}
		
		SEQ_ALWAYS_INLINE void lock()
		{
			static constexpr uint8_t max_spin = 64;

			uint8_t count = 0;
			uint8_t val = d_lock.load(std::memory_order_relaxed);
			while (++count < max_spin) {
				if(try_lock(val))
					return;

				// Wait for lock to be released without generating cache misses
				val = d_lock.load(std::memory_order_relaxed); 
				while (++count < max_spin && (val & 1)) {
					// Issue YIELD instruction to reduce contention between hyper-threads
					// with a linear growth
					yield(count);
					val = d_lock.load(std::memory_order_relaxed); 
				}
			}
			acquire();
		}

		SEQ_ALWAYS_INLINE void lock_shared() { lock(); }
		SEQ_ALWAYS_INLINE void unlock_shared() { unlock(); }
	};
}




