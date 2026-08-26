#ifndef ART_RADIX_SET_H
#define ART_RADIX_SET_H

#include <type_traits>
#include "art/ar_prefix_tree.h"
#include "art/ar_tree.h"

namespace art {
    namespace detail {
        template<typename T>
        struct Identity : public unary_function<T, T> {

            T &operator()(T &__x) const {
                return __x;
            }

            const T &operator()(const T &__x) const {
                return __x;
            }
        };
    }

    /**
     * @brief A container made up of unique keys , which can be
     * retrieved in linear time in size of the key.
     *
     *  @tparam _Key  Type of key objects.
     *  @tparam _Key_transform  Key transformation function object type,
     *                          defaults to key_transform<_Key>.
     *
     * Meets the requirements of a <a href="tables.html#65">container</a>, a
     *  <a href="tables.html#66">reversible container</a>, and an
     *  <a href="tables.html#69">associative container</a> (using unique keys).
     *
     *  Sets support bidirectional iterators.
     */
    template<typename _Key,
            typename _Key_transform = key_transform<_Key> >
    class radix_set {
    public:
        typedef _Key key_type;
        typedef _Key value_type;
        typedef _Key_transform key_transformer;
        typedef value_type &reference;
        typedef const value_type &const_reference;

    private:
        /**
         * Switch between art implementation with and without path compression
         * (prefixes in nodes) based on (transformed) key length. For short keys,
         * the cost of handling prefixes outweigh the benefits.
         */
        typedef typename std::conditional<sizeof(decltype(_Key_transform()(_Key()))) <= 6,
                ar_tree<key_type, value_type,
                        detail::Identity<value_type>, _Key_transform>,
                ar_prefix_tree<key_type, value_type,
                        detail::Identity<value_type>, _Key_transform>>::type _Rep_type;

        _Rep_type _M_t;

    public:
        // Bidirectional iterator
        typedef typename _Rep_type::iterator iterator;
        typedef typename _Rep_type::const_iterator const_iterator;
        typedef typename _Rep_type::size_type size_type;
        typedef typename _Rep_type::difference_type difference_type;
        typedef typename _Rep_type::reverse_iterator reverse_iterator;
        typedef typename _Rep_type::const_reverse_iterator const_reverse_iterator;

        /**
         * @brief  Default constructor creates no elements.
         */
        radix_set() : _M_t() {}

        /**
         * @brief  Set copy constructor.
         */
        radix_set(const radix_set &__x) : _M_t(__x._M_t) {}

        /**
         * @brief  Set move constructor.
         */
        radix_set(radix_set &&__x) : _M_t(std::move(__x._M_t)) {}

        /**
         *  @brief  Builds a set from a range.
         *  @param  __first  An input iterator.
         *  @param  __last  An input iterator.
         *
         */
        template<typename _InputIterator>
        radix_set(_InputIterator __first, _InputIterator __last)
                : _M_t() {
            _M_t.insert_unique(__first, __last);
        }

        /**
         *  @brief  Builds a set from an initializer_list.
         *  @param  __l  An initializer_list.
         *  @param  __key_transformer  A key transformation functor.
         *
         */
        radix_set(std::initializer_list<value_type> __l,
                  const _Key_transform &__key_transformer = _Key_transform())
                : _M_t(__key_transformer) {
            _M_t.insert_unique(__l.begin(), __l.end());
        }

        /**
         *  @brief  Set assignment operator.
         *  @param  __x  A %set with identical elements.
         *
         */
        radix_set &operator=(const radix_set &__x) {
            _M_t = __x._M_t;
            return *this;
        }

        /**
         * @brief  Move assignment operator.
         */
        radix_set &operator=(radix_set &&__x) = default;

        /**
         *  @brief  Set list assignment operator.
         *  @param  __l  An initializer_list.
         *
         *  Fills the set with the contents of an initializer_list.
         *  Already existing elements in a set will be lost.
         */
        radix_set &operator=(std::initializer_list<value_type> __l) {
            _M_t.assign_unique(__l.begin(), __l.end());
            return *this;
        }

        // Capacity

        /**
         * Returns true if the set is empty.
        */
        bool empty() const noexcept {
            return _M_t.empty();
        }

        /**
         * Returns the size of the set.
         */
        size_type size() const noexcept {
            return _M_t.size();
        }

        /**
         * Returns the maximum size of the set.
         */
        size_type max_size() const noexcept {
            return _M_t.max_size();
        }

        // Modifiers

        /**
         *  Erases all elements in a set.
         */
        void clear() { _M_t.clear(); }

        /**
         *  @brief Attempts to insert an element into the set.

         *  @param __x Element to be inserted.
         *
         *  @return  A pair, of which the first element is an iterator that
         *           points to the possibly inserted pair, and the second is
         *           a bool that is true if the pair was actually inserted.
         *
         *  This function attempts to insert an element into the set. A set
         *  relies on unique keys and thus an element is only inserted if
         *  it is not already present in the set.
         *
         *  Insertion requires linear time in size of the key.
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
         *  @brief Attempts to insert a list of elements into the set.
         *  @param  __list  A std::initializer_list<value_type> of
         *                  elements to be inserted.
         */
        void insert(std::initializer_list<value_type> __list) {
            insert(__list.begin(), __list.end());
        }

        /**
         *  @brief Attempts to build and insert an element into the set.
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
         * @brief Erases a range [first, last) of elements from a set.
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
         *  @brief  Swaps data with another set.
         *  @param  __x  A set of the same element and allocator types.
         *
         *  This exchanges the elements between two sets in constant time.
         */
        void swap(radix_set &__x) {
            _M_t.swap(__x._M_t);
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
         *  @brief Tries to locate an element in a set.
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
         * Returns an iterator that points to the first element in the set.
         * Iteration is done in ascending order acoording to the comparison
         * of the transformed keys' binary representations.
         */
        iterator begin() noexcept {
            return _M_t.begin();
        }

        /**
         * Returns a read-only iterator that points to the first element in the set.
         * Iteration is done in ascending order acoording to the comparison
         * of the transformed keys' binary representations.
         */
        const_iterator begin() const noexcept {
            return _M_t.begin();
        }

        /**
         * Returns a read-only iterator that points to the first element in the set.
         * Iteration is done in ascending order acoording to the comparison
         * of the transformed keys' binary representations.
         */
        const_iterator cbegin() const noexcept {
            return _M_t.begin();
        }

        /**
         * Returns a iterator that points to the last element in the set.
         * Iteration is done in ascending order acoording to the comparison
         * of the transformed keys' binary representations.
         */
        iterator end() noexcept {
            return _M_t.end();
        }

        /**
         * Returns a read-only iterator that points to the last element in the set.
         * Iteration is done in ascending order acoording to the comparison
         * of the transformed keys' binary representations.
         */
        const_iterator end() const noexcept {
            return _M_t.end();
        }

        /**
         * Returns a read-only iterator that points to the last element in the set.
         * Iteration is done in ascending order acoording to the comparison
         * of the transformed keys' binary representations.
         */
        const_iterator cend() const noexcept {
            return _M_t.end();
        }

        /**
         * Returns a reverse iterator that points to the first element in the set.
         * Iteration is done in descending order acoording to the comparison
         * of the transformed keys' binary representations.
         */
        reverse_iterator rbegin() noexcept {
            return _M_t.rbegin();
        }

        /**
         * Returns a read-only reverse iterator that points to the first element
         * in the set. Iteration is done in descending order acoording to the
         * comparison of the transformed keys' binary representations.
         */
        const_reverse_iterator rbegin() const noexcept {
            return _M_t.rbegin();
        }

        /**
         * Returns a read-only reverse iterator that points to the first element
         * in the set. Iteration is done in descending order acoording to the
         * comparison of the transformed keys' binary representations.
         */
        const_reverse_iterator crbegin() const noexcept {
            return _M_t.rbegin();
        }

        /**
         * Returns a reverse iterator that points to the last element
         * in the set. Iteration is done in descending order acoording to the
         * comparison of the transformed keys' binary representations.
         */
        reverse_iterator rend() noexcept {
            return _M_t.rend();
        }

        /**
         * Returns a read-only reverse iterator that points to the last element
         * in the set. Iteration is done in descending order acoording to the
         * comparison of the transformed keys' binary representations.
         */
        const_reverse_iterator rend() const noexcept {
            return _M_t.rend();
        }

        /**
         * Returns a read-only reverse iterator that points to the last element
         * in the set. Iteration is done in descending order acoording to the
         * comparison of the transformed keys' binary representations.
         */
        const_reverse_iterator crend() const noexcept {
            return _M_t.rend();
        }

        // Observers

        key_transformer key_trans() const {
            return _M_t.key_trans();
        }

        key_transformer value_comp() const {
            return _M_t.key_trans();
        }

        template<typename _K1, typename _T1>
        friend bool operator==(const radix_set<_K1, _T1> &,
                               const radix_set<_K1, _T1> &);

        template<typename _K1, typename _T1>
        friend bool operator<(const radix_set<_K1, _T1> &,
                              const radix_set<_K1, _T1> &);
    };

    // Relational Operators

    /**
     *  @brief  Set equality comparison.
     *  @param  __x  A set.
     *  @param  __y  A set of the same type as @a x.
     *  @return  True iff the size and elements of the sets are equal.
     *
     *  This is an equivalence relation.  It is linear in the size of the
     *  sets.  Sets are considered equivalent if their sizes are equal,
     *  and if corresponding elements compare equal.
    */
    template<typename _Key, typename _Key_transform>
    inline bool
    operator==(const radix_set<_Key, _Key_transform> &__x,
               const radix_set<_Key, _Key_transform> &__y) {
        return __x._M_t == __y._M_t;
    }

    /**
     *  @brief  Set ordering relation.
     *  @param  __x  A set.
     *  @param  __y  A set of the same type as @a x.
     *  @return  True iff @a __x is lexicographically less than @a __y.
     *
     *  This is a total ordering relation.  It is linear in the size of the
     *  sets.
     *
     *  See std::lexicographical_compare() for how the determination is made.
    */
    template<typename _Key, typename _Key_transform>
    inline bool
    operator<(const radix_set<_Key, _Key_transform> &__x,
              const radix_set<_Key, _Key_transform> &__y) {
        return __x._M_t < __y._M_t;
    }

    // Based on operator==
    template<typename _Key, typename _Key_transform>
    inline bool
    operator!=(const radix_set<_Key, _Key_transform> &__x,
               const radix_set<_Key, _Key_transform> &__y) {
        return !(__x == __y);
    }

    // Based on operator<
    template<typename _Key, typename _Key_transform>
    inline bool
    operator>(const radix_set<_Key, _Key_transform> &__x,
              const radix_set<_Key, _Key_transform> &__y) {
        return __y < __x;
    }

    // Based on operator<
    template<typename _Key, typename _Key_transform>
    inline bool
    operator<=(const radix_set<_Key, _Key_transform> &__x,
               const radix_set<_Key, _Key_transform> &__y) {
        return !(__y < __x);
    }

    // Based on operator<
    template<typename _Key, typename _Key_transform>
    inline bool
    operator>=(const radix_set<_Key, _Key_transform> &__x,
               const radix_set<_Key, _Key_transform> &__y) {
        return !(__x < __y);
    }

    // See radix_set::swap()
    template<typename _Key, typename _Key_transform>
    inline void
    swap(radix_set<_Key, _Key_transform> &__x,
         radix_set<_Key, _Key_transform> &__y) {
        __x.swap(__y);
    }
}

#endif //ART_RADIX_SET_H
