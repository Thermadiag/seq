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

#ifndef SEQ_TESTING_HPP
#define SEQ_TESTING_HPP


#if defined( WIN32) || defined(_WIN32)
#include <Windows.h>
#include <Psapi.h>
#else 
#include <time.h>
#endif

#include <chrono>
#include <iostream>
#include <string>
#include <stdexcept>
#include <sstream>
#include <random>
#include <algorithm>

#include "bits.hpp"
#include "format.hpp"

namespace seq
{
	/// @brief Exception thrown for failed tests
	class test_error : public std::runtime_error
	{
	public:
		test_error(const std::string & str)
			:std::runtime_error(str) {}
	};

	/// @brief Streambuf that stores the number of outputed characters
	class streambuf_size : public std::streambuf 
	{
		std::streambuf* sbuf{ NULL };
		std::ostream* oss{ NULL };
		size_t size{ 0 };

		int overflow(int c) {
			size++;
			return sbuf->sputc(c);
		}
		int sync() { return sbuf->pubsync(); }
	public:
		streambuf_size(std::ostream& o) : sbuf(o.rdbuf()), oss(&o) { oss->rdbuf(this); }
		~streambuf_size() { oss->rdbuf(sbuf); }
		size_t get_size() const { return size; }
	};
}



/// @brief Very basic testing macro that throws seq::test_error if condition is not met.
#define SEQ_TEST( ... ) \
	if(! (__VA_ARGS__) ) {std::string v =seq::fmt(__LINE__);  throw seq::test_error(("testing error at file " __FILE__ "(" + v + "): "  #__VA_ARGS__).c_str()); }

/// @brief Test if writting given argument to a std::ostream produces the string 'result', throws seq::test_error if not.
#define SEQ_TEST_TO_OSTREAM( result, ... ) \
	{std::ostringstream oss; \
	oss <<(__VA_ARGS__) ; oss.flush() ; \
	if( oss.str() != result) \
		{std::string v =seq::fmt(__LINE__);  throw seq::test_error(("testing error at file " __FILE__ "(" + v + "): \"" + std::string(result) + "\" == "  #__VA_ARGS__).c_str());} \
	}

/// @brief Test if given statement throws a 'exception' object. If not, throws seq::test_error.
#define SEQ_TEST_THROW(exception, ...) \
	{bool has_thrown = false;  \
	try { __VA_ARGS__; } \
	catch(const exception &) {has_thrown = true;} \
	catch(...) {} \
	if(! has_thrown ) {std::string v =seq::fmt(__LINE__);  \
		throw seq::test_error(("testing error at file " __FILE__ "(" + v + "): "  #__VA_ARGS__).c_str()); } \
	}

/// @brief Test module
#define SEQ_TEST_MODULE(name, ... ) \
	{ seq::streambuf_size str(std::cout); size_t size = 0; bool ok = true; \
	try { std::cout << "TEST MODULE " << #name << "... " ; std::cout.flush(); size = str.get_size(); __VA_ARGS__; } \
	catch (const test_error& e) {std::cout<< std::endl; ok = false; std::cerr << "TEST FAILURE IN MODULE " << #name << ": " << e.what() << std::endl; } \
	catch (const std::exception& e) { std::cout<< std::endl; ok = false; std::cerr << "UNEXPECTED ERROR IN MODULE " << #name << " (std::exception): " << e.what() << std::endl; } \
	catch (...) { std::cout<< std::endl; ok = false;  std::cerr << "UNEXPECTED ERROR IN MODULE " << #name << std::endl; }\
	if(ok) { if(str.get_size() != size) std::cout<<std::endl; std::cout<< "SUCCESS" << std::endl; } \
	}


namespace seq
{
	namespace detail
	{
		static inline auto msecs_since_epoch() -> int64_t
		{
			using namespace std::chrono;
			return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
		}


#if defined( WIN32) || defined(_WIN32)


		/*using high_def_timer = struct high_def_timer {
			LARGE_INTEGER timer;
			LARGE_INTEGER freq;
		};

		inline void start_timer(high_def_timer* timer)
		{
			QueryPerformanceCounter(&timer->timer);
			QueryPerformanceFrequency(&timer->freq);
		}

		inline auto elapsed_microseconds(high_def_timer* timer) -> std::uint64_t
		{
			LARGE_INTEGER end;
			QueryPerformanceCounter(&end);
			std::uint64_t quad = end.QuadPart - timer->timer.QuadPart;
			return (quad * 1000000ULL) / timer->freq.QuadPart;
		}*/
#else


		/*typedef struct timespec high_def_timer;

		inline void start_timer(high_def_timer* timer)
		{
			clock_gettime(CLOCK_PROCESS_CPUTIME_ID, timer);
		}

		// call this function to end a timer, returning nanoseconds elapsed as a long
		inline std::uint64_t elapsed_microseconds(high_def_timer* timer) {
			struct timespec end_time;
			clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end_time);
			return (end_time.tv_sec - timer->tv_sec) * (std::uint64_t)1e6 + (end_time.tv_nsec - timer->tv_nsec) / 1000ULL;
		}*/

#endif

		/*static inline auto get_timer() -> high_def_timer&
		{
			static thread_local high_def_timer timer;
			return timer;
		}*/
		static inline std::chrono::time_point<std::chrono::high_resolution_clock>& get_clock()
		{
			thread_local std::chrono::time_point<std::chrono::high_resolution_clock> clock;
			return clock;
		}
	}


	/// @brief For tests only, reset timer for calling thread
	void tick()
	{
		detail::get_clock() = std::chrono::high_resolution_clock::now();
		//detail::start_timer(&detail::get_timer());
	}
	/// @brief For tests only, returns elapsed microseconds since last call to tick()
	auto tock_us() -> std::uint64_t
	{
		return  std::chrono::duration_cast<std::chrono::microseconds>( std::chrono::high_resolution_clock::now() - detail::get_clock()).count();
		//return detail::elapsed_microseconds(&detail::get_timer());
	}
	/// @brief For tests only, returns elapsed milliseconds since last call to tick()
	auto tock_ms() -> std::uint64_t
	{
		return  std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - detail::get_clock()).count();
		//return detail::elapsed_microseconds(&detail::get_timer()) / 1000ULL;
	}
	
	/// @brief Similar to C++11 (and deprecated) std::random_shuffle
	template<class Iter>
	void random_shuffle(Iter begin, Iter end)
	{
		std::random_device rd;
		std::mt19937 g(rd());
		std::shuffle(begin,end, g);
	}


	void reset_memory_usage()
	{
#if defined( WIN32) || defined(_WIN32)
			SetProcessWorkingSetSize(GetCurrentProcess(), -1, -1);
#endif
	}

	auto get_memory_usage() -> size_t
	{
#if defined( WIN32) || defined(_WIN32)
			Sleep(50);
			HANDLE currentProcessHandle = GetCurrentProcess();
			PROCESS_MEMORY_COUNTERS_EX   memoryCounters;// = { 0 };;
			memset(&memoryCounters, 0, sizeof(memoryCounters));
			if (GetProcessMemoryInfo(currentProcessHandle, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memoryCounters), sizeof(memoryCounters)))
			{
				return /*memoryCounters.PrivateUsage + */memoryCounters.WorkingSetSize;
			}
			return 0;
#else
			return 0;
#endif

	}



	template<typename Ch, typename Traits = std::char_traits<Ch> >
	struct basic_nullbuf : std::basic_streambuf<Ch, Traits> {
		using base_type = std::basic_streambuf<Ch, Traits>;
		using int_type = typename base_type::int_type;
		using traits_type = typename base_type::traits_type;

		virtual auto overflow(int_type c) -> int_type {
			return traits_type::not_eof(c);
		}
	};

	/// @brief For tests only, alias for null buffer, to be used with c++ iostreams
	using nullbuf = basic_nullbuf<char>;

	template<class T>
	void print_null(const T& v)
	{
		static nullbuf n;
		auto b = std::cout.rdbuf(&n);
		std::cout << v << std::endl;
		std::cout.rdbuf(b);
	}


	/// @brief For tests only, generate a random string of given max size
	template<class String>
	auto generate_random_string(int max_size, bool fixed = false) -> String
	{
		size_t size = static_cast<size_t>(fixed ? max_size : rand() % max_size);
		String res(size, 0);
		for (size_t i = 0; i < size; ++i)
			res[i] = (rand() & 63) + 33;

		return res;
	}



	namespace detail
	{
		template<class T>
		struct Multiply
		{
			static auto multiply(T value) -> T {
				return static_cast<T>((static_cast<int>(rand()) + static_cast<int>(rand())) * 14695981039346656037ULL * value);
			}
			template<class Stream>
			static auto read(Stream& str) -> T { T r;  seq::from_stream(str, r); return r; }
		};
		template<>
		struct Multiply<long double>
		{
			static auto multiply(long double value) -> long double {
				return static_cast<long double>((static_cast<long double>(rand()) + static_cast<long double>(rand()))) * 1.4695981039346656037 * value;
			}
			template<class Stream>
			static auto read(Stream& str) -> long double { long double r; seq::from_stream(str, r); return r; }
		} ;
		template<>
		struct Multiply<double>
		{
			static auto multiply(double value) -> double {
				return static_cast<double>((static_cast<double>(rand()) + static_cast<double>(rand()))) * 1.4695981039346656037 * value;
			}
			template<class Stream>
			static auto read(Stream& str) -> double { double r; seq::from_stream(str, r); return r; }
		} ;
		template<>
		struct Multiply<float>
		{
			static auto multiply(float value) -> float {
				return static_cast<float>((static_cast<float>(rand()) + static_cast<float>(rand())) * 1.4695981039346656037 * value);
			}
			template<class Stream>
			static auto read(Stream& str) -> float { float r;  seq::from_stream(str, r); return r; }
		} ;
	}

	/// @brief For tests only, generate random floating point number on the whole representable range (including potential infinit values)
	template<class Float>
	class random_float_genertor
	{
		static constexpr unsigned mask = sizeof(Float) == 4 ? 31 : sizeof(Float) == 8 ? 255 : 4095;

		unsigned count;

	public:
		random_float_genertor(unsigned seed=0)
			:count(0)
		{
			srand(seed);
		}

		auto operator()() -> Float
		{
			const bool type = rand() & 1;
			Float sign1 = (rand() & 1) ? static_cast<Float>(-1) : static_cast<Float>(1);
			Float res;
			if (type)
				res = (sign1 * static_cast<Float>(detail::Multiply<Float>::multiply(static_cast<Float>(count++ * rand()))));
			else
				res = (sign1 * static_cast<Float>(detail::Multiply<Float>::multiply(static_cast<Float>(count++ * rand())) * std::pow(static_cast<Float>(10.), static_cast<Float>(sign1) * (rand() & mask))));

			return res;
		}
	};


}
#endif