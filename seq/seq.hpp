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
	-	seq::concurrent_map and seq::concurrent_set: higly scalable concurrent hash tables.
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
-	\ref bits "bits": low-level bits manipulation utilities
-	\ref hash "hash": implementation of fnv1a and murmurhash2 algorithms
-	\ref memory "memory": allocators and memory pools mainly used for containers
-	\ref charconv "charconv": fast arithmetic to/from string conversion
-	\ref format "format": fast and type safe formatting tools
-	\ref containers "containers": main module, collection of original containers: double ended vector, tiered-vector, ordered hash map, flat map based on tiered-vector, compressed vector...
-	\ref any "any": type-erasing polymorphic object wrapper used to build heterogeneous containers, including hash tables and sorted containers.

A cmake project is provided for installation and compilation of tests/benchmarks.

*/


/// This is the default seq.hpp file, just in case people will use the seq folder directly without installing the library


#define SEQ_VERSION_MAJOR 0
#define SEQ_VERSION_MINOR 0
#define SEQ_VERSION "0.0"

#ifndef SEQ_BUILD_LIBRARY
	#define __SEQ_IS_HEADER_ONLY 1
#else
	#define __SEQ_IS_HEADER_ONLY 0
#endif

#if __SEQ_IS_HEADER_ONLY == 1
	#ifndef SEQ_HEADER_ONLY
		#define SEQ_HEADER_ONLY
	#endif
#endif

#endif
