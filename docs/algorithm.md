# Algorithm
`algorithm` is a small module providing a few iterator based algorithms:

-	`seq::merge`: similar to `std::merge` but providing a better handling of consecutive ordered values, and has a special case for unbalanced merging (one range is way smaller than the other)

-	`seq::inplace_merge`: similar to `std::inplace_merge` but relying on `seq::merge` and using a user-provided buffer. `seq::inplace_merge` never allocates memory unless one of the following constant buffer is provided:
	-	 `seq::default_buffer`: uses as much memory as `std::inplace_merge`
	-	 `seq::medium_buffer`: uses 8 times less memory than with `seq::default_buffer`
	-	 `small_buffer` : uses 32 times less memory than with `seq::default_buffer`
	-	 `tiny_buffer`: uses 128 times less memory than with `seq::default_buffer`

	Note that a buffer of size 0 is supported, the algorithm will just be way slower. The inplace merging is based on the following [article](https://www.jmeiners.com/efficient-programming-with-components/15_merge_inplace.html) and was first published in 1981.

-	`seq::reverse_descending`: reverse a range sorted in descending order in a stable way: consecutive equal values are not reversed.

-	`seq::unique`: removed duplicates from a range in a stable way. This is very similar to `std::unique` except that the range does not need to be sorted. It uses a hash table under the hood to find duplicate values. A custom hash function and comparison function can be passed for custom types.

-	`seq::net_sort` and `seq::net_sort_size`: "new" generic stable sorting algorithm that is used everywhere within the seq library.  `seq::net_sort` is a merge sort algorithm with the following specificities:
	-	Bottom-up merging instead of the more traditional top-down approach,
	-	Small blocks of 8 elements are sorted using a sorting network,
	-	Bidirectional merging is used for relocatable types,
	-	Ping-pong merge is used to merge 4 sorted ranges,
	-	Can work without allocating memory through a (potentially null) user provided buffer,
	-	Also works on bidirectional iterators.

	If provided buffer is one of `seq::default_buffer`, `seq::medium_buffer`, `seq::small_buffer` or `seq::tiny_buffer`, this function will try to allocate memory.

	From my tests on multiple input types, net_sort() is always faster than std::stable_sort().

	net_sort_size() and net_sort() work on bidirectional iterators. Using net_sort_size() instead of net_sort() is faster when the range size is already known.

	Full credits to scandum (https://github.com/scandum) for its quadsort algorithm from which I took several ideas (bidirectional merge and ping-pong merge).  