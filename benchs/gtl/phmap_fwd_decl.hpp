#ifndef phmap_fwd_decl_hpp_guard_
#define phmap_fwd_decl_hpp_guard_

// ---------------------------------------------------------------------------
// Copyright (c) 2019-2022, Gregory Popovitch - greg7mdp@gmail.com
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
// ---------------------------------------------------------------------------

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4514) // unreferenced inline function has been removed
    #pragma warning(disable : 4710) // function not inlined
    #pragma warning(disable : 4711) // selected for automatic inline expansion
#endif

#include <memory>
#include <utility>
#include <mutex>

#if defined(GTL_USE_ABSL_HASH) && !defined(ABSL_HASH_HASH_H_)
namespace absl {
template<class T>
struct Hash;
};
#endif

namespace gtl {

#if defined(GTL_USE_ABSL_HASH)
template<class T>
using Hash = ::absl::Hash<T>;
#else
template<class T>
struct Hash;
#endif

template<class T>
struct EqualTo;
template<class T>
struct Less;
template<class T>
using Allocator = typename std::allocator<T>;
template<class T1, class T2>
using Pair = typename std::pair<T1, T2>;

class NullMutex;

namespace priv {

// The hash of an object of type T is computed by using gtl::Hash.
template<class T, class E = void>
struct HashEq {
    using Hash = gtl::Hash<T>;
    using Eq   = gtl::EqualTo<T>;
};

template<class T>
using hash_default_hash = typename priv::HashEq<T>::Hash;

template<class T>
using hash_default_eq = typename priv::HashEq<T>::Eq;

// type alias for std::allocator so we can forward declare without including other headers
template<class T>
using Allocator = typename gtl::Allocator<T>;

// type alias for std::pair so we can forward declare without including other headers
template<class T1, class T2>
using Pair = typename gtl::Pair<T1, T2>;

struct empty {};

} // namespace priv

// ------------- forward declarations for hash containers ----------------------------------
template<class T,
         class Hash  = gtl::priv::hash_default_hash<T>,
         class Eq    = gtl::priv::hash_default_eq<T>,
         class Alloc = gtl::priv::Allocator<T>> // alias for std::allocator
class flat_hash_set;

template<class K,
         class V,
         class Hash  = gtl::priv::hash_default_hash<K>,
         class Eq    = gtl::priv::hash_default_eq<K>,
         class Alloc = gtl::priv::Allocator<gtl::priv::Pair<const K, V>>> // alias for std::allocator
class flat_hash_map;

template<class T,
         class Hash  = gtl::priv::hash_default_hash<T>,
         class Eq    = gtl::priv::hash_default_eq<T>,
         class Alloc = gtl::priv::Allocator<T>> // alias for std::allocator
class node_hash_set;

template<class Key,
         class Value,
         class Hash  = gtl::priv::hash_default_hash<Key>,
         class Eq    = gtl::priv::hash_default_eq<Key>,
         class Alloc = gtl::priv::Allocator<gtl::priv::Pair<const Key, Value>>> // alias for std::allocator
class node_hash_map;

template<class T,
         class Hash    = gtl::priv::hash_default_hash<T>,
         class Eq      = gtl::priv::hash_default_eq<T>,
         class Alloc   = gtl::priv::Allocator<T>, // alias for std::allocator
         size_t N      = 4,                       // 2**N submaps
         class Mutex   = gtl::NullMutex,          // use std::mutex to enable internal locks
         class AuxCont = gtl::priv::empty>
class parallel_flat_hash_set;

template<class K,
         class V,
         class Hash    = gtl::priv::hash_default_hash<K>,
         class Eq      = gtl::priv::hash_default_eq<K>,
         class Alloc   = gtl::priv::Allocator<gtl::priv::Pair<const K, V>>, // alias for std::allocator
         size_t N      = 4,                                                 // 2**N submaps
         class Mutex   = gtl::NullMutex,                                    // use std::mutex to enable internal locks
         class AuxCont = gtl::priv::empty>
class parallel_flat_hash_map;

template<class T,
         class Hash    = gtl::priv::hash_default_hash<T>,
         class Eq      = gtl::priv::hash_default_eq<T>,
         class Alloc   = gtl::priv::Allocator<T>, // alias for std::allocator
         size_t N      = 4,                       // 2**N submaps
         class Mutex   = gtl::NullMutex,          // use std::mutex to enable internal locks
         class AuxCont = gtl::priv::empty>
class parallel_node_hash_set;

template<class Key,
         class Value,
         class Hash    = gtl::priv::hash_default_hash<Key>,
         class Eq      = gtl::priv::hash_default_eq<Key>,
         class Alloc   = gtl::priv::Allocator<gtl::priv::Pair<const Key, Value>>, // alias for std::allocator
         size_t N      = 4,                                                       // 2**N submaps
         class Mutex   = gtl::NullMutex, // use std::mutex to enable internal locks
         class AuxCont = gtl::priv::empty>
class parallel_node_hash_map;

// -----------------------------------------------------------------------------
// phmap::parallel_*_hash_* using std::mutex by default
// -----------------------------------------------------------------------------
template<class T,
         class Hash    = gtl::priv::hash_default_hash<T>,
         class Eq      = gtl::priv::hash_default_eq<T>,
         class Alloc   = gtl::priv::Allocator<T>,
         size_t N      = 4,
         class AuxCont = gtl::priv::empty>
using parallel_flat_hash_set_m = parallel_flat_hash_set<T, Hash, Eq, Alloc, N, std::mutex, AuxCont>;

template<class K,
         class V,
         class Hash    = gtl::priv::hash_default_hash<K>,
         class Eq      = gtl::priv::hash_default_eq<K>,
         class Alloc   = gtl::priv::Allocator<gtl::priv::Pair<const K, V>>,
         size_t N      = 4,
         class AuxCont = gtl::priv::empty>
using parallel_flat_hash_map_m = parallel_flat_hash_map<K, V, Hash, Eq, Alloc, N, std::mutex, AuxCont>;

template<class T,
         class Hash    = gtl::priv::hash_default_hash<T>,
         class Eq      = gtl::priv::hash_default_eq<T>,
         class Alloc   = gtl::priv::Allocator<T>,
         size_t N      = 4,
         class AuxCont = gtl::priv::empty>
using parallel_node_hash_set_m = parallel_node_hash_set<T, Hash, Eq, Alloc, N, std::mutex, AuxCont>;

template<class K,
         class V,
         class Hash    = gtl::priv::hash_default_hash<K>,
         class Eq      = gtl::priv::hash_default_eq<K>,
         class Alloc   = gtl::priv::Allocator<gtl::priv::Pair<const K, V>>,
         size_t N      = 4,
         class AuxCont = gtl::priv::empty>
using parallel_node_hash_map_m = parallel_node_hash_map<K, V, Hash, Eq, Alloc, N, std::mutex, AuxCont>;

// -----------------------------------------------------------------------------
// phmap::parallel_*_hash_* using default Hash/Eq/Alloc
// -----------------------------------------------------------------------------
template<class T,
         size_t N    = 4,
         class Mutex = gtl::NullMutex // use std::mutex to enable internal locks
         >
using parallel_flat_hash_set_d = parallel_flat_hash_set<T,
                                                        gtl::priv::hash_default_hash<T>,
                                                        gtl::priv::hash_default_eq<T>,
                                                        gtl::priv::Allocator<T>,
                                                        N,
                                                        Mutex,
                                                        gtl::priv::empty>;

template<class K,
         class V,
         size_t N    = 4,
         class Mutex = gtl::NullMutex // use std::mutex to enable internal locks
         >
using parallel_flat_hash_map_d = parallel_flat_hash_map<K,
                                                        V,
                                                        gtl::priv::hash_default_hash<K>,
                                                        gtl::priv::hash_default_eq<K>,
                                                        gtl::priv::Allocator<gtl::priv::Pair<const K, V>>,
                                                        N,
                                                        Mutex,
                                                        gtl::priv::empty>;

template<class T,
         size_t N    = 4,
         class Mutex = gtl::NullMutex // use std::mutex to enable internal locks
         >
using parallel_node_hash_set_d = parallel_node_hash_set<T,
                                                        gtl::priv::hash_default_hash<T>,
                                                        gtl::priv::hash_default_eq<T>,
                                                        gtl::priv::Allocator<T>,
                                                        N,
                                                        Mutex,
                                                        gtl::priv::empty>;

template<class K,
         class V,
         size_t N    = 4,
         class Mutex = gtl::NullMutex // use std::mutex to enable internal locks
         >
using parallel_node_hash_map_d = parallel_node_hash_map<K,
                                                        V,
                                                        gtl::priv::hash_default_hash<K>,
                                                        gtl::priv::hash_default_eq<K>,
                                                        gtl::priv::Allocator<gtl::priv::Pair<const K, V>>,
                                                        N,
                                                        Mutex,
                                                        gtl::priv::empty>;
;

// ------------- forward declarations for btree containers ----------------------------------
template<typename Key, typename Compare = gtl::Less<Key>, typename Alloc = gtl::Allocator<Key>>
class btree_set;

template<typename Key, typename Compare = gtl::Less<Key>, typename Alloc = gtl::Allocator<Key>>
class btree_multiset;

template<typename Key,
         typename Value,
         typename Compare = gtl::Less<Key>,
         typename Alloc   = gtl::Allocator<gtl::priv::Pair<const Key, Value>>>
class btree_map;

template<typename Key,
         typename Value,
         typename Compare = gtl::Less<Key>,
         typename Alloc   = gtl::Allocator<gtl::priv::Pair<const Key, Value>>>
class btree_multimap;

} // namespace gtl

#ifdef _MSC_VER
    #pragma warning(pop)
#endif

#endif // phmap_fwd_decl_hpp_guard_
