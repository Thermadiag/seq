# seq v2.2

First version using AI, and only for code review (no code/documentation generation).

-	All associative containers: iterators dereferencing now return `std::pair<const Key, Value>` instead of plain `std::pair<Key, Value>`.
-	All containers have been reviewed to correct allocator propagation behavior.
-	All containers have been reviewed to correct potentially wrong exception guarantees.
-	Swiss table based containers (`radix_set/map`, `radix_hash_set/map`, `concurrent_set/map`) now support NEON instruction set.
-	Concurrent containers (`concurrent_set/map`, `concurrent_queue`) behavior have been strengthen.
-	All containers have been optimized on way or another.	


# seq v2.1


The version 2.1 introduced additional breaking changes:
-	The modules *charconv* and *format* are now deprecated. They are still available, but moved to the *legacy* folder.
	Indeed, while I really like these, they do not belong to a library about containers. They will eventually be moved to another library, and removed from *seq*.
-	The *tagged_pointer.hpp* file have been moved to the *internal* folder (private API).

Additional changes:

-	The hashing framework has a better handling of transparent keys.
-	The hashing framework now supports std::chrono::time_point and std::chrono::duration.
-	The `seq::net_sort` algorithm has been slightly optimized/refactored and moved to *net_sort.hpp* header.
-	The `radix_set/map` lower_bound() method has been corrected (compilation error).
-	`radix_set/map` now supports std::chrono::time_point and std::chrono::duration as key.
-	`radix_set/map` no longer have typedef `prefix_iterator` and `prefix_const_iterator`. Instead, member prefix_range() now returns a std::pair of regular iterators. 
-	All radix-based containers (`seq::radix_set/map`, `seq::radix_hash_set/map`) have been internally simplified.
-	The `seq::hold_any` class has been refactored and simplified. A `seq::hold_any` containing a char* or const char* is now considered holding a string. Comparison of `seq::hold_any` containing a `char*` or `const char*` results in string comparison.
-	New container: [`seq::concurrent_queue`](concurrent_queue.md).

The markdown documentation has been updated accordingly, and new benchmark results were added. Full list of benchmarks now:
-	Benchmark of [concurrent hash tables](concurrent_map.md) at the end of the page. Its goal is to compare `seq::concurrent_set/map` to other implementations.
-	Benchmark on [sorted containers](sorted_benchmark.md). Its goal is to compare `seq::flat_set/map` and `seq::radix_set/map` to other implementations.
-	Very tiny benchmark on [concurrent queues](concurrent_queue.md) to compare `seq::concurrent_queue` with other implementations.
-	[Memory and latency benchmark](latency_benchmark.md) on hash tables to compare `seq::radix_hash_set/map` with other implementations.



# seq v2.0


The version 2 of `seq` introduced several changes on all modules, which are listed below.

## Library wide changes


-	The biggest change is the library requirement which was upgraded to C++17. Indeed, working with C++14 was painfull and all compilers I now work with support at least C++17.
-	Another big change is the full removal of the `cvector` class (compressed vector-like container). Indeed, `cvector` relied on a compression algorithm that was heavily refactored and upgraded, up to the point where it did not belong to a library about containers...
	Therefore, the compression algorithm and the `cvector` class were moved to a new open source project called <a href="https://github.com/Thermadiag/stenos">stenos</a>.
-	The library is now header-only library thanks to the removal of `cvector` class.
-	[Pdqsort](https://github.com/orlp/pdqsort) is not used anymore within the library. Instead the `net_sort` algorithm (from [algorithm](algorithm.md) module) is used everywhere.
-	The `memory` module (deprecated in v1.3) was removed.

## [bits](bits.md)

-	Internal refactoring.
-	Updated SEQ_LIKELY/SEQ_UNLIKELY to use c++20 [[likely]]/[[unlikely]] attributes if available.
-	Added class `fast_rand`: fast 32 bits random number generator.

## [hash](hash.md)

-	Internal refactoring.
-	Moved implementation which was in `hash.cpp` to `hash_impl.hpp`


## [charconv](charconv.md)

-	Internal refactoring
-	Removed file `charconv.cpp`
-	All functions to read/write integral/floating numbers are now template, and work on any character type instead of just `char`.

## [format](format.md)

-	Full refactoring of the module.
-	All functions now work with any character type including `wchar_t`, `char16_t`, `char32_t` and `char8_t` (if available).

## [any](any.md)

-	Minor refactoring to simplify the code.
-	Now relies on seq::hasher instead of std::hash.

## [containers](containers.md)

-	Sequential random-access containers: 
	-	[seq::devector](devector.md):
		-	Minor refactoring.
		-	Corrected a memory leak in shrink_to_fit().
		-	Updated the internal strategy when growing from front or back: the data are not anymore moved in the middle of the array in case of front/back growing.
		This allows to get rid of the last template parameter `DEVectorFlag`. devector now behaves almost exactly like the QVector class.
	-	[seq::tiered_vector](tiered_vector.md): 
		-	Minor refactoring and optimizatoins.
		-	Update of the iterator class which was not detected as random access by c++20 concept random_access_iterator.
-	Sequential stable non random-access container: `seq::sequence`: minor refactoring. Switched from pdqsort to net_sort as sorting algorithm.
-	Sorted containers: 
	-	[seq::flat_set](flat_set.md), `seq::flat_map`, `seq::flat_multiset`, `seq::flat_multimap`: 
		-	Internal refactoring and optimizations, huge code simplification.
		-	Better support of transparent comparison functions.
		-	Switched from pdqsort to net_sort as sorting algorithm.
		-	Removed the possibility to modify the underlying tiered_vector container.
		-	Member functions `tvector()` and `ctvector()` where gathered and renamed in a single const member `container()`
		-	Added C++23 members extract() and replace()
	-	[seq::radix_set](radix_tree.md) and `seq::radix_map`:
		- Full refactoring to simplify the class.
		- It now supports any kind of key that has `data()` and `size()` members (all string classes, std::vector, std::array...)
		- The tree is now rebalanced when erasing keys in order to reduce its memory footprint.
-	Hash tables: 
	-	[seq::ordered_set](ordered_set.md) and `seq::ordered_map`: minor refactoring
	-	[seq::radix_hash_set](radix_tree.md) and `seq::radix_hash_map`: 
		-	Same huge refactoring as [seq::radix_set](radix_tree.md).
		-	Better handling of transparent hash/comparison functions.
	-	[seq::concurrent_map](concurrent_map.md) and `seq::concurrent_set`:
		-	Internal refactoring and optimizations.
		-	Added mechanisms to avoid busy wait on shard rehash. This increases performances on insert operations.
-	Strings:
	-	[seq::tiny_string](tiny_string.md): minor changes in the `find_*` functions. Most of them now use std::basic_string_view.
	-	Added several type traits to help detect string types and character types.
	

## [algorithm](algorithm.md)


New module, provides several iterator based algorithms including the `net_sort` sorting one.


