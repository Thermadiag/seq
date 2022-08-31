#pragma once

/** @file */

/**\defgroup lock Lock: locking classes

The lock module provides 4 lock classes that follow the <i>TimedMutex</i> requirements:
    -   seq::spinlock: a one byte fast spinlock implementation
    -   seq::spin_mutex: more general combination of spin locking and mutex
    -   seq::read_write_mutex: read-write mutex based on spinlock or spin_mutex
    -   seq::null_lock: empty lock

See classes documentation for more details.

*/

/** \addtogroup lock
 *  @{
 */


#include <atomic>
#include <thread>
#include <mutex>
#include <exception>
#include <chrono>
#include <condition_variable>


namespace seq
{
    /// @brief Lightweight and fast spinlock implementation based on https://rigtorp.se/spinlock/
    ///
    /// spinlock is a lightweight spinlock implementation following the TimedMutex requirements.
    ///  
    class spinlock {

        spinlock(const spinlock&) = delete;
        spinlock(spinlock&&) = delete;
        spinlock& operator=(const spinlock&) = delete;
        spinlock& operator=(spinlock&&) = delete;

        std::atomic<bool> d_lock;

    public:
        spinlock() : d_lock(0) {}

        ~spinlock()
        {
            if (is_locked()) {
                fprintf(stderr, "spinlock destroyed while locked!");
                fflush(stderr);
                std::terminate();
            }
        }

        void lock() noexcept {
            for (;;) {
                // Optimistically assume the lock is free on the first try
                if (!d_lock.exchange(true, std::memory_order_acquire)) {
                    return;
                }

                // Wait for lock to be released without generating cache misses
                while (d_lock.load(std::memory_order_relaxed)) {
                    // Issue X86 PAUSE or ARM YIELD instruction to reduce contention between
                    // hyper-threads

                    std::this_thread::yield();
                }
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
        bool try_lock_for(const std::chrono::duration<Rep, Period> & duration) noexcept
        {
            return try_lock_until( std::chrono::system_clock::now() + duration);
        }

        template<class Clock, class Duration>
        bool try_lock_until(const std::chrono::time_point<Clock, Duration> & timePoint) noexcept
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
    };


    /// @brief Mutex-like class combining a spinlock and std::mutex.
    ///
    /// spin_mutex is a kind of mutex that combines the speed of spin locking for uncontended scenarios
    /// and the CPU friendly wait of std::mutex.
    /// 
    /// On lock, spin_mutex first uses a spinning strategy for at most \a spin_count cycles.
    /// If the spinlock fails, then the std::mutex is locked.
    /// 
    /// Note that the std::mutex is only allocated when needed, i.e. at the first failed attempt to lock the spin_mutex with spinning.
    /// Therefore, the size of seq::spin_mutex is only 16 bytes on 64 bits systems.
    /// 
    /// spin_mutex follows the TimedMutex requirements.
    /// 
    template<unsigned spin_count = 16U>
    class spin_mutex
    {
        static_assert(spin_count > 0, "");

        struct internal_data
        {
            internal_data() :d_ref(0) {}
            // Helper mutex and condition on which threads can wait in case of collision
            std::mutex d_mutex;
            std::condition_variable d_cond;
            // Maximum number of threads that might be waiting on the d_cond (conservative estimation)
            std::atomic<int> d_ref;
        };

        spin_mutex(const spin_mutex&) = delete;
        spin_mutex(spin_mutex&&) = delete;
        spin_mutex& operator=(const spin_mutex&) = delete;
        spin_mutex& operator=(spin_mutex&&) = delete;

        void* d_data;
        spinlock d_data_lock;
        // Status of the fast mutex
        std::atomic<bool> d_locked;

        internal_data* data() noexcept { return static_cast<internal_data*>((void*)d_data); }

        void lock_slow()
        {
            if (!data()) {
                d_data_lock.lock();
                if (!d_data)
                    d_data = new internal_data();
                d_data_lock.unlock();
            }
            data()->d_ref++;
            {
                std::unique_lock<std::mutex> ul(data()->d_mutex);
                data()->d_cond.wait(ul, [&] {return !d_locked.exchange(true, std::memory_order_relaxed); });
            }
            data()->d_ref--;
        }

        template<class Clock, class Duration>
        bool lock_slow(const std::chrono::time_point<Clock, Duration>& timePoint)
        {
            if (!data()) {
                d_data_lock.lock();
                if (!d_data)
                    d_data = new internal_data();
                d_data_lock.unlock();
            }
            bool ret = false;
            data()->d_ref++;
            {
                std::unique_lock<std::mutex> ul(data()->d_mutex);
                ret = data()->d_cond.wait_until(ul, timePoint, [&] {return !d_locked.exchange(true, std::memory_order_relaxed); });
            }
            data()->d_ref--;
            return ret;
        }

        SEQ_ALWAYS_INLINE bool try_fast() noexcept
        {
            unsigned count = 0;
           
            /* while (count < spin_count && d_locked.load(std::memory_order_relaxed)) {
                // Issue X86 PAUSE or ARM YIELD instruction to reduce contention between hyper-threads
                std::this_thread::yield();
                ++count;
            }
            return !(d_locked.exchange(true, std::memory_order_acquire));*/
            
            // more efficient version
            while (d_locked.exchange(true, std::memory_order_acquire)) {
                if ((++count == spin_count))
                    return false;
                std::this_thread::yield();
            }
            return true;
        }

        template<class Clock, class Duration>
        SEQ_ALWAYS_INLINE bool try_fast(const std::chrono::time_point<Clock, Duration>& timePoint) noexcept
        {
            unsigned count = 0;

            while (d_locked.exchange(true, std::memory_order_acquire)) {
                if ((++count == spin_count) || std::chrono::system_clock::now() > timePoint)
                    return false;
                std::this_thread::yield();
            }
            return true;
        }

        void notify()
        {
            std::lock_guard<std::mutex> ul(data()->d_mutex);
            data()->d_cond.notify_one();
        }

    public:
        spin_mutex()
            :d_data(NULL), d_locked(false) {}

        ~spin_mutex()
        {
            if (is_locked()) {
                fprintf(stderr, "spin_mutex destroyed while locked!");
                fflush(stderr);
                std::terminate();
            }
            // If the mutex is lock while destroyed, we accept to leak
            else if (internal_data* d = data())
                delete d;
        }

        void lock()
        {
            if (!d_locked.exchange(true, std::memory_order_acquire))
                return;
            if (!try_fast())
                lock_slow();
        }
        void unlock() {
            d_locked.store(false, std::memory_order_release);
            if (d_data && data()->d_ref > 0)
                notify();
        }

        bool try_lock() noexcept {
            // First do a relaxed load to check if lock is free in order to prevent
            // unnecessary cache misses if someone does while(!try_lock())
            return !d_locked.load(std::memory_order_relaxed) &&
                !d_locked.exchange(true, std::memory_order_acquire);
        }

        bool is_locked() const noexcept { return d_locked.load(std::memory_order_relaxed); }

        template<class Rep, class Period>
        bool try_lock_for(const std::chrono::duration<Rep, Period>& duration)
        {
            return try_lock_until(std::chrono::system_clock::now() + duration);
        }

        template<class Clock, class Duration>
        bool try_lock_until(const std::chrono::time_point<Clock, Duration>& timePoint)
        {
            if (!d_locked.exchange(true, std::memory_order_acquire))
                return true;
            if (!try_fast(timePoint)) {
                if (std::chrono::system_clock::now() > timePoint)
                    return false;
                return lock_slow(timePoint);
            }
            return true;
        }
    };

    /// @brief A read-write mutex based on either seq::spinlock class or seq::spin_mutex class.
    /// @tparam Mutex either seq::spinlock or seq::spin_mutex class
    /// 
    /// read_write_mutex follows the TimedMutex requirements.
    /// 
    template<class Mutex = spin_mutex<> >
    class read_write_mutex
    {
        read_write_mutex(const read_write_mutex&) = delete;
        read_write_mutex(read_write_mutex&&) = delete;
        read_write_mutex& operator=(const read_write_mutex&) = delete;
        read_write_mutex& operator=(read_write_mutex&&) = delete;

        std::atomic<unsigned>   d_readers{ 0 };
        Mutex                   d_mutex;

    public:
        read_write_mutex() { }

        ~read_write_mutex()
        {
            if (is_locked()) {
                fprintf(stderr, "read_write_mutex destroyed while locked!");
                fflush(stderr);
                std::terminate();
            }
        }

        /// Locks the mutex for exclusive access (e.g. for a write operation).
        /// If another thread has already locked the mutex, a call to lock will block execution until the lock is acquired.
        void lock()
        {
            d_mutex.lock(); // Lock writing mutex
            // Wait for all readers to finish.
            while (d_readers != 0) {
                std::this_thread::yield(); // Give the reader-threads a chance to finish.
            }
        }

        /// Tries to lock the mutex for exclusive access (e.g. for a write operation).
        bool try_lock()
        {
            if (!d_mutex.try_lock())
                return false;

            if (d_readers == 0)
                return true;

            d_mutex.unlock();
            return false;
        }

        bool is_locked() const noexcept
        {
            return d_mutex.is_locked();
        }
        bool is_locked_shared() const noexcept
        {
            return d_mutex.is_locked() || d_readers != 0;
        }

        /// Unlocks the mutex.
        void unlock()
        {
            d_mutex.unlock();
        }

        template<class Rep, class Period>
        bool try_lock_for(const std::chrono::duration<Rep, Period>& duration)
        {
            return try_lock_until(std::chrono::system_clock::now() + duration);
        }

        template<class Clock, class Duration>
        bool try_lock_until(const std::chrono::time_point<Clock, Duration>& timePoint)
        {
            if (!d_mutex.try_lock_until(timePoint)) // Lock writing mutex
                return false;

            // Wait for all readers to finish.
            while (d_readers != 0) {
                if (std::chrono::system_clock::now() > timePoint) {
                    d_mutex.unlock();
                    return false;
                }
                std::this_thread::yield(); // Give the reader-threads a chance to finish.
            }
        }

        /// Acquires shared ownership of the mutex (e.g. for a read operation).
        void lock_shared()
        {
            // Check for write in progress
            d_mutex.lock();
            // Increment readers count
            ++d_readers;
            d_mutex.unlock();
        }

        /// Tries to lock the mutex in shared mode.
        bool try_lock_shared()
        {
            if (!d_mutex.try_lock())
                return false;

            // Increment readers count
            ++d_readers;
            d_mutex.unlock();
            return true;
        }

        /// Releases the mutex from shared ownership by the calling thread.
        void unlock_shared()
        {
            --d_readers;
        }

    };

    /// @brief Dumy lock class that basically does nothing
    ///
    struct null_lock
    {
        void lock()noexcept {}
        void unlock()noexcept {}
        bool try_lock() const noexcept { return false; }
        bool is_locked() const noexcept { return false; }
        template<class Rep, class Period>
        bool try_lock_for(const std::chrono::duration<Rep, Period>& duration) { return false; }
        template<class Clock, class Duration>
        bool try_lock_until(const std::chrono::time_point<Clock, Duration>& timePoint) { return false; }
    };

    
}

/** @}*/
//end lock
