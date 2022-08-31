#ifndef SEQ_TESTING_H
#define SEQ_TESTING_H

#include <chrono>
#include <fstream>
#include <vector>
#include <iostream>

#ifdef WIN32
#include <windows.h>
#include <psapi.h>
#else 
#include <time.h>
#endif


#include "bits.hpp"
#include "format.hpp"


/// @brief Very basic testing macro that throws std::runtime_error if condition is not met
#define SEQ_TEST_ASSERT( ... ) \
	if(! (__VA_ARGS__) ) {tstring v =seq::fmt(__LINE__);  \
		throw std::runtime_error(("testing error: file " __FILE__ "(" + v + ")").c_str()); }

namespace seq
{
	namespace detail
	{
		static inline int64_t msecs_since_epoch()
		{
			using namespace std::chrono;
			return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
		}


#ifdef WIN32


		typedef struct high_def_timer {
			LARGE_INTEGER timer;
			LARGE_INTEGER freq;
		} high_def_timer;

		inline void start_timer(high_def_timer* timer)
		{
			QueryPerformanceCounter(&timer->timer);
			QueryPerformanceFrequency(&timer->freq);
		}

		inline std::uint64_t elapsed_microseconds(high_def_timer* timer)
		{
			LARGE_INTEGER end;
			QueryPerformanceCounter(&end);
			std::uint64_t quad = end.QuadPart - timer->timer.QuadPart;
			return (quad * 1000000ULL) / timer->freq.QuadPart;
		}

#else


		typedef struct timespec high_def_timer;

		inline void start_timer(high_def_timer* timer)
		{
			clock_gettime(CLOCK_PROCESS_CPUTIME_ID, timer);
		}

		// call this function to end a timer, returning nanoseconds elapsed as a long
		inline std::uint64_t elapsed_microseconds(high_def_timer* timer) {
			struct timespec end_time;
			clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end_time);
			return (end_time.tv_sec - timer->tv_sec) * (std::uint64_t)1e6 + (end_time.tv_nsec - timer->tv_nsec) / 1000ULL;
		}

#endif

		static inline high_def_timer& get_timer()
		{
			static thread_local high_def_timer timer;
			return timer;
		}
	}


	/// @brief For tests only, reset timer for calling thread
	void tick()
	{
		detail::start_timer(&detail::get_timer());
	}
	/// @brief For tests only, returns elapsed microseconds since last call to tick()
	std::uint64_t tock_us()
	{
		return detail::elapsed_microseconds(&detail::get_timer());
	}
	/// @brief For tests only, returns elapsed milliseconds since last call to tick()
	std::uint64_t tock_ms()
	{
		return detail::elapsed_microseconds(&detail::get_timer()) / 1000ULL;
	}
	
	


#ifdef WIN32

		/// @brief Equivalent to std::system without a command prompt on Windows
		int system(const char* command)
		{
			// Windows has a system() function which works, but it opens a command prompt window.

			char* tmp_command, * cmd_exe_path;
			DWORD ret = 0;
			LPDWORD         ret_val = &ret;
			size_t          len;
			PROCESS_INFORMATION process_info = { 0,0,0,0 };
			STARTUPINFOA        startup_info;// = { 0 };
			memset(&startup_info, 0, sizeof(startup_info));


			len = strlen(command);
			tmp_command = (char*)malloc(len + 4);
			tmp_command[0] = 0x2F; // '/'
			tmp_command[1] = 0x63; // 'c'
			tmp_command[2] = 0x20; // <space>;
			memcpy(tmp_command + 3, command, len + 1);

			startup_info.cb = sizeof(STARTUPINFOA);
#ifdef _MSC_VER
#pragma warning(suppress : 4996)
			cmd_exe_path = getenv("COMSPEC");
#else
			cmd_exe_path = getenv("COMSPEC");
#endif
			
			_flushall();  // required for Windows system() calls, probably a good idea here too

			if (CreateProcessA(cmd_exe_path, tmp_command, NULL, NULL, 0, CREATE_NO_WINDOW, NULL, NULL, &startup_info, &process_info)) {
				WaitForSingleObject(process_info.hProcess, INFINITE);
				GetExitCodeProcess(process_info.hProcess, ret_val);
				CloseHandle(process_info.hProcess);
				CloseHandle(process_info.hThread);
			}

			free((void*)tmp_command);

			return ret;
		}
#else
#define system std::system
#endif

		void reset_memory_usage()
		{
#ifdef WIN32
			SetProcessWorkingSetSize(GetCurrentProcess(), -1, -1);
#endif
		}

		size_t get_memory_usage()
		{
#ifdef WIN32
			Sleep(50);
			HANDLE currentProcessHandle = GetCurrentProcess();
			PROCESS_MEMORY_COUNTERS_EX   memoryCounters;// = { 0 };;
			memset(&memoryCounters, 0, sizeof(memoryCounters));
			if (GetProcessMemoryInfo(currentProcessHandle, (PROCESS_MEMORY_COUNTERS*)&memoryCounters, sizeof(memoryCounters)))
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
		typedef std::basic_streambuf<Ch, Traits> base_type;
		typedef typename base_type::int_type int_type;
		typedef typename base_type::traits_type traits_type;

		virtual int_type overflow(int_type c) {
			return traits_type::not_eof(c);
		}
	};

	/// @brief For tests only, alias for null buffer, to be used with c++ iostreams
	typedef basic_nullbuf<char> nullbuf;


	/*size_t to_int(const std::string& v) { return v.size(); }
	template<size_t S, class AL>
	size_t to_int(const tiny_string<S, AL>& v) { return v.size(); }
	template<class T>
	size_t to_int(const T& v) { return v; }
	template<class T, class U>
	size_t to_int(const std::pair<T, U>& v) { return to_int(v.first); }
		
	template<class T, class U>
	size_t to_int(const std::pair<const T, U>& v) { return to_int(v.first); }*/

	/// @brief For tests only, generate a random string of given max size
	template<class String>
	String generate_random_string(int max_size, bool fixed = false)
	{
		size_t size = (size_t)(fixed ? max_size : rand() % max_size);
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
			static T multiply(T value) {
				return static_cast<T>(((int)rand() + (int)rand()) * 14695981039346656037ULL * value);
			}
			template<class Stream>
			static T read(Stream& str) { T r;  seq::from_stream(str, r); return r; }
		};
		template<>
		struct Multiply<long double>
		{
			static long double multiply(long double value) {
				return static_cast<long double>(((long double)rand() + (long double)rand())) * 1.4695981039346656037 * value;
			}
			template<class Stream>
			static long double read(Stream& str) { long double r; seq::from_stream(str, r); return r; }
		};
		template<>
		struct Multiply<double>
		{
			static double multiply(double value) {
				return static_cast<double>(((double)rand() + (double)rand())) * 1.4695981039346656037 * value;
			}
			template<class Stream>
			static double read(Stream& str) { double r; seq::from_stream(str, r); return r; }
		};
		template<>
		struct Multiply<float>
		{
			static float multiply(float value) {
				return static_cast<float>(((float)rand() + (float)rand()) * 1.4695981039346656037 * value);
			}
			template<class Stream>
			static float read(Stream& str) { float r;  seq::from_stream(str, r); return r; }
		};
	}

	/// @brief For tests only, generate random floating point number on the whole representable range (including potential infinit values)
	template<class Float>
	class random_float_genertor
	{
		static constexpr unsigned mask = sizeof(Float) == 4 ? 31 : sizeof(Float) == 8 ? 255 : 4095;

		unsigned count;

	public:
		random_float_genertor(unsigned seed = 0)
			:count(0)
		{
			srand(seed);
		}

		Float operator()()
		{
			const bool type = rand() & 1;
			Float sign1 = (rand() & 1) ? (Float)-1 : (Float)1;
			Float res;
			if (type)
				res = (sign1 * static_cast<Float>(detail::Multiply<Float>::multiply(static_cast<Float>(count++ * rand()))));
			else
				res = (sign1 * static_cast<Float>(detail::Multiply<Float>::multiply(static_cast<Float>(count++ * rand())) * std::pow((Float)10., (Float)sign1 * (rand() & mask))));

			return res;
		}
	};


}
#endif