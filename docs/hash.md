# Hash: small collection of hash utilities

The *hash* module provides several hash-related functions:

-	`seq::hash_finalize`: mix input hash value for better avalanching
-	`seq::hash_combine`: combine 2 hash values
-	`seq::hash_bytes_murmur64`: murmurhash2 algorithm
-	`seq::hash_bytes_fnv1a`: fnv1a hash algorithm 
-	`seq::hash_bytes_komihash`: simplified [komihash](https://github.com/avaneev/komihash) hash function.


The *hash* module also provides its own hashing class called `seq::hasher` that, by default, inherits `std::hash`. `seq::hasher` is specialized for:

- All arithmetic types as well character types (use 128 bits multiplication)
- Enumerations
- Pointers
- `std::unique_ptr` and `std::shared_ptr`
- `std::tuple` and `std::pair`
- `std::basic_string`, `std::basic_string_view` and `seq::tiny_string` (`<seq/tiny_string.hpp>` must be included).

For string types, `seq::hasher` uses a seeded version of [komihash](https://github.com/avaneev/komihash). komihash is a very fast hash function that passes all [SMhasher](https://github.com/rurban/smhasher) tests, and is especially efficient on small strings.

Note that `seq::hasher` is the default hash function for all hash tables within the *seq* library: [`seq::ordered_set/map`](ordered_set.md), [`seq::radix_hash_set/map`](radix_tree.md) and [`seq::concurrent_set/map`](concurrent_map.md).