
Purpose
-------

The Seq library is a collection of original C++11 STL-like containers and related tools.

Seq library does not try to reimplement already existing container classes present in other libraries like <a href="https://github.com/facebook/folly">folly</a>, <a href="https://abseil.io/">abseil</a>, <a href="https://www.boost.org/">boost</a> and (of course) std. Instead, it provides new features (or a combination of features) that are usually not present in other libraries. Some low level API like bits manipulation or hashing functions are not new, but must be defined to keep the seq library self dependent.

The seq library was developped based on my growing frustration when using standard containers (mainly whose of STD) on a large scale professional project. The biggest concerning points were:

-	The default hash table (`std::unordered_set/map`) is quite slow for all operations on almost all implementations.
-	`std::unordered_map` has a big memory overhead.
-	`std::unordered_map` does not invalidate iterators and references, which is great for my use cases, and partially explain the memory overhead and general slowness. However,
		iterators are still invalidated on rehash, which prevents me from using them in scenarios where the item count is not known in advance.
-	I would like to be able to sort a hash map in a fast way.
-	`std::vector` is great, but it's a shame that it is only optimized for back insertion.
-	`std::list` is the only standard container that keeps insertion order and ensure iterators/reference stability, but its memory overhead for small types and low speed make it hardly usable.
-	Random-access containers (`std::deque` and `std::vector`) are very slow when inserting/deleting in the middle.
-	Likewise, most flat map implementations (like the boost one) are very slow when inserting/deleting single entries.
-	Using C++ streams to format numerical values and build tables is usually slow and not very convenient.
-	...

Some of my concerns were already takled by external libraries. For instance, I use <a href="https://github.com/greg7mdp/parallel-hashmap">phmap::flat_hash_set/map</a> (based on <a href="https://github.com/abseil/abseil-cpp">absl::flat_hash_map</a>) when I need a fast hash map with low memory overhead (and when iterators/references stability is not a concern). The Seq library is an attempt to work around the other points.

Content
-------

The library is divided in 7 small modules:
-	[bits](docs/bits.md): low-level bits manipulation utilities
-	[hash](docs/hash.md): implementation of fnv1a and murmurhash2 algorithms
-	[memory](docs/memory.md): allocators and memory pools mainly used for containers
-	[charconv](docs/charconv.md): fast arithmetic to/from string conversion
-	[format](docs/format.md): fast and type safe formatting tools
-	[containers](docs/containers.md): main module, collection of original containers: double ended vector, tiered-vector, ordered hash map, flat map based on tiered-vector, compressed vector...
-	[any](docs/any.md): type-erasing polymorphic object wrapper used to build heterogeneous containers, including hash tables and sorted containers.

A cmake project is provided for installation and compilation of tests/benchmarks.

Why C++11 ?
-----------

For now the seq library is developped and maintained in order to remain compatible with C++11 only compilers.
While C++14, C++17 and even C++20 are now widely supported by the main compilers (namely msvc, gcc and clang), I often have to work on constrained and old environments (mostly on Linux) where the compiler cannot be upgraded. At least they (almost) all support C++11.

For instance, the [charconv](docs/charconv.md) and [format](docs/format.md) modules were developped because C++11 only compilers do not provide similar functionalities. They still provide their own specifities for more recent compilers.

Seq library was tested with gcc/10.1.0 (Windows, mingw), gcc/8.4.0 (Linux) and msvc/14.20 (Windows).

Design
------

Seq library is a small collection of header only and self dependant components. There is no restriction on internal dependencies, and a seq component can use any number of other components.
For instance, almost all modules rely on the [bits](docs/bits.md) one. The only exception is the `cvector` class that requires compilation, unless you define `SEQ_HEADER_ONLY`

All classes and functions are defined in the `seq` namespace, and names are lower case with underscore separators, much like the STL.
Macro names are upper case and start with the `SEQ_` prefix.

The directory structure is flat and use the "stuttering" scheme `seq/seq` used by many other libraries like boost.
Including a file has the following syntax: `#include <seq/tiered_vector.hpp>`

The `seq/seq/tests` subdirectory includes tests for all components, usually named `test_modulename.hpp`, with a unique `main.cpp`. The tests try to cover as much features as possible, but bugs might still be present. Do not hesitate to contact me if you discover something unusual.

The `seq/seq/benchs` subdirectory includes benchmarks for some components, usually named `bench_modulename.hpp`, with a unique `main.cpp`. The benchmarks are performed against other libraries that are provided in the 'benchs' folder.
The `seq/seq/docs` directory contains documentation using markdown format, and the `seq/seq/doc` directory contains the html documentation generated with doxygen (available <a href="https://raw.githack.com/Thermadiag/seq/master/doc/html/index.html">here</a>).

Build
-----

The seq library needs compilation using cmake if you intend to use the `cvector` class (compressed vector). Note that you can still use it without compilation by defining `SEQ_HEADER_ONLY`. 
Even on header-only mode, you should use the cmake file for installation.
Tests can be built using cmake from the `tests` folder, and benchmarks from the `benchs` folder.

Acknowledgements
----------------

The only library dependency is <a href="https://github.com/orlp/pdqsort">pdqsort</a> from Orson Peters. The header `pdqsort.hpp` is included within the seq library.
Seq library also uses a modified version <a href="https://github.com/lz4/lz4">LZ4</a> that could be used with `cvector` class.
Benchmarks (in `seq/seq/benchs`) compare the performances of the seq library with other great libraries:
-	<a href="https://plflib.org/">plf</a>: used for the plf::colony container,
-	<a href="https://github.com/greg7mdp/parallel-hashmap">phmap</a>: used for its phmap::btree_set and phmap::node_hash_set,
-	<a href="https://www.boost.org/">boost</a>: used for boost::flat_set and boost::unordered_set,
-	<a href="https://github.com/martinus/robin-hood-hashing">robin-hood</a>: used for robin_hood::unordered_node_set,
-	<a href="https://github.com/skarupke/flat_hash_map">ska</a>: used for ska::unordered_set.

These libraries are included in the `seq/seq/benchs` folder (only a subset of boost is provided).


seq:: library and this page Copyright (c) 2022, Victor Moncada
