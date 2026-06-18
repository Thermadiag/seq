#include "seq/timer.hpp"
#include "seq/radix_hash_map.hpp"
#include "robin_hood/robin_hood.h"
#include <tsl/sparse_set.h>
#include "ankerl/unordered_dense.h"
#include <gtl/phmap.hpp>

#include "testing.hpp"
#include <random>
#include <iostream>
#include <fstream>
#include <unordered_set>

using namespace seq;


inline void _reset_memory_usage()
{
#if defined(WIN32) || defined(_WIN32)
	SetProcessWorkingSetSize(GetCurrentProcess(), static_cast<SIZE_T>(-1), static_cast<SIZE_T>(-1));
#endif
}

inline auto _get_memory_usage() -> size_t
{
#if defined(WIN32) || defined(_WIN32)
	static HANDLE currentProcessHandle = GetCurrentProcess();
	PROCESS_MEMORY_COUNTERS_EX memoryCounters; // = { 0 };;
	memset(&memoryCounters, 0, sizeof(memoryCounters));
	if (GetProcessMemoryInfo(currentProcessHandle, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memoryCounters), sizeof(memoryCounters))) {
		return /*memoryCounters.PrivateUsage + */ memoryCounters.WorkingSetSize;
	}
	return 0;
#else
	return 0;
#endif
}


static void measure_max_mem(std::atomic<bool>& start, std::atomic<bool>& stop, std::atomic<size_t>& max_mem)
{
	while (!start)
		;

	while (!stop) {
		size_t mem = _get_memory_usage();
		auto max = max_mem.load();

		if (mem > 100000000000ull) { // more than 100GB
			std::this_thread::yield();
			continue;
		}

		while (mem > max) {
			if (max_mem.compare_exchange_strong(max,mem))
				break;
			mem = std::max(mem, _get_memory_usage());
		}
	}
}

struct LatenctyMem
{
	uint64_t max_nano = 0;
	uint64_t max_mem = 0;
};

struct LatenctyMemResult
{
	std::vector<LatenctyMem> results;
	std::string name;
};

struct LatenctyFind
{
	uint64_t max_find = 0;
	uint64_t max_failed = 0;
};

struct LatenctyFindResult
{
	std::vector<LatenctyFind> results;
	std::string name;
};

#define MAX_POINTS 1000

template<class T>
LatenctyMemResult test_hash_map_insert(const char* name)
{
	static constexpr uint64_t multiplier = 10000;
	_reset_memory_usage();
	T map;

	std::mt19937 e(0);
	std::uniform_int_distribution<uint64_t> rng;
	LatenctyMemResult ret;
	ret.results.resize(MAX_POINTS);
	ret.name = name;
	size_t count = ret.results.size() * multiplier;

	std::atomic<bool> start = false, stop=false;
	std::atomic<size_t> max_mem = 0;
	std::thread th([&]() { measure_max_mem(start, stop, max_mem); });

	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	size_t start_mem = _get_memory_usage();
	start = true;
	
	timer t;
	uint64_t el_max = 0;

	for (size_t i = 0; i< count; ++i) {
		auto val = rng(e);
		t.tick();
		map.emplace(val);
		auto el = t.tock();
		el_max = std::max(el, el_max);

		std::this_thread::yield();

		if (i % multiplier == 0) {
			ret.results[i / multiplier] = { el_max, max_mem.load() - start_mem };
			el_max = 0;
		}
	}

	stop = true;
	th.join();

	// Find max time/max mem
	uint64_t max_memv = 0;
	uint64_t max_nanov = 0;
	for (auto& m : ret.results) {
		max_memv = std::max(max_memv, m.max_mem);
		max_nanov = std::max(max_nanov, m.max_nano);
	}

	std::cout << name << ": max " << (double)max_memv / (1024 * 1024) << " MB, " << max_nanov << " ns" << std::endl;

	return ret;
}


template<class T>
LatenctyFindResult test_hash_map_find(const char* name)
{
	static constexpr uint64_t multiplier = 10000;
	T map;

	LatenctyFindResult ret;
	ret.results.resize(MAX_POINTS);
	ret.name = name;
	size_t count = ret.results.size() * multiplier;

	timer t;
	uint64_t el_max = 0;
	uint64_t el_max_fail = 0;
	std::vector<uint64_t> rn_vals(multiplier);

	for (size_t i = 0; i < count; ++i) {
		auto val = i;
		map.emplace(val);
		
		if (i && i % multiplier == 0) {

			{
				std::mt19937 e(i);
				std::uniform_int_distribution<uint64_t> rng(0, i);
				// Find the last multiplier values;
				for (auto& v : rn_vals)
					v = rng(e);
				
				t.tick();
				for (size_t j = 0; j < 100; ++j) {
					SEQ_TEST(map.count(rn_vals[j]) == 1);
				}
				auto el = t.tock();
				el_max = el / 100;
			}
			std::mt19937 e(i);
			std::uniform_int_distribution<uint64_t> rng(i+1, std::numeric_limits<uint64_t>::max());
			for (auto& v : rn_vals)
				v = rng(e);
			// Find fail multiplier values;
			t.tick();
			for (size_t j = 0; j < 100; ++j) {				
				SEQ_TEST(map.count(rn_vals[j]) == 0);
			}
			auto el = t.tock();
			el_max_fail = el / 100;

			ret.results[i / multiplier] = { el_max, el_max_fail };
			el_max = el_max_fail = 0;
		}
	}

	// Find max time/max mem
	uint64_t max_find = 0;
	uint64_t max_find_fail = 0;
	for (auto& m : ret.results) {
		max_find = std::max(max_find, m.max_find);
		max_find_fail = std::max(max_find_fail, m.max_failed);
	}

	std::cout << name << ": max find " << max_find << " ns, fail " << max_find_fail << " ns" << std::endl;

	return ret;
}




bool to_csv(const std::vector<LatenctyMemResult>& res, const char* filename)
{
	std::ofstream fout(filename);
	if (!fout)
		return false;
	fout << "\"sep=\t\"" << std::endl;

	fout << "Count\t";
	for (size_t i = 0; i < res.size(); ++i) {
		fout << res[i].name << " peak mem(MB)\t" << res[i].name << " peak latency(ns)\t";
	}
	fout << std::endl;

	for (size_t c = 0; c < MAX_POINTS; ++c) {
		fout << c * MAX_POINTS << "\t";
		//auto& vec = res[c];
		for (size_t i = 0; i < res.size(); ++i) {
			fout << (double)res[i].results[c].max_mem/(1024*1024) << "\t" << res[i].results[c].max_nano << "\t";
		}
		fout << std::endl;
	}

	return true;
}

bool to_csv(const std::vector<LatenctyFindResult>& res, const char* filename)
{
	std::ofstream fout(filename);
	if (!fout)
		return false;
	fout << "\"sep=\t\"" << std::endl;

	fout << "Count\t";
	for (size_t i = 0; i < res.size(); ++i) {
		fout << res[i].name << "\t" << res[i].name << "\t";
	}
	fout << std::endl;

	for (size_t c = 0; c < MAX_POINTS; ++c) {
		fout << c * MAX_POINTS << "\t";
		// auto& vec = res[c];
		for (size_t i = 0; i < res.size(); ++i) {
			fout << res[i].results[c].max_find  << "\t" << res[i].results[c].max_failed << "\t";
		}
		fout << std::endl;
	}

	return true;
}




int bench_hash_latency(int, char** const) 
{
	using Hash = seq::hasher<uint64_t>;
	
	/* {
		std::vector<LatenctyMemResult> ret;
		ret.push_back(test_hash_map_insert<tsl::sparse_set<uint64_t, Hash>>("tsl::sparse_set"));
		ret.push_back(test_hash_map_insert<gtl::flat_hash_set<uint64_t, Hash>>("gtl::flat_hash_set"));
		ret.push_back(test_hash_map_insert<ankerl::unordered_dense::set<uint64_t, Hash>>("ankerl::unordered_dense::set"));
		ret.push_back(test_hash_map_insert<seq::radix_hash_set<uint64_t, Hash>>("seq::radix_hash_set"));
		ret.push_back(test_hash_map_insert<std::unordered_set<uint64_t, Hash>>("std::unordered_set"));
		to_csv(ret, "bench_hash_latency.csv");
	}*/
	{
		std::vector<LatenctyFindResult> ret;
		ret.push_back(test_hash_map_find<tsl::sparse_set<uint64_t, Hash>>("tsl::sparse_set"));
		ret.push_back(test_hash_map_find<gtl::flat_hash_set<uint64_t, Hash>>("gtl::flat_hash_set"));
		ret.push_back(test_hash_map_find<ankerl::unordered_dense::set<uint64_t, Hash>>("ankerl::unordered_dense::set"));
		ret.push_back(test_hash_map_find<seq::radix_hash_set<uint64_t, Hash>>("seq::radix_hash_set"));
		ret.push_back(test_hash_map_find<std::unordered_set<uint64_t, Hash>>("std::unordered_set"));
		to_csv(ret, "bench_hash_find_latency.csv");
	}
	return 0;
}