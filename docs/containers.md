# Containers: original STL-like containers

The *containers* module defines several container classes as alternatives to STL containers or providing features not present in the STL.
These containers generally adhere to the properties of STL containers, though there are often some associated API differences and/or implementation details which differ from the standard library.

The *seq* containers are not necessarly drop-in replacement for their STL counterparts as they usually provide different iterator/reference statibility rules or different exception guarantees.

Currently, the *containers* module provide 5 types of containers:
-	Sequential random-access containers: 
	-	*seq::devector* 
	-	*seq::tiered_vector*
-	Sequential stable non random-access container: *seq::sequence*,
-	Sorted containers: *seq::flat_set*, *seq::flat_map*, *seq::flat_multiset* and *seq::flat_multimap*,
-	Ordered robin-hood hash tables: *seq::ordered_set* and *seq::ordered_map*.
-	Tiny string and string view: *seq::tiny_string* and *seq::tstring_view*.

See the documentation of each class for more details.