#ifndef SEQ_TESTING_HPP
#define SEQ_TESTING_HPP


#if defined( WIN32) || defined(_WIN32)
#include <Windows.h>
#include <Psapi.h>
#else 
#include <time.h>
#endif

#include <chrono>

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
		static inline auto msecs_since_epoch() -> int64_t
		{
			using namespace std::chrono;
			return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
		}


#if defined( WIN32) || defined(_WIN32)


		using high_def_timer = struct high_def_timer {
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

		static inline auto get_timer() -> high_def_timer&
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
	auto tock_us() -> std::uint64_t
	{
		return detail::elapsed_microseconds(&detail::get_timer());
	}
	/// @brief For tests only, returns elapsed milliseconds since last call to tick()
	auto tock_ms() -> std::uint64_t
	{
		return detail::elapsed_microseconds(&detail::get_timer()) / 1000ULL;
	}
	
	


#if defined( WIN32) || defined(_WIN32)

		/// @brief Equivalent to std::system without a command prompt on Windows
		auto system(const char* command) -> int
		{
			// Windows has a system() function which works, but it opens a command prompt window.

			char * tmp_command;
			char * cmd_exe_path;
			DWORD ret = 0;
			LPDWORD         ret_val = &ret;
			size_t          len = 0;
			PROCESS_INFORMATION process_info = { nullptr,nullptr,0,0 };
			STARTUPINFOA        startup_info;// = { 0 };
			memset(&startup_info, 0, sizeof(startup_info));


			len = strlen(command);
			tmp_command = static_cast<char*>(malloc(len + 4));
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

			if (CreateProcessA(cmd_exe_path, tmp_command, nullptr, nullptr, 0, CREATE_NO_WINDOW, nullptr, nullptr, &startup_info, &process_info) != 0) {
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


	/// @brief For tests only, generate a random string of given max size
	template<class String>
	auto generate_random_string(int max_size, bool fixed = false) -> String
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
			static auto multiply(T value) -> T {
				return static_cast<T>(((int)rand() + (int)rand()) * 14695981039346656037ULL * value);
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