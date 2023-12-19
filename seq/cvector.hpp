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

#ifndef SEQ_CVECTOR_HPP
#define SEQ_CVECTOR_HPP

/** @file */



namespace seq
{
	namespace detail
	{
		// forward declaration
		template<class Compress>
		class ValueWrapper;
	}
}
namespace std
{
	///////////////////////////
	// Completely illegal overload of std::move.
	// That's currently the only way I found to use generic algorithms (like std::move(It, It, Dst) ) with cvector.
	///////////////////////////

	template<class Compress>
	typename Compress::value_type move(seq::detail::ValueWrapper<Compress>& other);

	template<class Compress>
	typename Compress::value_type move(seq::detail::ValueWrapper<Compress>&& other);

	template<class Compressed>
	void swap(seq::detail::ValueWrapper<Compressed>&& a, seq::detail::ValueWrapper<Compressed>&& b);
}


#include <algorithm>
#include <vector>
#include <atomic>
#include <chrono>
#include <limits>

#include "internal/block_codec.h"
#include "internal/transpose.h"
#include "memory.hpp"
#include "bits.hpp"
#include "utils.hpp"
#include "tiny_string.hpp"

#ifdef __SSE4_1__

namespace seq
{
	

	/// @brief Basic lock guard class
	template<class Lock>
	class lock_guard
	{
		Lock* lock;
	public:
		lock_guard(Lock& l) noexcept : lock(&l) { lock->lock(); }
		lock_guard(const lock_guard&) noexcept = default;
		~lock_guard() noexcept { lock->unlock(); }
	};

	/// @brief Returns a lock guard around l.
	/// Use copy elison to avoid creating copies and locking twice the lock object.
	template<class Lock>
	auto make_lock_guard(Lock& l) noexcept -> lock_guard<Lock>
	{
		return lock_guard<Lock>(l);
	}


	/// @brief Enum type used by context_ratio
	enum ContextRatio
	{
		Fixed, //! Fixed number of decompression context
		Ratio //! Ratio of total number of chunks
	};

	/// @brief Define the maximum number of decompression contexts a cvector can use.
	/// The number of contexts is either a fixed value or a ratio of the cvector bucket count.
	/// Use ContextRatio::Fixed to define a fixed number of contexts, or ContextRatio::Ratio to define a fraction of the bucket count.
	/// In this case (ratio), the number of contexts is equal to bucket_count/(float)ratio_value.
	class context_ratio
	{
		unsigned d_ratio : sizeof(unsigned) * 8 - 1;
		unsigned d_type : 1;
	public:
		context_ratio() noexcept
			:d_ratio(8), d_type(Ratio) {} // 12.5% by default
		context_ratio(unsigned ratio_or_count, ContextRatio type = Fixed) noexcept
			:d_ratio(ratio_or_count == 0 ? 1 : ratio_or_count), d_type(type) {}

		unsigned ratio() const noexcept { return d_ratio; }
		ContextRatio type() const noexcept { return static_cast<ContextRatio>(d_type); }
		size_t context_count(size_t bucket_count) const noexcept {
			return d_type == 0 ? static_cast<size_t>(d_ratio) : static_cast<size_t>(static_cast<float>(bucket_count) / static_cast<float>(d_ratio));
		}
	};




	namespace detail
	{
		/// @brief Change the maximum number of contexts for a cvector object, and restore the previous value on destruction.
		template<class Compress>
		struct ContextRatioGuard
		{
			Compress* compress;
			context_ratio old_ratio;
			ContextRatioGuard(Compress* c, context_ratio new_ratio) noexcept : compress(c), old_ratio(c->max_contexts()) {
				compress->set_max_contexts(new_ratio);
			}
			ContextRatioGuard(const ContextRatioGuard&) noexcept = default;
			~ContextRatioGuard() noexcept {
				compress->set_max_contexts(old_ratio);
			}
		};
		template<class Compress>
		ContextRatioGuard< Compress> lock_context_ratio(Compress* c, context_ratio new_ratio) noexcept {
			return ContextRatioGuard< Compress>(c, new_ratio);
		}

		/// @brief Base class for RawBuffer to provide intrusive list features
		struct Iterator
		{
			Iterator* left;
			Iterator* right;

			void erase() noexcept
			{
				// remove from linked list
				left->right = right;
				right->left = left;
			}
			void insert(Iterator* _left, Iterator* _right) noexcept
			{
				// insert in linked list
				this->left = _left;
				this->right = _right;
				_left->right = this;
				_right->left = this;
			}
		};

		/// @brief Raw buffer used to compress/decompress blocks
		template<class T, unsigned block_size>
		struct RawBuffer : public Iterator
		{
			static constexpr size_t alignment = (alignof(T) > 16 ? alignof(T) : 16);
			static constexpr size_t storage_size = block_size * sizeof(T);//BlockBound<T>::value;
			static constexpr size_t invalid_index = std::numeric_limits<size_t>::max();//(1ULL << (sizeof(size_t) * 8 - 10)) - 1ULL;

			alignas((alignof(T) > 16 ? alignof(T) : 16)) char storage[storage_size]; //data storage, aligned on 16 bytes at least
			unsigned short		size;				// size is used for front and back buffer
			unsigned short		dirty;				// dirty (modified) buffer
			size_t				block_index;		// block index in list of blocks

			void mark_dirty() noexcept
			{
				// mark as dirty
				dirty = 1;
			}

			template<class CompressVec>
			void mark_dirty(CompressVec* vec) noexcept
			{
				// mark as dirty and release related compressed memory (if any)
				dirty = 1;
				//release memory
				if (SEQ_LIKELY(block_index != invalid_index)) {
					vec->dealloc_bucket(block_index);
				}
			}

			void clear_values() noexcept
			{
				// Destroy values
				if (!std::is_trivially_destructible<T>::value)
				{
					for (unsigned i = 0; i < size; ++i) {
						destroy_ptr(&at(i));
					}
				}
				// reset
				dirty = 0;
				size = 0;
			}

			void reset() noexcept
			{
				size = 0;
				dirty = 0;
				block_index = invalid_index;
			}

			auto data() noexcept -> T* { return reinterpret_cast<T*>(storage); }
			auto data() const noexcept -> const T* { return reinterpret_cast<const T*>(storage); }
			template<class U>
			auto at(U index) noexcept -> T& { return data()[index]; }
			template<class U>
			auto at(U index) const noexcept -> const T& { return data()[index]; }
		};

		/// @brief Intrusive linked list of RawBuffer
		template<class Buffer>
		struct BufferList
		{
			struct iterator
			{
				Iterator* it;
				iterator(Iterator* i = nullptr) noexcept : it(i) {}
				auto operator++() noexcept -> iterator&
				{
					it = it->right;
					return *this;
				}
				auto operator++(int) noexcept -> iterator
				{
					iterator _Tmp = *this;
					++(*this);
					return _Tmp;
				}
				auto operator--() noexcept -> iterator&
				{
					it = it->left;
					return *this;
				}
				auto operator--(int) noexcept -> iterator
				{
					iterator _Tmp = *this;
					--(*this);
					return _Tmp;
				}
				auto operator*() const noexcept -> Buffer* { return static_cast<Buffer*>(it); }
				bool operator==(const iterator& other) const noexcept { return it == other.it; }
				bool operator!=(const iterator& other) const noexcept { return it != other.it; }
			};

			Iterator d_end;
			size_t d_size;

			BufferList() noexcept
				:d_size(0) {
				d_end.left = d_end.right = &d_end;
			}
			auto size() const noexcept ->size_t { return d_size; }
			auto begin() noexcept -> iterator { return iterator(d_end.right); }
			auto end() noexcept -> iterator { return iterator(&d_end); }
			void assign(BufferList&& other) noexcept
			{
				auto l = other.d_end.left;
				auto r = other.d_end.right;
				d_end = other.d_end;
				d_size = other.d_size;
				if (d_size) {
					l->right = &d_end;
					r->left = &d_end;
				}
				else
					d_end.left = d_end.right = &d_end;
			}
			void clear() noexcept
			{
				d_size = 0;
				d_end.left = d_end.right = &d_end;
			}
			void push_back(Buffer* b) noexcept
			{
				++d_size;
				b->insert(d_end.left, &d_end);
			}
			void push_front(Buffer* b) noexcept
			{
				++d_size;
				b->insert(&d_end, d_end.right);
			}
			void pop_back() noexcept
			{
				--d_size;
				back()->erase();
			}
			void pop_front()noexcept
			{
				--d_size;
				front()->erase();
			}
			void erase(Buffer* b) noexcept
			{
				--d_size;
				b->erase();
			}
			void erase(iterator it) noexcept
			{
				erase(*it);
			}
			auto back() noexcept  -> Buffer*
			{
				return static_cast<Buffer*>(d_end.left);
			}
			auto front() noexcept -> Buffer*
			{
				return static_cast<Buffer*>(d_end.right);
			}
		};

		/// @brief Check if given code is a compression/decompression error code
		static inline bool has_error(unsigned code)
		{
			return code >= SEQ_LAST_ERROR_CODE;
		}


		/// @brief Compressed buffer class
		template<class T, class Encoder, unsigned block_size>
		struct PackBuffer
		{
			// Pointer to decompressed buffer, if any
			RawBuffer<T,block_size>* decompressed;
			// Compressed data
			char* buffer;
			// Internal lock for multithreaded contexts
			spinlock lock;
			// Compressed size
			unsigned csize;

			PackBuffer(RawBuffer<T,block_size>* dec = nullptr, char* buff = nullptr, unsigned _csize = 0) noexcept
				: decompressed(dec), buffer(buff), csize(_csize) {}

			// Move semantic for usage inside std::vector
			PackBuffer(PackBuffer&& other) noexcept
				:decompressed(other.decompressed), buffer(other.buffer), csize(other.csize)
			{
				other.decompressed = nullptr;
				other.buffer = nullptr;
				other.csize = 0;
			}
			PackBuffer& operator=(PackBuffer&& other) noexcept
			{
				std::swap(decompressed, other.decompressed);
				std::swap(buffer, other.buffer);
				std::swap(csize, other.csize);
				return *this;
			}

			// decompress block into dst. returns read size from buffer
			unsigned decompress(char* dst)
			{
				if (csize == 0)
					return 0;
				else if (csize == block_size * sizeof(T)) {
					memcpy(dst, buffer, block_size * sizeof(T));
					return block_size * sizeof(T);
				}
				else {
					return Encoder::decompress(buffer, csize, sizeof(T), block_size, dst);
					//return block_decode_256(buffer, csize, sizeof(T), 1, dst);
				}
			}
		};

		///@brief Create a raw buffer aligned on 16 bytes
		template<class T,unsigned block_size, class Alloc>
		static auto make_raw_buffer(Alloc al) -> RawBuffer<T, block_size>*
		{
			RawBuffer<T, block_size>* res = reinterpret_cast<RawBuffer<T, block_size>*>(al.allocate(sizeof(RawBuffer<T, block_size>)));
			res->size = 0;
			res->dirty = 0;
			res->block_index = 0;
#ifndef NDEBUG
			res->left = res->right = nullptr;
#endif
			return res;
		}

		template<class T, unsigned block_size>
		static auto get_raw_buffer() noexcept -> RawBuffer<T, block_size>*
		{
			// Returns a RawBuffer suitable to destroy a compressed buffer.
			// Wont be used in most cases.
			static thread_local RawBuffer<T, block_size> buff;
			return &buff;
		}


		///@brief Destroy and deallocate a pack buffer.
		/// Also destroy and deallocate the uncompressed data.
		template<class T, class Enc, unsigned block_size, class Alloc>
		static void destroy_pack_buffer(PackBuffer<T,Enc, block_size>* pack, RawBuffer<T, block_size>* tmp, Alloc al)
		{
			// Here, tmp is just used to decompress and destroy values
			if (pack && pack->buffer) {
				if (!std::is_trivially_destructible<T>::value) {
					if (pack->decompressed) {
						// destroy all values in  decompressed
						pack->decompressed->clear_values();
					}
					else {
						//we must decompress first
						unsigned r = pack->decompress(tmp->storage);
						if (has_error(r))
							SEQ_ABORT("cvector: abort on decompression error");//
						// destroy
						tmp->size = block_size;
						tmp->clear_values();
					}
				}
				al.deallocate(pack->buffer, pack->csize);
			}
		}

		/// @brief Write unsigned integer to stream in a compressed way
		template<class T>
		static inline void write_integer(std::ostream& oss, T r)
		{
			static_assert(std::is_integral<T>::value && !std::is_signed<T>::value, "write_integer only works on unsigned integral types");
			while (r > 127) {
				oss.put(static_cast<char>((r & 127) | 128)); 
				r >>= 7;
			}
			oss.put(static_cast<char>(r));
		}

		/// @brief Read unsigned integer from stream
		template<class T>
		static inline T read_integer(std::istream& iss)
		{
			static_assert(std::is_integral<T>::value && !std::is_signed<T>::value, "read_integer only works on unsigned integral types");
			T shift = 0;
			T r = 0;
			char src = static_cast<char>(iss.get());

			while (src & 128) {
				r = (static_cast<T>((src) & 127) << shift) | r;
				shift += 7; 
				src = static_cast<char>(iss.get());
			}
			r = (static_cast<T>((src) & 127) << shift) | r;
			return r;
		}



		// Forward declarations

		template< class U>
		struct CompressedConstIter;

		template< class U>
		struct CompressedIter;

		template<class Compressed>
		class ValueWrapper;


		/// @brief Const value wrapper class for cvector and cvector::iterator
		template<class Compressed>
		class ConstValueWrapper
		{
			friend struct CompressedConstIter<Compressed>;
			friend struct CompressedIter<Compressed>;
			friend class ValueWrapper<Compressed>;

		protected:

			using BucketType = typename Compressed::BucketType;

			Compressed* c;
			size_t bucket;
			unsigned short bpos;

			auto _bucket() const noexcept -> const BucketType* { return &_c()->d_buckets[bucket]; }
			auto _bucket() noexcept -> BucketType* { return &_c()->d_buckets[bucket]; }
			auto _c() const noexcept -> Compressed* { return const_cast<Compressed*>(c); }

			SEQ_ALWAYS_INLINE void decompress_if_needed(size_t exclude = static_cast<size_t>(-1)) const
			{
				if (!this->_bucket()->decompressed) {
					_c()->decompress_bucket(bucket, exclude);
					_c()->incr_disp();
				}
			}

		public:
			using value_type = typename Compressed::value_type;
			using T = value_type;
			using reference = const T&;
			using const_reference = const T&;

			ConstValueWrapper() noexcept : c(nullptr), bucket(0), bpos(0) {}
			ConstValueWrapper(const Compressed* _c, size_t b, unsigned short pos) noexcept
				:c(const_cast<Compressed*>(_c)), bucket(b), bpos(pos) {}
			ConstValueWrapper(const ConstValueWrapper& other) noexcept
				:c(other.c), bucket(other.bucket), bpos(other.bpos) {}

			auto bucket_index() const noexcept -> size_t { return bucket; }
			auto bucket_pos() const noexcept -> unsigned short { return bpos; }
			auto vector_data() const noexcept -> const void* { return c; }

			void setPos(const Compressed* c, size_t bucket, unsigned short bpos) noexcept
			{
				c = (const_cast<Compressed*>(c));
				bucket = (bucket);
				bpos = (bpos);
			}
			void setPos(const ConstValueWrapper& other) noexcept
			{
				c = other.c;
				bucket = other.bucket;
				bpos = other.bpos;
			}
			auto get() const -> const T&
			{
				this->decompress_if_needed();
				return _bucket()->decompressed->at(bpos);
			}

			operator const T& () const { return get(); }

			template<class Fun>
			auto compare(const ConstValueWrapper& other, const Fun& fun) const -> decltype(fun(*this, other))
			{
				this->decompress_if_needed(other.bucket);
				other.decompress_if_needed(this->bucket);
				return fun(this->_bucket()->decompressed->at(this->bpos), other._bucket()->decompressed->at(other.bpos));
			}

		};


		template<class Wrapper, class T = typename Wrapper::value_type, bool IsMoveOnly = (!std::is_copy_constructible<T>::value&& std::is_move_constructible<T>::value)>
		struct ConversionWrapper
		{
			using type = T&&;
			static T&& move(Wrapper& w) { return std::move(w.move()); }
		};
		template<class Wrapper, class T >
		struct ConversionWrapper<Wrapper, T, false>
		{
			using type = const T&;
			static const T& move(Wrapper& w) { return w.get(); }
		};


		template<class Compressed>
		class ValueWrapper : public ConstValueWrapper<Compressed>
		{
			using conv = ConversionWrapper<ValueWrapper<Compressed>, typename Compressed::value_type >;
			using conv_type = typename conv::type;

		public:


			using T = typename ConstValueWrapper<Compressed>::T;
			using base_type = ConstValueWrapper<Compressed>;
			using BucketType = typename base_type::BucketType;
			using value_type = T;
			using reference = T&;
			using const_reference = const T&;
			using base_type::get;

			ValueWrapper() noexcept : base_type() {}
			ValueWrapper(const Compressed* _c, size_t b, unsigned short pos) noexcept
				:base_type(_c, b, pos) {}
			ValueWrapper(const ValueWrapper& other) noexcept
				:base_type(other) {}
			ValueWrapper(const base_type& other) noexcept
				:base_type(other) {}


			auto move() -> T&&
			{
				this->decompress_if_needed();
				if (!std::is_trivially_move_assignable<T>::value)
					this->_bucket()->decompressed->mark_dirty(this->_c());
				return std::move(this->_bucket()->decompressed->at(this->bpos));
			}

			template<class = typename std::enable_if<std::is_move_assignable<T>::value, void>::type>
			void set(const T& obj)
			{
				this->decompress_if_needed();
				this->_bucket()->decompressed->at(this->bpos) = obj;
				this->_bucket()->decompressed->mark_dirty(this->_c());
				this->_c()->decr_disp();
			}
			void set(T&& obj)
			{
				this->decompress_if_needed();
				this->_bucket()->decompressed->at(this->bpos) = std::move(obj);
				this->_bucket()->decompressed->mark_dirty(this->_c());
				this->_c()->decr_disp();
			}
			operator const T& () const { return this->get(); }
			operator conv_type() { return conv::move(*this); }

			auto operator=(const base_type& other) -> ValueWrapper&
			{
				if (SEQ_LIKELY(std::addressof(other) != this)) {
					this->decompress_if_needed(other.bucket);
					other.decompress_if_needed(this->bucket);
					this->_bucket()->decompressed->mark_dirty(this->_c());
					this->_bucket()->decompressed->at(this->bpos) = other._bucket()->decompressed->at(other.bpos);
					this->_c()->decr_disp();
				}
				return *this;
			}
			auto operator=(ValueWrapper && other) -> ValueWrapper&
			{
				if (SEQ_LIKELY(std::addressof(other) != this)) {
					if (!this->_bucket()->decompressed)
						this->_c()->decompress_bucket(this->bucket, other.bucket);
					if (!other._bucket()->decompressed)
						other._c()->decompress_bucket(other.bucket, this->bucket);
					if (!std::is_trivially_move_assignable<T>::value)
						other._bucket()->decompressed->mark_dirty(other._c());
					this->_bucket()->decompressed->mark_dirty(this->_c());
					this->_bucket()->decompressed->at(this->bpos) = std::move(other._bucket()->decompressed->at(other.bpos));
					this->_c()->decr_disp();
				}
				return *this;
			}
			auto operator=(const T& other) -> ValueWrapper&
			{
				set(other);
				return *this;
			}
			auto operator=(T&& other) -> ValueWrapper&
			{
				set(std::move(other));
				return *this;
			}
			void swap(ValueWrapper& other)
			{
				if (SEQ_LIKELY(std::addressof(other) != this)) {
					this->decompress_if_needed(other.bucket);
					other.decompress_if_needed(this->bucket);
					this->_bucket()->decompressed->mark_dirty(this->_c());
					other._bucket()->decompressed->mark_dirty(other._c());
					std::swap(this->_bucket()->decompressed->at(this->bpos), other._bucket()->decompressed->at(other.bpos));
					this->_c()->decr_disp();
					other._c()->decr_disp();
				}
			}
		};



		// Overload comparison operators for ConstValueWrapper to make it work with most stl algorithms

		template<class Compressed>
		bool operator<(const ConstValueWrapper<Compressed>& a, const ConstValueWrapper<Compressed>& b)
		{
			using T = typename Compressed::value_type;
			return a.compare(b, [](const T& _a, const T& _b) {return _a < _b; });
		}
		template<class Compressed>
		bool operator<(const ConstValueWrapper<Compressed>& a, const typename Compressed::value_type& b)
		{
			return a.get() < b;
		}
		template<class Compressed>
		bool operator<(const typename Compressed::value_type& a, const ConstValueWrapper<Compressed>& b)
		{
			return a < b.get();
		}

		template<class Compressed>
		bool operator>(const ConstValueWrapper<Compressed>& a, const ConstValueWrapper<Compressed>& b)
		{
			using T = typename Compressed::value_type;
			return a.compare(b, [](const T& _a, const T& _b) {return _a > _b; });
		}
		template<class Compressed>
		bool operator>(const ConstValueWrapper<Compressed>& a, const typename Compressed::value_type& b)
		{
			return a.get() > b;
		}
		template<class Compressed>
		bool operator>(const typename Compressed::value_type& a, const ConstValueWrapper<Compressed>& b)
		{
			return a > b.get();
		}

		template<class Compressed>
		bool operator<=(const ConstValueWrapper<Compressed>& a, const ConstValueWrapper<Compressed>& b)
		{
			using T = typename Compressed::value_type;
			return a.compare(b, [](const T& _a, const T& _b) {return _a <= _b; });
		}
		template<class Compressed>
		bool operator<=(const ConstValueWrapper<Compressed>& a, const typename Compressed::value_type& b)
		{
			return a.get() <= b;
		}
		template<class Compressed>
		bool operator<=(const typename Compressed::value_type& a, const ConstValueWrapper<Compressed>& b)
		{
			return a <= b.get();
		}

		template<class Compressed>
		bool operator>=(const ConstValueWrapper<Compressed>& a, const ConstValueWrapper<Compressed>& b)
		{
			using T = typename Compressed::value_type;
			return a.compare(b, [](const T& _a, const T& _b) {return _a >= _b; });
		}
		template<class Compressed>
		bool operator>=(const ConstValueWrapper<Compressed>& a, const typename Compressed::value_type& b)
		{
			return a.get() >= b;
		}
		template<class Compressed>
		bool operator>=(const typename Compressed::value_type& a, const ConstValueWrapper<Compressed>& b)
		{
			return a >= b.get();
		}

		template<class Compressed>
		bool operator==(const ConstValueWrapper<Compressed>& a, const ConstValueWrapper<Compressed>& b)
		{
			using T = typename Compressed::value_type;
			return a.compare(b, [](const T& _a, const T& _b) {return _a == _b; });
		}
		template<class Compressed>
		bool operator==(const ConstValueWrapper<Compressed>& a, const typename Compressed::value_type& b)
		{
			return a.get() == b;
		}
		template<class Compressed>
		bool operator==(const typename Compressed::value_type& a, const ConstValueWrapper<Compressed>& b)
		{
			return a == b.get();
		}

		template<class Compressed>
		bool operator!=(const ConstValueWrapper<Compressed>& a, const ConstValueWrapper<Compressed>& b)
		{
			using T = typename Compressed::value_type;
			return a.compare(b, [](const T& _a, const T& _b) {return _a != _b; });
		}
		template<class Compressed>
		bool operator!=(const ConstValueWrapper<Compressed>& a, const typename Compressed::value_type& b)
		{
			return a.get() != b;
		}
		template<class Compressed>
		bool operator!=(const typename Compressed::value_type& a, const ConstValueWrapper<Compressed>& b)
		{
			return a != b.get();
		}



		/// @brief const iterator type for cvector
		template<class Compressed>
		struct CompressedConstIter
		{
			using T = typename Compressed::value_type;
			using value_type = T;
			using reference = const T&;
			using const_reference = const T&;
			using pointer = const T*;
			using const_pointer = const T*;
			using difference_type = typename Compressed::difference_type;
			using iterator_category = std::random_access_iterator_tag;
			using size_type = size_t;
			static const size_t elems_per_block = Compressed::elems_per_block;
			static const size_t mask = Compressed::mask;
			static const size_t shift = Compressed::shift;

			Compressed* data;
			difference_type abspos;


			CompressedConstIter() noexcept : abspos(0) {}
			CompressedConstIter(const Compressed* c, difference_type p) noexcept : data(const_cast<Compressed*>(c)), abspos(p) {  } //begin
			CompressedConstIter(const Compressed* c) noexcept : data(const_cast<Compressed*>(c)), abspos(static_cast<difference_type>(c->d_size)) {} //end()

			auto operator++() noexcept -> CompressedConstIter& {
				++abspos;
				return *this;
			}
			auto operator++(int) noexcept -> CompressedConstIter {
				CompressedConstIter it = *this;
				++(*this);
				return it;
			}
			auto operator--() noexcept -> CompressedConstIter& {
				--abspos;
				return *this;
			}
			auto operator--(int) noexcept -> CompressedConstIter {
				CompressedConstIter it = *this;
				--(*this);
				return it;
			}

			auto operator*() noexcept -> ConstValueWrapper<Compressed> {
				SEQ_ASSERT_DEBUG(this->abspos >= 0 && this->abspos < static_cast<difference_type>(this->data->size()), "attempt to dereference an invalid iterator");
				return data->at(static_cast<size_t>(abspos));
			}
			auto operator*() const noexcept -> ConstValueWrapper<Compressed> {
				SEQ_ASSERT_DEBUG(this->abspos >= 0 && this->abspos < static_cast<difference_type>(this->data->size()), "attempt to dereference an invalid iterator");
				return data->at(static_cast<size_t>(abspos));
			}
			auto operator->() const noexcept -> const T* {
				SEQ_ASSERT_DEBUG(this->abspos >= 0 && this->abspos < static_cast<difference_type>(this->data->size()), "attempt to dereference an invalid iterator");
				return &data->at(static_cast<size_t>(abspos)).get();
			}
			auto operator+=(difference_type diff) noexcept -> CompressedConstIter& {
				this->abspos += diff;
				return *this;
			}
			auto operator-=(difference_type diff) noexcept -> CompressedConstIter& {
				(*this) += -diff;
				return *this;
			}
			auto operator+(difference_type diff) const noexcept -> CompressedConstIter {
				CompressedConstIter tmp = *this;
				tmp += diff;
				return tmp;
			}
			auto operator-(difference_type diff) const noexcept -> CompressedConstIter {
				CompressedConstIter tmp = *this;
				tmp -= diff;
				return tmp;
			}
		};

		/// @brief iterator type for cvector
		template<class Compressed >
		struct CompressedIter : public CompressedConstIter< Compressed>
		{
			using base_type = CompressedConstIter< Compressed>;
			using value_type = typename Compressed::value_type;
			using reference = typename Compressed::reference;
			using const_reference = typename Compressed::const_reference;
			using pointer = typename Compressed::pointer;
			using const_pointer = typename Compressed::const_pointer;

			using difference_type = typename Compressed::difference_type;
			using iterator_category = std::random_access_iterator_tag;
			using size_type = size_t;

			CompressedIter() noexcept :base_type() {}
			CompressedIter(const base_type& other) noexcept : base_type(other) {}
			CompressedIter(const Compressed* c, difference_type p) noexcept : base_type(c, p) {} //begin
			CompressedIter(const Compressed* c) noexcept : base_type(c) {} //end

			auto operator*()  noexcept -> ValueWrapper<Compressed> {
				SEQ_ASSERT_DEBUG(this->abspos >= 0 && this->abspos < static_cast<difference_type>(this->data->size()), "attempt to dereference an invalid iterator");
				return this->data->at(static_cast<size_t>(this->abspos));
			}
			auto operator->()  noexcept -> value_type* {
				SEQ_ASSERT_DEBUG(this->abspos >= 0 && this->abspos < static_cast<difference_type>(this->data->size()), "attempt to dereference an invalid iterator");
				return (&this->data->at(static_cast<size_t>(this->abspos)).get());
			}
			auto operator*() const noexcept -> ConstValueWrapper<Compressed> {
				SEQ_ASSERT_DEBUG(this->abspos >= 0 && this->abspos < static_cast<difference_type>(this->data->size()), "attempt to dereference an invalid iterator");
				return this->data->at(static_cast<size_t>(this->abspos));
			}
			auto operator->() const noexcept -> const value_type* {
				SEQ_ASSERT_DEBUG(this->abspos >= 0 && this->abspos < static_cast<difference_type>(this->data->size()), "attempt to dereference an invalid iterator");
				return &this->data->at(static_cast<size_t>(this->abspos)).get();
			}
			auto operator++() noexcept -> CompressedIter& {
				base_type::operator++();
				return *this;
			}
			auto operator++(int) noexcept -> CompressedIter {
				CompressedIter _Tmp = *this;
				base_type::operator++();
				return _Tmp;
			}
			auto operator--() noexcept -> CompressedIter& {
				base_type::operator--();
				return *this;
			}
			auto operator--(int) noexcept -> CompressedIter {
				CompressedIter _Tmp = *this;
				base_type::operator--();
				return _Tmp;
			}
			auto operator+=(difference_type diff) noexcept -> CompressedIter& {
				base_type::operator+=(diff);
				return *this;
			}
			auto operator-=(difference_type diff) noexcept -> CompressedIter& {
				base_type::operator-=(diff);
				return *this;
			}
			auto operator+(difference_type diff) const noexcept -> CompressedIter {
				CompressedIter tmp = *this;
				tmp += diff;
				return tmp;
			}
			auto operator-(difference_type diff) const noexcept -> CompressedIter {
				CompressedIter tmp = *this;
				tmp -= diff;
				return tmp;
			}
		};

		template<class C>
		auto operator-(const CompressedConstIter<C>& a, const CompressedConstIter<C>& b)  noexcept -> typename CompressedConstIter<C>::difference_type {
			SEQ_ASSERT_DEBUG(a.data == b.data, "comparing iterators from different containers");
			return a.abspos - b.abspos;
		}

		template<class C>
		bool operator==(const CompressedConstIter<C>& a, const CompressedConstIter<C>& b)  noexcept {
			SEQ_ASSERT_DEBUG(a.data == b.data, "comparing iterators from different containers");
			return a.abspos == b.abspos;
		}
		template<class C>
		bool operator!=(const CompressedConstIter<C>& a, const CompressedConstIter<C>& b)  noexcept {
			SEQ_ASSERT_DEBUG(a.data == b.data, "comparing iterators from different containers");
			return a.abspos != b.abspos;
		}
		template<class C>
		bool operator<(const CompressedConstIter<C>& a, const CompressedConstIter<C>& b)  noexcept {
			SEQ_ASSERT_DEBUG(a.data == b.data, "comparing iterators from different containers");
			return a.abspos < b.abspos;
		}
		template<class C>
		bool operator>(const CompressedConstIter<C>& a, const CompressedConstIter<C>& b)  noexcept {
			SEQ_ASSERT_DEBUG(a.data == b.data, "comparing iterators from different containers");
			return a.abspos > b.abspos;
		}
		template<class C>
		bool operator<=(const CompressedConstIter<C>& a, const CompressedConstIter<C>& b)  noexcept {
			SEQ_ASSERT_DEBUG(a.data == b.data, "comparing iterators from different containers");
			return a.abspos <= b.abspos;
		}
		template<class C>
		bool operator>=(const CompressedConstIter<C>& a, const CompressedConstIter<C>& b)  noexcept {
			SEQ_ASSERT_DEBUG(a.data == b.data, "comparing iterators from different containers");
			return a.abspos >= b.abspos;
		}







		/// @brief Compress block inplace
		static inline auto debug_block_encode_256( void* __src, unsigned BPP, unsigned block_count, unsigned dst_size, unsigned level) noexcept -> unsigned
		{
			// Compress in-place __src
#ifdef NDEBUG
			unsigned ret = block_encode_256(__src, BPP, block_count, const_cast<void*>(__src), dst_size, level);
			if (ret == SEQ_ERROR_DST_OVERFLOW) return ret;
			if (has_error(ret))
				SEQ_ABORT("cvector: abort on compression error"); //cannot recover from this
			return ret;
#else
			void* __dst = (__src);
			std::vector<char> src(256 * BPP * block_count);
			std::vector<char> src2(256 * BPP * block_count);
			memcpy(src.data(), __src, src.size());

			std::vector<char> dst(256 * BPP * block_count * 2);
			unsigned ret = block_encode_256(__src, BPP, block_count, dst.data(), dst_size, level);
			if (ret == SEQ_ERROR_DST_OVERFLOW) return ret;
			assert(ret < SEQ_LAST_ERROR_CODE);

			//memcpy(__dst, dst.data(), ret);
			// remove stupid gcc warning 'forming offset [,] is out of the bounds'
			for (unsigned i = 0; i < ret; ++i)
				static_cast<char*>(__dst)[i] = dst[i];

			block_decode_256(dst.data(), ret, BPP, block_count, src2.data());

			assert(memcmp(src.data(), src2.data(), src.size()) == 0);
			return ret;
#endif
		}

		/// @brief Default encoder class used by cvector, relies on block compression
		struct DefaultEncoder
		{
			// inplace compression
			static unsigned compress(void* in_out, unsigned BPP, unsigned block_size, unsigned dst_size, unsigned acceleration)
			{
				(void)block_size;
				return debug_block_encode_256(in_out, BPP, 1, dst_size, acceleration);
			}
			// restore values in case of failed compression
			static void restore(void * in_out, void* dst, unsigned BPP, unsigned block_size)
			{
				(void)in_out;
				(void)block_size;
				transpose_inv_256_rows(static_cast<char*>(get_comp_buffer(0)) , static_cast<char*>(dst), BPP);
			}
			// decompress
			static unsigned decompress(const void* src, unsigned src_size, unsigned BPP, unsigned block_size, void* dst)
			{
				(void)block_size;
				return block_decode_256(src, src_size, BPP, 1, dst);
			}

		};

		/// @brief Encoder that relies on memcpy
		struct NullEncoder
		{
			// inplace compression
			static unsigned compress(void* , unsigned BPP, unsigned block_size, unsigned dst_size, unsigned )
			{
				unsigned s = block_size * BPP ;
				if (s > dst_size) return SEQ_ERROR_DST_OVERFLOW;
				return s;
			}
			// restore values in case of failed compression
			static void restore(void* in_out, void* dst, unsigned BPP, unsigned block_size)
			{
				memcpy(dst, in_out, BPP * block_size );
			}
			// decompress
			static unsigned decompress(const void* src, unsigned , unsigned BPP, unsigned block_size, void* dst)
			{
				memcpy(dst, src, block_size * BPP );
				return block_size * BPP ;
			}
		};


		/// @brief Internal structure used by cvector that gathers all the container logics
		///
		template<class T, class Allocator, unsigned Acceleration = 0, class Encoder = DefaultEncoder, unsigned block_size = 256>
		struct CompressedVectorInternal : private Allocator
		{
			static_assert(!(std::is_same<Encoder, DefaultEncoder>::value && (block_size != 256)), "DefaultEncoder only support a block size of 256");
			static_assert((block_size& (block_size - 1)) == 0, "block size must be a power of 2");

			using ThisType = CompressedVectorInternal<T, Allocator, Acceleration, Encoder, block_size>;

			using value_type = T;
			using allocator_type = Allocator;
			using reference = T&;
			using const_reference = const T&;
			using difference_type = typename std::allocator_traits<Allocator>::difference_type;
			using pointer = T*;
			using const_pointer = const T*;
			using iterator = CompressedIter< ThisType>;
			using const_iterator = CompressedConstIter< ThisType>;
			using ref_type = ValueWrapper< ThisType>;
			using const_ref_type = ConstValueWrapper< ThisType>;
			using BucketType = PackBuffer<T,Encoder, block_size>;
			using RawType = RawBuffer<T, block_size>;

			// Aligned allocator type, used to allocate RawBuffer object
			static constexpr size_t alignment = alignof(T) > 16 ? alignof(T) : 16;
			using AlignedAllocator = aligned_allocator<T, Allocator, alignment>;
			// Rebind allocator
			template< class U>
			using RebindAlloc = typename std::allocator_traits<Allocator>::template rebind_alloc<U>;
			// Rebind aligned allocator
			template< class U>
			using RebindAlignedAlloc = aligned_allocator<U, RebindAlloc<U>, alignment>;//typename std::allocator_traits<AlignedAllocator>::template rebind_alloc<U>;
			// List of decompression contexts
			using ContextType = BufferList<RawType>;

			// block size
			static constexpr size_t elems_per_block = block_size;
			// Mask for random access
			static constexpr size_t mask = elems_per_block - 1;
			// Shift for random access
			static constexpr size_t shift = static_bit_scan_reverse< block_size>::value;
			// Acceleration value, capped at 7
			static constexpr unsigned acceleration = Acceleration > 7 ? 7 : Acceleration;
			// Maximum allowed compressed size before storing the raw values
			static constexpr unsigned max_csize = block_size * sizeof(T) - 15 * sizeof(T) * acceleration - 1;


			std::vector<BucketType, RebindAlloc<BucketType> > d_buckets;	// compressed buckets
			ContextType d_contexts;				// decompression contexts
			size_t d_compress_size;				// compressed size in bytes
			size_t d_size;						// number of values
			context_ratio d_max_contexts;		// maximum number of contexts (fixed value or ratio of bucket count)
			spinlock d_lock;					// global spinlock
			std::atomic<short> d_disp;						// dispersion value, try to catch the current access pattern to release decompression contexts

		public:

			CompressedVectorInternal(const Allocator& al ) noexcept(std::is_nothrow_copy_constructible<Allocator>::value)
				:Allocator(al), d_buckets(RebindAlloc<BucketType>(al)), d_compress_size(0), d_size(0), d_max_contexts(8 - acceleration / 2, Ratio), d_disp(0)
			{}
			
			~CompressedVectorInternal()
			{
				clear();
			}

			/// @brief Increase dispersion value
			SEQ_ALWAYS_INLINE void incr_disp()noexcept {
				static constexpr short max = std::numeric_limits<short>::max() - 64 * 8;
				short disp = d_disp.load(std::memory_order_relaxed);
				if (SEQ_LIKELY(disp < max))
					d_disp.store(disp + 64 * 8,std::memory_order_relaxed);
				///*if (acceleration == 0)*/ d_disp = (d_disp < max) ? d_disp + 64 * 8 : d_disp;
			}
			/// @brief Decrease dispersion value
			SEQ_ALWAYS_INLINE void decr_disp() noexcept {
				static constexpr short min = std::numeric_limits<short>::min() + 4;
				short disp = d_disp.load(std::memory_order_relaxed);
				if (SEQ_LIKELY(disp > min))
					d_disp.store( disp - 4, std::memory_order_relaxed);
				///*if (acceleration == 0)*/ d_disp = (d_disp > min) ? d_disp - 4 : d_disp;
			}
			/// @brief Reset dispersion value
			SEQ_ALWAYS_INLINE void reset_disp() noexcept {
				d_disp = 0;
			}


			/// @brief Deallocate compressed memory for given bucket
			void dealloc_bucket(size_t index) noexcept
			{
				if (d_buckets[index].buffer) {
					RebindAlloc<char>(*this).deallocate(d_buckets[index].buffer, d_buckets[index].csize);
					d_compress_size -= d_buckets[index].csize;
					d_buckets[index].csize = 0;
					d_buckets[index].buffer = nullptr;
				}
			}

			/// @brief Clear content
			void clear() noexcept
			{
				reset_disp();
				RawType* tmp = nullptr;
				// First, try to find a valid context that we can reuse to destroy all compressed buffer
				if (d_buckets.size())
				{
					for (auto it = d_contexts.begin(); it != d_contexts.end(); ++it)
					{
						if ((*it)->size == 0 || (*it)->size == elems_per_block) {
							tmp = (*it);

							// Destroy its content
							tmp->clear_values();
							break;
						}
					}
					if (!tmp)
						tmp = get_raw_buffer<T,block_size>();
				}

				// Destroy and free all compressed buckets
				for (size_t i = 0; i < d_buckets.size(); ++i) {
					if (d_buckets[i].buffer) {
						if (d_buckets[i].decompressed != tmp)
							destroy_pack_buffer(&d_buckets[i], tmp, RebindAlloc<char>(*this));
						else
							RebindAlloc<char>(*this).deallocate(d_buckets[i].buffer, d_buckets[i].csize);
					}
					else if (d_buckets[i].decompressed && d_buckets[i].decompressed != tmp)
						d_buckets[i].decompressed->clear_values();
				}

				RebindAlignedAlloc<char> al = RebindAlloc<char>(*this);

				// Free all decompression contexts
				for (auto it = d_contexts.begin(); it != d_contexts.end();)
				{
					auto next = it;
					++next;
					//(*it)->clear();
					al.deallocate(reinterpret_cast<char*>(*it), sizeof(RawType));
					it = next;
				}

				// Reset all
				d_contexts.clear();
				d_buckets.clear();
				d_compress_size = 0;
				d_size = 0;
			}

			/// @brief Returns a new raw buffer, might throw
			auto make_raw() -> RawType*
			{
				RebindAlignedAlloc<char> al = RebindAlloc<char>(*this);
				return make_raw_buffer<T,block_size>(al);
			}

			/// @brief Returns the back bucket size
			auto back_size() const noexcept -> unsigned short {
				if (d_buckets.size()) {
					if (d_buckets.back().buffer)
						return static_cast<unsigned short>(elems_per_block);
					return static_cast<unsigned short>(d_buckets.back().decompressed->size);
				}
				return 0;
			}

			/// @brief Returns the front bucket size
			auto front_size() const noexcept -> unsigned short {
				if (d_buckets.size()) {
					if (d_buckets.front().buffer)
						return static_cast<unsigned short>(elems_per_block);
					return static_cast<unsigned short>(d_buckets.size() > 1 ? block_size : d_buckets.front().decompressed->size);
				}
				return 0;
			}

			/// @brief Returns the container size
			auto size() const noexcept -> size_t
			{
				return d_size;
			}

			/// @brief Returns the compression ratio achieved by the block encoder
			auto compression_ratio() const noexcept -> float
			{
				size_t decompressed_size = d_buckets.size();
				if (d_buckets.size() && d_buckets.back().csize == 0) --decompressed_size;
				decompressed_size *= block_size * sizeof(T);
				return (d_compress_size && decompressed_size) ? static_cast<float>(d_compress_size) / decompressed_size : 0;
			}

			/// @brief Returns the current compression ratio, that is the total memory footprint of this container divided by its thoric size (size()*sizeof(T))
			auto current_compression_ratio() const noexcept -> float
			{
				return static_cast<float>(memory_footprint()) / static_cast<float>(d_size * sizeof(T));
			}

			/// @brief Returns the maximum number of decompression contexts as a fraction of the bucket count
			auto max_contexts() const noexcept -> context_ratio
			{
				return d_max_contexts;
			}
			/// @brief Set the maximum allowed compression ratio, used to define the maximum number of allowed decompression contexts
			void set_max_contexts(context_ratio ratio)
			{
				d_max_contexts = ratio;
				shrink_to_fit();
			}

			/// @brief Compress dirty blocks and release all unnecessary decompssioo contexts
			void shrink_to_fit()
			{
				reset_disp();

				ContextType new_contexts;

				const size_t max_buffers = 1;

				for (auto it = d_contexts.begin(); it != d_contexts.end(); )
				{
					RawType* raw = *it++;

					// Try to reuse the decompression context
					if (raw->size > 0 && raw->size < elems_per_block) {
						// Mandatory: reuse a non full decompression context (back context)
						new_contexts.push_back(raw);
						continue;
					}
					else if (new_contexts.size() < max_buffers && !raw->dirty) {
						// Reuse a non dirty decompression context
						new_contexts.push_back(raw);
						if (raw->block_index != RawType::invalid_index)
							d_buckets[raw->block_index].decompressed = nullptr;
						raw->reset();
						continue;
					}

					// If the context is dirty, compress it
					if (raw->dirty) {
						// Find the corresponding PackBuffer, excluding front and back buckets
						size_t index = raw->block_index;
						SEQ_ASSERT_DEBUG(index != RawType::invalid_index, "raw block must belong to an existing bucket");
						// Compress
						unsigned r = Encoder::compress(raw->storage, sizeof(T), block_size, max_csize, acceleration);
						if (r == SEQ_ERROR_DST_OVERFLOW)
							r = block_size * sizeof(T);
						else if (has_error(r))
							SEQ_ABORT("cvector: abort on compression error"); // no way to recover from this

						if (r != d_buckets[index].csize) {
							// Free old buffer, alloc new one, update compressed size, might throw (fine)
							char* buff = this->allocate_buffer_for_compression(r, &d_buckets[index], index, raw);

							if (d_buckets[index].buffer)
								RebindAlloc<char>(*this).deallocate(d_buckets[index].buffer, d_buckets[index].csize);
							d_compress_size -= d_buckets[index].csize;
							d_compress_size += d_buckets[index].csize = r;
							d_buckets[index].buffer = buff;
						}
						if (r == block_size * sizeof(T))
							Encoder::restore(raw->storage, d_buckets[index].buffer, sizeof(T), block_size);
							//transpose_inv_256_rows((char*)get_comp_buffer(0), d_buckets[index].buffer, sizeof(T));
						else
							memcpy(d_buckets[index].buffer, raw->storage, r);

					}

					// Unlink this decompression context with its compressed buffer
					if (raw->block_index != RawType::invalid_index)
						d_buckets[raw->block_index].decompressed = nullptr;

					if (new_contexts.size() < max_buffers) {
						// Add this decompression  context to the new ones
						raw->reset();
						new_contexts.push_back(raw);
					}
					else {
						// Free decompression context
						RebindAlignedAlloc<char> al = RebindAlloc<char>(*this);
						al.deallocate(reinterpret_cast<char*>(raw), sizeof(RawType));
					}
				}

				// Swap contexts
				d_contexts.assign(std::move(new_contexts));

				while (d_contexts.size() > 1) {
					for (auto it = d_contexts.begin(); it != d_contexts.end(); ++it) {
						if ((*it)->size == 0) {
							erase_context(*it);
							break;
						}
					}
				}
			}

			/// @brief Try to lock a compression context
			bool try_lock(RawType* raw) noexcept
			{
				if (raw->block_index != RawType::invalid_index)
					return d_buckets[raw->block_index].lock.try_lock();
				return true;
			}
			/// @brief Unlock a decompression context
			void unlock(RawType* raw) noexcept
			{
				if (raw->block_index != RawType::invalid_index)
					return d_buckets[raw->block_index].lock.unlock();
			}


			/// @brief Allocate and return buffer of given size.
			/// In case of exception, decompress and destroy values, deallocate previous buffer, remove bucket, remove decompression context and rethrow.
			auto allocate_buffer_for_compression(unsigned size, BucketType* bucket, size_t bucket_index, RawType* context) -> char*
			{
				char* buff = nullptr;
				try {
					buff = RebindAlloc<char>(*this).allocate(size); //(char*)malloc(r);
				}
				catch (...) {
					//unlock bucket
					unlock(context);
					// deallocate
					if (bucket->buffer) {
						// first destroy values
						if (!std::is_trivially_destructible<T>::value) {
							context->size = block_size;
							unsigned r = bucket->decompress(context->storage);
							if (has_error(r))
								SEQ_ABORT("cvector: abort on decompression error"); //
							context->clear_values();
						}
						RebindAlloc<char>(*this).deallocate(bucket->buffer, bucket->csize);
						d_compress_size -= bucket->csize;
					}
					//remove context
					erase_context(context);
					//remove bucket
					d_buckets.erase(d_buckets.begin() + static_cast<difference_type>(bucket_index));

					//update indexes
					for (size_t i = bucket_index; i < d_buckets.size(); ++i)
						if (d_buckets[i].decompressed)
							d_buckets[i].decompressed->block_index = i;

					throw;
				}
				return buff;
			}

			/// @brief Returns a decompression context either by creating a new one, or by reusing an existing one
			auto make_or_find_free_context(RawType* exclude = nullptr) -> RawType*
			{
				if (SEQ_LIKELY(d_contexts.size() >= 2))
				{
					size_t max_buffers = std::max(static_cast<size_t>(2), d_max_contexts.context_count(d_buckets.size()));
					if (d_contexts.size() >= max_buffers || (/*acceleration == 0 &&*/ d_disp.load() < 0))
						return find_free_context(exclude);
				}

				// Create a new context, might throw (fine)
				RawType* raw = make_raw();
				d_contexts.push_front(raw);
				return raw;

			}

			/// @brief Reuse and return an existing decompression context that cannot be exclude one
			auto find_free_context(RawType* exclude = nullptr, typename ContextType::iterator* start = nullptr) -> RawType*
			{
				// All contexts used, compress one of them, if possible an empty or not dirty one

				// Start by the tail
				auto found = start ? *start : d_contexts.end();
				if (d_contexts.size()) {
					--found;
					if (found != d_contexts.end()) {
						while ((((*found)->size && (*found)->size != elems_per_block) || *found == exclude || !try_lock((*found)))) {
							if (found == d_contexts.begin()) {
								found = d_contexts.end();
								break;
							}
							--found;
						}
					}
				}

				if (found == d_contexts.end()) {
					if (start)
						return nullptr;
					// Cannot find one: create a new one, might throw (fine)
					RawType* raw = make_raw();

					// Insert the new context at the beginning
					d_contexts.push_front(raw);
					// Return it
					return raw;
				}



				RawType* found_raw = *found;
				BucketType* found_bucket = (*found)->block_index == RawType::invalid_index ? nullptr : &d_buckets[(*found)->block_index];
				size_t saved_index = (*found)->block_index;

				// Compress context if dirty
				if (found_raw->dirty) {
					//find the corresponding PackBuffer, excluding front and back buckets
					SEQ_ASSERT_DEBUG(found_bucket, "context must belong to an existing bucket");

					// unlock global spinlock for the compression stage, help in multithreaded contexts
					bool is_locked = d_lock.is_locked();
					if (is_locked) d_lock.unlock();
					unsigned r = Encoder::compress(found_raw->storage, sizeof(T), block_size, max_csize, acceleration);
					if (is_locked) d_lock.lock();

					if (r == SEQ_ERROR_DST_OVERFLOW) {
						// Store uncompressed
						r = block_size * sizeof(T);
					}
					else if (has_error(r))
						SEQ_ABORT("cvector: abort on compression error"); // no way to recover from this

					if (r != found_bucket->csize) {
						// Free old memory, alloc new one
						char* buff = allocate_buffer_for_compression(r, found_bucket, saved_index, found_raw);
						if (found_bucket->buffer)
							RebindAlloc<char>(*this).deallocate(found_bucket->buffer, found_bucket->csize);
						d_compress_size -= found_bucket->csize;
						d_compress_size += found_bucket->csize = r;
						found_bucket->buffer = buff;
					}

					if (r == block_size * sizeof(T))
						Encoder::restore(found_raw->storage, found_bucket->buffer, sizeof(T), block_size);
						//transpose_inv_256_rows((char*)get_comp_buffer(0), found_bucket->buffer, sizeof(T));
					else
						memcpy(found_bucket->buffer, found_raw->storage, r);

					//TEST
					if (/*acceleration == 0 &&*/ d_disp.load() < 0 && !start) {
						RawType* raw = find_free_context(exclude, &found);
						if (raw)
							erase_context(raw);
					}
				}

				if (d_contexts.size() > 1 && found != d_contexts.begin()) {
					// Move the context to index 0.
					// This way, we maximize the chances to find at the tail the (possibly) oldest context that should be the first to be reused
					d_contexts.erase(found_raw);
					d_contexts.push_front(found_raw);
				}

				// Unlink
				if (found_bucket)
					found_bucket->decompressed = nullptr;

				// Reset, unlock and return
				unlock(found_raw);
				found_raw->reset();
				return found_raw;
			}

			/// @brief Compress bucket using its own context
			auto compress_bucket(size_t index) -> RawType*
			{
				BucketType* bucket = &d_buckets[index];
				RawType* decompressed = bucket->decompressed;

				// Compress
				unsigned r = Encoder::compress(decompressed->storage, sizeof(T), block_size, max_csize, acceleration);
				if (r == SEQ_ERROR_DST_OVERFLOW) {
					// Store uncompressed
					r = block_size * sizeof(T);
				}
				else if (has_error(r))
					SEQ_ABORT("cvector: abort on compression error"); // no way to recover from this

				if (r != bucket->csize) {
					char* buff = allocate_buffer_for_compression(r, bucket, index, decompressed);
					if (bucket->buffer)
						RebindAlloc<char>(*this).deallocate(bucket->buffer, bucket->csize);
					bucket->buffer = buff;
				}
				if (r == block_size * sizeof(T))
					Encoder::restore(decompressed->storage, bucket->buffer, sizeof(T), block_size);
					//transpose_inv_256_rows((char*)get_comp_buffer(0), bucket->buffer, sizeof(T));
				else
					memcpy(bucket->buffer, decompressed->storage, r);

				bucket->decompressed = nullptr;
				//free buckets
				d_compress_size -= bucket->csize;
				d_compress_size += bucket->csize = r;
				decompressed->reset();
				return decompressed;
			}

			/// @brief Ensure that a back bucket is available for back insertion
			void ensure_has_back_bucket()
			{
				if (d_buckets.empty() || d_buckets.back().buffer) {
					//no buckets or the back buffer is compressed, create a new one
					RawType* raw = make_or_find_free_context();
					// might throw, fine as we did not specify yet the block index
					d_buckets.push_back(BucketType(raw));
					raw->block_index = d_buckets.size() - 1;
				}
				else if (d_buckets.back().decompressed->size == elems_per_block) {
					RawType* raw = compress_bucket(d_buckets.size() - 1);
					// might throw, fine as we did not specify yet the block index
					d_buckets.push_back(BucketType(raw));
					raw->block_index = d_buckets.size() - 1;
				}
			}

			/// @brief Decompress given bucket.
			/// If necessary, use an existing context or create a new one (which cannot be the exclude one)
			void decompress_bucket(size_t index, size_t exclude = static_cast<size_t>(-1))
			{
				if (!d_buckets[index].decompressed)
				{
					BucketType* pack = nullptr;
					RawType* raw = nullptr;
					{
						std::lock_guard<spinlock> lock(d_lock);
						pack = &d_buckets[index];
						raw = make_or_find_free_context(exclude == static_cast<size_t>(-1) ? nullptr : d_buckets[exclude].decompressed);
						raw->block_index = index;
					}
					
					unsigned r = pack->decompress(raw->storage);
					if (SEQ_UNLIKELY(has_error(r)))
						SEQ_ABORT("cvector: abort on compression error"); //no way to recover from this

					pack->decompressed = raw;
					raw->dirty = 0;
					raw->size = block_size;
				}
			}

			/// @brief Returns the class total memory footprint, including sizeof(*this)
			auto memory_footprint() const noexcept -> size_t
			{
				size_t res = d_compress_size;
				res += d_buckets.capacity() * sizeof(BucketType);
				res += d_contexts.size() * sizeof(RawType);
				res += sizeof(*this);
				return res;
			}

			/// @brief Back insertion
			template< class... Args >
			void emplace_back(Args&&... args)
			{
				// All functions might throw, fine (strong guarantee)
				if (!(d_buckets.size() && !d_buckets.back().buffer && d_buckets.back().decompressed->size < elems_per_block))
					ensure_has_back_bucket();
				BucketType& bucket = d_buckets.back();
				try {
					// might throw, see below
					construct_ptr(&bucket.decompressed->at(bucket.decompressed->size), std::forward<Args>(args)...);
					++bucket.decompressed->size;
					bucket.decompressed->mark_dirty();
					d_size++;
				}
				catch (...) {
					// Exception: if we just created the back bucket, we must remove it
					if (d_buckets.back().decompressed->size == 0) {
						d_buckets.back().decompressed->block_index = RawType::invalid_index;
						d_buckets.pop_back();
					}
					throw;
				}
			}
			/// @brief Back insertion
			void push_back(const T& value)
			{
				emplace_back(value);
			}
			/// @brief Back insertion
			void push_back(T&& value)
			{
				emplace_back(std::move(value));
			}

			/// @brief Remove context from list of contexts and deallocate it.
			/// Do not forget to destroy its content first!!!
			void erase_context(RawType* r)
			{
				d_contexts.erase(r);
				RebindAlignedAlloc<char> al = RebindAlloc<char>(*this);
				al.deallocate(reinterpret_cast<char*>(r), sizeof(RawType));
			}

			/// @brief Remove back value
			void pop_back() 
			{
				SEQ_ASSERT_DEBUG(size() > 0, "calling pop_back on empty container");

				// remove empty back bucket
				if (d_buckets.back().decompressed && d_buckets.back().decompressed->size == 0) {
					if (d_buckets.back().buffer)
						RebindAlloc<char>(*this).deallocate(d_buckets.back().buffer, d_buckets.back().csize);
					erase_context(d_buckets.back().decompressed);
					d_buckets.pop_back();
				}
				// decompress back bucket if necessary
				if (!d_buckets.back().decompressed)
					decompress_bucket(d_buckets.size() - 1);

				// destroy element
				if (!std::is_trivially_destructible<T>::value)
					destroy_ptr(&d_buckets.back().decompressed->at(d_buckets.back().decompressed->size - 1));
				d_buckets.back().decompressed->mark_dirty(this); //destroy compressed buffer if necessary
				--d_buckets.back().decompressed->size;
				--d_size;

				// destroy back bucket if necessary, as well as decompression context
				if (d_buckets.back().decompressed->size == 0) {
					erase_context(d_buckets.back().decompressed);
					d_buckets.pop_back();
				}
			}

			/// @brief Reserve is a no-op
			void reserve(size_t)
			{
				// No-op
			}

			/// @brief Resize to lower size
			void resize_shrink(size_t new_size)
			{
				// Pop back until we reach a size multiple of block_size
				while (size() > new_size && (size() & (block_size-1)))
					pop_back();

				if (size() > block_size) {

					// Remove full blocks
					while (size() > new_size + block_size)
					{
						// destroy values in last bucket
						if (!std::is_trivially_destructible<T>::value) {
							if (!d_buckets.back().decompressed)
								decompress_bucket(d_buckets.size() - 1);
							d_buckets.back().decompressed->clear_values();
						}

						if (d_buckets.back().buffer) {
							RebindAlloc<char>(*this).deallocate(d_buckets.back().buffer, d_buckets.back().csize);
							d_compress_size -= d_buckets.back().csize;
						}
						if (d_buckets.back().decompressed)
							erase_context(d_buckets.back().decompressed);
						d_buckets.pop_back();

						d_size -= block_size;
					}
				}
				// Pop back remaining values
				while (size() > new_size)
					pop_back();
			}

			void resize(size_t new_size)
			{
				reset_disp();

				if (new_size == 0)
					clear();
				else if (new_size == size())
					return;
				else if (new_size > size())
				{
					

					//finish filling back buffer
					while (size() < new_size && (size() & (block_size-1)))
						emplace_back();

					//fill by chunks of block_size elements
					if (new_size > block_size) {

						// temporary storage for chunks of block_size elements
						T value{};
						RawType raw;
						raw.size = 0;

						while (size() < new_size - block_size)
						{
							unsigned r = 0;
							if SEQ_CONSTEXPR (!std::is_same<detail::DefaultEncoder, Encoder>::value || !std::is_trivially_constructible<T>::value)
							{
								// Generic way to compress one value repeated
								raw.size = block_size;
								// construct, might throw, fine
								for (unsigned i = 0; i < block_size; ++i)
									construct_ptr(&raw.at(i));
								//compress 
								r = Encoder::compress(raw.storage, sizeof(T), block_size, max_csize, acceleration);
								if (r == SEQ_ERROR_DST_OVERFLOW) {
									r = block_size * sizeof(T);
								}
								else if (has_error(r))
									SEQ_ABORT("cvector: abort on compression error"); // no way to recover from this
							}
							else {
								MinimalBlockBound<T>::compress(value, raw.storage);
								r = MinimalBlockBound<T>::value;
							}

							char* buff = nullptr;
							try {
								// might throw, see below
								buff = RebindAlloc<char>(*this).allocate(r);
								d_buckets.push_back(BucketType(nullptr, buff, r));
								if (r == block_size * sizeof(T))
									Encoder::restore(raw.storage, buff, sizeof(T), block_size);
									//transpose_inv_256_rows((char*)get_comp_buffer(0), buff, sizeof(T));
								else
									memcpy(buff, raw.storage, r);
							}
							catch (...) {
								// In case of exception, free buffer if necessary and destroy elements
								if (buff) RebindAlloc<char>(*this).deallocate(buff, r);
								raw.clear_values();
								throw;
							}
							d_compress_size += r;
							//raw.clear();
							d_size += block_size;
						}
					}

					// finish with last elements
					while (size() < new_size)
						emplace_back();

				}
				else {
					resize_shrink(new_size);
				}
			}


			void resize(size_t new_size, const T& val)
			{
				reset_disp();

				if (new_size == 0)
					clear();
				else if (new_size == size())
					return;
				else if (new_size > size())
				{
					//finish filling back buffer
					while (size() < new_size && (size() & (block_size-1)))
						emplace_back(val);

					//fill by chunks of block_size elements
					if (new_size > block_size) {

						// temporary storage for chunks of block_size elements
						RawType raw;
						raw.size = 0;

						while (size() < new_size - block_size)
						{
							unsigned r = 0;
							if SEQ_CONSTEXPR(!std::is_same<detail::DefaultEncoder, Encoder>::value || !std::is_trivially_constructible<T>::value)
							{
								raw.size = block_size;
								// construct, might throw, fine
								for (unsigned i = 0; i < block_size; ++i)
									construct_ptr(&raw.at(i), val);
								//compress 
								r = Encoder::compress(raw.storage, sizeof(T), block_size, max_csize, acceleration);
								if (r == SEQ_ERROR_DST_OVERFLOW)
									r = block_size * sizeof(T);
								else if (has_error(r))
									SEQ_ABORT("cvector: abort on compression error");// no way to recover from this
							}
							else
							{
								MinimalBlockBound<T>::compress(val, raw.storage);
								r = MinimalBlockBound<T>::value;
							}

							char* buff = nullptr;
							try {
								buff = RebindAlloc<char>(*this).allocate(r);
								d_buckets.push_back(BucketType(nullptr, buff, r));
								if (r == block_size * sizeof(T))
									Encoder::restore(raw.storage, buff, sizeof(T), block_size);
									//transpose_inv_256_rows((char*)get_comp_buffer(0), buff, sizeof(T));
								else
									memcpy(buff, raw.storage, r);
							}
							catch (...) {
								// In case of exception, free buffer if necessary and destroy elements
								if (buff) RebindAlloc<char>(*this).deallocate(buff, r);
								raw.clear_values();
								throw;
							}
							d_compress_size += r;
							//raw.clear();
							d_size += block_size;
						}
					}

					// finish with last elements
					while (size() < new_size)
						emplace_back(val);
				}
				else {
					resize_shrink(new_size);
				}
			}

			/// @brief Erase range
			auto erase(const_iterator first, const_iterator last) -> const_iterator
			{
				reset_disp();

				SEQ_ASSERT_DEBUG(last >= first && first >= const_iterator(this, 0) && last <= const_iterator(this), "cvector erase iterator outside range");

				if (first == last)
					return last;

				// We need at least 3 contexts
				auto lock = lock_context_ratio(this, context_ratio(3));
				difference_type off = first - const_iterator(this, 0);
				size_t count = static_cast<size_t>(last - first);

				//iterator it = first;
				//this->const_for_each(static_cast<size_t>(last - const_iterator(this, 0)), size(), [&it](const T& v) {*it = std::move(const_cast<T&>(v)); ++it; });
				std::move(iterator(last), iterator(this), iterator(first));
				if (count == 1)
					pop_back();
				else
					resize(size() - count);

				return const_iterator(this, 0) + off;
			}

			/// @brief Erase middle
			auto erase(const_iterator pos) -> const_iterator
			{
				return erase(pos, pos + 1);
			}

			/// @brief Insert middle
			template< class... Args >
			auto emplace(const_iterator pos, Args&&... args) -> iterator
			{
				difference_type dist = pos - const_iterator(this, 0);
				SEQ_ASSERT_DEBUG(static_cast<size_t>(dist) <= size(), "cvector: invalid insertion location");

				// We need at least 3 contexts
				auto lock = lock_context_ratio(this, context_ratio(3));

				//insert on the right side
				this->emplace_back(std::forward<Args>(args)...);
				std::rotate(iterator(this, 0) + dist, iterator(this) - 1, iterator(this));

				return iterator(this, 0) + dist;
			}


			/// @brief Insert range
			template< class InputIt >
			auto insert(const_iterator pos, InputIt first, InputIt last) -> iterator
			{
				reset_disp();

				difference_type off = (pos - const_iterator(this, 0));
				size_t oldsize = size();

				SEQ_ASSERT_DEBUG(static_cast<size_t>(off) <= size(), "cvector insert iterator outside range");

				// We need at least 3 contexts
				auto lock = lock_context_ratio(this, context_ratio(3));

				if (size_t len = seq::distance(first, last))
				{
					// For random access iterators

					try {
						resize(size() + len);

						std::move_backward(iterator(this, 0) + off, iterator(this, 0) + static_cast<difference_type>(size() - len), iterator(this));
						
						// std::copy(first, last, iterator(this, 0) + off);
						// use for_each instead of std::copy
						for_each(static_cast<size_t>(off), static_cast<size_t>(off) + len, [&first](T& v) {v = *first; ++first; });
					}
					catch (...) {
						for (; oldsize < size(); )
							pop_back();	// restore old size, at least
						throw;
					}
					return (iterator(this, 0) + off);
				}


				// Non random access iterators
				if (first == last)
					;
				else {

					try {
						for (; first != last; ++first)
							push_back(*first);	// append
					}
					catch (...) {
						for (; oldsize < size(); )
							pop_back();	// restore old size, at least
						throw;
					}

					std::rotate(iterator(this, 0) + off, iterator(this, 0) + static_cast<difference_type>(oldsize), iterator(this));
				}
				return (iterator(this, 0) + off);
			}


			/// @brief Lock bucket for given flat position
			void lock(size_t pos)noexcept
			{
				size_t bucket = pos >> shift;
				d_buckets[bucket].lock.lock();
			}
			/// @brief Unlock bucket for given flat position
			void unlock(size_t pos)noexcept
			{
				size_t bucket = pos >> shift;
				d_buckets[bucket].lock.unlock();
			}

			auto make_lock(size_t pos) noexcept -> lock_guard<spinlock>
			{
				return make_lock_guard(d_buckets[pos >> shift].lock);
			}
			auto make_block_lock(size_t pos) noexcept -> lock_guard<spinlock>
			{
				return make_lock_guard(d_buckets[pos].lock);
			}

			auto at(size_t pos) noexcept -> ref_type { return ref_type(this, pos >> shift, pos & mask); }
			auto at(size_t pos) const noexcept -> const_ref_type { return const_ref_type(this, pos >> shift, pos & mask); }
			auto operator[](size_t pos) noexcept -> ref_type { return at(pos); }
			auto operator[](size_t pos) const noexcept -> const_ref_type { return at(pos); }
			auto back() noexcept -> ref_type { return at(size() - 1); }
			auto back() const noexcept -> const_ref_type { return at(size() - 1); }
			auto front() noexcept -> ref_type { return at(0); }
			auto front() const noexcept -> const_ref_type { return at(0); }
			auto max_size() const noexcept -> size_t { return std::numeric_limits<difference_type>::max(); }

			template<class Functor>
			auto const_for_each(size_t start, size_t end, Functor fun) const -> Functor
			{
				SEQ_ASSERT_DEBUG(start <= end, "const_for_each: invalid range");
				SEQ_ASSERT_DEBUG(end <= d_size, "const_for_each: invalid range");
				if (start == end)
					return fun;

				size_t remaining = end - start;
				size_t bindex = start >> shift;
				size_t pos = start & mask;

				while (remaining) {
					size_t to_process = std::min(remaining, static_cast<size_t>(block_size - pos));
					auto lock = const_cast<ThisType*>(this)->make_block_lock(bindex);
					const RawType* cur = d_buckets[bindex].decompressed;
					if (!cur) {
						const_cast<ThisType*>(this)->decompress_bucket(bindex);
						cur = d_buckets[bindex].decompressed;
					}
					remaining -= to_process;
					size_t en = pos + to_process;
					for (size_t p = pos; p != en; ++p)
						fun(cur->at(p));
					pos = 0;
					++bindex;
				}
				return fun;
			}

			template<class Functor>
			auto for_each(size_t start, size_t end, Functor fun) -> Functor
			{
				SEQ_ASSERT_DEBUG(start <= end, "for_each: invalid range");
				SEQ_ASSERT_DEBUG(end <= d_size, "for_each: invalid range");
				if (start == end)
					return fun;

				size_t remaining = end - start;
				size_t bindex = start >> shift;
				size_t pos = start & mask;

				while (remaining) {
					size_t to_process = std::min(remaining, static_cast<size_t>(block_size - pos));
					auto lock = this->make_block_lock(bindex);
					RawType* cur = d_buckets[bindex].decompressed;
					if (!cur) {
						this->decompress_bucket(bindex);
						cur = d_buckets[bindex].decompressed;
					}
					remaining -= to_process;
					size_t en = pos + to_process;
					for (size_t p = pos; p != en; ++p)
						fun(cur->at(p));
					pos = 0;
					++bindex;
					cur->mark_dirty(this);
				}
				return fun;
			}

			template<class Functor>
			auto const_for_each_backward(size_t first, size_t last, Functor fun) const -> Functor
			{
				SEQ_ASSERT_DEBUG(first <= last, "for_each_backward: invalid range");
				SEQ_ASSERT_DEBUG(last <= d_size, "for_each_backward: invalid range");
				if (first == last)
					return fun;

				--last;
				difference_type last_bucket = static_cast<difference_type>(last >> shift);
				int last_index = static_cast<int>(last & mask);
				difference_type first_bucket = static_cast<difference_type>(first >> shift);
				int first_index = static_cast<int>(first & mask);

				for (difference_type bindex = last_bucket; bindex >= first_bucket; --bindex)
				{
					auto lock = const_cast<ThisType*>(this)->make_block_lock(bindex);
					const RawType* cur = d_buckets[bindex].decompressed;
					if (!cur) {
						const_cast<ThisType*>(this)->decompress_bucket(bindex);
						cur = d_buckets[bindex].decompressed;
					}
					int low = bindex == first_bucket ? first_index : 0;
					int high = bindex == last_bucket ? last_index : static_cast<int>(block_size-1);
					for (int i = high; i >= low; --i)
						fun(cur->at(i));
				}

				return fun;
			}

			template<class Functor>
			auto for_each_backward(size_t first, size_t last, Functor fun) -> Functor
			{
				SEQ_ASSERT_DEBUG(first <= last, "for_each_backward: invalid range");
				SEQ_ASSERT_DEBUG(last <= d_size, "for_each_backward: invalid range");
				if (first == last)
					return fun;

				--last;
				difference_type last_bucket = static_cast<difference_type>(last >> shift);
				int last_index = static_cast<int>(last & mask);
				difference_type first_bucket = static_cast<difference_type>(first >> shift);
				int first_index = static_cast<int>(first & mask);

				for (difference_type bindex = last_bucket; bindex >= first_bucket; --bindex)
				{
					auto lock = this->make_block_lock(bindex);
					RawType* cur = d_buckets[bindex].decompressed;
					if (!cur) {
						this->decompress_bucket(bindex);
						cur = d_buckets[bindex].decompressed;
					}
					int low = bindex == first_bucket ? first_index : 0;
					int high = bindex == last_bucket ? last_index : static_cast<int>(block_size-1);
					for(int i= high; i>= low; --i)
						fun(cur->at(i));
					cur->mark_dirty(this);
				}

				return fun;
			}


		};
	}


	template<class Compressed>
	struct is_hashable<detail::ConstValueWrapper<Compressed> > : std::false_type {};
	template<class Compressed>
	struct is_hashable<detail::ValueWrapper<Compressed> > : std::false_type {};



	/// @brief vector like class using compression to store its elements
	/// @tparam T value type, must be relocatable
	/// @tparam Allocator allocator type
	/// @tparam Acceleration acceleratio nparameter for the compression algorithm, from 0 to 7
	/// @tparam Encoder encoder type, default to DefaultEncoder
	/// @tparam block_size number of elements per chunks, must be a power of 2
	/// 
	/// seq::cvector is a is a random-access container with an interface similar to std::vector but storing its element in a compressed way. 
	/// Its goal is to reduce the memory footprint of the container while providing performances as close as possible to std::vector. 
	/// 
	/// 
	/// Internals
	/// ---------
	/// 
	/// By default, cvector stores its elements by chunks of 256 values. Whenever a chunk is filled (due for instance to calls to push_back()), it is compressed
	/// and the chunk itself is either kept internally (for further decompression) or deallocated. This means that cvector NEVER ensure reference stability,
	/// as a stored object might only exist in its compressed form.
	/// 
	/// When accessing a value using iterators or operator[], the corresponding chunk is first located. If this chunk was already decompressed, a reference wrapper
	/// to the corresponding element is returned. Otherwise, the memory chunk is decompressed first. If the accessed element is modified, the chunk is mark as dirty,
	/// meaning that it will require recompression at some point.
	/// 
	/// To avoid compressing/decompressing lots of chunks when performing heavy random-access operations, cvector allows multiple chunks to store their elements
	/// in their decompressed form, called **decompression contexts**. The maximum number of decompression contexts is defined using
	/// cvector::set_max_contexts(), and is either a fixed number or a fraction of the total number of chunks. By default, the number of decompression contexts is
	/// limited to 12.5% of the number of chunks. This means that the cvector memory footprint is at most 1.125 times higher than cvector::size()*sizeof(value_type).
	/// 
	/// Whenever a chunk must be decompressed to access one of its element, it allocates a new decompression context if the maximum number of allowed contexts has not been
	/// reach yet, and this context is added to an internal list of all available contexts. Otherwise, it try to reuse an existing decompression context 
	/// from this internal list. Note that the chunk might reuse a context already used by another chunk. In this case, this other chunk is recompressed if marked dirty, 
	/// the context is detached from this chunk and attached to the new one which decompress its elements inside. This means that accessing an element 
	/// (even with const members) might invalidate any other reference to elements within the container. 
	///  
	/// 
	/// Element access
	/// --------------
	/// 
	/// Individual elements can be accessed using cvector::operator[], cvector::at, cvector::front, cvector::back or iterators. As seen in previous section, accessing
	/// an element might invalidate all other references to the container elements (in fact, it might invalidate all elements that do not belong to the corresponding chunk).
	/// 
	/// That's why access members return a reference wrapper instead of a plain reference (types cvector::ref_type and cvector::const_ref_type). A reference wrapper basically
	/// stores a pointer to cvector internal data and the coordinate of the corresponding element. When casting this wrapper to **value_type**, the corresponding chunk is
	/// decompressed (if needed) and the value at given location is returned. A reference wrapper can be casted to **value_type** or **const value_type&**, in which case the 
	/// reference should not be used after accessing another element.
	/// 
	/// Example:
	/// 
	/// \code{.cpp}
	/// 
	/// // Fill cvector
	/// seq::cvector<int> vec;
	/// for(int i=0; i < 1000; ++i)
	///		vec.push_back(i);
	/// 
	/// // a is of type cvector<int>::ref_type
	/// auto a = vec[0];
	/// 
	/// // copy element pointed by a to b
	/// int b = a;
	/// 
	/// // Store a const reference of element 0
	/// const int & c = vec[0];
	/// 
	/// // WARNING: accessing element at position 600 might invalidate reference c!
	/// const int & d = vec[600];
	/// 
	/// \endcode 
	/// 
	/// In order for cvector to work with all STL algorithms, some latitudes with C++ standard were taken:
	///		-	std::swap is overloaded for reference wrapper types. Overloading std::swap is forbidden by the standard, but works in practive with msvc and gcc at least.
	///		-	std::move is overloaded for reference wrapper types. This was mandatory for algorithms like std::move(first,last,dst) to work on move-only types.
	/// 
	/// Thanks to this, it is possible to call std::sort or std::random_shuffle on a cvector. For instance, the following code snippet successively:
	///		-	Call cvector::push_back to fill the cvector with sorted data. In this case the compression ratio is very low due to high values correlation.
	///		-	Call std::random_shuffle to shuffle the cvector: the compresion ratio become very high as compressing random data is not possible.
	///		-	Sort again the cvector with std::sort to get back the initial compression ratio.
	/// 
	/// \code{.cpp}
	/// 
	/// #include "cvector.hpp"
	/// #include "utils.hpp"
	/// #include "testing.hpp"
	/// 
	/// #include <iostream>
	/// 
	/// //...
	/// 
	/// using namespace seq;
	///
	/// cvector<int> w;
	/// 
	/// // fill with consecutive values
	/// for (size_t i = 0; i < 10000000; ++i)
	/// 	w.push_back((int)i);
	/// 
	/// std::cout << "push_back: " << w.current_compression_ratio() << std::endl;
	/// 
	/// // shuffle the cvector
	/// tick();
	/// seq::random_shuffle(w.begin(), w.end());
	/// size_t el = tock_ms();
	/// std::cout << "random_shuffle: " << w.current_compression_ratio() << " in " << el << " ms" << std::endl;
	/// 
	/// // sort the cvector
	/// tick();
	/// std::sort(w.begin(), w.end());
	/// el = tock_ms();
	/// std::cout << "sort: " << w.current_compression_ratio() << " in " << el << " ms" << std::endl;
	/// 
	/// \endcode 
	/// 
	/// Below is a curve representing the program memory footprint during previous operations (extracted with Visual Studio diagnostic tools):
	/// 
	/// \image html cvector_memory.png
	/// 
	/// Restrictions
	/// ------------
	/// 
	/// cvector only works with relocatable value types (relocation in terms of move plus destroy).
	/// seq::is_relocatable type trait will be used to detect invalid data types. You can specialize seq::is_relocatable for your custom
	/// types if you are certain they are indeed relocatable.
	/// 
	/// 
	/// Compression algorithm
	/// ---------------------
	/// 
	/// In order for cvector to have any interset over a standard **std::vector** or **std::deque**, its compression algorithm must be:
	///		-	very fast or the performance gap will be to high compared to STL counterparts,
	///		-	symetric if possible, as compression is performed almost as often as decompression,
	///		-	efficient on small blocks of data to allow fast random access.
	/// 
	/// It turns out I developed a compression algorithm a while back for lossless image compression that worked on small blocks of 16*16 pixels. I just had
	/// to adjust it to work on flat input and blocks of 256 elements. This algorithm relies on well known compression methods: it uses bit packing, delta coding
	/// and RLE (whichever is better) on the transposed block. All of this is performed using SSE4. Both compression and decompression run at more or less 2GB/s 
	/// on a my laptop (Intel(R) Core(TM) i7-10850H CPU @ 2.70GHz).
	/// 
	/// If compared to other compression methods working on transposed input like <a href="https://www.blosc.org/">blosc</a> with <a href="https://github.com/lz4/lz4">LZ4</a>, my compression algorithm
	/// provides slighly lower values: it is slower and compress less by a small margin. However, it is way more efficient on small blocks (256 elements in this case)
	/// as it keeps its full strength: indeed, each block is compressed independently.
	///	
	/// The compression algorithm supports an acceleration factor ranging from 0 (maximum compression) to 7 (fastest). It mainly changes the way near-uncompressible 
	/// blocks are handled. The acceleration factor is given as a template parameter of cvector class.
	/// 
	/// It is possible to specify a different compression method using the **Encoder** template argument of cvector. For instance one can use the **seq::detail::NullEncoder**
	/// that encode/decode blocks using... memcpy (transforming cvector to a poor deque-like class). For custom encoder, it is possible to specify a different block size 
	/// instead of the default 256 elements (it must remain a power of 2). Note that increasing the block size might increase the compression ratio and bidirectional access patterns,
	/// but will slow down random-access patterns.
	/// 
	/// seq library provides seq::Lz4FlatEncoder and seq::Lz4TransposeEncoder (in **internal/lz4small.h**) as example of custom encoders that can be passed to cvector.
	/// They rely on a modified version of LZ4 compression algorithm suitable for small input length. seq::Lz4FlatEncoder is sometimes better than the default block encoder 
	/// for cvector of seq::tiny_string.
	/// 
	/// 
	/// Multithreading
	/// --------------
	/// 
	/// By default, cvector does not support multi-threaded access, even on read-only mode. Indeed, retrieving an element might trigger a block decompression, which in
	/// turn might trigger a recompression of another block in order to steal its decompression context.
	/// 
	/// cvector supports a locking mechanism at the block level for concurrent accesses. Below is a commented example of several ways to apply std::cos function
	/// to all elements of a cvector, including multi-threading based on openmp and use of the low level block API.
	/// 
	/// \code{.cpp}
	/// 
	/// #include "cvector.hpp"
	/// #include "testing.hpp"
	/// 
	/// #include <vector>
	/// #include <cstdlib>
	/// #include <algorithm>
	/// #include <iostream>
	/// 
	/// using namespace seq;
	/// 
	/// 
	/// int  main  (int , char** )
	/// {
	/// 
	///	// Create 10000000 random float values
	/// std::srand(0);
	/// std::vector<float>  random_vals(10000000);
	/// for (float& v : random_vals)
	///		v = std::rand();
	/// 
	/// // fill a cvector with random values
	/// cvector<float> vec;
	/// for (float v : random_vals)
	///		vec.push_back(v);
	/// 
	/// 
	/// // Standard loop over all values using operator[]
	/// tick();
	/// for (size_t i = 0; i < vec.size(); ++i)
	/// {
	/// 	vec[i] = std::cos((float)vec[i]);
	/// }
	/// size_t el = tock_ms();
	/// std::cout << "operator[]: " << el << " ms" << std::endl;
	/// 
	/// 
	/// // reset values
	/// std::copy(random_vals.begin(), random_vals.end(), vec.begin());
	/// 
	/// 
	/// // Standard loop over all values using iterators
	/// tick();
	/// for (auto it = vec.begin(); it != vec.end(); ++it)
	/// {
	/// 	*it = std::cos((float)*it);
	/// }
	/// el = tock_ms();
	/// std::cout << "iterator: " << el << " ms" << std::endl;
	/// 
	/// 
	/// //reset values
	/// std::copy(random_vals.begin(), random_vals.end(), vec.begin());
	/// 
	/// 
	/// // Use cvector::for_each (mono threaded, but supports concurrent access).
	/// // This is the fastest non multithreaded way to access cvector values.
	/// tick();
	/// vec.for_each(0, vec.size(), [](float& v) {v = std::cos(v); });
	/// el = tock_ms();
	/// std::cout << "cvector::for_each: " << el << " ms" << std::endl;
	/// 
	/// 
	/// //reset values
	/// std::copy(random_vals.begin(), random_vals.end(), vec.begin());
	/// 
	/// 
	/// // Multithreaded loop
	/// tick();
	/// #pragma omp parallel for
	/// for (int i = 0; i < (int)vec.size(); ++i)
	/// {
	/// 	// lock position i since we multithreaded the loop
	/// 	auto lock_guard = vec.lock(i);
	/// 	vec[i] = std::cos((float)vec[i]);
	/// }
	/// el = tock_ms();
	/// std::cout << "operator[] multithreaded: " << el << " ms" << std::endl;
	/// 
	/// 
	/// //reset values
	/// std::copy(random_vals.begin(), random_vals.end(), vec.begin());
	/// 
	/// 
	/// // Parallel loop over blocks instead of values, using cvector block API
	/// tick();
	/// #pragma omp parallel for
	/// // loop over all blocks
	/// for (int i = 0; i < (int)vec.block_count(); ++i)
	/// {
	/// 	// lock block since we multithreaded the loop
	/// 	auto lock_guard = vec.lock_block(i);
	/// 
	/// 	// retrieve the block as a std::pair<float*, unsigned> (data pointer, block size)
	/// 	auto bl = vec.block(i);
	/// 
	/// 	// apply std::cos functions on all elements of the block
	/// 	for (unsigned j = 0; j < bl.second; ++j)
	/// 		bl.first[j] = std::cos(bl.first[j]);
	/// 
	/// 	// manually mark the block as dirty (need recompression at some point)
	/// 	vec.mark_dirty_block(i);
	/// }
	/// el = tock_ms();
	/// std::cout << "block API multithreaded: " << el << " ms" << std::endl;
	/// 
	/// 
	/// return 0;
	/// }
	/// 
	/// \endcode
	/// 
	/// 
	/// Above example compiled with gcc 10.1.0 (-O3) for msys2 on Windows 10 on a Intel(R) Core(TM) i7-10850H at 2.70GHz gives the following output:
	/// 
	/// > operator[]: 454 ms
	/// >
	/// > iterator: 444 ms
	/// >
	/// > cvector::for_each : 418 ms
	/// >
	/// > operator[] multithreaded : 179 ms
	/// >
	/// > block API multithreaded : 59 ms
	/// 
	/// 
	/// Serialization
	/// -------------
	/// 
	/// cvector provides serialization/deserialization functions working on compressed blocks. Use cvector::serialize to save the cvector content in
	/// a std::ostream object, and cvector::deserialize to read back the cvector from a std::istream object. When deserializing a cvector object with
	/// cvector::deserialize, the cvector template parameters must be the same as the ones used for serialization, except for the **Acceleration** parameter and the allocator type.
	/// 
	/// Example:
	/// 
	/// \code{.cpp}
	/// 
	/// #include "cvector.hpp"
	/// 
	/// #include <string>
	/// #include <iostream>
	/// #include <vector>
	/// #include <sstream>
	/// #include <algorithm>
	/// 
	/// using namespace seq;
	/// 
	/// 
	/// int  main  (int , char** )
	/// {
	/// 
	/// // Create values we want to serialize
	/// std::vector<int> content(10000000);
	/// for (size_t i = 0; i < content.size(); ++i)
	/// 	content[i] = i;
	/// 
	/// 
	/// std::string saved;
	/// {
	/// 	// Create a cvector, fill it
	/// 	cvector<int> vec;
	/// 	std::copy(content.begin(), content.end(), std::back_inserter(vec));
	/// 
	/// 	// Save cvector in 'saved' string
	/// 	std::ostringstream oss;
	/// 	vec.serialize(oss);
	/// 	saved = oss.str();
	/// 
	/// 	// print the compression ratio based on 'saved'
	/// 	std::cout << "serialize compression ratio: " << saved.size() / (double)(sizeof(int) * vec.size()) << std::endl;
	/// }
	/// 
	/// // Deserialize 'saved' string
	/// std::istringstream iss(saved);
	/// cvector<int> vec;
	/// vec.deserialize(iss);
	/// 
	/// // Make sure the deserialized cvector is equal to the original vector
	/// std::cout << "deserialization valid: " << std::equal(vec.begin(), vec.end(), content.begin(), content.end()) << std::endl;
	/// 
	/// return 0;
	/// }
	/// 
	/// \endcode
	/// 
	/// 
	/// Custom comparison
	/// -----------------
	/// 
	/// When using a custom comparator function with STL algorithms like std::sort or std::equal on cvector, there are chances that the algorithm won't work as expected or just crash.
	/// This is because 2 reference wrappers might be casted at the same time to real value_type references, that then will be passed to the custom comparator.
	/// However, since accessing a cvector value might invalidate all other references, the custom comparator might be applied on dangling objects.  
	/// To avoid this error, you must use a comparator wrapper that will smoothly handle such situations using seq::make_comparator.
	/// Example:
	/// 
	/// \code{.cpp}
	/// 
	/// #include "cvector.hpp"
	/// 
	/// #include <algorithm>
	/// #include <memory>
	/// #include <iostream>
	/// #include <cstdlib>
	/// 
	/// using namespace seq;
	/// 
	/// 
	/// int  main  (int , char** )
	/// {
	///		using ptr_type = std::unique_ptr<size_t>;
	/// 
	///		// Create a cvector of unique_ptr with random integers
	///		cvector<ptr_type> vec;
	///		std::srand(0);
	///		for(size_t i = 0; i < 1000000; ++i)
	///			vec.emplace_back(new size_t(std::rand()));
	/// 
	///		// print the compression ratio
	///		std::cout<< vec.current_compression_ratio() <<std::endl;
	/// 
	///		// sort the cvector using the defined comparison operator between 2 std::unique_ptr objects (sort by pointer address)
	///		std::sort(vec.begin(),vec.end());
	/// 
	///		// print again the compression ratio
	///		std::cout<< vec.current_compression_ratio() <<std::endl;
	/// 
	///		// Now we want to sort by pointed value. 
	///		// We need a custom comparison function that will be passed to seq::make_comparator
	///		
	///		std::sort(vec.begin(),vec.end(), make_comparator([](const ptr_type & a, const ptr_type & b){return *a < *b; }));
	/// 
	///		// print again the compression ratio
	///		std::cout<< vec.current_compression_ratio() <<std::endl;
	/// 
	///		return 0;
	/// }
	/// 
	/// \endcode
	/// 
	/// 
	/// Heterogeneous container
	/// -----------------------
	/// 
	/// cvector works with seq::hold_any to provide heterogeneous compressed vector. However it only works with seq::r_any instead of seq::any as it requires a relocatable type.
	/// Example:
	/// 
	/// \code{.cpp}
	/// 
	/// #include "cvector.hpp"
	/// #include "any.hpp"
	/// #include "testing.hpp"
	/// 
	/// #include <algorithm>
	/// #include <iostream>
	/// #include <cstdlib>
	/// 
	/// using namespace seq;
	/// 
	/// 
	/// int  main  (int , char** )
	/// {
	///		// Construct a cvector of r_any filled with various values of type size_t, double, std::string or tstring.
	///		seq::cvector<seq::r_any> vec;
	///		for (size_t i = 0; i < 100000; ++i)
	///		{
	///			size_t idx = i & 3U;
	///			switch (idx) {
	///			case 0: vec.push_back(seq::r_any(i * UINT64_C(0xc4ceb9fe1a85ec53)));
	///			case 1: vec.push_back(seq::r_any((double)(i * UINT64_C(0xc4ceb9fe1a85ec53))));
	///			case 2: vec.push_back(seq::r_any(generate_random_string<std::string>(14, true)));
	///			default: vec.push_back(seq::r_any(generate_random_string<tstring>(14, true)));
	/// 	}
	/// 
	///		// Print the compression ratio
	///		std::cout << vec.current_compression_ratio() << std::endl;
	///		
	///		// Sort the heterogeneous container (see hold_any documentation for more details on its comparison operators)
	///		std::sort(vec.begin(), vec.end());
	/// 
	///		// Print the compression ratio
	///		std::cout << vec.current_compression_ratio() << std::endl;
	///		
	///		// Ensure the container is well sorted
	///		std::cout << std::is_sorted(vec.begin(), vec.end()) << std::endl;
	/// 
	///		return 0;
	/// }
	/// 
	/// \endcode
	/// 
	template<class T, class Allocator = std::allocator<T>, unsigned Acceleration = 0 , class Encoder = detail::DefaultEncoder, unsigned block_size = 256>
	class cvector : private Allocator
	{
		using internal_type = detail::CompressedVectorInternal<T, Allocator, Acceleration, Encoder, block_size>;
		using bucket_type = typename internal_type::BucketType;
		template< class U>
		using RebindAlloc = typename std::allocator_traits<Allocator>::template rebind_alloc<U>;


		internal_type* d_data;	// Store a pointer to internal data, easier to provide noexcept move constructor/copy and iterator stability on swap

		internal_type* make_internal(const Allocator& al)
		{
			RebindAlloc< internal_type> a = al;
			internal_type* ret = nullptr;
			try {
				ret = a.allocate(1);
				construct_ptr(ret, al);
			}
			catch (...)
			{
				if (ret)
					a.deallocate(ret, 1);
				throw;
			}
			return ret;
		}
		void destroy_internal(internal_type* data)
		{
			destroy_ptr(data);
			RebindAlloc< internal_type> a = get_allocator();
			a.deallocate(data, 1);
		}

		void make_data_if_null()
		{
			if (!d_data)
				d_data = make_internal(get_allocator());
		}


		/// @brief Returns the compressed buffer for given block.
		/// This function will compress if necessary the corresponding block and deallocate the decompression context (if any).
		/// For all blocks except the last one, this function returns a view on compression buffer.
		/// For the last block, and only if not full, this function returns the raw values.
		tstring_view compressed_block(size_t pos)
		{
			if (!d_data)
				return tstring_view();

			if (pos == d_data->d_buckets.size() - 1 && d_data->d_buckets.back().decompressed && d_data->d_buckets.back().decompressed->size < internal_type::elems_per_block)
			{
				//last non full bucket: just return the raw values
				return tstring_view(d_data->d_buckets.back().decompressed->storage, d_data->d_buckets.back().decompressed->size * sizeof(T));
			}

			auto* bucket = &d_data->d_buckets[pos];
			if (bucket->decompressed && bucket->decompressed->dirty) {
				// compress bucket
				auto* raw = d_data->compress_bucket(pos);

				// erase decompression context if this is not the last one
				d_data->erase_context(raw);
			}

			return tstring_view(bucket->buffer, bucket->csize);
		}



	public:
		static_assert(is_relocatable<T>::value, "cvector: given type must be relocatable based on seq::is_relocatable type trait");

		static constexpr unsigned acc = Acceleration > 7 ? 7 : Acceleration;
		static constexpr unsigned max_block_size = (internal_type::elems_per_block * sizeof(T)) - (internal_type::elems_per_block * sizeof(T)) / (10 - acc);

		using value_type = T;
		using reference = T&;
		using const_reference = const T&;
		using pointer = T*;
		using const_pointer = const T*;
		using size_type = size_t;
		using difference_type = typename std::allocator_traits<Allocator>::difference_type;
		using allocator_type = Allocator;
		using iterator = typename internal_type::iterator;
		using const_iterator = typename internal_type::const_iterator;
		using reverse_iterator = std::reverse_iterator<iterator>;
		using const_reverse_iterator = std::reverse_iterator<const_iterator>;

		// Value type returned by at() and operator[]
		using ref_type = typename internal_type::ref_type;
		using const_ref_type = typename internal_type::const_ref_type;
		using encoder_type = Encoder;

		// Compression level
		static constexpr unsigned acceleration = Acceleration;

		/// @brief Default constructor, initialize the internal bucket manager.
		cvector()
			:Allocator(), d_data(nullptr)
		{
		}
		/// @brief Constructs an empty container with the given allocator alloc.
		/// @param alloc allocator object
		explicit cvector(const Allocator& alloc)
			:Allocator(alloc), d_data(make_internal(alloc))
		{
		}
		/// @brief Constructs the container with \a count copies of elements with value \a value.
		/// @param count new cvector size
		/// @param value the value to initialize elements of the container with
		/// @param alloc allocator object 
		cvector(size_type count, const T& value, const Allocator& alloc = Allocator())
			:Allocator(alloc), d_data(make_internal(alloc))
		{
			resize(count, value);
		}
		/// @brief Constructs the container with count default-inserted instances of T. No copies are made.
		/// @param count new cvector size
		/// @param alloc allocator object
		explicit cvector(size_type count, const Allocator& alloc = Allocator())
			:Allocator(alloc), d_data(make_internal(alloc))
		{
			resize(count);
		}
		/// @brief Copy constructor. Constructs the container with the copy of the contents of other.
		/// @param other another container to be used as source to initialize the elements of the container with
		cvector(const cvector& other)
			:Allocator(copy_allocator(other.get_allocator())), d_data(nullptr)
		{
			if (other.size()) {

				// Use the block API for faster copy
				resize(other.size());
				auto lock1 = detail::lock_context_ratio(d_data, context_ratio(3));
				auto lock2 = detail::lock_context_ratio(const_cast<internal_type*>(other.d_data), context_ratio(3));
				
				for (size_t i = 0; i < other.block_count(); ++i)
				{
					auto blo = other.block(i);
					auto bl = this->block(i);
					std::copy(blo.first, blo.first + blo.second, bl.first);
					this->mark_dirty_block(i);
				}

				//d_data = make_internal(other.get_allocator());
				//auto lock = detail::lock_context_ratio(d_data, context_ratio(3));
				//other.for_each(0, other.size(), [this](const T& v) { this->push_back(v); });
				//for (auto it = other.begin(); it != other.end(); ++it)
				//	push_back(*it);
			}
		}
		/// @brief Constructs the container with the copy of the contents of other, using alloc as the allocator.
		/// @param other other another container to be used as source to initialize the elements of the container with
		/// @param alloc allcoator object
		cvector(const cvector& other, const Allocator& alloc)
			:Allocator(alloc), d_data(nullptr)
		{
			if (other.size()) {
				d_data = make_internal(alloc);
				// calling push_back is faster than resize + copy
				auto lock = detail::lock_context_ratio(d_data, context_ratio(3));
				other.for_each(0, other.size(), [this](const T& v) { this->push_back(v); });
			}
		}
		/// @brief Move constructor. Constructs the container with the contents of other using move semantics. Allocator is obtained by move-construction from the allocator belonging to other.
		/// @param other another container to be used as source to initialize the elements of the container with
		cvector(cvector&& other) noexcept(std::is_nothrow_move_constructible<Allocator>::value)
			:Allocator(std::move(other.get_allocator())), d_data(other.d_data)
		{
			other.d_data = nullptr;
		}
		/// @brief  Allocator-extended move constructor. Using alloc as the allocator for the new container, moving the contents from other; if alloc != other.get_allocator(), this results in an element-wise move.
		/// @param other another container to be used as source to initialize the elements of the container with
		/// @param alloc allocator object
		cvector(cvector&& other, const Allocator& alloc)
			:Allocator(alloc), d_data(make_internal(alloc))
		{
			if (alloc == other.get_allocator()) {
				std::swap(d_data, other.d_data);
			}
			else {
				auto lock1 = detail::lock_context_ratio(d_data, context_ratio(3));
				auto lock2 = detail::lock_context_ratio(other.d_data, context_ratio(3));

				resize(other.size());
				std::move(other.begin(), other.end(), begin());
			}
		}
		/// @brief Constructs the container with the contents of the initializer list \a init.
		/// @param lst initializer list
		/// @param alloc allocator object
		cvector(const std::initializer_list<T>& lst, const Allocator& alloc = Allocator())
			:Allocator(alloc), d_data(make_internal(alloc))
		{
			assign(lst.begin(), lst.end());
		}
		/// @brief  Constructs the container with the contents of the range [first, last).
		/// @tparam Iter iterator type
		/// @param first first iterator of the range
		/// @param last last iterator of the range
		/// @param alloc allocator object
		template< class Iter>
		cvector(Iter first, Iter last, const Allocator& alloc = Allocator())
			:Allocator(alloc), d_data(make_internal(alloc))
		{
			assign(first, last);
		}

		/// @brief  Destructor
		~cvector()
		{
			if(d_data)
				destroy_internal(d_data);
		}

		/// @brief Move assignment operator.
		/// @param other another container to use as data source
		/// @return reference to this
		auto operator=(cvector&& other) noexcept(noexcept(std::declval<cvector&>().swap(std::declval<cvector&>()))) -> cvector&
		{
			this->swap(other);
			return *this;
		}

		/// @brief Copy assignment operator.
		/// @param other another container to use as data source
		/// @return reference to this
		auto operator=(const cvector& other) -> cvector&
		{
			if (this != std::addressof(other)) {

				if SEQ_CONSTEXPR(assign_alloc<Allocator>::value)
				{
					if (get_allocator() != other.get_allocator()) {
						destroy_internal(d_data);
						d_data = nullptr;
					}
				}
				assign_allocator(get_allocator(), other.get_allocator());

				if (other.size() == 0)
					clear();
				else {
					internal_type* tmp = make_internal(get_allocator());
					try {
						auto lock = detail::lock_context_ratio(tmp, context_ratio(3));
						try {
							other.for_each(0, other.size(), [tmp](const T& v) {tmp->push_back(v); });
						}
						catch (...) {
							destroy_internal(tmp);
							throw;
						}
						destroy_internal(d_data);
						d_data = tmp;
					}
					catch (...) {
						destroy_internal(tmp);
					}

				}
			}
			return *this;
		}

		/// @brief Returns the total memory footprint in bytes of this cvector, excluding sizeof(*this)
		auto memory_footprint() const noexcept -> size_t
		{
			return d_data ? d_data->memory_footprint() : 0;
		}

		/// @brief Returns the compression ratio achieved by the block encoder
		auto compression_ratio() const noexcept -> float
		{
			return d_data ? d_data->compression_ratio() : 0;
		}

		/// @brief Returns the current compression ratio, which is the total memory footprint of this container divided by its theoric size (size()*sizeof(T))
		auto current_compression_ratio() const noexcept -> float
		{
			return d_data ? d_data->current_compression_ratio() : 0;
		}

		/// @brief Returns the maximum number of decompression contexts
		auto max_contexts() const noexcept -> context_ratio
		{
			return d_data ? d_data->max_contexts() : context_ratio();
		}
		/// @brief Set the maximum number of allowed decompression contexts
		void set_max_contexts(context_ratio ratio)
		{
			make_data_if_null();
			d_data->set_max_contexts(ratio);
		}

		/// @brief Returns the container size.
		auto size() const noexcept -> size_type
		{
			return d_data ? d_data->size() : 0;
		}
		/// @brief Returns the container maximum size.
		static auto max_size() noexcept -> size_type
		{
			return std::numeric_limits<difference_type>::max();
		}
		/// @brief Retruns true if the container is empty, false otherwise.
		auto empty() const noexcept -> bool
		{
			return !d_data || d_data->size() == 0;
		}
		/// @brief Returns the allocator associated with the container.
		auto get_allocator() const noexcept -> const Allocator&
		{
			return static_cast<const Allocator&>(*this);
		}
		/// @brief Returns the allocator associated with the container.
		auto get_allocator() noexcept -> Allocator&
		{
			return static_cast< Allocator&>(*this);
		}
		/// @brief Exchanges the contents of the container with those of other. Does not invoke any move, copy, or swap operations on individual elements.
		/// @param other other sequence to swap with
		/// All iterators and references remain valid.
		/// An iterator holding the past-the-end value in this container will refer to the other container after the operation.
		void swap(cvector& other) noexcept(noexcept(swap_allocator(std::declval<Allocator&>(), std::declval<Allocator&>())))
		{
			if (this != std::addressof(other)) {
				std::swap(d_data, other.d_data);
				swap_allocator(get_allocator(), other.get_allocator());
			}
		}

		/// @brief Release all unused memory, and compress all dirty blocks.
		/// This function does NOT invalidate iterators, except if an exception is thrown.
		/// Basic exception guarantee only.
		void shrink_to_fit()
		{
			if (d_data)
				d_data->shrink_to_fit();
		}

		/// @brief Resizes the container to contain count elements.
		/// @param count new size of the container
		/// If the current size is greater than count, the container is reduced to its first count elements.
		/// If the current size is less than count, additional default-inserted elements are appended.
		/// Basic exception guarantee.
		void resize(size_type count)
		{
			if (count == this->size())
				return; // No-op
			else if (count == 0)
				clear();
			else {
				make_data_if_null();
				d_data->resize(count);
			}
		}

		/// @brief Resizes the container to contain count elements.
		/// @param count new size of the container
		/// @param value the value to initialize the new elements with
		/// If the current size is greater than count, the container is reduced to its first count elements.
		/// If the current size is less than count, additional copies of value are appended.
		/// Basic exception guarantee.
		void resize(size_type count, const T& value)
		{
			if (count == this->size())
				return; // No-op
			else if (count == 0)
				clear();
			else {
				make_data_if_null();
				d_data->resize(count, value);
			}
		}

		/// @brief Clear the container.
		void clear() noexcept
		{
			if (empty())
				return;
			d_data->clear();
		}

		/// @brief Appends the given element value to the end of the container.
		/// @param value the value of the element to append
		/// Strong exception guarantee.
		void push_back(const T& value)
		{
			make_data_if_null();
			d_data->push_back(value);
		}

		/// @brief Appends the given element value to the end of the container using move semantic.
		/// @param value the value of the element to append
		/// Strong exception guarantee.
		void push_back(T&& value)
		{
			make_data_if_null();
			d_data->push_back(std::move(value));
		}

		/// @brief Appends a new element to the end of the container
		/// @tparam ...Args 
		/// @param ...args T constructor arguments
		/// @return reference to inserted element
		/// Strong exception guarantee.
		template< class... Args >
		auto emplace_back(Args&&... args) -> ref_type
		{
			make_data_if_null();
			d_data->emplace_back(std::forward<Args>(args)...);
			return back();
		}

		/// @brief Insert \a value before \a it
		/// @param it iterator within the cvector
		/// @param value element to insert
		/// Basic exception guarantee.
		auto insert(const_iterator it, const T& value) -> iterator
		{
			return emplace(it, value);
		}

		/// @brief Insert \a value before \a it using move semantic.
		/// @param it iterator within the cvector
		/// @param value element to insert
		/// Basic exception guarantee.
		auto insert(const_iterator it, T&& value) -> iterator
		{
			return emplace(it, std::move(value));
		}
		/// @brief Inserts a new element into the container directly before \a pos.
		/// @tparam ...Args 
		/// @param pos iterator within the cvector
		/// @param ...args T constructor arguments
		/// @return reference to inserted element
		/// Basic exception guarantee.
		template<class... Args>
		auto emplace(const_iterator pos, Args&&... args) -> iterator
		{
			make_data_if_null();
			d_data->emplace(pos, std::forward<Args>(args)...);
			return begin() + pos.abspos;
		}


		/// @brief Inserts elements from range [first, last) before it.
		/// @tparam Iter iterator type
		/// @param it iterator within the cvector
		/// @param first first iterator of the range
		/// @param last last iterator of the range
		/// @return Iterator pointing to the first element inserted, or it if first==last
		/// Basic exception guarantee.
		template<class Iter>
		auto insert(const_iterator it, Iter first, Iter last) -> iterator
		{
			make_data_if_null();
			return d_data->insert(it, first, last);
		}

		/// @brief Inserts elements from initializer list ilist before pos.
		/// @return Iterator pointing to the first element inserted, or it if first==last.
		/// Basic exception guarantee.
		auto insert(const_iterator pos, std::initializer_list<T> ilist) -> iterator
		{
			return insert(pos, ilist.begin(), ilist.end());
		}

		/// @brief Inserts count copies of the value before pos
		/// Basic exception guarantee. 	
		void insert(size_type pos, size_type count, const T& value)
		{
			insert(pos, cvalue_iterator<T>(0, value), cvalue_iterator<T>(count, value));
		}

		/// @brief Inserts count copies of the value before pos
		/// @return Iterator pointing to the first element inserted, or it if count==0
		/// Basic exception guarantee.
		auto insert(const_iterator pos, size_type count, const T& value) -> iterator
		{
			return insert(pos, cvalue_iterator<T>(0, value), cvalue_iterator<T>(count, value));
		}

		/// @brief Removes the last element of the container.
		/// Calling pop_back on an empty container results in undefined behavior.
		/// Strong exception guarantee.
		void pop_back()
		{
			SEQ_ASSERT_DEBUG(d_data, "empty cvector");
			d_data->pop_back();
		}

		/// @brief Erase element at given position.
		/// @param it iterator to the element to remove
		/// @return iterator following the last removed element
		/// Basic exception guarantee.
		auto erase(const_iterator it) -> iterator
		{
			SEQ_ASSERT_DEBUG(d_data, "empty cvector");
			return d_data->erase(it);
		}

		/// @brief Removes the elements in the range [first, last).
		/// @param first iterator to the first element to erase
		/// @param last iterator to the last (excluded) element to erase
		/// @return Iterator following the last removed element.
		/// Basic exception guarantee.
		auto erase(const_iterator first, const_iterator last) -> iterator
		{
			SEQ_ASSERT_DEBUG(d_data, "empty cvector");
			return d_data->erase(first, last);
		}

		/// @brief Replaces the contents with \a count copies of value \a value
		/// Basic exception guarantee.
		void assign(size_type count, const T& value)
		{
			clear();
			resize(count, value);
			//assign(cvalue_iterator<T>(0, value), cvalue_iterator<T>(count, value));
		}

		/// @brief Replaces the contents with copies of those in the range [first, last). The behavior is undefined if either argument is an iterator into *this.
		/// Basic exception guarantee.
		template< class Iter >
		void assign(Iter first, Iter last)
		{
			if (size_t len = seq::distance(first, last))
			{
				// For random access iterators
				resize(len);
				auto lock1 = detail::lock_context_ratio(d_data, context_ratio(2));
				for (size_t i = 0; i < block_count(); ++i)
				{
					auto bl = block(i);
					for (unsigned j = 0; j < bl.second; ++j, ++first) {
						bl.first[j] = *first;
					}
					mark_dirty_block(i);
				}
			}
			else
			{
				// For forward iterators
				// It is faster to just dump everything and use push_back
				clear();
				make_data_if_null();
				while (first != last) {
					d_data->push_back(*first);
					++first;
				}
			}
		}

		/// @brief Replaces the contents with the elements from the initializer list ilist.
		/// Basic exception guarantee.
		void assign(std::initializer_list<T> ilist)
		{
			assign(ilist.begin(), ilist.end());
		}

		auto lock_block(size_t block_pos) -> lock_guard<spinlock>
		{
			make_data_if_null();
			return d_data->make_block_lock(block_pos);
		}
		auto lock(size_t pos) -> lock_guard<spinlock>
		{
			make_data_if_null();
			return d_data->make_lock(pos);
		}

		/// @brief Returns a reference wrapper to the element at specified location pos, with bounds checking.
		auto at(size_type pos) const -> const_ref_type {
			//random access
			if (pos >= size()) throw std::out_of_range("");
			return (d_data->at(pos));
		}
		/// @brief Returns a reference wrapper to the element at specified location pos, with bounds checking.
		auto at(size_type pos) -> ref_type
		{
			//random access
			if (pos >= size()) throw std::out_of_range("");
			return (d_data->at(pos));
		}
		/// @brief Returns a reference wrapper to the element at specified location pos, without bounds checking.
		auto operator[](size_type pos) const noexcept -> const_ref_type {
			//random access
			SEQ_ASSERT_DEBUG(d_data, "empty cvector");
			return (d_data->at(pos));
		}
		/// @brief Returns a reference wrapper to the element at specified location pos, without bounds checking.
		auto operator[](size_type pos) noexcept -> ref_type {
			//random access
			SEQ_ASSERT_DEBUG(d_data, "empty cvector");
			return (d_data->at(pos));
		}

		/// @brief Returns a reference wrapper to the last element in the container.
		auto back() noexcept -> ref_type { SEQ_ASSERT_DEBUG(d_data, "empty cvector"); return d_data->back(); }
		/// @brief Returns a reference wrapper to the last element in the container.
		auto back() const noexcept -> const_ref_type { SEQ_ASSERT_DEBUG(d_data, "empty cvector"); return d_data->back(); }
		/// @brief Returns a reference wrapper to the first element in the container.
		auto front() noexcept -> ref_type { SEQ_ASSERT_DEBUG(d_data, "empty cvector"); return d_data->front(); }
		/// @brief Returns a reference wrapper to the first element in the container.
		auto front() const noexcept -> const_ref_type { SEQ_ASSERT_DEBUG(d_data, "empty cvector"); return d_data->front(); }

		/// @brief Returns an iterator to the first element of the cvector.
		auto begin() const noexcept -> const_iterator { return const_iterator(d_data, 0); }
		/// @brief Returns an iterator to the first element of the cvector.
		auto begin() noexcept -> iterator { return iterator(d_data, 0); }
		/// @brief Returns an iterator to the element following the last element of the cvector.
		auto end() const noexcept -> const_iterator { return const_iterator(d_data, d_data ? static_cast<difference_type>(d_data->d_size) : 0); }
		/// @brief Returns an iterator to the element following the last element of the cvector.
		auto end() noexcept -> iterator { return iterator(d_data, d_data ? static_cast<difference_type>(d_data->d_size) : 0); }
		/// @brief Returns a reverse iterator to the first element of the reversed list.
		auto rbegin() noexcept -> reverse_iterator { return reverse_iterator(end()); }
		/// @brief Returns a reverse iterator to the first element of the reversed list.
		auto rbegin() const noexcept -> const_reverse_iterator { return const_reverse_iterator(end()); }
		/// @brief Returns a reverse iterator to the element following the last element of the reversed list.
		auto rend() noexcept -> reverse_iterator { return reverse_iterator(begin()); }
		/// @brief Returns a reverse iterator to the element following the last element of the reversed list.
		auto rend() const noexcept -> const_reverse_iterator { return const_reverse_iterator(begin()); }
		/// @brief Returns an iterator to the first element of the cvector.
		auto cbegin() const noexcept -> const_iterator { return begin(); }
		/// @brief Returns an iterator to the element following the last element of the cvector.
		auto cend() const noexcept -> const_iterator { return end(); }
		/// @brief Returns a reverse iterator to the first element of the reversed list.
		auto crbegin() const noexcept -> const_reverse_iterator { return rbegin(); }
		/// @brief Returns a reverse iterator to the element following the last element of the reversed list.
		auto crend() const noexcept -> const_reverse_iterator { return rend(); }



		template<class Functor>
		auto for_each(size_t first, size_t last, Functor fun) -> Functor
		{
			if (d_data)
				return d_data->for_each(first, last, fun);
			return fun;
		}
		
		template<class Functor>
		auto for_each(size_t first, size_t last, Functor fun) const -> Functor
		{
			if (d_data)
				return d_data->const_for_each(first, last, fun);
			return fun;
		}
		template<class Functor>
		auto const_for_each(size_t first, size_t last, Functor fun) const -> Functor
		{
			if (d_data)
				return d_data->const_for_each(first, last, fun);
			return fun;
		}


		template<class Functor>
		auto for_each_backward(size_t first, size_t last, Functor fun) -> Functor
		{
			if (d_data)
				return d_data->for_each_backward(first, last, fun);
			return fun;
		}

		template<class Functor>
		auto for_each_backward(size_t first, size_t last, Functor fun) const -> Functor
		{
			if (d_data)
				return d_data->const_for_each_backward(first, last, fun);
			return fun;
		}
		template<class Functor>
		auto const_for_each_backward(size_t first, size_t last, Functor fun) const -> Functor
		{
			if (d_data)
				return d_data->const_for_each_backward(first, last, fun);
			return fun;
		}


		///////////////////////////
		// Additional low level interface related to cvector specificities
		///////////////////////////


		///////////////////////////
		// Block access
		///////////////////////////

		/// @brief Returns the number of blocks. A block contains at most 256 elements.
		size_t block_count() const noexcept { return d_data ? d_data->d_buckets.size() : 0; }

		/// @brief Mark the block at given position as dirty.
		/// Call this function if you modified the block using block(pos).
		void mark_dirty_block(size_t pos) noexcept
		{
			if (d_data && d_data->d_buckets[pos].decompressed)
				d_data->d_buckets[pos].decompressed->mark_dirty(d_data);
		}
		/// @brief Returns a pair (block data pointer, block size) for given block position.
		/// This let you apply low level functions based on this block like simd based computations.
		std::pair<const T*, unsigned> block(size_t pos) const
		{
			if (d_data) {
				const auto* bucket = &d_data->d_buckets[pos];
				if (!bucket->decompressed)
					const_cast<internal_type*>(d_data)->decompress_bucket(pos);
				SEQ_ASSERT_DEBUG(bucket->decompressed != nullptr, "");
				return std::pair<const T*, unsigned>(reinterpret_cast<const T*>(bucket->decompressed->storage), bucket->decompressed->size);
			}
			return std::pair<const T*, unsigned>(nullptr, 0);
		}
		/// @brief Returns a pair (block data pointer, block size) for given block position.
		/// This let you apply low level functions on this block like simd based computations.
		/// If block data are modified, you must call yourself mark_dirty_block().
		std::pair<T*, unsigned> block(size_t pos)
		{
			if (d_data) {
				auto* bucket = &d_data->d_buckets[pos];
				if (!bucket->decompressed)
					d_data->decompress_bucket(pos);
				SEQ_ASSERT_DEBUG(bucket->decompressed != nullptr, "");
				return std::pair<T*, unsigned>(reinterpret_cast<T*>(bucket->decompressed->storage), bucket->decompressed->size);
			}
			return std::pair<T*, unsigned>(nullptr, 0);
		}

		///////////////////////////
		// Serialization/deserialization
		///////////////////////////

		/// @brief Serialize cvector content into a std::ostream object
		std::ostream& serialize(std::ostream& oss)
		{
			std::uint64_t s = size();
#if SEQ_BYTEORDER_ENDIAN == SEQ_BYTEORDER_BIG_ENDIAN
			s = byte_swap_64(s);
#endif
			// write total size
			detail::write_integer(oss, s);
			if (!oss)
				return oss;
			if (!d_data) {
				oss.flush();
				return oss;
			}


			for (size_t i = 0; i < d_data->d_buckets.size(); ++i)
			{
				// check if last bucket is empty
				if (i == d_data->d_buckets.size() - 1 && d_data->d_buckets.back().decompressed && d_data->d_buckets.back().decompressed->size == 0)
					return oss;

				tstring_view buf = compressed_block(i);
				//write buffer size
				detail::write_integer(oss, static_cast<unsigned>(buf.size()));
				//write buffer data
				oss.write(buf.data(), buf.size());
				if (!oss)
					return oss;
			}
			oss.flush();

			return oss;
		}

		/// @brief Deserialize cvector from a std::istream object.
		/// Previous content of the cvector is cleared.
		std::istream& deserialize(std::istream& iss)
		{
			clear();
			make_data_if_null();

			d_data->d_compress_size = 0;

			// read total size
			std::uint64_t s = detail::read_integer< std::uint64_t>(iss);
			if (!iss)
				return iss;

#if SEQ_BYTEORDER_ENDIAN == SEQ_BYTEORDER_BIG_ENDIAN
			s = byte_swap_64(s);
#endif
			RebindAlloc<char> al = get_allocator();

			size_t full_blocks = s / internal_type::elems_per_block;
			for (size_t i = 0; i < full_blocks; ++i)
			{
				unsigned bsize = detail::read_integer<unsigned>(iss);
				if (!iss)
					return iss;

				// might throw, fine
				char* data = al.allocate(bsize);
				iss.read(data, bsize);
				if (!iss) {
					al.deallocate(data, bsize);
					return iss;
				}

				try {
					d_data->d_buckets.push_back(bucket_type());
				}
				catch (...) {
					al.deallocate(data, bsize);
					throw;
				}
				d_data->d_buckets.back().buffer = data;
				d_data->d_buckets.back().csize = bsize;
				d_data->d_buckets.back().decompressed = nullptr;
				d_data->d_size += internal_type::elems_per_block;
				d_data->d_compress_size += bsize;
			}

			if (s % internal_type::elems_per_block) {

				// last bucket

				// create a raw buffer, might throw, fine
				detail::RawBuffer<T,internal_type::elems_per_block>* raw = d_data->make_raw();
				// add to contexts
				d_data->d_contexts.push_front(raw);

				// read bucket
				unsigned bsize = detail::read_integer<unsigned>(iss);
				if (!iss)
					return iss;

				// direct read
				iss.read(raw->storage, bsize);

				// might throw, fine
				d_data->d_buckets.push_back(bucket_type());
				d_data->d_buckets.back().buffer = nullptr;
				d_data->d_buckets.back().csize = 0;
				d_data->d_buckets.back().decompressed = raw;

				raw->size = s % internal_type::elems_per_block;
				raw->dirty = 1;
				d_data->d_size += raw->size;
				raw->block_index = d_data->d_buckets.size() - 1;
			}

			return iss;
		}
	};


	/// @brief Binary comparison wrapper for using STL algorithm on cvector with custom comparators.
	/// Use seq::make_comparator to build & comp_wrapper object.
	template<class Comp>
	struct comp_wrapper : public Comp
	{
		Comp& comparator() { return static_cast<Comp&>(*this); }
		const Comp& comparator() const { return static_cast<const Comp&>(*this); }

		comp_wrapper() {}
		comp_wrapper(const Comp& other) : Comp(other) {}
		comp_wrapper(const comp_wrapper& other) : Comp(other) {}

		template<class Compressed>
		bool operator()(const detail::ConstValueWrapper<Compressed>& a, const detail::ConstValueWrapper<Compressed>& b) const
		{
			using T = typename Compressed::value_type;
			return a.compare(b, [this](const T& _a, const T& _b) {return this->comparator()(_a, _b); });
		}
		template<class Compressed>
		bool operator()(const detail::ConstValueWrapper<Compressed>& a, const typename Compressed::value_type& b) const
		{
			return this->comparator()(a.get(), b);
		}
		template<class Compressed>
		bool operator()(const typename Compressed::value_type& a, const detail::ConstValueWrapper<Compressed>& b) const
		{
			return this->comparator()(a, b.get());
		}
	};

	/// @brief Create a comparison functor that can be used by algorithms working on cvector objects.
	/// @tparam Comp actual comparison function type
	/// @param comp comparison function
	/// @return comparison wrapper
	template<class Comp>
	auto make_comparator(const Comp& comp) -> comp_wrapper<Comp>
	{
		return comp_wrapper<Comp>(comp);
	}



	namespace detail
	{
		template<class T, bool MoveOnly = !std::is_copy_assignable<T>::value>
		struct MoveObject
		{
			static T apply(T&& ref)
			{
				return std::move(ref);
			}
		};
		template<class T>
		struct MoveObject<T, true>
		{
			static T apply(T&& ref)
			{
				T tmp = std::move(ref);
				return tmp;
			}
		};
	}


} //end namespace seq

namespace std
{
	/////////////////////////////
	// std::swap overloads. This is illegal considering C++ standard, but works in practice, and is mandatory to make cvector work with standard algorithms, like std::sort
	/////////////////////////////

	template<class Compressed>
	void swap(seq::detail::ValueWrapper<Compressed>&& a, seq::detail::ValueWrapper<Compressed>&& b) 
	{
		a.swap(b);
	}


	///////////////////////////
	// Completely illegal overload of std::move.
	// That's currently the only way I found to use generic algorithms (like std::move(It, It, Dst) ) with cvector. Works with msvc and gcc.
	///////////////////////////

	template<class Compress>
	typename Compress::value_type move(seq::detail::ValueWrapper<Compress>& other) {
		return seq::detail::MoveObject<typename Compress::value_type>::apply(other.move());

	}

	template<class Compress>
	typename Compress::value_type move(seq::detail::ValueWrapper<Compress>&& other) {
		return seq::detail::MoveObject<typename Compress::value_type>::apply(other.move());
	}

}

#endif //#ifdef __SSE4_1__

#endif
