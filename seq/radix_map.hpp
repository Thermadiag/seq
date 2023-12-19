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

#ifndef SEQ_RADIX_MAP_HPP
#define SEQ_RADIX_MAP_HPP



/** @file */

/** \addtogroup containers
 *  @{
 */


#include "internal/radix_tree.hpp"
#include "internal/radix_extra.hpp"

namespace seq
{
	

	/// @brief Radix based sorted container using Variable Arity Radix Tree (VART). Same interface as std::set.
	/// @tparam Key key type
	/// @tparam ExtractKey Functor extracting a suitable key for the radix tree
	/// @tparam Allocator allocator
	template<class Key, class ExtractKey = default_key<Key>, class Allocator = std::allocator<Key> >
	class radix_set
	{
		using radix_key_type = typename radix_detail::ExtractKeyResultType< ExtractKey, Key>::type;
		using radix_tree_type = radix_detail::RadixTree<Key, radix_detail::SortedHasher< radix_key_type>, ExtractKey, Allocator, radix_detail::LeafNode<Key> >;
		radix_tree_type d_tree;

	public:

		/// @brief const iterator class
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
			const_iterator(const iter_type &it) : iter(it) {}
			const_iterator(iter_type && it) : iter(std::move(it)) {}
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
			SEQ_ALWAYS_INLINE auto operator*() const noexcept -> reference {return *iter;}
			SEQ_ALWAYS_INLINE auto operator->() const noexcept -> pointer {return  std::pointer_traits<pointer>::pointer_to(**this);}
			
			SEQ_ALWAYS_INLINE auto operator==(const const_iterator& it) const noexcept -> bool { return iter == it.iter; }
			SEQ_ALWAYS_INLINE auto operator!=(const const_iterator& it) const noexcept -> bool { return iter != it.iter; }
		};
		using iterator = const_iterator;

		/// @brief const iterator class for prefix search
		struct const_prefix_iterator
		{
			using iter_type = typename radix_tree_type::const_prefix_iterator;
			using value_type = Key;
			using iterator_category = std::forward_iterator_tag;
			using size_type = size_t;
			using difference_type = std::ptrdiff_t;
			using pointer = const value_type*;
			using reference = const value_type&;
			iter_type iter;

			const_prefix_iterator() {}
			const_prefix_iterator(const iter_type& it) : iter(it) {}
			SEQ_ALWAYS_INLINE auto operator++() noexcept -> const_prefix_iterator& {
				++iter;
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator++(int) noexcept -> const_prefix_iterator {
				const_prefix_iterator _Tmp = *this;
				++(*this);
				return _Tmp;
			}
			SEQ_ALWAYS_INLINE auto operator*() const noexcept -> reference { return *iter; }
			SEQ_ALWAYS_INLINE auto operator->() const noexcept -> pointer { return  std::pointer_traits<pointer>::pointer_to(**this); }

			SEQ_ALWAYS_INLINE auto operator==(const const_prefix_iterator& it) const noexcept -> bool { return iter == it.iter; }
			SEQ_ALWAYS_INLINE auto operator!=(const const_prefix_iterator& it) const noexcept -> bool { return iter != it.iter; }
		};
		using prefix_iterator = const_prefix_iterator;

		using key_type = Key;
		using value_type = Key;
		using difference_type = typename std::allocator_traits<Allocator>::difference_type;
		using size_type = typename std::allocator_traits<Allocator>::size_type;
		using key_extract = ExtractKey;
		using allocator_type = Allocator;
		using reference = Key&;
		using const_reference = const Key&;
		using pointer = typename std::allocator_traits<Allocator>::pointer;
		using const_pointer = typename std::allocator_traits<Allocator>::const_pointer;
		using reverse_iterator = std::reverse_iterator<iterator>;
		using const_reverse_iterator = std::reverse_iterator<const_iterator>;

		/// @brief Default constructor
		radix_set():d_tree(){}
		/// @brief Construct from allocator object
		/// @param alloc allocator to use for all memory allocations of this container
		explicit radix_set(const Allocator& alloc)
			:d_tree(alloc) {}
		/// @brief Range constructor. Constructs the container with the contents of the range [first, last). 
		/// If multiple elements in the range have keys that compare equivalent, it is unspecified which element is inserted,
		/// except if template parameter Stable is true.
		/// @param first the range to copy the elements from
		/// @param last the range to copy the elements from
		/// @param alloc allocator to use for all memory allocations of this container
		template< class InputIt >
		radix_set(InputIt first, InputIt last, const Allocator& alloc = Allocator())
			: d_tree(alloc) 
		{
			d_tree.insert(first, last);
		}
		/// @brief Copy constructor
		/// @param other another container to be used as source to initialize the elements of the container with
		radix_set(const radix_set& other)
			:d_tree(other.d_tree) {}
		/// @brief Copy constructor
		/// @param other another container to be used as source to initialize the elements of the container with
		/// @param alloc allocator to use for all memory allocations of this container
		radix_set(const radix_set& other, const Allocator& alloc)
			:d_tree(other.d_tree, alloc) {}
		/// @brief Move constructor. Constructs the container with the contents of other using move semantics. 
		/// If alloc is not provided, allocator is obtained by move-construction from the allocator belonging to other.
		/// @param other another container to be used as source to initialize the elements of the container with
		radix_set(radix_set&& other) noexcept(std::is_nothrow_move_constructible< radix_tree_type>::value)
			:d_tree(std::move(other.d_tree)) {}
		/// @brief Move constructor. Constructs the container with the contents of other using move semantics. 
		/// If alloc is not provided, allocator is obtained by move-construction from the allocator belonging to other.
		/// @param other another container to be used as source to initialize the elements of the container with
		/// @param alloc allocator to use for all memory allocations of this container
		radix_set(radix_set&& other, const Allocator& alloc) 
			:d_tree(std::move(other.d_tree), alloc) {}
		/// @brief  Initializer-list constructor. Constructs the container with the contents of the initializer list init. 
		/// If multiple elements in the range have keys that compare equivalent, it is unspecified which element is inserted,
		/// except if template parameter Stable is true.
		/// @param init initializer list to initialize the elements of the container with
		/// @param comp comparison function object to use for all comparisons of keys
		/// @param alloc allocator to use for all memory allocations of this container
		radix_set(std::initializer_list<value_type> init,
			const Allocator& alloc = Allocator())
			:radix_set(init.begin(), init.end(), alloc) {}

		/// @brief Move assignment operator
		/// @param other another container to be used as source to initialize the elements of the container with
		/// @return reference to this container
		auto operator=(radix_set&& other) noexcept(std::is_nothrow_move_assignable< radix_tree_type>::value) -> radix_set& {
			d_tree = std::move(other.d_tree);
			return *this;
		}
		/// @brief Copy operator
		/// @param other another container to be used as source to initialize the elements of the container with
		/// @return reference to this container
		auto operator=(const radix_set& other) -> radix_set& {
			d_tree = (other.d_tree);
			return *this;
		}
		/// @brief Assign initializer-list
		/// @param init initializer list to initialize the elements of the container with
		/// @return reference to this container
		auto operator=(const std::initializer_list<value_type>& init) -> radix_set&
		{
			d_tree = init;
			return *this;
		}

		/// @brief Returns container's allocator
		SEQ_ALWAYS_INLINE auto get_allocator() const noexcept -> const Allocator & { return d_tree.get_allocator(); }
		SEQ_ALWAYS_INLINE auto get_allocator() noexcept -> Allocator& { return d_tree.get_allocator(); }

		/// @brief Returns true if container is empty, false otherwise
		SEQ_ALWAYS_INLINE auto empty() const noexcept -> bool { return d_tree.empty(); }
		/// @brief Returns the container size
		SEQ_ALWAYS_INLINE auto size() const noexcept -> size_type { return d_tree.size(); }
		/// @brief Returns the container maximum size
		SEQ_ALWAYS_INLINE auto max_size() const noexcept -> size_type { return d_tree.max_size(); }
		/// @brief Clears the container
		SEQ_ALWAYS_INLINE void clear() noexcept { d_tree.clear(); }
		/// @brief Swap this container's content with another. Iterators to both containers remain valid, including end iterators.
		SEQ_ALWAYS_INLINE void swap(radix_set& other) noexcept(noexcept(std::declval< radix_tree_type&>().swap(std::declval< radix_tree_type&>())))
		{ d_tree.swap(other.d_tree); }

		void shrink_to_fit()
		{
			d_tree.shrink_to_fit();
		}

		/// @brief See std::set::insert
		SEQ_ALWAYS_INLINE auto insert(const value_type& value) -> std::pair<iterator, bool> {	return d_tree.emplace(value);}
		/// @brief See std::set::insert
		SEQ_ALWAYS_INLINE auto insert(value_type&& value) -> std::pair<iterator, bool> { return d_tree.emplace(std::move(value)); }

		/// @brief See std::set::emplace().
		template< class... Args >
		SEQ_ALWAYS_INLINE auto emplace(Args&&... args) -> std::pair<iterator, bool> { return d_tree.emplace(std::forward<Args>(args)...); }		
		/// @brief Same as std::set::emplace_hint
		template <class... Args>
		SEQ_ALWAYS_INLINE auto emplace_hint(const_iterator hint, Args&&... args) -> iterator { 
			return d_tree.emplace_hint(hint.iter,std::forward<Args>(args)...); 
		}

		/// @brief Same as std::set::insert()
		SEQ_ALWAYS_INLINE auto insert(const_iterator hint, const value_type& value) -> iterator { return d_tree.emplace_hint(hint.iter,value); }
		/// @brief Same as std::set::insert()
		SEQ_ALWAYS_INLINE auto insert(const_iterator hint, value_type&& value) -> iterator { return d_tree.emplace_hint(hint.iter, std::move(value)); }

		/// @brief Inserts elements from range [first, last). If multiple elements in the range have keys that compare equivalent, 
		/// it is unspecified which element is inserted, except if template parameter Stable is true.
		/// This function will insert the new elements at the end of the underlying tiered_vector, sort them, inplace merge the old and new key,
		/// and remove potential duplicates. If the number of elements in the range [first, last) is roughly equal or greater than the container's size,
		/// this function is much faster than inserting elements one by one.
		template< class InputIt >
		SEQ_ALWAYS_INLINE void insert(InputIt first, InputIt last) { return d_tree.insert(first, last); }
		/// @brief Inserts elements from initializer list ilist. If multiple elements in the range have keys that compare equivalent, 
		/// it is unspecified which element is inserted, except if template parameter Stable is true.
		SEQ_ALWAYS_INLINE void insert(std::initializer_list<value_type> ilist) { return d_tree.insert(ilist.begin(), ilist.end()); }

		/// @brief Assign the content of the range [first, last) to this container, discarding previous values.
		template< class Iter >
		SEQ_ALWAYS_INLINE void assign(Iter first, Iter last) {
			d_tree.clear();
			d_tree.insert(first, last);
		}

		/// @brief Erase element at given location.
		SEQ_ALWAYS_INLINE auto erase(const_iterator pos) -> iterator{return d_tree.erase(pos.iter);}
		/// @brief Erase elements in the range [first, last)
		SEQ_ALWAYS_INLINE auto erase(const_iterator first, const_iterator last) -> iterator{ return d_tree.erase(first.iter, last.iter);}
		
		/// @brief Removes the element (if one exists) with key that compares equivalent to the value key. 
		/// @param key key value of the elements to remove
		/// @return number of erased elements
		template <class K>
		SEQ_ALWAYS_INLINE	auto erase(const K& key) -> size_type {return d_tree.erase(key);}
		
		/// @brief Finds an element with key that compares equivalent to the value key. 
		/// @param key value to look for
		/// @return iterator pointing to found element, or end iterator
		template <class K>
		SEQ_ALWAYS_INLINE auto find(const K& key) const -> const_iterator { return d_tree.find(key); }

		template <class K>
		SEQ_ALWAYS_INLINE auto find_ptr(const K& key) const -> const value_type* { return d_tree.find_ptr(key); }

		/// @brief Returns an iterator pointing to the first element that compares not less (i.e. greater or equal) to the value key. 
		template <class K>
		SEQ_ALWAYS_INLINE auto lower_bound(const K& key) const -> const_iterator  { return d_tree.lower_bound(key); }
		
		/// @brief Returns an iterator pointing to the first element that compares greater to the value key. 
		template <class K>
		SEQ_ALWAYS_INLINE auto upper_bound(const K& key) const -> const_iterator  { return d_tree.upper_bound(key); }

		template <class K>
		SEQ_ALWAYS_INLINE auto prefix(const K& key) const -> const_iterator { return d_tree.prefix(key); }

		template <class K>
		SEQ_ALWAYS_INLINE auto prefix_range(const K& key) const -> std::pair<const_prefix_iterator, const_prefix_iterator> { 
			auto p= d_tree.prefix_range(key); 
			return { p.first, p.second };
		}
		
		/// @brief Checks if there is an element with key that compares equivalent to the value key.
		template <class K>
		SEQ_ALWAYS_INLINE auto contains(const K& key) const -> bool  { return find_ptr(key) != nullptr; }
		

		/// @brief Returns the number of elements with key key (either 1 or 0 for radix_set and radix_map).
		template <class K>
		SEQ_ALWAYS_INLINE auto count(const K& key) const -> size_type  { return d_tree.find(key) != d_tree.end(); }
		
		/// @brief Returns a range containing all elements with the given key in the container. 
		/// The range is defined by two iterators, one pointing to the first element that is not less than key and another pointing to the first element greater than key.
		/// Alternatively, the first iterator may be obtained with lower_bound(), and the second with upper_bound().
		template <class K>
		SEQ_ALWAYS_INLINE auto equal_range(const K& key) const -> std::pair<iterator, iterator>  { 
			auto it = d_tree.find(key);
			if (it == d_tree.end())
				return std::pair<iterator, iterator>(d_tree.end(), d_tree.end());
			auto start = it++;
			return std::pair<iterator, iterator>(start, it);
		}

		SEQ_ALWAYS_INLINE void merge(radix_set& source) { d_tree.merge(source.d_tree); }
		
		/// @brief Returns an iterator to the first element of the container.
		SEQ_ALWAYS_INLINE auto begin() noexcept -> iterator { return d_tree.begin(); }
		/// @brief Returns an iterator to the first element of the container.
		SEQ_ALWAYS_INLINE auto begin() const noexcept -> const_iterator { return d_tree.cbegin(); }
		/// @brief Returns an iterator to the first element of the container.
		SEQ_ALWAYS_INLINE auto cbegin() const noexcept -> const_iterator { return d_tree.cbegin(); }

		/// @brief Returns an iterator to the element following the last element of the container.
		SEQ_ALWAYS_INLINE auto end() noexcept -> iterator { return d_tree.end(); }
		/// @brief Returns an iterator to the element following the last element of the container.
		SEQ_ALWAYS_INLINE auto end() const noexcept -> const_iterator { return d_tree.cend(); }
		/// @brief Returns an iterator to the element following the last element of the container.
		SEQ_ALWAYS_INLINE auto cend() const noexcept -> const_iterator { return d_tree.cend(); }

		/// @brief Returns a reverse iterator to the first element of the reversed list.
		SEQ_ALWAYS_INLINE auto rbegin() noexcept -> reverse_iterator { return reverse_iterator(end()); }
		/// @brief Returns a reverse iterator to the first element of the reversed list.
		SEQ_ALWAYS_INLINE auto rbegin() const noexcept -> const_reverse_iterator { return const_reverse_iterator(end()); }
		/// @brief Returns a reverse iterator to the element following the last element of the reversed list.
		SEQ_ALWAYS_INLINE auto rend() noexcept -> reverse_iterator { return reverse_iterator(begin()); }
		/// @brief Returns a reverse iterator to the element following the last element of the reversed list.
		SEQ_ALWAYS_INLINE auto rend() const noexcept -> const_reverse_iterator { return const_reverse_iterator(begin()); }
		/// @brief Returns a reverse iterator to the first element of the reversed list.
		SEQ_ALWAYS_INLINE auto crbegin() const noexcept -> const_reverse_iterator { return rbegin(); }
		/// @brief Returns a reverse iterator to the element following the last element of the reversed list.
		SEQ_ALWAYS_INLINE auto crend() const noexcept -> const_reverse_iterator { return rend(); }
	};





	/// @brief Checks if the contents of s1 and s2 are equal, that is, they have the same number of elements and each element in s1 compares equal with the element in s2 at the same position.
	template<class Key, class ExtractKey , class Al1, class Al2  >
	auto operator==(const radix_set<Key, ExtractKey, Al1>& s1, const radix_set<Key, ExtractKey, Al2>& s2) -> bool
	{
		if (s1.size() != s2.size())
			return false;
		auto it1 = s1.begin();
		for (auto it2 = s2.begin(); it2 != s2.end(); ++it2, ++it1) {
			if (ExtractKey{}(*it1) != ExtractKey{}(*it2))
				return false;
		}
		return true;
	}
	/// @brief Checks if the contents of s1 and s2 are not equals.
	template<class Key, class ExtractKey, class Al1, class Al2  >
	auto operator!=(const radix_set<Key, ExtractKey, Al1>& s1, const radix_set<Key, ExtractKey, Al2>& s2) -> bool
	{
		return !(s1==s2);
	}
	/// @brief Erases all elements that satisfy the predicate p from the container.
	template<class Key, class ExtractKey, class Al, class Pred >
	auto erase_if(radix_set<Key, ExtractKey, Al>& s,  Pred p) -> typename radix_set<Key, ExtractKey, Al>::size_type
	{
		typename radix_set<Key, ExtractKey, Al>::size_type count = 0;
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




	/// @brief Radix based sorted container using Variable Arity Radix Tree (VART). Same interface as std::map.
	/// @tparam Key key type
	/// @tparam T mapped type
	/// @tparam ExtractKey Functor extracting a suitable key for the radix tree
	/// @tparam Allocator allocator
	/// 
	template<class Key, class T, class ExtractKey = default_key<Key>, class Allocator = std::allocator<std::pair<Key, T> > >
	class radix_map
	{
		using radix_key_type = typename radix_detail::ExtractKeyResultType< ExtractKey, Key>::type;
		struct Extract
		{
			radix_key_type operator()(const std::pair<Key, T>& p) const noexcept {
				return ExtractKey{}(p.first);
			}
			radix_key_type operator()(const std::pair<const Key, T>& p) const noexcept {
				return ExtractKey{}(p.first);
			}
			template<class U, class V>
			radix_key_type operator()(const std::pair<U,V>& p) const noexcept {
				return ExtractKey{}(p.first);
			}
			const radix_key_type & operator()(const radix_key_type& p) const noexcept {
				return p;
			}
			template<class U>
			radix_key_type operator()(const U& p) const noexcept {
				return ExtractKey{}(p);
			}
		};
		using radix_tree_type = radix_detail::RadixTree< std::pair<Key, T>, radix_detail::SortedHasher< radix_key_type>, Extract, Allocator >;
		radix_tree_type d_tree;

	public:

		struct const_iterator
		{
			using iter_type = typename radix_tree_type::const_iterator;
			using value_type = std::pair<Key, T>;
			using iterator_category = std::bidirectional_iterator_tag;
			using size_type = size_t;
			using difference_type = std::ptrdiff_t;
			using pointer = const value_type*;
			using reference = const value_type&;
			using const_pointer = const value_type*;
			using const_reference = const value_type&;
			iter_type iter;

			const_iterator() {}
			const_iterator(const iter_type& it) : iter(it) {}
			const_iterator(iter_type&& it) : iter(std::move(it)) {}
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
			
			SEQ_ALWAYS_INLINE auto operator==(const const_iterator& it) const noexcept -> bool { return iter == it.iter; }
			SEQ_ALWAYS_INLINE auto operator!=(const const_iterator& it) const noexcept -> bool { return iter != it.iter; }
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
			iterator(iter_type&& it) : const_iterator(std::move(it)) {}
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

			SEQ_ALWAYS_INLINE auto operator==(const const_iterator& it) const noexcept -> bool { return this->iter == it.iter; }
			SEQ_ALWAYS_INLINE auto operator!=(const const_iterator& it) const noexcept -> bool { return this->iter != it.iter; }
		};



		/// @brief const iterator class for prefix search
		struct const_prefix_iterator
		{
			using iter_type = typename radix_tree_type::const_prefix_iterator;
			using value_type = std::pair<Key, T>;
			using iterator_category = std::forward_iterator_tag;
			using size_type = size_t;
			using difference_type = std::ptrdiff_t;
			using pointer = const value_type*;
			using reference = const value_type&;
			using const_pointer = const value_type*;
			using const_reference = const value_type&;
			iter_type iter;

			const_prefix_iterator() {}
			const_prefix_iterator(const iter_type& it) : iter(it) {}
			SEQ_ALWAYS_INLINE auto operator++() noexcept -> const_prefix_iterator& {
				++iter;
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator++(int) noexcept -> const_prefix_iterator {
				const_prefix_iterator _Tmp = *this;
				++(*this);
				return _Tmp;
			}
			SEQ_ALWAYS_INLINE auto operator*() const noexcept -> reference { return *iter; }
			SEQ_ALWAYS_INLINE auto operator->() const noexcept -> pointer { return  std::pointer_traits<pointer>::pointer_to(**this); }

			SEQ_ALWAYS_INLINE auto operator==(const const_prefix_iterator& it) const noexcept -> bool { return iter == it.iter; }
			SEQ_ALWAYS_INLINE auto operator!=(const const_prefix_iterator& it) const noexcept -> bool { return iter != it.iter; }
		};

		struct prefix_iterator : public const_prefix_iterator
		{
			using iter_type = typename radix_tree_type::const_prefix_iterator;
			using value_type = std::pair<Key, T>;
			using iterator_category = std::forward_iterator_tag;
			using size_type = size_t;
			using difference_type = std::ptrdiff_t;
			using pointer = value_type*;
			using reference = value_type&;
			using const_pointer = const value_type*;
			using const_reference = const value_type&;

			prefix_iterator() : const_prefix_iterator() {}
			prefix_iterator(const iter_type& it) : const_prefix_iterator(it) {}
			SEQ_ALWAYS_INLINE auto operator++() noexcept -> prefix_iterator& {
				++this->iter;
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator++(int) noexcept -> prefix_iterator {
				prefix_iterator _Tmp = *this;
				++(*this);
				return _Tmp;
			}
			SEQ_ALWAYS_INLINE auto operator*() noexcept -> reference { return reinterpret_cast<value_type&>(const_cast<std::pair<Key, T>&>(*this->iter)); }
			SEQ_ALWAYS_INLINE auto operator->() noexcept -> pointer { return  std::pointer_traits<pointer>::pointer_to(**this); }
			SEQ_ALWAYS_INLINE auto operator*() const noexcept -> const_reference { return reinterpret_cast<const value_type&>(*this->iter); }
			SEQ_ALWAYS_INLINE auto operator->() const noexcept -> const_pointer { return  std::pointer_traits<const_pointer>::pointer_to(**this); }
			SEQ_ALWAYS_INLINE auto operator==(const const_prefix_iterator& it) const noexcept -> bool { return this->iter == it.iter; }
			SEQ_ALWAYS_INLINE auto operator!=(const const_prefix_iterator& it) const noexcept -> bool { return this->iter != it.iter; }
		};



		using key_type = Key;
		using mapped_type = T;
		using value_type = std::pair<Key, T>;
		using difference_type = typename std::allocator_traits<Allocator>::difference_type;
		using size_type = typename std::allocator_traits<Allocator>::size_type;
		using allocator_type = Allocator;
		using key_extract = ExtractKey;
		using value_type_extract = Extract;
		using reference = value_type&;
		using const_reference = const value_type&;
		using pointer = typename std::allocator_traits<Allocator>::pointer;
		using const_pointer = typename std::allocator_traits<Allocator>::const_pointer;
		using reverse_iterator = std::reverse_iterator<iterator>;
		using const_reverse_iterator = std::reverse_iterator<const_iterator>;

		radix_map() {}
		explicit radix_map(const Allocator& alloc)
			:d_tree(alloc) {}
		template< class InputIt >
		radix_map(InputIt first, InputIt last,
			const Allocator& alloc = Allocator())
			: d_tree(first, last, alloc) {}
		template< class InputIt >
		radix_map(const radix_map& other)
			:d_tree(other.d_tree) {}

		radix_map(const radix_map& other, const Allocator& alloc)
			:d_tree(other.d_tree, alloc) {}
		radix_map(const radix_map& other)
			:d_tree(other.d_tree) {}
		radix_map(radix_map&& other) noexcept(std::is_nothrow_move_constructible<radix_tree_type>::value)
			:d_tree(std::move(other.d_tree)) {}
		radix_map(radix_map&& other, const Allocator& alloc)
			:d_tree(std::move(other.d_tree), alloc) {}
		radix_map(std::initializer_list<value_type> init, const Allocator& alloc = Allocator())
			: radix_map(init.begin(),init.end(), alloc) {}

		auto operator=(radix_map&& other) noexcept(std::is_nothrow_move_assignable<radix_tree_type>::value) -> radix_map& {
			d_tree = std::move(other.d_tree);
			return *this;
		}
		auto operator=(const radix_map& other) -> radix_map& {
			d_tree = (other.d_tree);
			return *this;
		}
		auto operator=(const std::initializer_list<value_type>& init) -> radix_map&
		{
			d_tree = init;
			return *this;
		}

		SEQ_ALWAYS_INLINE auto get_allocator() const noexcept -> const Allocator& { return d_tree.get_allocator(); }
		SEQ_ALWAYS_INLINE auto get_allocator() noexcept -> Allocator& { return d_tree.get_allocator(); }

		SEQ_ALWAYS_INLINE auto empty() const noexcept -> bool { return d_tree.empty(); }
		SEQ_ALWAYS_INLINE auto size() const noexcept -> size_type { return d_tree.size(); }
		SEQ_ALWAYS_INLINE auto max_size() const noexcept -> size_type { return d_tree.max_size(); }
		SEQ_ALWAYS_INLINE void clear() noexcept { d_tree.clear(); }
		SEQ_ALWAYS_INLINE void swap(radix_map& other) noexcept(noexcept(std::declval< radix_tree_type&>().swap(std::declval< radix_tree_type&>()))) 
		{ d_tree.swap(other.d_tree); }

		SEQ_ALWAYS_INLINE auto insert(const value_type& value) -> std::pair<iterator, bool> { return d_tree.emplace(value); }
		SEQ_ALWAYS_INLINE auto insert(value_type&& value) -> std::pair<iterator, bool> { return d_tree.emplace(std::move(value)); }
		template< class P, typename std::enable_if<std::is_constructible<value_type, P>::value, int>::type = 0 >
		SEQ_ALWAYS_INLINE auto insert(P&& value) -> std::pair<iterator, bool> { return d_tree.emplace(std::forward<P>(value)); }
		SEQ_ALWAYS_INLINE auto insert(const_iterator hint, const value_type& value) -> iterator { return d_tree.emplace_hint(hint.iter, value); }
		SEQ_ALWAYS_INLINE auto insert(const_iterator hint, value_type&& value) -> iterator { return d_tree.emplace_hint(hint.iter, std::move(value)); }
		template< class P, typename std::enable_if<std::is_constructible<value_type, P>::value, int>::type = 0 >
		auto insert(const_iterator hint, P&& value) -> iterator { return d_tree.emplace_hint(hint.iter, std::forward<P>(value)); }

		template< class... Args >
		SEQ_ALWAYS_INLINE auto emplace(Args&&... args) -> std::pair<iterator, bool> { return d_tree.emplace(std::forward<Args>(args)...); }
		template <class... Args>
		SEQ_ALWAYS_INLINE auto emplace_hint(const_iterator hint, Args&&... args) -> iterator { return d_tree.emplace_hint(hint.iter, std::forward<Args>(args)...); }
		

		template< class... Args >
		SEQ_ALWAYS_INLINE auto try_emplace(const Key& k, Args&&... args) -> std::pair<iterator, bool>
		{
			return d_tree.try_emplace(k, std::forward<Args>(args)...);
		}
		template< class... Args >
		SEQ_ALWAYS_INLINE auto try_emplace(Key&& k, Args&&... args) -> std::pair<iterator, bool>
		{
			return d_tree.try_emplace(std::move(k), std::forward<Args>(args)...);
		}
		template< class... Args >
		SEQ_ALWAYS_INLINE auto try_emplace(const_iterator hint, const Key& k, Args&&... args) -> iterator
		{
			return d_tree.try_emplace_hint(hint.iter, k, std::forward<Args>(args)...).first;
		}
		template< class... Args >
		SEQ_ALWAYS_INLINE auto try_emplace(const_iterator hint, Key&& k, Args&&... args) -> iterator
		{
			return d_tree.try_emplace_hint(hint.iter, std::move(k), std::forward<Args>(args)...).first;
		}


		template <class M>
		SEQ_ALWAYS_INLINE auto insert_or_assign(const Key& k, M&& obj) -> std::pair<iterator, bool>
		{
			auto inserted = try_emplace(k, std::forward<M>(obj));
			if (!inserted.second)
				inserted.first->second = std::forward<M>(obj);
			return inserted;
		}
		template <class M>
		SEQ_ALWAYS_INLINE auto insert_or_assign(Key&& k, M&& obj) -> std::pair<iterator, bool>
		{
			auto inserted = try_emplace(std::move(k), std::forward<M>(obj));
			if (!inserted.second)
				inserted.first->second = std::forward<M>(obj);
			return inserted;
		}
		
		template <class M>
		SEQ_ALWAYS_INLINE auto insert_or_assign(const_iterator hint, const Key& k, M&& obj) -> iterator
		{
			auto inserted = d_tree.try_emplace_hint(hint.iter, k, std::forward<M>(obj));
			if (!inserted.second)
				inserted.first->second = std::forward<M>(obj);
			return inserted.first;
		}
		template <class M>
		SEQ_ALWAYS_INLINE auto insert_or_assign(const_iterator hint, Key&& k, M&& obj) -> iterator
		{
			auto inserted = d_tree.try_emplace_hint(hint.iter, std::move(k), std::forward<M>(obj));
			if (!inserted.second)
				inserted.first->second = std::forward<M>(obj);
			return inserted.first;
		}

		SEQ_ALWAYS_INLINE auto at(const Key& key) -> T&
		{
			auto it = find(key);
			if (it == end())
				throw  std::out_of_range("radix_map::at : invalid key");
			return it->second;
		}
		SEQ_ALWAYS_INLINE auto at(const Key& key) const -> const T&
		{
			auto it = find(key);
			if (it == end())
				throw  std::out_of_range("radix_map::at : invalid key");
			return it->second;
		}

		SEQ_ALWAYS_INLINE auto operator[](const Key& k) -> T&
		{
			return try_emplace(k).first->second;
		}
		SEQ_ALWAYS_INLINE auto operator[](Key&& k) -> T&
		{
			return try_emplace(std::move(k)).first->second;
		}

		template< class InputIt >
		SEQ_ALWAYS_INLINE void insert(InputIt first, InputIt last) { return d_tree.insert(first, last); }
		SEQ_ALWAYS_INLINE void insert(std::initializer_list<value_type> ilist) { return d_tree.insert(ilist.begin(),ilist.end()); }

		template< class Iter >
		SEQ_ALWAYS_INLINE void assign(Iter first, Iter last) { d_tree.assign(first, last); }

		SEQ_ALWAYS_INLINE auto erase(iterator pos) -> iterator { return d_tree.erase(pos.iter); }
		SEQ_ALWAYS_INLINE auto erase(const_iterator pos) -> iterator { return d_tree.erase(pos.iter); }
		SEQ_ALWAYS_INLINE auto erase(const_iterator first, const_iterator last) -> iterator { return d_tree.erase(first.iter, last.iter); }

		template <class K>
		SEQ_ALWAYS_INLINE	auto erase(const K& key) -> size_type { return d_tree.erase(key); }

		template <class K>
		SEQ_ALWAYS_INLINE auto find(const K& key) -> iterator { return d_tree.find(key); }
		template <class K>
		SEQ_ALWAYS_INLINE auto find_ptr(const K& key) -> value_type* { return const_cast<value_type*>(d_tree.find_ptr(key)); }

		template <class K>
		SEQ_ALWAYS_INLINE auto find(const K& key) const -> const_iterator { return d_tree.find(key); }
		template <class K>
		SEQ_ALWAYS_INLINE auto find_ptr(const K& key) const -> const value_type* { return d_tree.find_ptr(key); }

		template <class K>
		SEQ_ALWAYS_INLINE auto lower_bound(const K& x) -> iterator { return d_tree.lower_bound(x); }

		template <class K>
		SEQ_ALWAYS_INLINE auto lower_bound(const K& x) const -> const_iterator { return d_tree.lower_bound(x); }

		template <class K>
		SEQ_ALWAYS_INLINE auto upper_bound(const K& x) -> iterator { return d_tree.upper_bound(x); }

		template <class K>
		SEQ_ALWAYS_INLINE auto upper_bound(const K& x) const -> const_iterator { return d_tree.upper_bound(x); }

		template <class K>
		SEQ_ALWAYS_INLINE auto prefix(const K& x) -> iterator { return d_tree.prefix(x); }

		
		template <class K>
		SEQ_ALWAYS_INLINE auto prefix(const K& x) const -> const_iterator { return d_tree.prefix(x); }

		template <class K>
		SEQ_ALWAYS_INLINE auto prefix_range(const K& x) -> std::pair<prefix_iterator, prefix_iterator>
		{
			auto p = d_tree.prefix_range(x);
			return { p.first, p.second };
		}
		template <class K>
		SEQ_ALWAYS_INLINE auto prefix_range(const K& x) const -> std::pair<const_prefix_iterator, const_prefix_iterator>
		{
			auto p = d_tree.prefix_range(x);
			return { p.first, p.second };
		}

		template <class K>
		SEQ_ALWAYS_INLINE auto contains(const K& x) const -> bool { return d_tree.find_ptr(x) != nullptr; }

		template <class K>
		SEQ_ALWAYS_INLINE auto count(const K& x) const -> size_type { return static_cast<size_type>(contains(x)); }

		template <class K>
		SEQ_ALWAYS_INLINE auto equal_range(const K& key) -> std::pair<iterator, iterator> {
			auto it = d_tree.find(key);
			if (it == d_tree.end())
				return std::pair<iterator, iterator>(d_tree.end(), d_tree.end());
			auto start = it++;
			return std::pair<iterator, iterator>(start, it);
		}

		template <class K>
		SEQ_ALWAYS_INLINE auto equal_range(const K& key) const -> std::pair<const_iterator, const_iterator> {
			auto it = d_tree.find(key);
			if (it == d_tree.end())
				return std::pair<const_iterator, const_iterator>(d_tree.end(), d_tree.end());
			auto start = it++;
			return std::pair<const_iterator, const_iterator>(start, it);
		}
		
		SEQ_ALWAYS_INLINE void merge(radix_map& source) { d_tree.merge(source.d_tree); }

		SEQ_ALWAYS_INLINE auto begin() noexcept -> iterator { return d_tree.begin(); }
		SEQ_ALWAYS_INLINE auto begin() const noexcept -> const_iterator { return d_tree.cbegin(); }
		SEQ_ALWAYS_INLINE auto cbegin() const noexcept -> const_iterator { return d_tree.cbegin(); }

		SEQ_ALWAYS_INLINE auto end() noexcept -> iterator { return d_tree.end(); }
		SEQ_ALWAYS_INLINE auto end() const noexcept -> const_iterator { return d_tree.cend(); }
		SEQ_ALWAYS_INLINE auto cend() const noexcept -> const_iterator { return d_tree.cend(); }

		/// @brief Returns a reverse iterator to the first element of the reversed list.
		SEQ_ALWAYS_INLINE auto rbegin() noexcept -> reverse_iterator { return reverse_iterator(end()); }
		/// @brief Returns a reverse iterator to the first element of the reversed list.
		SEQ_ALWAYS_INLINE auto rbegin() const noexcept -> const_reverse_iterator { return const_reverse_iterator(end()); }
		/// @brief Returns a reverse iterator to the element following the last element of the reversed list.
		SEQ_ALWAYS_INLINE auto rend() noexcept -> reverse_iterator { return reverse_iterator(begin()); }
		/// @brief Returns a reverse iterator to the element following the last element of the reversed list.
		SEQ_ALWAYS_INLINE auto rend() const noexcept -> const_reverse_iterator { return const_reverse_iterator(begin()); }
		/// @brief Returns a reverse iterator to the first element of the reversed list.
		SEQ_ALWAYS_INLINE auto crbegin() const noexcept -> const_reverse_iterator { return rbegin(); }
		/// @brief Returns a reverse iterator to the element following the last element of the reversed list.
		SEQ_ALWAYS_INLINE auto crend() const noexcept -> const_reverse_iterator { return rend(); }

	};




	/// @brief Checks if the contents of s1 and s2 are equal, that is, they have the same number of elements and each element in s1 compares equal with the element in s2 at the same position.
	template<class Key, class T, class ExtractKey, class Al1, class Al2  >
	auto operator==(const radix_map<Key, T, ExtractKey, Al1>& s1, const radix_map<Key,T, ExtractKey, Al2>& s2) -> bool
	{
		using value_type_extract = typename radix_map<Key, T, ExtractKey, Al1>::value_type_extract;
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
	template<class Key, class T, class ExtractKey, class Al1, class Al2  >
	auto operator!=(const radix_map<Key,T, ExtractKey, Al1>& s1, const radix_map<Key,T, ExtractKey, Al2>& s2) -> bool
	{
		return !(s1 == s2);
	}
	/// @brief Erases all elements that satisfy the predicate p from the container.
	template<class Key, class T, class ExtractKey, class Al, class Pred >
	auto erase_if(radix_map<Key, T, ExtractKey, Al>& s, Pred p) -> typename radix_map<Key, T, ExtractKey, Al>::size_type
	{
		typename radix_map<Key, T, ExtractKey, Al>::size_type count = 0;
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

} //end namespace seq



/** @}*/
//end containers

#endif
