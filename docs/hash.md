# Hash: small collection of hash utilities

The *hash* module provides several hash-related functions:
-	*seq::hash_combine*: combine 2 hash values
-	*seq::hash_bytes_murmur64*: murmurhash2 algorithm
-	*seq::hash_bytes_fnv1a*: fnv1a hash algorithm working on chunks of 4 to 8 bytes
-	*seq::hash_bytes_fnv1a_slow*: standard fnv1a hash algorithm

Note that the specialization of *std::hash* for *seq::tiny_string* uses murmurhash2 algorithm.
