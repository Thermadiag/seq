# Tiered vector

`seq::tiered_vector` is a random-access, bucket based container providing a similar interface to std::deque.
Its internals are however very different as it is implemented as a <a href="http://cs.brown.edu/cgc/jdsl/papers/tiered-vector.pdf">tiered vector</a>.
Instead of maintaining a vector of fixed size buckets, `seq::tiered_vector` uses a bucket size close to sqrt(size()). The bucket size is always a power of 2 for fast division and modulo.
Furtheremore, the bucket is not a linear buffer but is instead implemented as a circular buffer.
This allows a complexity of O(sqrt(N)) for insertion and deletion in the middle of the tiered_vector instead of O(N) for std::deque. Inserting and deleting elements at both ends is still O(1).

`seq::tiered_vector` internally uses `seq::devector` to store the buckets.
`seq::tiered_vector` is used as the backend container for [seq::flat_set](flat_set.md), `seq::flat_map`, `seq::flat_multiset` and `seq::flat_multimap`.


## Interface

`seq::tiered_vector` interface is the same as `std::deque`, with the additional members:
-	for_each() providing a faster way to walk through the tiered_vector than iterators,
-	resize_front() to resize the tiered_vector from the front instead of the back of the container,


## Bucket managment

The `seq::tiered_vector` maintains internally an array of circular buffers (or buckets). At any moment, all buckets have the same size which is a power of 2. At container initialization, the bucket size is given by template parameter *MinBSize* which is, by default, between 64 and 8 depending on value_type size.
Whenever `seq::tiered_vector` grows (through push_back(), push_front(), insert(), resize() ...), the new bucket size is computed using the template parameter *FindBSize*. *FindBSize* must provide the member

```cpp
 unsigned  FindBSize::operator() (size_t size, unsigned MinBSize, unsigned MaxBSize) const noexcept ;
```

returning the new bucket size based on the container size, the minimum and maximum bucket size. Default implementation returns a value close to sqrt(size()) rounded up to the next power of 2.

If the new bucket size is different than the current one, new buckets are created and elements from the old buckets are moved to the new ones. This has the practical consequence to <b> invalidate all iterators and references on growing or shrinking </b>, as opposed to std::deque which maintains references when inserting/deleting at both ends.


## Inserting and deleting elements

Inserting or deleting elements at the back or the front behaves the same way as for std::deque, except if the bucket size is updated (as explained above).

Inerting an element in the middle of `seq::tiered_vector` follows these steps:
- The bucket index and the element position within the bucket are first computed
- The back element of the bucket is extracted and removed from the bucket
- The new element is inserted at the right position. Since the bucket is implemented as a dense circular buffer, at most half of the bucket elements must be moved (toward the left or the right, whichever is the shortest path)
- The back value that was previously removed is inserted at the front of the next bucket
- The next bucket back value is extracted and inserted at the front of the following bucket
- ....
- And so forth until we reach the last bucket. 

This operation of <em>insert front/pop back</em> is very fast on a circular buffer as it involves only two element moves and shifting the begin index of the buffer. If the bucket size is exactly sqrt(size()), inserting an element in the middle performs in O(sqrt(N)) as it involves as many element moves within a single bucket than between buckets.

In practice the buckets size should be greater than sqrt(size()) as moving elements within a bucket is much faster than between buckets due to cache locality.
Note that, depending on the insertion location, elements can be shifted toward the front bucket instead of the back one if this is the shortest path. This practically divides by 2 (on average) the number of moved elements.
Erasing an element in the middle follows the exact same logic.
Note that inserting/removing relocatable types (where `seq::is_relocatable<T>::value` is true) is faster than for other types.


## Exception guarantee

All insertion/deletion operations on a `seq::tiered_vector` are much more complex than for a std::deque. Especially, each operation might change the bucket size, and therefore trigger the allocation of new buckets plus moving all elements within the new buckets.
Although possible, providing strong exception guarantee on `seq::tiered_vector` operations would have added a very complex layer hurting its performances. Therefore, all `seq::tiered_vector` operations only provide <b>basic exception guarantee</b>.


## Iterators and references invalidation

As explained above, all `seq::tiered_vector` operations invalidate iterators and references, except for swapping two `seq::tiered_vector`.

The only exception is when providing a minimum bucket size (*MinBSize*) equals to the maximum bucket size *MaxBSize*).
In this case, inserting/deleting elements will <em>never</em> change the bucket size and move all elements within new buckets. This affects the members emplace_back(), push_back(), emplace_front() and push_front() that provide the same invalidation rules as for std::deque.


## Performances

`seq::tiered_vector` was optimized to match libstdc++ `std::deque` performances as close as possible.
My benchmarhs show that most members are actually as fast or faster than libstdc++ `std::deque`.

Usually, iterating through a `seq::tiered_vector` is faster than through a std::deque, and the random-access `operator[](size_t)` is also faster. Making a lot of random access based on iterators can be slightly slower with `seq::tiered_vector` depending on the use case. For instance, sorting a `seq::tiered_vector` is slower than sorting a `std::deque`.

Inserting/deleting single elements in the middle of a `seq::tiered_vector` is several order of magnitudes faster than std::deque due to the tiered-vector implementation.

`seq::tiered_vector` is faster when working with relocatable types (where `seq::is_relocatable<T>::value == true`).

The standard conlusion is: you should always benchmark with your own use cases.
