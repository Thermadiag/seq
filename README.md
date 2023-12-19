[![CTest](https://github.com/Thermadiag/seq/actions/workflows/cmake.yml/badge.svg?branch=master)](https://github.com/Thermadiag/seq/actions/workflows/cmake.yml)


Purpose
-------

The *seq* library is a collection of original C++14 STL-like containers and related tools.

*seq* library does not try to reimplement already existing container classes present in other libraries like <a href="https://github.com/facebook/folly">folly</a>, <a href="https://abseil.io/">abseil</a>, <a href="https://www.boost.org/">boost</a> and (of course) std. Instead, it provides new features (or a combination of features) that are usually not present in other libraries. Some low level API like bits manipulation or hashing functions are not new, but must be defined to keep the seq library self dependent.

Among other things (see modules below), the *seq* library defines several container classes as alternatives to STL containers or providing features not present in the STL.
These containers generally adhere to the properties of STL containers (in C++17 version), though there are often some associated API differences and/or implementation details which differ from the standard library.

The *seq* containers are not necessarly drop-in replacement for their STL counterparts as they usually provide different iterator/reference statibility rules or different exception guarantees.

Currently, the *containers* module provide 5 types of containers:
-	Sequential random-access containers: 
	-	[seq::devector](docs/devector.md): double ended vector that can be optimized for front operations, back operations or both. Similar interface to `std::deque`.
	-	[seq::tiered_vector](docs/tiered_vector.md): tiered vector implementation optimized for fast insertion and deletion in the middle. Similar interface to `std::deque`.
	-	[seq::cvector](docs/cvector.md): vector-like class storing its values in a compressed way to reduce program memory footprint. Similar interface to `std::vector`.
-	Sequential stable non random-access container: `seq::sequence`, fast stable list-like container.
-	Sorted containers: 
	-	[seq::flat_set](docs/flat_set.md) : flat set container similar to boost::flat_set but based on seq::tiered_vector and providing fast insertion/deletion of single elements.
	-	`seq::flat_map`: associative version of `seq::flat_set`.
	-	`seq::flat_multiset`: similar to `seq::flat_set` but supporting duplicate keys.
	-	`seq::flat_multimap`: similar to `seq::flat_map` but supporting duplicate keys.
	-	[seq::radix_set](docs/radix_tree.md) : radix based sorted container with a similar interface to std::set. Provides very fast lookup.
	-	`seq::radix_map`: associative version of `seq::radix_set`.
-	Hash tables: 
	-	[seq::ordered_set](docs/ordered_set.md): Ordered robin-hood hash table with backward shift deletion. Drop-in replacement for `std::unordered_set` (except for the bucket interface) with iterator/reference stability, and additional features (see class documentation).
	-	`seq::ordered_map`: associative version of `seq::ordered_set`.
	-	[seq::radix_hash_set](docs/radix_tree.md): radix based hash table with a similar interface to `std::unordered_set`. Uses incremental rehash, no memory peak.
	-	`seq::radix_hash_map`: associative version of `seq::radix_hash_set`.
	-	[seq::concurrent_map](docs/concurrent_map.md) and `seq::concurrent_set`: higly scalable concurrent hash tables.
-	Strings:
	-	[seq::tiny_string](docs/tiny_string.md): string-like class with configurable Small String Optimization and tiny memory footprint. Makes most string containers faster.


Content
-------

The library is divided in 7 small modules:
-	[bits](docs/bits.md): low-level bits manipulation utilities
-	[hash](docs/hash.md): tiny hashing framework
-	[memory](docs/memory.md): allocators and memory pools mainly used for containers
-	[charconv](docs/charconv.md): fast arithmetic to/from string conversion
-	[format](docs/format.md): fast and type safe formatting tools
-	[containers](docs/containers.md): main module, collection of original containers: double ended vector, tiered-vector, ordered hash map, flat map based on tiered-vector, compressed vector...
-	[any](docs/any.md): type-erasing polymorphic object wrapper used to build heterogeneous containers, including hash tables and sorted containers.

A cmake project is provided for installation and compilation of tests/benchmarks.

Why C++14 ?
-----------

For now the *seq* library is developped and maintained in order to remain compatible with C++14 only compilers.
While C++17 and C++20 are now widely supported by the main compilers (namely msvc, gcc and clang), I often have to work on constrained and old environments (mostly on Linux) where the compiler cannot be upgraded. At least they (almost) all support C++14.

For instance, the [charconv](docs/charconv.md) and [format](docs/format.md) modules were developped because C++11 only compilers do not provide similar functionalities. They still provide their own specifities for more recent compilers.

*seq* library was tested with gcc/10.1.0, gcc/13.2.0 (Windows, mingw), gcc/8.4.0 (Linux), gcc/6.4.0 (Linux), msvc/19.29 (Windows), ClangCL/12.0.0 (Windows).

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

The *seq* library requires compilation using cmake, but you can still use it without compilation by using `-DHEADER_ONLY=ON`. 
Even in header-only mode, you should use the cmake file for installation.

Currently, the following options are provided:

-	HEADER_ONLY(OFF): header only version of the *seq* library
-	ENABLE_AVX2(ON): enable AVX2 support, usefull (but not mandatory) for `seq::radix_(map/set/hash_map/hash_set)` as well as `seq::cvector`
-	BUILD_TESTS(OFF): build all tests
-	BUILD_BENCHS(OFF): build all benchmarks
-	TEST_CVECTOR(ON): if building tests, add the `seq::cvector` class tests

Note that for msvc build, AVX2 support is enabled by default. You should call cmake with `-DENABLE_AVX2=OFF` to disable it.

Using Seq library with CMake
----------------------------

The follwing example shows how to use the *seq* library within a CMake project:

```cmake

cmake_minimum_required(VERSION 3.8)

# Dummy test project
project(test)

# Add sources
add_executable(test test.cpp)

# Find package seq
find_package(seq REQUIRED)
	
# SEQ_FOUND is set to TRUE if found
if(${SEQ_FOUND})
	message(STATUS "Seq library found, version ${SEQ_VERSION}")
endif()

# Add include directory
target_include_directories(test PRIVATE ${SEQ_INCLUDE_DIR})

# Link with seq library if not header only
if(DEFINED SEQ_LIBRARY)
	# Add lib directory
	target_link_directories(test PRIVATE ${SEQ_LIB_DIR})
	# Add lib
	target_link_libraries(test ${SEQ_LIBRARY} )
endif()

```

Acknowledgements
----------------

The only library dependency is <a href="https://github.com/orlp/pdqsort">pdqsort</a> from Orson Peters. The header `pdqsort.hpp` is included within the *seq* library.
*seq* library also uses a modified version <a href="https://github.com/lz4/lz4">LZ4</a> that could be used with `cvector` class.
Finaly, *seq* library uses a simplified version of the [komihash](https://github.com/avaneev/komihash) hash function for its hashing framework.

Benchmarks (in `seq/benchs`) compare the performances of the *seq* library with other great libraries that I use in personnal or professional projects:
-	<a href="https://plflib.org/">plf</a>: used for the plf::colony container,
-	<a href="https://github.com/greg7mdp/gtl">gtl</a>: used for its gtl::btree_set and gtl::parallel_flat_hash_map,
-	<a href="https://www.boost.org/">boost</a>: used for boost::flat_set, boost::unordered_flat_set and boost::concurrent_flat_map,
-	<a href="https://github.com/martinus/unordered_dense">unordered_dense</a>: used for ankerl::unordered_dense::set,
-	<a href="https://github.com/oneapi-src/oneTBB">TBB</a>: used for tbb::concurrent_unordered_map and tbb::concurrent_hash_map.

Some of these libraries are included in the `seq/benchs` folder.


seq:: library and this page Copyright (c) 2023, Victor Moncada
