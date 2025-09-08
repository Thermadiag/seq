

[![CTest](https://github.com/Thermadiag/seq/actions/workflows/build-linux.yml/badge.svg?branch=main)](https://github.com/Thermadiag/seq/actions/workflows/build-linux.yml)
[![CTest](https://github.com/Thermadiag/seq/actions/workflows/build-macos.yml/badge.svg?branch=main)](https://github.com/Thermadiag/seq/actions/workflows/build-macos.yml)
[![CTest](https://github.com/Thermadiag/seq/actions/workflows/build-windows.yml/badge.svg?branch=main)](https://github.com/Thermadiag/seq/actions/workflows/build-windows.yml)

Transitioning to v2.0
---------------------

*seq* v2.0 introduced a lot of changes. See this [note](docs/v2.md) for more details and explanations.

Purpose
-------

The *seq* library is a header-only collection of original C++17 STL-like containers and related tools.

*seq* library does not try to reimplement already existing container classes present in other libraries like <a href="https://github.com/facebook/folly">folly</a>, <a href="https://abseil.io/">abseil</a>, <a href="https://www.boost.org/">boost</a> and (of course) std. Instead, it provides new features (or a combination of features) that are usually not present in other libraries. Some low level API like bits manipulation or hashing functions are not new, but must be defined to keep the seq library self dependent.

Among other things (see modules below), the *seq* library defines several container classes as alternatives to STL containers or providing features not present in the STL.
These containers generally adhere to the properties of STL containers (in C++17 version), though there are often some associated API differences and/or implementation details which differ from the standard library.

The *seq* containers are not necessarly drop-in replacement for their STL counterparts as they usually provide different iterator/reference statibility rules or different exception guarantees.

Currently, the *containers* module provide 5 types of containers:
-	Sequential random-access containers: 
	-	[seq::devector](docs/devector.md): double ended vector that optimized for front and back operations. Similar interface to `std::deque`.
	-	[seq::tiered_vector](docs/tiered_vector.md): tiered vector implementation optimized for fast insertion and deletion in the middle. Similar interface to `std::deque`.
-	Sequential stable non random-access container: `seq::sequence`, fast stable list-like container.
-	Sorted containers: 
	-	[seq::flat_set](docs/flat_set.md) : flat set container similar to boost::flat_set but based on seq::tiered_vector and providing fast insertion/deletion of single elements, while keeping the fast lookup performances of flat containers.
	-	`seq::flat_map`: associative version of `seq::flat_set`.
	-	`seq::flat_multiset`: similar to `seq::flat_set` but supporting duplicate keys.
	-	`seq::flat_multimap`: similar to `seq::flat_map` but supporting duplicate keys.
	-	[seq::radix_set](docs/radix_tree.md) : radix based (derived from the *Burst Trie*) sorted container with a similar interface to `std::set`. One of the fastest `std::set` like data structure for all operations, especially point lookup.
	-	`seq::radix_map`: associative version of `seq::radix_set`.
-	Hash tables: 
	-	[seq::ordered_set](docs/ordered_set.md): Ordered robin-hood hash table with backward shift deletion. Drop-in replacement for `std::unordered_set` (except for the bucket and node interface) with iterator/reference stability, with performances close to 'flat' hash maps. `seq::ordered_set` preserves the insertion order.
	-	`seq::ordered_map`: associative version of `seq::ordered_set`.
	-	[seq::radix_hash_set](docs/radix_tree.md): radix based hash table with a similar interface to `std::unordered_set`. Uses incremental rehash (no memory peak) with a very small memory footprint.
	-	`seq::radix_hash_map`: associative version of `seq::radix_hash_set`.
	-	[seq::concurrent_map](docs/concurrent_map.md) and `seq::concurrent_set`: higly scalable concurrent hash tables with interfaces similar to `boost::concurrent_flat_set/map`.
-	Strings:
	-	[seq::tiny_string](docs/tiny_string.md): relocatable string-like class with configurable Small String Optimization and tiny memory footprint. Makes most string containers faster.


Content
-------

The library is divided in 7 small modules:
-	[bits](docs/bits.md): low-level bits manipulation utilities
-	[hash](docs/hash.md): tiny hashing framework
-	[charconv](docs/charconv.md): fast arithmetic to/from string conversion
-	[format](docs/format.md): fast and type safe formatting tools
-	[containers](docs/containers.md): main module, collection of original containers: double ended vector, tiered-vector, ordered hash map, flat map based on tiered-vector, compressed vector...
-	[any](docs/any.md): type-erasing polymorphic object wrapper used to build heterogeneous containers, including hash tables and sorted containers.
-	[algorithm](docs/algorithm.md): a (small) collection of algorithm include the `net_sort` stable sorting algorithm.

A cmake project is provided for installation and compilation of tests/benchmarks.

*seq* library was tested with gcc 10.1.0 and 13.2.0 (Windows and Linux), msvc 19.43 (Windows), ClangCL 12.0.0 (Windows).

Design
------

*seq* library is a small collection of self dependant components. There is no restriction on internal dependencies, and a seq component can use any number of other components. For instance, almost all modules rely on the [bits](docs/bits.md) one.

All classes and functions are defined in the `seq` namespace, and names are lower case with underscore separators, much like the STL.
Macro names are upper case and start with the `SEQ_` prefix.

The directory structure is flat and use the "stuttering" scheme `seq/seq` used by many other libraries like boost.
Including a file has the following syntax: `#include <seq/tiered_vector.hpp>`

The `seq/tests` subdirectory includes tests for all components, usually named `test_modulename.cpp`, and rely on CTest (shipped with CMake). The tests try to cover as much features as possible, *but bugs might still be present*. Do not hesitate to contact me if you discover something unusual.
The `seq/benchs` subdirectory includes benchmarks for some components, usually named `bench_modulename.cpp`, and rely on CTest. The benchmarks are performed against other libraries that are provided in the 'benchs' folder.

Build
-----

The *seq* library is header-only, but a cmake file is provided for installation and build of tests and benchmarks. 

Currently, the following options are provided:

-	SEQ_BUILD_TESTS(OFF): build all tests
-	SEQ_BUILD_BENCHS(OFF): build all benchmarks


Acknowledgements
----------------

*seq* library uses a simplified version of the [komihash](https://github.com/avaneev/komihash) hash function for its hashing framework.

The `net_sort` stable sorting algorithm uses several ideas originaly coming (I think) from the [quadsort](https://github.com/scandum/quadsort) algorithm from scandum (bidirectional merge and ping-pong merge). 

Benchmarks (in `seq/benchs`) compare the performances of the *seq* library with other great libraries that I use in personnal or professional projects:
-	<a href="https://plflib.org/">plf</a>: used for the plf::colony container,
-	<a href="https://github.com/greg7mdp/gtl">gtl</a>: used for its gtl::btree_set and gtl::parallel_flat_hash_map,
-	<a href="https://www.boost.org/">boost</a>: used for boost::flat_set, boost::unordered_flat_set and boost::concurrent_flat_map,
-	<a href="https://github.com/martinus/unordered_dense">unordered_dense</a>: used for ankerl::unordered_dense::set,
-	<a href="https://github.com/oneapi-src/oneTBB">TBB</a>: used for tbb::concurrent_unordered_map and tbb::concurrent_hash_map.

Some of these libraries are included in the `seq/benchs` folder.


seq:: library and this page Copyright (c) 2025, Victor Moncada
