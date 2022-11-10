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

## Optimization flags

devector can be configured with the following flags:
-	`OptimizeForPushBack`: the devector behaves like a `std::vector`, adding free space at the back based on the growth factor `SEQ_GROW_FACTOR`.
	In this case, inserting elements at the front is as slow as for std::vector as it require to move all elements toward the back.
-	`OptimizeForPushFront`: the devector adds free space at the front based on the growth factor SEQ_GROW_FACTOR. Inserting elements at the front is amortized O(1), inserting at the back is O(N).
-	`OptimizeForBothEnds` (default): the devector has as many free space at the back and the front. Both push_back() and push_front() behave in amortized O(1).
	When the memory storage grows (by a factor of SEQ_GROW_FACTOR), the elements are moved to the middle of the storage, leaving as much space at the front and the back.
	When inserting an element at the back, several scenarios are checked (this is similar for front insertion):
	1.	Free slots are available at the back and the element is inserted there.
	2.	The devector does not have available slots at the back or the front, a new chunk of memory of size size()*SEQ_GROW_FACTOR is allocated, elements are moved to this new memory location (leaving the same capacity at the back and the front) and the new element is inserted at the back.
	3.	The devector does not have enough capacity at the back, but has free capacity at the front. In this case, there are 2 possibilities:
		1.	front capacity is greater than size() / __SEQ_DEVECTOR_SIZE_LIMIT: elements are moved toward the front, leaving the same capacity at the back and the front. The new element is then inserted at the back.
		2.	front capacity is lower or equal to size() / __SEQ_DEVECTOR_SIZE_LIMIT: a new chunk is allocated like in b). By default, __SEQ_DEVECTOR_SIZE_LIMIT is set to 16. __SEQ_DEVECTOR_SIZE_LIMIT can be adjusted to provide a different trade-off between insertion speed and memory usage.


## Performances

Internal benchmarks show that devector is as fast as `std::vector` when inserting at the back with `OptimizeForPushBack`, or inserting at the front with `OptimizeForPushFront`.
Using `OptimizeForBothEnds` makes insertion at both ends usually twice as slow as back insertion for `std::vector`.
`seq::devector` is faster than `std::vector` for relocatable types (where `seq::is_relocatable<T>::value` is true) as memcpy and memmove can be used instead of `std::copy` or `std::move` on reallocation.
Inserting a new element in the middle of a devector is on average twice as fast as on `std::vector`, since the values can be pushed to either ends, whichever is faster (at least with `OptimizeForBothEnds`).
