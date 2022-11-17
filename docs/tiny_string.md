# Tiny string

`seq::tiny_string` is a string class similar to std::string but aiming at greater performances when used in containers.
It provides a customizable Small String Optimization (SSO) much like boost::small_vector, where the maximum static size before an allocation is triggered is defined at compile time.

`seq::tiny_string` contains some preallocated elements in-place, which can avoid the use of dynamic storage allocation when the actual number of elements is below that preallocated threshold (*MaxStaticSize* parameter).

`seq::tiny_string` only supports characters of type char, not wchar_t.


## Motivation

Why another string class? I started writing tiny_string to provide a small and relocatable string class that can be used within [seq::cvector](cvector.md).
Indeed, gcc std::string implementation is not relocatable as it stores a pointer to its internal data for small strings. In addition, most std::string implementations have a size of 32 bytes (at least msvc and gcc ones), which was a lot for my compressed container. Therefore, I started working on a string implementation with the following specificities:
-	Relocatable,
-	Small footprint (16 bytes on 64 bits machine if possible),
-	Customizable Small String Optimization (SSO),
-	Higly optimized for small strings (fast copy/move assignment and fast comparison operators).

It turns out that such string implementation makes all flat containers (like `std::vector` or `std::deque`) faster (at least for small strings) as it greatly reduces cache misses.
The Customizable Small String Optimization is also very convenient to avoid unnecessary memory allocations for different workloads.


## Size and bookkeeping

By default, a tiny_string contains enough room to store a 15 bytes string, therefore a length of 14 bytes for null terminated strings.
For small strings (below the preallocated threshold), tiny_string only store one additional byte for bookkeeping: 7 bits for string length and 1 bit to tell if the string is allocated in-place or on the heap. It means that the default tiny_string size is 16 bytes, which is half the size of std::string on gcc and msvc. This small footprint is what makes tiny_string very fast on flat containers like std::vector, std::deque or open addressing hash tables, while node based container (like std::map) are less impacted. Note that this tiny size is only reach when using an empty allocator like std::allocator<char>. 

When the tiny_string grows beyong the preallocated threshold, memory is allocated on the heap based on provided allocator, and the bookkeeping part is divided as follow:
-	still 1 bit to tell is the memory is heap allocated or not,
-	1 bit to tell if the string capacity is exactly equal to the string size + 1 (as tiny_string is always null terminated),
-	sizeof(size_t)*8 - 2 bits to store the string size. Therefore, the maximum size of tiny_string is slightly lower than std::string.
-	a pointer (4 to 8 bytes) to the actual memory chunk.

`seq::tiny_string` does not store the memory capacity, and always use a grow factor of 2. The capacity is always deduced from the string length using compiler intrinsics (like `_BitScanReverse` on msvc). In some cases (like copy construction), the allocated capacity is the same as the string length, in which case a 1 bit flag is set to track this information.

The global typedef `seq::tstring` is provided for convenience, and is equivalent to `seq::tiny_string<0,std::allocator<char>>`.

## Static size

The maximum preallocated space is specified as a template parameter (*MaxStaticSize*).
By default, this value is set to 0, meaning that the tiny_string only uses 2 word of either 32 or 64 bits depending on the architecture.
Therefore, the maximum in-place capacity is either 7 or 15 bytes.

The maximum preallocated space can be increased up to 126 bytes. To have a tiny_string of 32 bytes like std::string on gcc and msvc, you could use, for instance, tiny_string<28>.
In this case, the maximum string size (excluding null-terminated character) to use SSO would be 30 bytes (!).

## Relocatable type

`seq::tiny_string` is relocatable, meaning that it does not store pointer to internal data.
Relocatable types can be used more efficiently in containers that are aware of this property. For instance, `seq::devector`, `seq::tiered_vector` and `seq::flat_map` are faster when working with relocatable types, as the process to move one object from a memory layout about to be destroyed to a new one can be accomplished through a simple memcpy.

Msvc implementation of std::string is also relocatable, while gcc implementation is not as it stores a pointer to its internal data for small strings.

Within the *seq* library, a relocatable type must statify `seq::is_relocatable<type>::value == true`.

Note that tiny_string is only relocatable if the allocator itself is relocatable (which is the case for the default std::allocator<char>).

## Interface

`seq::tiny_string` provides the same interface as std::string.
Functions working on other strings like find(), compare()... are overloaded to also work on std::string.
The comparison operators are also overloaded to work with std::string.

`seq::tiny_string` provides the following additional members for convenience:
-	join(): merge several strings with a common separator
-	split(): split string based on separator
-	replace(): replace a string by another one
-	convert(): convert the string to another type using streams
-	sprintf(): format string

`seq::tiny_string` also works with std::istream/std::ostream exactly like std::string.
A specialization of std::hash is provided for tiny_string types which relies on murmurhash2. This specialization is transparent and supports hashing std::string, tiny_string and const char*.

`seq::tiny_string` provides the same invalidation rules as std::string as well as the same exception guarantees.

The main difference compared to std::string is memory deallocation. As tiny_string does not store the capacity internally, its capacity is deduced from the size and must be the closest greater or equal power of 2 (except for a few situations where the capacity is excatly the length +1).
Therefore, tiny_string must release memory when its size decreases due, for instance, to calls to tiny_string::pop_back().
Likewise, shrink_to_fit() and reserve() are no-ops.


## String view

`seq::tiny_string` is specialized for seq::view_allocator in order to provide a `std::string_view` like class.
It is provided for compilers that do not support (yet) std::string_view, and provides a similar interface.

The global typedef `seq::tstring_view` is provided  for convenience, and is equivalent to `seq::tiny_string<0,seq::view_allocator>`.
tstring_view is also hashable and can be compared to other tiny_string types as well as std::string.


## Performances

All tiny_string members have been optimized to match or outperform (for small strings) most std::string implementations. They have been benchmarked against gcc (10.0.1) and msvc (14.20) for members compare(), find(), rfind(), find_first_of(), find_last_of(), find_first_not_of() and find_last_not_of(). 
Comparison operators <=> are usually faster than std::string. However, tiny_string is usually slower for back insertion with push_back(). The pop_back() member is also slower than msvc and gcc implementations.

tiny_string is usually faster when used inside flat containers simply because its size is smaller than std::string (32 bytes on gcc and msvc).
The following table shows the performances of tiny_string against std::string for sorting a vector of 1M random short string (size = 14, where both tiny_string and std::string use SSO) and 1M random longer strings (size = 200, both use heap allocation). Tested with gcc 10.1.0 (-O3) for msys2 on Windows 10, using Intel(R) Core(TM) i7-10850H at 2.70GHz.

String class       | sort small (std::less) | sort small (tstring::less) | sort wide (std::less) | sort wide (tstring::less) |
-------------------|------------------------|----------------------------|-----------------------|---------------------------|
std::string        |          160 ms        |          122 ms            |       382 ms          |         311 ms            |
seq::tiny_string   |          112 ms        |          112 ms            |       306 ms          |         306 ms            |

This benchmark is available in file 'seq/benchs/bench_tiny_string.hpp'.
Note that tiny_string always uses its own comparison function. We can see that the comparison function of tiny_string is faster than the default one used
by std::string. Even when using tiny_string comparator, std::string remains slightly slower due to the tinier memory footprint of tiny_string.

The difference is wider on msvc (14.20):

String class       | sort small (std::less) | sort small (tstring::less) | sort wide (std::less) | sort wide (tstring::less) |
-------------------|------------------------|----------------------------|-----------------------|---------------------------|
std::string        |          281 ms        |          264 ms            |       454 ms          |         441 ms            |
seq::tiny_string   |          176 ms        |          176 ms            |       390 ms          |         390 ms            |


