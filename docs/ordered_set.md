### Ordered set

`seq::ordered_set` is a open addressing hash table using robin hood hashing and backward shift deletion. Its main properties are:
-	Keys are ordered by insertion order. Therefore, ordered_set provides the additional members push_back(), push_front(), emplace_back() and emplace_front() to control key ordering.
-	Since the container is ordered, it is also sortable. ordered_set provides the additional member sort() for this purpose.
-	The hash table itself basically stores iterators to a `seq::sequence` object storing the actual values. Therefore, `seq::ordered_set` provides *stable references and iterators, even on rehash* (unlike `std::unordered_set` that invalidates iterators on rehash).
-	`seq::ordered_set` uses robin hood probing with backward shift deletion. It does not rely on tombstones and supports high load factors.
-	It is fast and memory efficient compared to other node based hash tables (see section *Performances*), but still slower than most open addressing hash tables due to the additional indirection.


## Interface

`seq::ordered_set` provides a similar interface to `std::unordered_set` with the following differences:
-	The bucket related functions are not implemented,
-	The default load factor is set to 0.6,
-	Additional members push_back(), push_front(), emplace_back() and emplace_front() let you control the key ordering,
-	Additional member sort() let you sort the container in a stable way,
-	Its iterator and const_iterator types are bidirectional iterators.

## Direct access to sequence

`seq::ordered_set/map` provide the members `extract()` to retrieve (move) the underlying seq::sequence object.
The sequence can be modified at will and then moved to the ordered_set using `replace()` member. In this case, the sequence must already be unique.

## Exception guarantee

Most members of `seq::ordered_set` provide *strong exception guarantee*, except if specified otherwise (mentionned in function documentation).

## Growth policy and load factor

`seq::ordered_set` uses a growth factor of 2 to use the fast modulo. The hash table size is multiplied by 2 each time the table load factor exceeds the given max_load_factor(). The default maximum load factor is set to 0.6 and can by set up to 0.95, which is well supported thanks to the robin hood hashing.

## Handling of bad hash function

Like most robin hood based hash tables, `seq::ordered_set` uses 8 bits to store the node distance to its computed location. For a bad hash function leading to strong clustering, this 8 bits distance quickly overflows. The usual strategy in this case is to rehash the table based on the growth strategy, hopping for a better key distribution.

For very bad hash function (or in case of DOS attack), hash tables like <a href="https://github.com/skarupke/flat_hash_map/blob/master/flat_hash_set.hpp">ska::flat_hash_set</a> and <a href="https://github.com/Tessil/robin-map/blob/master/include/tsl/robin_set.h">tsl::robin_set</a> will keep rehashing its values and reallocating the table, until a fatal std::bad_alloc is thrown. `seq::ordered_set` uses a different strategy to cope with such situation:
-	When the distance value overflows, it is discarded and the table relies on pure linear hashing.
-	In this case, deleting an entry will create a tombstone instead of the standard backward shift deletion.
-	The linear probing behavior is kept until a call to ordered_set::rehash() that might switch back the behavior to robin hood hashing.

Therefore, a (very) bad hash function will only make the table slower but will never explode with a std::bad_alloc exception

## Deleting entries

`seq::ordered_set` uses backward shift deletion to avoid introducing tombstones, except for bad hash functions (see section above).
The key lookup remains fast when deleting lots of entries, and mixed scenarios involving lots of interleaved insertion/deletion are well supported.

Note however that removing a key will never trigger a rehash. The user is free to rehash the ordered_set at any point using ordered_set::rehash() member.

## Sorting

Since the ordered_set is ordered by key insertion order, it makes sense to provide a function to sort it.
The member ordered_set::sort() calls seq::sequence::sort() and uses a stable sorting algorithm.
This function rehash the full table after sorting.

## Performances

Performances of `seq::ordered_set` has been measured and compared to other node based hash tables: std::unordered_set, <a href="https://github.com/skarupke/flat_hash_map/blob/master/unordered_map.hpp">ska::unordered_set</a>, <a href="https://github.com/martinus/robin-hood-hashing">robin_hood::unordered_node_set</a>, <a href="https://github.com/greg7mdp/parallel-hashmap">phmap::node_hash_set</a> (based on abseil hash table) and <a href="https://www.boost.org/doc/libs/1_51_0/doc/html/boost/unordered_set.html">boost::unordered_set</a>.
The following table show the results when compiled with gcc 10.1.0 (-O3) for msys2 on Windows 10, using Intel(R) Core(TM) i7-10850H at 2.70GHz. Measured operations are:
-	Insert successfully 5M unique double randomly shuffled in an empty table using hash_table::insert()
-	Insert successfully 5M unique double randomly shuffled in an empty table using hash_table::insert() after reserving enough space
-	Successfully search for 5M double in random order using hash_table::find(const Key&)
-	Search for 5M double not present in the table (failed lookup)
-	Walk through the full table (5M double) using iterators
-	Erase half the table in random order using hash_table::erase(iterator)
-	Perform successfull lookups on the remaining 2.5M keys.

For each tested hash table, the maximum load factor is left to its default value, and std::hash<double> is used. For insert and erase operations, the program memory consumption is given. Note that the memory consumption is not the exact memory usage of the hash table, and should only be used to measure the difference between implementations.

Hash table name               |       Insert       |  Insert(reserve)   |Find (success) | Find (failed) |    Iterate    |       Erase        |  Find again   |
------------------------------|--------------------|--------------------|---------------|---------------|---------------|--------------------|---------------|
seq::ordered_set              |   461 ms/145 MO    |   310 ms/145 MO    |    321 ms     |    177 ms     |     5 ms      |   462 ms/222 MO    |    203 ms     |
phmap::node_hash_set          |   975 ms/188 MO    |   489 ms/187 MO    |    438 ms     |    132 ms     |     95 ms     |   732 ms/264 MO    |    250 ms     |
robin_hood::unordered_node_set|   583 ms/182 MO    |   242 ms/149 MO    |    341 ms     |    142 ms     |     83 ms     |   379 ms/258 MO    |    224 ms     |
ska::unordered_set            |   1545 ms/257 MO   |   774 ms/256 MO    |    324 ms     |    258 ms     |    128 ms     |   613 ms/333 MO    |    238 ms     |
boost::unordered_set          |   1708 ms/257 MO   |   901 ms/257 MO    |    571 ms     |    532 ms     |    262 ms     |   1073 ms/333 MO   |    405 ms     |
std::unordered_set            |   1830 ms/238 MO   |   1134 ms/232 MO   |    847 ms     |    878 ms     |    295 ms     |   1114 ms/315 MO   |    646 ms     |


This benchmark is available in file 'seq/benchs/bench_hash.cpp'.
`seq::ordered_set` is among the fastest hashtable for each operation except for failed lookup, and has a lower memory overhead.
Note that this benchmark does not represent all possible workloads, and additional tests must be fullfilled for specific scenarios.

`seq::ordered_set` uses internally and if possible compressed pointers to reduce its memory footprint. In such case, the last 16 bits of a pointer are used to store metadata. Situations where this is not possible are detected at compile time, but it is possible to manually disable this optimization by defining `SEQ_NO_COMPRESSED_PTR`.
