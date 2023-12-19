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

#ifndef SEQ_CONCURRENT_MAP_HPP
#define SEQ_CONCURRENT_MAP_HPP

#include "internal/concurrent_hash_table.hpp"


namespace seq
{
	

	template<
		class Key,
		class Hash = hasher<Key>,
		class Equal = equal_to<>,
		class Allocator = std::allocator<Key>,
		unsigned Shards = seq::medium_concurrency
	>
		class concurrent_set : private detail::ConcurrentHashTable<Key, Key, Hash, Equal, Allocator, Shards>
	{

		using base_type = detail::ConcurrentHashTable<Key, Key, Hash, Equal, Allocator, Shards>;
		using this_type = concurrent_set<Key, Hash, Equal, Allocator, Shards>;
		using extract_key = detail::ExtractKey<Key, Key >;

		template< class K, class H, class E>
		using is_transparent = std::integral_constant<bool, detail::key_transparent<K>::value&& detail::has_is_transparent<H>::value&& detail::has_is_transparent<E>::value>;

		template <class K, class H, class KE, class A, unsigned S>
		friend bool operator==(concurrent_set<K, H, KE, A, S> const& lhs,
			concurrent_set<K, H, KE, A, S> const& rhs);

		template <class K, class H, class KE, class A, unsigned S, class Predicate>
		friend typename concurrent_set<K, H, KE, A, S>::size_type erase_if(
			concurrent_set<K, H, KE, A, S>& set, Predicate pred);

		struct emplace_or_visit_impl {
			this_type* _this;
			template < class F, class... A>
			bool operator()(F&& f, A&&... a) {
				return _this->base_type::template emplace_policy<detail::InsertConcurrentPolicy>(std::forward<F>(f), std::forward<A>(a)...);
			}
		};
		struct emplace_or_cvisit_impl {
			this_type* _this;
			template < class F, class... A>
			bool operator()(F&& f, A&&... a) {
				return _this->base_type::template emplace_policy<detail::InsertConcurrentPolicy>([&](const auto& v) {std::forward<F>(f)(v); }, std::forward<A>(a)...);
			}
		};
	public:

		using key_type = Key;
		using value_type = Key;
		using allocator_type = Allocator;
		using size_type = size_t;
		using difference_type = std::ptrdiff_t;
		using hasher = Hash;
		using key_equal = Equal;
		using reference = value_type&;
		using const_reference = const value_type&;
		using pointer = typename std::allocator_traits<Allocator>::pointer;
		using const_pointer = typename std::allocator_traits<Allocator>::const_pointer;

		concurrent_set()
			:base_type()
		{}

		explicit concurrent_set(size_type n, const hasher& hf = hasher(),
			const key_equal& eql = key_equal(),
			const allocator_type& a = allocator_type())
			: base_type(hf, eql, a)
		{
			if (n) this->rehash(n);
		}

		template <class InputIterator>
		concurrent_set(InputIterator f, InputIterator l,
			size_type n = 0,
			const hasher& hf = hasher(), const key_equal& eql = key_equal(),
			const allocator_type& a = allocator_type())
			: base_type(hf, eql, a)
		{
			if (n) this->rehash(n);
			this->insert(f, l);
		}

		concurrent_set(const concurrent_set& other)
			:base_type(other)
		{}

		concurrent_set(concurrent_set&& other) noexcept(std::is_nothrow_move_constructible<base_type>::value)
			:base_type(std::move(other))
		{}

		template <class InputIterator>
		concurrent_set(
			InputIterator f, InputIterator l, allocator_type const& a)
			: concurrent_set(f, l, 0, hasher(), key_equal(), a)
		{}

		explicit concurrent_set(allocator_type const& a)
			: base_type(hasher(), key_equal(), a)
		{}

		concurrent_set(const concurrent_set& other, const allocator_type& alloc)
			: base_type(other, alloc)
		{}

		concurrent_set(concurrent_set&& other, const allocator_type& alloc)
			:base_type(std::move(other), alloc)
		{}

		concurrent_set(std::initializer_list<value_type> il,
			size_type n = 0,
			const hasher& hf = hasher(), const key_equal& eql = key_equal(),
			const allocator_type& a = allocator_type())
			: concurrent_set(n, hf, eql, a)
		{
			this->insert(il.begin(), il.end());
		}

		concurrent_set(size_type n, const allocator_type& a)
			: concurrent_set(n, hasher(), key_equal(), a)
		{
		}

		concurrent_set(
			size_type n, const hasher& hf, const allocator_type& a)
			: concurrent_set(n, hf, key_equal(), a)
		{
		}

		template <typename InputIterator>
		concurrent_set(
			InputIterator f, InputIterator l, size_type n, const allocator_type& a)
			: concurrent_set(f, l, n, hasher(), key_equal(), a)
		{
		}

		template <typename InputIterator>
		concurrent_set(InputIterator f, InputIterator l, size_type n,
			const hasher& hf, const allocator_type& a)
			: concurrent_set(f, l, n, hf, key_equal(), a)
		{
		}

		concurrent_set(
			std::initializer_list<value_type> il, const allocator_type& a)
			: concurrent_set(
				il, 0, hasher(), key_equal(), a)
		{
		}

		concurrent_set(std::initializer_list<value_type> il, size_type n,
			const allocator_type& a)
			: concurrent_set(il, n, hasher(), key_equal(), a)
		{
		}

		concurrent_set(std::initializer_list<value_type> il, size_type n,
			const hasher& hf, const allocator_type& a)
			: concurrent_set(il, n, hf, key_equal(), a)
		{
		}

		~concurrent_set() = default;


		auto operator=(const concurrent_set& other) -> concurrent_set&
		{
			static_cast<base_type&>(*this) = static_cast<const base_type&>(other);
			return *this;
		}
		auto operator=(concurrent_set&& other) noexcept(noexcept(std::declval< base_type&>().swap(std::declval< base_type&>()))) -> concurrent_set&
		{
			base_type::swap(other);
			return *this;
		}

		SEQ_ALWAYS_INLINE auto size() const noexcept -> size_t { return base_type::size(); }
		SEQ_ALWAYS_INLINE auto max_size() const noexcept -> size_t { return std::numeric_limits<size_t>::max(); }
		SEQ_ALWAYS_INLINE auto empty() const noexcept -> bool { return size() == 0; }
		SEQ_ALWAYS_INLINE auto load_factor() const noexcept -> float { return base_type::load_factor(); }
		SEQ_ALWAYS_INLINE auto max_load_factor() const noexcept -> float { return base_type::max_load_factor(); }
		SEQ_ALWAYS_INLINE void max_load_factor(float f) { base_type::max_load_factor(f); }
		SEQ_ALWAYS_INLINE auto get_allocator() const -> allocator_type { return base_type::get_safe_allocator(); }
		SEQ_ALWAYS_INLINE auto hash_function() const -> hasher { return base_type::get_hash_function(); }
		SEQ_ALWAYS_INLINE auto key_eq() const -> key_equal { return base_type::get_key_eq(); }

		SEQ_ALWAYS_INLINE void clear() { base_type::clear(); }
		SEQ_ALWAYS_INLINE void rehash(size_t n) { base_type::rehash(n); }
		SEQ_ALWAYS_INLINE void reserve(size_t size) { base_type::reserve(size); }
		SEQ_ALWAYS_INLINE void swap(concurrent_set& other) noexcept(noexcept(std::declval< base_type&>().swap(std::declval< base_type&>())))
		{
			base_type::swap(other);
		}


		template<class F>
		SEQ_ALWAYS_INLINE bool visit_all(F&& fun)
		{
			return base_type::visit_all(std::forward<F>(fun));
		}
		template<class F>
		SEQ_ALWAYS_INLINE bool visit_all(F&& fun) const
		{
			return base_type::visit_all(std::forward<F>(fun));
		}
		template<class F>
		SEQ_ALWAYS_INLINE bool cvisit_all(F&& fun) const
		{
			return base_type::visit_all(std::forward<F>(fun));
		}

#ifdef SEQ_HAS_CPP_17

		template<class ExecPolicy, class F>
		SEQ_ALWAYS_INLINE typename std::enable_if<detail::internal_is_execution_policy<ExecPolicy>::value,
			bool>::type visit_all(ExecPolicy&& p, F&& fun)
		{
			return base_type::visit_all(std::forward<ExecPolicy>(p), std::forward<F>(fun));
		}
		template<class ExecPolicy, class F>
		SEQ_ALWAYS_INLINE typename std::enable_if<detail::internal_is_execution_policy<ExecPolicy>::value,
			bool>::type visit_all(ExecPolicy&& p, F&& fun) const
		{
			return base_type::visit_all(std::forward<ExecPolicy>(p), std::forward<F>(fun));
		}
		template<class ExecPolicy, class F>
		SEQ_ALWAYS_INLINE typename std::enable_if<detail::internal_is_execution_policy<ExecPolicy>::value,
			bool>::type cvisit_all(ExecPolicy&& p, F&& fun) const
		{
			return base_type::visit_all(std::forward<ExecPolicy>(p), std::forward<F>(fun));
		}

#endif

		template<class F>
		SEQ_ALWAYS_INLINE size_type visit(const Key& key, F&& fun) const
		{
			return base_type::visit(key, std::forward<F>(fun));
		}
		template<class F>
		SEQ_ALWAYS_INLINE size_type cvisit(const Key& key, F&& fun) const
		{
			return base_type::visit(key, std::forward<F>(fun));
		}
		template<class F>
		SEQ_ALWAYS_INLINE size_type visit(const Key& key, F&& fun)
		{
			return base_type::visit(key, std::forward<F>(fun));
		}

		template <class K, class F>
		SEQ_ALWAYS_INLINE typename std::enable_if<
			is_transparent<K, Equal, Hash>::value, size_type>::type
			visit(const K& key, F&& fun) const
		{
			return base_type::visit(key, std::forward<F>(fun));
		}
		template <class K, class F>
		SEQ_ALWAYS_INLINE typename std::enable_if<
			is_transparent<K, Equal, Hash>::value, size_type>::type
			cvisit(const K& key, F&& fun) const
		{
			return base_type::visit(key, std::forward<F>(fun));
		}
		template <class K, class F>
		SEQ_ALWAYS_INLINE typename std::enable_if<
			is_transparent<K, Equal, Hash>::value, size_type>::type
			visit(const K& key, F&& fun)
		{
			return base_type::visit(key, std::forward<F>(fun));
		}


		template< class... Args >
		SEQ_ALWAYS_INLINE auto emplace(Args&&... args) -> bool
		{
			return base_type::emplace(std::forward<Args>(args)...);
		}
		template <class... Args>
		SEQ_ALWAYS_INLINE bool emplace_or_visit(Args&&... args)
		{
			return detail::ApplyFLast(emplace_or_visit_impl{ this }, std::forward<Args>(args)...);
		}
		template <class... Args>
		SEQ_ALWAYS_INLINE bool emplace_or_cvisit(Args&&... args)
		{
			return detail::ApplyFLast(emplace_or_cvisit_impl{ this }, std::forward<Args>(args)...);
		}


		SEQ_ALWAYS_INLINE auto insert(const value_type& value) -> bool
		{
			return base_type::emplace(value);
		}

		SEQ_ALWAYS_INLINE auto insert(value_type&& value) -> bool
		{
			return base_type::emplace(std::move(value));
		}

		template< class InputIt >
		SEQ_ALWAYS_INLINE void insert(InputIt first, InputIt last)
		{
			base_type::insert(first, last);
		}

		SEQ_ALWAYS_INLINE void insert(std::initializer_list<value_type> ilist)
		{
			insert(ilist.begin(), ilist.end());
		}



		template <class Ty, class F>
		SEQ_ALWAYS_INLINE bool insert_or_visit(Ty&& value, F&& f)
		{
			return base_type::template emplace_policy<detail::InsertConcurrentPolicy>(std::forward<F>(f), std::forward<Ty>(value));
		}
		template <class InputIterator, class F>
		void insert_or_visit(InputIterator first, InputIterator last, F&& f)
		{
			for (; first != last; ++first)
				insert_or_visit(*first, std::forward<F>(f));
		}
		template <class F>
		void insert_or_visit(std::initializer_list<value_type> ilist, F&& f)
		{
			this->insert_or_visit(ilist.begin(), ilist.end(), std::forward<F>(f));
		}


		template <class Ty, class F>
		SEQ_ALWAYS_INLINE bool insert_or_cvisit(Ty&& value, F&& f)
		{
			return base_type::template emplace_policy<detail::InsertConcurrentPolicy>([&](const auto& v) {std::forward<F>(f)(v); }, std::forward<Ty>(value));
		}
		template <class InputIterator, class F>
		void insert_or_cvisit(InputIterator first, InputIterator last, F&& f)
		{
			for (; first != last; ++first)
				insert_or_cvisit(*first, std::forward<F>(f));
		}
		template <class F>
		void insert_or_cvisit(std::initializer_list<value_type> ilist, F&& f)
		{
			this->insert_or_cvisit(ilist.begin(), ilist.end(), std::forward<F>(f));
		}



		SEQ_ALWAYS_INLINE auto erase(const Key& key) -> size_type
		{
			return base_type::erase(key, [](const auto&) {return true; });
		}

		template <class K>
		SEQ_ALWAYS_INLINE typename std::enable_if<
			is_transparent<K, Equal, Hash>::value, size_type>::type
			erase(const K& key)
		{
			return base_type::erase(key, [](const auto&) {return true; });
		}

		template<class F>
		SEQ_ALWAYS_INLINE auto erase_if(const Key& key, F&& fun) -> size_type
		{
			return base_type::erase(key, std::forward<F>(fun));
		}

		template <class K, class F>
		SEQ_ALWAYS_INLINE typename std::enable_if<
			is_transparent<K, Equal, Hash>::value &&
			!detail::internal_is_execution_policy<K>::value, size_type>::type
			erase_if(const K& key, F&& fun)
		{
			return base_type::erase(key, std::forward<F>(fun));
		}


		template <class F>
		SEQ_ALWAYS_INLINE size_type erase_if(F&& fun)
		{
			return base_type::erase_if(std::forward<F>(fun));
		}

#ifdef SEQ_HAS_CPP_17

		template <class ExecPolicy, class F>
		SEQ_ALWAYS_INLINE typename std::enable_if<detail::internal_is_execution_policy<ExecPolicy>::value,
			size_type>::type
			erase_if(ExecPolicy&& p, F&& fun)
		{
			return base_type::erase_if(p, std::forward<F>(fun));
		}

#endif

		SEQ_ALWAYS_INLINE auto count(const Key& key) const -> size_type
		{
			return base_type::count(key);
		}

		template <class K>
		SEQ_ALWAYS_INLINE typename std::enable_if<
			is_transparent<K, Equal, Hash>::value, size_type>::type
			count(const K& key) const
		{
			return base_type::count(key);
		}

		SEQ_ALWAYS_INLINE bool contains(const Key& key) const
		{
			return base_type::contains(key);
		}

		template <class K>
		SEQ_ALWAYS_INLINE typename std::enable_if<
			is_transparent<K, Equal, Hash>::value, bool>::type
			contains(const K& key) const
		{
			return base_type::contains(key);
		}

		template <class H2, class P2>
		size_type merge(concurrent_set<Key, H2, P2, Allocator, Shards>& x)
		{
			SEQ_ASSERT_DEBUG(get_allocator() == x.get_allocator(), "");
			return base_type::merge(x);
		}

		template <class H2, class P2>
		size_type merge(concurrent_set<Key, H2, P2, Allocator, Shards>&& x)
		{
			return merge(x);
		}

#ifdef SEQ_HAS_CPP_17
		template <class ExecPolicy, class H2, class P2>
		typename std::enable_if<detail::internal_is_execution_policy<ExecPolicy>::value,
			size_type>::type merge(ExecPolicy&& p, concurrent_set<Key, H2, P2, Allocator, Shards>& x)
		{
			SEQ_ASSERT_DEBUG(get_allocator() == x.get_allocator(), "");
			return base_type::merge(std::forward<ExecPolicy>(p), x);
		}

		template <class ExecPolicy, class H2, class P2>
		typename std::enable_if<detail::internal_is_execution_policy<ExecPolicy>::value,
			size_type>::type  merge(ExecPolicy&& p, concurrent_set<Key, H2, P2, Allocator, Shards>&& x)
		{
			return merge(std::forward<ExecPolicy>(p), x);
		}
#endif
	};


	template<
		class Key,
		class Hash,
		class Equal,
		class Allocator,
		unsigned Shards
	>
		bool operator==(const concurrent_set<Key, Hash, Equal, Allocator, Shards>& left, const concurrent_set<Key, Hash, Equal, Allocator, Shards>& right)
	{
		using base = typename concurrent_set<Key, Hash, Equal, Allocator, Shards>::base_type;
		return static_cast<const base&>(left) == static_cast<const base&>(right);
	}
	template<
		class Key,
		class Hash,
		class Equal,
		class Allocator,
		unsigned Shards
	>
		bool operator!=(const concurrent_set<Key, Hash, Equal, Allocator, Shards>& left, const concurrent_set<Key, Hash, Equal, Allocator, Shards>& right)
	{
		return !(left == right);
	}

	template<
		class Key,
		class Hash,
		class Equal,
		class Allocator,
		unsigned Shards,
		class Predicate
	>
		typename concurrent_set<Key, Hash, Equal, Allocator, Shards>::size_type erase_if(concurrent_set<Key, Hash, Equal, Allocator, Shards>& set, Predicate pred)
	{
		set.erase_if(pred);
	}






	
	template<
		class Key,
		class T,
		class Hash = hasher<Key>,
		class Equal = equal_to<>,
		class Allocator = std::allocator<std::pair< Key,T> >,
		unsigned Shards = seq::medium_concurrency
	>
		class concurrent_map : private detail::ConcurrentHashTable<Key, std::pair< Key, T>, Hash, Equal, Allocator, Shards>
	{
		
		using base_type = detail::ConcurrentHashTable<Key, std::pair< Key, T>, Hash, Equal, Allocator, Shards>;
		using this_type = concurrent_map<Key, T, Hash, Equal, Allocator, Shards>;
		using extract_key = detail::ExtractKey<Key, std::pair< Key, T> >;

		template< class K, class H, class E>
		using is_transparent = std::integral_constant<bool,detail::key_transparent<K>::value && detail::has_is_transparent<H>::value&& detail::has_is_transparent<E>::value>;

		template <class K, class V, class H, class KE, class A, unsigned S>
		friend bool operator==(concurrent_map<K, V, H, KE, A, S> const& lhs,
			concurrent_map<K, V, H, KE, A, S> const& rhs);

		template <class K, class V, class H, class KE, class A, unsigned S, class Predicate>
		friend typename concurrent_map<K, V, H, KE, A, S>::size_type erase_if(
			concurrent_map<K, V, H, KE, A, S>& set, Predicate pred);


		struct emplace_or_visit_impl {
			this_type* _this;
			template < class F, class... A>
			bool operator()(F&& f, A&&... a) {
				return _this->base_type::template emplace_policy<detail::InsertConcurrentPolicy>(std::forward<F>(f), std::forward<A>(a)...);
			}
		};
		struct emplace_or_cvisit_impl {
			this_type* _this;
			template < class F, class... A>
			bool operator()(F&& f, A&&... a) {
				return _this->base_type::template emplace_policy<detail::InsertConcurrentPolicy>([&](const auto& v) {std::forward<F>(f)(v); }, std::forward<A>(a)...);
			}
		};

		struct try_emplace_or_visit_impl {
			this_type* _this;
			template < class F, class... A>
			bool operator()(F&& f, A&&... a) {
				return _this->base_type::template emplace_policy<detail::TryInsertConcurrentPolicy>(std::forward<F>(f), std::forward<A>(a)...);
			}
		};
		struct try_emplace_or_cvisit_impl {
			this_type* _this;
			template < class F, class... A>
			bool operator()(F&& f, A&&... a) {
				return _this->base_type::template emplace_policy<detail::TryInsertConcurrentPolicy>([&](const auto& v) {std::forward<F>(f)(v); }, std::forward<A>(a)...);
			}
		};


	public:

		using key_type = Key;
		using mapped_type = T;
		using value_type = std::pair< Key, T>;
		using allocator_type = Allocator;
		using size_type = size_t;
		using difference_type = std::ptrdiff_t;
		using hasher = Hash;
		using key_equal = Equal;
		using reference = value_type&;
		using const_reference = const value_type&;
		using pointer = typename std::allocator_traits<Allocator>::pointer;
		using const_pointer = typename std::allocator_traits<Allocator>::const_pointer;

		concurrent_map() 
			:base_type()
		{}

		explicit concurrent_map(size_type n, const hasher& hf = hasher(),
			const key_equal& eql = key_equal(),
			const allocator_type& a = allocator_type())
			: base_type(hf, eql, a)
		{
			if(n) this->rehash(n);
		}

		template <class InputIterator>
		concurrent_map(InputIterator f, InputIterator l,
			size_type n = 0,
			const hasher& hf = hasher(), const key_equal& eql = key_equal(),
			const allocator_type& a = allocator_type())
			: base_type(hf, eql, a)
		{
			if(n) this->rehash(n);
			this->insert(f, l);
		}

		concurrent_map(const concurrent_map& other)
			:base_type(other)
		{}

		concurrent_map(concurrent_map&& other) noexcept(std::is_nothrow_move_constructible< base_type>::value)
			:base_type(std::move(other))
		{}

		template <class InputIterator>
		concurrent_map(
			InputIterator f, InputIterator l, allocator_type const& a)
			: concurrent_map(f, l, 0, hasher(), key_equal(), a)
		{}

		explicit concurrent_map(allocator_type const& a)
			: base_type(hasher(), key_equal(), a)
		{}

		concurrent_map(const concurrent_map& other, const allocator_type& alloc)
			: base_type(other, alloc)
		{}

		concurrent_map(concurrent_map&& other, const allocator_type& alloc)
			:base_type(std::move(other), alloc)
		{}

		concurrent_map(std::initializer_list<value_type> il,
			size_type n = 0,
			const hasher& hf = hasher(), const key_equal& eql = key_equal(),
			const allocator_type& a = allocator_type())
			: concurrent_map(n, hf, eql, a)
		{
			this->insert(il.begin(), il.end());
		}

		concurrent_map(size_type n, const allocator_type& a)
			: concurrent_map(n, hasher(), key_equal(), a)
		{
		}

		concurrent_map(
			size_type n, const hasher& hf, const allocator_type& a)
			: concurrent_map(n, hf, key_equal(), a)
		{
		}

		template <typename InputIterator>
		concurrent_map(
			InputIterator f, InputIterator l, size_type n, const allocator_type& a)
			: concurrent_map(f, l, n, hasher(), key_equal(), a)
		{
		}

		template <typename InputIterator>
		concurrent_map(InputIterator f, InputIterator l, size_type n,
			const hasher& hf, const allocator_type& a)
			: concurrent_map(f, l, n, hf, key_equal(), a)
		{
		}

		concurrent_map(
			std::initializer_list<value_type> il, const allocator_type& a)
			: concurrent_map(
				il, 0, hasher(), key_equal(), a)
		{
		}

		concurrent_map(std::initializer_list<value_type> il, size_type n,
			const allocator_type& a)
			: concurrent_map(il, n, hasher(), key_equal(), a)
		{
		}

		concurrent_map(std::initializer_list<value_type> il, size_type n,
			const hasher& hf, const allocator_type& a)
			: concurrent_map(il, n, hf, key_equal(), a)
		{
		}

		~concurrent_map() = default;


		auto operator=(const concurrent_map& other) -> concurrent_map&
		{
			static_cast<base_type&>(*this) = static_cast<const base_type&>(other);
			return *this;
		}
		auto operator=(concurrent_map&& other) noexcept(noexcept(std::declval< base_type&>().swap(std::declval< base_type&>()))) -> concurrent_map&
		{
			base_type::swap(other);
			return *this;
		}

		SEQ_CONCURRENT_INLINE auto size() const noexcept -> size_t { return base_type::size(); }
		SEQ_CONCURRENT_INLINE auto max_size() const noexcept -> size_t { return std::numeric_limits<size_t>::max(); }
		SEQ_CONCURRENT_INLINE auto empty() const noexcept -> bool { return size() == 0; }
		SEQ_CONCURRENT_INLINE auto load_factor() const noexcept -> float { return base_type::load_factor(); }
		SEQ_CONCURRENT_INLINE auto max_load_factor() const noexcept -> float { return base_type::max_load_factor(); }
		SEQ_CONCURRENT_INLINE void max_load_factor(float f)  { base_type::max_load_factor(f); }
		SEQ_CONCURRENT_INLINE auto get_allocator() const -> allocator_type { return base_type::get_safe_allocator(); }
		SEQ_CONCURRENT_INLINE auto hash_function() const -> hasher { return base_type::get_hash_function(); }
		SEQ_CONCURRENT_INLINE auto key_eq() const  -> key_equal { return base_type::get_key_eq(); }

		SEQ_CONCURRENT_INLINE void clear(){base_type::clear();}
		SEQ_CONCURRENT_INLINE void rehash(size_t n) { base_type::rehash(n); }
		SEQ_CONCURRENT_INLINE void reserve(size_t size){base_type::reserve(size);}
		SEQ_CONCURRENT_INLINE void swap(concurrent_map& other)   noexcept(noexcept(std::declval< base_type&>().swap(std::declval< base_type&>())))
		{base_type::swap(other);}


		template<class F>
		SEQ_CONCURRENT_INLINE bool visit_all(F&& fun)
		{
			return base_type::visit_all(std::forward<F>(fun));
		}
		template<class F>
		SEQ_CONCURRENT_INLINE bool visit_all(F&& fun) const
		{
			return base_type::visit_all(std::forward<F>(fun));
		}
		template<class F>
		SEQ_CONCURRENT_INLINE bool cvisit_all(F&& fun) const
		{
			return base_type::visit_all(std::forward<F>(fun));
		}

#ifdef SEQ_HAS_CPP_17

		template<class ExecPolicy, class F>
		SEQ_CONCURRENT_INLINE typename std::enable_if<detail::internal_is_execution_policy<ExecPolicy>::value,
			bool>::type visit_all(ExecPolicy&& p, F&& fun)
		{
			return base_type::visit_all(std::forward<ExecPolicy>(p), std::forward<F>(fun));
		}
		template<class ExecPolicy, class F>
		SEQ_CONCURRENT_INLINE typename std::enable_if<detail::internal_is_execution_policy<ExecPolicy>::value,
			bool>::type visit_all(ExecPolicy&& p, F&& fun) const
		{
			return base_type::visit_all(std::forward<ExecPolicy>(p), std::forward<F>(fun));
		}
		template<class ExecPolicy, class F>
		SEQ_CONCURRENT_INLINE typename std::enable_if<detail::internal_is_execution_policy<ExecPolicy>::value,
			bool>::type cvisit_all(ExecPolicy&& p, F&& fun) const
		{
			return base_type::visit_all(std::forward<ExecPolicy>(p), std::forward<F>(fun));
		}

#endif

		template<class F>
		SEQ_CONCURRENT_INLINE size_type visit(const Key& key, F&& fun) const
		{
			return base_type::visit(key, std::forward<F>(fun));
		}
		template<class F>
		SEQ_CONCURRENT_INLINE size_type cvisit(const Key& key, F&& fun) const
		{
			return base_type::visit(key, std::forward<F>(fun));
		}
		template<class F>
		SEQ_CONCURRENT_INLINE size_type visit(const Key& key, F&& fun)
		{
			return base_type::visit(key, std::forward<F>(fun));
		}

		template <class K, class F>
		SEQ_CONCURRENT_INLINE typename std::enable_if<
			is_transparent<K, Equal, Hash>::value, size_type>::type
		visit(const K& key, F&& fun) const
		{
			return base_type::visit(key, std::forward<F>(fun));
		}
		template <class K, class F>
		SEQ_CONCURRENT_INLINE typename std::enable_if<
			is_transparent<K, Equal, Hash>::value, size_type>::type
		cvisit(const K& key, F&& fun) const
		{
			return base_type::visit(key, std::forward<F>(fun));
		}
		template <class K, class F>
		SEQ_CONCURRENT_INLINE typename std::enable_if<
			is_transparent<K, Equal, Hash>::value, size_type>::type
		visit(const K& key, F&& fun)
		{
			return base_type::visit(key, std::forward<F>(fun));
		}


		template< class... Args >
		SEQ_CONCURRENT_INLINE auto emplace(Args&&... args) -> bool
		{
			return base_type::emplace(std::forward<Args>(args)...);
		}

		template < class... Args>
		SEQ_CONCURRENT_INLINE bool emplace_or_visit(Args&&... args)
		{
			return detail::ApplyFLast(emplace_or_visit_impl{ this }, std::forward<Args>(args)...);
		}
		template < class... Args>
		SEQ_CONCURRENT_INLINE bool emplace_or_cvisit(Args&&... args)
		{
			return detail::ApplyFLast(emplace_or_cvisit_impl{ this }, std::forward<Args>(args)...);
		}


		SEQ_CONCURRENT_INLINE auto insert(const value_type& value) -> bool
		{
			return base_type::emplace(value);
		}

		SEQ_CONCURRENT_INLINE auto insert(value_type&& value) -> bool
		{
			return base_type::emplace(std::move(value));
		}

		template< class P, typename std::enable_if<std::is_constructible<value_type, P>::value, int>::type = 0>
		SEQ_CONCURRENT_INLINE auto insert(P&& value) -> bool
		{
			return base_type::emplace(std::forward<P>(value));
		}
		
		template< class InputIt >
		SEQ_CONCURRENT_INLINE void insert(InputIt first, InputIt last)
		{
			base_type::insert(first,last);
		}

		SEQ_CONCURRENT_INLINE void insert(std::initializer_list<value_type> ilist)
		{
			insert(ilist.begin(), ilist.end());
		}

		template <class M>
		SEQ_CONCURRENT_INLINE bool insert_or_assign(key_type const& k, M&& obj)
		{
			return base_type::template emplace_policy<detail::TryInsertConcurrentPolicy>([&](auto& p) {p.second = std::forward<M>(obj); }, k, std::forward<M>(obj));
		}

		template <class M>
		SEQ_CONCURRENT_INLINE bool insert_or_assign(key_type && k, M&& obj)
		{
			return base_type::template emplace_policy<detail::TryInsertConcurrentPolicy>([&](auto& p) {p.second = std::forward<M>(obj); }, std::move(k), std::forward<M>(obj));
		}

		template <class K, class M>
		SEQ_CONCURRENT_INLINE typename std::enable_if<
			is_transparent<K, Equal,Hash>::value, bool>::type
			insert_or_assign(K&& k, M&& obj)
		{
			return base_type::template emplace_policy<detail::TryInsertConcurrentPolicy>([&](auto& p) {p.second = std::forward<M>(obj); }, std::forward<K>(k), std::forward<M>(obj));
		}



		template <class Ty, class F>
		SEQ_CONCURRENT_INLINE bool insert_or_visit(Ty&& value, F && f)
		{
			return base_type::template emplace_policy<detail::InsertConcurrentPolicy>(std::forward<F>(f), std::forward<Ty>(value));
		}
		template <class InputIterator, class F>
		void insert_or_visit(InputIterator first, InputIterator last, F && f)
		{
			for (; first != last; ++first) 
				insert_or_visit(*first, std::forward<F>(f));
		}
		template <class F>
		void insert_or_visit(std::initializer_list<value_type> ilist, F && f)
		{
			this->insert_or_visit(ilist.begin(), ilist.end(), std::forward<F>(f));
		}


		template <class Ty, class F>
		SEQ_CONCURRENT_INLINE bool insert_or_cvisit(Ty&& value, F&& f)
		{
			return base_type::template emplace_policy<detail::InsertConcurrentPolicy>([&](const auto& v) {std::forward<F>(f)(v); }, std::forward<Ty>(value));
		}
		template <class InputIterator, class F>
		void insert_or_cvisit(InputIterator first, InputIterator last, F&& f)
		{
			for (; first != last; ++first)
				insert_or_cvisit(*first, std::forward<F>(f));
		}
		template <class F>
		void insert_or_cvisit(std::initializer_list<value_type> ilist, F&& f)
		{
			this->insert_or_cvisit(ilist.begin(), ilist.end(), std::forward<F>(f));
		}





		template <class... Args>
		SEQ_CONCURRENT_INLINE bool try_emplace(key_type const& k, Args&&... args)
		{
			return base_type::template emplace_policy<detail::TryInsertConcurrentPolicy>([](auto& ) {},k, std::forward<Args>(args)...);
		}

		template <class... Args>
		SEQ_CONCURRENT_INLINE bool try_emplace(key_type&& k, Args&&... args)
		{
			return base_type::template emplace_policy<detail::TryInsertConcurrentPolicy>([](auto&) {}, std::move(k), std::forward<Args>(args)...);
		}

		template <class K, class... Args>
		SEQ_CONCURRENT_INLINE typename std::enable_if<
			is_transparent<K, Equal, Hash>::value, bool>::type
			try_emplace(K&& k, Args&&... args)
		{
			return base_type::template emplace_policy<detail::TryInsertConcurrentPolicy>([](auto&) {}, std::forward<K>(k), std::forward<Args>(args)...);
		}


		template <class... Args>
		SEQ_CONCURRENT_INLINE bool try_emplace_or_visit(key_type const& k,  Args&&... args)
		{
			return detail::ApplyFLast(try_emplace_or_visit_impl{ this }, k, std::forward<Args>(args)...);
		}

		template <class... Args>
		SEQ_CONCURRENT_INLINE bool try_emplace_or_visit(key_type&& k, Args&&... args)
		{
			return detail::ApplyFLast(try_emplace_or_visit_impl{ this }, std::move( k), std::forward<Args>(args)...);
		}

		template <class K, class... Args>
		SEQ_CONCURRENT_INLINE typename std::enable_if<
			is_transparent<K, Equal, Hash>::value, bool>::type
			try_emplace_or_visit(K&& k, Args&&... args)
		{
			return detail::ApplyFLast(try_emplace_or_visit_impl{ this }, std::forward<K>(k), std::forward<Args>(args)...);
		}



		template <class... Args>
		SEQ_CONCURRENT_INLINE bool try_emplace_or_cvisit(key_type const& k, Args&&... args)
		{
			return detail::ApplyFLast(try_emplace_or_cvisit_impl{ this }, k, std::forward<Args>(args)...);
		}

		template <class... Args>
		SEQ_CONCURRENT_INLINE bool try_emplace_or_cvisit(key_type&& k, Args&&... args)
		{
			return detail::ApplyFLast(try_emplace_or_cvisit_impl{ this }, std::move(k), std::forward<Args>(args)...);
		}

		template <class K, class... Args>
		SEQ_CONCURRENT_INLINE typename std::enable_if<
			is_transparent<K, Equal, Hash>::value, bool>::type
			try_emplace_or_cvisit(K&& k, Args&&... args)
		{
			return detail::ApplyFLast(try_emplace_or_cvisit_impl{ this }, std::forward<K>(k), std::forward<Args>(args)...);
		}


		SEQ_CONCURRENT_INLINE auto erase(const Key& key) -> size_type
		{
			return base_type::erase(key, [](const auto&) {return true; });
		}

		template <class K>
		SEQ_CONCURRENT_INLINE typename std::enable_if<
			is_transparent<K, Equal, Hash>::value, size_type>::type
			erase(const K& key)
		{
			return base_type::erase(key, [](const auto&) {return true; });
		}

		template<class F>
		SEQ_CONCURRENT_INLINE auto erase_if(const Key& key, F && fun) -> size_type
		{
			return base_type::erase(key,std::forward<F>(fun));
		}

		template <class K, class F>
		SEQ_CONCURRENT_INLINE typename std::enable_if<
			is_transparent<K, Equal, Hash>::value &&
			!detail::internal_is_execution_policy<K>::value, size_type>::type
			erase_if(const K& key, F&& fun)
		{
			return base_type::erase(key, std::forward<F>(fun));
		}


		template <class F> 
		SEQ_CONCURRENT_INLINE size_type erase_if(F && fun)
		{
			return base_type::erase_if(std::forward<F>(fun));
		}

#ifdef SEQ_HAS_CPP_17

		template <class ExecPolicy, class F>
		SEQ_CONCURRENT_INLINE typename std::enable_if<detail::internal_is_execution_policy<ExecPolicy>::value,
			size_type>::type
			erase_if(ExecPolicy&& p, F && fun)
		{
			return base_type::erase_if(p, std::forward<F>(fun));
		}

#endif

		SEQ_CONCURRENT_INLINE auto count(const Key& key) const -> size_type
		{
			return base_type::count(key);
		}

		template <class K>
		SEQ_CONCURRENT_INLINE typename std::enable_if<
			is_transparent<K, Equal, Hash>::value, size_type>::type
		count(const K& key) const 
		{
			return base_type::count(key);
		}

		SEQ_CONCURRENT_INLINE bool contains(const Key& key) const
		{
			return base_type::contains(key);
		}

		template <class K>
		SEQ_CONCURRENT_INLINE typename std::enable_if<
			is_transparent<K, Equal, Hash>::value, bool>::type
		contains(const K& key) const
		{
			return base_type::contains(key);
		}
		
		template <class H2, class P2>
		size_type merge(concurrent_map<Key, T, H2, P2, Allocator, Shards>& x)
		{
			SEQ_ASSERT_DEBUG(get_allocator() == x.get_allocator(),"");
			return base_type::merge(x);
		}

		template <class H2, class P2>
		size_type merge(concurrent_map<Key, T, H2, P2, Allocator, Shards> && x)
		{
			return merge(x);
		}

#ifdef SEQ_HAS_CPP_17
		template <class ExecPolicy, class H2, class P2>
		typename std::enable_if<detail::internal_is_execution_policy<ExecPolicy>::value,
			size_type>::type merge(ExecPolicy && p, concurrent_map<Key, T, H2, P2, Allocator, Shards>& x)
		{
			SEQ_ASSERT_DEBUG(get_allocator() == x.get_allocator(), "");
			return base_type::merge(std::forward<ExecPolicy>(p), x);
		}

		template <class ExecPolicy, class H2, class P2>
		typename std::enable_if<detail::internal_is_execution_policy<ExecPolicy>::value,
			size_type>::type  merge(ExecPolicy&& p, concurrent_map<Key, T, H2, P2, Allocator, Shards>&& x)
		{
			return merge(std::forward<ExecPolicy>(p), x);
		}
#endif
	};


	template<
		class Key,
		class T,
		class Hash ,
		class Equal ,
		class Allocator ,
		unsigned Shards
	>
		bool operator==(const concurrent_map<Key, T, Hash, Equal, Allocator, Shards>& left, const concurrent_map<Key, T, Hash, Equal, Allocator, Shards>& right)
	{
		using base = typename concurrent_map<Key, T, Hash, Equal, Allocator, Shards>::base_type;
		return static_cast<const base&>(left) == static_cast<const base&>(right);
	}
	template<
		class Key,
		class T,
		class Hash ,
		class Equal ,
		class Allocator ,
		unsigned Shards
	>
		bool operator!=(const concurrent_map<Key, T, Hash, Equal, Allocator, Shards>& left, const concurrent_map<Key, T, Hash, Equal, Allocator, Shards>& right)
	{
		return !(left == right);
	}

	template<
		class Key,
		class T,
		class Hash ,
		class Equal,
		class Allocator ,
		unsigned Shards,
		class Predicate
	>
		typename concurrent_map<Key, T, Hash, Equal, Allocator, Shards>::size_type erase_if(concurrent_map<Key, T, Hash, Equal, Allocator, Shards>& set, Predicate pred)
	{
		set.erase_if(pred);
	}

}

#endif
