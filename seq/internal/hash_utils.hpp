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

#ifndef SEQ_HASH_UTILS_HPP
#define SEQ_HASH_UTILS_HPP

 /** @file */

#include <type_traits>
#include <utility>
#include "../bits.hpp"

namespace seq
{
	namespace detail
	{


		/// @brief Gather hash class and equal_to class in the same struct. Inherits both for non empty classes.
		/// This is a simple way to handle statefull hash function or equality comparison function.
		template< class Hash, class Equal >
		struct HashEqual : private Hash, private Equal
		{
			HashEqual() {}
			HashEqual(const Hash& h, const Equal& e) : Hash(h), Equal(e) {}
			HashEqual(const HashEqual& other)  noexcept(std::is_nothrow_copy_constructible<Hash>::value&& std::is_nothrow_copy_constructible<Equal>::value)
			: Hash(other), Equal(other) {}
			HashEqual(HashEqual && other) noexcept(std::is_nothrow_move_constructible<Hash>::value && std::is_nothrow_move_constructible<Equal>::value)
			: Hash(std::move(other)), Equal(std::move(other)) {}

			auto operator=(const HashEqual& other) noexcept(std::is_nothrow_copy_assignable<Hash>::value&& std::is_nothrow_copy_assignable<Equal>::value)
				-> HashEqual&{
				static_cast<Hash&>(*this) = static_cast<const Hash&>(other);
				static_cast<Equal&>(*this) = static_cast<const Equal&>(other);
				return *this;
			}
			auto operator=( HashEqual&& other) noexcept(std::is_nothrow_move_assignable<Hash>::value && std::is_nothrow_move_assignable<Equal>::value)
				-> HashEqual& {
				static_cast<Hash&>(*this) = std::move(static_cast<Hash&>(other));
				static_cast<Equal&>(*this) = std::move(static_cast<Equal&>(other));
				return *this;
			}

			void swap(HashEqual& other)
				noexcept(std::is_nothrow_move_assignable<Hash>::value&& std::is_nothrow_move_assignable<Equal>::value&&
					std::is_nothrow_move_constructible<Hash>::value&& std::is_nothrow_move_constructible<Equal>::value) 
			{
				std::swap(static_cast<Hash&>(*this), static_cast<Hash&>(other));
				std::swap(static_cast<Equal&>(*this), static_cast<Equal&>(other));
			}

			auto hash_function() const noexcept -> const Hash&{ return (*this); }
			auto key_eq() const noexcept -> const Equal& { return (*this); }

			template< class... Args >
			auto hash(Args&&... args) const noexcept(noexcept(std::declval<Hash&>().operator()(std::forward<Args>(args)...))) -> size_t 
			{ return (Hash::operator()(std::forward<Args>(args)...)); }
			template< class... Args >
			bool operator()(Args&&... args) const noexcept(noexcept(std::declval<Equal&>().operator()(std::forward<Args>(args)...))) 
			{ return Equal::operator()(std::forward<Args>(args)...); }
		};
		

	}
}


#endif
