
Purpose
-------

The Seq library is a collection of C++11 STL-like containers and related tools optimized for speed and/or memory usage.

Seq library does not try to reimplement already existing container classes present in other libraries like <a href="https://github.com/facebook/folly">folly</a>, <a href="https://abseil.io/">abseil</a>, <a href="https://www.boost.org/">boost</a> and (of course) std. Instead, it provides new features (or a combination of features) that are usually not present in other libraries. Some low level API like bits manipulation or hashing functions are not new, but must be defined to keep the seq library self dependent.

The seq library was developped based on my growing frustration when using standard containers (mainly whose of STD) on a large scale professional project. The biggest concerning points were:

-	The default hash table (std::unordered_set/map) is painfully slow for all operations on almost all implementations.
-	std::unordered_map has a huge memory overhead.
-	std::unordered_map does not invalidate iterators and references, which is great for my use cases, and partially explain the memory overhead and general slowness. However,
		iterators are still invalidated on rehash, which prevents me from using them in scenarios where the item count is not known in advance.
-	I would like to be able to sort a hash map.
-	std::vector is great, but it's a shame that it is only optimized for back insertion.
-	std::list is the only standard container that keeps insertion order and ensure iterators/reference stability, but its memory overhead for small types and low speed make it barely usable.
-	Random-access containers (std::deque and std::vector) are very slow when inserting/deleting in the middle.
-	Likewise, most flat map implementations (like the boost one) are very slow when inserting/deleting single entries.
-	Using C++ streams to format numerical values and build tables is usually slow and not very convenient.
-	...

Some of my concerns were already takled by external libraries. For instance, I use <a href="https://github.com/greg7mdp/parallel-hashmap">phmap::flat_hash_set/map</a> (based on <a href="https://github.com/abseil/abseil-cpp">absl::flat_hash_map</a>) when I need a fast hash map with low memory overhead (and when iterators/references stability is not a concern). I started working on the seq library for the other points.


Content
-------

The library is divided in 8 modules:
-	[bits](docs/bits.md): low-level bits manipulation utilities
-	[hash](docs/hash.md): implementation of fnv1a and murmurhash2 algorithms
-	[lock](docs/lock.md): locking classes
-	[memory](docs/memory.md): allocators and memory pools mainly used for containers
-	[charconv](docs/charconv.md): fast arithmetic to/from string conversion
-	[format](docs/format.md): fast and type safe formatting tools
-	[containers](docs/containers.md): main module, collection of original containers: double ended vector, tiered-vector, ordered hash map, flat map based on tiered-vector...
-	[any](docs/ant.md): type-erasing polymorphic object wrapper used to build heterogeneous containers, including hash tables and sorted containers.

seq library is header-only and self-dependent. A cmake project is provided for installation and compilation of tests.


Why C++11 ?
-----------

For now the seq library is developped and maintained in order to remain compatible with C++11 only compilers.
While C++14, C++17 and even C++20 are now widely supported by the main compilers (namely msvc, gcc and clang), I often have to work on constrained and old environments (mostly on Linux) where the compiler cannot be upgraded. At least they (almost) all support C++11.


Documentation
-------------

The full library documentation (generated with *doxygen*) is available <a href="https://rawcdn.githack.com/Thermadiag/seq/731467950d3591147b62856972e0d543173dddc1/doc/html/index.html">here</a>.
