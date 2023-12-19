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





#include <memory>
#include <vector>

namespace seq
{
	template<class SharedLock, class Allocator = std::allocator<SharedLock> >
	class rb_shared_lock: private Allocator
	{
		using this_type = rb_shared_lock< SharedLock, Allocator>;
		template< class U>
		using rebind_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<U>;
		struct thread_data;

		std::vector<thread_data*, rebind_alloc<thread_data*> > d_data;
		spinlock d_lock;

		// Thread data using thread_local specifier.
		// Contains the shared lock and pointer to parent object.
		struct thread_data
		{
			SharedLock lock;
			this_type* parent{ nullptr };

			~thread_data() {
				// Called on TLS destruction
				if (parent) {
					std::unique_lock<spinlock> l(parent->d_lock);
					for (auto it = parent->d_data.begin(); it != parent->d_data.end(); ++it)
						if (*it == this) {
							parent->d_data.erase(it);
							return;
						}
				}
			}
		};

		// Delete unique_ptr content using provided allocator
		struct unique_ptr_delete : private rebind_alloc< thread_data>
		{
			using base = rebind_alloc< thread_data>;
			unique_ptr_delete() : base() {}
			unique_ptr_delete(const unique_ptr_delete& other) : base(other) {}
			unique_ptr_delete(unique_ptr_delete&& other) noexcept : base(std::move(other)) {}
			unique_ptr_delete(const Allocator& al) : base(al) {}
			unique_ptr_delete& operator=(unique_ptr_delete&& other) noexcept{
				static_cast<rebind_alloc< thread_data>&>(*this) = std::move(rebind_alloc< thread_data>(other));
				return *this;
			}
			void operator()(thread_data* ptr) const {
				ptr->~thread_data();
				const_cast<unique_ptr_delete*>(this)->deallocate(ptr, 1);
			}
		};

		// unique_ptr type
		using ptr_type = std::unique_ptr<thread_data, unique_ptr_delete>;

		// Stored in TLS object
		struct parent_thread_data
		{
			this_type* parent;
			ptr_type data;
			parent_thread_data(this_type* p, ptr_type&& ptr)
				:parent(p), data(std::move(ptr)) {}
			parent_thread_data(parent_thread_data&& other) noexcept
				:parent(other.parent), data(std::move(other.data)) {}
			parent_thread_data& operator=(parent_thread_data&& other) noexcept {
				parent = other.parent;
				data = std::move(other.data);
				return *this;
			}
		};


		// Thread local storage
		struct TLS
		{
			using list_type = std::vector<parent_thread_data, rebind_alloc<parent_thread_data> >;
			
			thread_data* last;
			list_type data;

			SEQ_ALWAYS_INLINE typename list_type::iterator lower_bound(this_type* parent) {
				return std::lower_bound(data.begin(), data.end(), parent, [](const parent_thread_data& d, this_type* p) {return d.parent < p; });
			}
			explicit TLS(const Allocator& al)
				:last(nullptr), data(al) {}
		};

		// Build thread_data object
		thread_data* make_thread_data()
		{
			rebind_alloc< thread_data> al = *this;
			thread_data* t = al.allocate(1);
			construct_ptr(t);
			return t;
		}

		// Register this object into TLS object
		template<class It>
		auto register_this(TLS& tls, It loc) -> thread_data*
		{
			// Add new thread data
			// might throw std::bad_alloc
			loc = tls.data.emplace(loc, this, ptr_type(make_thread_data(), unique_ptr_delete(*this)));
			loc->data->parent = this;
			std::unique_lock <spinlock> l(d_lock);
			d_data.push_back(tls.last = loc->data.get());
			return tls.last;
		}

		auto find_this(TLS& tls) -> thread_data*
		{
			// Find the thread_data for this object
			auto it = tls.lower_bound(this);
			if(it != tls.data.end() && it->parent == this)
				return tls.last = it->data.get();

			// Add new thread data
			// might throw std::bad_alloc
			return register_this(tls, it);
		}

		SEQ_ALWAYS_INLINE auto get_data() -> thread_data*
		{
			// Returns the thread_data for current thread
			// might throw std::bad_alloc
			static thread_local TLS tls(*this);
			if (tls.last && tls.last->parent == this)
				return tls.last;
			return find_this(tls);
		}

	public:
		SEQ_ALWAYS_INLINE rb_shared_lock(const Allocator& al = Allocator())
			:Allocator(al) {}

		bool try_lock()
		{
			std::unique_lock<spinlock> l(d_lock);
			for (size_t i = 0; i < d_data.size(); ++i)
				if (!d_data[i]->lock.try_lock()) {
					for (size_t j = 0; j < i; ++j)
						d_data[j]->lock.unlock();
					return false;
				}
			return true;
		}
		void lock() 
		{
			//std::unique_lock<spinlock> l(d_lock);
			//for (auto& d : d_data)
			//	d->lock.lock();
			while (!try_lock())
				std::this_thread::yield();
		}
		void unlock() 
		{
			std::unique_lock<spinlock> l(d_lock);
			for (auto& d : d_data)
				d->lock.unlock();
		}

		bool try_lock_shared()
		{
			return get_data()->try_lock_shared();
		}
		void lock_shared() 
		{
			get_data()->lock.lock_shared();
		}
		void unlock_shared()
		{
			get_data()->lock.unlock_shared();
		}
	};


}



#endif
