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

#ifndef SEQ_LOCK_HPP
#define SEQ_LOCK_HPP


/** @file */

#include <atomic>
#include <mutex>
#include <chrono>
#include <thread>
#include <type_traits>

#include "bits.hpp"


namespace seq
{
	/// @brief Lightweight and fast spinlock implementation based on https://rigtorp.se/spinlock/
	///
	/// spinlock is a lightweight spinlock implementation following the TimedMutex requirements.
	///  
	class spinlock 
    {
		std::atomic<bool> d_lock;

	public:
		constexpr spinlock() : d_lock(0) {}

        spinlock(spinlock const&) = delete;
        spinlock& operator=(spinlock const&) = delete;

		void lock() noexcept {
			for (;;) {
				// Optimistically assume the lock is free on the first try
				if (!d_lock.exchange(true, std::memory_order_acquire)) 
					return;
				
				// Wait for lock to be released without generating cache misses
				while (d_lock.load(std::memory_order_relaxed)) 
					// Issue X86 PAUSE or ARM YIELD instruction to reduce contention between
					// hyper-threads
					std::this_thread::yield();
			}
		}

		bool is_locked() const noexcept { return d_lock.load(std::memory_order_relaxed); }
		bool try_lock() noexcept {
			// First do a relaxed load to check if lock is free in order to prevent
			// unnecessary cache misses if someone does while(!try_lock())
			return !d_lock.load(std::memory_order_relaxed) &&
				!d_lock.exchange(true, std::memory_order_acquire);
		}

		void unlock() noexcept {
			d_lock.store(false, std::memory_order_release);
		}

		template<class Rep, class Period>
		bool try_lock_for(const std::chrono::duration<Rep, Period>& duration) noexcept
		{
			return try_lock_until(std::chrono::system_clock::now() + duration);
		}

		template<class Clock, class Duration>
		bool try_lock_until(const std::chrono::time_point<Clock, Duration>& timePoint) noexcept
		{
			for (;;) {
				if (!d_lock.exchange(true, std::memory_order_acquire))
					return true;

				while (d_lock.load(std::memory_order_relaxed)) {
					if (std::chrono::system_clock::now() > timePoint)
						return false;
					std::this_thread::yield();
				}
			}
		}


		void lock_shared() { lock(); }
		void unlock_shared() { unlock(); }
	};
	
	


	/// @brief An unfaire read-write spinlock class that favors write operations
	/// 
	template<class LockType = std::uint32_t>
	class shared_spinner
	{
		static_assert(std::is_unsigned<LockType>::value, "shared_spinner only supports unsigned atomic types!");
		using lock_type = LockType;
		static constexpr lock_type write = 1;
		static constexpr lock_type need_lock = 2;
		static constexpr lock_type read = 4;
		static constexpr lock_type max_read_mask = 1ull << (sizeof(lock_type) * 8u - 1u);

		bool failed_lock(lock_type& expect)
		{
			if (!(expect & (need_lock)))
				d_lock.fetch_or(need_lock, std::memory_order_release);
			expect = need_lock;
			return false;
		}
		SEQ_ALWAYS_INLINE bool try_lock(lock_type& expect)
		{
			if (SEQ_UNLIKELY(!d_lock.compare_exchange_strong(
				expect, write, std::memory_order_acq_rel))) {
				return failed_lock(expect);
			}
			return true;
		}
		SEQ_ALWAYS_INLINE void yield() {
			std::this_thread::yield();
		}

		std::atomic<lock_type> d_lock;

	public:
		constexpr shared_spinner() : d_lock(0) {}
		shared_spinner(shared_spinner const&) = delete;
		shared_spinner& operator=(shared_spinner const&) = delete;

		SEQ_ALWAYS_INLINE void lock()
		{
			lock_type expect = 0;
			while (SEQ_UNLIKELY(!try_lock(expect))) 
				yield();
		}
		SEQ_ALWAYS_INLINE void unlock()
		{
			SEQ_ASSERT_DEBUG(d_lock & write, "");
			d_lock.fetch_and(static_cast<lock_type>(~(write | need_lock)), std::memory_order_release);
		}
		SEQ_ALWAYS_INLINE void lock_shared()
		{
			while (SEQ_UNLIKELY(!try_lock_shared()))
				yield();
		}
		SEQ_ALWAYS_INLINE void unlock_shared()
		{
			SEQ_ASSERT_DEBUG(d_lock > 0, "");
			d_lock.fetch_sub(read, std::memory_order_release);
		}
		// Attempt to acquire writer permission. Return false if we didn't get it.
		SEQ_ALWAYS_INLINE bool try_lock()
		{
			if (d_lock.load(std::memory_order_relaxed) & (need_lock | write))
				return false;
			lock_type expect = 0;
			return d_lock.compare_exchange_strong(
				expect, write, std::memory_order_acq_rel);
		}
		SEQ_ALWAYS_INLINE bool try_lock_shared()
		{
			// This version might be slightly slower in some situations (low concurrency).
			// However it works for very small lock type (like uint8_t) by avoiding overflows.
			if SEQ_CONSTEXPR (sizeof(d_lock) == 1) {
				lock_type content = d_lock.load(std::memory_order_relaxed);
				return (!(content & (need_lock | write | max_read_mask)) && d_lock.compare_exchange_strong(content, content + read));
			}
			else {
				// Version based on fetch_add
				if (!(d_lock.load(std::memory_order_relaxed) & (need_lock | write))) {
					if (SEQ_LIKELY(!(d_lock.fetch_add(read, std::memory_order_acquire) & (need_lock | write))))
						return true;
					d_lock.fetch_add(-read, std::memory_order_release);
				}
				return false;
			}
		}

	};

	using shared_spinlock = shared_spinner<>;


	/// @brief Dumy lock class that basically does nothing
	///
	struct null_lock
	{
		void lock()noexcept {}
		void unlock()noexcept {}
		bool try_lock() noexcept { return true; }
		template<class Rep, class Period>
		bool try_lock_for(const std::chrono::duration<Rep, Period>& ) noexcept { return true; }
		template<class Clock, class Duration>
		bool try_lock_until(const std::chrono::time_point<Clock, Duration>& ) noexcept { return true; }

		void lock_shared() noexcept {}
		bool try_lock_shared() noexcept { return true; }
		void unlock_shared() noexcept {}
	};


}





#endif
