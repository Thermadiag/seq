# Flat set

`seq::flat_set` is a sorted associative container similar to <a href="https://www.boost.org/doc/libs/1_64_0/doc/html/boost/container/flat_set.html">boost::flat_set</a>, but relying on a [seq::tiered_vector](tiered_vector.md) instead of a flat array.
Therefore, it inherits seq::tiered_vector  properties: faster insertion and deletion of individual values (thanks to its tiered-vector based implementation), fast iteration, random access, invalidation of references/iterators on insertion and deletion.

All keys in a `seq::flat_set` are unique.
All references and iterators are invalidated when inserting/removing keys.

## Interface

`seq::flat_set` provides a similar interface to std::set with the following differences:
-	The node related functions are not implemented,
-	The member `flat_set::pos()` is used to access to a random location,
-	The members `flat_set::tvector()` returns a reference to the underlying seq::tiered_vector object,
-	Its iterator and const_iterator types are random access iterators.

The underlying tiered_vector object stores plain non const Key objects. However, in order to avoid modifying the keys through iterators (and potentially invalidating the order), both iterator and const_iterator types can only return const references.

In addition to members returning (const_)iterator(s), the flat_set provides the same members ending with the '_pos' prefix and returning positions within the tiered_vector instead of iterators. These functions are slightly faster than the iterator based members.


## Direct access to tiered_vector

Unlike most flat set implementations, it it possible to directly access and modify the underlying tiered_vector directly. 
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

Note that flat_set used by default <a href="https://github.com/orlp/pdqsort">pdqsort</a> implementation from Orson Peters which is not stable.
If you need to keep stability when inserting range of values, you must set the Stable template parameter to true. `std::stable_sort` will be used instead of pdqsort (std::inplace_merge and std::unique are already stable).


## Exception guarantee

All `seq::flat_set` operations only provide *basic exception guarantee*, exactly like the underlying `seq::tiered_vector`.


## Performances

Performances of `seq::flat_set` has been measured and compared to `std::set`, `std::unordered_set`, <a href="https://www.boost.org/doc/libs/1_64_0/doc/html/boost/container/flat_set.html">boost::flat_set</a> and <a href="https://abseil.io/docs/cpp/guides/container">absel::btree_set</a>.
The following table show the results when compiled with gcc 10.1.0 (-O3) for msys2 on Windows 10, using Intel(R) Core(TM) i7-10850H at 2.70GHz. Measured operations are:
-	Insert successfully a range of 1M unique double randomly shuffled using set_class::insert(first,last)
-	Insert successfully 1M unique double randomly shuffled one by one using set_class::insert(const Key&)
-	Insert 1M double randomly shuffled one by one and already present in the set (failed insertion)
-	Successfully search for 1M double in the set using set_class::find(const Key&), or flat_set::find_pos(const Key&)
-	Search for 1M double not present in the set (failed lookup)
-	Walk through the full set (1M double) using iterators
-	Erase all 1M double one by one using set_class::erase(iterator)

Set name                      |   Insert(range)    |       Insert       |Insert(failed) |Find (success) | Find (failed) |    Iterate    |     Erase     |
------------------------------|--------------------|--------------------|---------------|---------------|---------------|---------------|---------------|
seq::flat_set                 |    61 ms/15 MO     |    449 ms/17 MO    |    135 ms     |    144 ms     |    128 ms     |     1 ms      |    432 ms     |
phmap::btree_set              |    121 ms/18 MO    |    138 ms/19 MO    |    118 ms     |    141 ms     |    112 ms     |     2 ms      |    127 ms     |
boost::flat_set<T>            |    60 ms/15 MO     |   49314 ms/16 MO   |    130 ms     |    135 ms     |    129 ms     |     0 ms      |   131372 ms   |
std::set                      |    470 ms/54 MO    |    489 ms/54 MO    |    479 ms     |    535 ms     |    526 ms     |     82 ms     |    737 ms     |
std::unordered_set            |    185 ms/47 MO    |    284 ms/50 MO    |     97 ms     |    129 ms     |    153 ms     |     30 ms     |    332 ms     |

This benchmark is available in file 'seq/benchs/bench_map.hpp'.
`seq::flat_set` behaves quite well compared to absl::btree_set or boost::flat_set, and is even faster for single insertion/deletion than std::set.

`seq::flat_set` insertion/deletion performs in O(sqrt(N)) on average, as compared to std::set that performs in O(log(N)).
`seq::flat_set` is therfore slower for inserting and deleting elements than std::set when dealing with several millions of elements.
Lookup functions (find, lower_bound, upper_bound...) still perform in O(log(N)) and remain faster that std::set couterparts because seq::tiered_vector is much more cache friendly than std::set. flat_set will always be slower for element lookup than boost::flat_set wich uses a single dense array.

Inserting/removing relocatable types (where `seq::is_relocatable<T>::value` is true) is faster than for other types.

Note that insertion/deletion of single elements become slower when sizeof(Key) increases, where std::set performances remain stable.
Also note that the members using the '_pos' prefix are usually slightly faster than their iterator based counterparts.
