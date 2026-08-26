### Sequence container

The `seq::sequence` container behaves like a hybrid version of std::deque and std::hive. It provides:
-	Constant time insertion at the back or the front using members push_back(), emplace_back(), push_front() and emplace_front(),
-	Constant time removal of one or more elements with erase(),
-	Stability of references and iterators.

Unlike std::list, the sequence container does not provide insertion anywhere in the container.
Instead, `seq::sequence` provides unordered insertion through its member insert(), much like std::hive class.
Unordered insertion is usually prefered to back or front insertion as it can reuse free slots created by erase() and avoid potential allocations.
In addition, its sort() and shrink_to_fit() members do NOT preserve reference and iterator stability.

The memory overhead of a seq::sequence around 1 byte per element.

The sequence container is a perfect candidate for std::queue and std::stack.
It is used by the `seq` library as the backend container for `seq::ordered_set` and `seq::ordered_map`.

## (Small) technical description

Sequence container is implemented as a linked list of buckets. Each bucket holds (up to) 64 elements in a contiguous storage, and a 64 bits integer telling if a slot is empty or occupied.

In order to retrieve the index of the first (or last) used slot in a bucket, or to get the number of occupied slots, the sequence container uses OS intrinsics to scan the 64 bits integer.
Removing an element from the sequence will set the corresponding bit to 0, inserting will set the bit to 1.

In addition, the sequence maintains another linked list of partially free buckets in order to perform fast
unordered insertion using insert() member and therefore reuse slots previously deleted by erase().
