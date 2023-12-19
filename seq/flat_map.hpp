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

#ifndef SEQ_FLAT_MAP_HPP
#define SEQ_FLAT_MAP_HPP



/** @file */

/**\defgroup containers Containers: original STL-like containers

The \ref containers "containers" module defines several container classes as alternatives to STL containers or providing features not present in the STL.
These containers generally adhere to the properties of STL containers, though there are often some associated API differences and/or implementation details which differ from the standard library.

The Seq containers are not necessarly drop-in replacement for their STL counterparts as they usually provide different iterator/reference statibility rules or different exception guarantees.

Currently, the \ref containers "containers" module provide 5 types of containers:
	-	Sequential random-access containers: seq::devector, seq::cvector and seq::tiered_vector,
	-	Sequential stable non random-access container: seq::sequence,
	-	Sorted containers: seq::flat_set, seq::flat_map, seq::flat_multiset and seq::flat_multimap,
	-	Ordered robin-hood hash tables: seq::ordered_set and seq::ordered_map.
	-	Tiny string and string view: seq::tiny_string and seq::tstring_view.

See the documentation of each class for more details.

*/


#include "tiered_vector.hpp"
#include "tiny_string.hpp"
#include "internal/binary_search.hpp"

// Disable old style cast warning for gcc
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif
#include "pdqsort.hpp"
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif




/** \addtogroup containers
 *  @{
 */


namespace seq
{

	/// @brief Default less class for flat_set/map
	template<class T, bool HasCompare = (seq::is_tiny_string<T>::value || seq::is_basic_string<T>::value || seq::is_basic_string_view<T>::value)>
	struct SeqLess 
	{
		using is_transparent = int;

		template<class T1, class T2>
		constexpr auto operator()(T1&& left, T2&& right) const
			noexcept(noexcept(static_cast<T1&&>(left) < static_cast<T2&&>(right)))
			-> decltype(static_cast<T1&&>(left) < static_cast<T2&&>(right)) {
			return static_cast<T1&&>(left) < static_cast<T2&&>(right);
		}
	};

	/// @brief Default less class for flat_set/map of string-like keys
	/// Uses compare() member function whenever possible.
	template<class T>
	struct SeqLess<T,true> 
	{
		using comparable = int;
		using is_transparent = int;
		using char_type = typename T::value_type;
		using view_type = seq::basic_tstring_view<char_type >;

		template<class Str>
		view_type build(const Str& s) const noexcept { return view_type(s); }
		template<class C, class Tr, class A, size_t S>
		const tiny_string<C, Tr, A, S>& build(const tiny_string<C, Tr,A,S>& s) const noexcept { return s; }

		template<class L, class R>
		int compare(L&& left, R&& right) const noexcept {
			return build(std::forward<L>(left)).compare(build(std::forward<R>(right)));
		}
		
		template<class T1, class T2>
		constexpr auto operator()(T1&& left, T2&& right) const
			noexcept(noexcept(static_cast<T1&&>(left) < static_cast<T2&&>(right)))
			-> decltype(static_cast<T1&&>(left) < static_cast<T2&&>(right)) {
			return static_cast<T1&&>(left) < static_cast<T2&&>(right);
		}
	};

	namespace detail
	{
		
		
		
		/// @brief Policy used when inserting new key
		struct FlatEmplacePolicy
		{
			template<class Vector, class K, class... Args >
			static typename Vector::value_type& emplace(Vector& v, size_t pos, K&& key, Args&&... args)
			{
				return v.emplace(pos, std::forward<K>(key), std::forward<Args>(args)...);
			}
		};
		/// @brief Policy used when inserting new key using try_emplace
		struct TryFlatEmplacePolicy
		{
			template<class Vector, class K, class... Args >
			static typename Vector::value_type& emplace(Vector& v, size_t pos, K&& key, Args&&... args)
			{
				return v.emplace(pos, typename Vector::value_type(std::piecewise_construct,
					std::forward_as_tuple(std::forward<K>(key)),
					std::forward_as_tuple(std::forward<Args>(args)...)));
			}
		};
		

		template<class KeyType, class Less, bool Branchless = std::is_arithmetic<KeyType>::value>
		struct Pdqsorter
		{
			template<class Iter>
			static void apply(Iter first, Iter last, const Less &le)
			{
				pdqsort_branchless(first, last, le);
			}
		};
		template<class KeyType, class Less>
		struct Pdqsorter<KeyType,Less,false>
		{
			template<class Iter>
			static void apply(Iter first, Iter last, const Less & le)
			{
				pdqsort(first, last, le);
			}
		};


		template<class Deque, bool Stable, class Less>
		struct DequeSorter
		{
			// Sort tiered_vector using pdqsort or pdqsort_branchless (for arithmetic types).
			static void sort(Deque& d, size_t begin, size_t end, const Less & le) 
			{
				using key_type = typename Deque::value_compare::key_type;
				Pdqsorter< key_type, Less>::apply(d.begin() + static_cast<std::ptrdiff_t>(begin), d.begin() + static_cast<std::ptrdiff_t>(end), le);
			}

			static void sort_full(Deque& d, const Less& le) 
			{
				// Sort a full deque.
				// Instead of using default tiered vector iterators, this version drops vector ordering to use faster iterators.
				// Only working for non stable sort.

				using T = typename Deque::value_type;
				using key_type = typename Deque::value_compare::key_type;
				using bucket_manager = typename Deque::bucket_manager;
				static constexpr bool nothrow = std::is_nothrow_move_constructible<T>::value && std::is_nothrow_move_assignable<T>::value;
				
				if (d.size() == 0)
					return;

				if SEQ_CONSTEXPR (!nothrow)
				{
					Pdqsorter< key_type, Less>::apply(d.begin(), d.end(), le);
				}
				else
				{
					// Fill front bucket
					while (d.manager()->d_buckets.size() > 1 && d.manager()->d_buckets[0].bucket->size != d.manager()->d_bucket_size)
					{
						T tmp = std::move(d.back());
						d.pop_back();
						d.push_front(std::move(tmp));
					}
					SEQ_ASSERT_DEBUG(d.manager()->d_buckets[0].bucket->size == d.manager()->d_bucket_size || d.manager()->d_buckets.size() == 1, "");

					if (d.manager()->d_buckets.back().bucket->size != d.manager()->d_bucket_size)
					{
						// If the back bucket is not full, make it start at 0
						auto* bucket = d.manager()->d_buckets.back().bucket;
						auto al = d.get_allocator();
						while (bucket->begin != 0) {
							T tmp = std::move(bucket->back());
							bucket->pop_back(al);
							bucket->push_front(al, std::move(tmp));
						}
					}

					auto b = detail::tvector_ra_iterator<bucket_manager>(d.manager());
					auto e = detail::tvector_ra_iterator<bucket_manager>(d.manager(), 0);
					Pdqsorter< key_type, Less>::apply(b, e, le);

					for (size_t i = 0; i < d.manager()->d_buckets.size(); ++i)
					{
						auto& bucket = d.manager()->d_buckets[i];
						bucket.bucket->begin = 0;
						bucket.update();
					}
				}
			}
		};

		
		template<class Deque, class Less >
		struct DequeSorter<Deque,true,Less>
		{
			// Sort tiered_vector in a stable way
			static void sort(Deque& d, size_t begin, size_t end, const Less& le)
			{
				std::stable_sort(d.begin() + begin, d.begin() + end, le);
			}
			static void sort_full(Deque& d, const Less& le)
			{
				std::stable_sort(d.begin() , d.end(), le);
			}
		};

		template<bool Stable, class Deque, class Less>
		void sort_deque(Deque & d, size_t begin, size_t end, Less less)
		{
			DequeSorter<Deque,Stable,Less>::sort(d, begin, end, less);
		}
		template<bool Stable, class Deque, class Less>
		void sort_full_deque(Deque& d, Less less)
		{
			DequeSorter<Deque, Stable, Less>::sort_full(d, less);
		}



		/// @brief Less adapter functor that works on tiered_vector buckets
		template<class Le, bool Comparable = has_comparable<Le>::value>
		struct LeAdapter : Le
		{
			LeAdapter(const Le& le):Le(le) {}

			template<class L, class R>
			bool operator()(L&& left, R&& right) const {return Le::operator()(std::forward<L>(left) , std::forward<R>(right));}
			template< class L, class Al, class VC, bool SB, bool IA, class R >
			bool operator()(const detail::StoreBucket<L, Al, VC, SB, IA>& bucket, R&& right) const {return Le::operator()(bucket.back(), std::forward<R>(right));}
		};
		template<class Le>
		struct LeAdapter<Le, true> : Le
		{
			LeAdapter(const Le& le):Le(le) {}

			template<class L, class R>
			int compare(L&& left, R&& right) const  noexcept {return Le::compare(std::forward<L>(left), std::forward<R>(right));}
			template< class L, class Al, class VC, bool SB, bool IA, class R >
			int compare(const detail::StoreBucket<L, Al, VC, SB, IA>& bucket, R&& right) const noexcept {return Le::compare(bucket.back() , std::forward<R>(right));}

			template<class L, class R>
			bool operator()(L&& left, R&& right) const {return Le::operator()(std::forward<L>(left), std::forward<R>(right));}
			template< class L, class Al, class VC, bool SB, bool IA, class R >
			bool operator()(const detail::StoreBucket<L, Al, VC, SB, IA>& bucket, R&& right) const {return Le::operator()(bucket.back(), std::forward<R>(right));}
		};

		/// @brief Optimized version of std::lower_bound(begin(),end(),value,le);
		/// Only works for sorted tiered_vector.
		template<bool Multi, class Deque, class U, class Less >
		__SEQ_INLINE_LOOKUP auto tvector_lower_bound(const Deque & d, const U& value, const Less& le ) noexcept -> std::pair<size_t,bool>
		{
			if (SEQ_UNLIKELY(!d.manager()))
				return { 0,false };

			using T = typename Deque::value_type;
			using bucket_manager = typename Deque::bucket_manager;
			using BucketVector = typename bucket_manager::BucketVector;
			//using BucketType = typename BucketVector::value_type;
			using ValueCompare = typename Deque::value_compare;
			using KeyType = typename ValueCompare::key_type;
			const BucketVector& buckets = d.manager()->buckets();

			//find bucket
			const size_t b_index = lower_bound<Multi,KeyType>(buckets.data(), buckets.size(), value,
				LeAdapter<Less>{le}//[&le](const BucketType& a, const U& b) {return le(a.back(), b); }
			).first;

			if (SEQ_UNLIKELY(b_index == buckets.size()))
				return { d.size(),false };

			{
				//find inside bucket
				const auto* bucket = buckets[b_index].bucket;

				// Partition the bucket into left/right side based on the circular buffer begin position, 
				// and apply the lower bound on one of the 2.

				using pos_type = int;
				const T* begin_ptr = bucket->begin_ptr();

				const bool low_half =
					begin_ptr != bucket->buffer() && // begin is not 0
					(bucket->begin + bucket->size) > bucket->max_size_ && // begin + size overflow
					le(*(bucket->buffer() + bucket->max_size1), value); // value to took for is not in the upper half (between begin and buffer end)

				const T* ptr = low_half ? bucket->buffer() : begin_ptr;
				const pos_type partition_size = low_half ?
					((bucket->begin + bucket->size) & bucket->max_size1) :
					(std::min(bucket->size, static_cast<detail::cbuffer_pos>(bucket->max_size_ - bucket->begin)));

				auto _low = lower_bound<Multi,KeyType>(ptr, partition_size, value, le);

				_low.first = ptr == begin_ptr ? _low.first : _low.first + (bucket->max_size_ - bucket->begin);
				size_t r = static_cast<size_t>(_low.first) + (b_index != 0 ? static_cast<size_t>(buckets[0]->size) + static_cast<size_t>(b_index - 1) * static_cast<size_t>(bucket->max_size_) : 0);
				return { r,_low.second };
			}

		}




		/// @brief Optimized version of std::upper_bound(begin(),end(),value,le);
		/// Only works for sorted tiered_vector.
		template<class Deque, class U, class Less >
		auto tvector_upper_bound(const Deque & d, const U& value, const Less& le ) noexcept -> size_t
		{
			return tvector_lower_bound<false>(d, value, [&le](const auto& a, const auto& b) {return !le(b, a); }).first;
		}


		/// @brief Optimized version of std::binary_search(begin(),end(),value,le);
		/// Only works for sorted tiered_vector.
		template<bool Multi, class Deque, class U, class Less >
		auto tvector_binary_search(const Deque & d, const U& value, const Less& le = Less()) noexcept -> size_t
		{
			auto l = tvector_lower_bound<Multi>(d, value, le);
			if SEQ_CONSTEXPR (has_comparable<Less>::value) {
				if (l.second) return l.first;
				return d.size();
			}
			if (l.first != d.size() && le(value, d[l.first])) l.first = d.size();
			return l.first;
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
			size_t len1 = seq::distance(first1, last1);
			size_t len2 = seq::distance(first2, last2);
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



		template<class Less, bool HasComparable = has_comparable<Less>::value >
		struct BaseLess : Less
		{
			BaseLess() : Less() {}
			BaseLess(const Less& l) : Less(l) {}
			BaseLess(const BaseLess&) = default;
			BaseLess( BaseLess&&) = default;
			BaseLess& operator=(const BaseLess&) = default;
			BaseLess& operator=( BaseLess&&) = default;
			// provides a dummy compare() member just to compile
			template<class U, class V>
			int compare( U&& ,  V&& ) const { return 0; }
		};
		template<class Less >
		struct BaseLess<Less,true> : Less
		{
			BaseLess() : Less() {}
			BaseLess(const Less& l) : Less(l) {}
			BaseLess(const BaseLess&) = default;
			BaseLess(BaseLess&&) = default;
			BaseLess& operator=(const BaseLess&) = default;
			BaseLess& operator=(BaseLess&&) = default;
		};

	
		/// @brief Base class for flat_tree
		template<class Key, class Value, class Less>
		struct BaseTree : BaseLess<Less>
		{
			using base = BaseLess<Less>;
			// Base class for flat_tree, directly inherit Less and provides basic comparison functions
			using extract_key = ExtractKey<Key, Value>;

			BaseTree() {}
			BaseTree(const Less& l) : base(l) {}
			BaseTree(const BaseTree& other) : base(other) {}
			BaseTree(BaseTree&& other)  noexcept(std::is_nothrow_move_constructible<Less>::value) 
				: base(std::move(static_cast<base&>(other))) {}

			auto operator=(const BaseTree& other) -> BaseTree& {
				static_cast<base&>(*this) = static_cast<const base&>(other);
				return *this;
			}
			auto operator=( BaseTree&& other) noexcept(std::is_nothrow_move_assignable<Less>::value) -> BaseTree&   {
				static_cast<base&>(*this) = std::move(static_cast<base&>(other));
				return *this;
			}

			void swap(BaseTree& other) noexcept(std::is_nothrow_move_constructible<Less>::value&& std::is_nothrow_move_assignable<Less>::value)
			{
				std::swap(static_cast<base&>(*this), static_cast<base&>(other));
			}

			template<class U, class V>
			SEQ_ALWAYS_INLINE int compare( U&& v1,  V&& v2) const { return base::compare( extract_key::key(std::forward<U>(v1)), extract_key::key(std::forward<V>(v2))); }
			template<class U, class V>
			SEQ_ALWAYS_INLINE bool operator()( U&& v1,  V&& v2) const { return base::operator()(extract_key::key(std::forward<U>(v1)), extract_key::key(std::forward<V>(v2))); }
			template<class U, class V>
			SEQ_ALWAYS_INLINE bool equal( U&& v1,  V&& v2) const { return !operator()(std::forward<U>(v1), std::forward<V>(v2)) && !operator()(std::forward<V>(v2), std::forward<U>(v1)); }
		};
		/// @brief Base class for flat_tree in case of simple set (no associative container)
		template<class Key, class Less>
		struct BaseTree<Key,Key,Less> : BaseLess<Less>
		{
			using base = BaseLess<Less>;
			using extract_key = ExtractKey<Key, Key>;

			BaseTree() {}
			BaseTree(const Less& l) : base(l) {}
			BaseTree(const BaseTree& other) : base(other) {}
			BaseTree(BaseTree&& other)  noexcept(std::is_nothrow_move_constructible<Less>::value) : base(std::move(static_cast<base&>(other))) {}

			auto operator=(const BaseTree& other) -> BaseTree& {
				static_cast<base&>(*this) = static_cast<const base&>(other);
				return *this;
			}
			auto operator=(BaseTree&& other) noexcept(std::is_nothrow_move_assignable<Less>::value) -> BaseTree&   {
				static_cast<base&>(*this) = std::move(static_cast<base&>(other));
				return *this;
			}

			void swap(BaseTree& other) noexcept(std::is_nothrow_move_constructible<Less>::value&& std::is_nothrow_move_assignable<Less>::value)
			{
				std::swap(static_cast<base&>(*this), static_cast<base&>(other));
			}

			template<class U, class V>
			SEQ_ALWAYS_INLINE int compare( U&& v1,  V&& v2) const { return base::compare(std::forward<U>(v1), std::forward<V>(v2)); }
			template<class U, class V>
			SEQ_ALWAYS_INLINE bool operator()( U&& v1,  V&& v2) const { return base::operator()(std::forward<U>(v1), std::forward<V>(v2)); }
			template<class U, class V>
			SEQ_ALWAYS_INLINE bool equal( U&& v1,  V&& v2) const { return !operator()(std::forward<U>(v1), std::forward<V>(v2)) && !operator()(std::forward<U>(v1), std::forward<V>(v2)); }
		};

	
		/// @brief Base class for flat_map/set
		/// Uses a seq::tiered_vector to store the values and provide faster insertion/deletion of unique elements.
		template<class Key, class Value = Key, class Compare = SeqLess<Key>, class Allocator = std::allocator<Value>, bool Stable = true, bool Unique = true>
		struct flat_tree : private BaseTree<Key,Value,Compare>
		{
			
			using extract_key = ExtractKey<Key, Value>;
			using base_type = BaseTree<Key, Value, Compare>;
			using this_type = flat_tree<Key, Value, Compare, Allocator, Stable, Unique>;

			static constexpr bool Comparable = has_comparable<Compare>::value;

			// Structure for equality comparison
			struct Equal
			{
				flat_tree* c;
				Equal(flat_tree * _c):c(_c) {}
				SEQ_ALWAYS_INLINE auto operator()(const Value& a, const Value& b) const -> bool { return c->equal((a), (b)); }
			};
			// Structure for less than comparison
			struct Less
			{
				using key_type = Key;
				using compare = Compare;
				flat_tree* c;
				Less(flat_tree* _c) :c(_c) {}
				SEQ_ALWAYS_INLINE auto operator()(const Value& a, const Value& b) const -> bool { return (*c)((a), (b)); }
			};

			SEQ_ALWAYS_INLINE auto base() noexcept -> base_type&  { return (*this); }
			SEQ_ALWAYS_INLINE auto base() const noexcept -> const base_type& { return  (*this); }

		public:
		
			using deque_type = tiered_vector<Value, Allocator, SEQ_MIN_BUCKET_SIZE(Value), SEQ_MAX_BUCKET_SIZE, FindBucketSize<Value>, true, ExtractKey<Key,Value> >;
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

			SEQ_ALWAYS_INLINE void mark_dirty() noexcept
			{
				d_dirty = 1;
			}
			SEQ_ALWAYS_INLINE bool dirty() const noexcept
			{
				return d_dirty != 0;
			}
			SEQ_ALWAYS_INLINE void check_dirty() const
			{
				if (SEQ_UNLIKELY(dirty()))
					throw std::logic_error("flat_tree is dirty");
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
					sort_full_deque<Stable>(d_deque, Less(this));
				if SEQ_CONSTEXPR (Unique)
					if ( (!sorted || !unique)) {
						iterator it = std::unique(d_deque.begin(), d_deque.end(), Equal(this));
						d_deque.erase(it, d_deque.end());
					}
				d_deque.manager()->update_all_back_values();
			}

			template<class Iter>
			void assign_cat(Iter first, Iter last, std::random_access_iterator_tag /*unused*/)
			{
				//d_deque.clear();
				d_deque.resize(static_cast<size_t>(last - first));
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
					sort_full_deque<Stable>(d_deque, Less(this));
				if SEQ_CONSTEXPR (Unique )
					if((!sorted || !unique)) {
						iterator l = std::unique(d_deque.begin(), d_deque.end(), Equal(this));
						d_deque.erase(l, d_deque.end());
					}
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
			flat_tree(flat_tree&& other) noexcept(std::is_nothrow_move_constructible<base_type>::value && std::is_nothrow_move_constructible<deque_type>::value)
				:base_type(std::move(other)), d_deque(std::move(other.d_deque)), d_dirty(other.d_dirty) {}
			flat_tree(flat_tree&& other, const Allocator& alloc) 
				: base_type(std::move(other)), d_deque(std::move(other.d_deque), alloc), d_dirty(other.d_dirty) {}
			flat_tree(std::initializer_list<value_type> init,
				const Compare& comp = Compare(),
				const Allocator& alloc = Allocator())
				:flat_tree(init.begin(), init.end(), comp, alloc) {}
			flat_tree(std::initializer_list<value_type> init, const Allocator& alloc)
				: flat_tree(init, Compare(), alloc) {}

			auto operator=(flat_tree&& other) noexcept(std::is_nothrow_move_assignable<base_type>::value&& std::is_nothrow_move_assignable<deque_type>::value) -> flat_tree& {
				if (this != std::addressof(other)) {
					d_deque = std::move(other.d_deque);
					d_dirty = other.d_dirty;
					base() = std::move(other.base());
				}
				return *this;
			}
			auto operator=(const flat_tree& other) -> flat_tree& 
			{
				if (this != std::addressof(other)) {
					d_deque = (other.d_deque);
					d_dirty = other.d_dirty;
					base() = other.base();
				}
				return *this;
			}
			auto operator=(const std::initializer_list<value_type>& init) -> flat_tree&
			{
				assign(init.begin(), init.end());
				return *this;
			}

			auto get_allocator() const noexcept -> const Allocator & { return d_deque.get_allocator(); }
			auto get_allocator() noexcept -> Allocator& { return d_deque.get_allocator(); }

			auto empty() const noexcept -> bool { return d_deque.empty(); }
			auto size() const noexcept -> size_type { return d_deque.size(); }
			auto max_size() const noexcept -> size_type { return d_deque.max_size(); }
			void clear() noexcept { d_deque.clear(); }
			auto tvector() noexcept -> deque_type& { mark_dirty(); return d_deque; }
			auto tvector() const noexcept -> const deque_type& { return d_deque; }
			auto ctvector() const noexcept -> const deque_type& { return d_deque; }

			void swap(flat_tree& other) 
				noexcept(noexcept(std::declval<deque_type&>().swap(std::declval<deque_type&>())) && noexcept(std::declval<base_type&>().swap(std::declval<base_type&>())))
			{
				if (this != std::addressof(other)) {
					d_deque.swap(other.d_deque);
					base().swap(other.base());
					std::swap(d_dirty, other.d_dirty);
				}
			}

			iterator iterator_at(size_t pos)noexcept { return d_deque.iterator_at(pos); }
			const_iterator iterator_at(size_t pos) const noexcept { return d_deque.iterator_at(pos); }

			template<class Policy, class K, class... Args >
			auto emplace_pos_multi(Policy, K&& key, Args&&... args) -> std::pair<size_t, bool>
			{
				size_t pos = tvector_upper_bound(d_deque, key, base());
				Policy::emplace(d_deque, pos, std::forward<K>(key), std::forward<Args>(args)...);
				return std::pair<size_t, bool>(pos, true);
			}
			template<class Policy, class K, class... Args >
			auto emplace_pos(Policy p, K&& key, Args&&... args) -> std::pair<size_t, bool> {

				check_dirty();
				if SEQ_CONSTEXPR (!Unique)
					return emplace_pos_multi(p, std::forward<K>(key), std::forward<Args>(args)...);
			
				if SEQ_CONSTEXPR (Comparable) {
					auto l = tvector_lower_bound<!Unique>(d_deque, key, base());
					if (l.second)
						return{ l.first, false };
					l.second = true;
					Policy::emplace(d_deque, l.first, std::forward<K>(key), std::forward<Args>(args)...);
					return l;
				}

				std::pair<size_t, bool> res;
				res.first = tvector_lower_bound<!Unique>(d_deque, key, base()).first;
				res.second = !(res.first != d_deque.size() && !(*this)(key, d_deque[res.first]));
				if(res.second)
					Policy::emplace(d_deque, res.first, std::forward<K>(key), std::forward<Args>(args)...);
				return res;
			}
			template<class K, class... Args >
			auto emplace(K&& key, Args&&... args) -> std::pair<size_t, bool> 
			{
				return emplace_pos(FlatEmplacePolicy{}, std::forward<K>(key), std::forward<Args>(args)...);
			}
			template<class K, class... Args >
			auto try_emplace(K&& key, Args&&... args) -> std::pair<size_t, bool>
			{
				return emplace_pos(TryFlatEmplacePolicy{}, std::forward<K>(key), std::forward<Args>(args)...);
			}
			
			template<class Policy, class K, class... Args >
			auto emplace_pos_hint(const_iterator hint, Policy p, K&& key, Args&&... args) -> std::pair<size_t, bool>
			{
				check_dirty();
				if SEQ_CONSTEXPR (!Unique) {
					if (hint != end() && (*this)(key, *hint)) {
						auto h = hint;
						if (hint != begin() && !(*this)(key, *(--hint))) {
							Policy::emplace(d_deque, static_cast<size_t>(h.pos), std::forward<K>(key), std::forward<Args>(args)...);
							return std::pair<size_t, bool>(static_cast<size_t>(h.pos), true);
						}
					}
					return emplace_pos(p, std::forward<K>(key), std::forward<Args>(args)...);
				}
				if (hint != end() && (*this)(key, *hint)) {
					//value is < hint, check that value before hint is smaller
					auto h = hint;
					if (hint == begin() || (*this)(*(--hint), key))
					{
						Policy::emplace(d_deque, static_cast<size_t>(h.pos), std::forward<K>(key), std::forward<Args>(args)...);
						return std::pair<size_t, bool>(static_cast<size_t>(h.pos), true);
					}
				}
				return emplace_pos(p, std::forward<K>(key), std::forward<Args>(args)...);
			}
			template<class K, class... Args >
			auto emplace_hint(const_iterator hint, K&& key, Args&&... args) -> std::pair<size_t, bool>
			{
				return emplace_pos_hint(hint, FlatEmplacePolicy{}, std::forward<K>(key), std::forward<Args>(args)...);
			}
			


			template<class K, class... Args >
			auto try_emplace_hint(const_iterator hint, K&& key, Args&&... args) -> std::pair<size_t, bool>
			{
				return emplace_pos_hint(hint, TryFlatEmplacePolicy{}, std::forward<K>(key), std::forward<Args>(args)...);
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
			
				size_t len = seq::distance(first, last);
				size_t size_before = d_deque.size();

				// Resize tiered_vector
				d_deque.resize(len + size_before);

				// Append input to tiered_vector
				auto dit = d_deque.begin() + static_cast<difference_type>(size_before);
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
				std::inplace_merge(d_deque.begin(), d_deque.begin() + static_cast<difference_type>(size_before), d_deque.end(), Less(this));

				if SEQ_CONSTEXPR (Unique) {
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
			void assign(Iter first, Iter last)
			{
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
				if SEQ_CONSTEXPR (Unique) {
					size_t pos = tvector_binary_search<!Unique>(d_deque,key, base());
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
			__SEQ_INLINE_LOOKUP auto find(const K& x) -> iterator
			{
				check_dirty();
				return d_deque.iterator_at(tvector_binary_search<!Unique>(d_deque,x, base()));
			}
			template< class K>
			__SEQ_INLINE_LOOKUP auto find(const K& x) const -> const_iterator
			{
				check_dirty();
				return d_deque.iterator_at(tvector_binary_search(d_deque,x, base()));
			}
			template< class K>
			__SEQ_INLINE_LOOKUP auto find_pos(const K& x) const -> size_t
			{
				check_dirty();
				return tvector_binary_search<!Unique>(d_deque,x, base());
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
			__SEQ_INLINE_LOOKUP auto lower_bound(const K& x) -> iterator
			{
				check_dirty();
				return d_deque.iterator_at(tvector_lower_bound<!Unique>(d_deque,x, base()).first);
			}
			template< class K >
			__SEQ_INLINE_LOOKUP auto lower_bound(const K& x) const -> const_iterator
			{
				check_dirty();
				return d_deque.iterator_at(tvector_lower_bound<!Unique>(d_deque,x, base()).first);
			}
			template< class K >
			__SEQ_INLINE_LOOKUP auto lower_bound_pos(const K& x) const -> size_t
			{
				check_dirty();
				return tvector_lower_bound<!Unique>(d_deque,x, base()).first;
			}
			template< class K >
			__SEQ_INLINE_LOOKUP auto upper_bound(const K& x) -> iterator
			{
				check_dirty();
				return d_deque.iterator_at(tvector_upper_bound(d_deque,x, base()));
			}
			template< class K >
			__SEQ_INLINE_LOOKUP auto upper_bound(const K& x) const -> const_iterator
			{
				check_dirty();
				return d_deque.iterator_at(tvector_upper_bound(d_deque,x, base()));
			}
			template< class K >
			__SEQ_INLINE_LOOKUP auto upper_bound_pos(const K& x) const -> size_t
			{
				check_dirty();
				return tvector_upper_bound(d_deque,x, base());
			}
			template< class K >
			__SEQ_INLINE_LOOKUP auto contains(const K& x) const -> bool
			{
				check_dirty();
				return tvector_binary_search<!Unique>(d_deque,x, base()) != size();
			}
			template< class K >
			__SEQ_INLINE_LOOKUP auto count(const K& x) const -> size_type
			{
				check_dirty();
				if SEQ_CONSTEXPR (Unique) {
					size_t pos = tvector_binary_search<!Unique>(d_deque,x, base());
					return (pos == d_deque.size() ? 0 : 1);
				}
				size_t low = tvector_lower_bound<!Unique>(d_deque,x, base()).first;
				if (low == d_deque.size()) return 0;
				return tvector_upper_bound(d_deque,x, base()) - low;
			}
			template< class K >
			__SEQ_INLINE_LOOKUP auto equal_range(const K& x) -> std::pair<iterator, iterator>
			{
				iterator low = lower_bound(x);
				if (low == end())
					return std::pair<iterator, iterator>(low, low);
				if SEQ_CONSTEXPR (Unique)
					return std::pair<iterator, iterator>(low, low+1);
				iterator up = upper_bound(x);
				return std::pair<iterator, iterator>(low, up);
			}
			template< class K >
			__SEQ_INLINE_LOOKUP auto equal_range(const K& x) const -> std::pair<const_iterator, const_iterator>
			{
				const_iterator low = lower_bound(x);
				if (low == end())
					return std::pair<iterator, iterator>(low, low);
				if SEQ_CONSTEXPR (Unique)
					return std::pair<iterator, iterator>(low, low + 1);
				const_iterator up = upper_bound(x);
				return std::pair<iterator, iterator>(low, up);
			}
			template< class K >
			__SEQ_INLINE_LOOKUP auto equal_range_pos(const K& x) const -> std::pair<size_t, size_t>
			{
				size_t low = lower_bound_pos(x);
				if (low == size())
					return std::pair<size_t, size_t>(low, low);
				if SEQ_CONSTEXPR (Unique)
					return std::pair<size_t, size_t>(low, low + 1);
				size_t up = upper_bound_pos(x);
				return std::pair<size_t, size_t>(low, up);
			}

			template<class C2, LayoutManagement layout2, bool Unique2 >
			void merge(flat_tree<Key, Key, C2, Allocator, layout2, Unique2>& source)
			{
				check_dirty();
				if SEQ_CONSTEXPR (Unique) {
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
					sort_full_deque<Stable>(d_deque, Less(this));
				if SEQ_CONSTEXPR (Unique )
					if((!sorted || !unique)){
						iterator l = std::unique(d_deque.begin(), d_deque.end(), Equal(this));
						d_deque.erase(l, d_deque.end());
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
	/// seq::flat_set provides a similar interface to std::set (C++17) with the following differences:
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
	/// ------------------------------
	/// 
	/// Unlike most flat set implementations, it it possible to directly access and modify the underlying tiered_vector. 
	/// This possibility must be used with great care, as modifying directly the tiered_vector might break the key ordering. 
	/// When calling the non-const version of flat_set::tvector(), the flat_set will be marked as dirty, and further attempts 
	/// to call functions like flat_set::find() of flat_set::lower_bound() (functions based on key ordering) will throw a std::logic_error.
	/// 
	/// Therefore, after finishing modifying the tiered_vector, you must call flat_set::sort() to sort again the tiered_vector, remove potential duplicates,
	/// and mark the flat_set as non dirty anymore.
	/// 
	/// This way of modifying a flat_set must be used carefully, but is way faster than multiple calls to flat_set::insert() or flat_set::erase().
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
	/// and <a href="https://github.com/greg7mdp/parallel-hashmap">phmap::btree_set</a> (based on abseil btree_set). The following table show the results when compiled with gcc 10.1.0 (-O3) for msys2 on Windows 10,
	/// using Intel(R) Core(TM) i7-10850H at 2.70GHz. Measured operations are:
	///		-	Insert successfully a range of 1M unique double randomly shuffled using set_class::insert(first,last)
	///		-	Insert successfully 1M unique double randomly shuffled one by one using set_class::insert(const Key&)
	///		-	Insert 1M double randomly shuffled one by one and already present in the set (failed insertion)
	///		-	Successfully search for 1M double in the set using set_class::find(const Key&), or flat_set::find_pos(const Key&)
	///		-	Search for 1M double not present in the set (failed lookup)
	///		-	Walk through the full set (1M double) using iterators
	///		-	Successfull find and erase all 1M double one by one using set_class::erase(iterator)
	/// 
	/// Note the the given memory is NOT the memory footprint of the container, but the one of the full program. It should be used relatively to compare
	/// memory usage difference between each container.
	/// 
	///	Set name                      |   Insert(range)    |       Insert       |Insert(failed) |Find (success) | Find (failed) |    Iterate    |     Erase     |
	/// ------------------------------|--------------------|--------------------|---------------|---------------|---------------|---------------|---------------|
	/// seq::flat_set                 |    46 ms/15 MO     |    408 ms/16 MO    |    128 ms     |    130 ms     |    122 ms     |     1 ms      |    413 ms     |
	/// phmap::btree_set              |    135 ms/18 MO    |    118 ms/18 MO    |    118 ms     |    119 ms     |    120 ms     |     3 ms      |    131 ms     |
	/// boost::flat_set<T>            |    57 ms/15 MO     |   49344 ms/17 MO   |    133 ms     |    132 ms     |    127 ms     |     1 ms      |   131460 ms   |
	/// std::set                      |    457 ms/54 MO    |    457 ms/54 MO    |    449 ms     |    505 ms     |    502 ms     |     92 ms     |    739 ms     |
	/// std::unordered_set            |    187 ms/46 MO    |    279 ms/50 MO    |    100 ms     |    116 ms     |    155 ms     |     29 ms     |    312 ms     |
	/// 
	/// Below is the same benchmark using random seq::tstring of length 14 (using Small String Optimization):
	/// 
	/// Set name                      |   Insert(range)    |       Insert       |Insert(failed) |Find (success) | Find (failed) |    Iterate    |     Erase     |
	/// ------------------------------|--------------------|--------------------|---------------|---------------|---------------|---------------|---------------|
	/// seq::flat_set                 |    127 ms/30 MO    |    891 ms/31 MO    |    252 ms     |    245 ms     |    240 ms     |     1 ms      |    904 ms     |
	/// phmap::btree_set              |    280 ms/37 MO    |    278 ms/37 MO    |    266 ms     |    292 ms     |    279 ms     |     10 ms     |    292 ms     |
	/// boost::flat_set<T>            |    107 ms/30 MO    |  585263 ms/32 MO   |    228 ms     |    232 ms     |    232 ms     |     0 ms      |   601541 ms   |
	/// std::set                      |    646 ms/77 MO    |    640 ms/77 MO    |    611 ms     |    672 ms     |    710 ms     |     87 ms     |    798 ms     |
	/// std::unordered_set            |    205 ms/54 MO    |    319 ms/57 MO    |    157 ms     |    192 ms     |    220 ms     |     34 ms     |    380 ms     |
	/// 
	/// 
	/// These benchmarks are available in file 'seq/benchs/bench_map.hpp'.
	/// seq::flat_set behaves quite well compared to phmap::btree_set or boost::flat_set, and is even faster for single insertion/deletion than std::set for double type.
	/// 
	/// seq::flat_set insertion/deletion performs in O(sqrt(N)) on average, as compared to std::set that performs in O(log(N)).
	/// seq::flat_set is therfore slower for inserting and deleting elements than std::set when dealing with several millions of elements.
	/// Lookup functions (find, lower_bound, upper_bound...) still perform in O(log(N)) and remain faster that std::set couterparts because of the underlying seq::tiered_vector cache friendliness.
	/// flat_set will almost always be slower for element lookup than boost::flat_set wich uses a single dense array, except for very small keys (like in above benchmark).
	/// 
	/// Several factors will impact the performances of seq::flat_set:
	/// -	Relocatable types (where seq::is_relocatable<T>::value is true) are faster than other types for insertion/deletion, as tiered_vector will use memmove to move around objects. 
	///		Therefore, a flat_set of seq::tstring will always be faster than std::string.
	/// -	Performances of insertion/deletion decrease as sizeof(value_type) increases. This is especially true for insertion/deletion, much less for lookup functions which
	///		remain (more or less) as fast as boost::flat_set.
	/// -	All members using the '_pos' prefix are usually slightly faster than their iterator based counterparts.
	/// 
	template<class Key, class Compare = SeqLess<Key>, class Allocator = std::allocator<Key>, bool Stable = false, bool Unique = true>
	class flat_set
	{
		using flat_tree_type = detail::flat_tree<Key, Key, Compare, Allocator,Stable, Unique>;
		flat_tree_type d_tree;

		template <typename U>
		using has_is_transparent = detail::has_is_transparent<U>;


	public:
		using deque_type = typename flat_tree_type::deque_type;
		using iterator = typename deque_type::const_iterator;
		using const_iterator = typename deque_type::const_iterator;
		using reverse_iterator = typename deque_type::const_reverse_iterator;
		using const_reverse_iterator = typename deque_type::const_reverse_iterator;

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
		flat_set(flat_set&& other) noexcept(std::is_nothrow_move_constructible< flat_tree_type>::value)
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
		auto operator=(flat_set&& other) noexcept(std::is_nothrow_move_assignable<flat_tree_type>::value) -> flat_set& {
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
		SEQ_ALWAYS_INLINE void swap(flat_set& other) noexcept (noexcept(std::declval<flat_tree_type&>().swap(std::declval<flat_tree_type&>())))
		{ d_tree.swap(other.d_tree); }

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


		/// @brief See std::set::emplace().
		template< class... Args >
		SEQ_ALWAYS_INLINE auto emplace(Args&&... args) -> std::pair<iterator, bool>
		{
			auto p = d_tree.emplace(std::forward<Args>(args)...);
			return std::pair<iterator, bool>(d_tree.iterator_at(p.first), p.second);
		}

		/// @brief See std::set::insert
		SEQ_ALWAYS_INLINE auto insert(const value_type& value) -> std::pair<iterator, bool> 
		{	
			return emplace(value);
		}
		/// @brief See std::set::insert
		SEQ_ALWAYS_INLINE auto insert(value_type&& value) -> std::pair<iterator, bool> 
		{ 
			return emplace(std::move(value));
		}

		/// @brief Same as insert(), but returns the inserted object position instead of iterator (or size() if no element was inserted).
		SEQ_ALWAYS_INLINE auto insert_pos(const value_type& value) -> std::pair<size_t, bool> { return d_tree.emplace(value); }
		/// @brief Same as insert(), but returns the inserted object position instead of iterator (or size() if no element was inserted).
		SEQ_ALWAYS_INLINE auto insert_pos(value_type&& value) -> std::pair<size_t, bool> { return d_tree.emplace(std::move(value)); }

		/// @brief Same as emplace(), but returns the inserted object position instead of iterator (or size() if no element was inserted).
		template< class... Args >
		SEQ_ALWAYS_INLINE auto emplace_pos(Args&&... args) -> std::pair<size_t, bool> { return d_tree.emplace(std::forward<Args>(args)...); }
		
		/// @brief Same as std::set::emplace_hint
		template <class... Args>
		SEQ_ALWAYS_INLINE auto emplace_hint(const_iterator hint, Args&&... args) -> iterator 
		{ 
			auto p = d_tree.emplace_hint(hint,std::forward<Args>(args)...); 
			return d_tree.iterator_at(p.first);
		}
		/// @brief Same as std::set::insert()
		SEQ_ALWAYS_INLINE auto insert(const_iterator hint, const value_type& value) -> iterator 
		{ 
			return emplace_hint(hint, value);
		}
		/// @brief Same as std::set::insert()
		SEQ_ALWAYS_INLINE auto insert(const_iterator hint, value_type&& value) -> iterator 
		{ 
			return emplace_hint(hint, std::move(value));
		}

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
		SEQ_ALWAYS_INLINE auto erase(const_iterator pos) -> iterator{return d_tree.erase(pos);}
		/// @brief Erase elements in the range [first, last)
		SEQ_ALWAYS_INLINE auto erase(const_iterator first, const_iterator last) -> iterator{ return d_tree.erase(first, last);}
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
	template<class Key, class Compare = SeqLess<Key>, class Allocator = std::allocator<Key>, bool Stable = false>
	class flat_multiset : public flat_set<Key,Compare,Allocator, Stable, false>
	{
		using base_type = flat_set<Key, Compare, Allocator, Stable, false>;
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
		flat_multiset(flat_multiset&& other) noexcept(std::is_nothrow_move_constructible< base_type>::value)
			:base_type(std::move(other)) {}
		flat_multiset(flat_multiset&& other, const Allocator& alloc) 
			:base_type(std::move(other), alloc) {}
		flat_multiset(std::initializer_list<value_type> init,
			const Compare& comp = Compare(),
			const Allocator& alloc = Allocator())
			:flat_multiset(init.begin(), init.end(), comp, alloc) {}
		flat_multiset(std::initializer_list<value_type> init, const Allocator& alloc)
			: flat_multiset(init, Compare(), alloc) {}

		auto operator=(flat_multiset&& other) noexcept(std::is_nothrow_move_assignable<base_type>::value) -> flat_multiset& {
			static_cast<base_type&>(*this) = std::move(static_cast<base_type&>(other));
			return *this;
		}
		auto operator=(const flat_multiset& other) -> flat_multiset& {
			static_cast<base_type&>(*this) = static_cast<const base_type&>(other);
			return *this;
		}
		auto operator=(const std::initializer_list<value_type>& init) -> flat_multiset&
		{
			static_cast<base_type&>(this) = init;
			return *this;
		}

		SEQ_ALWAYS_INLINE auto insert(const value_type& value) -> iterator { return base_type::insert(value).first; }
		SEQ_ALWAYS_INLINE auto insert(value_type&& value) -> iterator { return base_type::insert(std::move(value)).first; }

		SEQ_ALWAYS_INLINE auto insert(const_iterator hint, const value_type& value) -> iterator { return base_type::insert(hint,value); }
		SEQ_ALWAYS_INLINE auto insert(const_iterator hint, value_type&& value) -> iterator { return base_type::insert(hint, std::move(value)); }

		template< class InputIt >
		SEQ_ALWAYS_INLINE void insert(InputIt first, InputIt last) { base_type::insert(first, last); }
		SEQ_ALWAYS_INLINE void insert(std::initializer_list<value_type> ilist) { base_type::insert(ilist); }


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
	template<class Key, class T, class Compare = SeqLess<Key>, class Allocator = std::allocator<std::pair<Key,T> >, bool Stable = false, bool Unique = true>
	class flat_map
	{
		using flat_tree_type = detail::flat_tree<Key, std::pair<Key, T> , Compare, Allocator, Stable, Unique>;
		flat_tree_type d_tree;

		template <typename U>
		using has_is_transparent = detail::has_is_transparent<U>;


	public:
		using deque_type = typename flat_tree_type::deque_type;
		using iterator = typename deque_type::iterator;
		using const_iterator = typename deque_type::const_iterator;
		using reverse_iterator = typename deque_type::reverse_iterator;
		using const_reverse_iterator = typename deque_type::const_reverse_iterator;


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
		flat_map(flat_map&& other) noexcept(std::is_nothrow_move_constructible< flat_tree_type>::value)
			:d_tree(std::move(other.d_tree)) {}
		flat_map(flat_map&& other, const Allocator& alloc) 
			:d_tree(std::move(other.d_tree), alloc) {}
		flat_map(std::initializer_list<value_type> init,
			const Compare& comp = Compare(),
			const Allocator& alloc = Allocator())
			:flat_map(init.begin(), init.end(), comp, alloc) {}
		flat_map(std::initializer_list<value_type> init, const Allocator& alloc)
			: flat_map(init, Compare(), alloc) {}

		auto operator=(flat_map&& other) noexcept(std::is_nothrow_move_assignable< flat_tree_type>::value) -> flat_map& {
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

		SEQ_ALWAYS_INLINE auto get_allocator() const noexcept -> const Allocator & { return d_tree.get_allocator(); }
		SEQ_ALWAYS_INLINE auto get_allocator() noexcept -> Allocator & { return d_tree.get_allocator(); }

		SEQ_ALWAYS_INLINE auto empty() const noexcept -> bool { return d_tree.empty(); }
		SEQ_ALWAYS_INLINE auto size() const noexcept -> size_type { return d_tree.size(); }
		SEQ_ALWAYS_INLINE auto max_size() const noexcept -> size_type { return d_tree.max_size(); }
		SEQ_ALWAYS_INLINE void clear() noexcept { d_tree.clear(); }
		SEQ_ALWAYS_INLINE void swap(flat_map& other) noexcept (noexcept(std::declval< flat_tree_type&>().swap(std::declval< flat_tree_type&>())))
		{ d_tree.swap(other.d_tree); }

		SEQ_ALWAYS_INLINE auto tvector() noexcept -> deque_type& { return d_tree.tvector(); }
		SEQ_ALWAYS_INLINE auto tvector() const noexcept -> const deque_type& { return d_tree.tvector(); }
		SEQ_ALWAYS_INLINE auto ctvector() const noexcept -> const deque_type& { return d_tree.ctvector(); }

		template< class... Args >
		SEQ_ALWAYS_INLINE auto emplace(Args&&... args) -> std::pair<iterator, bool> 
		{
			auto p= d_tree.emplace(std::forward<Args>(args)...); 
			return std::pair<iterator, bool>(d_tree.iterator_at(p.first), p.second);
		}
		SEQ_ALWAYS_INLINE auto insert(const value_type& value) -> std::pair<iterator, bool> { return emplace(value); }
		SEQ_ALWAYS_INLINE auto insert(value_type&& value) -> std::pair<iterator, bool> { return emplace(std::move(value)); }
		template< class P , typename std::enable_if<std::is_constructible<value_type, P>::value, int>::type = 0>
		SEQ_ALWAYS_INLINE auto insert(P&& value) -> std::pair<iterator, bool>{return emplace(std::forward<P>(value));}


		template <class... Args>
		SEQ_ALWAYS_INLINE auto emplace_hint(const_iterator hint, Args&&... args) -> iterator 
		{ 
			auto p = d_tree.emplace_hint(hint, std::forward<Args>(args)...); 
			return d_tree.iterator_at(p.first);
		}
		

		SEQ_ALWAYS_INLINE auto insert(const_iterator hint, const value_type& value) -> iterator { return emplace_hint(hint, value); }
		SEQ_ALWAYS_INLINE auto insert(const_iterator hint, value_type&& value) -> iterator { return emplace_hint(hint, std::move(value)); }
		template< class P, typename std::enable_if<std::is_constructible<value_type, P>::value, int>::type = 0 >
		auto insert(const_iterator hint, P&& value) -> iterator { return emplace_hint(hint, std::forward<P>(value)); }

		template< class... Args >
		SEQ_ALWAYS_INLINE auto emplace_pos(Args&&... args) -> std::pair<size_t, bool> { return d_tree.emplace(std::forward<Args>(args)...); }
		SEQ_ALWAYS_INLINE auto insert_pos(const value_type& value) -> std::pair<size_t, bool> { return d_tree.emplace(value); }
		SEQ_ALWAYS_INLINE auto insert_pos(value_type&& value) -> std::pair<size_t, bool> { return d_tree.emplace(std::move(value)); }
		template< class P, typename std::enable_if<std::is_constructible<value_type, P>::value, int>::type = 0 >
		SEQ_ALWAYS_INLINE auto insert_pos(P&& value) -> std::pair<size_t, bool> { return d_tree.emplace(std::forward<P>(value)); }

		
		template< class... Args >
		SEQ_ALWAYS_INLINE auto try_emplace(const Key& k, Args&&... args) -> std::pair<iterator, bool>
		{
			auto p = d_tree.try_emplace(k, std::forward<Args>(args)...);
			return std::pair<iterator, bool>(d_tree.iterator_at(p.first), p.second);
		}
		template< class... Args >
		SEQ_ALWAYS_INLINE auto try_emplace(Key&& k, Args&&... args) -> std::pair<iterator, bool>
		{
			auto p = d_tree.try_emplace(std::move(k), std::forward<Args>(args)...);
			return std::pair<iterator, bool>(d_tree.iterator_at(p.first), p.second);
		}
		template< class... Args >
		SEQ_ALWAYS_INLINE auto try_emplace_pos(const Key& k, Args&&... args) -> std::pair<size_t, bool>
		{
			return d_tree.try_emplace(k, std::forward<Args>(args)...);
		}
		template< class... Args >
		SEQ_ALWAYS_INLINE auto try_emplace_pos(Key&& k, Args&&... args) -> std::pair<size_t, bool>
		{
			return d_tree.try_emplace(std::move(k), std::forward<Args>(args)...);
		}

		template< class... Args >
		SEQ_ALWAYS_INLINE auto try_emplace(const_iterator hint, const Key& k, Args&&... args) -> iterator
		{
			auto p = d_tree.try_emplace_hint(hint, k, std::forward<Args>(args)...);
			return d_tree.iterator_at(p.first);
		}
		template< class... Args >
		SEQ_ALWAYS_INLINE auto try_emplace(const_iterator hint, Key&& k, Args&&... args) -> iterator
		{
			auto p = d_tree.try_emplace_hint(hint, std::move(k), std::forward<Args>(args)...);
			return d_tree.iterator_at(p.first);
		}
		template< class... Args >
		SEQ_ALWAYS_INLINE auto try_emplace_pos(const_iterator hint, const Key& k, Args&&... args) -> size_t
		{
			return d_tree.try_emplace_hint(hint, k, std::forward<Args>(args)...).first;
		}
		template< class... Args >
		SEQ_ALWAYS_INLINE auto try_emplace_pos(const_iterator hint, Key&& k, Args&&... args) -> size_t
		{
			return d_tree.try_emplace_hint(hint, std::move(k), std::forward<Args>(args)...).first;
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
		SEQ_ALWAYS_INLINE auto insert_or_assign_pos(const Key& k, M&& obj) -> std::pair<size_t, bool>
		{
			auto inserted = try_emplace_pos(k, std::forward<M>(obj));
			if (!inserted.second)
				pos(inserted.first).second = std::forward<M>(obj);
			return inserted;
		}
		template <class M>
		SEQ_ALWAYS_INLINE auto insert_or_assign_pos(Key&& k, M&& obj) -> std::pair<size_t, bool>
		{
			auto inserted = try_emplace_pos(std::move(k), std::forward<M>(obj));
			if (!inserted.second)
				pos(inserted.first).second = std::forward<M>(obj);
			return inserted;
		}
		template <class M>
		SEQ_ALWAYS_INLINE auto insert_or_assign(const_iterator , const Key& k, M&& obj) -> iterator
		{
			return insert_or_assign(k, std::forward<M>(obj)).first;
		}
		template <class M>
		SEQ_ALWAYS_INLINE auto insert_or_assign(const_iterator , Key&& k, M&& obj) -> iterator
		{
			return insert_or_assign(std::move(k), std::forward<M>(obj)).first;
		}


		SEQ_ALWAYS_INLINE auto at(const Key& key) -> T&
		{
			size_t pos = find_pos(key);
			if (pos == size())
				throw  std::out_of_range("flat_map::at : invalid key");
			return d_tree.pos(pos).second;
		}
		SEQ_ALWAYS_INLINE auto at(const Key& key) const -> const T&
		{
			size_t pos = find_pos(key);
			if (pos == size())
				throw  std::out_of_range("flat_map::at : invalid key");
			return d_tree.pos(pos).second;
		}

		SEQ_ALWAYS_INLINE auto operator[](const Key& k) -> T&
		{
			return try_emplace(k).first->second;
		}
		SEQ_ALWAYS_INLINE auto operator[](Key&& k) -> T&
		{
			return try_emplace(std::move(k)).first->second;
		}


		SEQ_ALWAYS_INLINE auto pos(size_t i) const noexcept -> const value_type& {return d_tree.d_deque[i];}
		SEQ_ALWAYS_INLINE auto pos(size_t i) noexcept -> value_type& {return d_tree.d_deque[i];}

	
		template< class InputIt >
		SEQ_ALWAYS_INLINE void insert(InputIt first, InputIt last) { return d_tree.insert(first, last); }
		SEQ_ALWAYS_INLINE void insert(std::initializer_list<value_type> ilist) { return d_tree.insert(ilist); }

		template< class Iter >
		SEQ_ALWAYS_INLINE void assign(Iter first, Iter last) { d_tree.assign(first, last); }

		SEQ_ALWAYS_INLINE void erase_pos(size_type pos) { d_tree.erase_pos(pos); }
		SEQ_ALWAYS_INLINE auto erase(const_iterator pos) -> iterator { return d_tree.erase(pos); }
		SEQ_ALWAYS_INLINE auto erase(iterator pos) -> iterator { return d_tree.erase(pos); }
		SEQ_ALWAYS_INLINE auto erase(const_iterator first, const_iterator last) -> iterator { return d_tree.erase(first, last); }
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
	template<class Key, class T, class Compare = SeqLess<Key>, class Allocator = std::allocator<std::pair<Key,T>>, bool Stable = false>
	class flat_multimap : public flat_map<Key, T, Compare, Allocator, Stable, false>
	{
		using base_type = flat_map<Key, T, Compare, Allocator, Stable, false>;
	public:
		using deque_type = typename base_type::deque_type;
		using key_type = Key;
		using mapped_type = T;
		using value_type = typename base_type::value_type;
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
		flat_multimap(flat_multimap&& other) noexcept(std::is_nothrow_move_constructible< base_type>::value)
			:base_type(std::move(other)) {}
		flat_multimap(flat_multimap&& other, const Allocator& alloc) 
			:base_type(std::move(other), alloc) {}
		flat_multimap(std::initializer_list<value_type> init,
			const Compare& comp = Compare(),
			const Allocator& alloc = Allocator())
			:flat_multimap(init.begin(), init.end(), comp, alloc) {}
		flat_multimap(std::initializer_list<value_type> init, const Allocator& alloc)
			: flat_multimap(init, Compare(), alloc) {}

		auto operator=(flat_multimap&& other) noexcept(std::is_nothrow_move_assignable< base_type>::value) -> flat_multimap& {
			static_cast<base_type&>(*this) = std::move(static_cast<base_type&>(other));
			return *this;
		}
		auto operator=(const flat_multimap& other) -> flat_multimap& {
			static_cast<base_type&>(*this) = static_cast<const base_type&>(other);
			return *this;
		}
		auto operator=(const std::initializer_list<value_type>& init) -> flat_multimap&
		{
			static_cast<base_type&>(this) = init;
			return *this;
		}


		SEQ_ALWAYS_INLINE auto insert(const value_type& value) -> iterator { return base_type::insert(value).first; }
		SEQ_ALWAYS_INLINE auto insert(value_type&& value) -> iterator { return base_type::insert(std::move(value)).first; }
		SEQ_ALWAYS_INLINE auto insert(const_iterator hint, const value_type& value) -> iterator { return base_type::insert(hint,value); }
		SEQ_ALWAYS_INLINE auto insert(const_iterator hint, value_type&& value) -> iterator { return base_type::insert(hint, std::move(value)); }

		template< class P, typename std::enable_if<std::is_constructible<value_type, P>::value, int>::type = 0>
		SEQ_ALWAYS_INLINE auto insert(P&& value) -> iterator { return base_type::emplace(std::forward<P>(value)).first; }

		template< class P, typename std::enable_if<std::is_constructible<value_type, P>::value, int>::type = 0>
		SEQ_ALWAYS_INLINE auto insert(const_iterator hint, P&& value) -> iterator { return base_type::emplace_hint(hint, std::forward<P>(value)); }


		template< class InputIt >
		SEQ_ALWAYS_INLINE void insert(InputIt first, InputIt last) { return base_type::insert(first, last); }
		SEQ_ALWAYS_INLINE void insert(std::initializer_list<value_type> ilist) { return base_type::insert(ilist); }


		SEQ_ALWAYS_INLINE auto insert_pos(const value_type& value) -> size_t { return base_type::insert_pos(value).first; }
		SEQ_ALWAYS_INLINE auto insert_pos(value_type&& value) -> size_t { return base_type::insert_pos(std::move(value)).first; }
		template< class P, typename std::enable_if<std::is_constructible<value_type, P>::value, int>::type = 0 >
		SEQ_ALWAYS_INLINE auto insert_pos(P&& value) -> size_t { return base_type::insert_pos(std::forward<P>(value)).first; }

		template< class... Args >
		SEQ_ALWAYS_INLINE auto emplace(Args&&... args) -> iterator { return base_type::emplace(std::forward<Args>(args)...).first; }

		SEQ_ALWAYS_INLINE auto emplace(const value_type& value) -> iterator { return base_type::insert(value).first; }
		SEQ_ALWAYS_INLINE auto emplace(value_type&& value) -> iterator { return base_type::insert(std::move(value)).first; }
		template< class K >
		SEQ_ALWAYS_INLINE auto emplace(K && value) -> iterator { return base_type::emplace(value_type(std::forward<K>(value))).first; }
	
		template< class... Args >
		SEQ_ALWAYS_INLINE auto emplace_pos(Args&&... args) -> size_t { return base_type::emplace_pos(std::forward<Args>(args)...).first; }

	};






	/// @brief Checks if the contents of s1 and s2 are equal, that is, they have the same number of elements and each element in s1 compares equal with the element in s2 at the same position.
	template<class Key, class Compare , class Al1, class Al2 , bool S1 , bool S2, bool U1, bool U2 >
	auto operator==(const flat_set<Key, Compare, Al1, S1, U1>& s1, const flat_set<Key, Compare, Al2, S2, U2>& s2) -> bool
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
	template<class Key, class Compare, class Al1, class Al2,bool S1, bool S2, bool U1, bool U2 >
	auto operator!=(const flat_set<Key, Compare, Al1, S1, U1>& s1, const flat_set<Key, Compare, Al2, S2, U2>& s2) -> bool
	{
		return !(s1==s2);
	}
	/// @brief Erases all elements that satisfy the predicate p from the container.
	template<class Key, class Compare, class Al1, bool S1, bool U1, class Pred >
	auto erase_if(flat_set<Key, Compare, Al1, S1, U1>& s1,  Pred p) -> typename flat_set<Key, Compare, Al1, S1, U1>::size_type
	{
		using deque_type = typename flat_set<Key, Compare, Al1, S1, U1>::deque_type;
		// use const_cast to avoid flagging the flat_set as dirty
		deque_type& d = const_cast<deque_type&>(s1.cdeque());
		auto it = std::remove_if(d.begin(), d.end(), p);
		size_t res = d.end() - it;
		d.erase(it, d.end());
		d.manager()->update_all_back_values();
		return res;
	}

	/// @brief Checks if the contents of s1 and s2 are equal, that is, they have the same number of elements and each element in s1 compares equal with the element in s2 at the same position.
	template<class Key, class T, class Compare, class Al1, class Al2, bool S1, bool S2, bool U1, bool U2 >
	auto operator==(const flat_map<Key, T, Compare, Al1, S1, U1>& s1, const flat_map<Key, T, Compare, Al2, S2, U2>& s2) -> bool
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
	template<class Key, class T, class Compare, class Al1, class Al2, bool S1, bool S2, bool U1, bool U2 >
	auto operator!=(const flat_map<Key, T, Compare, Al1, S1, U1>& s1, const flat_map<Key, T, Compare, Al2, S2, U2>& s2) -> bool
	{
		return !(s1 == s2);
	}
	/// @brief Erases all elements that satisfy the predicate p from the container.
	template<class Key, class T, class Compare, class Al1, bool S1, bool U1, class Pred >
	auto erase_if(flat_map<Key, T, Compare, Al1, S1, U1>& s1, Pred p) -> typename flat_map<Key, T, Compare, Al1, S1, U1>::size_type
	{
		using deque_type = typename flat_map<Key, T, Compare, Al1, S1, U1>::deque_type;
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
