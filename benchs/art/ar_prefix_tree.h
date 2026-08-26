#ifndef ART_AR_PREFIX_TREE_H
#define ART_AR_PREFIX_TREE_H

#include <array>
#include <cstring>
#include <stddef.h>
#include <iterator>
#include <utility>
#include <limits>
#include "key_transform.h"

#ifdef ART_DEBUG

#include <iostream>

#endif

namespace art {
    using std::pair;
    using std::make_pair;

    typedef uint8_t byte;

    static const size_t MAX_PREFIX_LENGTH = 8;

    template<typename _Key, typename _Value, typename _KeyOfValue,
            typename _Key_transform = key_transform<_Key> >
    struct ar_prefix_tree {
    public:
        // Forward declaration for typedefs
        struct _Node;
        struct _Inner_Node;
        struct _Base_Leaf;
        struct _Leaf;
        struct _Node_4;
        struct _Node_16;
        struct _Node_48;
        struct _Node_256;

        typedef _Key key_type;
        typedef _Value value_type;

    protected:
        typedef _Node *Node_ptr;
        typedef const _Node *Const_Node_ptr;
        typedef _Inner_Node *Inner_Node_ptr;
        typedef const _Inner_Node *Const_Inner_Node_ptr;
        typedef _Base_Leaf *Base_Leaf_ptr;
        typedef const _Base_Leaf *Const_Base_Leaf_ptr;
        typedef _Leaf *Leaf_ptr;
        typedef const _Leaf *Const_Leaf_ptr;

        static const byte EMPTY_MARKER;

        enum node_type : uint8_t {
            _leaf_t = 0, node_4_t = 1, node_16_t = 2,
            node_48_t = 3, node_256_t = 4, _dummy_node_t = 5
        };

        _Key_transform _M_key_transform;

        typedef decltype(_M_key_transform(key_type())) transformed_key_type;

    public:
        union Key {
            const transformed_key_type value;
            const byte chunks[sizeof(transformed_key_type)];
        };

        struct _Node {
        public:
            Node_ptr _parent;

            _Node(Node_ptr parent) : _parent(parent) {}

            // Copy constructor
            _Node(const _Node &__x) : _parent(__x._parent) {
            }

            // Copy assignment
            _Node &operator=(const _Node &__x) {
                _parent = __x._parent;
                return *this;
            }

            virtual ~_Node() {}

            virtual void clear() = 0;

            virtual void insert(const byte key_byte, Node_ptr node) = 0;

            virtual void erase(const byte key_byte) = 0;

            virtual Node_ptr find(const byte key_byte) = 0;

            virtual Const_Node_ptr find(const byte key_byte) const = 0;

            virtual void update_child_ptr(const byte key_byte, Node_ptr node) = 0;

            virtual Base_Leaf_ptr minimum() = 0;

            virtual Const_Base_Leaf_ptr minimum() const = 0;

            virtual Base_Leaf_ptr maximum() = 0;

            virtual Const_Base_Leaf_ptr maximum() const = 0;

            virtual bool is_leaf() const { return false; }

            virtual uint16_t size() const { return 1; }

            virtual uint16_t min_size() const = 0;

            virtual uint16_t max_size() const = 0;

            virtual node_type get_type() const = 0;

            virtual Base_Leaf_ptr successor(const Key &key) = 0;

            virtual Const_Base_Leaf_ptr successor(const Key &key) const = 0;

            virtual Base_Leaf_ptr predecessor(const Key &key) = 0;

            virtual Const_Base_Leaf_ptr predecessor(const Key &key) const = 0;

#ifdef ART_DEBUG

            virtual void debug() const = 0;

#endif
        };

        struct _Inner_Node : public _Node {
        public:
            uint16_t _count;

            uint16_t _prefix_length;

            int32_t _depth;

            std::array<byte, MAX_PREFIX_LENGTH> _prefix{};

            _Inner_Node(Node_ptr parent, uint16_t count, int32_t depth)
                    : _Node(parent), _count(count), _depth(depth), _prefix_length(0) {}

            _Inner_Node(Node_ptr parent, uint16_t count, int32_t depth, uint16_t prefix_length,
                        const std::array<byte, MAX_PREFIX_LENGTH> &prefix)
                    : _Node(parent), _count(count), _depth(depth),
                      _prefix_length(prefix_length), _prefix(prefix) {}


            // Copy constructor
            _Inner_Node(const _Inner_Node &__x)
                    : _Node(__x._parent), _count(__x._count), _depth(__x._depth),
                      _prefix_length(__x._prefix_length), _prefix(__x._prefix) {}

            // Copy assignment
            _Inner_Node &operator=(const _Inner_Node &__x) {
                this->_parent = __x._parent;
                _count = __x._count;
                _depth = __x._depth;
                _prefix_length = __x._prefix_length;
                _prefix = __x._prefix;
                return *this;
            }

            /**
             * @brief Determines the first mismatch position of the key with the prefix.
             * @param key  Key to be matched with the prefix.
             * @return Position of first mismatch. If sequences are identical, the prefix length
             *         is returned.
             *
             * Compares the key with the prefix up to the prefix length. If the prefix length is
             * longer than what can be stored in the node directly (pessimistic path compression),
             * the rest of the prefix is automatically loaded from a descendent leaf.
             */
            size_t prefix_mismatch_pos(const Key &key) const {
                // pessimistic path compression
                size_t pos = 0;
                const auto pessimistic_length = std::min((size_t) _prefix_length, MAX_PREFIX_LENGTH);
                for (; pos < pessimistic_length; pos++)
                    if (key.chunks[_depth + pos] != _prefix[pos])
                        return pos;

                // optimistic path compression
                if (_prefix_length > MAX_PREFIX_LENGTH) {
                    Key min_key = {_Key_transform()(_KeyOfValue()(static_cast<Const_Leaf_ptr>(this->minimum())->_value))};
                    for (; pos < _prefix_length; pos++)
                        if (key.chunks[_depth + pos] != min_key.chunks[_depth + pos])
                            return pos;
                }

                return _prefix_length;
            }

            /**
             * @brief Determines the first mismatch position of the key with the prefix.
             * @param key  Key to be matched with the prefix.
             * @return Position of first mismatch. If sequences are identical, the prefix length
             *         is returned.
             *
             * Only compares the prefix directly stored in the node (pessimistic path compression).
             * Prefer this method when doing read-only operations as performance is significantly
             * improved and complete equality is checked in the leaf anyway.
             */
            size_t pes_prefix_mismatch_pos(const Key &key) const {
                // pessimistic path compression
                size_t pos = 0;
                const auto pessimistic_length = std::min((size_t) _prefix_length, MAX_PREFIX_LENGTH);
                for (; pos < pessimistic_length; pos++)
                    if (key.chunks[_depth + pos] != _prefix[pos])
                        return pos;

                return _prefix_length;
            }

#ifdef ART_DEBUG

            void debug_prefix() const {
                std::cout << "Prefix " << this->_prefix_length << ": ";
                for (uint16_t i = 0; i < std::min((size_t) this->_prefix_length, MAX_PREFIX_LENGTH); i++) {
                    std::cout << ((unsigned) this->_prefix[i]) << " | ";
                }
                std::cout << std::endl;
                std::cout << "Opt prefix: ";
                Key min_key = {_Key_transform()(_KeyOfValue()(static_cast<Const_Leaf_ptr>(this->minimum())->_value))};
                for (uint16_t i = MAX_PREFIX_LENGTH; i < this->_prefix_length; i++) {
                    std::cout << ((unsigned) min_key.chunks[i]) << " | ";
                }
                std::cout << std::endl;
            }

#endif

            virtual bool is_leaf() const override { return false; }

            virtual uint16_t size() const override { return _count; }
        };

        struct _Base_Leaf : public _Node {
        public:
            _Base_Leaf(Node_ptr parent) : _Node(parent) {}

            void clear() override {}

            virtual void insert(const byte key_byte, Node_ptr node) override {}

            virtual void erase(const byte key_byte) override {}

            virtual Node_ptr find(const byte key_byte) override { return nullptr; }

            virtual Const_Node_ptr find(const byte key_byte) const override { return nullptr; }

            virtual void update_child_ptr(const byte key_byte, Node_ptr node) override {}

            virtual Base_Leaf_ptr minimum() override { return this; }

            virtual Const_Base_Leaf_ptr minimum() const override { return this; }

            virtual Base_Leaf_ptr maximum() override { return this; }

            virtual Const_Base_Leaf_ptr maximum() const override { return this; }

            virtual Base_Leaf_ptr successor(const Key &key) override { return this; }

            virtual Const_Base_Leaf_ptr successor(const Key &key) const override { return this; }

            virtual Base_Leaf_ptr predecessor(const Key &key) override { return this; }

            virtual Const_Base_Leaf_ptr predecessor(const Key &key) const override { return this; }

            virtual uint16_t min_size() const override { return 1; }

            virtual uint16_t max_size() const override { return 1; }

            bool is_leaf() const override { return true; }

            virtual node_type get_type() const override { return node_type::_leaf_t; }

#ifdef ART_DEBUG

            virtual void debug() const override {
                std::cout << this << " Base Leaf: " << ", parent " << this->_parent << std::endl;
            }

#endif
        };

        struct _Leaf : public _Base_Leaf {
        public:
            value_type _value;

            _Leaf(value_type value)
                    : _Base_Leaf(nullptr), _value(value) {}


            _Leaf(value_type value, Node_ptr parent)
                    : _Base_Leaf(parent), _value(value) {}

            // Copy constructor
            _Leaf(const _Leaf &__x) : _Base_Leaf(nullptr), _value(__x._value) {}

            // Copy assignment
            _Leaf &operator=(const _Leaf &__x) {
                this->_parent = nullptr;
                _value = __x._value;
                return *this;
            }

            void clear() override {}

            virtual void insert(const byte key_byte, Node_ptr node) override {}

            virtual void erase(const byte key_byte) override {}

            virtual Node_ptr find(const byte key_byte) override {
                return nullptr;
            }

            virtual Const_Node_ptr find(const byte key_byte) const override {
                return nullptr;
            }

            virtual void update_child_ptr(const byte key_byte, Node_ptr node) override {}

            virtual Leaf_ptr minimum() override { return this; }

            virtual Const_Base_Leaf_ptr minimum() const override { return this; }

            virtual Base_Leaf_ptr maximum() override { return this; }

            virtual Const_Base_Leaf_ptr maximum() const override { return this; }

            virtual Base_Leaf_ptr successor(const Key &key) override { return this; }

            virtual Const_Base_Leaf_ptr successor(const Key &key) const override { return this; }

            virtual Base_Leaf_ptr predecessor(const Key &key) override { return this; }

            virtual Const_Base_Leaf_ptr predecessor(const Key &key) const override { return this; }

            virtual uint16_t min_size() const override { return 1; }

            virtual uint16_t max_size() const override { return 1; }

            bool is_leaf() const override { return true; }

            virtual node_type get_type() const override { return node_type::_leaf_t; }

#ifdef ART_DEBUG

            virtual void debug() const override {
                std::cout << this << " Leaf: "
                          << ", parent " << this->_parent << std::endl;
            }

#endif
        };

        /**
         * End of iteration marker right of the container.
         */
        struct _Dummy_Node : public _Inner_Node {
        public:
            Node_ptr _root;

            Base_Leaf_ptr _leaf;

            _Dummy_Node()
                    : _Inner_Node(this, 1, -1), _root(nullptr),
                      _leaf(new _Base_Leaf(this)) {
            }

            _Dummy_Node(Node_ptr root, Leaf_ptr leaf)
                    : _Inner_Node(this, 1, -1), _root(root),
                      _leaf(new _Base_Leaf(this)) {
            }

            // Copy constructor
            _Dummy_Node(const _Dummy_Node &__x)
                    : _Inner_Node(__x._parent, 1, -1) {
                _leaf = __x._leaf;
            }

            // Copy assignment
            _Dummy_Node &operator=(const _Dummy_Node &__x) {
                this->_parent = __x._parent;
                _leaf = __x._leaf;
                return *this;
            }

            void clear() override {}

            virtual void insert(const byte key_byte, Node_ptr node) override {}

            virtual void erase(const byte key_byte) override {}

            virtual Node_ptr find(const byte key_byte) override {
                return nullptr;
            }

            virtual Const_Node_ptr find(const byte key_byte) const override {
                return nullptr;
            }

            virtual void update_child_ptr(const byte key_byte, Node_ptr node) override {
                _root = node;
            }

            virtual Base_Leaf_ptr minimum() override { return this->_leaf; }

            virtual Const_Base_Leaf_ptr minimum() const override { return this->_leaf; }

            virtual Base_Leaf_ptr maximum() override { return this->_leaf; }

            virtual Const_Base_Leaf_ptr maximum() const override { return this->_leaf; }

            virtual Base_Leaf_ptr successor(const Key &key) override {
                return this->_leaf;
            }

            virtual Const_Base_Leaf_ptr successor(const Key &key) const override {
                return this->_leaf;
            }

            virtual Base_Leaf_ptr predecessor(const Key &key) override {
                if (_root != nullptr)
                    return _root->maximum();

                return this->_leaf;
            }

            virtual Const_Base_Leaf_ptr predecessor(const Key &key) const override {
                if (_root != nullptr)
                    return _root->maximum();

                return this->_leaf;
            }

            virtual uint16_t min_size() const override { return 0; }

            virtual uint16_t max_size() const override { return 0; }

            virtual node_type get_type() const override { return node_type::_dummy_node_t; }

#ifdef ART_DEBUG

            virtual void debug() const override {
                std::cout << "Dummy Node debug" << std::endl;
            }

#endif

            ~_Dummy_Node() {
                delete _leaf;
            }
        };

        struct _Node_4 : public _Inner_Node {
        public:
            std::array<byte, 4> keys{};
            std::array<Node_ptr, 4> children{};

            // Grow constructor
            _Node_4(Leaf_ptr leaf, const byte key_byte, int32_t depth)
                    : _Inner_Node(leaf->_parent, 1, depth) {
                keys[0] = key_byte;
                children[0] = leaf;
                leaf->_parent = this;
            }

            _Node_4(Node_ptr child, const byte key_byte, int32_t depth)
                    : _Inner_Node(child->_parent, 1, depth) {
                keys[0] = key_byte;
                children[0] = child;
                child->_parent = this;
            }

            _Node_4(Node_ptr child, const byte key_byte, int32_t depth,
                    uint16_t prefix_length, std::array<byte, MAX_PREFIX_LENGTH> &prefix)
                    : _Inner_Node(child->_parent, 1, depth, prefix_length, prefix) {
                keys[0] = key_byte;
                children[0] = child;
                child->_parent = this;
            }

            // Shrink constructor
            _Node_4(_Node_16 *node)
                    : _Inner_Node(node->_parent, 4, node->_depth, node->_prefix_length, node->_prefix) {
                std::copy(node->keys.begin(), node->keys.begin() + 4, keys.begin());
                std::copy(node->children.begin(), node->children.begin() + 4, children.begin());

                for (size_t i = 0; i < 4; i++)
                    children[i]->_parent = this;
            }

            // Copy constructor
            _Node_4(const _Node_4 &__x)
                    : _Inner_Node(__x._parent, __x._count, __x._depth, __x._prefix_length, __x._prefix),
                      keys(__x.keys) {
                copy_children(__x);
            }

            // Copy assignment
            _Node_4 &operator=(const _Node_4 &__x) {
                this->_parent = __x._parent;
                this->_count = __x._count;
                this->_depth = __x._depth;
                this->_prefix_length = __x._prefix_length;
                this->_prefix = __x._prefix;
                keys = __x.keys;
                copy_children(__x);
                return *this;
            }

            void copy_children(const _Node_4 &__x) {
                for (int i = 0; i < __x._count; i++) {
                    switch (__x.children[i]->get_type()) {
                        case node_type::node_4_t:
                            children[i] = new _Node_4(*static_cast<_Node_4 *>(__x.children[i]));
                            children[i]->_parent = this;
                            break;
                        case node_type::node_16_t:
                            children[i] = new _Node_16(*static_cast<_Node_16 *>(__x.children[i]));
                            children[i]->_parent = this;
                            break;
                        case node_type::node_48_t:
                            children[i] = new _Node_48(*static_cast<_Node_48 *>(__x.children[i]));
                            children[i]->_parent = this;
                            break;
                        case node_type::node_256_t:
                            children[i] = new _Node_256(*static_cast<_Node_256 *>(__x.children[i]));
                            children[i]->_parent = this;
                            break;
                        case node_type::_leaf_t:
                            children[i] = new _Leaf(*static_cast<Leaf_ptr>(__x.children[i]));
                            children[i]->_parent = this;
                            break;
                        default:
                            throw;
                    }
                }
            }

            void clear() override {
                for (size_t i = 0; i < this->_count; i++) {
                    children[i]->clear();
                    delete children[i];
                }
            }

            virtual void insert(const byte key_byte, Node_ptr node) override {
                unsigned pos = 0;
                for (; pos < this->_count && keys[pos] < key_byte; pos++);
                if (pos < this->_count) {
                    std::move(keys.begin() + pos, keys.begin() + this->_count, keys.begin() + pos + 1);
                    std::move(children.begin() + pos, children.begin() + this->_count, children.begin() + pos + 1);
                }
                keys[pos] = key_byte;
                children[pos] = node;
                this->_count++;
            }

            virtual void erase(const byte key_byte) override {
                unsigned pos = 0;
                for (; pos < this->_count && keys[pos] < key_byte; pos++);
                if (pos < this->_count) {
                    delete children[pos];
                    std::move(keys.begin() + pos + 1, keys.begin() + this->_count, keys.begin() + pos);
                    std::move(children.begin() + pos + 1, children.begin() + this->_count, children.begin() + pos);
                    this->_count--;
                }
            }

            virtual Node_ptr find(const byte key_byte) override {
                unsigned pos = 0;
                for (; pos < this->_count && keys[pos] < key_byte; pos++);

                if (pos < this->_count && keys[pos] == key_byte)
                    return children[pos];
                return nullptr;
            }

            virtual Const_Node_ptr find(const byte key_byte) const override {
                unsigned pos = 0;
                for (; pos < this->_count && keys[pos] < key_byte; pos++);

                if (pos < this->_count && keys[pos] == key_byte)
                    return children[pos];
                return nullptr;
            }

            virtual void update_child_ptr(const byte key_byte, Node_ptr node) override {
                unsigned pos = 0;
                for (; pos < this->_count && keys[pos] < key_byte; pos++);

                if (pos < this->_count && keys[pos] == key_byte)
                    children[pos] = node;
            }

            virtual Base_Leaf_ptr minimum() override {
                return children[0]->minimum();
            }

            virtual Const_Base_Leaf_ptr minimum() const override {
                return children[0]->minimum();
            }

            virtual Base_Leaf_ptr maximum() override {
                return children[this->_count - 1]->maximum();
            }

            virtual Const_Base_Leaf_ptr maximum() const override {
                return children[this->_count - 1]->maximum();
            }

            virtual Base_Leaf_ptr successor(const Key &key) override {
                unsigned pos = 0;
                const auto depth = this->_depth + this->_prefix_length;
                for (; pos < this->_count && keys[pos] <= key.chunks[depth]; pos++);
                if (pos < this->_count)
                    return children[pos]->minimum();
                else
                    return this->_parent->successor(key);
            }

            virtual Const_Base_Leaf_ptr successor(const Key &key) const override {
                unsigned pos = 0;
                const auto depth = this->_depth + this->_prefix_length;
                for (; pos < this->_count && keys[pos] <= key.chunks[depth]; pos++);
                if (pos < this->_count)
                    return children[pos]->minimum();
                else
                    return this->_parent->successor(key);
            }

            virtual Base_Leaf_ptr predecessor(const Key &key) override {
                int pos = this->_count - 1;
                const auto depth = this->_depth + this->_prefix_length;
                for (; pos >= 0 && keys[pos] >= key.chunks[depth]; pos--);
                if (pos >= 0)
                    return children[pos]->maximum();
                else
                    return this->_parent->predecessor(key);
            }

            virtual Const_Base_Leaf_ptr predecessor(const Key &key) const override {
                int pos = this->_count - 1;
                const auto depth = this->_depth + this->_prefix_length;
                for (; pos >= 0 && keys[pos] >= key.chunks[depth]; pos--);
                if (pos >= 0)
                    return children[pos]->maximum();
                else
                    return this->_parent->predecessor(key);
            }

            virtual uint16_t min_size() const override { return 2; }

            virtual uint16_t max_size() const override { return 4; }

            virtual node_type get_type() const override { return node_type::node_4_t; }

#ifdef ART_DEBUG

            virtual void debug() const override {
                std::cout << this << " Node 4, depth: " << this->_depth
                          << " count: " << this->size()
                          << ", parent " << this->_parent << std::endl;
                this->debug_prefix();

                if (this->size() > 0) {
                    for (size_t i = 0; i < this->_count; i++) {
                        std::cout << ((unsigned) keys[i]) << " | ";
                    }
                    std::cout << std::endl;
                    for (size_t i = 0; i < this->_count; i++) {
                        std::cout << children[i] << " | ";
                    }
                    std::cout << std::endl;
                }
            }

#endif
        };

        struct _Node_16 : public _Inner_Node {
        public:
            std::array<byte, 16> keys{};
            std::array<Node_ptr, 16> children{};

            // Grow constructor
            _Node_16(_Node_4 *node)
                    : _Inner_Node(node->_parent, 4, node->_depth, node->_prefix_length, node->_prefix) {
                std::copy(node->keys.begin(), node->keys.end(), keys.begin());
                std::copy(node->children.begin(), node->children.end(), children.begin());

                for (size_t i = 0; i < 4; i++)
                    children[i]->_parent = this;
            }

            // Shrink constructor
            _Node_16(_Node_48 *node)
                    : _Inner_Node(node->_parent, 16, node->_depth, node->_prefix_length, node->_prefix) {
                uint8_t pos = 0;
                for (uint16_t i = 0; i < 256; i++) {
                    if (node->child_index[i] != EMPTY_MARKER) {
                        keys[pos] = i;
                        children[pos] = node->children[node->child_index[i]];
                        children[pos]->_parent = this;
                        pos++;
                    }
                }
            }

            // Copy constructor
            _Node_16(const _Node_16 &__x)
                    : _Inner_Node(__x._parent, __x._count, __x._depth, __x._prefix_length, __x._prefix),
                      keys(__x.keys) {
                copy_children(__x);
            }

            // Copy assignment
            _Node_16 &operator=(const _Node_16 &__x) {
                this->_parent = __x._parent;
                this->_count = __x._count;
                this->_depth = __x._depth;
                this->_prefix_length = __x._prefix_length;
                this->_prefix = __x._prefix;
                keys = __x.keys;
                copy_children(__x);
                return *this;
            }

            void copy_children(const _Node_16 &__x) {
                for (int i = 0; i < __x._count; i++) {
                    switch (__x.children[i]->get_type()) {
                        case node_type::node_4_t:
                            children[i] = new _Node_4(*static_cast<_Node_4 *>(__x.children[i]));
                            children[i]->_parent = this;
                            break;
                        case node_type::node_16_t:
                            children[i] = new _Node_16(*static_cast<_Node_16 *>(__x.children[i]));
                            children[i]->_parent = this;
                            break;
                        case node_type::node_48_t:
                            children[i] = new _Node_48(*static_cast<_Node_48 *>(__x.children[i]));
                            children[i]->_parent = this;
                            break;
                        case node_type::node_256_t:
                            children[i] = new _Node_256(*static_cast<_Node_256 *>(__x.children[i]));
                            children[i]->_parent = this;
                            break;
                        case node_type::_leaf_t:
                            children[i] = new _Leaf(*static_cast<Leaf_ptr>(__x.children[i]));
                            children[i]->_parent = this;
                            break;
                        default:
                            throw;
                            break;
                    }
                }
            }

            void clear() override {
                for (size_t i = 0; i < this->_count; i++) {
                    children[i]->clear();
                    delete children[i];
                }
            }

            virtual void insert(const byte key_byte, Node_ptr node) override {
                unsigned pos = 0;
                for (; pos < this->_count && keys[pos] < key_byte; pos++);
                if (pos != this->_count) {
                    std::move(keys.begin() + pos, keys.begin() + this->_count, keys.begin() + pos + 1);
                    std::move(children.begin() + pos, children.begin() + this->_count, children.begin() + pos + 1);
                }
                keys[pos] = key_byte;
                children[pos] = node;
                this->_count++;
            }

            virtual void erase(const byte key_byte) override {
                unsigned pos = 0;
                for (; pos < this->_count && keys[pos] < key_byte; pos++);
                if (pos < this->_count) {
                    delete children[pos];
                    std::move(keys.begin() + pos + 1, keys.begin() + this->_count, keys.begin() + pos);
                    std::move(children.begin() + pos + 1, children.begin() + this->_count, children.begin() + pos);
                    this->_count--;
                }
            }

            virtual Node_ptr find(const byte key_byte) override {
                unsigned pos = 0;
                for (; pos < this->_count && keys[pos] < key_byte; pos++);

                if (pos < this->_count && keys[pos] == key_byte)
                    return children[pos];
                return nullptr;
            }

            virtual Const_Node_ptr find(const byte key_byte) const override {
                unsigned pos = 0;
                for (; pos < this->_count && keys[pos] < key_byte; pos++);

                if (pos < this->_count && keys[pos] == key_byte)
                    return children[pos];
                return nullptr;
            }

            virtual void update_child_ptr(const byte key_byte, Node_ptr node) override {
                unsigned pos = 0;
                for (; pos < this->_count && keys[pos] < key_byte; pos++);

                if (pos < this->_count && keys[pos] == key_byte)
                    children[pos] = node;
            }

            virtual Base_Leaf_ptr minimum() override {
                return children[0]->minimum();
            }

            virtual Const_Base_Leaf_ptr minimum() const override {
                return children[0]->minimum();
            }

            virtual Base_Leaf_ptr maximum() override {
                return children[this->_count - 1]->maximum();
            }

            virtual Const_Base_Leaf_ptr maximum() const override {
                return children[this->_count - 1]->maximum();
            }

            virtual Base_Leaf_ptr successor(const Key &key) override {
                unsigned pos = 0;
                const auto depth = this->_depth + this->_prefix_length;
                for (; pos < this->_count && keys[pos] <= key.chunks[depth]; pos++);
                if (pos < this->_count)
                    return children[pos]->minimum();
                else
                    return this->_parent->successor(key);
            }

            virtual Const_Base_Leaf_ptr successor(const Key &key) const override {
                unsigned pos = 0;
                const auto depth = this->_depth + this->_prefix_length;
                for (; pos < this->_count && keys[pos] <= key.chunks[depth]; pos++);
                if (pos < this->_count)
                    return children[pos]->minimum();
                else
                    return this->_parent->successor(key);
            }

            virtual Base_Leaf_ptr predecessor(const Key &key) override {
                int pos = this->_count - 1;
                const auto depth = this->_depth + this->_prefix_length;
                for (; pos >= 0 && keys[pos] >= key.chunks[depth]; pos--);
                if (pos >= 0)
                    return children[pos]->maximum();
                else
                    return this->_parent->predecessor(key);
            }

            virtual Const_Base_Leaf_ptr predecessor(const Key &key) const override {
                int pos = this->_count - 1;
                const auto depth = this->_depth + this->_prefix_length;
                for (; pos >= 0 && keys[pos] >= key.chunks[depth]; pos--);
                if (pos >= 0)
                    return children[pos]->maximum();
                else
                    return this->_parent->predecessor(key);
            }

            virtual uint16_t min_size() const override { return 5; }

            virtual uint16_t max_size() const override { return 16; }

            virtual node_type get_type() const override { return node_type::node_16_t; }

#ifdef ART_DEBUG

            virtual void debug() const override {
                std::cout << this << " Node 16, depth: " << this->_depth
                          << " count: " << this->size()
                          << ", parent " << this->_parent << std::endl;
                this->debug_prefix();

                for (size_t i = 0; i < this->_count; i++) {
                    std::cout << ((unsigned) keys[i]) << " | ";
                }
                std::cout << std::endl;
                for (size_t i = 0; i < this->_count; i++) {
                    std::cout << children[i] << " | ";
                }
                std::cout << std::endl;
            }

#endif
        };


        struct _Node_48 : public _Inner_Node {
        public:
            std::array<byte, 256> child_index;
            std::array<Node_ptr, 48> children{};

            // Grow constructor
            _Node_48(_Node_16 *node)
                    : _Inner_Node(node->_parent, 16, node->_depth, node->_prefix_length, node->_prefix) {
                std::fill(child_index.begin(), child_index.end(), EMPTY_MARKER);

                for (uint8_t i = 0; i < 16; i++) {
                    child_index[node->keys[i]] = i;
                    children[i] = node->children[i];
                    children[i]->_parent = this;
                }
            }

            // Shrink constructor
            _Node_48(_Node_256 *node)
                    : _Inner_Node(node->_parent, 48, node->_depth, node->_prefix_length, node->_prefix) {
                std::fill(child_index.begin(), child_index.end(), EMPTY_MARKER);

                uint8_t pos = 0;
                for (uint16_t i = 0; i < 256; i++) {
                    if (node->children[i] != nullptr) {
                        child_index[i] = pos;
                        children[pos] = node->children[i];
                        children[pos]->_parent = this;
                        pos++;
                    }
                }
            }

            // Copy constructor
            _Node_48(const _Node_48 &__x)
                    : _Inner_Node(__x._parent, __x._count, __x._depth), child_index(__x.child_index) {
                copy_children(__x);
            }

            // Copy assignment
            _Node_48 &operator=(const _Node_48 &__x) {
                this->_parent = __x._parent;
                this->_count = __x._count;
                this->_depth = __x._depth;
                this->_prefix_length = __x._prefix_length;
                this->_prefix = __x._prefix;
                child_index = __x.child_index;
                copy_children(__x);
                return *this;
            }

            void copy_children(const _Node_48 &__x) {
                for (int i = 0; i < 256; i++) {
                    if (__x.child_index[i] != EMPTY_MARKER) {
                        switch (__x.children[child_index[i]]->get_type()) {
                            case node_type::node_4_t:
                                children[child_index[i]] = new _Node_4(*static_cast<_Node_4 *>(__x.children[i]));
                                children[child_index[i]]->_parent = this;
                                break;
                            case node_type::node_16_t:
                                children[child_index[i]] = new _Node_16(*static_cast<_Node_16 *>(__x.children[i]));
                                children[child_index[i]]->_parent = this;
                                break;
                            case node_type::node_48_t:
                                children[child_index[i]] = new _Node_48(*static_cast<_Node_48 *>(__x.children[i]));
                                children[child_index[i]]->_parent = this;
                                break;
                            case node_type::node_256_t:
                                children[child_index[i]] = new _Node_256(*static_cast<_Node_256 *>(__x.children[i]));
                                children[child_index[i]]->_parent = this;
                                break;
                            case node_type::_leaf_t:
                                children[child_index[i]] = new _Leaf(*static_cast<Leaf_ptr>(__x.children[i]));
                                children[child_index[i]]->_parent = this;
                                break;
                            default:
                                throw;
                                break;
                        }
                    }
                }
            }

            void clear() override {
                for (size_t i = 0; i < 256; i++) {
                    if (child_index[i] != EMPTY_MARKER) {
                        children[child_index[i]]->clear();
                        delete children[child_index[i]];
                    }
                }
            }

            virtual void insert(const byte key_byte, Node_ptr node) override {
                unsigned pos = 0;
                for (; children[pos] != nullptr; pos++);
                child_index[key_byte] = pos;
                children[pos] = node;
                this->_count++;
            }

            virtual void erase(const byte key_byte) override {
                delete children[child_index[key_byte]];
                children[child_index[key_byte]] = nullptr;
                child_index[key_byte] = EMPTY_MARKER;
                this->_count--;
            }

            virtual Node_ptr find(const byte key_byte) override {
                if (child_index[key_byte] != EMPTY_MARKER)
                    return children[child_index[key_byte]];
                return nullptr;
            }

            virtual Const_Node_ptr find(const byte key_byte) const override {
                if (child_index[key_byte] != EMPTY_MARKER)
                    return children[child_index[key_byte]];
                return nullptr;
            }

            virtual void update_child_ptr(const byte key_byte, Node_ptr node) override {
                if (child_index[key_byte] != EMPTY_MARKER)
                    children[child_index[key_byte]] = node;
            }

            virtual Base_Leaf_ptr minimum() override {
                unsigned pos = 0;
                while (child_index[pos] == EMPTY_MARKER)
                    pos++;
                return children[child_index[pos]]->minimum();
            }

            virtual Const_Base_Leaf_ptr minimum() const override {
                unsigned pos = 0;
                while (child_index[pos] == EMPTY_MARKER)
                    pos++;
                return children[child_index[pos]]->minimum();
            }

            virtual Base_Leaf_ptr maximum() override {
                auto pos = 255;
                while (child_index[pos] == EMPTY_MARKER)
                    pos--;
                return children[child_index[pos]]->maximum();
            }

            virtual Const_Base_Leaf_ptr maximum() const override {
                auto pos = 255;
                while (child_index[pos] == EMPTY_MARKER)
                    pos--;
                return children[child_index[pos]]->maximum();
            }

            virtual Base_Leaf_ptr successor(const Key &key) override {
                for (unsigned pos = ((unsigned) key.chunks[this->_depth + this->_prefix_length]) + 1; pos < 256; pos++)
                    if (child_index[pos] != EMPTY_MARKER)
                        return children[child_index[pos]]->minimum();

                return this->_parent->successor(key);
            }

            virtual Const_Base_Leaf_ptr successor(const Key &key) const override {
                for (unsigned pos = ((unsigned) key.chunks[this->_depth + this->_prefix_length]) + 1; pos < 256; pos++)
                    if (child_index[pos] != EMPTY_MARKER)
                        return children[child_index[pos]]->minimum();

                return this->_parent->successor(key);
            }

            virtual Base_Leaf_ptr predecessor(const Key &key) override {
                for (int pos = ((int) key.chunks[this->_depth + this->_prefix_length]) - 1; pos >= 0; pos--)
                    if (child_index[pos] != EMPTY_MARKER)
                        return children[child_index[pos]]->maximum();

                return this->_parent->predecessor(key);
            }

            virtual Const_Base_Leaf_ptr predecessor(const Key &key) const override {
                for (int pos = ((int) key.chunks[this->_depth + this->_prefix_length]) - 1; pos >= 0; pos--)
                    if (child_index[pos] != EMPTY_MARKER)
                        return children[child_index[pos]]->maximum();

                return this->_parent->predecessor(key);
            }

            virtual uint16_t min_size() const override { return 17; }

            virtual uint16_t max_size() const override { return 48; }

            virtual node_type get_type() const override { return node_type::node_48_t; }

#ifdef ART_DEBUG

            virtual void debug() const override {
                std::cout << this << " Node 48, depth: " << this->_depth
                          << " count: " << this->size()
                          << ", parent " << this->_parent << std::endl;
                this->debug_prefix();

                for (size_t i = 0; i < 256; i++) {
                    if (child_index[i] != EMPTY_MARKER)
                        std::cout << i << " | ";
                }
                std::cout << std::endl;
                for (size_t i = 0; i < 256; i++) {
                    if (child_index[i] != EMPTY_MARKER)
                        std::cout << i << ": " << children[child_index[i]] << " | ";
                }
                std::cout << std::endl;
            }

#endif
        };

        struct _Node_256 : public _Inner_Node {
        public:
            std::array<Node_ptr, 256> children{};

            // Grow constructor
            _Node_256(_Node_48 *node)
                    : _Inner_Node(node->_parent, 48, node->_depth, node->_prefix_length, node->_prefix) {
                for (uint16_t i = 0; i < 256; i++) {
                    if (node->child_index[i] != EMPTY_MARKER) {
                        children[i] = node->children[node->child_index[i]];
                        children[i]->_parent = this;
                    }
                }
            }

            // Copy constructor
            _Node_256(const _Node_256 &__x)
                    : _Inner_Node(__x._parent, __x._count, __x._depth, __x._prefix_length, __x._prefix) {
                copy_children(__x);
            }

            // Copy assignment
            _Node_256 &operator=(const _Node_256 &__x) {
                this->_parent = __x._parent;
                this->_count = __x._count;
                this->_depth = __x._depth;
                this->_prefix_length = __x._prefix_length;
                this->_prefix = __x._prefix;
                copy_children(__x);
                return *this;
            }

            void copy_children(const _Node_256 &__x) {
                for (int i = 0; i < 256; i++) {
                    if (__x.children[i] != nullptr) {
                        switch (__x.children[i]->get_type()) {
                            case node_type::node_4_t:
                                children[i] = new _Node_4(*static_cast<_Node_4 *>(__x.children[i]));
                                children[i]->_parent = this;
                                break;
                            case node_type::node_16_t:
                                children[i] = new _Node_16(*static_cast<_Node_16 *>(__x.children[i]));
                                children[i]->_parent = this;
                                break;
                            case node_type::node_48_t:
                                children[i] = new _Node_48(*static_cast<_Node_48 *>(__x.children[i]));
                                children[i]->_parent = this;
                                break;
                            case node_type::node_256_t:
                                children[i] = new _Node_256(*static_cast<_Node_256 *>(__x.children[i]));
                                children[i]->_parent = this;
                                break;
                            case node_type::_leaf_t:
                                children[i] = new _Leaf(*static_cast<Leaf_ptr>(__x.children[i]));
                                children[i]->_parent = this;
                                break;
                            default:
                                throw;
                                break;
                        }
                    }
                }
            }

            void clear() override {
                for (size_t i = 0; i < 256; i++) {
                    if (children[i] != nullptr) {
                        children[i]->clear();
                        delete children[i];
                    }
                }
            }

            virtual void insert(const byte key_byte, Node_ptr node) override {
                children[key_byte] = node;
                this->_count++;
            }

            virtual void erase(const byte key_byte) override {
                delete children[key_byte];
                children[key_byte] = nullptr;
                this->_count--;
            }

            virtual Node_ptr find(const byte key_byte) override {
                if (children[key_byte] != nullptr)
                    return children[key_byte];
                return nullptr;
            }

            virtual Const_Node_ptr find(const byte key_byte) const override {
                if (children[key_byte] != nullptr)
                    return children[key_byte];
                return nullptr;
            }

            virtual void update_child_ptr(const byte key_byte, Node_ptr node) override {
                if (children[key_byte] != nullptr)
                    children[key_byte] = node;
            }

            virtual Base_Leaf_ptr minimum() override {
                unsigned pos = 0;
                while (children[pos] == nullptr)
                    pos++;
                return children[pos]->minimum();
            }

            virtual Const_Base_Leaf_ptr minimum() const override {
                unsigned pos = 0;
                while (children[pos] == nullptr)
                    pos++;
                return children[pos]->minimum();
            }

            virtual Base_Leaf_ptr maximum() override {
                auto pos = 255;
                while (children[pos] == nullptr)
                    pos--;
                return children[pos]->maximum();
            }

            virtual Const_Base_Leaf_ptr maximum() const override {
                auto pos = 255;
                while (children[pos] == nullptr)
                    pos--;
                return children[pos]->maximum();
            }

            virtual Base_Leaf_ptr successor(const Key &key) override {
                for (unsigned pos = ((unsigned) key.chunks[this->_depth + this->_prefix_length]) + 1; pos < 256; pos++)
                    if (children[pos] != nullptr)
                        return children[pos]->minimum();

                return this->_parent->successor(key);
            }

            virtual Const_Base_Leaf_ptr successor(const Key &key) const override {
                for (unsigned pos = ((unsigned) key.chunks[this->_depth + this->_prefix_length]) + 1; pos < 256; pos++)
                    if (children[pos] != nullptr)
                        return children[pos]->minimum();

                return this->_parent->successor(key);
            }

            virtual Base_Leaf_ptr predecessor(const Key &key) override {
                for (int pos = key.chunks[this->_depth + this->_prefix_length] - 1; pos >= 0; pos--)
                    if (children[pos] != nullptr)
                        return children[pos]->maximum();

                return this->_parent->predecessor(key);
            }

            virtual Const_Base_Leaf_ptr predecessor(const Key &key) const override {
                for (int pos = key.chunks[this->_depth + this->_prefix_length] - 1; pos >= 0; pos--)
                    if (children[pos] != nullptr)
                        return children[pos]->maximum();

                return this->_parent->predecessor(key);
            }

            virtual uint16_t min_size() const override { return 49; }

            virtual uint16_t max_size() const override { return 256; }

            virtual node_type get_type() const override { return node_type::node_256_t; }

#ifdef ART_DEBUG

            virtual void debug() const override {
                std::cout << this << " Node 256, depth: " << this->_depth
                          << " count: " << this->size()
                          << ", parent " << this->_parent << std::endl;
                this->debug_prefix();

                for (size_t i = 0; i < 256; i++) {
                    if (children[i] != nullptr)
                        std::cout << i << " | ";
                    else
                        std::cout << "--" << " | ";
                }
                std::cout << std::endl;
                for (size_t i = 0; i < 256; i++) {
                    if (children[i] != nullptr)
                        std::cout << i << ": " << children[i] << " | ";
                    else
                        std::cout << "--" << " | ";
                }
                std::cout << std::endl;
            }

#endif
        };

    private:
        size_t _M_count;

        // Dummy node of special type to mark the right end of the container
        // Necessary as the end marker for iterators
        _Dummy_Node *_M_dummy_node;

    public:
        // Current root node of the radix tree
        Node_ptr _M_root;

        // Default constructor
        ar_prefix_tree() {
            init();
        }

        ar_prefix_tree(const _Key_transform &key_transformer)
                : _M_key_transform(key_transformer) {
            init();
        }

        // Copy constructor
        ar_prefix_tree(const ar_prefix_tree &__x) {
            if (__x.empty()) {
                // Nothing to copy
                init();
                return;
            }

            _M_root = copy_node(__x._M_root);
            _M_count = __x._M_count;
            _M_dummy_node = new _Dummy_Node();
            _M_root->_parent = _M_dummy_node;
            _M_dummy_node->_root = _M_root;

            _M_key_transform = __x._M_key_transform;
        }

        // Move constructor
        ar_prefix_tree(ar_prefix_tree &&__x) {
            _M_root = std::move(__x._M_root);
            _M_count = std::move(__x._M_count);
            _M_key_transform = std::move(__x._M_key_transform);

            _M_dummy_node = new _Dummy_Node();
            if (_M_root != nullptr) {
                _M_root->_parent = _M_dummy_node;
                _M_dummy_node->_root = _M_root;
            }

            // Leaf move source in a valid state
            __x._M_root = nullptr;
            __x._M_count = 0;
        }

        // Copy assignment
        ar_prefix_tree &operator=(const ar_prefix_tree &__x) {
            if (this != &__x) {
                // remove old container contents
                clear();

                if (__x.empty()) {
                    // Nothing to copy
                    init();
                    return *this;
                }

                // Then do copy construction
                _M_root = copy_node(__x._M_root);
                _M_count = __x._M_count;
                _M_root->_parent = _M_dummy_node;
                _M_dummy_node->_root = _M_root;

                _M_key_transform = __x._M_key_transform;
            }
            return *this;
        }

        // Move assignment
        ar_prefix_tree &operator=(ar_prefix_tree &&__x) {
            _M_root = std::move(__x._M_root);
            _M_count = std::move(__x._M_count);
            _M_key_transform = std::move(__x._M_key_transform);

            if (_M_root != nullptr) {
                _M_root->_parent = _M_dummy_node;
                _M_dummy_node->_root = _M_root;
            }

            __x._M_root = nullptr;
            __x._M_count = 0;
            return *this;
        }

        void init() {
            _M_count = 0;
            _M_root = nullptr;
            _M_dummy_node = new _Dummy_Node();
        }

        Node_ptr copy_node(Node_ptr __x) {
            switch (__x->get_type()) {
                case node_type::node_4_t:
                    return new _Node_4(*static_cast<_Node_4 *>(__x));
                case node_type::node_16_t:
                    return new _Node_16(*static_cast<_Node_16 *>(__x));
                case node_type::node_48_t:
                    return new _Node_48(*static_cast<_Node_48 *>(__x));
                case node_type::node_256_t:
                    return new _Node_256(*static_cast<_Node_256 *>(__x));
                case node_type::_leaf_t:
                    return new _Leaf(*static_cast<Leaf_ptr>(__x));
                default:
                    throw;
            }
        }

        //////////////
        // Capacity //
        //////////////

        size_t size() const {
            return _M_count;
        }

        size_t max_size() const {
            if (sizeof(transformed_key_type) < sizeof(size_type))
                return size_type(1) << (8 * sizeof(transformed_key_type));
            return std::numeric_limits<size_type>::max();
        }

        bool empty() const {
            return _M_count == 0;
        }

        ///////////////
        // Iterators //
        ///////////////

        struct ar_prefix_tree_iterator {
            typedef _Value value_type;
            typedef value_type &reference;
            typedef value_type *pointer;

            typedef std::bidirectional_iterator_tag iterator_category;
            typedef ptrdiff_t difference_type;

            typedef ar_prefix_tree_iterator _Self;

            // pointer to current leaf node
            Base_Leaf_ptr node;

            ar_prefix_tree_iterator() : node(nullptr) {}


            ar_prefix_tree_iterator(Base_Leaf_ptr node)
                    : node(node) {
            }

            // preincrement
            _Self &operator++() {
                Inner_Node_ptr parent = static_cast<Inner_Node_ptr>(node->_parent);
                if (parent->get_type() == node_type::_dummy_node_t) {
                    node = parent->successor({_Key_transform()(key_type())});
                } else {
                    Leaf_ptr leaf = static_cast<Leaf_ptr>(node);
                    node = parent->successor({_Key_transform()(_KeyOfValue()(leaf->_value))});
                }
                return *this;
            }

            // postincrement
            _Self operator++(int) {
                _Self retval = *this;
                ++(*this);
                return retval;
            }

            // predecrement
            _Self &operator--() {
                Inner_Node_ptr parent = static_cast<Inner_Node_ptr>(node->_parent);
                if (parent->get_type() == node_type::_dummy_node_t) {
                    node = parent->predecessor({_Key_transform()(key_type())});
                } else {
                    Leaf_ptr leaf = static_cast<Leaf_ptr>(node);
                    node = parent->predecessor({_Key_transform()(_KeyOfValue()(leaf->_value))});
                }
                return *this;
            }

            // postdecrement
            _Self operator--(int) {
                _Self retval = *this;
                --(*this);
                return retval;
            }

            bool operator==(const _Self &other) const {
                return node == other.node;
            }

            bool operator!=(const _Self &other) const {
                return !(*this == other);
            }

            reference operator*() const {
                return static_cast<Leaf_ptr>(node)->_value;
            }

            pointer operator->() const {
                return &static_cast<Leaf_ptr>(node)->_value;
            }
        };

        struct ar_prefix_tree_const_iterator {
            typedef _Value value_type;
            typedef const value_type &reference;
            typedef const value_type *pointer;

            typedef ar_prefix_tree_iterator iterator;

            typedef std::bidirectional_iterator_tag iterator_category;
            typedef ptrdiff_t difference_type;

            typedef ar_prefix_tree_const_iterator _Self;

            // pointer to current leaf node
            Const_Base_Leaf_ptr node;

            ar_prefix_tree_const_iterator() : node(nullptr) {}

            // Copy Constructor
            ar_prefix_tree_const_iterator(const iterator &__it)
                    : node(__it.node) {}

            explicit ar_prefix_tree_const_iterator(Const_Base_Leaf_ptr node)
                    : node(node) {
            }

            // preincrement
            _Self &operator++() {
                Const_Inner_Node_ptr parent = static_cast<Const_Inner_Node_ptr>(node->_parent);
                if (parent->get_type() == node_type::_dummy_node_t) {
                    node = parent->successor({_Key_transform()(key_type())});
                } else {
                    Const_Leaf_ptr leaf = static_cast<Const_Leaf_ptr>(node);
                    node = parent->successor({_Key_transform()(_KeyOfValue()(leaf->_value))});
                }
                return *this;
            }

            // postincrement
            _Self operator++(int) {
                _Self retval = *this;
                ++(*this);
                return retval;
            }

            // predecrement
            _Self &operator--() {
                Const_Inner_Node_ptr parent = static_cast<Const_Inner_Node_ptr>(node->_parent);
                if (parent->get_type() == node_type::_dummy_node_t) {
                    node = parent->predecessor({_Key_transform()(key_type())});
                } else {
                    Const_Leaf_ptr leaf = static_cast<Const_Leaf_ptr>(node);
                    node = parent->predecessor({_Key_transform()(_KeyOfValue()(leaf->_value))});
                }
                return *this;
                return *this;
            }

            // postdecrement
            _Self operator--(int) {
                _Self retval = *this;
                --(*this);
                return retval;
            }

            iterator _const_cast() const {
                return iterator(const_cast<Node_ptr>(node));
            }

            bool operator==(const _Self &other) const {
                return node == other.node;
            }

            bool operator!=(const _Self &other) const {
                return !(*this == other);
            }

            reference operator*() const {
                return static_cast<Const_Leaf_ptr>(node)->_value;
            }

            pointer operator->() const {
                return &static_cast<Const_Leaf_ptr>(node)->_value;
            }
        };

        typedef ar_prefix_tree_iterator iterator;
        typedef ar_prefix_tree_const_iterator const_iterator;
        typedef std::reverse_iterator<iterator> reverse_iterator;
        typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
        typedef size_t size_type;
        typedef ptrdiff_t difference_type;

        iterator begin() {
            Base_Leaf_ptr min = minimum();
            if (min == nullptr)
                return iterator(_M_dummy_node->_leaf);

            return iterator(min);
        }

        const_iterator begin() const {
            Const_Base_Leaf_ptr min = minimum();
            if (min == nullptr)
                return const_iterator(_M_dummy_node->_leaf);
            return const_iterator(min);
        }

        iterator end() {
            return iterator(_M_dummy_node->_leaf);
        }

        const_iterator end() const {
            return const_iterator(_M_dummy_node->_leaf);
        }

        reverse_iterator rbegin() {
            return reverse_iterator(end());
        }

        const_reverse_iterator rbegin() const {
            return const_reverse_iterator(end());
        }

        reverse_iterator rend() {
            return reverse_iterator(begin());
        }

        const_reverse_iterator rend() const {
            return const_reverse_iterator(begin());
        }

        _Key_transform key_trans() const {
            return _M_key_transform;
        }

        ///////////////
        // Modifiers //
        ///////////////

        void clear() {
            if (_M_root != nullptr) {
                _M_root->clear();
                delete _M_root;
            }
            _M_root = nullptr;
            _M_count = 0;
        }


        pair<iterator, bool> insert_unique(const value_type &__x) {
            Key transformed_key = {_M_key_transform(_KeyOfValue()(__x))};
            const auto key_size = sizeof(Key);

            // Empty Tree
            if (_M_root == nullptr) {
                Leaf_ptr new_leaf = new _Leaf(__x, _M_dummy_node);
                _M_root = new_leaf;
                _M_count++;

                _M_dummy_node->_root = _M_root;
                return make_pair(iterator(new_leaf), true);
            }

            Node_ptr current_node = _M_root;
            Node_ptr previous_node = _M_dummy_node;

            for (unsigned depth = 0; depth < key_size + 1; depth++) {
                if (current_node != nullptr) {
                    if (current_node->is_leaf()) {
                        // Hit an existing leaf
                        Leaf_ptr existing_leaf = static_cast<Leaf_ptr>(current_node);
                        Key existing_key = {_M_key_transform(_KeyOfValue()(existing_leaf->_value))};
                        if (!std::memcmp(&transformed_key, &existing_key, sizeof(transformed_key))) {
                            // if it is a duplicate entry, ignore
                            return make_pair(iterator(existing_leaf), false);
                        } else {
                            for (unsigned j = depth; j < key_size; j++) {
                                // if the keys are matching, go down all the way until we find a tiebreaker
                                if (transformed_key.chunks[j] != existing_key.chunks[j]) {
                                    // once we find a tiebreaker, create a new node with that key byte
                                    // and add the skipped key bytes as a prefix
                                    Inner_Node_ptr inner = new _Node_4(existing_leaf, existing_key.chunks[j], depth);
                                    update_child_ptr(inner->_parent, inner, existing_key);
                                    inner->_prefix_length = (uint16_t) (j - depth);
                                    if (inner->_prefix_length) {
                                        std::copy(std::begin(transformed_key.chunks) + depth,
                                                  std::begin(transformed_key.chunks) + depth +
                                                  std::min((size_t) inner->_prefix_length, MAX_PREFIX_LENGTH),
                                                  inner->_prefix.begin());
                                    }

                                    Leaf_ptr new_leaf = new _Leaf(__x, inner);
                                    inner->insert(transformed_key.chunks[j], new_leaf);
                                    _M_count++;

                                    return make_pair(iterator(new_leaf), true);
                                }
                            }
                            throw; // unreachable
                        }
                    } else {
                        // traverse down the tree, first check the prefix,
                        // then find the next child with the key byte
                        Inner_Node_ptr current_inner = static_cast<Inner_Node_ptr>(current_node);
                        auto pos = current_inner->prefix_mismatch_pos(transformed_key);
                        if (pos < current_inner->_prefix_length) {
                            current_node = split_prefix_into_parent(current_inner, pos);
                            update_child_ptr(current_node->_parent, current_node, transformed_key);
                        }
                        depth += pos;

                        previous_node = current_node;
                        current_node = current_node->find(transformed_key.chunks[depth]);
                    }
                } else {
                    // hit empty point, this can only happen if the inserted key
                    // is not a prefix/equal to another key already in the tree
                    // therefore we can just insert a new leaf
                    // previous node might have to be grown before that
                    if (previous_node->size() == previous_node->max_size()) {
                        previous_node = grow(previous_node);
                        update_child_ptr(previous_node->_parent, previous_node, transformed_key);
                    }

                    Leaf_ptr new_leaf = new _Leaf(__x, previous_node);
                    previous_node->insert(transformed_key.chunks[depth - 1], new_leaf);
                    _M_count++;

                    return make_pair(iterator(new_leaf), true);
                }
            }
            throw; // unreachable
        }

        template<typename _InputIterator>
        void insert_unique(_InputIterator __first, _InputIterator __last) {
            for (; __first != __last; ++__first)
                insert_unique(*__first);
        }

        template<typename _InputIterator>
        void assign_unique(_InputIterator __first, _InputIterator __last) {
            clear();
            for (; __first != __last; ++__first)
                insert_unique(*__first);
        }

        template<typename... _Args>
        pair<iterator, bool> emplace_unique(_Args &&... __args) {
            // try to create a leaf given the arguments
            Leaf_ptr leaf;
            try {
                // @TODO forward directly into constructor
                leaf = new _Leaf(value_type(std::forward<_Args>(__args)...));
            } catch (...) {
                delete leaf;
                // leaf->~_Leaf();
                throw;
            }

            // try inserting the leaf
            try {
                Key key = {_M_key_transform(_KeyOfValue()(leaf->_value))};
                Node_ptr node = get_insert_unique_pos(key);

                // if position of insertion is a leaf, the key already exists
                if (node->get_type() != node_type::_leaf_t)
                    return pair<iterator, int>(insert_leaf(node, leaf, key), true);

                delete leaf;
                return pair<iterator, int>(iterator(static_cast<Leaf_ptr>(node)), false);
            } catch (...) {
                delete leaf;
                throw;
            }
        }

    private:
        /**
         * @brief Splits the prefix of node into a parent node.
         * @param node  Pointer to node that will be split.
         * @param mismatch_pos  Position where the prefix split will occur.
         * @return Pointer to he newly created node where the first part of the prefix was moved to.
         *
         * Implements a hybrid compression approach, loads key from leaf if necessary.
         */
        Node_ptr split_prefix_into_parent(Inner_Node_ptr node, size_t mismatch_pos) {
            Node_ptr parent = node->_parent;

            Inner_Node_ptr new_node;
            if (node->_prefix_length < MAX_PREFIX_LENGTH) {
                // pessimistic path compression
                new_node = new _Node_4(node, node->_prefix[mismatch_pos], node->_depth);
                new_node->_prefix_length = (uint16_t) mismatch_pos;
                // move first half of the prefix to the new node
                std::move(node->_prefix.begin(),
                          node->_prefix.begin() + mismatch_pos,
                          new_node->_prefix.begin());
                // move second half of prefix to the front
                std::move(node->_prefix.begin() + mismatch_pos + 1,
                          node->_prefix.end(),
                          node->_prefix.begin());
            } else {
                // optimistic path compression, key has to be loaded
                // from a leaf and copied to the node's prefix
                Key min_key = {_Key_transform()(_KeyOfValue()(static_cast<Const_Leaf_ptr>(node->minimum())->_value))};

                new_node = new _Node_4(node, min_key.chunks[node->_depth + mismatch_pos], node->_depth);
                new_node->_prefix_length = (uint16_t) mismatch_pos;

                // move whole prefix to new node as the mismatch pos is after the prefix array
                std::move(node->_prefix.begin(), node->_prefix.end(), new_node->_prefix.begin());

                // shift the prefix array of the old node
                // load further prefix bytes from leaf as necessary
                const auto start = node->_depth + mismatch_pos + 1;
                const auto prefix_length = std::min(MAX_PREFIX_LENGTH,
                                                    ((int32_t) node->_prefix_length) - (mismatch_pos + 1));
                const auto end = start + prefix_length;

                std::copy(std::begin(min_key.chunks) + start,
                          std::begin(min_key.chunks) + end,
                          node->_prefix.begin());
            }

            node->_prefix_length -= (mismatch_pos + 1);
            node->_depth += mismatch_pos + 1;

            return new_node;
        }

        /**
         * @brief Removes a one-way node and moves its prefix and key into its only child.
         * @param node  Pointer to one-way node that will be removed.
         * @param child Pointer to only child of the one-way node.
         */
        Node_ptr compress_node_into_child(_Node_4 *node) {
            std::array<byte, MAX_PREFIX_LENGTH> prefix;

            Node_ptr child = node->children[0];

            if (child->get_type() == node_type::_leaf_t) {
                child->_parent = node->_parent;
                if (node->_depth == 0)
                    replace_root(child);
            } else {
                auto child_inner = static_cast<Inner_Node_ptr>(child);
                if (node->_prefix_length < MAX_PREFIX_LENGTH) {
                    // shift existing prefix to the right
                    std::move(child_inner->_prefix.begin(),
                              child_inner->_prefix.begin() + (MAX_PREFIX_LENGTH - node->_prefix_length - 1),
                              child_inner->_prefix.begin() + node->_prefix_length + 1);
                    // move prefix and key of node into the child
                    std::move(node->_prefix.begin(), node->_prefix.begin() + node->_prefix_length,
                              child_inner->_prefix.begin());
                    child_inner->_prefix[node->_prefix_length] = node->keys[0];
                } else {
                    std::move(node->_prefix.begin(), node->_prefix.end(), child_inner->_prefix.begin());
                }
                child_inner->_depth = node->_depth;
                child_inner->_prefix_length += node->_prefix_length + 1;
                child_inner->_parent = node->_parent;
            }

            delete node;
            return child;
        }

        /**
         * @brief Updates the child pointer of the parent.
         * @param parent  Pointer to parent whose child pointer is out of date.
         * @param child  Pointer to the new child.
         * @param key  Key to know the point of insertion.
         *
         * Updates the children array of a node when one of its children has
         * changed location in memory (grown/shrunken).
         */
        inline void update_child_ptr(Node_ptr parent, Node_ptr child, const Key &key) {
            if (parent->get_type() == node_type::_dummy_node_t) {
                replace_root(child);
            } else {
                Inner_Node_ptr inner_parent = static_cast<Inner_Node_ptr>(parent);
                const int32_t depth = inner_parent->_depth + inner_parent->_prefix_length;
                inner_parent->update_child_ptr(key.chunks[depth], child);
            }
        }

        /**
         * @brief  Returns the inner node where a leaf for the given key would be inserted.
         * @param __k  Key to be inserted
         * @return  If the key is not already in the tree, an inner node pointer is returned
         *          where insertion may take place. Otherwise, a pointer to the existing leaf
         *          is returned.
         *
         * If a leaf is hit with non-matching key, the leaf will be replaced by a node 4 and
         * a pointer to that node will be returned. If an inner node is at its capacity limit
         * the node will be grown before a pointer is returned.
         */
        Node_ptr get_insert_unique_pos(const Key &__k) {
            const auto key_size = sizeof(Key);

            // Empty Tree
            if (_M_root == nullptr)
                return _M_dummy_node;

            Node_ptr previous_node = _M_dummy_node;
            Node_ptr current_node = _M_root;

            for (int32_t depth = 0; depth < key_size + 1; depth++) {
                if (current_node != nullptr) {
                    if (current_node->is_leaf()) {
                        Leaf_ptr existing_leaf = reinterpret_cast<Leaf_ptr>(current_node);
                        Key existing_key = {_M_key_transform(_KeyOfValue()(existing_leaf->_value))};
                        if (!std::memcmp(&__k, &existing_key, sizeof(__k))) {
                            return existing_leaf;
                        } else {
                            for (int32_t j = depth; j < key_size; j++) {
                                if (__k.chunks[j] != existing_key.chunks[j]) {
                                    Inner_Node_ptr inner = new _Node_4(existing_leaf, existing_key.chunks[j], depth);
                                    update_child_ptr(inner->_parent, inner, existing_key);

                                    inner->_prefix_length = (uint16_t) (j - depth);
                                    std::copy(std::begin(__k.chunks) + depth,
                                              std::begin(__k.chunks) + depth +
                                              std::min((size_t) inner->_prefix_length, MAX_PREFIX_LENGTH),
                                              inner->_prefix.begin());

                                    return inner;
                                }
                            }
                            throw; // unreachable
                        }
                    } else {
                        Inner_Node_ptr current_inner = static_cast<Inner_Node_ptr>(current_node);
                        auto pos = current_inner->prefix_mismatch_pos(__k);
                        if (pos < current_inner->_prefix_length) {
                            current_node = split_prefix_into_parent(current_inner, pos);
                            update_child_ptr(current_node->_parent, current_node, __k);
                        }
                        depth += pos;
                        // traverse down the tree
                        previous_node = current_node;
                        current_node = (current_node)->find(__k.chunks[depth]);
                    }
                } else {
                    // hit empty point, this can only happen if the inserted key
                    // is not a prefix/equal to another key already in the tree
                    // therefore we can just insert a new leaf
                    // previous node might have to be grown before that
                    if (previous_node->size() == previous_node->max_size()) {
                        previous_node = grow(previous_node);
                        update_child_ptr(previous_node->_parent, previous_node, __k);
                    }

                    return previous_node;
                }
            }
            throw; // unreachable
        }

        /**
         * @brief Inserts an already constructed leaf into its parent.
         * @param parent  Pointer to the inner node that will be the
         *                immediate parent of the leaf.
         * @param leaf  Pointer to the leaf that will be inserted.
         * @param key  Key of leaf.
         * @return Return An iterator pointing to the inserted leaf.
         */
        iterator insert_leaf(Node_ptr parent, Leaf_ptr leaf, const Key &key) {
            leaf->_parent = parent;

            if (parent->get_type() != node_type::_dummy_node_t) {
                auto inner_parent = static_cast<Inner_Node_ptr>(parent);
                inner_parent->insert(key.chunks[inner_parent->_depth + inner_parent->_prefix_length], leaf);
            } else {
                replace_root(leaf);
            }

            _M_count++;
            return iterator(leaf);
        };

        /**
         * @brief Replaces the root of the tree.
         * @param node  Pointer to the node that will be the new root.
         *
         * Also updates the pointer of the dummy node to the new root.
         */
        inline void replace_root(Node_ptr node) {
            _M_root = node;
            _M_dummy_node->_root = _M_root;
        }

    public:

        size_type erase_unique(const key_type &__k) {
            // Empty Tree
            if (_M_root == nullptr)
                return 0;

            Key transformed_key = {_M_key_transform(__k)};
            const auto key_size = sizeof(Key);

            if (_M_root->is_leaf()) {
                Key existing_key = {_M_key_transform(_KeyOfValue()(static_cast<Leaf_ptr>(_M_root)->_value))};
                if (!std::memcmp(&transformed_key, &existing_key, sizeof(transformed_key))) {
                    delete _M_root;
                    _M_root = nullptr;
                    _M_count--;
                    return 1;
                }
                return 0;
            }

            Node_ptr current_node = _M_root;

            for (int32_t depth = 0; depth < key_size; depth++) {
                // traverse down the tree
                Inner_Node_ptr current_inner = static_cast<Inner_Node_ptr>(current_node);
                auto pos = current_inner->prefix_mismatch_pos(transformed_key);
                if (pos < current_inner->_prefix_length)
                    return 0;

                depth += pos;
                Node_ptr child = current_node->find(transformed_key.chunks[depth]);

                if (child == nullptr)
                    return 0;

                if (child->is_leaf()) {
                    Leaf_ptr existing_leaf = static_cast<Leaf_ptr>(child);
                    Key existing_key = {_M_key_transform(_KeyOfValue()(existing_leaf->_value))};
                    if (!std::memcmp(&transformed_key, &existing_key, sizeof(transformed_key))) {
                        // Delete the leaf
                        current_node->erase(transformed_key.chunks[depth]);
                        _M_count--;

                        fix_after_erase(static_cast<Inner_Node_ptr>(current_node), transformed_key);

                        return 1;
                    } else {
                        return 0; // key not in tree (leaf mismatch)
                    }
                } else {
                    current_node = child;
                }

            }
            throw; // unreachable
        }

        iterator erase(iterator __it) {
            iterator __result = __it;
            ++__result;

            // Delete leaf
            Key transformed_key = {_M_key_transform(_KeyOfValue()(*__it))};
            Leaf_ptr leaf = static_cast<Leaf_ptr >(__it.node);

            // Edge case, leaf is the root/last element in container
            if (leaf == _M_root) {
                delete leaf;
                _M_root = nullptr;
                _M_count--;
                return __result;
            }

            Inner_Node_ptr inner_node = static_cast<Inner_Node_ptr>(leaf->_parent);
            inner_node->erase(transformed_key.chunks[inner_node->_depth + inner_node->_prefix_length]);
            _M_count--;

            fix_after_erase(inner_node, transformed_key);

            return __result;
        }

    private:
        /**
         * @brief Shrink node if necessary and compress one-way node into node below.
         * @param node  Pointer to node where erasure took place.
         * @param transformed_key  Key that was erased.
         */
        void fix_after_erase(Node_ptr node, const Key &transformed_key) {
            if (node->size() < node->min_size()) {
                pair<Node_ptr, bool> p = shrink(node);

                node = p.first;
                update_child_ptr(node->_parent, node, transformed_key);
            }

            if (node->get_type() == node_type::node_4_t && node->size() == 1) {
                node = compress_node_into_child(static_cast<_Node_4 *>(node));
                update_child_ptr(node->_parent, node, transformed_key);
            }
        }

    public:
        void swap(ar_prefix_tree &__x) {
            std::swap(_M_root, __x._M_root);
            std::swap(_M_count, __x._M_count);
            std::swap(_M_dummy_node, __x._M_dummy_node);
            std::swap(_M_key_transform, __x._M_key_transform);
        }

        ////////////
        // Lookup //
        ////////////

        iterator find(const key_type &__k) {
            Key transformed_key = {_M_key_transform(__k)};
            const auto key_size = sizeof(transformed_key);

            if (_M_root == nullptr)
                return end();

            Node_ptr current_node = _M_root;
            for (unsigned depth = 0; depth < key_size + 1; depth++) {
                if (current_node == nullptr)
                    return end();

                if (current_node->is_leaf()) {
                    Leaf_ptr leaf = static_cast<Leaf_ptr>(current_node);
                    Key existing_key = {_M_key_transform(_KeyOfValue()(leaf->_value))};
                    if (!std::memcmp(&transformed_key, &existing_key, sizeof(transformed_key)))
                        return iterator(leaf);
                    return end();
                }

                auto current_inner = static_cast<Inner_Node_ptr>(current_node);
                auto pos = current_inner->pes_prefix_mismatch_pos(transformed_key);
                if (pos < current_inner->_prefix_length)
                    return end();

                depth += pos;
                current_node = current_node->find(transformed_key.chunks[depth]);
            }
            throw; // unreachable
        }

        const_iterator find(const key_type &__k) const {
            Key transformed_key = {_M_key_transform(__k)};
            const auto key_size = sizeof(transformed_key);

            if (_M_root == nullptr)
                return end();

            Const_Node_ptr current_node = _M_root;
            for (unsigned depth = 0; depth < key_size + 1; depth++) {
                if (current_node == nullptr)
                    return end();

                if (current_node->is_leaf()) {
                    Const_Leaf_ptr leaf = static_cast<Const_Leaf_ptr>(current_node);
                    Key existing_key = {_M_key_transform(_KeyOfValue()(leaf->_value))};
                    if (!std::memcmp(&transformed_key, &existing_key, sizeof(transformed_key)))
                        return const_iterator(leaf);
                    return end();
                }

                auto current_inner = static_cast<Const_Inner_Node_ptr>(current_node);
                auto pos = current_inner->pes_prefix_mismatch_pos(transformed_key);
                if (pos < current_inner->_prefix_length)
                    return end();

                depth += pos;
                current_node = current_node->find(transformed_key.chunks[depth]);
            }
            throw; // unreachable
        }

        iterator lower_bound(const key_type &__k) {
            Key transformed_key = {_M_key_transform(__k)};
            const auto key_size = sizeof(transformed_key);

            if (_M_root == nullptr)
                return end();

            Node_ptr previous_node = nullptr;
            Node_ptr current_node = _M_root;
            for (unsigned depth = 0; depth < key_size + 1; depth++) {
                if (current_node == nullptr) {
                    auto successor = previous_node->successor(transformed_key);
                    return iterator(successor);
                }

                if (current_node->is_leaf()) {
                    Leaf_ptr leaf = static_cast<Leaf_ptr>(current_node);
                    Key existing_key = {_M_key_transform(_KeyOfValue()(leaf->_value))};

                    if (std::memcmp(&transformed_key, &existing_key, sizeof(transformed_key)) <= 0)
                        return iterator(leaf);

                    auto successor = previous_node->successor(transformed_key);
                    return iterator(successor);
                }

                auto current_inner = static_cast<Inner_Node_ptr>(current_node);
                auto pos = current_inner->prefix_mismatch_pos(transformed_key);
                if (pos < current_inner->_prefix_length)
                    return end();

                depth += pos;
                previous_node = current_node;
                current_node = current_node->find(transformed_key.chunks[depth]);
            }
            throw; // unreachable
        }

        const_iterator lower_bound(const key_type &__k) const {
            Key transformed_key = {_M_key_transform(__k)};
            const auto key_size = sizeof(transformed_key);

            if (_M_root == nullptr)
                return end();

            Node_ptr previous_node = nullptr;
            Node_ptr current_node = _M_root;
            for (unsigned depth = 0; depth < key_size + 1; depth++) {
                if (current_node == nullptr) {
                    auto successor = previous_node->successor(transformed_key);
                    return const_iterator(successor);
                }

                if (current_node->is_leaf()) {
                    Leaf_ptr leaf = static_cast<Leaf_ptr>(current_node);
                    Key existing_key = {_M_key_transform(_KeyOfValue()(leaf->_value))};

                    if (std::memcmp(&transformed_key, &existing_key, sizeof(transformed_key)) <= 0)
                        return const_iterator(leaf);

                    auto successor = previous_node->successor(transformed_key);
                    return const_iterator(successor);
                }
                auto current_inner = static_cast<Const_Inner_Node_ptr>(current_node);
                auto pos = current_inner->prefix_mismatch_pos(transformed_key);
                if (pos < current_inner->_prefix_length)
                    return end();

                depth += pos;
                previous_node = current_node;
                current_node = current_node->find(transformed_key.chunks[depth]);
            }
            throw; // unreachable
        }

        iterator upper_bound(const key_type &__k) {
            Key transformed_key{_M_key_transform(__k)};
            const auto key_size = sizeof(transformed_key);

            if (_M_root == nullptr)
                return end();

            Node_ptr previous_node = nullptr;
            Node_ptr current_node = _M_root;
            for (unsigned depth = 0; depth < key_size + 1; depth++) {
                if (current_node == nullptr) {
                    auto successor = previous_node->successor(transformed_key);
                    return iterator(successor);
                }

                if (current_node->is_leaf()) {
                    Leaf_ptr leaf = static_cast<Leaf_ptr>(current_node);
                    Key existing_key = {_M_key_transform(_KeyOfValue()(leaf->_value))};

                    if (std::memcmp(&transformed_key, &existing_key, sizeof(transformed_key)) < 0)
                        return iterator(leaf);

                    auto successor = previous_node->successor(transformed_key);
                    return iterator(successor);
                }
                auto current_inner = static_cast<Inner_Node_ptr>(current_node);
                auto pos = current_inner->prefix_mismatch_pos(transformed_key);
                if (pos < current_inner->_prefix_length)
                    return end();

                depth += pos;
                previous_node = current_node;
                current_node = current_node->find(transformed_key.chunks[depth]);
            }
            throw; // unreachable
        }

        const_iterator upper_bound(const key_type &__k) const {
            Key transformed_key = {_M_key_transform(__k)};
            const auto key_size = sizeof(transformed_key);

            if (_M_root == nullptr)
                return end();

            Node_ptr previous_node = nullptr;
            Node_ptr current_node = _M_root;
            for (unsigned depth = 0; depth < key_size + 1; depth++) {
                if (current_node == nullptr) {
                    auto successor = previous_node->successor(transformed_key);
                    return const_iterator(successor);
                }

                if (current_node->is_leaf()) {
                    Leaf_ptr leaf = static_cast<Leaf_ptr>(current_node);
                    Key existing_key = {_M_key_transform(_KeyOfValue()(leaf->_value))};

                    if (std::memcmp(&transformed_key, &existing_key, sizeof(transformed_key)) < 0)
                        return const_iterator(leaf);

                    auto successor = previous_node->successor(transformed_key);
                    return const_iterator(successor);
                }
                auto current_inner = static_cast<Const_Inner_Node_ptr>(current_node);
                auto pos = current_inner->prefix_mismatch_pos(transformed_key);
                if (pos < current_inner->_prefix_length)
                    return end();

                depth += pos;
                previous_node = current_node;
                current_node = current_node->find(transformed_key.chunks[depth]);
            }
            throw; // unreachable
        }

        pair<iterator, iterator> equal_range(const key_type &__k) {
            auto lower = lower_bound(__k);
            auto upper = upper_bound(__k);
            return pair<iterator, iterator>(lower, upper);
        };

        pair<const_iterator, const_iterator> equal_range(const key_type &__k) const {
            auto lower = lower_bound(__k);
            auto upper = upper_bound(__k);
            return pair<const_iterator, const_iterator>(lower, upper);
        };

        Base_Leaf_ptr minimum() {
            if (_M_root != nullptr)
                return _M_root->minimum();
            return nullptr;
        }

        Const_Base_Leaf_ptr minimum() const {
            if (_M_root != nullptr)
                return _M_root->minimum();
            return nullptr;
        }

        Base_Leaf_ptr maximum() {
            if (_M_root != nullptr)
                return _M_root->maximum();
            return nullptr;
        }

        Const_Base_Leaf_ptr maximum() const {
            if (_M_root != nullptr)
                return _M_root->maximum();
            return nullptr;
        }

        ~ar_prefix_tree() {
            clear();
            delete _M_dummy_node;
        }

    private:

        Node_ptr grow(Node_ptr old_node) {
            auto type = old_node->get_type();
            switch (type) {
                case node_type::node_4_t: {
                    _Node_4 *node4 = static_cast<_Node_4 *>(old_node);
                    Node_ptr node = new _Node_16(node4);
                    delete node4;
                    return node;
                }
                case node_type::node_16_t: {
                    _Node_16 *node16 = static_cast<_Node_16 *>(old_node);
                    Node_ptr node = new _Node_48(node16);
                    delete node16;
                    return node;
                }
                case node_type::node_48_t: {
                    _Node_48 *node48 = static_cast<_Node_48 *>(old_node);
                    Node_ptr node = new _Node_256(node48);
                    delete node48;
                    return node;
                }
                default:
                    throw;
            }
            throw; // unreachable
        }

        pair<Node_ptr, bool> shrink(Node_ptr old_node) {
            auto type = old_node->get_type();
            switch (type) {
                case node_type::node_4_t: {
                    _Node_4 *node4 = static_cast<_Node_4 *>(old_node);

                    if (node4->children[0]->is_leaf()) {
                        node4->children[0]->_parent = old_node->_parent;
                        Leaf_ptr child = static_cast<Leaf_ptr >(node4->children[0]);
                        delete node4;
                        return pair<Node_ptr, bool>(child, true);
                    }
                    return pair<Node_ptr, bool>(old_node, false);
                }
                case node_type::node_16_t: {
                    _Node_16 *node16 = static_cast<_Node_16 *>(old_node);
                    Node_ptr node = new _Node_4(node16);
                    delete node16;
                    return pair<Node_ptr, bool>(node, true);
                }
                case node_type::node_48_t: {
                    _Node_48 *node48 = static_cast<_Node_48 *>(old_node);
                    Node_ptr node = new _Node_16(node48);
                    delete node48;
                    return pair<Node_ptr, bool>(node, true);
                }
                case node_type::node_256_t: {
                    _Node_256 *node256 = static_cast<_Node_256 *>(old_node);
                    Node_ptr node = new _Node_48(node256);
                    delete node256;
                    return pair<Node_ptr, bool>(node, true);
                }
                default:
                    throw;
            }
            throw; // unreachable
        }
    };

    template<typename _Key, typename _Tp, typename _KeyOfValue, typename _Key_transform>
    const byte ar_prefix_tree<_Key, _Tp, _KeyOfValue, _Key_transform>::EMPTY_MARKER = 63;

    //////////////////////////
    // Relational Operators //
    //////////////////////////

    template<typename _Key, typename _Tp, typename _KeyOfValue, typename _Key_transform>
    inline bool
    operator==(const ar_prefix_tree<_Key, _Tp, _KeyOfValue, _Key_transform> &lhs,
               const ar_prefix_tree<_Key, _Tp, _KeyOfValue, _Key_transform> &rhs) {
        return lhs.size() == rhs.size()
               && std::equal(lhs.begin(), lhs.end(), rhs.begin());
    }

    // Based on operator==
    template<typename _Key, typename _Tp, typename _KeyOfValue, typename _Key_transform>
    inline bool
    operator!=(const ar_prefix_tree<_Key, _Tp, _KeyOfValue, _Key_transform> &lhs,
               const ar_prefix_tree<_Key, _Tp, _KeyOfValue, _Key_transform> &rhs) {
        return !(lhs == rhs);
    }

    template<typename _Key, typename _Tp, typename _KeyOfValue, typename _Key_transform>
    inline bool
    operator<(const ar_prefix_tree<_Key, _Tp, _KeyOfValue, _Key_transform> &__x,
              const ar_prefix_tree<_Key, _Tp, _KeyOfValue, _Key_transform> &__y) {
        return std::lexicographical_compare(__x.begin(), __x.end(),
                                            __y.begin(), __y.end());
    }

    // Based on operator<
    template<typename _Key, typename _Tp, typename _KeyOfValue, typename _Key_transform>
    inline bool
    operator>(const ar_prefix_tree<_Key, _Tp, _KeyOfValue, _Key_transform> &__x,
              const ar_prefix_tree<_Key, _Tp, _KeyOfValue, _Key_transform> &__y) {
        return __y < __x;
    }

    // Based on operator<
    template<typename _Key, typename _Tp, typename _KeyOfValue, typename _Key_transform>
    inline bool
    operator<=(const ar_prefix_tree<_Key, _Tp, _KeyOfValue, _Key_transform> &__x,
               const ar_prefix_tree<_Key, _Tp, _KeyOfValue, _Key_transform> &__y) {
        return !(__y < __x);
    }

    // Based on operator<
    template<typename _Key, typename _Tp, typename _KeyOfValue, typename _Key_transform>
    inline bool
    operator>=(const ar_prefix_tree<_Key, _Tp, _KeyOfValue, _Key_transform> &__x,
               const ar_prefix_tree<_Key, _Tp, _KeyOfValue, _Key_transform> &__y) {
        return !(__x < __y);
    }

}


#endif //ART_AR_PREFIX_TREE_H
