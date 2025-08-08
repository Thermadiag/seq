# Flat set

``seq::flat_set`` is a sorted associative container similar to <a href="https://www.boost.org/doc/libs/1_64_0/doc/html/boost/container/flat_set.html">boost::flat_set</a>, but relying on a [seq::tiered_vector](tiered_vector.md) instead of a flat array.
Therefore, it inherits seq::tiered_vector  properties: faster insertion and deletion of individual values (thanks to its tiered-vector based implementation), fast iteration, random access, invalidation of references/iterators on insertion and deletion.

All keys in a ``seq::flat_set`` are unique.
All references and iterators are invalidated when inserting/removing keys.

## Interface

``seq::flat_set`` provides a similar interface to std::set (C++17) with the following differences:
-	The node related functions are not implemented,
-	The member `flat_set::pos()` is used to access elements at a random location,
-	The members `flat_set::tvector()` returns a reference to the underlying seq::tiered_vector object,
-	Its iterator and const_iterator types are random access iterators.

The underlying tiered_vector object stores plain non const Key objects, and `seq::flat_map` iterators return objects of type `std::pair<Key,Value>`.

In addition to members returning (const_)iterator(s), the flat_set provides the same members ending with the '_pos' prefix and returning positions within the tiered_vector instead of iterators. These functions are slightly faster than the iterator based members.


## Direct access to tiered_vector

Unlike most flat set implementations, it it possible to directly access and modify the underlying tiered_vector. 
This possibility must be used with great care, as modifying directly the tiered_vector might break the key ordering. 
When calling the non-const version of `flat_set::tvector()`, the flat_set will be marked as dirty, and further attempts to call functions like `flat_set::find()` of `flat_set::lower_bound()` (functions based on key ordering) will throw a `std::logic_error`.

Therefore, after finishing modifying the tiered_vector, you must call `flat_set::sort()` to sort again the tiered_vector, remove potential duplicates, and mark the flat_set as non dirty anymore.

This way of modifying a flat_set must be used carefully, but is way faster than multiple calls to `flat_set::insert()` or `flat_set::erase()`.


## Range insertion

Inserting a range of values using `flat_set::insert(first, last)` is faster than inserting keys one by one, and should be favored if possible.
Range insertion works the following way:
-	New keys are first appended to the underlying tiered_vector
-	These new keys are sorted directly within the tiered_vector
-	Old keys and new keys are merged using `std::inplace_merge`
-	Duplicate values are removed if necessary using `std::unique`.

If you need to keep stability when inserting range of values, you must set the Stable template parameter to true.


## Exception guarantee

All ``seq::flat_set`` operations only provide *basic exception guarantee*, exactly like the underlying `seq::tiered_vector`.


## Performances

Performances of `seq::flat_set` has been measured and compared to std::set, std::unordered_set, <a href="https://www.boost.org/doc/libs/1_64_0/doc/html/boost/container/flat_set.html">boost::flat_set</a> and <a href="https://github.com/greg7mdp/parallel-hashmap">phmap::btree_set</a> (based on abseil btree_set). 
The following table show the results when compiled with gcc 10.1.0 (-O3) for msys2 on Windows 10, using Intel(R) Core(TM) i7-10850H at 2.70GHz. Measured operations are:
-	Insert successfully a range of 1M unique double randomly shuffled using set_class::insert(first,last)
-	Insert successfully 1M unique double randomly shuffled one by one using set_class::insert(const Key&)
-	Insert 1M double randomly shuffled one by one and already present in the set (failed insertion)
-	Successfully search for 1M double in the set using set_class::find(const Key&), or flat_set::find_pos(const Key&)
-	Search for 1M double not present in the set (failed lookup)
-	Walk through the full set (1M double) using iterators
-	Successfull find and erase all 1M double one by one using set_class::erase(iterator)

Note the the given memory is NOT the memory footprint of the container, but the one of the full program. It should be used relatively to compare memory usage difference between each container.

Set name                      |   Insert(range)    |       Insert       |Insert(failed) |Find (success) | Find (failed) |    Iterate    |     Erase     |
------------------------------|--------------------|--------------------|---------------|---------------|---------------|---------------|---------------|
seq::flat_set                 |    46 ms/15 MO     |    408 ms/16 MO    |    128 ms     |    130 ms     |    122 ms     |     1 ms      |    413 ms     |
phmap::btree_set              |    135 ms/18 MO    |    118 ms/18 MO    |    118 ms     |    119 ms     |    120 ms     |     3 ms      |    131 ms     |
boost::flat_set<T>            |    57 ms/15 MO     |   49344 ms/17 MO   |    133 ms     |    132 ms     |    127 ms     |     1 ms      |   131460 ms   |
std::set                      |    457 ms/54 MO    |    457 ms/54 MO    |    449 ms     |    505 ms     |    502 ms     |     92 ms     |    739 ms     |
std::unordered_set            |    187 ms/46 MO    |    279 ms/50 MO    |    100 ms     |    116 ms     |    155 ms     |     29 ms     |    312 ms     |

Below is the same benchmark using random seq::tstring of length 13 (using Small String Optimization):

Set name                      |   Insert(range)    |       Insert       |Insert(failed) |Find (success) | Find (failed) |    Iterate    |     Erase     |
------------------------------|--------------------|--------------------|---------------|---------------|---------------|---------------|---------------|
seq::flat_set                 |    127 ms/30 MO    |    891 ms/31 MO    |    252 ms     |    245 ms     |    240 ms     |     1 ms      |    904 ms     |
phmap::btree_set              |    280 ms/37 MO    |    278 ms/37 MO    |    266 ms     |    292 ms     |    279 ms     |     10 ms     |    292 ms     |
boost::flat_set<T>            |    107 ms/30 MO    |  585263 ms/32 MO   |    228 ms     |    232 ms     |    232 ms     |     0 ms      |   601541 ms   |
std::set                      |    646 ms/77 MO    |    640 ms/77 MO    |    611 ms     |    672 ms     |    710 ms     |     87 ms     |    798 ms     |
std::unordered_set            |    205 ms/54 MO    |    319 ms/57 MO    |    157 ms     |    192 ms     |    220 ms     |     34 ms     |    380 ms     |


These benchmarks are available in file 'seq/benchs/bench_map.cpp'.
`seq::flat_set` behaves quite well compared to phmap::btree_set or boost::flat_set, and is even faster for single insertion/deletion than std::set for double type.

`seq::flat_set` insertion/deletion performs in O(sqrt(N)) on average, as compared to std::set that performs in O(log(N)).
`seq::flat_set` is therfore slower for inserting and deleting elements than std::set when dealing with several millions of elements.
Lookup functions (find, lower_bound, upper_bound...) still perform in O(log(N)) and remain faster that std::set couterparts because of the underlying seq::tiered_vector cache friendliness.
flat_set will almost always be slower for element lookup than boost::flat_set wich uses a single dense array, except for very small keys (like in above benchmark).

Several factors will impact the performances of `seq::flat_set`:
-	Relocatable types (where `seq::is_relocatable<value_type>::value` is true) are faster than other types for insertion/deletion, as tiered_vector will use memmove to move around objects. Therefore, a flat_set of seq::tstring will always be faster than std::string.
-	Performances of insertion/deletion decrease as `sizeof(value_type)` increases. This is especially true for insertion/deletion, much less for lookup functions which remain (more or less) as fast as boost::flat_set.
-	All members using the '_pos' prefix are usually slightly faster than their iterator based counterparts.
