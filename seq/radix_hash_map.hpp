/**
 * MIT License
 *
 * Copyright (c) 2022 Victor Moncada <vtr.moncada@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef SEQ_RADIX_HASH_MAP_HPP
#define SEQ_RADIX_HASH_MAP_HPP



 /** @file */

 /** \addtogroup containers
  *  @{
  */


#include "internal/radix_tree.hpp"

namespace seq
{
	/// @brief Radix based sorted container using Variable Arity Radix Tree (VART). Same interface as std::unordered_set.
	/// @tparam Key key type
	/// @tparam Hash hash function
	/// @tparam KeyEqual equality comparison functor
	/// @tparam Allocator allocator
	/// @tparam KeyLess optional less than comparison
	template<
		class Key,
		class Hash = hasher<Key>,
		class KeyEqual = equal_to<>,
		class Allocator = std::allocator<Key>,
		class KeyLess = default_less
	>
		class radix_hash_set
	{
		struct Extract
		{
			template<class T>
			const T& operator()(const T& value) const noexcept { return value; }
		};
		template <typename U>
		using has_is_transparent = detail::has_is_transparent<U>;
		using radix_tree_type = radix_detail::RadixTree<Key, radix_detail::Hasher<Hash, KeyEqual, KeyLess>, Extract, Allocator, radix_detail::LeafNode<Key, false>, 2 >;
		radix_tree_type d_tree;
	public:

		static_assert(std::is_empty<KeyEqual>::value, "radix_hash_set only supports empty KeyEqual functor");

		struct const_iterator
		{
			using iter_type = typename radix_tree_type::const_iterator;
			using value_type = Key;
			using iterator_category = std::bidirectional_iterator_tag;
			using size_type = size_t;
			using difference_type = std::ptrdiff_t;
			using pointer = const value_type*;
			using reference = const value_type&;
			iter_type iter;

			const_iterator() {}
			const_iterator(iter_type it) : iter(it) {}
			SEQ_ALWAYS_INLINE auto operator++() noexcept -> const_iterator& {
				++iter;
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator++(int) noexcept -> const_iterator {
				const_iterator _Tmp = *this;
				++(*this);
				return _Tmp;
			}
			SEQ_ALWAYS_INLINE auto operator--() noexcept -> const_iterator& {
				--iter;
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator--(int) noexcept -> const_iterator {
				const_iterator _Tmp = *this;
				--(*this);
				return _Tmp;
			}
			SEQ_ALWAYS_INLINE auto operator*() const noexcept -> reference { return *iter; }
			SEQ_ALWAYS_INLINE auto operator->() const noexcept -> pointer { return  std::pointer_traits<pointer>::pointer_to(**this); }
			SEQ_ALWAYS_INLINE bool operator==(const const_iterator& it) const noexcept { return iter == it.iter; }
			SEQ_ALWAYS_INLINE bool operator!=(const const_iterator& it) const noexcept { return iter != it.iter; }

		};

		using iterator = const_iterator;
		using key_type = Key;
		using value_type = Key;
		using allocator_type = Allocator;
		using size_type = size_t;
		using difference_type = std::ptrdiff_t;
		using hasher = Hash;
		using key_equal = KeyEqual;
		using key_less = KeyLess;
		using reference = value_type&;
		using const_reference = const value_type&;
		using pointer = typename std::allocator_traits<Allocator>::pointer;
		using const_pointer = typename std::allocator_traits<Allocator>::const_pointer;

		
		/// @brief Constructs empty container.
		/// @param hash hash function to use
		/// @param equal comparison function to use for all key comparisons of this container
		/// @param alloc allocator to use for all memory allocations of this container
		radix_hash_set(const Hash& hash =Hash() ,
			const KeyEqual&  = KeyEqual(),
			const Allocator& alloc = Allocator()) 
			:d_tree(hash, alloc)
		{}
		radix_hash_set(const Hash& hash,
			const Allocator& alloc) 
			:d_tree(hash, alloc)
		{}
		/// @brief Constructs empty container.
		/// @param alloc allocator to use for all memory allocations of this container
		explicit radix_hash_set(const Allocator& alloc)
			:d_tree(alloc)
		{}
		/// @brief constructs the container with the contents of the range [first, last). 
		/// If multiple elements in the range have keys that compare equivalent, only the first occurence is inserted.
		/// @tparam InputIt iterator type
		/// @param first the range to copy the elements from
		/// @param last the range to copy the elements from
		/// @param hash hash function to use
		/// @param equal comparison function to use for all key comparisons of this container
		/// @param alloc allocator to use for all memory allocations of this container
		template< class InputIt >
		radix_hash_set(InputIt first, InputIt last,
			const Hash& hash = Hash(),
			const key_equal&  = key_equal(),
			const Allocator& alloc = Allocator())
			: d_tree(hash, alloc)
		{
			insert(first, last);
		}
		/// @brief constructs the container with the contents of the range [first, last). 
		/// If multiple elements in the range have keys that compare equivalent, only the first occurence is inserted.
		/// @tparam InputIt iterator type
		/// @param first the range to copy the elements from
		/// @param last the range to copy the elements from
		/// @param alloc allocator to use for all memory allocations of this container
		template< class InputIt >
		radix_hash_set(InputIt first, InputIt last,
			const Allocator& alloc)
			: d_tree(first, last, alloc)
		{}
		/// @brief constructs the container with the contents of the range [first, last). 
		/// If multiple elements in the range have keys that compare equivalent, only the first occurence is inserted.
		/// @tparam InputIt iterator type
		/// @param first the range to copy the elements from
		/// @param last the range to copy the elements from
		/// @param hash hash function to use
		/// @param alloc allocator to use for all memory allocations of this container
		template< class InputIt >
		radix_hash_set(InputIt first, InputIt last,
			const Hash& hash,
			const Allocator& alloc)
			: d_tree(hash, alloc)
		{
			d_tree.insert(first, last);
		}
		/// @brief Copy constructor
		/// @param other another container to be used as source to initialize the elements of the container with
		/// @param alloc allocator to use for all memory allocations of this container
		radix_hash_set(const radix_hash_set& other, const Allocator& alloc)
			:d_tree(other.d_tree, alloc)
		{}
		/// @brief Copy constructor
		/// @param other another container to be used as source to initialize the elements of the container with
		radix_hash_set(const radix_hash_set& other)
			:d_tree(other.d_tree)
		{}
		/// @brief Move constructor
		/// @param other another container to be used as source to initialize the elements of the container with
		radix_hash_set(radix_hash_set&& other) noexcept(std::is_nothrow_move_constructible<radix_tree_type>::value)
			:d_tree(std::move(other.d_tree))
		{}
		/// @brief Move constructor
		/// @param other another container to be used as source to initialize the elements of the container with
		/// @param alloc allocator to use for all memory allocations of this container
		radix_hash_set(radix_hash_set&& other, const Allocator& alloc)
			:d_tree(std::move(other.d_tree), alloc)
		{}
		radix_hash_set(std::initializer_list<value_type> init,
			const Hash& hash = Hash(),
			const key_equal&  = key_equal(),
			const Allocator& alloc = Allocator())
			:d_tree(hash, alloc)
		{
			insert(init.begin(), init.end());
		}
		/// @brief constructs the container with the contents of the initializer list init, same as radix_hash_set(init.begin(), init.end())
		/// @param init initializer list to initialize the elements of the container with
		/// @param hash hash function to use
		/// @param alloc allocator to use for all memory allocations of this container
		radix_hash_set(std::initializer_list<value_type> init,
			const Hash& hash,
			const Allocator& alloc)
			:d_tree(hash, alloc)
		{
			insert(init.begin(), init.end());
		}
		/// @brief constructs the container with the contents of the initializer list init, same as radix_hash_set(init.begin(), init.end())
		/// @param init initializer list to initialize the elements of the container with
		/// @param alloc allocator to use for all memory allocations of this container
		radix_hash_set(std::initializer_list<value_type> init,
			const Allocator& alloc)
			:d_tree(init.begin(), init.end(), alloc)
		{}


		/// @brief Copy assignment operator
		auto operator=(const radix_hash_set& other) -> radix_hash_set&
		{
			d_tree = other.d_tree;
			return *this;
		}

		/// @brief Move assignment operator
		auto operator=(radix_hash_set&& other) noexcept(std::is_nothrow_move_assignable<radix_tree_type>::value) -> radix_hash_set&
		{
			d_tree = std::move(other.d_tree);
			return *this;
		}

		/// @brief Returns the container size
		auto size() const noexcept -> size_t { return d_tree.size(); }
		/// @brief Returns the container maximum size
		auto max_size() const noexcept -> size_t { return d_tree.max_size(); }
		/// @brief Returns true if the container is empty, false otherwise
		auto empty() const noexcept -> bool { return d_tree.empty(); }
		/// @brief Returns the container allocator object
		auto get_allocator() noexcept -> allocator_type& { return d_tree.get_allocator(); }
		/// @brief Returns the container allocator object
		auto get_allocator() const noexcept -> const allocator_type& { return d_tree.get_allocator(); }
		/// @brief Returns the hash function
		auto hash_function() const noexcept -> const hasher& { return d_tree.hash_function(); }
		/// @brief Returns the equality comparison function
		auto key_eq() const noexcept -> key_equal { return key_equal{}; }

		/// @brief Returns an iterator to the first element of the container.
		auto end() noexcept -> iterator { return d_tree.end(); }
		/// @brief Returns an iterator to the first element of the container.
		auto end() const noexcept -> const_iterator { return d_tree.end(); }
		/// @brief Returns an iterator to the first element of the container.
		auto cend() const noexcept -> const_iterator { return d_tree.end(); }
		/// @brief Returns an iterator to the element following the last element of the container.
		auto begin() noexcept -> iterator { return d_tree.begin(); }
		/// @brief Returns an iterator to the element following the last element of the container.
		auto begin() const noexcept -> const_iterator { return d_tree.begin(); }
		/// @brief Returns an iterator to the element following the last element of the container.
		auto cbegin() const noexcept -> const_iterator { return d_tree.begin(); }

		/// @brief Clear the container
		void clear() { d_tree.clear(); }

		/// @brief Does nothing, just provided for STL compatibility purpose.
		auto load_factor() const noexcept -> float { return 1.f; }
		/// @brief Does nothing, just provided for STL compatibility purpose.
		auto max_load_factor() const noexcept -> float { return 1.f; }
		/// @brief Does nothing, just provided for STL compatibility purpose.
		void max_load_factor(float) noexcept { }

		/// @brief Rehash the container. Does nothing, just provided for STL compatibility purpose.
		void rehash(size_t) { }

		/// @brief Sets the number of nodes to the number needed to accomodate at least count elements.
		/// @param count new capacity of the container
		void reserve(size_t count) { d_tree.reserve(count); }

		/// @brief Swap this container with other
		void swap(radix_hash_set& other)
		{
			d_tree.swap(other.d_tree);
		}

		/// @brief Inserts a new element into the container constructed in-place with the given args if there is no element with the key in the container.
		/// Careful use of emplace allows the new element to be constructed while avoiding unnecessary copy or move operations. 
		/// The constructor of the new element is called with exactly the same arguments as supplied to emplace, forwarded via std::forward<Args>(args).... 
		/// The element may be constructed even if there already is an element with the key in the container, in which case the newly constructed element will be destroyed immediately.
		/// 
		/// @return pair consisting of an iterator to the inserted element, or the already-existing element if no insertion happened, 
		/// and a bool denoting whether the insertion took place (true if insertion happened, false if it did not).
		template< class... Args >
		SEQ_ALWAYS_INLINE auto emplace(Args&&... args) -> std::pair<iterator, bool>
		{
			return d_tree.emplace(std::forward<Args>(args)...);
		}
		/// @brief Inserts a new element into the container constructed in-place with the given args if there is no element with the key in the container.
		/// Same as radix_hash_set::emplace().
		template <class... Args>
		SEQ_ALWAYS_INLINE auto emplace_hint(const_iterator hint, Args&&... args) -> iterator
		{
			(void)hint;
			return d_tree.emplace( std::forward<Args>(args)...).first;
		}
		/// @brief Inserts element into the container, if the container doesn't already contain an element with an equivalent key.
		/// @param value value to be inserted
		/// @return pair consisting of an iterator to the inserted element, or the already-existing element if no insertion happened, 
		/// and a bool denoting whether the insertion took place (true if insertion happened, false if it did not).
		SEQ_ALWAYS_INLINE auto insert(const value_type& value) -> std::pair<iterator, bool>
		{
			return d_tree.emplace(value);
		}
		/// @brief Inserts element into the container using move semantic, if the container doesn't already contain an element with an equivalent key.
		/// @param value value to be inserted
		/// @return pair consisting of an iterator to the inserted element, or the already-existing element if no insertion happened, 
		/// and a bool denoting whether the insertion took place (true if insertion happened, false if it did not).
		SEQ_ALWAYS_INLINE auto insert(value_type&& value) -> std::pair<iterator, bool>
		{
			return d_tree.emplace(std::move(value));
		}
		/// @brief Inserts element into the container, if the container doesn't already contain an element with an equivalent key.
		/// Same as radix_hash_set::insert().
		SEQ_ALWAYS_INLINE auto insert(const_iterator hint, const value_type& value) -> iterator
		{
			(void)hint;
			return d_tree.emplace( value).first;
		}
		/// @brief Inserts element into the container using move semantic, if the container doesn't already contain an element with an equivalent key.
		/// Same as radix_hash_set::insert().
		SEQ_ALWAYS_INLINE auto insert(const_iterator hint, value_type&& value) -> iterator
		{
			(void)hint;
			return d_tree.emplace( std::move(value)).first;
		}
		/// @brief Inserts elements from range [first, last). If multiple elements in the range have keys that compare equivalent, only the first occurence is inserted.
		/// Elements are inserted in any order.
		/// @param first range of elements to insert
		/// @param last range of elements to insert
		template< class InputIt >
		void insert(InputIt first, InputIt last)
		{
			d_tree.insert(first, last);
		}
		/// @brief Inserts elements from range [init.begin(), init.end()). If multiple elements in the range have keys that compare equivalent, only the first occurence is inserted.
		/// Elements are inserted in any order.
		/// @param first range of elements to insert
		/// @param last range of elements to insert
		void insert(std::initializer_list<value_type> ilist)
		{
			insert(ilist.begin(), ilist.end());
		}

		/// @brief Erase element at given location.
		/// Iterators and references are not invalidated. Rehashing never occurs.
		/// @param pos iterator to the element to erase
		/// @return iterator to the next element
		SEQ_ALWAYS_INLINE auto erase(const_iterator pos) -> iterator
		{
			return d_tree.erase(pos.iter);
		}
		/// @brief Erase element comparing equal to given key (if any).
		/// Iterators and references are not invalidated. Rehashing never occurs.
		/// @param key key to be erased
		/// @return number of erased elements (0 or 1)
		SEQ_ALWAYS_INLINE auto erase(const Key& key) -> size_type
		{
			return d_tree.erase(key);
		}
		/// @brief Erase element comparing equal to given key (if any).
		/// Removes the element (if one exists) with key that compares equivalent to the value x. 
		/// This overload participates in overload resolution only if Hash::is_transparent and KeyEqual::is_transparent are valid and each denotes a type, 
		/// and neither iterator nor const_iterator is implicitly convertible from K. 
		/// This assumes that such Hash is callable with both K and Key type, and that the KeyEqual is transparent, which, together, allows calling this function without constructing an instance of Key.
		/// @return number of erased elements (0 or 1)
		template <class K, class KE = KeyEqual, class H = Hash,
			typename std::enable_if<has_is_transparent<KE>::value&& has_is_transparent<H>::value>::type* = nullptr>
		SEQ_ALWAYS_INLINE auto erase(const K& x) -> size_type
		{
			return d_tree.erase(x);
		}
		/// @brief Removes the elements in the range [first; last), which must be a valid range in *this.
		/// @param first range of elements to remove
		/// @param last range of elements to remove
		/// @return Iterator following the last removed element
		auto erase(const_iterator first, const_iterator last) -> iterator
		{
			return d_tree.erase(first.iter, last.iter);
		}


		/// @brief Finds an element with key equivalent to key
		/// @param value key value to search for
		/// @return iterator pointing to found key on success, end iterator on failure.
		SEQ_ALWAYS_INLINE auto find(const Key& key) const -> const_iterator
		{
			return d_tree.find(key);
		}
		/// @brief Finds an element with key equivalent to key
		/// @param value key value to search for
		/// @return iterator pointing to found key on success, end iterator on failure.
		SEQ_ALWAYS_INLINE auto find(const Key& value) -> iterator
		{
			return d_tree.find(value);
		}
		/// @brief Finds an element with key equivalent to key.
		/// Finds an element with key that compares equivalent to the value x. 
		/// This overload participates in overload resolution only if Hash::is_transparent and KeyEqual::is_transparent are valid and each denotes a type. 
		/// This assumes that such Hash is callable with both K and Key type, and that the KeyEqual is transparent, which, together, allows calling this function without constructing an instance of Key.
		/// @param value key value to search for
		/// @return iterator pointing to found key on success, end iterator on failure.
		template <class K, class KE = KeyEqual, class H = Hash,
			typename std::enable_if<has_is_transparent<KE>::value&& has_is_transparent<H>::value>::type* = nullptr>
		SEQ_ALWAYS_INLINE auto find(const K& x) const -> const_iterator
		{
			return d_tree.find(x);
		}
		/// @brief Finds an element with key equivalent to key.
		/// Finds an element with key that compares equivalent to the value x. 
		/// This overload participates in overload resolution only if Hash::is_transparent and KeyEqual::is_transparent are valid and each denotes a type. 
		/// This assumes that such Hash is callable with both K and Key type, and that the KeyEqual is transparent, which, together, allows calling this function without constructing an instance of Key.
		/// @param value key value to search for
		/// @return iterator pointing to found key on success, end iterator on failure.
		template <class K, class KE = KeyEqual, class H = Hash,
			typename std::enable_if<has_is_transparent<KE>::value&& has_is_transparent<H>::value>::type* = nullptr>
		SEQ_ALWAYS_INLINE auto find(const K& key) -> iterator
		{
			return d_tree.find(key);
		}

		/// @brief Returns 1 of key exists, 0 otherwise
		SEQ_ALWAYS_INLINE auto count(const Key& key) const -> size_type
		{
			return find(key) != end();
		}
		/// @brief Returns 1 of key exists, 0 otherwise
		template <class K, class KE = KeyEqual, class H = Hash,
			typename std::enable_if<has_is_transparent<KE>::value&& has_is_transparent<H>::value>::type* = nullptr>
		SEQ_ALWAYS_INLINE auto count(const K& key) const -> size_type
		{
			return find(key) != end();
		}

		/// @brief Returns true of key exists, false otherwise
		SEQ_ALWAYS_INLINE bool contains(const Key& key) const
		{
			return find(key) != end();
		}
		/// @brief Returns true of key exists, false otherwise
		template <class K, class KE = KeyEqual, class H = Hash,
			typename std::enable_if<has_is_transparent<KE>::value&& has_is_transparent<H>::value>::type* = nullptr>
		SEQ_ALWAYS_INLINE bool contains(const K& key) const
		{
			return find(key) != end();
		}

	};


	/// @brief Compare two radix_hash_set for equality.
	/// Two ordered_set are considered equal if they contain the same keys. Key ordering is not considered.
	/// @param lhs left ordered_set
	/// @param rhs right ordered_set
	/// @return true if both containers compare equals, false otherwise
	template<
		class Key,
		class Hash1,
		class Hash2,
		class KeyEqual,
		class Allocator1,
		class Allocator2,
		class Less1,
		class Less2
	>
		auto operator == (const radix_hash_set<Key, Hash1, KeyEqual, Allocator1, Less1>& lhs, const radix_hash_set<Key, Hash2, KeyEqual, Allocator2, Less2>& rhs) -> bool
	{
		if (lhs.size() != rhs.size())
			return false;
		for (auto it = lhs.begin(); it != lhs.end(); ++it) {
			if (rhs.find(*it) == rhs.end())
				return false;
		}
		return true;
	}
	/// @brief Compare two radix_hash_set for inequality, synthesized from operator==.
	/// @return true if both containers compare non equals, false otherwise
	template<
		class Key,
		class Hash1,
		class Hash2,
		class KeyEqual,
		class Allocator1,
		class Allocator2,
		class Less1,
		class Less2
	>
		auto operator != (const radix_hash_set<Key, Hash1, KeyEqual, Allocator1, Less1>& lhs, const radix_hash_set<Key, Hash2, KeyEqual, Allocator2, Less2>& rhs) -> bool
	{
		return !(lhs == rhs);
	}
	/// @brief Erases all elements that satisfy the predicate p from the container.
	/// @param set container from which to erase
	/// @param p predicate that returns true if the element should be erased
	/// @return number of erased elements
	template<
		class Key,
		class Hash1,
		class KeyEqual,
		class Allocator1,
		class Less,
		class Pred
	>
		auto erase_if(radix_hash_set<Key, Hash1, KeyEqual, Allocator1, Less>& s, Pred p) -> size_t
	{
		typename radix_hash_set<Key, Hash1, KeyEqual, Allocator1, Less>::size_type count = 0;
		for (auto it = s.begin(); it != s.end(); ) {
			if (p(*it)) {
				++count;
				it = s.erase(it);
			}
			else
				++it;
		}
		return count;
	}








	/// @brief Radix based sorted container using Variable Arity Radix Tree (VART). Same interface as std::unordered_map.
	/// @tparam Key key type
	/// @tparam T mapped type
	/// @tparam Hash hash function
	/// @tparam KeyEqual equality comparison functor
	/// @tparam Allocator allocator
	/// @tparam KeyLess optional less than comparison
	template<
		class Key,
		class T,
		class Hash = hasher<Key>,
		class KeyEqual = equal_to<>,
		class Allocator = std::allocator< std::pair<Key, T> >,
		class KeyLess = default_less
	>
		class radix_hash_map 
	{
		struct Extract
		{
			template<class U>
			const U& operator()(const U & value) const noexcept { return value; }
			const Key& operator()(const std::pair<Key, T>& value) const noexcept { return value.first; }
			const Key& operator()(const std::pair<const Key, T>& value) const noexcept { return value.first; }
			template<class U, class V>
			const U& operator()(const std::pair<U, V>& p) const {
				return (p.first);
			}
		};
		template <typename U>
		using has_is_transparent = detail::has_is_transparent<U>;
		using radix_tree_type = radix_detail::RadixTree<std::pair<Key, T>, radix_detail::Hasher<Hash, KeyEqual, KeyLess>, Extract, Allocator, radix_detail::LeafNode<std::pair<Key, T>, false>, 2 >;
		radix_tree_type d_tree;

	public:

		static_assert(std::is_empty<KeyEqual>::value, "radix_hash_map only supports empty KeyEqual functor");

		struct const_iterator
		{
			using iter_type = typename radix_tree_type::const_iterator;
			iter_type iter;

		public:
			using value_type = std::pair<Key, T>;
			using iterator_category = std::bidirectional_iterator_tag;
			using size_type = size_t;
			using difference_type = std::ptrdiff_t;
			using pointer = const value_type*;
			using reference = const value_type&;
			using const_pointer = const value_type*;
			using const_reference = const value_type&;
			

			const_iterator() {}
			const_iterator(const iter_type& it) : iter(it) {}
			SEQ_ALWAYS_INLINE auto operator++() noexcept -> const_iterator& {
				++iter;
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator++(int) noexcept -> const_iterator {
				const_iterator _Tmp = *this;
				++(*this);
				return _Tmp;
			}
			SEQ_ALWAYS_INLINE auto operator--() noexcept -> const_iterator& {
				--iter;
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator--(int) noexcept -> const_iterator {
				const_iterator _Tmp = *this;
				--(*this);
				return _Tmp;
			}
			SEQ_ALWAYS_INLINE auto operator*() const noexcept -> reference { return reinterpret_cast<const value_type&>(*iter); }
			SEQ_ALWAYS_INLINE auto operator->() const noexcept -> pointer { return  std::pointer_traits<pointer>::pointer_to(**this); }
			
			SEQ_ALWAYS_INLINE bool operator==(const const_iterator& it) const noexcept { return iter == it.iter; }
			SEQ_ALWAYS_INLINE bool operator!=(const const_iterator& it) const noexcept { return iter != it.iter; }
		};
		struct iterator : public const_iterator
		{
			using iter_type = typename radix_tree_type::const_iterator;
			using value_type = std::pair<Key, T>;
			using iterator_category = std::bidirectional_iterator_tag;
			using size_type = size_t;
			using difference_type = std::ptrdiff_t;
			using pointer = value_type*;
			using reference = value_type&;
			using const_pointer = const value_type*;
			using const_reference = const value_type&;

			iterator() : const_iterator() {}
			iterator(const iter_type& it) : const_iterator(it) {}
			iterator(const const_iterator& it) : const_iterator(it) {}
			SEQ_ALWAYS_INLINE auto operator++() noexcept -> iterator& {
				++this->iter;
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator++(int) noexcept -> iterator {
				iterator _Tmp = *this;
				++(*this);
				return _Tmp;
			}
			SEQ_ALWAYS_INLINE auto operator--() noexcept -> iterator& {
				--this->iter;
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator--(int) noexcept -> iterator {
				iterator _Tmp = *this;
				--(*this);
				return _Tmp;
			}
			SEQ_ALWAYS_INLINE auto operator*() noexcept -> reference { return reinterpret_cast<value_type&>(const_cast<std::pair<Key, T>&>(*this->iter)); }
			SEQ_ALWAYS_INLINE auto operator->() noexcept -> pointer { return  std::pointer_traits<pointer>::pointer_to(**this); }
			SEQ_ALWAYS_INLINE auto operator*() const noexcept -> const_reference { return reinterpret_cast<const value_type&>(*this->iter); }
			SEQ_ALWAYS_INLINE auto operator->() const noexcept -> const_pointer { return  std::pointer_traits<const_pointer>::pointer_to(**this); }

			SEQ_ALWAYS_INLINE bool operator==(const const_iterator& it) const noexcept { return this->iter == it.iter; }
			SEQ_ALWAYS_INLINE bool operator!=(const const_iterator& it) const noexcept { return this->iter != it.iter; }
		};

		using key_type = Key;
		using mapped_type = T;
		using value_type = std::pair<Key, T>;
		using allocator_type = Allocator;
		using size_type = size_t;
		using difference_type = std::ptrdiff_t;
		using hasher = Hash;
		using key_equal = KeyEqual;
		using value_type_extract = Extract;
		using reference = value_type&;
		using const_reference = const value_type&;
		using pointer = typename std::allocator_traits<Allocator>::pointer;
		using const_pointer = typename std::allocator_traits<Allocator>::const_pointer;

		radix_hash_map(const Hash& hash = Hash(),
			const KeyEqual&  = KeyEqual(),
			const Allocator& alloc = Allocator()) 
			:d_tree(hash,  alloc)
		{}

		explicit radix_hash_map(const Allocator& alloc)
			:d_tree( alloc)
		{}

		template< class InputIt >
		radix_hash_map(InputIt first, InputIt last,
			const Hash& hash = Hash(),
			const key_equal&  = key_equal(),
			const Allocator& alloc = Allocator())
			: d_tree(hash, alloc)
		{
			insert(first, last);
		}
		template< class InputIt >
		radix_hash_map(InputIt first, InputIt last,
			const Allocator& alloc)
			: d_tree(alloc)
		{
			insert(first, last);
		}
		template< class InputIt >
		radix_hash_map(InputIt first, InputIt last,
			const Hash& hash,
			const Allocator& alloc)
			: radix_hash_map(first, last, hash, key_equal(), alloc)
		{}
		radix_hash_map(const radix_hash_map& other, const Allocator& alloc)
			:d_tree(other.d_tree, alloc)
		{}
		radix_hash_map(const radix_hash_map& other)
			:d_tree(other.d_tree)
		{
		}
		radix_hash_map(radix_hash_map&& other) noexcept(std::is_nothrow_move_constructible< radix_tree_type>::value)
			:d_tree(std::move(other.d_tree))
		{}
		radix_hash_map(radix_hash_map&& other, const Allocator& alloc)
			:d_tree(std::move(other.d_tree), alloc)
		{}
		radix_hash_map(std::initializer_list<value_type> init,
			const Hash& hash = Hash(),
			const key_equal&  = key_equal(),
			const Allocator& alloc = Allocator())
			:radix_hash_map(init.begin(), init.end(), hash, key_equal(), alloc)
		{}
		radix_hash_map(std::initializer_list<value_type> init,
			const Hash& hash,
			const Allocator& alloc)
			:radix_hash_map(init.begin(), init.end(), hash, alloc)
		{}
		radix_hash_map(std::initializer_list<value_type> init,
			const Allocator& alloc)
			:radix_hash_map(init.begin(), init.end(), alloc)
		{}

		auto operator=(const radix_hash_map& other) -> radix_hash_map&
		{
			d_tree = other.d_tree;
			return *this;
		}
		auto operator=(radix_hash_map&& other) noexcept(std::is_nothrow_move_assignable< radix_tree_type>::value) -> radix_hash_map&
		{
			d_tree = std::move(other.d_tree);
			return *this;
		}

		auto size() const noexcept -> size_t { return this->d_tree.size(); }
		auto max_size() const noexcept -> size_t { return this->d_tree.max_size(); }
		auto empty() const noexcept -> bool { return this->d_tree.empty(); }

		auto load_factor() const noexcept -> float { return 1.f; }
		auto max_load_factor() const noexcept -> float { return 1.f; }
		void max_load_factor(float ) noexcept {}

		auto get_allocator() noexcept -> allocator_type& { return this->d_tree.get_allocator(); }
		auto get_allocator() const noexcept -> const allocator_type& { return this->d_tree.get_allocator(); }

		auto hash_function() const -> hasher { return this->d_tree.hash_function(); }
		auto key_eq() const -> key_equal { return KeyEqual{}; }

		auto end() noexcept -> iterator { return this->d_tree.end(); }
		auto end() const noexcept -> const_iterator { return this->d_tree.end(); }
		auto cend() const noexcept -> const_iterator { return this->d_tree.end(); }

		auto begin() noexcept -> iterator { return this->d_tree.begin(); }
		auto begin() const noexcept -> const_iterator { return this->d_tree.begin(); }
		auto cbegin() const noexcept -> const_iterator { return this->d_tree.begin(); }

		void clear()
		{
			this->d_tree.clear();
		}
		void rehash()
		{}
		void reserve(size_t size)
		{
			this->d_tree.reserve(size);
		}
		
		void swap(radix_hash_map& other)
		{
			d_tree.swap(other.d_tree);
		}

		template< class... Args >
		SEQ_ALWAYS_INLINE auto emplace(Args&&... args) -> std::pair<iterator, bool>
		{
			return d_tree.emplace(std::forward<Args>(args)...);
		}
		template <class... Args>
		SEQ_ALWAYS_INLINE auto emplace_hint(const_iterator hint, Args&&... args) -> iterator
		{
			(void)hint;
			return d_tree.emplace(std::forward<Args>(args)...).first;
		}
		SEQ_ALWAYS_INLINE auto insert(const value_type& value) -> std::pair<iterator, bool>
		{
			return d_tree.emplace(value);
		}
		SEQ_ALWAYS_INLINE auto insert(value_type&& value) -> std::pair<iterator, bool>
		{
			return d_tree.emplace(std::move(value));
		}
		template< class P, typename std::enable_if<std::is_constructible<value_type, P>::value, int>::type = 0 >
		SEQ_ALWAYS_INLINE auto insert(P&& value) -> std::pair<iterator, bool>
		{
			return d_tree.emplace(std::forward<P>(value));
		}
		


		SEQ_ALWAYS_INLINE auto insert(const_iterator hint, const value_type& value) -> iterator
		{
			(void)hint;
			return d_tree.emplace(value).first;
		}
		SEQ_ALWAYS_INLINE 	auto insert(const_iterator hint, value_type&& value) -> iterator
		{
			(void)hint;
			return d_tree.emplace(std::move( value)).first;
		}
		template< class P, typename std::enable_if<std::is_constructible<value_type, P>::value, int>::type = 0 >
		SEQ_ALWAYS_INLINE auto insert(const_iterator hint, P&& value) -> iterator
		{
			(void)hint;
			return d_tree.emplace( std::forward<P>(value)).first;
		}
		template< class InputIt >
		void insert(InputIt first, InputIt last)
		{
			d_tree.insert(first, last);
		}
		void insert(std::initializer_list<value_type> ilist)
		{
			insert(ilist.begin(), ilist.end());
		}




		template <class M>
		auto insert_or_assign(const Key& k, M&& obj) -> std::pair<iterator, bool>
		{
			auto inserted = try_emplace(k, std::forward<M>(obj));
			if (!inserted.second)
				inserted.first->second = std::forward<M>(obj);
			return inserted;
		}
		template <class M>
		auto insert_or_assign(Key&& k, M&& obj) -> std::pair<iterator, bool>
		{
			auto inserted = try_emplace(std::move(k), std::forward<M>(obj));
			if (!inserted.second)
				inserted.first->second = std::forward<M>(obj);
			return inserted;
		}
		template <class M>
		auto insert_or_assign(const_iterator hint, const Key& k, M&& obj) -> iterator
		{
			(void)hint;
			auto inserted = try_emplace(k, std::forward<M>(obj));
			if (!inserted.second)
				inserted.first->second = std::forward<M>(obj);
			return inserted.first;
		}
		template <class M>
		auto insert_or_assign(const_iterator hint, Key&& k, M&& obj) -> iterator
		{
			(void)hint;
			auto inserted = try_emplace(std::move(k), std::forward<M>(obj));
			if (!inserted.second)
				inserted.first->second = std::forward<M>(obj);
			return inserted.first;
		}

		template< class... Args >
		auto try_emplace(const Key& k, Args&&... args) -> std::pair<iterator, bool>
		{
			return d_tree.try_emplace(k, std::forward<Args>(args)...);
		}
		template< class... Args >
		auto try_emplace(Key&& k, Args&&... args) -> std::pair<iterator, bool>
		{
			return d_tree.try_emplace(std::move(k), std::forward<Args>(args)...);
		}
		template< class... Args >
		auto try_emplace(const_iterator hint, const Key& k, Args&&... args) -> iterator
		{
			(void)hint;
			return d_tree.try_emplace(k, std::forward<Args>(args)...).first;
		}
		template< class... Args >
		auto try_emplace(const_iterator hint, Key&& k, Args&&... args) -> iterator
		{
			(void)hint;
			return d_tree.try_emplace(std::move(k), std::forward<Args>(args)...).first;
		}


		SEQ_ALWAYS_INLINE auto at(const Key& key) -> T&
		{
			auto it = find(key);
			if (it == end())
				throw std::out_of_range("radix_hash_map: key not found");
			return it->second;
		}
		SEQ_ALWAYS_INLINE auto at(const Key& key) const -> const T&
		{
			auto it = find(key);
			if (it == end())
				throw std::out_of_range("radix_hash_map: key not found");
			return it->second;
		}


		SEQ_ALWAYS_INLINE auto operator[](const Key& key) -> T&
		{
			return try_emplace(key).first->second;
		}
		SEQ_ALWAYS_INLINE 	auto operator[](Key&& key) -> T&
		{
			return try_emplace(std::move(key)).first->second;
		}



		SEQ_ALWAYS_INLINE auto erase(const_iterator pos) -> iterator
		{
			return d_tree.erase(pos.iter);
		}
		SEQ_ALWAYS_INLINE auto erase(const Key& key) -> size_type
		{
			return d_tree.erase(key);
		}
		template <class K, class KE = KeyEqual, class H = Hash,
			typename std::enable_if<has_is_transparent<KE>::value&& has_is_transparent<H>::value>::type* = nullptr>
			SEQ_ALWAYS_INLINE	auto erase(const K& key) -> size_type
		{
			return d_tree.erase(key);
		}
		SEQ_ALWAYS_INLINE auto erase(const_iterator first, const_iterator last) -> iterator
		{
			return d_tree.erase(first.iter, last.iter);
		}

		SEQ_ALWAYS_INLINE auto find(const Key& value) const -> const_iterator
		{
			return d_tree.find(value);
		}
		SEQ_ALWAYS_INLINE auto find(const Key& value) -> iterator
		{
			return d_tree.find(value);
		}

		template <class K, class KE = KeyEqual, class H = Hash,
			typename std::enable_if<has_is_transparent<KE>::value&& has_is_transparent<H>::value>::type* = nullptr>
			SEQ_ALWAYS_INLINE auto find(const K& key) const -> const_iterator
		{
			return d_tree.find(key);
		}
		template <class K, class KE = KeyEqual, class H = Hash,
			typename std::enable_if<has_is_transparent<KE>::value&& has_is_transparent<H>::value>::type* = nullptr>
			SEQ_ALWAYS_INLINE auto find(const K& key) -> iterator
		{
			return d_tree.find(key);
		}


		auto count(const Key& key) const -> size_type
		{
			return find(key) != end();
		}

		template <class K, class KE = KeyEqual, class H = Hash,
			typename std::enable_if<has_is_transparent<KE>::value&& has_is_transparent<H>::value>::type* = nullptr>
			auto count(const K& key) const -> size_type
		{
			return find(key) != end();
		}

		bool contains(const Key& key) const
		{
			return find(key) != end();
		}

		template <class K, class KE = KeyEqual, class H = Hash,
			typename std::enable_if<has_is_transparent<KE>::value&& has_is_transparent<H>::value>::type* = nullptr>
			bool contains(const K& key) const
		{
			return find(key) != end();
		}
	};




	/// @brief Checks if the contents of s1 and s2 are equal, that is, they have the same number of elements and each element in s1 compares equal with the element in s2 at the same position.
	template<class Key, class T, class Hash, class Equal, class L1, class L2, class Al1, class Al2  >
	auto operator==(const radix_hash_map<Key, T, Hash, Equal, Al1, L1>& s1, const radix_hash_map<Key, T, Hash, Equal, Al2, L2>& s2) -> bool
	{
		using value_type_extract = typename radix_hash_map<Key, T, Hash, Equal, Al1, L1>::value_type_extract;
		if (s1.size() != s2.size())
			return false;
		auto it1 = s1.begin();
		for (auto it2 = s2.begin(); it2 != s2.end(); ++it2, ++it1) {
			if (value_type_extract{}(*it1) != value_type_extract{}(*it2) || it1->second != it2->second)
				return false;
		}
		return true;
	}
	/// @brief Checks if the contents of s1 and s2 are not equals.
	template<class Key, class T, class Hash, class Equal, class L1, class L2, class Al1, class Al2  >
	auto operator!=(const radix_hash_map<Key, T, Hash, Equal, Al1, L1>& s1, const radix_hash_map<Key, T, Hash, Equal, Al2, L2>& s2) -> bool
	{
		return !(s1 == s2);
	}
	/// @brief Erases all elements that satisfy the predicate p from the container.
	template<class Key, class T, class Hash, class Equal, class Al, class Less, class Pred >
	auto erase_if(radix_hash_map<Key, T, Hash, Equal, Al, Less>& s, Pred p) -> typename radix_hash_map<Key, T, Hash, Equal, Al, Less>::size_type
	{
		typename radix_hash_map<Key, T, Hash, Equal, Al, Less>::size_type count = 0;
		for (auto it = s.begin(); it != s.end(); ) {
			if (p(*it)) {
				++count;
				it = s.erase(it);
			}
			else
				++it;
		}
		return count;
	}

}// end namespace seq

#endif
