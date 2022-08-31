# Memory: collection of tools for memory management

The *memory* module provides several classes to help for containers memory management.

## Allocators

The *memory* module provides several types of STL conforming allocators:
-	*seq::aligned_allocator*: allocator returning aligned memory
-	*seq::external_allocator*: allocator using external allocation/deallocation functions
-	*seq::object_allocator*: allocate memory using an object pool (see next section)

See classes documentation for more details.

## Memory pools

The *memory* module provides 2 types of memory pool classes:
-	*seq::object_pool*: standard memory pool for fast allocation of 1 or more objects of the same type, as well as *std::unique_ptr*. Not thread safe.
-	*seq::parallel_object_pool*: memory pool for fast allocation of 1 or more objects in a multi-threaded context. Uses an almost lock-free approach.

See classes documentation for more details.


## Miscellaneous

The class *seq::tagged_pointer* stores a pointer and tag value on 4-8 bytes (depending on the architecture).