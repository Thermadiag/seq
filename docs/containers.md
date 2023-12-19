# Containers: original STL-like containers

The *containers* module defines several container classes as alternatives to STL containers or providing features not present in the STL.
These containers generally adhere to the properties of STL containers (in C++17 version), though there are often some associated API differences and/or implementation details which differ from the standard library.

The *seq* containers are not necessarly drop-in replacement for their STL counterparts as they usually provide different iterator/reference statibility rules or different exception guarantees.

Currently, the *containers* module provide 5 types of containers:
-	Sequential random-access containers: 
	-	[seq::devector](devector.md): double ended vector that can be optimized for front operations, back operations or both. Similar interface to `std::deque`.
	-	[seq::tiered_vector](tiered_vector.md): tiered vector implementation optimized for fast insertion and deletion in the middle. Similar interface to `std::deque`.
	-	[seq::cvector](cvector.md): vector-like class storing its values in a compressed way to reduce program memory footprint. Similar interface to `std::vector`.
-	Sequential stable non random-access container: `seq::sequence`, fast stable list-like container.
-	Sorted containers: 
	-	[seq::flat_set](flat_set.md) : flat set container similar to boost::flat_set but based on seq::tiered_vector and providing fast insertion/deletion of single elements.
	-	`seq::flat_map`: associative version of `seq::flat_set`.
	-	`seq::flat_multiset`: similar to `seq::flat_set` but supporting duplicate keys.
	-	`seq::flat_multimap`: similar to `seq::flat_map` but supporting duplicate keys.
	-	[seq::radix_set](radix_tree.md) : radix based sorted container with a similar interface to std::set. Provides very fast lookup.
	-	`seq::radix_map`: associative version of `seq::radix_set`.
-	Hash tables: 
	-	[seq::ordered_set](ordered_set.md): Ordered robin-hood hash table with backward shift deletion. Drop-in replacement for `std::unordered_set` (except for the bucket interface) with iterator/reference stability, and additional features (see class documentation).
	-	`seq::ordered_map`: associative version of `seq::ordered_set`.
	-	[seq::radix_hash_set](radix_tree.md): radix based hash table with a similar interface to `std::unordered_set`. Uses incremental rehash, no memory peak.
	-	`seq::radix_hash_map`: associative version of `seq::radix_hash_set`.
	-	[seq::concurrent_map](concurrent_map.md) and `seq::concurrent_set`: higly scalable concurrent hash tables.
-	Strings:
	-	[seq::tiny_string](tiny_string.md): string-like class with configurable Small String Optimization and tiny memory footprint. Makes most string containers faster.

