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

#ifndef SEQ_SEQ_HPP
#define SEQ_SEQ_HPP


/** @file */

// include everything except cvector
#include "format.hpp"
#include "flat_map.hpp"
#include "hash.hpp"
#include "memory.hpp"
#include "ordered_map.hpp"
#include "sequence.hpp"
#include "tagged_pointer.hpp"
#include "tiered_vector.hpp"
#include "tiny_string.hpp"
#include "radix_map.hpp"
#include "radix_hash_map.hpp"

/** \mainpage Seq library: original STL-like containers and container related tools


Purpose
-------

The *seq* library is a collection of original C++11 STL-like containers and related tools.

*seq* library does not try to reimplement already existing container classes present in other libraries like <a href="https://github.com/facebook/folly">folly</a>, <a href="https://abseil.io/">abseil</a>, <a href="https://www.boost.org/">boost</a> and (of course) std. Instead, it provides new features (or a combination of features) that are usually not present in other libraries. Some low level API like bits manipulation or hashing functions are not new, but must be defined to keep the seq library self dependent.

Among other things (see modules below), the *seq* library defines several container classes as alternatives to STL containers or providing features not present in the STL.
These containers generally adhere to the properties of STL containers (in C++17 version), though there are often some associated API differences and/or implementation details which differ from the standard library.

The *seq* containers are not necessarly drop-in replacement for their STL counterparts as they usually provide different iterator/reference statibility rules or different exception guarantees.

Currently, the *containers* module provide 5 types of containers:
-	Sequential random-access containers: 
	-	seq::devector: double ended vector that can be optimized for front operations, back operations or both. Similar interface to `std::deque`.
	-	seq::tiered_vector: tiered vector implementation optimized for fast insertion and deletion in the middle. Similar interface to `std::deque`.
	-	seq::cvector: vector-like class storing its values in a compressed way to reduce program memory footprint. Similar interface to `std::vector`.
-	Sequential stable non random-access container: `seq::sequence`, fast stable list-like container.
-	Sorted containers: 
	-	seq::flat_set: flat set container similar to boost::flat_set but based on seq::tiered_vector and providing fast insertion/deletion of single elements.
	-	seq::flat_map: associative version of `seq::flat_set`.
	-	seq::flat_multiset: similar to `seq::flat_set` but supporting duplicate keys.
	-	seq::flat_multimap: similar to `seq::flat_map` but supporting duplicate keys.
	-	seq::radix_set: radix based sorted container with a similar interface to std::set. Provides very fast lookup.
	-	seq::radix_map: associative version of `seq::radix_set`.
-	Hash tables: 
	-	seq::ordered_set: Ordered robin-hood hash table with backward shift deletion. Drop-in replacement for `std::unordered_set` (except for the bucket interface) with iterator/reference stability, and additional features (see class documentation).
	-	seq::ordered_map: associative version of `seq::ordered_set`.
	-	seq::radix_hash_set: radix based hash table with a similar interface to `std::unordered_set`. Uses incremental rehash, no memory peak.
	-	seq::radix_hash_map: associative version of seq::radix_hash_set.
-	Strings:
	-	seq::tiny_string: string-like class with configurable Small String Optimization and tiny memory footprint. Makes most string containers faster.


Content
-------

The library is divided in 7 small modules:
-	\ref bits: low-level bits manipulation utilities
-	\ref hash: implementation of fnv1a and murmurhash2 algorithms
-	\ref memory: allocators and memory pools mainly used for containers
-	\ref charconv: fast arithmetic to/from string conversion
-	\ref format: fast and type safe formatting tools
-	\ref containers: main module, collection of original containers: double ended vector, tiered-vector, ordered hash map, flat map based on tiered-vector, compressed vector...
-	\ref any: type-erasing polymorphic object wrapper used to build heterogeneous containers, including hash tables and sorted containers.

A cmake project is provided for installation and compilation of tests/benchmarks.

Why C++11 ?
-----------

For now the *seq* library is developped and maintained in order to remain compatible with C++11 only compilers.
While C++14, C++17 and even C++20 are now widely supported by the main compilers (namely msvc, gcc and clang), I often have to work on constrained and old environments (mostly on Linux) where the compiler cannot be upgraded. At least they (almost) all support C++11.

For instance, the \ref charconv and \ref format modules were developped because C++11 only compilers do not provide similar functionalities. They still provide their own specifities for more recent compilers.

*seq* library was tested with gcc/10.1.0 (Windows, mingw), gcc/8.4.0 (Linux), gcc/4.8.5 (!) (Linux), msvc/14.20, msvc/14.0 (Windows), ClangCL/12.0.0 (Windows).

Design
------

*seq* library is a small collection of self dependant components. There is no restriction on internal dependencies, and a seq component can use any number of other components. For instance, almost all modules rely on the [bits](docs/bits.md) one.

All classes and functions are defined in the `seq` namespace, and names are lower case with underscore separators, much like the STL.
Macro names are upper case and start with the `SEQ_` prefix.

The directory structure is flat and use the "stuttering" scheme `seq/seq` used by many other libraries like boost.
Including a file has the following syntax: `#include <seq/tiered_vector.hpp>`

The `seq/seq/tests` subdirectory includes tests for all components, usually named `test_modulename.cpp`, and rely on CTest (shipped with CMake). The tests try to cover as much features as possible, but bugs might still be present. Do not hesitate to contact me if you discover something unusual.
The `seq/seq/benchs` subdirectory includes benchmarks for some components, usually named `bench_modulename.cpp`, and rely on CTest. The benchmarks are performed against other libraries that are provided in the 'benchs' folder.
The `seq/seq/docs` directory contains documentation using markdown format, and the `seq/seq/doc` directory contains the html documentation generated with doxygen (available <a href="https://raw.githack.com/Thermadiag/seq/master/doc/html/index.html">here</a>).

Build
-----

The *seq* library requires compilation using cmake, but you can still use it without compilation by defining `SEQ_HEADER_ONLY`. 
Even in header-only mode, you should use the cmake file for installation.
Tests can be built using cmake from the `tests` folder, and benchmarks from the `benchs` folder.

Acknowledgements
----------------

The only library dependency is <a href="https://github.com/orlp/pdqsort">pdqsort</a> from Orson Peters. The header `pdqsort.hpp` is included within the *seq* library.
*seq* library also uses a modified version <a href="https://github.com/lz4/lz4">LZ4</a> that could be used with `cvector` class.
Benchmarks (in `seq/seq/benchs`) compare the performances of the *seq* library with other great libraries that I use in personnal or professional projects:
-	<a href="https://plflib.org/">plf</a>: used for the plf::colony container,
-	<a href="https://github.com/greg7mdp/parallel-hashmap">phmap</a>: used for its phmap::btree_set and phmap::node_hash_set,
-	<a href="https://www.boost.org/">boost</a>: used for boost::flat_set and boost::unordered_set,
-	<a href="https://github.com/martinus/robin-hood-hashing">robin-hood</a>: used for robin_hood::unordered_node_set,
-	<a href="https://github.com/skarupke/flat_hash_map">ska</a>: used for ska::unordered_set.

These libraries are included in the `seq/seq/benchs` folder (only a subset of boost is provided).

*/

#endif
