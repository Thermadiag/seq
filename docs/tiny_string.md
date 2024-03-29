# Tiny string

`seq::tiny_string` is a string class similar to std::string but aiming at greater performances when used in containers.
It provides a customizable Small String Optimization (SSO) much like boost::small_vector, where the maximum static size before an allocation is triggered is defined at compile time.

`seq::tiny_string` contains some preallocated elements in-place, which can avoid the use of dynamic storage allocation when the actual number of elements is below that preallocated threshold (*MaxStaticSize* parameter).

`seq::tiny_string` supports any type of character in a similar way as `std::basic_string`.


## Motivation

Why another string class? I started writing tiny_string to provide a small and relocatable string class that can be used within [seq::cvector](cvector.md).
Indeed, gcc std::string implementation is not relocatable as it stores a pointer to its internal data for small strings. In addition, most std::string implementations have a size of 32 bytes (at least msvc and gcc ones), which is unnecessary when considering a compressed container. Therefore, I started working on a string implementation with the following specificities:
-	Relocatable,
-	Small footprint (16 bytes on 64 bits machine if possible),
-	Customizable Small String Optimization (SSO),
-	Higly optimized for small strings (fast copy/move assignment and fast comparison operators).

It turns out that such string implementation makes all flat containers (like `std::vector` or `std::deque`) faster (at least for small strings) as it greatly reduces cache misses, memory allocations, and allows optimization tricks for relocatable types.
The Customizable Small String Optimization is also very convenient to avoid unnecessary memory allocations for different workloads.

For the rest of the documentation, only standard `char` strings are considered.

## Size and bookkeeping

By default on 64 bits machines, a tiny_string contains enough room to store a 16 bytes string, therefore a length of 15 bytes for null terminated strings.
Starting version 1.2 of *seq* library, tiny_string does not store any additional bytes for internal bookkeeping. The fact that a string MUST be null terminated allows some tricks in order to use the full string size for SSO.

This means that the default `seq::tstring` has a size of 16 bytes and can hold a small string of... 16 bytes (including trailing 0).
Likewise, `seq::tiny_string<char, std::char_traits<char>, std::allocator<char>, 31>` has a size of 32 bytes and can hold a small string of 32 bytes.

When the tiny_string grows beyong the preallocated threshold, memory is allocated on the heap based on provided allocator using a grow factor of 2.

Starting version 1.2 of *seq* library, tiny_string behaves exactly like std::string and does not deallocate memory on shrinking, except on calls to `shrink_to_fit()`.

Several typedefs are provided for convenience:

-	`seq::tstring`: equivalent to `seq::tiny_string<char, std::char_traits<char>, std::allocator<char>, 0 >`
-	`seq::wtstring`: equivalent to `seq::tiny_string<wchar_t, std::char_traits<wchar_t>, std::allocator<wchar_t>, 0 >`


## Static size

The maximum preallocated space is specified as a template parameter (*MaxStaticSize*).
By default, this value is set to 0, meaning that the tiny_string only uses 2 words of either 32 or 64 bits depending on the architecture (whatever the char type is).
Therefore, the maximum in-place capacity is either 8 or 16 bytes for 1 byte char type, less for wchar_t string.

The maximum preallocated space can be increased up to 120 elements (size of 119 since the string is null terminated) for char type. 
Use `seq::tiny_string<char_type>::max_allowed_static_size` to retrieve the maximum static size for a given char type.

## Relocatable type

`seq::tiny_string` is relocatable, meaning that it does not store pointer to internal data.
Relocatable types can be used more efficiently in containers that are aware of this property. For instance, `seq::devector`, `seq::tiered_vector`, `seq::flat_map/set`, `seq::radix_map/set` and `seq::radix_hash_map/set` are faster when working with relocatable types, as the process to move one object from a memory layout about to be destroyed to a new one can be accomplished through a simple memcpy.

Msvc implementation of std::string is also relocatable, while gcc implementation is not as it stores a pointer to its internal data for small strings.

Within the *seq* library, a relocatable type must statify `seq::is_relocatable<type>::value == true`.

Note that tiny_string is only relocatable if the allocator itself is relocatable (which is the case for the default std::allocator<char>).

The tables below gives performances results when inserting and successfully finding 500k random small strings (size = 13) in a `seq::flat_set`, `phmap::btree_set` and `std::set`. The first table is for std::string, the second for seq::tstring.
The benchmark is available in `seq/benchs/bench_tiny_string.cpp` and was compiled with gcc 10.1.0 (-O3) for msys2 on Windows 10, using Intel(R) Core(TM) i7-10850H at 2.70GHz.

std::string         |    seq::flat_set   |  phmap::btree_set  |     std::set       |
--------------------|--------------------|--------------------|--------------------|
insert              |        783 ms      |        250 ms      |        250 ms      |
find                |        141 ms      |        234 ms      |        235 ms      |


seq::tstring        |    seq::flat_set   |  phmap::btree_set  |     std::set       |
--------------------|--------------------|--------------------|--------------------|
insert              |        265 ms      |        109 ms      |        219 ms      |
find                |         94 ms      |         94 ms      |        203 ms      |


`seq::flat_set` internally uses a `seq::tiered_vector` which is very sensible to the value type size for inserting/erasing. Furtheremore, it is aware of the relocatable property of its value type, which is why there is a factor 3 speed-up when inserting tstring instead of std::string. Even its lookup performance benefits from the small size of tstring as it reduces cache misses for the last stages of the find operation.

A pure stable node based container like `std::set` does not benefit greatly from using tstring as it only reduces the size of allocated nodes. Observed speedup for find operation is probably due to the comparison operator for tstring which is slightly faster than the std::string one for small strings.

`phmap::btree_set` is also a node based container, but each node can contain several elements. Note that btree_set uses a fix sized node in bytes, and its arity is computed from this size. A similar strategy is used for std::deque in most implementation to get its bucket size. Using tstring instead of std::string doubles the btree arity, increasing its overall performances for both insertion and lookup.
	
Within the `seq` library, the following containers provide optimizations for relocatable types:
-	random access containers: `seq::tiered_vector`, `seq::devector`, `seq::cvector`
-	sorted containers: `seq::flat_set`, `seq::flat_map`, `seq::flat_multiset`, `seq::flat_mutlimap`, `seq::radix_set`, `seq::radix_map`
-	hash tables: `seq::radix_hash_set`, `seq::radix_hash_map`


## Interface

`seq::tiny_string` provides the same interface as std::string.
Functions working on other strings like find(), compare()... are overloaded to also work on std::string.
The comparison operators are also overloaded to work with std::string.

`seq::tiny_string` also works with std::istream/std::ostream exactly like std::string.
A specialization of std::hash is provided for tiny_string types which relies on [komihash](https://github.com/avaneev/komihash). This specialization is transparent and supports hashing std::string, tiny_string and const char*.

`seq::tiny_string` provides the same invalidation rules as std::string as well as the same exception guarantees.

`seq::tiny_string` is hashable using `std::hash` or [`seq::hasher`](hash.md).

## String view

`seq::tiny_string` is specialized for seq::view_allocator in order to provide a `std::string_view` like class.
It is provided for compilers that do not support (yet) std::string_view, and provides a similar interface.

The global typedef `seq::tstring_view` is provided  for convenience, and is equivalent to `seq::tiny_string<char,std::char_traits<char>,seq::view_allocator<char>,0>`.
tstring_view is also hashable and can be compared to other tiny_string types as well as std::string.

