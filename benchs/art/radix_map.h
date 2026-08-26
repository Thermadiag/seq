#ifndef REFERENCE_ART_MAP_H
#define REFERENCE_ART_MAP_H

#include <type_traits>
#include "ar_prefix_tree.h"
#include "ar_tree.h"

namespace art {
    namespace detail {
        template<typename _Pair>
        struct Select1st : public unary_function<_Pair, typename _Pair::first_type> {

            typename _Pair::first_type &operator()(_Pair &__x) const {
                return __x.first;
            }

            const typename _Pair::first_type &operator()(const _Pair &__x) const {
                return __x.first;
            }
        };
    }

    /**
     * @brief A container made up of (key,value) pairs, which can be
     * retrieved based on a key, in linear time in size of the key.
     *
     *  @tparam _Key  Type of key objects.
     *  @tparam  _Tp  Type of mapped objects.
     *  @tparam _Key_transform  Key transformation function object type,
     *                          defaults to key_transform<_Key>.
     *
     *  Meets the requirements of a <a href="tables.html#65">container</a>, a
     *  <a href="tables.html#66">reversible container</a>, and an
     *  <a href="tables.html#69">associative container</a> (using unique keys).
     *
     *  Maps support bidirectional iterators.
     */
    template<typename _Key, typename _T,
            typename _Key_transform = key_transform<_Key> >
    class radix_map {

    public:
        typedef _Key key_type;
        typedef _T mapped_type;
        typedef std::pair<const _Key, _T> value_type;
        typedef _Key_transform key_transformer_type;
        //typedef _Alloc allocator_type;
        typedef value_type &reference;
        typedef const value_type &const_reference;

    private:
        //typedef typename _Alloc::value_type _Alloc_value_type;

        /**
         * Switch between art implementation with and without path compression
         * (prefixes in nodes) based on (transformed) key length. For short keys,
         * the cost of handling prefixes outweigh the benefits.
         */
        typedef typename std::conditional<sizeof(decltype(_Key_transform()(_Key()))) <= 6,
                ar_tree<key_type, value_type,
                        detail::Select1st<value_type>, _Key_transform>,
                ar_prefix_tree<key_type, value_type,
                        detail::Select1st<value_type>, _Key_transform>>::type _Rep_type;

        _Rep_type _M_t;

    public:

        //typedef typename _Alloc_traits::pointer            pointer;
        //typedef typename _Alloc_traits::const_pointer      const_pointer;
        // Bidirectional iterator
        typedef typename _Rep_type::iterator iterator;
        typedef typename _Rep_type::const_iterator const_iterator;
        typedef typename _Rep_type::size_type size_type;
        typedef typename _Rep_type::difference_type difference_type;
        typedef typename _Rep_type::reverse_iterator reverse_iterator;
        typedef typename _Rep_type::const_reverse_iterator const_reverse_iterator;


        class value_compare : public binary_function<value_type, value_type, bool> {

            friend class radix_map<_Key, _T, _Key_transform>;

        protected:
            _Key_transform key_transformer;

            value_compare(_Key_transform __key_transformer)
                    : key_transformer(__key_transformer) {}

        public:
            typedef typename ar_prefix_tree<_Key, _T, _Key_transform>::Key transformed_key_type;

            bool operator()(const value_type &__x, const value_type &__y) const {
                transformed_key_type x_key = {key_transformer(__x.first)};
                transformed_key_type y_key = {key_transformer(__y.first)};
                for (int i = 0; i < sizeof(transformed_key_type); i++) {
                    if (x_key.chunks[i] < y_key.chunks[i])
                        return true;
                    if (x_key.chunks[i] > y_key.chunks[i])
                        return false;
                }
                return false;
            }
        };

        /**
         * @brief  Default constructor creates no elements.
         */
        radix_map() : _M_t() {}

        /**
         * @brief  Map copy constructor.
         */
        radix_map(const radix_map &__x) : _M_t(__x._M_t) {}

        /**
         * @brief  Map move constructor.
         */
        radix_map(radix_map &&__x) : _M_t(std::move(__x._M_t)) {}

        /**
         *  @brief  Builds a map from a range.
         *  @param  __first  An input iterator.
         *  @param  __last  An input iterator.
         *
         */
        template<typename _InputIterator>
        radix_map(_InputIterator __first, _InputIterator __last)
                : _M_t() {
            _M_t.insert_unique(__first, __last);
        }

        /**
         *  @brief  Builds a map from an initializer_list.
         *  @param  __l  An initializer_list.
         *  @param  __key_transformer  A key transformation functor.
         *
         */
        radix_map(std::initializer_list<value_type> __l,
                  const _Key_transform &__key_transformer = _Key_transform())
                : _M_t(__key_transformer) {
            _M_t.insert_unique(__l.begin(), __l.end());
        }

        /**
         *  @brief  Map assignment operator.
         *  @param  __x  A map with identical elements.
         *
         */
        radix_map &operator=(const radix_map &__x) {
            _M_t = __x._M_t;
            return *this;
        }

        /**
         * @brief  Move assignment operator.
         */
        radix_map &operator=(radix_map &&__x) = default;

        /**
         *  @brief  Map list assignment operator.
         *  @param  __l  An initializer_list.
         *
         *  Fills the map with the contents of an initializer_list.
         *  Already existing elements in a map will be lost.
         */
        radix_map &operator=(std::initializer_list<value_type> __l) {
            _M_t.assign_unique(__l.begin(), __l.end());
            return *this;
        }

        // Capacity

        /**
         * Returns true if the map is empty.
         */
        bool empty() const noexcept {
            return _M_t.empty();
        }

        /**
         * Returns the size of the map.
         */
        size_type size() const noexcept {
            return _M_t.size();
        }

        /**
         * Returns the maximum size of the map.
         */
        size_type max_size() const noexcept {
            return _M_t.max_size();
        }

        // Modifiers

        /**
         *  Erases all elements in a map.
         */
        void clear() { _M_t.clear(); }

        /**
         *  @brief Attempts to insert a std::pair into the map.

         *  @param __x Pair to be inserted.
         *
         *  @return  A pair, of which the first element is an iterator that
         *           points to the possibly inserted pair, and the second is
         *           a bool that is true if the pair was actually inserted.
         *
         *  This function attempts to insert a (key, value) %pair into the map.
         *  A map relies on unique keys and thus a %pair is only inserted if its
         *  first element (the key) is not already present in the map.
         *
         *  Insertion requires O(k) time.
         */
        std::pair<iterator, bool> insert(const value_type &__x) {
            return _M_t.insert_unique(__x);
        }

        /**
         *  @brief Attempts to insert a range of elements.
         *  @param  __first  Iterator pointing to the start of the range.
         *  @param  __last  Iterator pointing to the end of the range.
         */
        template<typename _InputIterator>
        void insert(_InputIterator __first, _InputIterator __last) {
            _M_t.insert_unique(__first, __last);
        }

        /**
         *  @brief Attempts to insert a list of std::pairs into the map.
         *  @param  __list  A std::initializer_list<value_type> of pairs to be
         *                  inserted.
         */
        void insert(std::initializer_list<value_type> __list) {
            insert(__list.begin(), __list.end());
        }

        /**
         *  @brief Attempts to build and insert an element into the map.
         *  @param __args  Arguments used to generate an element.
         *  @return  A pair, of which the first element is an iterator that points
         *           to the possibly inserted element, and the second is a bool
         *           that is true if the element was actually inserted.
         */
        template<typename... _Args>
        std::pair<iterator, bool> emplace(_Args &&... __args) {
            return _M_t.emplace_unique(std::forward<_Args>(__args)...);
        }

        /**
         *  @brief Attempts to erase the element with the given key (if it exists).
         *  @param  __k The key to erase.
         *  @return The number of erased elements (0 or 1).
         *
         * Note that this function only erases the element, and that if
         * the element is itself a pointer, the pointed-to memory is not touched
         * in any way.
         */
        size_type erase(const key_type &__k) {
            return _M_t.erase_unique(__k);
        }

        /**
         *  @brief Erases an element at a given position.
         *  @param  __position  Iterator pointing to the element to be erased.
         *  @return An iterator to the successor of the erased element.
         *
         * Note that this function only erases the element, and that if
         * the element is itself a pointer, the pointed-to memory is not touched
         * in any way.
         */
        iterator erase(iterator __position) {
            return _M_t.erase(__position);
        }

        /**
         * @brief Erases a range [first, last) of elements from a map.
         * @param __first  Iterator pointing to the start of the range.
         * @param __last  Iteraot pointing to the end of the range.
         * @return The iterator __last.
         *
         * Note that this function only erases the element, and that if
         * the element is itself a pointer, the pointed-to memory is not touched
         * in any way.
         */
        iterator erase(iterator __first, iterator __last) {
            while (__first != __last)
                _M_t.erase(__first++);

            return __last;
        }

        /**
         *  @brief  Swaps data with another map.
         *  @param  __x  A map of the same element and allocator types.
         *
         *  This exchanges the elements between two maps in constant time.
         */
        void swap(radix_map &__x) {
            _M_t.swap(__x._M_t);
        }


        // Element access

        /**
         * @brief  Subscript ( @c [] ) access to map data.
         * @param  __k  The key for which data should be retrieved.
         * @return  A reference to the data of the (key,data) %pair.
         *
         * Allows for easy lookup with the subscript ( @c [] )
         * operator.  Returns data associated with the key specified in
         * subscript.  If the key does not exist, a pair with that key
         * is created using default values, which is then returned.
         *
         * Lookup requires O(k) time.
         *
         */
        mapped_type &operator[](const key_type &__k) {
            // @TODO more efficient to write extra method to avoid 2 lookups
            iterator it = _M_t.find(__k);
            if (it != _M_t.end())
                return it->second;

            std::pair<iterator, bool> res = _M_t.emplace_unique(__k, mapped_type());
            return res.first->second;
        }

        /**
         *  @brief  Access to map data.
         *  @param  __k  The key for which data should be retrieved.
         *  @return  A reference to the data whose key is equivalent to @a __k, if
         *           such a data is present in the map.
         *  @throw  std::out_of_range  If no such data is present.
         */
        mapped_type &at(const key_type &__k) {
            iterator res = _M_t.find(__k);
            if (res == _M_t.end())
                std::__throw_out_of_range("radix_map::at");
            return (*res).second;
        }

        // Lookup

        /**
         *  @brief  Finds the number of elements.
         *  @param  __x  Key to located.
         *  @return  Number of elements with specified key.
         */
        size_type count(const key_type &__x) const {
            return _M_t.find(__x) == _M_t.end() ? 0 : 1;
        }

        /**
         *  @brief Tries to locate an element in a map.
         *  @param  __k  Key of (key, value) %pair to be located.
         *  @return  Iterator pointing to sought-after element, or end() if not
         *           found.
         *
         *  This function takes a key and tries to locate the element with which
         *  the key matches.  If successful the function returns an iterator
         *  pointing to the sought after %pair.  If unsuccessful it returns the
         *  past-the-end ( @c end() ) iterator.
         */
        iterator find(const key_type &__k) {
            return _M_t.find(__k);
        }

        const_iterator find(const key_type &__k) const {
            return _M_t.find(__k);
        }

        /**
         *  @brief Finds the beginning of a subsequence matching given key.
         *  @param  __k  Key of (key, value) pair to be located.
         *  @return  Iterator pointing to first element equal to or greater
         *           than key, or end().
         *
         *  This function returns the first element of a subsequence of elements
         *  that matches the given key.  If unsuccessful it returns an iterator
         *  pointing to the first element that has a greater value than given key
         *  or end() if no such element exists.
         */
        iterator lower_bound(const key_type &__k) {
            return _M_t.lower_bound(__k);
        }

        const_iterator lower_bound(const key_type &__k) const {
            return _M_t.lower_bound(__k);
        }

        /**
         *  @brief Finds the end of a subsequence matching given key.
         *  @param  __k  Key of (key, value) pair to be located.
         *  @return Iterator pointing to the first element
         *          greater than key, or end().
         */
        iterator upper_bound(const key_type &__k) {
            return _M_t.upper_bound(__k);
        }

        const_iterator upper_bound(const key_type &__k) const {
            return _M_t.upper_bound(__k);
        }

        // These functions are useless....
        std::pair<iterator, iterator> equal_range(const key_type &__k) {
            return _M_t.equal_range(__k);
        }

        std::pair<const_iterator, const_iterator> equal_range(const key_type &__k) const {
            return _M_t.equal_range(__k);
        }

        // Iterators

        /**
         * Returns an iterator that points to the first element in the map.
         * Iteration is done in ascending order acoording to the comparison
         * of the transformed keys' binary representations.
         */
        iterator begin() noexcept {
            return _M_t.begin();
        }

        /**
         * Returns a read-only iterator that points to the first element in the map.
         * Iteration is done in ascending order acoording to the comparison
         * of the transformed keys' binary representations.
         */
        const_iterator begin() const noexcept {
            return _M_t.begin();
        }

        /**
         * Returns a read-only iterator that points to the first element in the map.
         * Iteration is done in ascending order acoording to the comparison
         * of the transformed keys' binary representations.
         */
        const_iterator cbegin() const noexcept {
            return _M_t.begin();
        }

        /**
         * Returns a iterator that points to the last element in the map.
         * Iteration is done in ascending order acoording to the comparison
         * of the transformed keys' binary representations.
         */
        iterator end() noexcept {
            return _M_t.end();
        }

        /**
         * Returns a read-only iterator that points to the last element in the map.
         * Iteration is done in ascending order acoording to the comparison
         * of the transformed keys' binary representations.
         */
        const_iterator end() const noexcept {
            return _M_t.end();
        }

        /**
         * Returns a read-only iterator that points to the last element in the map.
         * Iteration is done in ascending order acoording to the comparison
         * of the transformed keys' binary representations.
         */
        const_iterator cend() const noexcept {
            return _M_t.end();
        }

        /**
         * Returns a reverse iterator that points to the first element in the map.
         * Iteration is done in descending order acoording to the comparison
         * of the transformed keys' binary representations.
         */
        reverse_iterator rbegin() noexcept {
            return _M_t.rbegin();
        }

        /**
         * Returns a read-only reverse iterator that points to the first element
         * in the map. Iteration is done in descending order acoording to the
         * comparison of the transformed keys' binary representations.
         */
        const_reverse_iterator rbegin() const noexcept {
            return _M_t.rbegin();
        }

        /**
         * Returns a read-only reverse iterator that points to the first element
         * in the map. Iteration is done in descending order acoording to the
         * comparison of the transformed keys' binary representations.
         */
        const_reverse_iterator crbegin() const noexcept {
            return _M_t.rbegin();
        }

        /**
         * Returns a reverse iterator that points to the last element
         * in the map. Iteration is done in descending order acoording to the
         * comparison of the transformed keys' binary representations.
         */
        reverse_iterator rend() noexcept {
            return _M_t.rend();
        }

        /**
         * Returns a read-only reverse iterator that points to the last element
         * in the map. Iteration is done in descending order acoording to the
         * comparison of the transformed keys' binary representations.
         */
        const_reverse_iterator rend() const noexcept {
            return _M_t.rend();
        }

        /**
         * Returns a read-only reverse iterator that points to the last element
         * in the map. Iteration is done in descending order acoording to the
         * comparison of the transformed keys' binary representations.
         */
        const_reverse_iterator crend() const noexcept {
            return _M_t.rend();
        }

        // Observers

        _Key_transform key_trans() const {
            return _M_t.key_trans();
        }

        value_compare value_comp() const {
            return value_compare(_M_t.key_trans());
        }

        template<typename _K1, typename _T1, typename _C1>
        friend bool operator==(const radix_map<_K1, _T1, _C1> &,
                               const radix_map<_K1, _T1, _C1> &);

        template<typename _K1, typename _T1, typename _C1>
        friend bool operator<(const radix_map<_K1, _T1, _C1> &,
                              const radix_map<_K1, _T1, _C1> &);
    };

    // Relational Operators

    /**
     *  @brief  Map equality comparison.
     *  @param  __x  A map.
     *  @param  __y  A map of the same type as @a x.
     *  @return  True iff the size and elements of the maps are equal.
     *
     *  This is an equivalence relation.  It is linear in the size of the
     *  maps.  Maps are considered equivalent if their sizes are equal,
     *  and if corresponding elements compare equal.
    */
    template<typename _Key, typename _Tp, typename _Key_transform>
    inline bool
    operator==(const radix_map<_Key, _Tp, _Key_transform> &__x,
               const radix_map<_Key, _Tp, _Key_transform> &__y) {
        return __x._M_t == __y._M_t;
    }

    /**
     *  @brief  Map ordering relation.
     *  @param  __x  A map.
     *  @param  __y  A map of the same type as @a x.
     *  @return  True iff @a x is lexicographically less than @a y.
     *
     *  This is a total ordering relation.  It is linear in the size of the
     *  maps.  The elements must be comparable with @c <.
     *
     *  See std::lexicographical_compare() for how the determination is made.
    */
    template<typename _Key, typename _Tp, typename _Key_transform>
    inline bool
    operator<(const radix_map<_Key, _Tp, _Key_transform> &__x,
              const radix_map<_Key, _Tp, _Key_transform> &__y) {
        return __x._M_t < __y._M_t;
    }

    // Based on operator==
    template<typename _Key, typename _Tp, typename _Key_transform>
    inline bool
    operator!=(const radix_map<_Key, _Tp, _Key_transform> &__x,
               const radix_map<_Key, _Tp, _Key_transform> &__y) {
        return !(__x == __y);
    }

    // Based on operator<
    template<typename _Key, typename _Tp, typename _Key_transform>
    inline bool
    operator>(const radix_map<_Key, _Tp, _Key_transform> &__x,
              const radix_map<_Key, _Tp, _Key_transform> &__y) {
        return __y < __x;
    }

    // Based on operator<
    template<typename _Key, typename _Tp, typename _Key_transform>
    inline bool
    operator<=(const radix_map<_Key, _Tp, _Key_transform> &__x,
               const radix_map<_Key, _Tp, _Key_transform> &__y) {
        return !(__y < __x);
    }

    // Based on operator<
    template<typename _Key, typename _Tp, typename _Key_transform>
    inline bool
    operator>=(const radix_map<_Key, _Tp, _Key_transform> &__x,
               const radix_map<_Key, _Tp, _Key_transform> &__y) {
        return !(__x < __y);
    }

    // See radix_map::swap()
    template<typename _Key, typename _Tp, typename _Key_transform>
    inline void
    swap(radix_map<_Key, _Tp, _Key_transform> &__x,
         radix_map<_Key, _Tp, _Key_transform> &__y) {
        __x.swap(__y);
    }
}

#endif //REFERENCE_ART_MAP_H
