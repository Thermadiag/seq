# Double ended vector

`seq::devector` is a double-ending vector class that mixes the behavior and performances of `std::deque` and `std::vector`.
Elements are stored in a contiguous memory chunk exatcly like a vector, but might contain free space at the front in addition to free space at the back in order to provide O(1) insertion at the front.

`seq::devector` provides a similar interface as std::vector with the following additional members:
-	`push_front()` and `emplace_front()`: insert an element at the front of the devector
-	`resize_front()`: resize the devector from the front instead of the back of the container
-	`back_capacity()`: returns the capacity (free slots) at the back of the devector
-	`front_capacity()`: returns the capacity (free slots) at the front of the devector

Almost all members provide basic exception guarantee, except if the value type has a noexcept move constructor and move assignment operator, in which case members provide strong exception guarantee.
References and iterators are invalidated by insertion/removal of elements.
`seq::devector` is used by [seq::tiered_vector](tiered_vector.md) for bucket storage.


## Performances

Internal benchmarks show that devector is as fast as `std::vector` when inserting at the back. `seq::devector` is also faster than `std::vector` for relocatable types (where `seq::is_relocatable<T>::value` is true) as memcpy and memmove can be used instead of `std::copy` or `std::move` on reallocation.
Inserting a new element in the middle of a devector is on average twice as fast as on `std::vector`, since the values can be pushed to either ends, whichever is faster.
