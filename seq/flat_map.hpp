#ifndef SEQ_FLAT_MAP_HPP
#define SEQ_FLAT_MAP_HPP



/** @file */

/**\defgroup containers Containers: original STL-like containers

The \ref containers "containers" module defines several container classes as alternatives to STL containers or providing features not present in the STL.
These containers generally adhere to the properties of STL containers, though there are often some associated API differences and/or implementation details which differ from the standard library.

The Seq containers are not necessarly drop-in replacement for their STL counterparts as they usually provide different iterator/reference statibility rules or different exception guarantees.

Currently, the \ref containers "containers" module provide 5 types of containers:
	-	Sequential random-access containers: seq::devector and seq::tiered_vector,
	-	Sequential stable non random-access container: seq::sequence,
	-	Sorted containers: seq::flat_set, seq::flat_map, seq::flat_multiset and seq::flat_multimap,
	-	Ordered robin-hood hash tables: seq::ordered_set and seq::ordered_map.
	-	Tiny string and string view: seq::tiny_string and seq::tstring_view.

See the documentation of each class for more details.

*/


#include "tiered_vector.hpp"
#include "pdqsort.hpp"



/** \addtogroup containers
 *  @{
 */


namespace seq
{
	namespace detail
	{


		template<class T>
		struct is_less_or_greater : std::false_type {};
		template<class T>
		struct is_less_or_greater<std::less<T> > : std::true_type {};
		template<class T>
		struct is_less_or_greater<std::greater<T> > : std::true_type {};

		template<class Sequence, class Value>
		struct MapInserter
		{
			// Insert value into tiered_vector while trying to avoid constructing a new value_type
			static SEQ_ALWAYS_INLINE auto insert_pos(Sequence& seq, Value& value) -> std::pair<size_t,bool> { return seq.insert_pos(std::move(value)); }
			static SEQ_ALWAYS_INLINE auto insert(Sequence& seq, Value& value) -> std::pair<typename Sequence::iterator, bool> { return seq.insert(std::move(value)); }
		};
		template<class Sequence, class Value>
		struct MapInserter<Sequence, const Value&>
		{
			static SEQ_ALWAYS_INLINE auto insert_pos(Sequence& seq, const Value& value) -> std::pair<size_t, bool> { return seq.insert_pos((value)); }
			static SEQ_ALWAYS_INLINE auto insert(Sequence& seq, const Value& value) -> std::pair<typename Sequence::iterator, bool> { return seq.insert((value)); }
		};

		template<class Sequence, class Value>
		struct MapInserter<Sequence, Value&&>
		{
			static SEQ_ALWAYS_INLINE auto insert_pos(Sequence& seq, Value& value) -> std::pair<size_t, bool> { return seq.insert_pos(std::move(value)); }
			static SEQ_ALWAYS_INLINE auto insert(Sequence& seq, Value& value) -> std::pair<typename Sequence::iterator, bool> { return seq.insert(std::move(value)); }
		};
	


		template<bool Stable, class Less, bool IsArithmetic = std::is_arithmetic<typename Less::key_type>::value , bool LessOrGreater = is_less_or_greater<typename Less::compare>::value >
		struct DequeSorter
		{
			// Sort tiered_vector using pdqsort.
			// For now, always uses pdqsort_branchless as it seems to be faster event with non trivial comparison operator.
			template<class Deque>
			static void sort(Deque& d, size_t begin, size_t end, const Less & less) {

				pdqsort_branchless(d.begin() + begin, d.begin() + end, less);
			}
		};
		template< class Less>
		struct DequeSorter<false,Less,true,true>
		{
			template<class Deque>
			static void sort(Deque& d, size_t begin, size_t end , const Less & less) {

				pdqsort_branchless(d.begin() + begin, d.begin() + end, less);
			}
		};
		template<class Less, bool IsArithmetic, bool LessOrGreater >
		struct DequeSorter<true,Less, IsArithmetic, LessOrGreater>
		{
			// Sort tiered_vector in a stable way
			template<class Deque>
			static void sort(Deque& d, size_t begin, size_t end, const Less& less) {

				std::stable_sort(d.begin() + begin, d.begin() + end, less);
			}
		};

		template<bool Stable, class Deque, class Less>
		void sort_deque(Deque & d, size_t begin, size_t end, Less less)
		{
			DequeSorter<Stable,Less>::sort(d, begin, end, less);
		}


		template< class It, class Cat>
		auto internal_distance(It first, It last, Cat /*unused*/) -> std::ptrdiff_t
		{
			return 0;
		}
		template< class It>
		auto internal_distance(It first, It last, std::random_access_iterator_tag /*unused*/) -> std::ptrdiff_t
		{
			return last - first;
		}
		template< class It>
		auto distance(It first, It last) -> std::ptrdiff_t
		{
			return internal_distance(first, last, typename std::iterator_traits<It>::iterator_category());
		}

	

		/**
		* \internal
		* Merge 2 sorted ranges into a single array (or tiered_vector).
		* the 2 sequences must be sorted without consecutive equal values.
		* This method is stable: if the 2 ranges contain equal values, only the values from the first range are inserted.
		* 
		*/
		template<class Deque, class InputIt1, class InputIt2, class Less, class Deque2 = Deque>
		void unique_merge_move(Deque& out, InputIt1 first1, InputIt1 last1, InputIt2 first2, InputIt2 last2, Less less, Deque2 * remaining = NULL)
		{
			size_t len1 = distance(first1, last1);
			size_t len2 = distance(first2, last2);
			if (len1 && len2) {
				out.resize(len1 + len2);
				typename Deque::iterator it_rem;
				if (remaining) {
					remaining->resize(len2);
					it_rem = remaining->begin();
				}
				auto it = out.begin();
				while (!(first1 == last1 && first2 == last2)) {

					while (first2 != last2 && (first1 == last1 || less(*first2, *first1))) {
						*it = std::move(*first2);
						++it;
						++first2;
					}
					if (first1 != last1) {
						//skip equals elements
						while (first2 != last2 && *first2 == *first1) {
							if (remaining) {
								*it_rem = std::move (*first2);
								++it_rem;
							}
							++first2;
						}
						*it = std::move(*first1);
						++it;
						first1++;
					}
					while (first1 != last1 && (first2 == last2 || less(*first1, *first2))) {
						*it = std::move(*first1);
						++it;
						++first1;
					}
				}
				if (remaining)
					remaining->erase(it_rem, remaining->end());
				out.erase(it, out.end());
				return;
			}

		
			while (!(first1 == last1 && first2 == last2)) {

				while (first2 != last2 && (first1 == last1 || less(*first2, *first1))) {
					out.push_back(std::move (*first2));
					++first2;
				}
				if (first1 != last1) {
					//skip equals elements
					while (first2 != last2 && *first2 == *first1) {
						if (remaining) {
							remaining->push_back(std::move (*first2));
						}
						++first2;
					}
					out.push_back(std::move (*first1));
					first1++;
				}
				while (first1 != last1 && (first2 == last2 || less(*first1, *first2))) {
					out.push_back(std::move (*first1));
					++first1;
				}
			
			}
		}


	

		template<class Key, class Value, class Less>
		struct BaseTree : Less
		{
			// Base class for flat_tree, directly inherit Less and provides basic comparison functions
			using extract_key = ExtractKey<Key, Value>;

			BaseTree() {}
			BaseTree(const Less& l) : Less(l) {}
			BaseTree(const BaseTree& other) : Less(other) {}
			BaseTree(BaseTree&& other)  noexcept : Less(std::move(static_cast<Less&>(other))) {}

			auto operator=(const BaseTree& other) -> BaseTree& {
				static_cast<Less&>(*this) = static_cast<const Less&>(other);
				return *this;
			}
			auto operator=( BaseTree&& other) noexcept -> BaseTree&   {
				static_cast<Less&>(*this) = std::move(static_cast<Less&>(other));
				return *this;
			}

			template<class U, class V>
			SEQ_ALWAYS_INLINE auto operator()(const U& v1, const V& v2)  const noexcept -> bool { return Less::operator()(extract_key::key(v1), extract_key::key(v2)); }

			template<class U, class V>
			SEQ_ALWAYS_INLINE auto equal(const U& v1, const V& v2)  const noexcept -> bool { return !operator()(v1, v2) && !operator()(v2, v1); }
		};
		template<class Key, class Less>
		struct BaseTree<Key,Key,Less> : Less
		{
			using extract_key = ExtractKey<Key, Key>;

			BaseTree() {}
			BaseTree(const Less& l) : Less(l) {}
			BaseTree(const BaseTree& other) : Less(other) {}
			BaseTree(BaseTree&& other)  noexcept : Less(std::move(static_cast<Less&>(other))) {}

			auto operator=(const BaseTree& other) -> BaseTree& {
				static_cast<Less&>(*this) = static_cast<const Less&>(other);
				return *this;
			}
			auto operator=(BaseTree&& other) noexcept -> BaseTree&   {
				static_cast<Less&>(*this) = std::move(static_cast<Less&>(other));
				return *this;
			}

			template<class U, class V>
			SEQ_ALWAYS_INLINE auto operator()(const U& v1, const V& v2) const noexcept -> bool{ return Less::operator()(v1,v2); }

			template<class U, class V>
			SEQ_ALWAYS_INLINE auto equal(const U& v1, const V& v2) const noexcept -> bool { return !operator()(v1, v2) && !operator()(v2, v1); }
		};

	
		
		template<class Key, class Value = Key, class Compare = std::less<Key>, class Allocator = std::allocator<Value>, LayoutManagement layout = OptimizeForSpeed, bool Stable = true, bool Unique = true>
		struct flat_tree : private BaseTree<Key,Value,Compare>
		{
			// Flat set/map class. 
			// Uses a seq::tiered_vector to store the values and provide faster insertion/deletion of unique elements.

		
			using extract_key = ExtractKey<Key, Value>;
			using base_type = BaseTree<Key, Value, Compare>;
			using this_type = flat_tree<Key, Value, Compare, Allocator, layout, Stable, Unique>;

			struct Equal
			{
				flat_tree* c;
				Equal(flat_tree * c):c(c) {}
				SEQ_ALWAYS_INLINE auto operator()(const Value& a, const Value& b) const -> bool { return c->equal((a), (b)); }
			};
			struct Less
			{
				using key_type = Key;
				using compare = Compare;
				flat_tree* c;
				Less(flat_tree* c) :c(c) {}
				SEQ_ALWAYS_INLINE auto operator()(const Value& a, const Value& b) const -> bool { return (*c)((a), (b)); }
			};

			SEQ_ALWAYS_INLINE auto base() noexcept -> base_type&  { return (*this); }
			SEQ_ALWAYS_INLINE auto base() const noexcept -> const base_type& { return  (*this); }

		public:
		
			using deque_type = tiered_vector<Value, Allocator, layout, SEQ_MIN_BUCKET_SIZE(Value), SEQ_MAX_BUCKET_SIZE, FindBucketSize<Value>, true, ExtractKey<Key,Value> >;
			using key_type = Key;
			using value_type = Value;
			using difference_type = typename std::allocator_traits<Allocator>::difference_type;
			using size_type = typename std::allocator_traits<Allocator>::size_type;
			using key_compare = Compare;
			using value_compare = Less;
			using allocator_type = Allocator;
			using reference = Value&;
			using const_reference = const Value&;
			using pointer = typename std::allocator_traits<Allocator>::pointer;
			using const_pointer = typename std::allocator_traits<Allocator>::const_pointer;
			using iterator = typename deque_type::iterator;
			using const_iterator = typename deque_type::const_iterator;
			using reverse_iterator = typename deque_type::reverse_iterator;
			using const_reverse_iterator = typename deque_type::const_reverse_iterator;

			// tiered_vector object
			deque_type d_deque;
			// check if dirty (in case of calls to flat_tree::tvector())
			int d_dirty;

			void mark_dirty()
			{
				d_dirty = 1;
			}
			auto dirty() const -> bool
			{
				return d_dirty != 0;
			}
			SEQ_ALWAYS_INLINE void check_dirty() const
			{
				if (SEQ_UNLIKELY(dirty()))
					throw std::logic_error("");
			}


			template<class Iter, class Cat>
			void assign_cat(Iter first, Iter last, Cat /*unused*/)
			{
				iterator it = d_deque.begin();
				iterator en = d_deque.end();
				bool sorted = true;
				bool unique = true;

				// Copy the range [first,last) to the tiered_vector using already allocated memory

				Iter prev = first;
				size_type count = 1;
				if (d_deque.size() > 0) {
					*it = *first;
					++it;
					++first;
				}
				else {
					d_deque.push_back(*first);
					++first;
				}

				while (it != en && first != last) {
					*it = *first;
					if (sorted) {
						if ((*this)(*first, *prev))
							sorted = false;
						else if (Unique && !(*this)(*prev, *first))
							unique = false;
					}
					++it;
					prev = first;
					++first;
					++count;
				}
				while (first != last) {
					push_back(*first);
					if (sorted) {
						if ((*this)(*first, *prev))
							sorted = false;
						else if (Unique && !(*this)(*prev, *first))
							unique = false;
						prev = first;
					}
				
					++first;
					++count;
				}

				// Resize if the tiered_vector was bigger
				resize(count);

				// Sort if necessary, remove non unique values if necessary
				if (!sorted)
					sort_deque<Stable>(d_deque, 0, d_deque.size(), Less(this));
				if (Unique && (!sorted || !unique)) {
					iterator it = std::unique(d_deque.begin(), d_deque.end(), Equal(this));
					d_deque.erase(it, d_deque.end());
				}
				if (!sorted || (Unique && (!sorted || !unique)))
					d_deque.manager()->update_all_back_values();
			}

			template<class Iter>
			void assign_cat(Iter first, Iter last, std::random_access_iterator_tag /*unused*/)
			{
				//d_deque.clear();
				d_deque.resize(last - first);
				auto it = d_deque.begin();
				bool sorted = true;
				bool unique = true;

				Iter prev = first;
				*it = *first;
				++it;
				++first;
				while (first != last && sorted) {
					*it = *first;
				
					if ((*this)(*first, *prev))
						sorted = false;
					else if (Unique && !(*this)(*prev, *first))
						unique = false;
					prev = first;
				
					++it;
					++first;
				}
				while (first != last) {
					*it = *first;
					++it;
					++first;
				}

				if (!sorted)
					sort_deque<Stable>(d_deque, 0, d_deque.size(), Less(this));
				if (Unique && (!sorted || !unique)) {
					iterator it = std::unique(d_deque.begin(), d_deque.end(), Equal(this));
					d_deque.erase(it, d_deque.end());
				}
				if(!sorted || (Unique && (!sorted || !unique)))
					d_deque.manager()->update_all_back_values();
			}


			flat_tree() :base_type(), d_deque(), d_dirty(0) {}
			explicit flat_tree(const Compare& comp,
				const Allocator& alloc = Allocator())
				: base_type(comp), d_deque(alloc), d_dirty(0) {}
			explicit flat_tree(const Allocator& alloc)
				: base_type(), d_deque(alloc), d_dirty(0) {}
			template< class InputIt >
			flat_tree(InputIt first, InputIt last,
				const Compare& comp = Compare(),
				const Allocator& alloc = Allocator())
				: base_type(comp), d_deque(alloc), d_dirty(0)
			{
				assign(first, last);
			}
			template< class InputIt >
			flat_tree(InputIt first, InputIt last, const Allocator& alloc)
				: flat_tree(first, last, Compare(), alloc) {}
			flat_tree(const flat_tree& other)
				: base_type(other), d_deque(other.d_deque), d_dirty(other.d_dirty) {}
			flat_tree(const flat_tree& other, const Allocator& alloc)
				: base_type(other), d_deque(other.d_deque, alloc), d_dirty(other.d_dirty) {}
			flat_tree(flat_tree&& other) noexcept
				:base_type(std::move(other)), d_deque(std::move(other.d_deque)), d_dirty(other.d_dirty) {}
			flat_tree(flat_tree&& other, const Allocator& alloc) 
				: base_type(std::move(other)), d_deque(std::move(other.d_deque), alloc), d_dirty(other.d_dirty) {}
			flat_tree(std::initializer_list<value_type> init,
				const Compare& comp = Compare(),
				const Allocator& alloc = Allocator())
				:flat_tree(init.begin(), init.end(), comp, alloc) {}
			flat_tree(std::initializer_list<value_type> init, const Allocator& alloc)
				: flat_tree(init, Compare(), alloc) {}

			auto operator=(flat_tree&& other) noexcept -> flat_tree& {
				d_deque = std::move(other.d_deque);
				d_dirty = other.d_dirty;
				base() = std::move(other.base());
				return *this;
			}
			auto operator=(const flat_tree& other) -> flat_tree& 
			{
				d_deque = (other.d_deque);
				d_dirty = other.d_dirty;
				base() = other.base();
				return *this;
			}
			auto operator=(const std::initializer_list<value_type>& init) -> flat_tree&
			{
				assign(init.begin(), init.end());
				return *this;
			}

			auto get_allocator() const noexcept -> Allocator { return d_deque.get_allocator(); }
			auto empty() const noexcept -> bool { return d_deque.empty(); }
			auto size() const noexcept -> size_type { return d_deque.size(); }
			auto max_size() const noexcept -> size_type { return d_deque.max_size(); }
			void clear() noexcept { d_deque.clear(); }
			auto tvector() noexcept -> deque_type& { mark_dirty(); return d_deque; }
			auto tvector() const noexcept -> const deque_type& { return d_deque; }
			auto ctvector() const noexcept -> const deque_type& { return d_deque; }

			void swap(flat_tree& other) noexcept
			{
				d_deque.swap(other.d_deque);
				std::swap(d_dirty,other.d_dirty);
				std::swap(base(), other.base());
			}
			template<class U>
			auto insert_pos_multi(U && value) -> std::pair<size_t, bool>
			{
				size_t pos = d_deque.upper_bound(value, base());
				d_deque.insert(pos, std::move(value));
				return std::pair<size_t, bool>(pos, true);
			}
			auto insert_pos(const value_type& value) -> std::pair<size_t, bool> {
				check_dirty();
				if (!Unique) 
					return insert_pos_multi(value);
			
				std::pair<size_t, bool> res;
				res.first = d_deque.lower_bound(value, base());
				res.second = !(res.first != d_deque.size() && !(*this)(value, d_deque[res.first]));
				if(res.second)
					d_deque.insert(res.first, value);
				return res;
			}
			auto insert_pos( value_type&& value) -> std::pair<size_t, bool> {
				check_dirty();
				if (!Unique)
					return insert_pos_multi(std::move(value));

				size_t pos = d_deque.lower_bound(value, base());
				if (pos != d_deque.size() && !(*this)(value, d_deque[pos]))//!(value < tiered_vector[pos]))
					return std::pair<size_t, bool>(pos, false);
				else {
					d_deque.insert(pos, std::move(value));
					return std::pair<size_t, bool>(pos, true);

				}
			}
			auto insert(const value_type& value) -> std::pair<iterator, bool> {
				check_dirty();
				if (!Unique) {
					size_t pos = d_deque.upper_bound(value, base());
					d_deque.insert(pos, value);
					return std::pair<iterator, bool>(d_deque.iterator_at(pos), true);
				}
				size_t pos = d_deque.lower_bound(value, base());
				if (pos != d_deque.size() && !(*this)(value, d_deque[pos]))//!(value < tiered_vector[pos]))
					return std::pair<iterator, bool>(d_deque.iterator_at(pos), false);
				else {
					d_deque.insert(pos, value);
					return std::pair<iterator, bool>(d_deque.iterator_at(pos), true);
				
				}
			}
			template< class... Args >
			auto emplace(Args&&... args) -> std::pair<iterator, bool>
			{
				using store_type = typename BuildValue<Value, Args&&...>::type;
				store_type st = BuildValue<Value, Args&&...>::build(std::forward<Args>(args)...);
				return MapInserter<this_type, store_type>::insert(*this, st);
			}
			template< class... Args >
			auto emplace_pos(Args&&... args) -> std::pair<size_t, bool>
			{
				using store_type = typename BuildValue<Value, Args&&...>::type;
				store_type st = BuildValue<Value, Args&&...>::build(std::forward<Args>(args)...);
				return MapInserter<this_type, store_type>::insert_pos(*this, st);
			}
			template <class... Args>
			auto emplace_hint(const_iterator hint, Args&&... args) -> iterator
			{
				return insert(hint, Value(std::forward<Args>(args)...));
			}
			auto insert(value_type&& value) -> std::pair<iterator, bool> {
				check_dirty();
				if (!Unique) {
					size_t pos = d_deque.upper_bound(value, base());
					d_deque.insert(pos, std::move(value));
					return std::pair<iterator, bool>(d_deque.iterator_at(pos), true);
				}
				size_t pos = d_deque.lower_bound(value, base());
				if (pos != d_deque.size() && !(*this)(value, d_deque[pos]))//!(value < tiered_vector[pos]))
					return std::pair<iterator, bool>(d_deque.iterator_at(pos), false);
				else {
					d_deque.insert(pos, std::move(value));
					return std::pair<iterator, bool>(d_deque.iterator_at(pos), true);
				}
			}
			auto insert(const_iterator hint, const value_type& value) -> iterator {
				check_dirty();
				if (!Unique) {
					if ((*this)(value, *hint)) {
						if (!(*this)( value, *(hint - 1)))
							return d_deque.insert(hint, value);
					}
					return insert(value).first;
				}
				if ((*this)(value, *hint)) {
					//value is < hint, check that value before hint is smaller
					if ((*this)(*(hint - 1), value))
						return d_deque.insert(hint, value);
				}
				return insert(value).first;
			}
			auto insert(const_iterator hint, value_type&& value) -> iterator {
				check_dirty();
				if (!Unique) {
					if ((*this)(value, *hint)) {
						if (!(*this)(value, *(hint - 1)))
							return d_deque.insert(hint, std::move(value));
					}
					return insert(std::move(value)).first;
				}
				if ((*this)(value, *hint)) {
					//value is < hint, check that value before hint is smaller
					if ((*this)(*(hint - 1), value))
						return d_deque.insert(hint, std::move(value));
				}
				return insert(std::move(value)).first;
			}
			template< class InputIt >
			void insert(InputIt first, InputIt last)
			{
				if (first == last)
					return;

				if (d_deque.empty()) {
					assign(first, last);
					return;
				}
			
				size_t len = detail::distance(first, last);
				size_t size_before = d_deque.size();

				// Resize tiered_vector
				d_deque.resize(len + size_before);

				// Append input to tiered_vector
				auto dit = d_deque.begin() + size_before;
				if(len)
					std::copy(first, last, dit);
				else {
					while (first != last) {
						*dit = *first;
						++first;
						++dit;
					}
				}
				// Sort new values
				sort_deque<Stable>(d_deque, size_before, d_deque.size(), Less(this));

				// Merge
				std::inplace_merge(d_deque.begin(), d_deque.begin() + size_before, d_deque.end(), Less(this));

				if (Unique) {
					// Remove duplicates
					auto it = std::unique(d_deque.begin(), d_deque.end(), Equal(this));
					d_deque.erase(it.absolutePos(), d_deque.size());
				}
				d_deque.manager()->update_all_back_values();

				// insert range of value will undirty the tiered_vector
				d_dirty = 0;
			}
			void insert(std::initializer_list<value_type> ilist)
			{
				insert(ilist.begin(), ilist.end());
			}
			template< class Iter >
			void assign(Iter first, Iter last) {
				if (first == last) {
					d_deque.clear();
					return;
				}
				assign_cat(first, last, typename std::iterator_traits<Iter>::iterator_category());
				// insert range of value will undirty the tiered_vector
				d_dirty = 0;
			}
			void erase_pos(size_type pos)
			{
				d_deque.erase(pos);
			}
			auto erase(const_iterator pos) -> iterator
			{
				return d_deque.erase(pos);
			}
			auto erase(iterator pos) -> iterator
			{
				return d_deque.erase(pos);
			}
			auto erase(const_iterator first, const_iterator last) -> iterator
			{
				return d_deque.erase(first, last);
			}
			void erase(size_t first, size_t last)
			{
				d_deque.erase(first, last);
			}
			template< class K>
			auto erase(const K& key) -> size_type
			{
				check_dirty();
				if (Unique) {
					size_t pos = d_deque.binary_search(key, base());
					if (pos == d_deque.size()) return 0;
					d_deque.erase(pos);
					return 1;
				}
				else {
					auto range = equal_range_pos(key);
					if (range.first == range.second)
						return 0;
					size_t count = range.second - range.first;
					erase(range.first, range.second);
					return count;
				}
			}
			template< class K>
			auto find(const K& x) -> iterator 
			{
				check_dirty();
				return d_deque.iterator_at(d_deque.binary_search(x, base()));
			}
			template< class K>
			auto find(const K& x) const -> const_iterator 
			{
				check_dirty();
				return d_deque.iterator_at(d_deque.binary_search(x, base()));
			}
			template< class K>
			auto find_pos(const K& x) const -> size_t 
			{
				check_dirty();
				return (d_deque.binary_search(x, base()));
			}
			SEQ_ALWAYS_INLINE auto pos(size_t i) const noexcept -> const Value&
			{
				return d_deque[i];
			}
			SEQ_ALWAYS_INLINE auto pos(size_t i) noexcept -> Value&
			{
				return d_deque[i];
			}
			template< class K >
			auto lower_bound(const K& x) -> iterator 
			{
				check_dirty();
				return d_deque.iterator_at(d_deque.lower_bound(x, base()));
			}
			template< class K >
			auto lower_bound(const K& x) const -> const_iterator 
			{
				check_dirty();
				return d_deque.iterator_at(d_deque.lower_bound(x, base()));
			}
			template< class K >
			auto lower_bound_pos(const K& x) const -> size_t 
			{
				check_dirty();
				return d_deque.lower_bound(x, base());
			}
			template< class K >
			auto upper_bound(const K& x) -> iterator 
			{
				check_dirty();
				return d_deque.iterator_at(d_deque.upper_bound(x, base()));
			}
			template< class K >
			auto upper_bound(const K& x) const -> const_iterator 
			{
				check_dirty();
				return d_deque.iterator_at(d_deque.upper_bound(x, base()));
			}
			template< class K >
			auto upper_bound_pos(const K& x) const -> size_t 
			{
				check_dirty();
				return d_deque.upper_bound(x, base());
			}
			template< class K >
			auto contains(const K& x) const -> bool 
			{
				check_dirty();
				return d_deque.binary_search(x, base()) != size();
			}
			template< class K >
			auto count(const K& x) const -> size_type 
			{
				check_dirty();
				if (Unique) {
					size_t pos = d_deque.binary_search(x, base());
					return (pos == d_deque.size() ? 0 : 1);
				}
				size_t low = d_deque.lower_bound(x, base());
				if (low == d_deque.size()) return 0;
				return d_deque.upper_bound(x, base()) - low;
			}
			template< class K >
			auto equal_range(const K& x) -> std::pair<iterator, iterator> 
			{
				iterator low = lower_bound(x);
				if (low == end())
					return std::pair<iterator, iterator>(low, low);
				if (Unique) 
					return std::pair<iterator, iterator>(low, low+1);
				iterator up = upper_bound(x);
				return std::pair<iterator, iterator>(low, up);
			}
			template< class K >
			auto equal_range(const K& x) const -> std::pair<const_iterator, const_iterator> 
			{
				const_iterator low = lower_bound(x);
				if (low == end())
					return std::pair<iterator, iterator>(low, low);
				if (Unique) 
					return std::pair<iterator, iterator>(low, low + 1);
				const_iterator up = upper_bound(x);
				return std::pair<iterator, iterator>(low, up);
			}
			template< class K >
			auto equal_range_pos(const K& x) const -> std::pair<size_t, size_t> 
			{
				size_t low = lower_bound_pos(x);
				if (low == size())
					return std::pair<size_t, size_t>(low, low);
				if (Unique)
					return std::pair<size_t, size_t>(low, low + 1);
				size_t up = upper_bound_pos(x);
				return std::pair<size_t, size_t>(low, up);
			}

			template<class C2, LayoutManagement layout2, bool Unique2 >
			void merge(flat_tree<Key, Key, C2, Allocator, layout2, Unique2>& source)
			{
				check_dirty();
				if (Unique) {
					deque_type out;
					typename flat_tree<Key, Key, C2, Allocator, layout2, Unique2>::deque_type rem;
					detail::unique_merge_move(out, begin(), end(), source.begin(), source.end(), Less(this), &rem);
					d_deque = std::move(out);
					source.d_deque = std::move(rem);
				}
				else {
					//add elements from source to this tiered_vector
					size_t before_size = size();
					d_deque.insert(d_deque.end(), source.begin(), source.end());
					std::inplace_merge(d_deque.begin(), d_deque.begin() + before_size, d_deque.end(), Less(this));
					source.clear();
				}
			}

			auto begin() noexcept -> iterator { return d_deque.begin(); }
			auto begin() const noexcept -> const_iterator { return d_deque.cbegin(); }
			auto cbegin() const noexcept -> const_iterator { return d_deque.cbegin(); }

			auto end() noexcept -> iterator { return d_deque.end(); }
			auto end() const noexcept -> const_iterator { return d_deque.cend(); }
			auto cend() const noexcept -> const_iterator { return d_deque.cend(); }

			auto rbegin() noexcept -> reverse_iterator { return d_deque.rbegin(); }
			auto rbegin() const noexcept -> const_reverse_iterator { return d_deque.crbegin(); }
			auto crbegin() const noexcept -> const_reverse_iterator { return d_deque.crbegin(); }

			auto rend() noexcept -> reverse_iterator { return d_deque.rend(); }
			auto rend() const noexcept -> const_reverse_iterator { return d_deque.crend(); }
			auto crend() const noexcept -> const_reverse_iterator { return d_deque.crend(); }

			auto key_comp() const -> key_compare { return static_cast<const key_compare&>(base()); }
			auto value_comp() const -> value_compare { return Less(this); }

			void sort()
			{
				// Sort again the tiered_vector, but only if:
				//	- the flat_tree is dirty (and potentially unsorted),
				//  - and the tiered_vector is not already sorted.

				if (d_deque.empty() || d_dirty == 0)
					return;

				iterator it = d_deque.begin();
				iterator en = d_deque.end();
				bool sorted = true;
				bool unique = true;

				// Check for sorted and uniqueness 
				iterator prev = it++;
				while (it != en && sorted) {
					if ((*this)(*it, *prev))
						sorted = false;
					else if (Unique && !(*this)(*prev, *it))
						unique = false;
					++it;
				}

				// Sort if necessary, remove non unique values if necessary

				if (!sorted) 
					sort_deque<Stable>(d_deque, 0, d_deque.size(), Less(this));
				if (Unique && (!sorted || !unique)){
					iterator it = std::unique(d_deque.begin(), d_deque.end(), Equal(this));
					d_deque.erase(it, d_deque.end());
				}
				d_deque.manager()->update_all_back_values();

				d_dirty = 0;
			}

		};

	}// end namespace detail


	/// @brief flat sorted container similar to boost::flat_set with faster insertion/deletion of single values
	/// @tparam Key key type
	/// @tparam Compare comparison function
	/// @tparam Allocator allocator
	/// @tparam layout memory layout of the underlying seq::tiered_vector
	/// @tparam Stable insertion order stability
	/// 
	/// 
	/// seq::flat_set is a sorted associative container similar to <a href="https://www.boost.org/doc/libs/1_64_0/doc/html/boost/container/flat_set.html">boost::flat_set</a>, 
	/// but relying on a seq::tiered_vector instead of a flat array.
	/// Therefore, it inherits seq::tiered_vector  properties: faster insertion and deletion of individual values 
	/// (thanks to its tiered-vector based implementation), fast iteration, random access, invalidation of references/iterators
	/// on insertion and deletion.
	/// 
	/// All keys in a seq::flat_set are unique.
	/// All references and iterators are invalidated when inserting/removing keys.
	/// 
	/// Interface
	/// ---------
	/// 
	/// seq::flat_set provides a similar interface to std::set with the following differences:
	///		-	The node related functions are not implemented,
	///		-	The member flat_set::pos() is used to access to a random location,
	///		-	The members flat_set::tvector() returns a reference to the underlying seq::tiered_vector object,
	///		-	Its iterator and const_iterator types are random access iterators.
	/// 
	/// The underlying tiered_vector object stores plain non const Key objects. However, in order to avoid modifying the keys
	/// through iterators (and potentially invalidating the order), both iterator and const_iterator types can only return
	/// const references.
	/// 
	/// In addition to members returning (const_)iterator(s), the flat_set provides the same members ending with the '_pos' prefix
	/// and returning positions within the tiered_vector instead of iterators. These functions are slightly faster than the iterator based members.
	/// 
	/// 
	/// Direct access to tiered_vector
	/// ----------------------
	/// 
	/// Unlike most flat set implementations, it it possible to directly access and modify the underlying tiered_vector directly. 
	/// This possibility must be used with great care, as modifying directly the tiered_vector might break the key ordering. 
	/// When calling the non-const version of flat_set::tvector(), the flat_set will be marked as dirty, and further attempts 
	/// to call functions like flat_set::find() of flat_set::lower_bound() (functions based on key ordering) will throw a std::logic_error.
	/// 
	/// Therefore, after finishing modifying the tiered_vector, you must call flat_set::sort() to sort again the tiered_vector, remove potential duplicates,
	/// and mark the flat_set as non dirty anymore.
	/// 
	/// This way of modifying a flat_set must be used carefully, but is way faster than multiple calls to flat_set::insert() of flat_set::erase().
	/// 
	/// 
	/// Range insertion
	/// ---------------
	/// 
	/// Inserting a range of values using flat_set::insert(first, last) is faster than inserting keys one by one, and should be favored if possible.
	/// Range insertion works the following way:
	///		-	New keys are first appended to the underlying tiered_vector
	///		-	These new keys are sorted directly within the tiered_vector
	///		-	Old keys and new keys are merged using std::inplace_merge
	///		-	Duplicate values are removed if necessary using std::unique.
	/// 
	/// Note that flat_set used by default <a href="https://github.com/orlp/pdqsort">pdqsort</a> implementation from Orson Peters which is not stable.
	/// If you need to keep stability when inserting range of values, you must set the Stable template parameter
	/// to true. std::stable_sort will be used instead of pdqsort (std::inplace_merge and std::unique are already stable).
	/// 
	/// 
	/// Exception guarantee
	/// -------------------
	/// 
	/// All seq::flat_set operations only provide <b>basic exception guarantee</b>, exactly like the underlying seq::tiered_vector.
	/// 
	/// 
	/// Performances
	/// ------------
	/// 
	/// Performances of seq::flat_set has been measured and compared to std::set, std::unordered_set, <a href="https://www.boost.org/doc/libs/1_64_0/doc/html/boost/container/flat_set.html">boost::flat_set</a>
	/// and <a href="https://abseil.io/docs/cpp/guides/container">absel::btree_set</a>. The following table show the results when compiled with gcc 10.1.0 (-O3) for msys2 on Windows 10,
	/// using Intel(R) Core(TM) i7-10850H at 2.70GHz. Measured operations are:
	///		-	Insert successfully a range of 1M unique double randomly shuffled using set_class::insert(first,last)
	///		-	Insert successfully 1M unique double randomly shuffled one by one using set_class::insert(const Key&)
	///		-	Insert 1M double randomly shuffled one by one and already present in the set (failed insertion)
	///		-	Successfully search for 1M double in the set using set_class::find(const Key&), or flat_set::find_pos(const Key&)
	///		-	Search for 1M double not present in the set (failed lookup)
	///		-	Walk through the full set (1M double) using iterators
	///		-	Erase all 1M double one by one using set_class::erase(iterator)
	/// 
	/// Set name           | Insert range | Insert (success) | Insert (fail) | Find (success) | Find (fail) | Iterate |  Erase  |
	/// -------------------|--------------|------------------|---------------|----------------|-------------|---------|---------|
	/// seq::flat_set      | 53 ms        |     260 ms       |    96 ms      |    97 ms       |    38 ms    |   1 ms  | 213 ms  |
	/// absl::btree_set    | 98 ms        |     98 ms        |    77 ms      |    82 ms       |    55 ms    |   2 ms  |  94 ms  |
	/// boost::flat_set    | 50 ms        |    8756 ms       |    87 ms      |    94 ms       |    29 ms    |   0 ms  |223106 ms|
	/// std::set           | 252 ms       |     263 ms       |   245 ms      |   306 ms       |    41 ms    |  40 ms  | 333 ms  |
	/// std::unordered_set | 203 ms       |     206 ms       |   107 ms      |   104 ms       |   161 ms    |  34 ms  | 258 ms  |
	/// 
	/// seq::flat_set behaves quite well compared to absl::btree_set or boost::flat_set, and is even faster for single insertion/deletion than std::set.
	/// 
	/// seq::flat_set insertion/deletion perform in O(sqrt(N)) on average, as compared to std::set that performs in O(log(N)).
	/// seq::flat_set is therfore slower for inserting and deleting elements than std::set when dealing with several millions of elements.
	/// Lookup functions (find, lower_bound, upper_bound...) still perform in O(log(N)) and remain faster that std::set couterparts because
	/// seq::tiered_vector is much more cache friendly than std::set. flat_set will always be slower for element lookup than boost::flat_set wich uses
	/// a single dense array.
	/// 
	/// Inserting/removing relocatable types (where seq::is_relocatable<T>::value is true) is faster than for other types.
	/// 
	/// Note that insertion/deletion of single elements become slower when sizeof(Key) increases, where std::set performances remain stable.
	/// Also note that the members using the '_pos' prefix are usually slightly faster than their iterator based counterparts.
	/// 
	template<class Key, class Compare = std::less<Key>, class Allocator = std::allocator<Key>, LayoutManagement layout = OptimizeForMemory, bool Stable = false, bool Unique = true>
	class flat_set
	{
		using flat_tree_type = detail::flat_tree<Key, Key, Compare, Allocator, layout,Stable, Unique>;
		flat_tree_type d_tree;

		template <typename U>
		using has_is_transparent = detail::has_is_transparent<U>;


	public:
		using deque_type = typename flat_tree_type::deque_type;

		/// @brief Random access const iterator class
		struct const_iterator
		{
			using iter_type = typename deque_type::const_iterator;
			using value_type = Key;
			using iterator_category = std::random_access_iterator_tag;
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
			SEQ_ALWAYS_INLINE auto operator*() const noexcept -> reference {return *iter;}
			SEQ_ALWAYS_INLINE auto operator->() const noexcept -> pointer {return  std::pointer_traits<pointer>::pointer_to(**this);}
			SEQ_ALWAYS_INLINE auto operator+=(difference_type diff) noexcept -> const_iterator& {
				iter += diff;
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator-=(difference_type diff) noexcept -> const_iterator& {
				iter -= diff;
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator==(const const_iterator& it) const -> bool { return iter == it.iter; }
			SEQ_ALWAYS_INLINE auto operator!=(const const_iterator& it) const -> bool { return iter != it.iter; }
			SEQ_ALWAYS_INLINE auto operator+(difference_type diff) -> const_iterator { return iter + diff; }
			SEQ_ALWAYS_INLINE auto operator-(difference_type diff) -> const_iterator { return iter + diff; }
			SEQ_ALWAYS_INLINE auto operator-(const const_iterator & other) -> difference_type { return iter - other.iter; }
		};
		using iterator = const_iterator;

		using key_type = Key;
		using value_type = Key;
		using difference_type = typename std::allocator_traits<Allocator>::difference_type;
		using size_type = typename std::allocator_traits<Allocator>::size_type;
		using key_compare = Compare;
		using allocator_type = Allocator;
		using reference = Key&;
		using const_reference = const Key&;
		using pointer = typename std::allocator_traits<Allocator>::pointer;
		using const_pointer = typename std::allocator_traits<Allocator>::const_pointer;
		using reverse_iterator = std::reverse_iterator<iterator>;
		using const_reverse_iterator = std::reverse_iterator<const_iterator>;

		/// @brief Default constructor
		flat_set():d_tree(){}
		/// @brief Construct from comparator function and optional allocator object
		/// @param comp comparison function object to use for all comparisons of keys
		/// @param alloc allocator to use for all memory allocations of this container
		explicit flat_set(const Compare& comp,
			const Allocator& alloc = Allocator())
			:d_tree(comp,alloc) {}
		/// @brief Construct from allocator object
		/// @param alloc allocator to use for all memory allocations of this container
		explicit flat_set(const Allocator& alloc)
			:d_tree(alloc) {}
		/// @brief Range constructor. Constructs the container with the contents of the range [first, last). 
		/// If multiple elements in the range have keys that compare equivalent, it is unspecified which element is inserted,
		/// except if template parameter Stable is true.
		/// @param first the range to copy the elements from
		/// @param last the range to copy the elements from
		/// @param comp comparison function object to use for all comparisons of keys
		/// @param alloc allocator to use for all memory allocations of this container
		template< class InputIt >
		flat_set(InputIt first, InputIt last,
			const Compare& comp = Compare(),
			const Allocator& alloc = Allocator())
			: d_tree(first,last,comp,alloc){}
		/// @brief Range constructor. Constructs the container with the contents of the range [first, last). 
		/// If multiple elements in the range have keys that compare equivalent, it is unspecified which element is inserted,
		/// except if template parameter Stable is true.
		/// @param first the range to copy the elements from
		/// @param last the range to copy the elements from
		/// @param alloc allocator to use for all memory allocations of this container
		template< class InputIt >
		flat_set(InputIt first, InputIt last, const Allocator& alloc)
			: flat_set(first, last, Compare(), alloc) {}
		/// @brief Copy constructor
		/// @param other another container to be used as source to initialize the elements of the container with
		flat_set(const flat_set& other)
			:d_tree(other.d_tree) {}
		/// @brief Copy constructor
		/// @param other another container to be used as source to initialize the elements of the container with
		/// @param alloc allocator to use for all memory allocations of this container
		flat_set(const flat_set& other, const Allocator& alloc)
			:d_tree(other.d_tree, alloc) {}
		/// @brief Move constructor. Constructs the container with the contents of other using move semantics. 
		/// If alloc is not provided, allocator is obtained by move-construction from the allocator belonging to other.
		/// @param other another container to be used as source to initialize the elements of the container with
		flat_set(flat_set&& other) noexcept
			:d_tree(std::move(other.d_tree)) {}
		/// @brief Move constructor. Constructs the container with the contents of other using move semantics. 
		/// If alloc is not provided, allocator is obtained by move-construction from the allocator belonging to other.
		/// @param other another container to be used as source to initialize the elements of the container with
		/// @param alloc allocator to use for all memory allocations of this container
		flat_set(flat_set&& other, const Allocator& alloc) 
			:d_tree(std::move(other.d_tree), alloc) {}
		/// @brief  Initializer-list constructor. Constructs the container with the contents of the initializer list init. 
		/// If multiple elements in the range have keys that compare equivalent, it is unspecified which element is inserted,
		/// except if template parameter Stable is true.
		/// @param init initializer list to initialize the elements of the container with
		/// @param comp comparison function object to use for all comparisons of keys
		/// @param alloc allocator to use for all memory allocations of this container
		flat_set(std::initializer_list<value_type> init,
			const Compare& comp = Compare(),
			const Allocator& alloc = Allocator())
			:flat_set(init.begin(), init.end(), comp, alloc) {}
		/// @brief  Initializer-list constructor. Constructs the container with the contents of the initializer list init. 
		/// If multiple elements in the range have keys that compare equivalent, it is unspecified which element is inserted,
		/// except if template parameter Stable is true.
		/// @param init initializer list to initialize the elements of the container with
		/// @param alloc allocator to use for all memory allocations of this container
		flat_set(std::initializer_list<value_type> init, const Allocator& alloc)
			: flat_set(init, Compare(), alloc) {}

		/// @brief Move assignment operator
		/// @param other another container to be used as source to initialize the elements of the container with
		/// @return reference to this container
		auto operator=(flat_set&& other) noexcept -> flat_set& {
			d_tree = std::move(other.d_tree);
			return *this;
		}
		/// @brief Copy operator
		/// @param other another container to be used as source to initialize the elements of the container with
		/// @return reference to this container
		auto operator=(const flat_set& other) -> flat_set& {
			d_tree = (other.d_tree);
			return *this;
		}
		/// @brief Assign initializer-list
		/// @param init initializer list to initialize the elements of the container with
		/// @return reference to this container
		auto operator=(const std::initializer_list<value_type>& init) -> flat_set&
		{
			d_tree = init;
			return *this;
		}

		/// @brief Returns container's allocator
		SEQ_ALWAYS_INLINE auto get_allocator() const noexcept -> Allocator { return d_tree.get_allocator(); }
		/// @brief Returns true if container is empty, false otherwise
		SEQ_ALWAYS_INLINE auto empty() const noexcept -> bool { return d_tree.empty(); }
		/// @brief Returns the container size
		SEQ_ALWAYS_INLINE auto size() const noexcept -> size_type { return d_tree.size(); }
		/// @brief Returns the container maximum size
		SEQ_ALWAYS_INLINE auto max_size() const noexcept -> size_type { return d_tree.max_size(); }
		/// @brief Clears the container
		SEQ_ALWAYS_INLINE void clear() noexcept { d_tree.clear(); }
		/// @brief Swap this container's content with another. Iterators to both containers remain valid, including end iterators.
		SEQ_ALWAYS_INLINE void swap(flat_set& other) noexcept { d_tree.swap(other.d_tree); }

		/// @brief Returns the underlying tiered_vector.
		/// Calling this function will mark the container as dirty. Any further attempts to call members like find(), lower_bound, upper_bound... will
		/// raise a std::logic_error. To mark the container as non dirty anymore, the user must call flat_set::sort().
		/// @return a reference to the underlying seq::tiered_vector object
		SEQ_ALWAYS_INLINE auto tvector() noexcept -> deque_type& { return d_tree.tvector(); }
		/// @brief Returns a const reference to the underlying tiered_vector.
		/// The container will NOT be marked as dirty.
		SEQ_ALWAYS_INLINE auto tvector() const noexcept -> const deque_type& { return d_tree.tvector();}
		/// @brief Returns a const reference to the underlying tiered_vector.
		/// The container will NOT be marked as dirty.
		SEQ_ALWAYS_INLINE auto ctvector() const noexcept -> const deque_type& { return d_tree.ctvector(); }

		/// @brief See std::set::insert
		SEQ_ALWAYS_INLINE auto insert(const value_type& value) -> std::pair<iterator, bool> {	return d_tree.insert(value);}
		/// @brief See std::set::insert
		SEQ_ALWAYS_INLINE auto insert(value_type&& value) -> std::pair<iterator, bool> { return d_tree.insert(std::move(value)); }

		/// @brief Same as insert(), but returns the inserted object position instead of iterator (or size() if no element was inserted).
		SEQ_ALWAYS_INLINE auto insert_pos(const value_type& value) -> std::pair<size_t, bool> { return d_tree.insert_pos(value); }
		/// @brief Same as insert(), but returns the inserted object position instead of iterator (or size() if no element was inserted).
		SEQ_ALWAYS_INLINE auto insert_pos(value_type&& value) -> std::pair<size_t, bool> { return d_tree.insert_pos(std::move(value)); }

		/// @brief See std::set::emplace().
		template< class... Args >
		SEQ_ALWAYS_INLINE auto emplace(Args&&... args) -> std::pair<iterator, bool> { return d_tree.emplace(std::forward<Args>(args)...); }
		/// @brief Same as emplace(), but returns the inserted object position instead of iterator (or size() if no element was inserted).
		template< class... Args >
		SEQ_ALWAYS_INLINE auto emplace_pos(Args&&... args) -> std::pair<size_t, bool> { return d_tree.emplace_pos(std::forward<Args>(args)...); }
		/// @brief Same as std::set::emplace_hint
		template <class... Args>
		SEQ_ALWAYS_INLINE auto emplace_hint(const_iterator hint, Args&&... args) -> iterator { return d_tree.emplace_hint(hint.iter,std::forward<Args>(args)...); }

		/// @brief Same as std::set::insert()
		SEQ_ALWAYS_INLINE auto insert(const_iterator hint, const value_type& value) -> iterator { return d_tree.insert(hint.iter,value); }
		/// @brief Same as std::set::insert()
		SEQ_ALWAYS_INLINE auto insert(const_iterator hint, value_type&& value) -> iterator { return d_tree.insert(hint.iter, std::move(value)); }

		/// @brief Inserts elements from range [first, last). If multiple elements in the range have keys that compare equivalent, 
		/// it is unspecified which element is inserted, except if template parameter Stable is true.
		/// This function will insert the new elements at the end of the underlying tiered_vector, sort them, inplace merge the old and new key,
		/// and remove potential duplicates. If the number of elements in the range [first, last) is roughly equal or greater than the container's size,
		/// this function is much faster than inserting elements one by one.
		template< class InputIt >
		SEQ_ALWAYS_INLINE void insert(InputIt first, InputIt last) { return d_tree.insert(first, last); }
		/// @brief Inserts elements from initializer list ilist. If multiple elements in the range have keys that compare equivalent, 
		/// it is unspecified which element is inserted, except if template parameter Stable is true.
		SEQ_ALWAYS_INLINE void insert(std::initializer_list<value_type> ilist) { return d_tree.insert(ilist); }

		/// @brief Assign the content of the range [first, last) to this container, discarding previous values.
		template< class Iter >
		SEQ_ALWAYS_INLINE void assign(Iter first, Iter last) {d_tree.assign(first, last);}

		/// @brief Erase element at given position in the container. This is slightly faster than calling erase(iterator).
		SEQ_ALWAYS_INLINE void erase_pos(size_type pos) { d_tree.erase_pos(pos); }
		/// @brief Erase element at given location.
		SEQ_ALWAYS_INLINE auto erase(const_iterator pos) -> iterator{return d_tree.erase(pos.iter);}
		/// @brief Erase elements in the range [first, last)
		SEQ_ALWAYS_INLINE auto erase(const_iterator first, const_iterator last) -> iterator{ return d_tree.erase(first.iter, last.iter);}
		/// @brief Erase elements in the range [first, last)
		SEQ_ALWAYS_INLINE void erase(size_t first, size_t last) { d_tree.erase(first, last); }

		/// @brief Removes the element (if one exists) with key that compares equivalent to the value key. 
		/// This overload participates in overload resolution only if the qualified-id Compare::is_transparent is valid and denotes a type,
		/// and neither iterator nor const_iterator is implicitly convertible from K. 
		/// It allows calling this function without constructing an instance of Key.
		/// @param key key value of the elements to remove
		/// @return number of erased elements
		template <class K, class LE = Compare, typename std::enable_if<has_is_transparent<LE>::value>::type* = nullptr>
		SEQ_ALWAYS_INLINE	auto erase(const K& key) -> size_type{return d_tree.erase(key);}
		/// @brief Removes the element (if one exists) with the key equivalent to key
		/// @param key key value of the elements to remove
		/// @return number of erased elements
		SEQ_ALWAYS_INLINE auto erase(const Key& key) -> size_type {return d_tree.erase(key);}
	
		/// @brief Finds an element with key that compares equivalent to the value key. 
		/// This overload participates in overload resolution only if the qualified-id Compare::is_transparent is valid and denotes a type. 
		/// It allows calling this function without constructing an instance of Key.
		/// @param key value to look for
		/// @return iterator pointing to found element, or end iterator
		template <class K, class LE = Compare, typename std::enable_if<has_is_transparent<LE>::value>::type* = nullptr>
		SEQ_ALWAYS_INLINE auto find(const K& key) -> iterator{return d_tree.find(key);}
		/// @brief Finds an element with key equivalent to key.
		/// @param key value to look for
		/// @return iterator pointing to found element, or end iterator
		SEQ_ALWAYS_INLINE auto find(const Key& key) -> iterator  { return d_tree.find(key); }

		/// @brief Finds an element with key that compares equivalent to the value key. 
		/// This overload participates in overload resolution only if the qualified-id Compare::is_transparent is valid and denotes a type. 
		/// It allows calling this function without constructing an instance of Key.
		/// @param key value to look for
		/// @return iterator pointing to found element, or end iterator
		template <class K, class LE = Compare, typename std::enable_if<has_is_transparent<LE>::value>::type* = nullptr>
		SEQ_ALWAYS_INLINE auto find(const K& key) const -> const_iterator { return d_tree.find(key); }
		/// @brief Finds an element with key equivalent to key.
		/// @param key value to look for
		/// @return iterator pointing to found element, or end iterator
		SEQ_ALWAYS_INLINE auto find(const Key& x) const -> const_iterator  { return d_tree.find(x); }
	
		/// @brief Finds an element with key that compares equivalent to the value key. 
		/// This overload participates in overload resolution only if the qualified-id Compare::is_transparent is valid and denotes a type. 
		/// It allows calling this function without constructing an instance of Key.
		/// @param key value to look for
		/// @return position of found value, or size()
		template <class K, class LE = Compare, typename std::enable_if<has_is_transparent<LE>::value>::type* = nullptr>
		SEQ_ALWAYS_INLINE auto find_pos(const K& key) const -> size_t {return d_tree.find_pos(key);}
		/// @brief Finds an element with key equivalent to key.
		/// @param key value to look for
		/// @return position of found value, or size()
		SEQ_ALWAYS_INLINE auto find_pos(const Key& x) const -> size_t  { return d_tree.find_pos(x); }

		/// @brief Returns value at given flat position
		SEQ_ALWAYS_INLINE auto pos(size_t i) const noexcept -> const value_type& { return d_tree.pos(i); }
	

		/// @brief Returns an iterator pointing to the first element that compares not less (i.e. greater or equal) to the value key. 
		/// This overload participates in overload resolution only if the qualified-id Compare::is_transparent is valid and denotes a type. 
		/// It allows calling this function without constructing an instance of Key.
		template <class K, class LE = Compare, typename std::enable_if<has_is_transparent<LE>::value>::type* = nullptr>
		SEQ_ALWAYS_INLINE auto lower_bound(const K& key) -> iterator  { return d_tree.lower_bound(key); }
		/// @brief Returns an iterator pointing to the first element that is not less than (i.e. greater or equal to) key.
		SEQ_ALWAYS_INLINE auto lower_bound(const Key& key) -> iterator  { return d_tree.lower_bound(key); }
	
		/// @brief Returns an iterator pointing to the first element that compares not less (i.e. greater or equal) to the value key. 
		/// This overload participates in overload resolution only if the qualified-id Compare::is_transparent is valid and denotes a type. 
		/// It allows calling this function without constructing an instance of Key.
		template <class K, class LE = Compare, typename std::enable_if<has_is_transparent<LE>::value>::type* = nullptr>
		SEQ_ALWAYS_INLINE auto lower_bound(const K& key) const -> const_iterator  { return d_tree.lower_bound(key); }
		/// @brief Returns an iterator pointing to the first element that is not less than (i.e. greater or equal to) key.
		SEQ_ALWAYS_INLINE auto lower_bound(const Key& key) const -> const_iterator  { return d_tree.lower_bound(key); }

		/// @brief Returns the position of the first element that compares not less (i.e. greater or equal) to the value key. 
		/// This overload participates in overload resolution only if the qualified-id Compare::is_transparent is valid and denotes a type. 
		/// It allows calling this function without constructing an instance of Key.
		template <class K, class LE = Compare, typename std::enable_if<has_is_transparent<LE>::value>::type* = nullptr>
		SEQ_ALWAYS_INLINE auto lower_bound_pos(const K& key) const -> size_t  { return d_tree.lower_bound_pos(key); }
		/// @brief Returns the position of the first element that is not less than (i.e. greater or equal to) key.
		SEQ_ALWAYS_INLINE auto lower_bound_pos(const Key& key) const -> size_t  { return d_tree.lower_bound_pos(key); }
	

		/// @brief Returns an iterator pointing to the first element that compares greater to the value key. 
		/// This overload participates in overload resolution only if the qualified-id Compare::is_transparent is valid and denotes a type. 
		/// It allows calling this function without constructing an instance of Key.
		template <class K, class LE = Compare, typename std::enable_if<has_is_transparent<LE>::value>::type* = nullptr>
		SEQ_ALWAYS_INLINE auto upper_bound(const K& key) -> iterator  { return d_tree.upper_bound(key); }
		/// @brief Returns an iterator pointing to the first element that is greater than key.
		SEQ_ALWAYS_INLINE auto upper_bound(const Key& key) -> iterator  { return d_tree.upper_bound(key); }
	
		/// @brief Returns an iterator pointing to the first element that compares greater to the value key. 
		/// This overload participates in overload resolution only if the qualified-id Compare::is_transparent is valid and denotes a type. 
		/// It allows calling this function without constructing an instance of Key.
		template <class K, class LE = Compare, typename std::enable_if<has_is_transparent<LE>::value>::type* = nullptr>
		SEQ_ALWAYS_INLINE auto upper_bound(const K& key) const -> const_iterator  { return d_tree.upper_bound(key); }
		/// @brief Returns an iterator pointing to the first element that is greater than key.
		SEQ_ALWAYS_INLINE auto upper_bound(const Key& key) const -> const_iterator  { return d_tree.upper_bound(key); }

		/// @brief Returns the position of the first element that compares greater to the value key. 
		/// This overload participates in overload resolution only if the qualified-id Compare::is_transparent is valid and denotes a type. 
		/// It allows calling this function without constructing an instance of Key.
		template <class K, class LE = Compare, typename std::enable_if<has_is_transparent<LE>::value>::type* = nullptr>
		SEQ_ALWAYS_INLINE auto upper_bound_pos(const K& key) const -> size_t  { return d_tree.upper_bound_pos(key); }
		/// @brief Returns the position of the first element that is greater than key.
		SEQ_ALWAYS_INLINE auto upper_bound_pos(const Key& key) const -> size_t  { return d_tree.upper_bound_pos(key); }
	
	
		/// @brief Checks if there is an element with key that compares equivalent to the value key.
		/// This overload participates in overload resolution only if the qualified-id Compare::is_transparent is valid and denotes a type. 
		/// It allows calling this function without constructing an instance of Key.
		template <class K, class LE = Compare, typename std::enable_if<has_is_transparent<LE>::value>::type* = nullptr>
		SEQ_ALWAYS_INLINE auto contains(const K& key) const -> bool  { return d_tree.contains(key); }
		/// @brief Checks if there is an element with key equivalent to key in the container.
		SEQ_ALWAYS_INLINE auto contains(const Key& key) const -> bool  { return d_tree.contains(key); }
	

		/// @brief Returns the number of elements with key key (either 1 or 0 for flat_set and flat_map).
		/// This overload participates in overload resolution only if the qualified-id Compare::is_transparent is valid and denotes a type. 
		/// It allows calling this function without constructing an instance of Key.
		template <class K, class LE = Compare, typename std::enable_if<has_is_transparent<LE>::value>::type* = nullptr>
		SEQ_ALWAYS_INLINE auto count(const K& key) const -> size_type  { return d_tree.count(key); }
		/// @brief Returns the number of elements with key key (either 1 or 0 for flat_set and flat_map).
		SEQ_ALWAYS_INLINE auto count(const Key& key) const -> size_type  { return d_tree.count(key); }
	
		/// @brief Returns a range containing all elements with the given key in the container. 
		/// The range is defined by two iterators, one pointing to the first element that is not less than key and another pointing to the first element greater than key.
		/// Alternatively, the first iterator may be obtained with lower_bound(), and the second with upper_bound().
		/// 
		/// This overload participates in overload resolution only if the qualified-id Compare::is_transparent is valid and denotes a type. 
		/// It allows calling this function without constructing an instance of Key.
		template <class K, class LE = Compare, typename std::enable_if<has_is_transparent<LE>::value>::type* = nullptr>
		SEQ_ALWAYS_INLINE auto equal_range(const K& key) -> std::pair<iterator, iterator>  { return d_tree.equal_range(key); }
		/// @brief Returns a range containing all elements with the given key in the container. 
		/// The range is defined by two iterators, one pointing to the first element that is not less than key and another pointing to the first element greater than key.
		/// Alternatively, the first iterator may be obtained with lower_bound(), and the second with upper_bound().
		SEQ_ALWAYS_INLINE auto equal_range(const Key& key) -> std::pair<iterator, iterator>  { return d_tree.equal_range(key); }
	
		/// @brief Returns a range containing all elements with the given key in the container. 
		/// The range is defined by two iterators, one pointing to the first element that is not less than key and another pointing to the first element greater than key.
		/// Alternatively, the first iterator may be obtained with lower_bound(), and the second with upper_bound().
		/// 
		/// This overload participates in overload resolution only if the qualified-id Compare::is_transparent is valid and denotes a type. 
		/// It allows calling this function without constructing an instance of Key.
		template <class K, class LE = Compare, typename std::enable_if<has_is_transparent<LE>::value>::type* = nullptr>
		SEQ_ALWAYS_INLINE auto equal_range(const K& key) const -> std::pair<const_iterator, const_iterator>  { return d_tree.equal_range(key); }
		/// @brief Returns a range containing all elements with the given key in the container. 
		/// The range is defined by two iterators, one pointing to the first element that is not less than key and another pointing to the first element greater than key.
		/// Alternatively, the first iterator may be obtained with lower_bound(), and the second with upper_bound().
		SEQ_ALWAYS_INLINE auto equal_range(const Key& key) const -> std::pair<const_iterator, const_iterator>  { return d_tree.equal_range(key); }

		/// @brief Returns a range containing all elements with the given key in the container. 
		/// The range is defined by two position, one pointing to the first element that is not less than key and another pointing to the first element greater than key.
		/// Alternatively, the first iterator may be obtained with lower_bound_pos(), and the second with upper_bound_pos().
		/// 
		/// This overload participates in overload resolution only if the qualified-id Compare::is_transparent is valid and denotes a type. 
		/// It allows calling this function without constructing an instance of Key.
		template <class K, class LE = Compare, typename std::enable_if<has_is_transparent<LE>::value>::type* = nullptr>
		SEQ_ALWAYS_INLINE auto equal_range_pos(const K& key) const -> std::pair<size_t, size_t>  { return d_tree.equal_range_pos(key); }

		/// @brief Returns a range containing all elements with the given key in the container. 
		/// The range is defined by two position, one pointing to the first element that is not less than key and another pointing to the first element greater than key.
		/// Alternatively, the first iterator may be obtained with lower_bound_pos(), and the second with upper_bound_pos().
		SEQ_ALWAYS_INLINE auto equal_range_pos(const Key& key) const -> std::pair<size_t, size_t>  { return d_tree.equal_range_pos(key); }

		/// @brief Attempts to extract each element in source and insert it into this using the comparison object of this.
		/// If there is an element in this with key equivalent to the key of an element from source, then that element is not extracted from source.
		/// Note that elements from source are moved to this.
		template<class C2, LayoutManagement layout2 >
		SEQ_ALWAYS_INLINE void merge(flat_set<Key, C2, Allocator, layout2>& source) { d_tree.merge(source.d_tree); }

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
		SEQ_ALWAYS_INLINE auto rbegin() noexcept -> reverse_iterator { return d_tree.rbegin(); }
		/// @brief Returns a reverse iterator to the first element of the reversed list.
		SEQ_ALWAYS_INLINE auto rbegin() const noexcept -> const_reverse_iterator { return d_tree.crbegin(); }
		/// @brief Returns a reverse iterator to the first element of the reversed list.
		SEQ_ALWAYS_INLINE auto crbegin() const noexcept -> const_reverse_iterator { return d_tree.crbegin(); }

		/// @brief Returns a reverse iterator to the element following the last element of the reversed list.
		SEQ_ALWAYS_INLINE auto rend() noexcept -> reverse_iterator { return d_tree.rend(); }
		/// @brief Returns a reverse iterator to the element following the last element of the reversed list.
		SEQ_ALWAYS_INLINE auto rend() const noexcept -> const_reverse_iterator { return d_tree.crend(); }
		/// @brief Returns a reverse iterator to the element following the last element of the reversed list.
		SEQ_ALWAYS_INLINE auto crend() const noexcept -> const_reverse_iterator { return d_tree.crend(); }

		/// @brief Returns the comparison object
		SEQ_ALWAYS_INLINE auto key_comp() const -> key_compare { return d_tree.key_comp(); }

		/// @brief Sort the container.
		/// This function sort again the container only if:
		///		-	The container is dirty (because of flat_set::tvector() calls) and
		///		-	the container is not already sorted.
		/// This function also remove duplicates if necessary.
		SEQ_ALWAYS_INLINE void sort() {d_tree.sort();}
	};


	/// @brief flat sorted container similar to boost::flat_multiset with faster insertion/deletion of single values
	/// @tparam Key key type
	/// @tparam Compare comparison function
	/// @tparam Allocator allocator
	/// @tparam layout memory layout of the underlying seq::tiered_vector
	/// @tparam Stable insertion order stability
	/// 
	/// 
	/// seq::flat_multiset is a sorted associative container similar to <a href="https://www.boost.org/doc/libs/1_64_0/doc/html/boost/container/flat_multiset.html">boost::flat_multiset</a>, 
	/// but relying on a seq::tiered_vector instead of a flat array. It supports multiple equal keys.
	/// 
	/// seq::flat_multiset directly inherits #seq::flat_set and behaves similarly, except for the support of multiple equal keys.
	/// Its interface is similar to std::multiset, except for the node based members.
	/// 
	/// All references and iterators are invalidated when inserting/removing keys.
	/// 
	template<class Key, class Compare = std::less<Key>, class Allocator = std::allocator<Key>, LayoutManagement layout = OptimizeForMemory, bool Stable = false>
	class flat_multiset : public flat_set<Key,Compare,Allocator,layout, Stable, false>
	{
		using base_type = flat_set<Key, Compare, Allocator, layout, Stable, false>;
	public:
		using key_type = Key;
		using value_type = Key;
		using difference_type = typename std::allocator_traits<Allocator>::difference_type;
		using size_type = typename std::allocator_traits<Allocator>::size_type;
		using key_compare = Compare;
		using allocator_type = Allocator;
		using reference = Key&;
		using const_reference = const Key&;
		using pointer = typename std::allocator_traits<Allocator>::pointer;
		using const_pointer = typename std::allocator_traits<Allocator>::const_pointer;
		using iterator = typename base_type::iterator;
		using const_iterator = typename base_type::const_iterator;
		using reverse_iterator = typename base_type::reverse_iterator;
		using const_reverse_iterator = typename base_type::const_reverse_iterator;

		flat_multiset() {}
		explicit flat_multiset(const Compare& comp,
			const Allocator& alloc = Allocator())
			:base_type(comp, alloc) {}
		explicit flat_multiset(const Allocator& alloc)
			:base_type(alloc) {}
		template< class InputIt >
		flat_multiset(InputIt first, InputIt last,
			const Compare& comp = Compare(),
			const Allocator& alloc = Allocator())
			: base_type(first, last, comp, alloc) {}
		template< class InputIt >
		flat_multiset(InputIt first, InputIt last, const Allocator& alloc)
			: flat_multiset(first, last, Compare(), alloc) {}
		flat_multiset(const flat_multiset& other)
			:base_type(other) {}

		flat_multiset(const flat_multiset& other, const Allocator& alloc)
			:base_type(other, alloc) {}
		flat_multiset(flat_multiset&& other) noexcept
			:base_type(std::move(other)) {}
		flat_multiset(flat_multiset&& other, const Allocator& alloc) 
			:base_type(std::move(other), alloc) {}
		flat_multiset(std::initializer_list<value_type> init,
			const Compare& comp = Compare(),
			const Allocator& alloc = Allocator())
			:flat_multiset(init.begin(), init.end(), comp, alloc) {}
		flat_multiset(std::initializer_list<value_type> init, const Allocator& alloc)
			: flat_multiset(init, Compare(), alloc) {}

		auto operator=(flat_multiset&& other) noexcept -> flat_multiset& {
			(base_type&)(*this) = std::move((base_type&)other);
			return *this;
		}
		auto operator=(const flat_multiset& other) -> flat_multiset& {
			(base_type&)(*this) = (const base_type&)(other);
			return *this;
		}
		auto operator=(const std::initializer_list<value_type>& init) -> flat_multiset&
		{
			(base_type&)(this) = init;
			return *this;
		}

		using base_type::insert;

		SEQ_ALWAYS_INLINE auto insert(const value_type& value) -> iterator { return base_type::insert(value).first; }
		SEQ_ALWAYS_INLINE auto insert(value_type&& value) -> iterator { return base_type::insert(std::move(value)).first; }

		SEQ_ALWAYS_INLINE auto insert_pos(const value_type& value) -> size_t {return base_type::insert_pos(value).first; }
		SEQ_ALWAYS_INLINE auto insert_pos(value_type&& value) -> size_t { return base_type::insert_pos(std::move(value)).first; }

		template< class... Args >
		SEQ_ALWAYS_INLINE auto emplace(Args&&... args) -> iterator { return base_type::emplace(std::forward<Args>(args)...).first; }
		template< class... Args >
		SEQ_ALWAYS_INLINE auto emplace_pos(Args&&... args) -> size_t { return base_type::emplace_pos(std::forward<Args>(args)...).first; }

	};




	/// @brief flat sorted associative container that contains key-value pairs with unique keys similar to boost::flat_map with faster insertion/deletion of single values
	/// @tparam Key key type
	/// @tparam T mapped type
	/// @tparam Compare comparison function
	/// @tparam Allocator allocator
	/// @tparam layout memory layout of the underlying seq::tiered_vector
	/// @tparam Stable insertion order stability
	/// 
	/// 
	/// seq::flat_map is a sorted associative container similar to <a href="https://www.boost.org/doc/libs/1_64_0/doc/html/boost/container/flat_map.html">boost::flat_map</a>, 
	/// but relying on a seq::tiered_vector instead of a flat array.
	/// 
	/// seq::flat_map behaves like #seq::flat_set except that the underlying tiered_vector stores elements of types std::pair<Key,T> instead of Key.
	/// Its interface is similar to std::map, except for the node based members.
	/// 
	/// All references and iterators are invalidated when inserting/removing elements.
	/// 
	/// Unlike std::map which stores objects of type std::pair<const Key,T>, flat_map stores objects of type  std::pair<Key,T>.
	/// To avoid modifying the key value through iterators or using flat_map::pos(), the value is reinterpreted to std::pair<const Key,T>.
	/// Although quite ugly, this solution works on all tested compilers.
	/// 
	template<class Key, class T, class Compare = std::less<Key>, class Allocator = std::allocator<std::pair<Key,T> >, LayoutManagement layout = OptimizeForMemory, bool Stable = false, bool Unique = true>
	class flat_map
	{
		using flat_tree_type = detail::flat_tree<Key, std::pair<Key, T> , Compare, Allocator, layout, Stable, Unique>;
		flat_tree_type d_tree;

		template <typename U>
		using has_is_transparent = detail::has_is_transparent<U>;


	public:
		using deque_type = typename flat_tree_type::deque_type;

		struct const_iterator
		{
			using iter_type = typename deque_type::const_iterator;
			using value_type = std::pair<const Key, T>;
			using iterator_category = std::random_access_iterator_tag;
			using size_type = size_t;
			using difference_type = std::ptrdiff_t;
			using pointer = const value_type*;
			using reference = const value_type&;
			iter_type iter;

			const_iterator() {}
			const_iterator(const iter_type & it) : iter(it) {}
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
			SEQ_ALWAYS_INLINE auto operator+=(difference_type diff) noexcept -> const_iterator& {
				iter += diff;
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator-=(difference_type diff) noexcept -> const_iterator& {
				iter -= diff;
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator==(const const_iterator& it) const -> bool { return iter == it.iter; }
			SEQ_ALWAYS_INLINE auto operator!=(const const_iterator& it) const -> bool { return iter != it.iter; }
			SEQ_ALWAYS_INLINE auto operator+(difference_type diff) -> const_iterator { return iter + diff; }
			SEQ_ALWAYS_INLINE auto operator-(difference_type diff) -> const_iterator { return iter + diff; }
			SEQ_ALWAYS_INLINE auto operator-(const const_iterator& other) -> difference_type { return iter - other.iter; }
		};
		struct iterator : public const_iterator
		{
			using iter_type = typename deque_type::const_iterator;
			using value_type = std::pair<const Key, T>;
			using iterator_category = std::random_access_iterator_tag;
			using size_type = size_t;
			using difference_type = std::ptrdiff_t;
			using pointer = value_type*;
			using reference = value_type&;

			iterator() : const_iterator() {}
			iterator(const iter_type & it) : const_iterator(it)  {}
			iterator(const const_iterator & it) : const_iterator(it) {}
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
			SEQ_ALWAYS_INLINE auto operator+=(difference_type diff) noexcept -> iterator& {
				this->iter += diff;
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator-=(difference_type diff) noexcept -> iterator& {
				this->iter -= diff;
				return *this;
			}
			SEQ_ALWAYS_INLINE auto operator==(const const_iterator& it) const -> bool { return this->iter == it.iter; }
			SEQ_ALWAYS_INLINE auto operator!=(const const_iterator& it) const -> bool { return this->iter != it.iter; }
			SEQ_ALWAYS_INLINE auto operator+(difference_type diff) -> iterator { return this->iter + diff; }
			SEQ_ALWAYS_INLINE auto operator-(difference_type diff) -> iterator { return this->iter + diff; }
			SEQ_ALWAYS_INLINE auto operator-(const const_iterator& other) -> difference_type { return this->iter - other.iter; }
		};


		using key_type = Key;
		using mapped_type = T;
		using value_type = std::pair<Key,T>;
		using difference_type = typename std::allocator_traits<Allocator>::difference_type;
		using size_type = typename std::allocator_traits<Allocator>::size_type;
		using key_compare = Compare;
		using value_compare = typename flat_tree_type::value_compare;
		using allocator_type = Allocator;
		using reference = value_type&;
		using const_reference = const value_type&;
		using pointer = typename std::allocator_traits<Allocator>::pointer;
		using const_pointer = typename std::allocator_traits<Allocator>::const_pointer;
		using reverse_iterator = std::reverse_iterator<iterator>;
		using const_reverse_iterator = std::reverse_iterator<const_iterator>;

		flat_map() {}
		explicit flat_map(const Compare& comp,
			const Allocator& alloc = Allocator())
			:d_tree(comp, alloc) {}
		explicit flat_map(const Allocator& alloc)
			:d_tree(alloc) {}
		template< class InputIt >
		flat_map(InputIt first, InputIt last,
			const Compare& comp = Compare(),
			const Allocator& alloc = Allocator())
			: d_tree(first, last, comp, alloc) {}
		template< class InputIt >
		flat_map(InputIt first, InputIt last, const Allocator& alloc)
			: flat_map(first, last, Compare(), alloc) {}
		flat_map(const flat_map& other)
			:d_tree(other.d_tree) {}

		flat_map(const flat_map& other, const Allocator& alloc)
			:d_tree(other.d_tree, alloc) {}
		flat_map(flat_map&& other) noexcept
			:d_tree(std::move(other.d_tree)) {}
		flat_map(flat_map&& other, const Allocator& alloc) 
			:d_tree(std::move(other.d_tree), alloc) {}
		flat_map(std::initializer_list<value_type> init,
			const Compare& comp = Compare(),
			const Allocator& alloc = Allocator())
			:flat_map(init.begin(), init.end(), comp, alloc) {}
		flat_map(std::initializer_list<value_type> init, const Allocator& alloc)
			: flat_map(init, Compare(), alloc) {}

		auto operator=(flat_map&& other) noexcept -> flat_map& {
			d_tree = std::move(other.d_tree);
			return *this;
		}
		auto operator=(const flat_map& other) -> flat_map& {
			d_tree = (other.d_tree);
			return *this;
		}
		auto operator=(const std::initializer_list<value_type>& init) -> flat_map&
		{
			d_tree = init;
			return *this;
		}

		SEQ_ALWAYS_INLINE auto get_allocator() const noexcept -> Allocator { return d_tree.get_allocator(); }
		SEQ_ALWAYS_INLINE auto empty() const noexcept -> bool { return d_tree.empty(); }
		SEQ_ALWAYS_INLINE auto size() const noexcept -> size_type { return d_tree.size(); }
		SEQ_ALWAYS_INLINE auto max_size() const noexcept -> size_type { return d_tree.max_size(); }
		SEQ_ALWAYS_INLINE void clear() noexcept { d_tree.clear(); }
		SEQ_ALWAYS_INLINE void swap(flat_map& other) noexcept { d_tree.swap(other.d_tree); }

		SEQ_ALWAYS_INLINE auto tvector() noexcept -> deque_type& { return d_tree.tvector(); }
		SEQ_ALWAYS_INLINE auto tvector() const noexcept -> const deque_type& { return d_tree.tvector(); }
		SEQ_ALWAYS_INLINE auto ctvector() const noexcept -> const deque_type& { return d_tree.ctvector(); }

		SEQ_ALWAYS_INLINE auto insert(const value_type& value) -> std::pair<iterator, bool> { return d_tree.insert(value); }
		SEQ_ALWAYS_INLINE auto insert(value_type&& value) -> std::pair<iterator, bool> { return d_tree.insert(std::move(value)); }
		template< class P >
		SEQ_ALWAYS_INLINE auto insert(P&& value) -> std::pair<iterator, bool>{return d_tree.insert(std::forward<P>(value));}
		SEQ_ALWAYS_INLINE auto insert(const_iterator hint, const value_type& value) -> iterator { return d_tree.insert(hint.iter, value); }
		SEQ_ALWAYS_INLINE auto insert(const_iterator hint, value_type&& value) -> iterator { return d_tree.insert(hint.iter, std::move(value)); }
		template< class P >
		auto insert(const_iterator hint, P&& value) -> iterator { return d_tree.insert(hint.iter, std::forward<P>(value)); }

		SEQ_ALWAYS_INLINE auto insert_pos(const value_type& value) -> std::pair<size_t, bool> { return d_tree.insert_pos(value); }
		SEQ_ALWAYS_INLINE auto insert_pos(value_type&& value) -> std::pair<size_t, bool> { return d_tree.insert_pos(std::move(value)); }
		template< class P >
		SEQ_ALWAYS_INLINE auto insert_pos(P&& value) -> std::pair<size_t, bool> { return d_tree.insert_pos(std::forward<P>(value)); }

		template< class... Args >
		SEQ_ALWAYS_INLINE auto emplace(Args&&... args) -> std::pair<iterator, bool> { return d_tree.emplace(std::forward<Args>(args)...); }
		template <class... Args>
		SEQ_ALWAYS_INLINE auto emplace_hint(const_iterator hint, Args&&... args) -> iterator { return d_tree.emplace_hint(hint.iter, std::forward<Args>(args)...); }
		template< class... Args >
		SEQ_ALWAYS_INLINE auto emplace_pos(Args&&... args) -> std::pair<size_t, bool> { return d_tree.emplace_pos(std::forward<Args>(args)...); }


		template< class... Args >
		auto try_emplace(const Key& k, Args&&... args) -> std::pair<iterator, bool>
		{
			size_t pos = d_tree.lower_bound_pos(k);
			if (pos != size() && !d_tree.key_comp()(k, this->pos(pos).first))//!(value < tiered_vector[pos]))
				return std::pair<iterator, bool>(d_tree.d_deque.iterator_at(pos), false);
			else {
				d_tree.d_deque.insert(pos, value_type(std::piecewise_construct,
					std::forward_as_tuple(k),
					std::forward_as_tuple(std::forward<Args>(args)...)));
				return std::pair<iterator, bool>(d_tree.d_deque.iterator_at(pos), true);
			}
		}
		template< class... Args >
		auto try_emplace(Key&& k, Args&&... args) -> std::pair<iterator, bool>
		{
			size_t pos = d_tree.lower_bound_pos(k);
			if (pos != size() && !d_tree.key_comp()(k, this->pos(pos).first))//!(value < tiered_vector[pos]))
				return std::pair<iterator, bool>(d_tree.d_deque.iterator_at(pos), false);
			else {
				d_tree.d_deque.insert(pos, value_type(std::piecewise_construct,
					std::forward_as_tuple(std::move(k)),
					std::forward_as_tuple(std::forward<Args>(args)...)));
				return std::pair<iterator, bool>(d_tree.d_deque.iterator_at(pos), true);
			}
		}
		template< class... Args >
		auto try_emplace_pos(const Key& k, Args&&... args) -> std::pair<size_t, bool>
		{
			size_t pos = d_tree.lower_bound_pos(k);
			if (pos != size() && !d_tree.key_comp()(k, this->pos(pos).first))//!(value < tiered_vector[pos]))
				return std::pair<size_t, bool>(pos, false);
			else {
				d_tree.d_deque.insert(pos, value_type(std::piecewise_construct,
					std::forward_as_tuple(k),
					std::forward_as_tuple(std::forward<Args>(args)...)));
				return std::pair<size_t, bool>(pos, true);
			}
		}
		template< class... Args >
		auto try_emplace_pos(Key&& k, Args&&... args) -> std::pair<size_t, bool>
		{
			size_t pos = d_tree.lower_bound_pos(k);
			if (pos != size() && !d_tree.key_comp()(k, this->pos(pos).first))//!(value < tiered_vector[pos]))
				return std::pair<size_t, bool>(pos, false);
			else {
				d_tree.d_deque.insert(pos, value_type(std::piecewise_construct,
					std::forward_as_tuple(std::move(k)),
					std::forward_as_tuple(std::forward<Args>(args)...)));
				return std::pair<size_t, bool>(pos, true);
			}
		}
		template< class... Args >
		auto try_emplace(const_iterator hint, const Key& k, Args&&... args) -> iterator
		{
			(void)hint;
			return try_emplace(k, std::forward<Args>(args)...).first;
		}
		template< class... Args >
		auto try_emplace(const_iterator hint, Key&& k, Args&&... args) -> iterator
		{
			(void)hint;
			return try_emplace(std::move(k), std::forward<Args>(args)...).first;
		}


		template <class M>
		auto insert_or_assign(const Key& k, M&& obj) -> std::pair<iterator, bool>
		{
			size_t pos = d_tree.lower_bound_pos(k);
			if (pos != size() && !d_tree.key_comp()(k, this->pos(pos).first)) {
				d_tree.pos(pos).second = std::forward<M>(obj);
				return std::pair<iterator, bool>(d_tree.d_deque.iterator_at(pos), false);
			}
			else {
				d_tree.d_deque.insert(pos, value_type(k, std::forward<M>(obj)));
				return std::pair<iterator, bool>(d_tree.d_deque.iterator_at(pos), true);
			}
		}
		template <class M>
		auto insert_or_assign(Key&& k, M&& obj) -> std::pair<iterator, bool>
		{
			size_t pos = d_tree.lower_bound_pos(k);
			if (pos != size() && !d_tree.key_comp()(k, this->pos(pos).first)) {
				d_tree.pos(pos).second = std::forward<M>(obj);
				return std::pair<iterator, bool>(d_tree.d_deque.iterator_at(pos), false);
			}
			else {
				d_tree.d_deque.insert(pos, value_type(std::move(k), std::forward<M>(obj)));
				return std::pair<iterator, bool>(d_tree.d_deque.iterator_at(pos), true);
			}
		}
		template <class M>
		auto insert_or_assign_pos(const Key& k, M&& obj) -> std::pair<size_t, bool>
		{
			size_t pos = d_tree.lower_bound_pos(k);
			if (pos != size() && !d_tree.key_comp()(k, this->pos(pos).first)) {
				d_tree.pos(pos).second = std::forward<M>(obj);
				return std::pair<size_t, bool>((pos), false);
			}
			else {
				d_tree.d_deque.insert(pos, value_type(k, std::forward<M>(obj)));
				return std::pair<size_t, bool>((pos), true);
			}
		}
		template <class M>
		auto insert_or_assign_pos(Key&& k, M&& obj) -> std::pair<size_t, bool>
		{
			size_t pos = d_tree.lower_bound_pos(k);
			if (pos != size() && !d_tree.key_comp()(k, this->pos(pos).first)) {
				d_tree.pos(pos).second = std::forward<M>(obj);
				return std::pair<size_t, bool>((pos), false);
			}
			else {
				d_tree.d_deque.insert(pos, value_type(std::move(k), std::forward<M>(obj)));
				return std::pair<size_t, bool>((pos), true);
			}
		}
		template <class M>
		auto insert_or_assign(const_iterator hint, const Key& k, M&& obj) -> iterator
		{
			(void)hint;
			return insert_or_assign(k, std::forward<M>(obj)).first;
		}
		template <class M>
		auto insert_or_assign(const_iterator hint, Key&& k, M&& obj) -> iterator
		{
			(void)hint;
			return insert_or_assign(std::move(k), std::forward<M>(obj)).first;
		}


		auto at(const Key& key) -> T&
		{
			size_t pos = find_pos(key);
			if (pos == size())
				throw  std::out_of_range("flat_map::at : invalid key");
			return d_tree.pos(pos).second;
		}
		auto at(const Key& key) const -> const T&
		{
			size_t pos = find_pos(key);
			if (pos == size())
				throw  std::out_of_range("flat_map::at : invalid key");
			return d_tree.pos(pos).second;
		}

		auto operator[](const Key& k) -> T&
		{
			size_t pos = d_tree.lower_bound_pos(k);
			if (pos != size() && !d_tree.key_comp()(k, this->pos(pos).first)) {
				return d_tree.pos(pos).second;
			}
			else {
				d_tree.d_deque.insert(pos, value_type(k, T()));
				return d_tree.pos(pos).second;
			}
		}
		auto operator[](Key&& k) -> T&
		{
			size_t pos = d_tree.lower_bound_pos(k);
			if (pos != size() && !d_tree.key_comp()(k, this->pos(pos).first)) {
				return d_tree.pos(pos).second;
			}
			else {
				d_tree.d_deque.insert(pos, value_type(std::move(k), T()));
				return d_tree.pos(pos).second;
			}
		}


		SEQ_ALWAYS_INLINE auto pos(size_t i) const noexcept -> const value_type& {return d_tree.d_deque[i];}
		SEQ_ALWAYS_INLINE auto pos(size_t i) noexcept -> std::pair<const Key,T> & {return reinterpret_cast<std::pair<const Key, T>&>(d_tree.d_deque[i]);}

	
		template< class InputIt >
		SEQ_ALWAYS_INLINE void insert(InputIt first, InputIt last) { return d_tree.insert(first, last); }
		SEQ_ALWAYS_INLINE void insert(std::initializer_list<value_type> ilist) { return d_tree.insert(ilist); }

		template< class Iter >
		SEQ_ALWAYS_INLINE void assign(Iter first, Iter last) { d_tree.assign(first, last); }

		SEQ_ALWAYS_INLINE void erase_pos(size_type pos) { d_tree.erase_pos(pos); }
		SEQ_ALWAYS_INLINE auto erase(const_iterator pos) -> iterator { return d_tree.erase(pos.iter); }
		SEQ_ALWAYS_INLINE auto erase(iterator pos) -> iterator { return d_tree.erase(pos.iter); }
		SEQ_ALWAYS_INLINE auto erase(const_iterator first, const_iterator last) -> iterator { return d_tree.erase(first.iter, last.iter); }
		SEQ_ALWAYS_INLINE void erase(size_t first, size_t last) { d_tree.erase(first, last); }

		template <class K, class LE = Compare, typename std::enable_if<has_is_transparent<LE>::value>::type* = nullptr>
		SEQ_ALWAYS_INLINE	auto erase(const K& key) -> size_type { return d_tree.erase(key); }
		SEQ_ALWAYS_INLINE auto erase(const Key& key) -> size_type { return d_tree.erase(key); }

		template <class K, class LE = Compare, typename std::enable_if<has_is_transparent<LE>::value>::type* = nullptr>
		SEQ_ALWAYS_INLINE auto find(const K& key) -> iterator { return d_tree.find(key); }
		SEQ_ALWAYS_INLINE auto find(const Key& x) -> iterator  { return d_tree.find(x); }

		template <class K, class LE = Compare, typename std::enable_if<has_is_transparent<LE>::value>::type* = nullptr>
		SEQ_ALWAYS_INLINE auto find(const K& key) const -> const_iterator { return d_tree.find(key); }
		SEQ_ALWAYS_INLINE auto find(const Key& x) const -> const_iterator  { return d_tree.find(x); }

		template <class K, class LE = Compare, typename std::enable_if<has_is_transparent<LE>::value>::type* = nullptr>
		SEQ_ALWAYS_INLINE auto find_pos(const K& key) const -> size_t  { return d_tree.find_pos(key); }
		SEQ_ALWAYS_INLINE auto find_pos(const Key& x) const -> size_t  { return d_tree.find_pos(x); }

		template <class K, class LE = Compare, typename std::enable_if<has_is_transparent<LE>::value>::type* = nullptr>
		SEQ_ALWAYS_INLINE auto lower_bound(const K& x) -> iterator  { return d_tree.lower_bound(x); }
		SEQ_ALWAYS_INLINE auto lower_bound(const Key& x) -> iterator  { return d_tree.lower_bound(x); }

		template <class K, class LE = Compare, typename std::enable_if<has_is_transparent<LE>::value>::type* = nullptr>
		SEQ_ALWAYS_INLINE auto lower_bound(const K& x) const -> const_iterator  { return d_tree.lower_bound(x); }
		SEQ_ALWAYS_INLINE auto lower_bound(const Key& x) const -> const_iterator  { return d_tree.lower_bound(x); }

		template <class K, class LE = Compare, typename std::enable_if<has_is_transparent<LE>::value>::type* = nullptr>
		SEQ_ALWAYS_INLINE auto lower_bound_pos(const K& x) const -> size_t  { return d_tree.lower_bound_pos(x); }
		SEQ_ALWAYS_INLINE auto lower_bound_pos(const Key& x) const -> size_t  { return d_tree.lower_bound_pos(x); }

		template <class K, class LE = Compare, typename std::enable_if<has_is_transparent<LE>::value>::type* = nullptr>
		SEQ_ALWAYS_INLINE auto upper_bound(const K& x) -> iterator  { return d_tree.upper_bound(x); }
		SEQ_ALWAYS_INLINE auto upper_bound(const Key& x) -> iterator  { return d_tree.upper_bound(x); }

		template <class K, class LE = Compare, typename std::enable_if<has_is_transparent<LE>::value>::type* = nullptr>
		SEQ_ALWAYS_INLINE auto upper_bound(const K& x) const -> const_iterator  { return d_tree.upper_bound(x); }
		SEQ_ALWAYS_INLINE auto upper_bound(const Key& x) const -> const_iterator  { return d_tree.upper_bound(x); }

		template <class K, class LE = Compare, typename std::enable_if<has_is_transparent<LE>::value>::type* = nullptr>
		SEQ_ALWAYS_INLINE auto upper_bound_pos(const K& x) const -> size_t  { return d_tree.upper_bound_pos(x); }
		SEQ_ALWAYS_INLINE auto upper_bound_pos(const Key& x) const -> size_t  { return d_tree.upper_bound_pos(x); }

		template <class K, class LE = Compare, typename std::enable_if<has_is_transparent<LE>::value>::type* = nullptr>
		SEQ_ALWAYS_INLINE auto contains(const K& x) const -> bool  { return d_tree.contains(x); }
		SEQ_ALWAYS_INLINE auto contains(const Key& x) const -> bool  { return d_tree.contains(x); }

		template <class K, class LE = Compare, typename std::enable_if<has_is_transparent<LE>::value>::type* = nullptr>
		SEQ_ALWAYS_INLINE auto count(const K& x) const -> size_type  { return d_tree.count(x); }
		SEQ_ALWAYS_INLINE auto count(const Key& x) const -> size_type  { return d_tree.count(x); }

		template <class K, class LE = Compare, typename std::enable_if<has_is_transparent<LE>::value>::type* = nullptr>
		SEQ_ALWAYS_INLINE auto equal_range(const K& x) -> std::pair<iterator, iterator>  { return d_tree.equal_range(x); }
		SEQ_ALWAYS_INLINE auto equal_range(const Key& x) -> std::pair<iterator, iterator>  { return d_tree.equal_range(x); }

		template <class K, class LE = Compare, typename std::enable_if<has_is_transparent<LE>::value>::type* = nullptr>
		SEQ_ALWAYS_INLINE auto equal_range(const K& x) const -> std::pair<const_iterator, const_iterator>  { return d_tree.equal_range(x); }
		SEQ_ALWAYS_INLINE auto equal_range(const Key& x) const -> std::pair<const_iterator, const_iterator>  { return d_tree.equal_range(x); }

		template <class K, class LE = Compare, typename std::enable_if<has_is_transparent<LE>::value>::type* = nullptr>
		SEQ_ALWAYS_INLINE auto equal_range_pos(const K& x) const -> std::pair<size_t, size_t>  { return d_tree.equal_range_pos(x); }
		SEQ_ALWAYS_INLINE auto equal_range_pos(const Key& x) const -> std::pair<size_t, size_t>  { return d_tree.equal_range_pos(x); }

		template<class C2, LayoutManagement layout2 >
		SEQ_ALWAYS_INLINE void merge(flat_map<Key,T, C2, Allocator, layout2>& source) { d_tree.merge(source.d_tree); }

		SEQ_ALWAYS_INLINE auto begin() noexcept -> iterator { return d_tree.begin(); }
		SEQ_ALWAYS_INLINE auto begin() const noexcept -> const_iterator { return d_tree.cbegin(); }
		SEQ_ALWAYS_INLINE auto cbegin() const noexcept -> const_iterator { return d_tree.cbegin(); }

		SEQ_ALWAYS_INLINE auto end() noexcept -> iterator { return d_tree.end(); }
		SEQ_ALWAYS_INLINE auto end() const noexcept -> const_iterator { return d_tree.cend(); }
		SEQ_ALWAYS_INLINE auto cend() const noexcept -> const_iterator { return d_tree.cend(); }

		SEQ_ALWAYS_INLINE auto rbegin() noexcept -> reverse_iterator { return d_tree.rbegin(); }
		SEQ_ALWAYS_INLINE auto rbegin() const noexcept -> const_reverse_iterator { return d_tree.crbegin(); }
		SEQ_ALWAYS_INLINE auto crbegin() const noexcept -> const_reverse_iterator { return d_tree.crbegin(); }

		SEQ_ALWAYS_INLINE auto rend() noexcept -> reverse_iterator { return d_tree.rend(); }
		SEQ_ALWAYS_INLINE auto rend() const noexcept -> const_reverse_iterator { return d_tree.crend(); }
		SEQ_ALWAYS_INLINE auto crend() const noexcept -> const_reverse_iterator { return d_tree.crend(); }

		SEQ_ALWAYS_INLINE auto key_comp() const -> key_compare { return d_tree.key_comp(); }
		SEQ_ALWAYS_INLINE auto value_comp() const -> value_compare { return d_tree.value_comp(); }

		SEQ_ALWAYS_INLINE void sort() { d_tree.sort(); }
	};



	/// @brief flat sorted container similar to boost::flat_multimap with faster insertion/deletion of single values
	/// @tparam Key key type
	/// @tparam T mapped type
	/// @tparam Compare comparison function
	/// @tparam Allocator allocator
	/// @tparam layout memory layout of the underlying seq::tiered_vector
	/// @tparam Stable insertion order stability
	/// 
	/// 
	/// @brief flat sorted associative container that contains key-value pairs similar to boost::flat_multimap with faster insertion/deletion of single values. It supports multiple equal keys.
	/// 
	/// seq::flat_multimap directly inherits seq::flat_map and behaves similarly, except for the support of multiple equal keys.
	/// Its interface is similar to std::multimap, except for the node based members.
	/// 
	/// All references and iterators are invalidated when inserting/removing keys.
	/// 
	/// Unlike std::multimap which stores objects of type std::pair<const Key,T>, flat_multimap stores objects of type  std::pair<Key,T>.
	/// To avoid modifying the key value through iterators or using flat_multimap::pos(), the value is reinterpreted to std::pair<const Key,T>.
	/// Although quite ugly, this solution works on all tested compilers.
	/// 
	template<class Key, class T, class Compare = std::less<Key>, class Allocator = std::allocator<std::pair<Key,T>>, LayoutManagement layout = OptimizeForMemory, bool Stable = false>
	class flat_multimap : public flat_map<Key, T, Compare, Allocator, layout, Stable, false>
	{
		using base_type = flat_map<Key, T, Compare, Allocator, layout, Stable, false>;
	public:
		using deque_type = typename base_type::deque_type;
		using key_type = Key;
		using mapped_type = T;
		using value_type = std::pair<Key, T>;
		using difference_type = typename std::allocator_traits<Allocator>::difference_type;
		using size_type = typename std::allocator_traits<Allocator>::size_type;
		using key_compare = Compare;
		using value_compare = typename base_type::value_compare;
		using allocator_type = Allocator;
		using reference = value_type&;
		using const_reference = const value_type&;
		using pointer = typename std::allocator_traits<Allocator>::pointer;
		using const_pointer = typename std::allocator_traits<Allocator>::const_pointer;
		using iterator = typename base_type::iterator;
		using const_iterator = typename base_type::const_iterator;
		using reverse_iterator = typename base_type::reverse_iterator;
		using const_reverse_iterator = typename base_type::const_reverse_iterator;


		using base_type::insert;

		flat_multimap() {}
		explicit flat_multimap(const Compare& comp,
			const Allocator& alloc = Allocator())
			:base_type(comp, alloc) {}
		explicit flat_multimap(const Allocator& alloc)
			:base_type(alloc) {}
		template< class InputIt >
		flat_multimap(InputIt first, InputIt last,
			const Compare& comp = Compare(),
			const Allocator& alloc = Allocator())
			: base_type(first, last, comp, alloc) {}
		template< class InputIt >
		flat_multimap(InputIt first, InputIt last, const Allocator& alloc)
			: flat_multimap(first, last, Compare(), alloc) {}
		flat_multimap(const flat_multimap& other)
			:base_type(other) {}
		flat_multimap(const flat_multimap& other, const Allocator& alloc)
			:base_type(other, alloc) {}
		flat_multimap(flat_multimap&& other) noexcept
			:base_type(std::move(other)) {}
		flat_multimap(flat_multimap&& other, const Allocator& alloc) 
			:base_type(std::move(other), alloc) {}
		flat_multimap(std::initializer_list<value_type> init,
			const Compare& comp = Compare(),
			const Allocator& alloc = Allocator())
			:flat_multimap(init.begin(), init.end(), comp, alloc) {}
		flat_multimap(std::initializer_list<value_type> init, const Allocator& alloc)
			: flat_multimap(init, Compare(), alloc) {}

		auto operator=(flat_multimap&& other) noexcept -> flat_multimap& {
			(base_type&)(*this) = std::move((base_type&)other);
			return *this;
		}
		auto operator=(const flat_multimap& other) -> flat_multimap& {
			(base_type&)(*this) = (const base_type&)(other);
			return *this;
		}
		auto operator=(const std::initializer_list<value_type>& init) -> flat_multimap&
		{
			(base_type&)(this) = init;
			return *this;
		}


		SEQ_ALWAYS_INLINE auto insert(const value_type& value) -> iterator { return base_type::insert(value).first; }
		SEQ_ALWAYS_INLINE auto insert(value_type&& value) -> iterator { return base_type::insert(std::move(value)).first; }
		SEQ_ALWAYS_INLINE auto insert(const_iterator hint, const value_type& value) -> iterator { return base_type::insert(hint,value); }
		SEQ_ALWAYS_INLINE auto insert(const_iterator hint, value_type&& value) -> iterator { return base_type::insert(hint, std::move(value)); }

		template< class P >
		SEQ_ALWAYS_INLINE auto insert(P&& value) -> iterator { return base_type::insert(std::forward<P>(value)).first; }

		SEQ_ALWAYS_INLINE auto insert_pos(const value_type& value) -> size_t { return base_type::insert_pos(value).first; }
		SEQ_ALWAYS_INLINE auto insert_pos(value_type&& value) -> size_t { return base_type::insert_pos(std::move(value)).first; }
		template< class P >
		SEQ_ALWAYS_INLINE auto insert_pos(P&& value) -> size_t { return base_type::insert_pos(std::forward<P>(value)).first; }

		template< class... Args >
		SEQ_ALWAYS_INLINE auto emplace(Args&&... args) -> iterator { return base_type::emplace(std::forward<Args>(args)...).first; }
	
		template< class... Args >
		SEQ_ALWAYS_INLINE auto emplace_pos(Args&&... args) -> size_t { return base_type::emplace_pos(std::forward<Args>(args)...).first; }

	};






	/// @brief Checks if the contents of s1 and s2 are equal, that is, they have the same number of elements and each element in s1 compares equal with the element in s2 at the same position.
	template<class Key, class Compare , class Al1, class Al2 , LayoutManagement L1, LayoutManagement L2, bool S1 , bool S2, bool U1, bool U2 >
	auto operator==(const flat_set<Key, Compare, Al1, L1, S1, U1>& s1, const flat_set<Key, Compare, Al2, L2, S2, U2>& s2) -> bool
	{
		if (s1.size() != s2.size())
			return false;
		auto it1 = s1.begin();
		auto cmp = s1.key_comp();
		for (auto it2 = s2.begin(); it2 != s2.end(); ++it2, ++it1) {
			if (cmp(*it1, *it2) || cmp(*it2, *it1))
				return false;
		}
		return true;
	}
	/// @brief Checks if the contents of s1 and s2 are not equals.
	template<class Key, class Compare, class Al1, class Al2, LayoutManagement L1, LayoutManagement L2, bool S1, bool S2, bool U1, bool U2 >
	auto operator!=(const flat_set<Key, Compare, Al1, L1, S1, U1>& s1, const flat_set<Key, Compare, Al2, L2, S2, U2>& s2) -> bool
	{
		return !(s1==s2);
	}
	/// @brief Erases all elements that satisfy the predicate p from the container.
	template<class Key, class Compare, class Al1, LayoutManagement L1, bool S1, bool U1, class Pred >
	auto erase_if(flat_set<Key, Compare, Al1, L1, S1, U1>& s1,  Pred p) -> typename flat_set<Key, Compare, Al1, L1, S1, U1>::size_type
	{
		using deque_type = typename flat_set<Key, Compare, Al1, L1, S1, U1>::deque_type;
		// use const_cast to avoid flagging the flat_set as dirty
		deque_type& d = const_cast<deque_type&>(s1.cdeque());
		auto it = std::remove_if(d.begin(), d.end(), p);
		size_t res = d.end() - it;
		d.erase(it, d.end());
		d.manager()->update_all_back_values();
		return res;
	}

	/// @brief Checks if the contents of s1 and s2 are equal, that is, they have the same number of elements and each element in s1 compares equal with the element in s2 at the same position.
	template<class Key, class T, class Compare, class Al1, class Al2, LayoutManagement L1, LayoutManagement L2, bool S1, bool S2, bool U1, bool U2 >
	auto operator==(const flat_map<Key, T, Compare, Al1, L1, S1, U1>& s1, const flat_map<Key, T, Compare, Al2, L2, S2, U2>& s2) -> bool
	{
		if (s1.size() != s2.size())
			return false;
		auto it1 = s1.begin();
		auto cmp = s1.key_comp();
		for (auto it2 = s2.begin(); it2 != s2.end(); ++it2, ++it1) {
			if (cmp(it1->first, it2->first) || cmp(it2->first, it1->first))
				return false;
			if (it1->second != it2->second)
				return false;
		}
		return true;
	}
	/// @brief Checks if the contents of s1 and s2 are not equals.
	template<class Key, class T, class Compare, class Al1, class Al2, LayoutManagement L1, LayoutManagement L2, bool S1, bool S2, bool U1, bool U2 >
	auto operator!=(const flat_map<Key, T, Compare, Al1, L1, S1, U1>& s1, const flat_map<Key, T, Compare, Al2, L2, S2, U2>& s2) -> bool
	{
		return !(s1 == s2);
	}
	/// @brief Erases all elements that satisfy the predicate p from the container.
	template<class Key, class T, class Compare, class Al1, LayoutManagement L1, bool S1, bool U1, class Pred >
	auto erase_if(flat_map<Key, T, Compare, Al1, L1, S1, U1>& s1, Pred p) -> typename flat_map<Key, T, Compare, Al1, L1, S1, U1>::size_type
	{
		using deque_type = typename flat_map<Key, T, Compare, Al1, L1, S1, U1>::deque_type;
		// use const_cast to avoid flagging the flat_set as dirty
		deque_type& d = const_cast<deque_type&>(s1.cdeque());
		auto it = std::remove_if(d.begin(), d.end(), p);
		size_t res = d.end() - it;
		d.erase(it, d.end());
		d.manager()->update_all_back_values();
		return res;
	}

} //end namespace seq



/** @}*/
//end containers

#endif
