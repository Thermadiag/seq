# Radix tree: radix based containers

The `seq` library provides the containers `seq::radix_set`, `seq::radix_map`, `seq::radix_hash_set` and `seq::radix_hash_map` all based on the same radix tree. The First 2 ones are sorted containers while the last 2 containers are (unsorted) hash tables.
The Radix tree implementation provided by the `seq` library is a new kind of radix tree called VART for *Variable Arity Radix Tree*.

# Radix trees

A radix tree is a kind of a space-optimized prefix tree in which each node that is the only child is merged with its parent. The radix tree uses the bits representation of a key to build a sorted tree with an arity (number of children) which is a power of 2.
There are already lots of radix tree implementations that we can sort by arity:
-	*256*: ART (Adaptative Radix Tree) and Judy Array walk through keys by increment of 8 bits. This creates low depth trees, reducing the number of investigated nodes for all operations (i.e. reduce cache misses). 
They use different techniques to reduce the memory footprint of nodes having few children, like having different types of nodes depending on the children count.
-	*64/32*: HAMT (Hash Array Mapped Trie) are unsorted radix trees using a hashed representation of the key instead of the key itself. They usually have an arity of 6 on 64 bits platform and 32 on 32 bits platform. 
A bitmap is used to reduce the size of a node based on its children count. A fast POPCOUNT implementation must be available on the targeted instruction set architecture.
-	*2*: PATRICIA trie is a special variant of the radix 2 (binary) trie.

Most implementations use common tricks to reduce the space utilization and maximize lookup speed, like path compression for nodes having only one child.

# Variable Arity Radix Tree (VART)

The VART has the following specificities:
-	Unlike above radix trees, and as its name implies, each node of a VART has a variable arity. It starts at a low value, tipically 4, and grows lazily as more keys are inserted.
-	The keys are stored in leaf nodes only, without compression.
-	Each leaf node is a flat sorted array of keys (or pairs of key/value for the radix_map) containing at most MAX keys, where MAX depends on the key size (typically 64).
-	Intermediate nodes are called 'directories'  and can potentially have an unlimited number of children (other directories or leaf nodes).
-	Since keys are stored 'as is' without compression, `seq::radix_set` and `seq::radix_map` can provide the same interface as `std::set` and `std::map`.
-	Keys can be arithmetic types, pointers, standard strings or wide strings.
-	Keys are unique within the tree.

For the rest of the documentation, we consider a VART with a start arity of 4 and maximum leaf node size of 64 keys.

## Insertion process


The VART starts with a root directory of 4 children that contains null nodes. When inserting a new key, 2 bits of the key are consumed to get the child node index. There are 4 possibilities to insert the key based on the child content:
1.	The child is null: a new leaf node is created (flat array) and the key is inserted inside.
2.	The child is a leaf node having less than 64 keys: the new key is inserted inside while maintaining the array order. The insertion position within a leaf node is typically computed with a lower bound algorithm.
3.	The child is a directory: 2 additional bits of the key are consumed to get its child node index, and we go back to step 1.
4.	The child is a full leaf node with 64 keys: the leaf is removed from its parent directory, and each key is then reinserted in this directory by consuming 2 more bits. This will dispatch the keys in new leaf nodes and create intermediate directories.

Therefore, as more keys are inserted, the depth of the tree will grow in a totally unbalanced way following the keys bits representation. This can be seen as a way to push the keys deeper and deeper within the tree.
This strategy works well for arithmetic keys as they all have a unique bits representation and the depth of the tree will be bounded to (sizeof(Key)*8) / 2, therefore 32 levels at most for 64 bits keys.
For variable length keys like strings, a couple of problem arises:
-	Short strings bits can be exhausted at some depth of the tree. To cope with that, we consider that the remaining bits are all 0.
-	Some sequences of input strings will always fall into the same leaf nodes. The worst possible inputs are the keys '\0', '\0\0', '\0\0\0'... as the step 4 of the insertion process (dispatch a full leaf node) will always produce a new full leaf node, that will then be dispatched to a new full leaf node, etc.
To cope with such situation, the maximum depth of the tree is bounded (typically to 32 levels) and the last leaf node is turned into an unlimited sized array called 'vector node'. For `seq::radix_set` and `seq::radix_map`, the 'vector node' is implemented as a `seq::flat_set`, ensuring a worst-case of O(sqrt(N)) for insertion/deletion and 0(log(N)) for lookup.

VART also uses path compression to reduce the tree depth for keys sharing common prefixes.


## Level merging


At some point during the insertion process, a directory (the root one or an intermediate directory) will be entirely filled with sub-directories. These sub-directories can be removed to efficiently reduce the tree depth.
For that, the parent directory arity is multiplied by 4, the sub-directories are suppressed, and their children nodes (other directories or leaf nodes) are reinserted into the parent directory.
We don't need to process the keys in order to find the position of each node within the new parent directory, as it is the combination of the sub-directory position and its child position.
Below is a simplified code sample describing this process:

```cpp

// here, 'parent' is the parent directory

// create a new parent directory with an arity equals to parent_arity * 4
// Note that the arity is stored as its log2 representation (number of bits)
directory* new_parent = directory::create(parent->bit_length + 2);

// Walk through the sub-directories
for (unsigned i = 0; i < parent->child_count(); ++i)
{
	// get sub-directory
	directory* sub_dir = parent->child(i);

	// walk through all nodes within sub_dir
	for (unsigned j = 0; j < sub_dir->child_count(); ++j)
	{
		void* child = sub_dir->child(j); // child could be null, a directory or a leaf node

		// compute the child position within new_parent 
		unsigned position = j | (i << sub_dir->bit_length);

		// add child to new_parent
		new_parent->set_child(position, child);
	}

	// we can safely destroy sub_dir as we moved all its children to new_parent
	directory::destroy(sub_dir);
}

// we can safely destroy parent as we moved its content to new_parent
directory::destroy(parent);
		
```

This level merging process is done on insertion step 4, during the keys dispatching to new nodes. The above code sample is straightforward and only works when all subdirectories have the same arity of 4.
Other scenarios are possible: the sub-directories have variable arities (because of other level mergings), or arities greater than 4, or store a non 0 prefix length. All these cases are not described here, but properly handled by the VART.

The insertion process is in fact more complex than described previously: because of this level merging, each intermediate directory might consume more than 2 bits of the key to get its child index, depending on its arity.

The level merging will reduce the depth of some branches of the tree. For an even distribution of keys (or if using a hash function like `seq::radix_hash_set`), the tree will usually end up as a unique wide root directory containing leaf nodes only.
The `seq::radix_hash_set` and `seq::radix_hash_map` classes (hash tables) use this property to provide fast lookup.


## Performances

Most types of radix trees (like ART one) support O(k) operations (where k is the size of the key in bytes). `seq::radix_set/map` does not depend on the key sizes, but instead on their distribution.
In terms of big O notation, `seq::radix_set/map` behaves in the following way:
-	Insertion is anywhere in-between O(1) and O(sqrt(N)). For evenly distributed keys (like random values), insertion will behave in O(1) thanks to the level merging. O(sqrt(N)) is the upper bound, when all entries fall into the last nodes implemented with seq::flat_set. Note that in this case, the constant factor is quite high.
-	Likewise, lookup is anywhere in-between O(1) for evenly distributed keys and O(log(N)) (seq::flat_set behavior) with a high constant factor.

## Key lookups

Searching for a key in a VART looks like the insertion process. It starts from the root node and N bits of the key are consumed depending on the root directory arity. 4 scenarios are possible based on the child node content:
1.	The child is a null node: the search process ends there as the key is not present.
2.	The child is a directory: M additional bits of the key are consumed based on the directory arity and we go back to step 1.
3.	The child is a vector node: the key is searched using a binary search (for sorted VART) or a linear search (for hash tables).
4.	The child is a leaf node: the key is searched within a flat array of at most 64 keys.

The key lookup within a leaf node is similar to what is used in swiss tables: the leaf stores a 8 bits hash value for each key and up to 32 keys (if AVX2 is available) are investigated in parallel.


## Key deletion

Removing a key is straightforward. The key is first removed from its node (either a leaf or vector node) while maintaining node ordering (for sorted trees). If the leaf becomes empty it is destroyed and the leaf location in the parent directory is marked as null.
If a directory only contains null nodes, it is destroyed and its location within its parent directory is marked as null. This process keeps repeating while going upward through the tree, until a non empty directory or the root one is reached.

Note that the deletion process could have benefited from the inverse strategy as the level merging (a kind of level unfolding). This would smoothen the memory pattern while deletting entries, but is also more complex and error prone.


## Hash table VART

Both `seq::radix_hash_set` and `seq::radix_hash_map` uses VART behind the scene, except that the tree structure is built upon the hashed representation of the keys instead of the keys themselves.
The step 4 of the insertion process (key dispatching) requires to rehash the keys within a leaf node in order to find their new locations. Therefore, the tree grows using incremental rehash by chunks of 64 keys. This is very similar to *extendible hashing*, except that a poor hash function will result in an unbalanced tree (still with a low memory footprint) instead of a huge and sparsely populated root directory. With a good hash function, the tree usually becomes a flat array (root directory) of leaf nodes, ensuring fast lookups. 

A `seq::radix_hash_set/map` grows rather smoothly for a hash table, with an almost linear memory pattern. The absence of memory peak makes it one of the least memory gready hash table implementation.
Another benefit of `seq::radix_hash_set/map` is its lower latency on insert/erase operations thanks to the incremental rehash, making it more suitbale for firm real-time applications.

### Collision resolution

In a VART, collisions cause more leaf node rehash until we exhaust the 64 bits (or 32 on 32 bits architecture) of the hash value. In this case, the last leaf node is replaced by an unlimited sized vector (basically a std::vector) using linear search. Therefore, a bad hash function induces a worst-case O(N) for insertion and lookup.
It is possible to mitigate such scenarios by providing a 'less than' comparison function as template argument to `seq::radix_hash_set/map`. In this case, the vector node uses a `seq::flat_set` instead of a std::vector, with a worst-case O(sqrt(N)) on insertion and O(log(N)) on lookup.


## Interface

The interface of `seq::radix_set` and `seq::radix_map` are similar to those of `std::set` and `std::map`. However, a `seq::radix_set/map` invalidates all references and iterators on insert/erase operations, and only provides Basic exception guarantees.
Note that dereferencing an iterator of `seq::radix_map` returns a `std::pair<Key,Value>` instead of `std::pair<const Key,Value>`.

`seq::radix_set` and `seq::radix_map` do not use a comparison function to provide key ordering. Therefore, the `Compare` template argument found in `std::set` and `std::map` is replaced by the `ExtractKey` argument. This type is a functor that must return a key type on which ordering is built.
Example:

```cpp

struct Person
{
	int id;
	std::string first_name;
	std::string last_name;
	//...
};

// Create a radix set of Person sorted by id

struct ExtractKey
{
	int operator()(const Person & p) const 
	{
		return p.id;
	}
};


seq::radix_set<Person, ExtractKey> set;
set.insert(Person{0,"Antoine","Dupont"});
set.insert(Person{1,"Stephane","Martin"});

// value contains {1,"Stephane","Martin"}
auto value = *set.find(1);

```

`seq::radix_set` and `seq::radix_map` support keys of type `std::tuple<...>`. It is therefore possible to combine multiple elements of a key to define keys ordering. Note that this only works for fixed size types (and therefore not string types).
Example:

```cpp

struct Point
{
	int x;
	int y;
};

// Create a radix set of Point sorted by x and y (if x are equals)

struct ExtractKey
{
	std::tuple<int,int> operator()(const Point& p) const
	{
		// By returning a tupe(x,y), the Point values will be sorted by x first, then y
		return std::make_tuple(p.x, p.y);
	}
};


seq::radix_set<Point, ExtractKey> set;
set.insert(Point{ 0,1 });
set.insert(Point{ 2,3 });

auto value = *set.find(Point{ 2,3 });

```

`seq::radix_hash_set/map` provide the same interface as `std::unordered_set/map`, but invalidates all references and iterators on insert/erase operations, and only provides Basic exception guarantees.
Bucket and node related functions are not present. Functions related to the load factor are defined but do nothing, as the VART do not use a notion of load factor.

