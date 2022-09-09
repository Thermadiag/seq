// Copyright (c) 2021, Matthew Bentley (mattreecebentley@gmail.com) www.plflib.org

// zLib license (https://www.zlib.net/zlib_license.html):
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
// 	claim that you wrote the original software. If you use this software
// 	in a product, an acknowledgement in the product documentation would be
// 	appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
// 	misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.


#ifndef PLF_LIST_H
#define PLF_LIST_H


#define PLF_BLOCK_MIN static_cast<group_size_type>((sizeof(node) * 8 > (sizeof(*this) + sizeof(group)) * 2) ? 8 : (((sizeof(*this) + sizeof(group)) * 2) / sizeof(node)) + 1)
#define PLF_BLOCK_MAX 2048


// Compiler-specific defines:

#if defined(_MSC_VER) && !defined(__clang__) && !defined(__GNUC__)
#define PLF_FORCE_INLINE __forceinline

#if _MSC_VER >= 1600
#define PLF_MOVE_SEMANTICS_SUPPORT
#define PLF_STATIC_ASSERT(check, message) static_assert(check, message)
#else
#define PLF_STATIC_ASSERT(check, message) assert(check)
#endif
#if _MSC_VER >= 1700
#define PLF_TYPE_TRAITS_SUPPORT
#define PLF_ALLOCATOR_TRAITS_SUPPORT
#endif
#if _MSC_VER >= 1800
#define PLF_VARIADICS_SUPPORT // Variadics, in this context, means both variadic templates and variadic macros are supported
#define PLF_INITIALIZER_LIST_SUPPORT
#endif
#if _MSC_VER >= 1900
#define PLF_ALIGNMENT_SUPPORT
#define PLF_NOEXCEPT noexcept
#define PLF_IS_ALWAYS_EQUAL_SUPPORT
#else
#define PLF_NOEXCEPT throw()
#endif

#if defined(_MSVC_LANG) && (_MSVC_LANG >= 201703L)
#define PLF_CONSTEXPR constexpr
#else
#define PLF_CONSTEXPR
#endif

#if defined(_MSVC_LANG) && (_MSVC_LANG > 201703L) && _MSC_VER >= 1923
#define PLF_CPP20_SUPPORT
#endif
#elif defined(__cplusplus) && __cplusplus >= 201103L // C++11 support, at least
#define PLF_FORCE_INLINE // note: GCC and clang create faster code without forcing inline

#if defined(__GNUC__) && defined(__GNUC_MINOR__) && !defined(__clang__) // If compiler is GCC/G++
#if (__GNUC__ == 4 && __GNUC_MINOR__ >= 3) || __GNUC__ > 4 // 4.2 and below do not support variadic templates
#define PLF_MOVE_SEMANTICS_SUPPORT
#define PLF_VARIADICS_SUPPORT
#define PLF_STATIC_ASSERT(check, message) static_assert(check, message)
#else
#define PLF_STATIC_ASSERT(check, message) assert(check)
#endif
#if (__GNUC__ == 4 && __GNUC_MINOR__ >= 4) || __GNUC__ > 4 // 4.3 and below do not support initializer lists
#define PLF_INITIALIZER_LIST_SUPPORT
#endif
#if (__GNUC__ == 4 && __GNUC_MINOR__ >= 6) || __GNUC__ > 4
#define PLF_NOEXCEPT noexcept
#else
#define PLF_NOEXCEPT throw()
#endif
#if (__GNUC__ == 4 && __GNUC_MINOR__ >= 7) || __GNUC__ > 4
#define PLF_ALLOCATOR_TRAITS_SUPPORT
#endif
#if (__GNUC__ == 4 && __GNUC_MINOR__ >= 8) || __GNUC__ > 4
#define PLF_ALIGNMENT_SUPPORT
#endif
#if __GNUC__ >= 5 // GCC v4.9 and below do not support std::is_trivially_copyable
#define PLF_TYPE_TRAITS_SUPPORT
#endif
#if __GNUC__ > 6
#define PLF_IS_ALWAYS_EQUAL_SUPPORT
#endif
#elif defined(__clang__) && !defined(__GLIBCXX__) && !defined(_LIBCPP_CXX03_LANG)
#if __clang_major__ >= 3 // clang versions < 3 don't support __has_feature() or traits
#define PLF_ALLOCATOR_TRAITS_SUPPORT
#define PLF_TYPE_TRAITS_SUPPORT

#if __has_feature(cxx_alignas) && __has_feature(cxx_alignof)
#define PLF_ALIGNMENT_SUPPORT
#endif
#if __has_feature(cxx_noexcept)
#define PLF_NOEXCEPT noexcept
#define PLF_IS_ALWAYS_EQUAL_SUPPORT
#else
#define PLF_NOEXCEPT throw()
#endif
#if __has_feature(cxx_rvalue_references) && !defined(_LIBCPP_HAS_NO_RVALUE_REFERENCES)
#define PLF_MOVE_SEMANTICS_SUPPORT
#endif
#if __has_feature(cxx_static_assert)
#define PLF_STATIC_ASSERT(check, message) static_assert(check, message)
#else
#define PLF_STATIC_ASSERT(check, message) assert(check)
#endif
#if __has_feature(cxx_variadic_templates) && !defined(_LIBCPP_HAS_NO_VARIADICS)
#define PLF_VARIADICS_SUPPORT
#endif
#if (__clang_major__ == 3 && __clang_minor__ >= 1) || __clang_major__ > 3
#define PLF_INITIALIZER_LIST_SUPPORT
#endif
#endif
#elif defined(__GLIBCXX__) // Using another compiler type with libstdc++ - we are assuming full c++11 compliance for compiler - which may not be true
#if __GLIBCXX__ >= 20080606
#define PLF_MOVE_SEMANTICS_SUPPORT
#define PLF_VARIADICS_SUPPORT
#define PLF_STATIC_ASSERT(check, message) static_assert(check, message)
#else
#define PLF_STATIC_ASSERT(check, message) assert(check)
#endif
#if __GLIBCXX__ >= 20090421
#define PLF_INITIALIZER_LIST_SUPPORT
#endif
#if __GLIBCXX__ >= 20120322
#define PLF_ALLOCATOR_TRAITS_SUPPORT
#define PLF_NOEXCEPT noexcept
#else
#define PLF_NOEXCEPT throw()
#endif
#if __GLIBCXX__ >= 20130322
#define PLF_ALIGNMENT_SUPPORT
#endif
#if __GLIBCXX__ >= 20150422 // libstdc++ v4.9 and below do not support std::is_trivially_copyable
#define PLF_TYPE_TRAITS_SUPPORT
#endif
#if __GLIBCXX__ >= 20160111
#define PLF_IS_ALWAYS_EQUAL_SUPPORT
#endif
#elif defined(_LIBCPP_CXX03_LANG) || defined(_LIBCPP_HAS_NO_RVALUE_REFERENCES) // Special case for checking C++11 support with libCPP
#define PLF_STATIC_ASSERT(check, message) assert(check)
#define PLF_NOEXCEPT throw()
#if !defined(_LIBCPP_HAS_NO_VARIADICS)
#define PLF_VARIADICS_SUPPORT
#endif
#else // Assume type traits and initializer support for other compilers and standard library implementations
#define PLF_MOVE_SEMANTICS_SUPPORT
#define PLF_STATIC_ASSERT(check, message) static_assert(check, message)
#define PLF_VARIADICS_SUPPORT
#define PLF_TYPE_TRAITS_SUPPORT
#define PLF_ALLOCATOR_TRAITS_SUPPORT
#define PLF_ALIGNMENT_SUPPORT
#define PLF_INITIALIZER_LIST_SUPPORT
#define PLF_NOEXCEPT noexcept
#define PLF_IS_ALWAYS_EQUAL_SUPPORT
#endif

#if __cplusplus >= 201703L && ((defined(__clang__) && ((__clang_major__ == 3 && __clang_minor__ == 9) || __clang_major__ > 3)) || (defined(__GNUC__) && __GNUC__ >= 7) || (!defined(__clang__) && !defined(__GNUC__))) // assume correct C++17 implementation for non-gcc/clang compilers
#define PLF_CONSTEXPR constexpr
#else
#define PLF_CONSTEXPR
#endif

#if __cplusplus > 201703L && ((defined(__clang__) && (__clang_major__ >= 13)) || (defined(__GNUC__) && __GNUC__ >= 10) || (!defined(__clang__) && !defined(__GNUC__)))
#define PLF_CPP20_SUPPORT
#endif
#else
#define PLF_FORCE_INLINE
#define PLF_STATIC_ASSERT(check, message) assert(check)
#define PLF_NOEXCEPT throw()
#define PLF_CONSTEXPR
#endif

#if defined(PLF_IS_ALWAYS_EQUAL_SUPPORT) && defined(PLF_MOVE_SEMANTICS_SUPPORT) && defined(PLF_ALLOCATOR_TRAITS_SUPPORT) && (__cplusplus >= 201703L || (defined(_MSVC_LANG) && (_MSVC_LANG >= 201703L)))
#define PLF_NOEXCEPT_MOVE_ASSIGN(the_allocator) noexcept(std::allocator_traits<the_allocator>::propagate_on_container_move_assignment::value || std::allocator_traits<the_allocator>::is_always_equal::value)
#define PLF_NOEXCEPT_SWAP(the_allocator) noexcept(std::allocator_traits<the_allocator>::propagate_on_container_swap::value || std::allocator_traits<the_allocator>::is_always_equal::value)
#else
#define PLF_NOEXCEPT_MOVE_ASSIGN(the_allocator)
#define PLF_NOEXCEPT_SWAP(the_allocator)
#endif

#undef PLF_IS_ALWAYS_EQUAL_SUPPORT


#ifdef PLF_ALLOCATOR_TRAITS_SUPPORT
#ifdef PLF_VARIADICS_SUPPORT
#define PLF_CONSTRUCT(the_allocator, allocator_instance, location, ...)	std::allocator_traits<the_allocator>::construct(allocator_instance, location, __VA_ARGS__)
#else
#define PLF_CONSTRUCT(the_allocator, allocator_instance, location, data)	std::allocator_traits<the_allocator>::construct(allocator_instance, location, data)
#endif

#define PLF_DESTROY(the_allocator, allocator_instance, location) 				std::allocator_traits<the_allocator>::destroy(allocator_instance, location)
#define PLF_ALLOCATE(the_allocator, allocator_instance, size, hint) 			std::allocator_traits<the_allocator>::allocate(allocator_instance, size, hint)
#define PLF_DEALLOCATE(the_allocator, allocator_instance, location, size) 	std::allocator_traits<the_allocator>::deallocate(allocator_instance, location, size)
#else
#ifdef PLF_VARIADICS_SUPPORT
#define PLF_CONSTRUCT(the_allocator, allocator_instance, location, ...)		(allocator_instance).construct(location, __VA_ARGS__)
#else
#define PLF_CONSTRUCT(the_allocator, allocator_instance, location, data)	(allocator_instance).construct(location, data)
#endif

#define PLF_DESTROY(the_allocator, allocator_instance, location) 				(allocator_instance).destroy(location)
#define PLF_ALLOCATE(the_allocator, allocator_instance, size, hint)	 		(allocator_instance).allocate(size, hint)
#define PLF_DEALLOCATE(the_allocator, allocator_instance, location, size)	(allocator_instance).deallocate(location, size)
#endif




#include <cstring>	// memmove, memcpy
#include <cassert>	// assert
#include <limits>  	// std::numeric_limits
#include <memory>		// std::uninitialized_copy, std::allocator
#include <iterator> 	// std::bidirectional_iterator_tag, iterator_traits, make_move_iterator, std::distance for range insert
#include <stdexcept> // std::length_error


#ifndef PLF_SORT_FUNCTION
#include <algorithm> // std::sort
#endif

#ifdef PLF_TYPE_TRAITS_SUPPORT
#include <type_traits> // std::is_trivially_destructible, etc
#endif

#ifdef PLF_MOVE_SEMANTICS_SUPPORT
#include <utility> // std::move
#endif

#ifdef PLF_INITIALIZER_LIST_SUPPORT
#include <initializer_list>
#endif




namespace plf
{



	template <class element_type, class element_allocator_type = std::allocator<element_type> > class list : private element_allocator_type
	{
	public:
		// Standard container typedefs:
		typedef element_type															value_type;
		typedef element_allocator_type											allocator_type;
		typedef unsigned short														group_size_type;

#ifdef PLF_ALLOCATOR_TRAITS_SUPPORT // >= C++11
		typedef typename std::allocator_traits<element_allocator_type>::size_type			size_type;
		typedef typename std::allocator_traits<element_allocator_type>::difference_type 	difference_type;
		typedef element_type& reference;
		typedef const element_type& const_reference;
		typedef typename std::allocator_traits<element_allocator_type>::pointer 			pointer;
		typedef typename std::allocator_traits<element_allocator_type>::const_pointer		const_pointer;
#else
		typedef typename element_allocator_type::size_type			size_type;
		typedef typename element_allocator_type::difference_type	difference_type;
		typedef typename element_allocator_type::reference			reference;
		typedef typename element_allocator_type::const_reference	const_reference;
		typedef typename element_allocator_type::pointer			pointer;
		typedef typename element_allocator_type::const_pointer	const_pointer;
#endif


		// Iterator declarations:
		template <bool is_const> class list_iterator;
		typedef list_iterator<false>		iterator;
		typedef list_iterator<true>			const_iterator;
		friend class list_iterator<false>; // Using 'iterator' typedef name here is illegal under C++03
		friend class list_iterator<true>;

		template <bool is_const> class list_reverse_iterator;
		typedef list_reverse_iterator<false>		reverse_iterator;
		typedef list_reverse_iterator<true>			const_reverse_iterator;
		friend class list_reverse_iterator<false>;
		friend class list_reverse_iterator<true>;


	private:
		struct group; // forward declarations for typedefs below
		struct node;

#ifdef PLF_ALLOCATOR_TRAITS_SUPPORT // >= C++11
		typedef typename std::allocator_traits<element_allocator_type>::template rebind_alloc<group>				group_allocator_type;
		typedef typename std::allocator_traits<element_allocator_type>::template rebind_alloc<node>					node_allocator_type;
		typedef typename std::allocator_traits<group_allocator_type>::pointer 				group_pointer_type;
		typedef typename std::allocator_traits<node_allocator_type>::pointer				node_pointer_type;
		typedef typename std::allocator_traits<element_allocator_type>::template rebind_alloc<node_pointer_type>	node_pointer_allocator_type;
#else
		typedef typename element_allocator_type::template rebind<group>::other				group_allocator_type;
		typedef typename element_allocator_type::template rebind<node>::other				node_allocator_type;
		typedef typename group_allocator_type::pointer 				group_pointer_type;
		typedef typename node_allocator_type::pointer				node_pointer_type;
		typedef typename element_allocator_type::template rebind<node_pointer_type>::other	node_pointer_allocator_type;
#endif



		struct node_base
		{
			node_pointer_type next, previous;

			node_base() PLF_NOEXCEPT
			{}

			node_base(const node_pointer_type& n, const node_pointer_type& p) PLF_NOEXCEPT:
			next(n),
				previous(p)
			{}


#ifdef PLF_MOVE_SEMANTICS_SUPPORT
			node_base(node_pointer_type&& n, node_pointer_type&& p) PLF_NOEXCEPT:
			next(std::move(n)),
				previous(std::move(p))
			{}
#endif
		};



		struct node : public node_base
		{
			element_type element;

			node(const node_pointer_type next, const node_pointer_type previous, const element_type& source) :
				node_base(next, previous),
				element(source)
			{}


#ifdef PLF_MOVE_SEMANTICS_SUPPORT
			node(node_pointer_type&& next, node_pointer_type&& previous, element_type&& source) PLF_NOEXCEPT:
			node_base(std::move(next), std::move(previous)),
				element(std::move(source))
			{}
#endif


#ifdef PLF_VARIADICS_SUPPORT
			template<typename... arguments>
			node(node_pointer_type const next, node_pointer_type const previous, arguments&&... parameters) :
				node_base(next, previous),
				element(std::forward<arguments>(parameters) ...)
			{}
#endif
		};



		struct group : public node_allocator_type // Node memory block + metadata
		{
			node_pointer_type nodes;
			node_pointer_type free_list_head;
			node_pointer_type beyond_end;
			group_size_type number_of_elements;


			group() PLF_NOEXCEPT:
			nodes(NULL),
				free_list_head(NULL),
				beyond_end(NULL),
				number_of_elements(0)
			{}


#if defined(PLF_VARIADICS_SUPPORT) || defined(PLF_MOVE_SEMANTICS_SUPPORT)
			group(const group_size_type group_size, node_pointer_type const previous = NULL) :
				nodes(PLF_ALLOCATE(node_allocator_type, *this, group_size, previous)),
				free_list_head(NULL),
				beyond_end(nodes + group_size),
				number_of_elements(0)
			{}
#else
			// This is a hack around the fact that allocator_type::construct only supports copy construction in C++03 and copy elision does not occur on the vast majority of compilers in this circumstance. And to avoid running out of memory (and performance loss) from allocating the same block twice, we're allocating in this constructor and moving data in the copy constructor.
			group(const group_size_type group_size, node_pointer_type const previous = NULL) PLF_NOEXCEPT:
			nodes(NULL),
				free_list_head(previous),
				beyond_end(NULL),
				number_of_elements(group_size)
			{}

			// Not a real copy constructor ie. actually a move constructor. Only used for allocator.construct in C++03 for reasons stated above:
			group(const group& source) :
				node_allocator_type(source),
				nodes(PLF_ALLOCATE(node_allocator_type, *this, source.number_of_elements, source.free_list_head)),
				free_list_head(NULL),
				beyond_end(nodes + source.number_of_elements),
				number_of_elements(0)
			{}
#endif


			group& operator = (const group& source) PLF_NOEXCEPT // Actually a move operator, used by c++03 in group_vector's remove, expand_capacity and append
			{
				nodes = source.nodes;
				free_list_head = source.free_list_head;
				beyond_end = source.beyond_end;
				number_of_elements = source.number_of_elements;
				return *this;
			}


#ifdef PLF_MOVE_SEMANTICS_SUPPORT
			group(group&& source) PLF_NOEXCEPT:
			node_allocator_type(source),
				nodes(std::move(source.nodes)),
				free_list_head(std::move(source.free_list_head)),
				beyond_end(std::move(source.beyond_end)),
				number_of_elements(source.number_of_elements)
			{
				source.nodes = NULL;
				source.beyond_end = NULL;
			}


			group& operator = (group&& source) PLF_NOEXCEPT
			{
				nodes = std::move(source.nodes);
				free_list_head = std::move(source.free_list_head);
				beyond_end = std::move(source.beyond_end);
				number_of_elements = std::move(source.number_of_elements);
				source.nodes = NULL;
				source.beyond_end = NULL;
				return *this;
			}
#endif


			~group() PLF_NOEXCEPT
			{
				PLF_DEALLOCATE(node_allocator_type, *this, nodes, static_cast<size_type>(beyond_end - nodes));
			}
		};




		class group_vector : private node_pointer_allocator_type // Simple vector of groups + associated functions
		{
		public:
			group_pointer_type last_endpoint_group, block_pointer, last_searched_group; // last_endpoint_group is the last -active- group in the block. Other -inactive- (previously used, now empty of elements) groups may be stored after this group for future usage (to reduce deallocation/reallocation of nodes). block_pointer + size - 1 == the last group in the block, regardless of whether or not the group is active.
			size_type size;


			struct ebco_pair2 : allocator_type // empty-base-class optimisation
			{
				size_type capacity; // Total element capacity of all initialized groups
				explicit ebco_pair2(const size_type number_of_elements) PLF_NOEXCEPT: capacity(number_of_elements) {};
			}		element_allocator_pair;

			struct ebco_pair : group_allocator_type
			{
				size_type capacity; // Total group capacity
				explicit ebco_pair(const size_type number_of_groups) PLF_NOEXCEPT: capacity(number_of_groups) {};
			}		group_allocator_pair;



			group_vector() PLF_NOEXCEPT:
			node_pointer_allocator_type(node_pointer_allocator_type()),
				last_endpoint_group(NULL),
				block_pointer(NULL),
				last_searched_group(NULL),
				size(0),
				element_allocator_pair(0),
				group_allocator_pair(0)
			{}



			inline PLF_FORCE_INLINE void blank() PLF_NOEXCEPT
			{
#ifdef PLF_TYPE_TRAITS_SUPPORT
				if PLF_CONSTEXPR(std::is_trivial<group_pointer_type>::value)
				{
					std::memset(static_cast<void*>(this), 0, sizeof(group_vector));
				}
				else
#endif
				{
					last_endpoint_group = NULL;
					block_pointer = NULL;
					last_searched_group = NULL;
					size = 0;
					element_allocator_pair.capacity = 0;
					group_allocator_pair.capacity = 0;
				}
			}



#ifdef PLF_MOVE_SEMANTICS_SUPPORT
			group_vector(group_vector&& source) PLF_NOEXCEPT:
			last_endpoint_group(std::move(source.last_endpoint_group)),
				block_pointer(std::move(source.block_pointer)),
				last_searched_group(std::move(source.last_searched_group)),
				size(source.size),
				element_allocator_pair(source.element_allocator_pair.capacity),
				group_allocator_pair(source.group_allocator_pair.capacity)
			{
				source.blank();
			}


			group_vector& operator = (group_vector&& source) PLF_NOEXCEPT
			{
#ifdef PLF_TYPE_TRAITS_SUPPORT
				if PLF_CONSTEXPR(std::is_trivial<group_pointer_type>::value)
				{
					std::memcpy(static_cast<void*>(this), &source, sizeof(group_vector));
				}
				else
#endif
				{
					last_endpoint_group = std::move(source.last_endpoint_group);
					block_pointer = std::move(source.block_pointer);
					last_searched_group = std::move(source.last_searched_group);
					size = source.size;
					element_allocator_pair.capacity = source.element_allocator_pair.capacity;
					group_allocator_pair.capacity = source.group_allocator_pair.capacity;
				}

				source.blank();
				return *this;
			}
#endif



			void destroy_all_data(const node_pointer_type last_endpoint_node) PLF_NOEXCEPT
			{
				if (block_pointer == NULL)
				{
					return;
				}

#ifdef PLF_TYPE_TRAITS_SUPPORT
				if PLF_CONSTEXPR(!std::is_trivially_destructible<element_type>::value || !std::is_trivially_destructible<node_pointer_type>::value)
#endif
				{
					if (last_endpoint_node != NULL)
					{
						clear(last_endpoint_node);
					}
				}

				const group_pointer_type end_group = block_pointer + size;
				for (group_pointer_type current_group = block_pointer; current_group != end_group; ++current_group)
				{
					PLF_DESTROY(group_allocator_type, group_allocator_pair, current_group);
				}

				PLF_DEALLOCATE(group_allocator_type, group_allocator_pair, block_pointer, group_allocator_pair.capacity);
				blank();
			}



			void clear(const node_pointer_type last_endpoint_node) PLF_NOEXCEPT
			{
				for (group_pointer_type current_group = block_pointer; current_group != last_endpoint_group; ++current_group)
				{
#ifdef PLF_TYPE_TRAITS_SUPPORT
					if PLF_CONSTEXPR(!std::is_trivially_destructible<element_type>::value || !std::is_trivially_destructible<node_pointer_type>::value)
#endif
					{
						const node_pointer_type end = current_group->beyond_end;

						if ((end - current_group->nodes) != current_group->number_of_elements) // If there are erased nodes present in the group
						{
							for (node_pointer_type current_node = current_group->nodes; current_node != end; ++current_node)
							{
#ifdef PLF_TYPE_TRAITS_SUPPORT
								if PLF_CONSTEXPR(!std::is_trivially_destructible<element_type>::value)
#endif
								{
									if (current_node->next != NULL) // ie. is not part of free list
									{
										PLF_DESTROY(element_allocator_type, element_allocator_pair, &(current_node->element));
									}
								}

#ifdef PLF_TYPE_TRAITS_SUPPORT
								if PLF_CONSTEXPR(!std::is_trivially_destructible<node_pointer_type>::value)
#endif
								{
									PLF_DESTROY(node_pointer_allocator_type, *this, &(current_node->next));
									PLF_DESTROY(node_pointer_allocator_type, *this, &(current_node->previous));
								}
							}
						}
						else
						{
							for (node_pointer_type current_node = current_group->nodes; current_node != end; ++current_node)
							{
#ifdef PLF_TYPE_TRAITS_SUPPORT
								if PLF_CONSTEXPR(!std::is_trivially_destructible<element_type>::value)
#endif
								{
									PLF_DESTROY(element_allocator_type, element_allocator_pair, &(current_node->element));
								}

#ifdef PLF_TYPE_TRAITS_SUPPORT
								if PLF_CONSTEXPR(!std::is_trivially_destructible<node_pointer_type>::value)
#endif
								{
									PLF_DESTROY(node_pointer_allocator_type, *this, &(current_node->next));
									PLF_DESTROY(node_pointer_allocator_type, *this, &(current_node->previous));
								}
							}
						}
					}

					current_group->free_list_head = NULL;
					current_group->number_of_elements = 0;
				}

#ifdef PLF_TYPE_TRAITS_SUPPORT
				if PLF_CONSTEXPR(!std::is_trivially_destructible<element_type>::value || !std::is_trivially_destructible<node_pointer_type>::value)
#endif
				{
					if ((last_endpoint_node - last_endpoint_group->nodes) != last_endpoint_group->number_of_elements) // If there are erased nodes present in the group
					{
						for (node_pointer_type current_node = last_endpoint_group->nodes; current_node != last_endpoint_node; ++current_node)
						{
#ifdef PLF_TYPE_TRAITS_SUPPORT
							if PLF_CONSTEXPR(!std::is_trivially_destructible<element_type>::value)
#endif
							{
								if (current_node->next != NULL) // is not part of free list ie. element has not already had it's destructor called
								{
									PLF_DESTROY(element_allocator_type, element_allocator_pair, &(current_node->element));
								}
							}

#ifdef PLF_TYPE_TRAITS_SUPPORT
							if PLF_CONSTEXPR(!std::is_trivially_destructible<node_pointer_type>::value)
#endif
							{
								PLF_DESTROY(node_pointer_allocator_type, *this, &(current_node->next));
								PLF_DESTROY(node_pointer_allocator_type, *this, &(current_node->previous));
							}
						}
					}
					else
					{
						for (node_pointer_type current_node = last_endpoint_group->nodes; current_node != last_endpoint_node; ++current_node)
						{
#ifdef PLF_TYPE_TRAITS_SUPPORT
							if PLF_CONSTEXPR(!std::is_trivially_destructible<element_type>::value)
#endif
							{
								PLF_DESTROY(element_allocator_type, element_allocator_pair, &(current_node->element));
							}

#ifdef PLF_TYPE_TRAITS_SUPPORT
							if PLF_CONSTEXPR(!std::is_trivially_destructible<node_pointer_type>::value)
#endif
							{
								PLF_DESTROY(node_pointer_allocator_type, *this, &(current_node->next));
								PLF_DESTROY(node_pointer_allocator_type, *this, &(current_node->previous));
							}
						}
					}
				}

				last_endpoint_group->free_list_head = NULL;
				last_endpoint_group->number_of_elements = 0;
				last_searched_group = last_endpoint_group = block_pointer;
			}



			void expand_capacity(const size_type new_capacity) // used by add_new and append
			{
				group_pointer_type const old_block = block_pointer;
				block_pointer = PLF_ALLOCATE(group_allocator_type, group_allocator_pair, new_capacity, 0);

#ifdef PLF_TYPE_TRAITS_SUPPORT
				if PLF_CONSTEXPR(std::is_trivially_copyable<node_pointer_type>::value && std::is_trivially_destructible<node_pointer_type>::value)
				{ // Dereferencing here in order to deal with smart pointer situations ie. obtaining the raw pointer from the smart pointer
					std::memcpy(static_cast<void*>(&*block_pointer), static_cast<void*>(&*old_block), sizeof(group) * size); // static_cast or reinterpret_cast necessary to deal with GCC 8 warnings
				}
#ifdef PLF_MOVE_SEMANTICS_SUPPORT
				else if PLF_CONSTEXPR(std::is_move_constructible<node_pointer_type>::value)
				{
					std::uninitialized_copy(std::make_move_iterator(old_block), std::make_move_iterator(old_block + size), block_pointer);
				}
#endif
				else
#endif
				{
					// If allocator supplies non-trivial pointers it becomes necessary to destroy the group. uninitialized_copy will not work in this context as the copy constructor for "group" is overriden in C++03/98. The = operator for "group" has been overriden to make the following work:
					const group_pointer_type beyond_end = old_block + size;
					group_pointer_type current_new_group = block_pointer;

					for (group_pointer_type current_group = old_block; current_group != beyond_end; ++current_group)
					{
						*(current_new_group++) = *(current_group);

						current_group->nodes = NULL;
						current_group->beyond_end = NULL;
						PLF_DESTROY(group_allocator_type, group_allocator_pair, current_group);
					}
				}

				last_searched_group = block_pointer + (last_searched_group - old_block); // correct pointer post-reallocation
				PLF_DEALLOCATE(group_allocator_type, group_allocator_pair, old_block, group_allocator_pair.capacity);
				group_allocator_pair.capacity = new_capacity;
			}



			void add_new(const group_size_type group_size)
			{
				if (group_allocator_pair.capacity == size)
				{
					expand_capacity(group_allocator_pair.capacity * 2);
				}

				last_endpoint_group = block_pointer + size - 1;

#ifdef PLF_VARIADICS_SUPPORT
				PLF_CONSTRUCT(group_allocator_type, group_allocator_pair, last_endpoint_group + 1, group_size, last_endpoint_group->nodes);
#else
				PLF_CONSTRUCT(group_allocator_type, group_allocator_pair, last_endpoint_group + 1, group(group_size, last_endpoint_group->nodes));
#endif

				++last_endpoint_group; // Doing this here instead of pre-construct to avoid need for a try-catch block
				element_allocator_pair.capacity += group_size;
				++size;
			}



			void initialize(const group_size_type group_size) // For adding first group *only* when group vector is completely empty and block_pointer is NULL
			{
				last_endpoint_group = block_pointer = last_searched_group = PLF_ALLOCATE(group_allocator_type, group_allocator_pair, 1, 0);
				group_allocator_pair.capacity = 1;

#ifdef PLF_VARIADICS_SUPPORT
				PLF_CONSTRUCT(group_allocator_type, group_allocator_pair, last_endpoint_group, group_size);
#else
				PLF_CONSTRUCT(group_allocator_type, group_allocator_pair, last_endpoint_group, group(group_size));
#endif

				size = 1; // Doing these here instead of pre-construct to avoid need for a try-catch block
				element_allocator_pair.capacity = group_size;
			}



			void remove(group_pointer_type const group_to_erase) PLF_NOEXCEPT
			{
				if (last_searched_group >= group_to_erase && last_searched_group != block_pointer)
				{
					--last_searched_group;
				}

				element_allocator_pair.capacity -= static_cast<size_type>(group_to_erase->beyond_end - group_to_erase->nodes);

				PLF_DESTROY(group_allocator_type, group_allocator_pair, group_to_erase);

#ifdef PLF_TYPE_TRAITS_SUPPORT
				if PLF_CONSTEXPR(std::is_trivially_copyable<node_pointer_type>::value && std::is_trivially_destructible<node_pointer_type>::value)
				{ // Dereferencing here in order to deal with smart pointer situations ie. obtaining the raw pointer from the smart pointer
					std::memmove(static_cast<void*>(&*group_to_erase), static_cast<void*>(&*group_to_erase + 1), sizeof(group) * (--size - static_cast<size_type>(&*group_to_erase - &*block_pointer)));
				}
#ifdef PLF_MOVE_SEMANTICS_SUPPORT
				else if PLF_CONSTEXPR(std::is_move_constructible<node_pointer_type>::value)
				{
					std::move(group_to_erase + 1, block_pointer + size--, group_to_erase);
				}
#endif
				else
#endif
				{
					group_pointer_type back = block_pointer + size--;
					std::copy(group_to_erase + 1, back--, group_to_erase);

					back->nodes = NULL;
					back->beyond_end = NULL;
					PLF_DESTROY(group_allocator_type, group_allocator_pair, back);
				}
			}



			void move_to_back(group_pointer_type const group_to_erase)
			{
				if (last_searched_group >= group_to_erase && last_searched_group != block_pointer)
				{
					--last_searched_group;
				}

				group* temp_group = PLF_ALLOCATE(group_allocator_type, group_allocator_pair, 1, NULL);

#ifdef PLF_TYPE_TRAITS_SUPPORT
				if PLF_CONSTEXPR(std::is_trivially_copyable<node_pointer_type>::value && std::is_trivially_destructible<node_pointer_type>::value)
				{
					std::memcpy(static_cast<void*>(&*temp_group), static_cast<void*>(&*group_to_erase), sizeof(group));
					std::memmove(static_cast<void*>(&*group_to_erase), static_cast<void*>(&*group_to_erase + 1), sizeof(group) * ((size - 1) - static_cast<size_type>(&*group_to_erase - &*block_pointer)));
					std::memcpy(static_cast<void*>(&*(block_pointer + size - 1)), static_cast<void*>(&*temp_group), sizeof(group));
				}
#ifdef PLF_MOVE_SEMANTICS_SUPPORT
				else if PLF_CONSTEXPR(std::is_move_constructible<node_pointer_type>::value)
				{
					PLF_CONSTRUCT(group_allocator_type, group_allocator_pair, temp_group, std::move(*group_to_erase));
					std::move(group_to_erase + 1, block_pointer + size, group_to_erase);
					*(block_pointer + size - 1) = std::move(*temp_group);

					if PLF_CONSTEXPR(!std::is_trivially_destructible<node_pointer_type>::value)
					{
						PLF_DESTROY(group_allocator_type, group_allocator_pair, temp_group);
					}
				}
#endif
				else
#endif
				{
					PLF_CONSTRUCT(group_allocator_type, group_allocator_pair, temp_group, group());

					*temp_group = *group_to_erase;
					std::copy(group_to_erase + 1, block_pointer + size, group_to_erase);
					*(block_pointer + --size) = *temp_group;

					temp_group->nodes = NULL;
					PLF_DESTROY(group_allocator_type, group_allocator_pair, temp_group);
				}

				PLF_DEALLOCATE(group_allocator_type, group_allocator_pair, temp_group, 1);
			}



			group_pointer_type get_nearest_freelist_group(const node_pointer_type location_node) PLF_NOEXCEPT // In working implementation this cannot throw
			{
				const group_pointer_type beyond_end_group = last_endpoint_group + 1;
				group_pointer_type left = last_searched_group - 1, right = last_searched_group + 1, freelist_group = NULL;
				bool right_not_beyond_back = (right < beyond_end_group);
				bool left_not_beyond_front = (left >= block_pointer);


				if (location_node >= last_searched_group->nodes && location_node < last_searched_group->beyond_end) // ie. location is within last_search_group
				{
					if (last_searched_group->free_list_head != NULL) // if last_searched_group has previously-erased nodes
					{
						return last_searched_group;
					}
				}
				else // search for the node group which location_node is located within, using last_searched_group as a starting point and searching left and right. Try and find the closest node group with reusable erased-element locations along the way:
				{
					group_pointer_type closest_freelist_left = (last_searched_group->free_list_head == NULL) ? NULL : last_searched_group, closest_freelist_right = (last_searched_group->free_list_head == NULL) ? NULL : last_searched_group;

					while (true)
					{
						if (right_not_beyond_back)
						{
							if ((location_node < right->beyond_end) && (location_node >= right->nodes)) // location_node's group is found
							{
								if (right->free_list_head != NULL) // group has erased nodes, reuse them:
								{
									last_searched_group = right;
									return right;
								}

								difference_type left_distance;

								if (closest_freelist_right != NULL)
								{
									last_searched_group = right;
									left_distance = right - closest_freelist_right;

									if (left_distance <= 2) // ie. this group is close enough to location_node's group
									{
										return closest_freelist_right;
									}

									freelist_group = closest_freelist_right;
								}
								else
								{
									last_searched_group = right;
									left_distance = right - left;
								}


								// Otherwise find closest group with freelist - check an equal distance on the right to the distance we've checked on the left:
								const group_pointer_type end_group = (((right + left_distance) > beyond_end_group) ? beyond_end_group : (right + left_distance - 1));

								while (++right != end_group)
								{
									if (right->free_list_head != NULL)
									{
										return right;
									}
								}

								if (freelist_group != NULL)
								{
									return freelist_group;
								}

								right_not_beyond_back = (right < beyond_end_group);
								break; // group with reusable erased nodes not found yet, continue searching in loop below
							}

							if (right->free_list_head != NULL) // location_node's group not found, but a reusable location found
							{
								if ((closest_freelist_right == NULL) & (closest_freelist_left == NULL))
								{
									closest_freelist_left = right;
								}

								closest_freelist_right = right;
							}

							right_not_beyond_back = (++right < beyond_end_group);
						}


						if (left_not_beyond_front)
						{
							if ((location_node >= left->nodes) && (location_node < left->beyond_end))
							{
								if (left->free_list_head != NULL)
								{
									last_searched_group = left;
									return left;
								}

								difference_type right_distance;

								if (closest_freelist_left != NULL)
								{
									last_searched_group = left;
									right_distance = closest_freelist_left - left;

									if (right_distance <= 2)
									{
										return closest_freelist_left;
									}

									freelist_group = closest_freelist_left;
								}
								else
								{
									last_searched_group = left;
									right_distance = right - left;
								}

								// Otherwise find closest group with freelist:
								const group_pointer_type end_group = (((left - right_distance) < block_pointer) ? block_pointer - 1 : (left - right_distance) + 1);

								while (--left != end_group)
								{
									if (left->free_list_head != NULL)
									{
										return left;
									}
								}

								if (freelist_group != NULL)
								{
									return freelist_group;
								}

								left_not_beyond_front = (left >= block_pointer);
								break;
							}

							if (left->free_list_head != NULL)
							{
								if ((closest_freelist_left == NULL) & (closest_freelist_right == NULL))
								{
									closest_freelist_right = left;
								}

								closest_freelist_left = left;
							}

							left_not_beyond_front = (--left >= block_pointer);
						}
					}
				}


				// The node group which location_node is located within, is known at this point. Continue searching outwards from this group until a group is found with a reusable location:
				while (true)
				{
					if (right_not_beyond_back)
					{
						if (right->free_list_head != NULL)
						{
							return right;
						}

						right_not_beyond_back = (++right < beyond_end_group);
					}

					if (left_not_beyond_front)
					{
						if (left->free_list_head != NULL)
						{
							return left;
						}

						left_not_beyond_front = (--left >= block_pointer);
					}
				}

				// Will never reach here on a functioning implementation
			}



			void swap(group_vector& source) PLF_NOEXCEPT_SWAP(group_allocator_type)
			{
#ifdef PLF_TYPE_TRAITS_SUPPORT
				if PLF_CONSTEXPR(std::is_trivial<group_pointer_type>::value) // if pointer type is trivial we can just copy using memcpy - faster - avoids constructors/destructors etc
				{
					char temp[sizeof(group_vector)];
					std::memcpy(static_cast<void*>(&temp), static_cast<void*>(this), sizeof(group_vector));
					std::memcpy(static_cast<void*>(this), static_cast<void*>(&source), sizeof(group_vector));
					std::memcpy(static_cast<void*>(&source), static_cast<void*>(&temp), sizeof(group_vector));
				}
				else
#endif
				{
					const group_pointer_type swap_last_endpoint_group = last_endpoint_group, swap_block_pointer = block_pointer, swap_last_searched_group = last_searched_group;
					const size_type swap_size = size, swap_element_capacity = element_allocator_pair.capacity, swap_capacity = group_allocator_pair.capacity;

					last_endpoint_group = source.last_endpoint_group;
					block_pointer = source.block_pointer;
					last_searched_group = source.last_searched_group;
					size = source.size;
					element_allocator_pair.capacity = source.element_allocator_pair.capacity;
					group_allocator_pair.capacity = source.group_allocator_pair.capacity;

					source.last_endpoint_group = swap_last_endpoint_group;
					source.block_pointer = swap_block_pointer;
					source.last_searched_group = swap_last_searched_group;
					source.size = swap_size;
					source.element_allocator_pair.capacity = swap_element_capacity;
					source.group_allocator_pair.capacity = swap_capacity;
				}
			}



			void trim_unused_groups() PLF_NOEXCEPT // trim trailing groups previously allocated by reserve() or retained via erase()
			{
				const group_pointer_type beyond_last = block_pointer + size;

				for (group_pointer_type current_group = last_endpoint_group + 1; current_group != beyond_last; ++current_group)
				{
					element_allocator_pair.capacity -= static_cast<size_type>(current_group->beyond_end - current_group->nodes);
					PLF_DESTROY(group_allocator_type, group_allocator_pair, current_group);
				}

				size -= static_cast<size_type>(beyond_last - (last_endpoint_group + 1));
			}



			void append(group_vector& source)
			{
				source.trim_unused_groups();
				trim_unused_groups();

				if (size + source.size > group_allocator_pair.capacity)
				{
					expand_capacity(size + source.size);
				}

#ifdef PLF_TYPE_TRAITS_SUPPORT
				if PLF_CONSTEXPR(std::is_trivially_copyable<node_pointer_type>::value && std::is_trivially_destructible<node_pointer_type>::value)
				{ // &* in order to deal with smart pointer situations ie. obtaining the raw pointer from the smart pointer
					std::memcpy(static_cast<void*>(&*block_pointer + size), static_cast<void*>(&*source.block_pointer), sizeof(group) * source.size);
				}
#ifdef PLF_MOVE_SEMANTICS_SUPPORT
				else if PLF_CONSTEXPR(std::is_move_constructible<node_pointer_type>::value)
				{
					std::uninitialized_copy(std::make_move_iterator(source.block_pointer), std::make_move_iterator(source.block_pointer + source.size), block_pointer + size);
				}
#endif
				else
#endif
				{
					group_pointer_type current_new_group = block_pointer + size;
					const group_pointer_type beyond_end_source = source.block_pointer + source.size;

					for (group_pointer_type current_group = source.block_pointer; current_group != beyond_end_source; ++current_group)
					{
						*(current_new_group++) = *(current_group);

						current_group->nodes = NULL;
						current_group->beyond_end = NULL;
						PLF_DESTROY(group_allocator_type, source.group_allocator_pair, current_group);
					}
				}

				PLF_DEALLOCATE(group_allocator_type, source.group_allocator_pair, source.block_pointer, source.group_allocator_pair.capacity);
				size += source.size;
				last_endpoint_group = block_pointer + size - 1;
				element_allocator_pair.capacity += source.element_allocator_pair.capacity;
				source.blank();
			}
		};



		// Implement const/non-const iterator switching pattern:
		template <bool flag, class IsTrue, class IsFalse> struct choose;

		template <class IsTrue, class IsFalse> struct choose<true, IsTrue, IsFalse>
		{
			typedef IsTrue type;
		};

		template <class IsTrue, class IsFalse> struct choose<false, IsTrue, IsFalse>
		{
			typedef IsFalse type;
		};


	public:

		template <bool is_const> class list_iterator
		{
		private:
			node_pointer_type node_pointer;

		public:
			typedef std::bidirectional_iterator_tag 	iterator_category;
			typedef typename list::value_type 			value_type;
			typedef typename list::difference_type 		difference_type;
			typedef typename choose<is_const, typename list::const_pointer, typename list::pointer>::type		pointer;
			typedef typename choose<is_const, typename list::const_reference, typename list::reference>::type	reference;

			friend class list;



			inline PLF_FORCE_INLINE bool operator == (const list_iterator rh) const PLF_NOEXCEPT
			{
				return (node_pointer == rh.node_pointer);
			}



			inline PLF_FORCE_INLINE bool operator == (const list_iterator<!is_const> rh) const PLF_NOEXCEPT
			{
				return (node_pointer == rh.node_pointer);
			}



			inline PLF_FORCE_INLINE bool operator != (const list_iterator rh) const PLF_NOEXCEPT
			{
				return (node_pointer != rh.node_pointer);
			}



			inline PLF_FORCE_INLINE bool operator != (const list_iterator<!is_const> rh) const PLF_NOEXCEPT
			{
				return (node_pointer != rh.node_pointer);
			}



			inline PLF_FORCE_INLINE reference operator * () const
			{
				return node_pointer->element;
			}



			inline PLF_FORCE_INLINE pointer operator -> () const
			{
				return &(node_pointer->element);
			}



			inline PLF_FORCE_INLINE list_iterator& operator ++ () PLF_NOEXCEPT
			{
				assert(node_pointer != NULL); // covers uninitialised list_iterator
				node_pointer = node_pointer->next;
				return *this;
			}



			inline list_iterator operator ++(int) PLF_NOEXCEPT
			{
				const list_iterator copy(*this);
				++* this;
				return copy;
			}



			inline PLF_FORCE_INLINE list_iterator& operator -- () PLF_NOEXCEPT
			{
				assert(node_pointer != NULL); // covers uninitialised list_iterator
				node_pointer = node_pointer->previous;
				return *this;
			}



			inline list_iterator operator -- (int) PLF_NOEXCEPT
			{
				const list_iterator copy(*this);
				--* this;
				return copy;
			}



			inline list_iterator& operator = (const list_iterator& rh) PLF_NOEXCEPT
			{
				node_pointer = rh.node_pointer;
				return *this;
			}



			inline list_iterator& operator = (const list_iterator<!is_const>& rh) PLF_NOEXCEPT
			{
				node_pointer = rh.node_pointer;
				return *this;
			}



#ifdef PLF_MOVE_SEMANTICS_SUPPORT
			inline list_iterator& operator = (const list_iterator&& rh) PLF_NOEXCEPT
			{
				assert(&rh != this);
				node_pointer = std::move(rh.node_pointer);
				return *this;
			}


			inline list_iterator& operator = (const list_iterator<!is_const>&& rh) PLF_NOEXCEPT
			{
				node_pointer = std::move(rh.node_pointer);
				return *this;
			}
#endif



			list_iterator() PLF_NOEXCEPT: node_pointer(NULL) {}

			list_iterator(const list_iterator& source) PLF_NOEXCEPT : node_pointer(source.node_pointer) {}

			list_iterator(const list_iterator<!is_const>& source) PLF_NOEXCEPT : node_pointer(source.node_pointer) {}

#ifdef PLF_MOVE_SEMANTICS_SUPPORT
			list_iterator(const list_iterator&& source) PLF_NOEXCEPT : node_pointer(std::move(source.node_pointer)) {}

			list_iterator(const list_iterator<!is_const>&& source) PLF_NOEXCEPT : node_pointer(std::move(source.node_pointer)) {}
#endif

		private:

			list_iterator(const node_pointer_type node_p) PLF_NOEXCEPT : node_pointer(node_p) {}
		};




		template <bool is_const> class list_reverse_iterator
		{
		private:
			node_pointer_type node_pointer;

		public:
			typedef std::bidirectional_iterator_tag 	iterator_category;
			typedef typename list::value_type 		value_type;
			typedef typename list::difference_type 		difference_type;
			typedef typename choose<is_const, typename list::const_pointer, typename list::pointer>::type		pointer;
			typedef typename choose<is_const, typename list::const_reference, typename list::reference>::type	reference;

			friend class list;


			inline PLF_FORCE_INLINE bool operator == (const list_reverse_iterator rh) const PLF_NOEXCEPT
			{
				return (node_pointer == rh.node_pointer);
			}



			inline PLF_FORCE_INLINE bool operator == (const list_reverse_iterator<!is_const> rh) const PLF_NOEXCEPT
			{
				return (node_pointer == rh.node_pointer);
			}



			inline PLF_FORCE_INLINE bool operator != (const list_reverse_iterator rh) const PLF_NOEXCEPT
			{
				return (node_pointer != rh.node_pointer);
			}



			inline PLF_FORCE_INLINE bool operator != (const list_reverse_iterator<!is_const> rh) const PLF_NOEXCEPT
			{
				return (node_pointer != rh.node_pointer);
			}



			inline PLF_FORCE_INLINE reference operator * () const
			{
				return node_pointer->element;
			}



			inline PLF_FORCE_INLINE pointer operator -> () const
			{
				return &(node_pointer->element);
			}



			inline PLF_FORCE_INLINE list_reverse_iterator& operator ++ () PLF_NOEXCEPT
			{
				assert(node_pointer != NULL); // covers uninitialised list_reverse_iterator
				node_pointer = node_pointer->previous;
				return *this;
			}



			inline list_reverse_iterator operator ++(int) PLF_NOEXCEPT
			{
				const list_reverse_iterator copy(*this);
				++* this;
				return copy;
			}



			inline PLF_FORCE_INLINE list_reverse_iterator& operator -- () PLF_NOEXCEPT
			{
				assert(node_pointer != NULL);
				node_pointer = node_pointer->next;
				return *this;
			}



			inline list_reverse_iterator operator -- (int) PLF_NOEXCEPT
			{
				const list_reverse_iterator copy(*this);
				--* this;
				return copy;
			}



			inline list_reverse_iterator& operator = (const list_reverse_iterator& rh) PLF_NOEXCEPT
			{
				node_pointer = rh.node_pointer;
				return *this;
			}



			inline list_reverse_iterator& operator = (const list_reverse_iterator<!is_const>& rh) PLF_NOEXCEPT
			{
				node_pointer = rh.node_pointer;
				return *this;
			}



#ifdef PLF_MOVE_SEMANTICS_SUPPORT
			inline list_reverse_iterator& operator = (const list_reverse_iterator&& rh) PLF_NOEXCEPT
			{
				assert(&rh != this);
				node_pointer = std::move(rh.node_pointer);
				return *this;
			}


			inline list_reverse_iterator& operator = (const list_reverse_iterator<!is_const>&& rh) PLF_NOEXCEPT
			{
				node_pointer = std::move(rh.node_pointer);
				return *this;
			}
#endif



			inline typename list::iterator base() const PLF_NOEXCEPT
			{
				return typename list::iterator(node_pointer->next);
			}



			list_reverse_iterator() PLF_NOEXCEPT: node_pointer(NULL) {}

			list_reverse_iterator(const list_reverse_iterator& source) PLF_NOEXCEPT : node_pointer(source.node_pointer) {}

#ifdef PLF_MOVE_SEMANTICS_SUPPORT
			list_reverse_iterator(const list_reverse_iterator&& source) PLF_NOEXCEPT : node_pointer(std::move(source.node_pointer)) {}

			list_reverse_iterator(const list_reverse_iterator<!is_const>&& source) PLF_NOEXCEPT : node_pointer(std::move(source.node_pointer)) {}
#endif

		private:

			list_reverse_iterator(const node_pointer_type node_p) PLF_NOEXCEPT : node_pointer(node_p) {}
		};



	private:

		// Used by range-insert and range-constructor to prevent fill-insert and fill-constructor function calls mistakenly resolving to the range insert/constructor
		template <bool condition, class T = void>
		struct plf_enable_if_c
		{
			typedef T type;
		};

		template <class T>
		struct plf_enable_if_c<false, T>
		{};



		group_vector groups; // Structure which contains all groups (structures containing node memory blocks + block metadata)
		node_base end_node; // The independent, content-less node which is returned by end()
		// When the list is empty, the previous and next pointers of end_node both point to end_node.
		node_pointer_type last_endpoint; // The node location which is one-past the last inserted element in the last group of the list. Is not affected by erasures to prior elements (these are handled using the group's freelist).
		// If last_endpoint is beyond the end of a memory block it means a new group must be created upon the next insertion if prior erased nodes are not available for re-use.
		// last_endpoint == NULL means total_size is zero, but there may still be groups available due to calling clear(), reserve() on an empty list, or having erased all elements in the list
		// groups.block_pointer == NULL means an uninitialized container ie. no groups or elements yet
		iterator end_iterator, begin_iterator; // Returned by begin() and end().
		// end_iterator always points to end_node. It is a convenience/optimization variable to save generating many temporary iterators from end_node during functions and during end().
		// When the list is empty of elements, begin_iterator == end_iterator so that program loops iterating from begin() to end() will function as expected.

		struct ebco_pair1 : node_pointer_allocator_type // Packaging the group allocator with least-used member variables, for empty-base-class optimisation
		{
			size_type total_size;
			explicit ebco_pair1(const size_type total_num_elements) PLF_NOEXCEPT: total_size(total_num_elements) {}
		}		node_pointer_allocator_pair;

		struct ebco_pair2 : node_allocator_type
		{
			size_type number_of_erased_nodes;
			explicit ebco_pair2(const size_type num_erased_nodes) PLF_NOEXCEPT: number_of_erased_nodes(num_erased_nodes) {}
		}		node_allocator_pair;



	public:

		// Default constructor:

		list() PLF_NOEXCEPT :
			element_allocator_type(element_allocator_type()),
			end_node(reinterpret_cast<node_pointer_type>(&end_node), reinterpret_cast<node_pointer_type>(&end_node)),
			last_endpoint(NULL),
			end_iterator(reinterpret_cast<node_pointer_type>(&end_node)),
			begin_iterator(reinterpret_cast<node_pointer_type>(&end_node)),
			node_pointer_allocator_pair(0),
			node_allocator_pair(0)
		{}



		// Allocator-extended constructor:

		explicit list(const element_allocator_type& alloc) :
			element_allocator_type(alloc),
			end_node(reinterpret_cast<node_pointer_type>(&end_node), reinterpret_cast<node_pointer_type>(&end_node)),
			last_endpoint(NULL),
			end_iterator(reinterpret_cast<node_pointer_type>(&end_node)),
			begin_iterator(reinterpret_cast<node_pointer_type>(&end_node)),
			node_pointer_allocator_pair(0),
			node_allocator_pair(0)
		{}



		// Copy constructor:

		list(const list& source) :
			element_allocator_type(source),
			end_node(reinterpret_cast<node_pointer_type>(&end_node), reinterpret_cast<node_pointer_type>(&end_node)),
			last_endpoint(NULL),
			end_iterator(reinterpret_cast<node_pointer_type>(&end_node)),
			begin_iterator(reinterpret_cast<node_pointer_type>(&end_node)),
			node_pointer_allocator_pair(0),
			node_allocator_pair(0)
		{
			range_insert(end_iterator, source.node_pointer_allocator_pair.total_size, source.begin_iterator);
		}



		// Allocator-extended copy constructor:

		list(const list& source, const allocator_type& alloc) :
			element_allocator_type(alloc),
			end_node(reinterpret_cast<node_pointer_type>(&end_node), reinterpret_cast<node_pointer_type>(&end_node)),
			last_endpoint(NULL),
			end_iterator(reinterpret_cast<node_pointer_type>(&end_node)),
			begin_iterator(reinterpret_cast<node_pointer_type>(&end_node)),
			node_pointer_allocator_pair(0),
			node_allocator_pair(0)
		{
			range_insert(end_iterator, source.node_pointer_allocator_pair.total_size, source.begin_iterator);
		}



#ifdef PLF_MOVE_SEMANTICS_SUPPORT
		// Move constructor:

		list(list&& source) PLF_NOEXCEPT:
		element_allocator_type(source),
			groups(std::move(source.groups)),
			end_node(std::move(source.end_node)),
			last_endpoint(std::move(source.last_endpoint)),
			end_iterator(reinterpret_cast<node_pointer_type>(&end_node)),
			begin_iterator((source.begin_iterator.node_pointer == source.end_iterator.node_pointer) ? reinterpret_cast<node_pointer_type>(&end_node) : std::move(source.begin_iterator)),
			node_pointer_allocator_pair(source.node_pointer_allocator_pair.total_size),
			node_allocator_pair(source.node_allocator_pair.number_of_erased_nodes)
		{
			end_node.previous->next = begin_iterator.node_pointer->previous = end_iterator.node_pointer;
			source.groups.blank();
			source.reset();
		}



		// Allocator-extended move constructor:

		list(list&& source, const allocator_type& alloc) :
			element_allocator_type(alloc),
			groups(std::move(source.groups)),
			end_node(std::move(source.end_node)),
			last_endpoint(std::move(source.last_endpoint)),
			end_iterator(reinterpret_cast<node_pointer_type>(&end_node)),
			begin_iterator((source.begin_iterator.node_pointer == source.end_iterator.node_pointer) ? reinterpret_cast<node_pointer_type>(&end_node) : std::move(source.begin_iterator)),
			node_pointer_allocator_pair(source.node_pointer_allocator_pair.total_size),
			node_allocator_pair(source.node_allocator_pair.number_of_erased_nodes)
		{
			end_node.previous->next = begin_iterator.node_pointer->previous = end_iterator.node_pointer;
			source.groups.blank();
			source.reset();
		}
#endif



		// Fill constructor:

		list(const size_type fill_number, const element_type& element, const element_allocator_type& alloc = element_allocator_type()) :
			element_allocator_type(alloc),
			end_node(reinterpret_cast<node_pointer_type>(&end_node), reinterpret_cast<node_pointer_type>(&end_node)),
			last_endpoint(NULL),
			end_iterator(reinterpret_cast<node_pointer_type>(&end_node)),
			begin_iterator(reinterpret_cast<node_pointer_type>(&end_node)),
			node_pointer_allocator_pair(0),
			node_allocator_pair(0)
		{
			insert(end_iterator, fill_number, element);
		}



		// Default element value fill constructor:

		explicit list(const size_type fill_number, const element_allocator_type& alloc = element_allocator_type()) :
			element_allocator_type(alloc),
			end_node(reinterpret_cast<node_pointer_type>(&end_node), reinterpret_cast<node_pointer_type>(&end_node)),
			last_endpoint(NULL),
			end_iterator(reinterpret_cast<node_pointer_type>(&end_node)),
			begin_iterator(reinterpret_cast<node_pointer_type>(&end_node)),
			node_pointer_allocator_pair(0),
			node_allocator_pair(0)
		{
			insert(end_iterator, fill_number, element_type());
		}



		// Range constructor:

		template<typename iterator_type>
		list(const typename plf_enable_if_c<!std::numeric_limits<iterator_type>::is_integer, iterator_type>::type& first, const iterator_type& last, const element_allocator_type& alloc = element_allocator_type()) :
			element_allocator_type(alloc),
			end_node(reinterpret_cast<node_pointer_type>(&end_node), reinterpret_cast<node_pointer_type>(&end_node)),
			last_endpoint(NULL),
			end_iterator(reinterpret_cast<node_pointer_type>(&end_node)),
			begin_iterator(reinterpret_cast<node_pointer_type>(&end_node)),
			node_pointer_allocator_pair(0),
			node_allocator_pair(0)
		{
			insert<iterator_type>(end_iterator, first, last);
		}



		// Initializer-list constructor:

#ifdef PLF_INITIALIZER_LIST_SUPPORT
		list(const std::initializer_list<element_type>& element_list, const element_allocator_type& alloc = element_allocator_type()) :
			element_allocator_type(alloc),
			end_node(reinterpret_cast<node_pointer_type>(&end_node), reinterpret_cast<node_pointer_type>(&end_node)),
			last_endpoint(NULL),
			end_iterator(reinterpret_cast<node_pointer_type>(&end_node)),
			begin_iterator(reinterpret_cast<node_pointer_type>(&end_node)),
			node_pointer_allocator_pair(0),
			node_allocator_pair(0)
		{
			range_insert(end_iterator, static_cast<size_type>(element_list.size()), element_list.begin());
		}

#endif



		~list() PLF_NOEXCEPT
		{
			groups.destroy_all_data(last_endpoint);
		}



		inline iterator begin() PLF_NOEXCEPT
		{
			return begin_iterator;
		}



		inline const_iterator begin() const PLF_NOEXCEPT
		{
			return begin_iterator;
		}



		inline iterator end() PLF_NOEXCEPT
		{
			return end_iterator;
		}



		inline const_iterator end() const PLF_NOEXCEPT
		{
			return end_iterator;
		}



		inline const_iterator cbegin() const PLF_NOEXCEPT
		{
			return const_iterator(begin_iterator.node_pointer);
		}



		inline const_iterator cend() const PLF_NOEXCEPT
		{
			return const_iterator(end_iterator.node_pointer);
		}



		inline reverse_iterator rbegin() PLF_NOEXCEPT
		{
			return reverse_iterator(end_node.previous);
		}



		inline reverse_iterator rend() PLF_NOEXCEPT
		{
			return reverse_iterator(end_iterator.node_pointer);
		}



		inline const_reverse_iterator crbegin() const PLF_NOEXCEPT
		{
			return const_reverse_iterator(end_node.previous);
		}



		inline const_reverse_iterator crend() const PLF_NOEXCEPT
		{
			return const_reverse_iterator(end_iterator.node_pointer);
		}



		inline reference front()
		{
			assert(begin_iterator.node_pointer != &end_node);
			return begin_iterator.node_pointer->element;
		}



		inline const_reference front() const
		{
			assert(begin_iterator.node_pointer != &end_node);
			return begin_iterator.node_pointer->element;
		}



		inline reference back()
		{
			assert(end_node.previous != &end_node);
			return end_node.previous->element;
		}



		inline const_reference back() const
		{
			assert(end_node.previous != &end_node);
			return end_node.previous->element;
		}



		void clear() PLF_NOEXCEPT
		{
			if (last_endpoint == NULL)
			{
				return;
			}

			if (node_pointer_allocator_pair.total_size != 0)
			{
				groups.clear(last_endpoint);
			}

			end_node.next = reinterpret_cast<node_pointer_type>(&end_node);
			end_node.previous = reinterpret_cast<node_pointer_type>(&end_node);
			last_endpoint = NULL;
			begin_iterator.node_pointer = end_iterator.node_pointer;
			node_pointer_allocator_pair.total_size = 0;
			node_allocator_pair.number_of_erased_nodes = 0;
		}



	private:


		void reset() PLF_NOEXCEPT
		{
			groups.destroy_all_data(last_endpoint);
			last_endpoint = NULL;
			end_node.next = reinterpret_cast<node_pointer_type>(&end_node);
			end_node.previous = reinterpret_cast<node_pointer_type>(&end_node);
			begin_iterator.node_pointer = end_iterator.node_pointer;
			node_pointer_allocator_pair.total_size = 0;
			node_allocator_pair.number_of_erased_nodes = 0;
		}



		inline void add_group_if_necessary()
		{
			if (last_endpoint == groups.last_endpoint_group->beyond_end) // last_endpoint is beyond the end of a group
			{
				if (static_cast<size_type>(groups.last_endpoint_group - groups.block_pointer) == groups.size - 1) // ie. there are no reusable groups available at the back of group vector
				{
					groups.add_new((node_pointer_allocator_pair.total_size < PLF_BLOCK_MAX) ? static_cast<group_size_type>(node_pointer_allocator_pair.total_size) : PLF_BLOCK_MAX);
				}
				else
				{
					++groups.last_endpoint_group;
				}

				last_endpoint = groups.last_endpoint_group->nodes;
			}
		}



		inline void update_sizes_and_iterators(const const_iterator it)
		{
			++(groups.last_endpoint_group->number_of_elements);
			++node_pointer_allocator_pair.total_size;

			if (it.node_pointer == begin_iterator.node_pointer)
			{
				begin_iterator.node_pointer = last_endpoint;
			}

			it.node_pointer->previous->next = last_endpoint;
			it.node_pointer->previous = last_endpoint;
		}



		inline void insert_initialize()
		{
			if (groups.block_pointer == NULL) // In case of prior reserve/clear call as opposed to being uninitialized
			{
				groups.initialize(PLF_BLOCK_MIN);
			}

			groups.last_endpoint_group->number_of_elements = 1;
			end_node.next = end_node.previous = last_endpoint = begin_iterator.node_pointer = groups.last_endpoint_group->nodes;
			node_pointer_allocator_pair.total_size = 1;
		}



	public:


		iterator insert(const const_iterator it, const element_type& element)
		{
			if (last_endpoint != NULL) // ie. list is not empty
			{
				if (node_allocator_pair.number_of_erased_nodes == 0) // No erased nodes available for reuse
				{
					add_group_if_necessary();

#ifdef PLF_VARIADICS_SUPPORT
					PLF_CONSTRUCT(node_allocator_type, node_allocator_pair, last_endpoint, it.node_pointer, it.node_pointer->previous, element);
#else
					PLF_CONSTRUCT(node_allocator_type, node_allocator_pair, last_endpoint, node(it.node_pointer, it.node_pointer->previous, element));
#endif

					update_sizes_and_iterators(it);
					return iterator(last_endpoint++);
				}
				else
				{
					group_pointer_type const node_group = groups.get_nearest_freelist_group((it.node_pointer != end_iterator.node_pointer) ? it.node_pointer : end_node.previous);
					node_pointer_type const selected_node = node_group->free_list_head;
					const node_pointer_type previous = node_group->free_list_head->previous;

#ifdef PLF_VARIADICS_SUPPORT
					PLF_CONSTRUCT(node_allocator_type, node_allocator_pair, selected_node, it.node_pointer, it.node_pointer->previous, element);
#else
					PLF_CONSTRUCT(node_allocator_type, node_allocator_pair, selected_node, node(it.node_pointer, it.node_pointer->previous, element));
#endif

					node_group->free_list_head = previous;
					++(node_group->number_of_elements);
					++node_pointer_allocator_pair.total_size;
					--node_allocator_pair.number_of_erased_nodes;

					it.node_pointer->previous->next = selected_node;
					it.node_pointer->previous = selected_node;

					if (it.node_pointer == begin_iterator.node_pointer)
					{
						begin_iterator.node_pointer = selected_node;
					}

					return iterator(selected_node);
				}
			}
			else // list is empty
			{
				insert_initialize();

#ifdef PLF_TYPE_TRAITS_SUPPORT
				if PLF_CONSTEXPR(std::is_nothrow_copy_constructible<node>::value) // Avoid try-catch code generation
				{
#ifdef PLF_VARIADICS_SUPPORT
					PLF_CONSTRUCT(node_allocator_type, node_allocator_pair, last_endpoint++, end_iterator.node_pointer, end_iterator.node_pointer, element);
#else
					PLF_CONSTRUCT(node_allocator_type, node_allocator_pair, last_endpoint++, node(end_iterator.node_pointer, end_iterator.node_pointer, element));
#endif
				}
				else
#endif
				{
					try
					{
#ifdef PLF_VARIADICS_SUPPORT
						PLF_CONSTRUCT(node_allocator_type, node_allocator_pair, last_endpoint++, end_iterator.node_pointer, end_iterator.node_pointer, element);
#else
						PLF_CONSTRUCT(node_allocator_type, node_allocator_pair, last_endpoint++, node(end_iterator.node_pointer, end_iterator.node_pointer, element));
#endif
					}
					catch (...)
					{
						reset();
						throw;
					}
				}

				return begin_iterator;
			}
		}



		inline PLF_FORCE_INLINE void push_back(const element_type& element)
		{
			insert(end_iterator, element);
		}



		inline PLF_FORCE_INLINE void push_front(const element_type& element)
		{
			insert(begin_iterator, element);
		}



#ifdef PLF_MOVE_SEMANTICS_SUPPORT
		iterator insert(const const_iterator it, element_type&& element) // This is almost identical to the insert implementation above with the only change being std::move of the element
		{
			if (last_endpoint != NULL)
			{
				if (node_allocator_pair.number_of_erased_nodes == 0)
				{
					add_group_if_necessary();

#ifdef PLF_VARIADICS_SUPPORT
					PLF_CONSTRUCT(node_allocator_type, node_allocator_pair, last_endpoint, it.node_pointer, it.node_pointer->previous, std::move(element));
#else
					PLF_CONSTRUCT(node_allocator_type, node_allocator_pair, last_endpoint, node(it.node_pointer, it.node_pointer->previous, std::move(element)));
#endif

					update_sizes_and_iterators(it);
					return iterator(last_endpoint++);
				}
				else
				{
					group_pointer_type const node_group = groups.get_nearest_freelist_group((it.node_pointer != end_iterator.node_pointer) ? it.node_pointer : end_node.previous);
					node_pointer_type const selected_node = node_group->free_list_head;
					const node_pointer_type previous = node_group->free_list_head->previous;

#ifdef PLF_VARIADICS_SUPPORT
					PLF_CONSTRUCT(node_allocator_type, node_allocator_pair, selected_node, it.node_pointer, it.node_pointer->previous, std::move(element));
#else
					PLF_CONSTRUCT(node_allocator_type, node_allocator_pair, selected_node, node(it.node_pointer, it.node_pointer->previous, std::move(element)));
#endif

					node_group->free_list_head = previous;
					++(node_group->number_of_elements);
					++node_pointer_allocator_pair.total_size;
					--node_allocator_pair.number_of_erased_nodes;

					it.node_pointer->previous->next = selected_node;
					it.node_pointer->previous = selected_node;

					if (it.node_pointer == begin_iterator.node_pointer)
					{
						begin_iterator.node_pointer = selected_node;
					}

					return iterator(selected_node);
				}
			}
			else
			{
				insert_initialize();

#ifdef PLF_TYPE_TRAITS_SUPPORT
				if PLF_CONSTEXPR(std::is_nothrow_move_constructible<node>::value)
				{
#ifdef PLF_VARIADICS_SUPPORT
					PLF_CONSTRUCT(node_allocator_type, node_allocator_pair, last_endpoint++, end_iterator.node_pointer, end_iterator.node_pointer, std::move(element));
#else
					PLF_CONSTRUCT(node_allocator_type, node_allocator_pair, last_endpoint++, node(end_iterator.node_pointer, end_iterator.node_pointer, std::move(element)));
#endif
				}
				else
#endif
				{
					try
					{
#ifdef PLF_VARIADICS_SUPPORT
						PLF_CONSTRUCT(node_allocator_type, node_allocator_pair, last_endpoint++, end_iterator.node_pointer, end_iterator.node_pointer, std::move(element));
#else
						PLF_CONSTRUCT(node_allocator_type, node_allocator_pair, last_endpoint++, node(end_iterator.node_pointer, end_iterator.node_pointer, std::move(element)));
#endif
					}
					catch (...)
					{
						reset();
						throw;
					}
				}

				return begin_iterator;
			}
		}



		inline PLF_FORCE_INLINE void push_back(element_type&& element)
		{
			insert(end_iterator, std::move(element));
		}



		inline PLF_FORCE_INLINE void push_front(element_type&& element)
		{
			insert(begin_iterator, std::move(element));
		}
#endif




#ifdef PLF_VARIADICS_SUPPORT
		template<typename... arguments>
		iterator emplace(const const_iterator it, arguments &&... parameters) // This is almost identical to the insert implementations above with the only changes being std::forward of element parameters, removal of VARIADICS support checking, and is_nothrow_contructible
		{
			if (last_endpoint != NULL)
			{
				if (node_allocator_pair.number_of_erased_nodes == 0)
				{
					add_group_if_necessary();

					PLF_CONSTRUCT(node_allocator_type, node_allocator_pair, last_endpoint, it.node_pointer, it.node_pointer->previous, std::forward<arguments>(parameters)...);

					update_sizes_and_iterators(it);
					return iterator(last_endpoint++);
				}
				else
				{
					group_pointer_type const node_group = groups.get_nearest_freelist_group((it.node_pointer != end_iterator.node_pointer) ? it.node_pointer : end_node.previous);
					node_pointer_type const selected_node = node_group->free_list_head;
					const node_pointer_type previous = node_group->free_list_head->previous;

					PLF_CONSTRUCT(node_allocator_type, node_allocator_pair, selected_node, it.node_pointer, it.node_pointer->previous, std::forward<arguments>(parameters)...);

					node_group->free_list_head = previous;
					++(node_group->number_of_elements);
					++node_pointer_allocator_pair.total_size;
					--node_allocator_pair.number_of_erased_nodes;

					it.node_pointer->previous->next = selected_node;
					it.node_pointer->previous = selected_node;

					if (it.node_pointer == begin_iterator.node_pointer)
					{
						begin_iterator.node_pointer = selected_node;
					}
					return iterator(selected_node);
				}
			}
			else
			{
				insert_initialize();

#ifdef PLF_TYPE_TRAITS_SUPPORT
				if PLF_CONSTEXPR(std::is_nothrow_constructible<element_type, arguments ...>::value)
				{
					PLF_CONSTRUCT(node_allocator_type, node_allocator_pair, last_endpoint++, end_iterator.node_pointer, end_iterator.node_pointer, std::forward<arguments>(parameters)...);
				}
				else
#endif
				{
					try
					{
						PLF_CONSTRUCT(node_allocator_type, node_allocator_pair, last_endpoint++, end_iterator.node_pointer, end_iterator.node_pointer, std::forward<arguments>(parameters)...);
					}
					catch (...)
					{
						reset();
						throw;
					}
				}

				return begin_iterator;
			}
		}



		template<typename... arguments>
		inline PLF_FORCE_INLINE reference emplace_back(arguments &&... parameters)
		{
			return (emplace(end_iterator, std::forward<arguments>(parameters)...)).node_pointer->element;
		}



		template<typename... arguments>
		inline PLF_FORCE_INLINE reference emplace_front(arguments &&... parameters)
		{
			return (emplace(begin_iterator, std::forward<arguments>(parameters)...)).node_pointer->element;
		}


#endif




	private:

		void fill(const element_type& element, group_size_type number_of_elements, node_pointer_type const position)
		{
			position->previous->next = last_endpoint;
			groups.last_endpoint_group->number_of_elements = static_cast<group_size_type>(groups.last_endpoint_group->number_of_elements + number_of_elements);
			node_pointer_type previous = position->previous;

			do
			{
#ifdef PLF_TYPE_TRAITS_SUPPORT
				if PLF_CONSTEXPR(std::is_nothrow_copy_constructible<element_type>::value)
				{
#ifdef PLF_VARIADICS_SUPPORT
					PLF_CONSTRUCT(node_allocator_type, node_allocator_pair, last_endpoint, last_endpoint + 1, previous, element);
#else
					PLF_CONSTRUCT(node_allocator_type, node_allocator_pair, last_endpoint, node(last_endpoint + 1, previous, element));
#endif
				}
				else
#endif
				{
					try
					{
#ifdef PLF_VARIADICS_SUPPORT
						PLF_CONSTRUCT(node_allocator_type, node_allocator_pair, last_endpoint, last_endpoint + 1, previous, element);
#else
						PLF_CONSTRUCT(node_allocator_type, node_allocator_pair, last_endpoint, node(last_endpoint + 1, previous, element));
#endif
					}
					catch (...)
					{
						previous->next = position;
						position->previous = --previous;
						groups.last_endpoint_group->number_of_elements = static_cast<group_size_type>(groups.last_endpoint_group->number_of_elements - (number_of_elements - (last_endpoint - position)));
						throw;
					}
				}

				previous = last_endpoint++;
			} while (--number_of_elements != 0);

			previous->next = position;
			position->previous = previous;
		}




		template <class iterator_type>
		iterator_type range_fill(iterator_type it, group_size_type number_of_elements, node_pointer_type const position)
		{
			position->previous->next = last_endpoint;
			groups.last_endpoint_group->number_of_elements = static_cast<group_size_type>(groups.last_endpoint_group->number_of_elements + number_of_elements);
			node_pointer_type previous = position->previous;

			do
			{
#ifdef PLF_TYPE_TRAITS_SUPPORT
				if PLF_CONSTEXPR(std::is_nothrow_copy_constructible<element_type>::value)
				{
#ifdef PLF_VARIADICS_SUPPORT
					PLF_CONSTRUCT(node_allocator_type, node_allocator_pair, last_endpoint, last_endpoint + 1, previous, *it++);
#else
					PLF_CONSTRUCT(node_allocator_type, node_allocator_pair, last_endpoint, node(last_endpoint + 1, previous, *it++));
#endif
				}
				else
#endif
				{
					try
					{
#ifdef PLF_VARIADICS_SUPPORT
						PLF_CONSTRUCT(node_allocator_type, node_allocator_pair, last_endpoint, last_endpoint + 1, previous, *it++);
#else
						PLF_CONSTRUCT(node_allocator_type, node_allocator_pair, last_endpoint, node(last_endpoint + 1, previous, *it++));
#endif
					}
					catch (...)
					{
						previous->next = position;
						position->previous = --previous;
						groups.last_endpoint_group->number_of_elements = static_cast<group_size_type>(groups.last_endpoint_group->number_of_elements - (number_of_elements - (last_endpoint - position)));
						throw;
					}
				}

				previous = last_endpoint++;
			} while (--number_of_elements != 0);

			previous->next = position;
			position->previous = previous;
			return it;
		}



		// This function is near-identical to fill-insert, the only difference is it uses and iterates over an iterator:
		template <class iterator_type>
		iterator range_insert(const const_iterator position, const size_type number_of_elements, iterator_type it)
		{
			if (number_of_elements == 0)
			{
				return end_iterator;
			}
			else if (number_of_elements == 1)
			{
				return insert(position, *it);
			}

			reserve(node_pointer_allocator_pair.total_size + number_of_elements);

			// Insert first element, then use up any erased nodes:
			size_type remainder = number_of_elements - 1;
			const iterator return_iterator = insert(position, *it++);

			while (node_allocator_pair.number_of_erased_nodes != 0)
			{
				insert(position, *it++);

				if (--remainder == 0)
				{
					return return_iterator;
				}
			}

			node_pointer_allocator_pair.total_size += remainder;

			// then use up remainder of last_endpoint_group:
			const group_size_type remaining_nodes_in_group = static_cast<group_size_type>(groups.last_endpoint_group->beyond_end - last_endpoint);

			if (remaining_nodes_in_group != 0)
			{
				if (remaining_nodes_in_group < remainder)
				{
					it = range_fill(it, remaining_nodes_in_group, position.node_pointer);
					remainder -= remaining_nodes_in_group;
				}
				else
				{
					it = range_fill(it, static_cast<group_size_type>(remainder), position.node_pointer);
					return return_iterator;
				}
			}


			// Then start using trailing (reserved) groups:
			while (true)
			{
				last_endpoint = (++groups.last_endpoint_group)->nodes;
				const group_size_type group_size = static_cast<group_size_type>(groups.last_endpoint_group->beyond_end - groups.last_endpoint_group->nodes);

				if (group_size < remainder)
				{
					it = range_fill(it, group_size, position.node_pointer);
					remainder -= group_size;
				}
				else
				{
					it = range_fill(it, static_cast<group_size_type>(remainder), position.node_pointer);
					break;
				}
			}

			return return_iterator;
		}




	public:

		// Fill insert

		iterator insert(const const_iterator position, const size_type number_of_elements, const element_type& element)
		{
			if (number_of_elements == 0)
			{
				return end_iterator;
			}
			else if (number_of_elements == 1)
			{
				return insert(position, element);
			}

			reserve(node_pointer_allocator_pair.total_size + number_of_elements);

			// Insert first element, then use up any erased nodes:
			size_type remainder = number_of_elements - 1;
			const iterator return_iterator = insert(position, element);

			while (node_allocator_pair.number_of_erased_nodes != 0)
			{
				insert(position, element);

				if (--remainder == 0)
				{
					return return_iterator;
				}
			}

			node_pointer_allocator_pair.total_size += remainder;

			// then use up remainder of last_endpoint_group:
			const group_size_type remaining_nodes_in_group = static_cast<group_size_type>(groups.last_endpoint_group->beyond_end - last_endpoint);

			if (remaining_nodes_in_group != 0)
			{
				if (remaining_nodes_in_group < remainder)
				{
					fill(element, remaining_nodes_in_group, position.node_pointer);
					remainder -= remaining_nodes_in_group;
				}
				else
				{
					fill(element, static_cast<group_size_type>(remainder), position.node_pointer);
					return return_iterator;
				}
			}


			// Then start using trailing (reserved) groups:
			while (true)
			{
				last_endpoint = (++groups.last_endpoint_group)->nodes;
				const group_size_type group_size = static_cast<group_size_type>(groups.last_endpoint_group->beyond_end - groups.last_endpoint_group->nodes);

				if (group_size < remainder)
				{
					fill(element, group_size, position.node_pointer);
					remainder -= group_size;
				}
				else
				{
					fill(element, static_cast<group_size_type>(remainder), position.node_pointer);
					break;
				}
			}

			return return_iterator;
		}




		// Range insert

		template <class iterator_type>
		inline iterator insert(const const_iterator position, typename plf_enable_if_c<!std::numeric_limits<iterator_type>::is_integer, iterator_type>::type first, const iterator_type last)
		{
			using std::distance;
			return range_insert(position, static_cast<size_type>(distance(first, last)), first);
		}



		// Range insert, move_iterator overload:

#ifdef PLF_MOVE_SEMANTICS_SUPPORT
		template <class iterator_type>
		inline iterator insert(const const_iterator position, const std::move_iterator<iterator_type> first, const std::move_iterator<iterator_type> last)
		{
			using std::distance;
			return range_insert(position, static_cast<size_type>(distance(first.base(), last.base())), first);
		}
#endif



#ifdef PLF_CPP20_SUPPORT
		// Support for differing iterator types eg. sentinels:
		template <class iterator_type1, class iterator_type2>
		requires (std::equality_comparable_with<iterator_type1, iterator_type2> && !std::integral<iterator_type1> && !std::integral<iterator_type2>)
			inline iterator insert(const const_iterator position, const iterator_type1 first, const iterator_type2 last)
		{
			size_type distance = 0;
			for (iterator_type1 current = first; current != last; ++current, ++distance) {};
			return range_insert(position, distance, first);
		}
#endif



		// Initializer-list insert

#ifdef PLF_INITIALIZER_LIST_SUPPORT
		inline iterator insert(const const_iterator it, const std::initializer_list<element_type>& element_list)
		{
			return range_insert(it, static_cast<size_type>(element_list.size()), element_list.begin());
		}
#endif



	private:

		inline PLF_FORCE_INLINE void destroy_all_node_pointers(group_pointer_type const group_to_process, const node_pointer_type beyond_end_node) PLF_NOEXCEPT
		{
			for (node_pointer_type current_node = group_to_process->nodes; current_node != beyond_end_node; ++current_node)
			{
				PLF_DESTROY(node_pointer_allocator_type, node_pointer_allocator_pair, &(current_node->next));
				PLF_DESTROY(node_pointer_allocator_type, node_pointer_allocator_pair, &(current_node->previous));
			}
		}



	public:


		// Single erase:

		iterator erase(const const_iterator it) // if uninitialized/invalid iterator supplied, function could generate an exception, hence no noexcept
		{
			assert(node_pointer_allocator_pair.total_size != 0);
			assert(it.node_pointer != NULL);
			assert(it.node_pointer != end_iterator.node_pointer);

#ifdef PLF_TYPE_TRAITS_SUPPORT
			if PLF_CONSTEXPR(!(std::is_trivially_destructible<element_type>::value))
#endif
			{
				PLF_DESTROY(element_allocator_type, *this, &(it.node_pointer->element)); // Destruct element
			}

			--node_pointer_allocator_pair.total_size;
			++node_allocator_pair.number_of_erased_nodes;


			// find the group this element is in, starting from the last group an element to-be-erased was found in (as erasures are, for most programs, likely to be closer in proximity to previous erasures):
			group_pointer_type node_group = groups.last_searched_group;

			if ((it.node_pointer < node_group->nodes) || (it.node_pointer >= node_group->beyond_end))
			{
				// Search groups to the left and right of the last searched group, in the group vector:
				const group_pointer_type beyond_end_group = groups.last_endpoint_group + 1;
				group_pointer_type left = node_group - 1;
				bool right_not_beyond_back = (++node_group < beyond_end_group);
				bool left_not_beyond_front = (left >= groups.block_pointer);

				while (true)
				{
					if (right_not_beyond_back)
					{
						if ((it.node_pointer < node_group->beyond_end) && (it.node_pointer >= node_group->nodes)) // element location found
						{
							break;
						}

						right_not_beyond_back = (++node_group < beyond_end_group);
					}

					if (left_not_beyond_front)
					{
						if ((it.node_pointer >= left->nodes) && (it.node_pointer < left->beyond_end)) // element location found
						{
							node_group = left;
							break;
						}

						left_not_beyond_front = (--left >= groups.block_pointer);
					}
				}

				groups.last_searched_group = node_group;
			}

			// To avoid pointer aliasing and increase performance:
			const node_pointer_type previous = it.node_pointer->previous;
			const node_pointer_type next = it.node_pointer->next;
			next->previous = previous;
			previous->next = next;

			if (it.node_pointer == begin_iterator.node_pointer)
			{
				begin_iterator.node_pointer = next;
			}


			const iterator return_iterator(next);

			if (--(node_group->number_of_elements) != 0) // ie. group is not empty yet, add node to free list
			{
				it.node_pointer->next = NULL; // next == NULL so that destructor and other functions which linearly iterate over node memory chunks can detect this as a free list node, ie an erased node
				it.node_pointer->previous = node_group->free_list_head;
				node_group->free_list_head = it.node_pointer;
				return return_iterator;
			}
			else if (node_group != groups.last_endpoint_group--) // remove group (and decrement active back group)
			{
				const group_size_type group_size = static_cast<group_size_type>(node_group->beyond_end - node_group->nodes);
				node_allocator_pair.number_of_erased_nodes -= group_size;

#ifdef PLF_TYPE_TRAITS_SUPPORT
				if PLF_CONSTEXPR(!std::is_trivially_destructible<node_pointer_type>::value)
#endif
				{
					destroy_all_node_pointers(node_group, node_group->beyond_end);
				}

				node_group->free_list_head = NULL;

				if ((group_size == PLF_BLOCK_MAX) | (node_group >= groups.last_endpoint_group - 1)) // Preserve only groups which are at the maximum possible size, or first/second/third-to-last active groups - seems to be best for performance under high-modification benchmarks
				{
					groups.move_to_back(node_group);
				}
				else
				{
					groups.remove(node_group);
				}

				return return_iterator;
			}
			else // clear back group, leave trailing
			{
#ifdef PLF_TYPE_TRAITS_SUPPORT
				if PLF_CONSTEXPR(!std::is_trivially_destructible<node_pointer_type>::value)
#endif
				{
					destroy_all_node_pointers(node_group, last_endpoint);
				}

				node_group->free_list_head = NULL;

				if (node_pointer_allocator_pair.total_size != 0)
				{
					node_allocator_pair.number_of_erased_nodes -= static_cast<group_size_type>(last_endpoint - node_group->nodes);
					last_endpoint = groups.last_endpoint_group->beyond_end;
				}
				else
				{
					groups.last_endpoint_group = groups.block_pointer; // If number of elements is zero, it indicates that this was the only group in the vector. In which case the last_endpoint_group would be invalid at this point due to the decrement in the above else-if statement. So it needs to be reset, as it will not be reset in the function call below.
					clear();
				}

				return return_iterator;
			}
		}



		// Range-erase:

		inline iterator erase(const_iterator iterator1, const const_iterator iterator2)
		{
			while (iterator1 != iterator2)
			{
				iterator1 = erase(iterator1);
			}

			return iterator2;
		}



		inline void pop_back() // Exception will occur on empty list
		{
			erase(iterator(end_node.previous));
		}



		inline void pop_front()
		{
			erase(begin_iterator);
		}



		inline list& operator = (const list& source)
		{
			assert(&source != this);

			clear();
			range_insert(end_iterator, source.node_pointer_allocator_pair.total_size, source.begin_iterator);

			return *this;
		}



#ifdef PLF_MOVE_SEMANTICS_SUPPORT
		// Move assignment
		list& operator = (list&& source) PLF_NOEXCEPT_MOVE_ASSIGN(allocator_type)
		{
			assert(&source != this);

			// Move source values across:
			groups.destroy_all_data(last_endpoint);

			groups = std::move(source.groups);
			end_node = std::move(source.end_node);
			last_endpoint = std::move(source.last_endpoint);
			begin_iterator.node_pointer = (source.begin_iterator.node_pointer == source.end_iterator.node_pointer) ? end_iterator.node_pointer : std::move(source.begin_iterator.node_pointer);
			node_pointer_allocator_pair.total_size = source.node_pointer_allocator_pair.total_size;
			node_allocator_pair.number_of_erased_nodes = source.node_allocator_pair.number_of_erased_nodes;

			end_node.previous->next = begin_iterator.node_pointer->previous = end_iterator.node_pointer;

			source.groups.blank();
			source.reset();
			return *this;
		}
#endif



#ifdef PLF_INITIALIZER_LIST_SUPPORT
		inline list& operator = (const std::initializer_list<element_type>& element_list)
		{
			clear();
			range_insert(end_iterator, static_cast<size_type>(element_list.size()), element_list.begin());
			return *this;
		}
#endif



		bool operator == (const list& rh) const PLF_NOEXCEPT
		{
			assert(this != &rh);

			if (node_pointer_allocator_pair.total_size != rh.node_pointer_allocator_pair.total_size)
			{
				return false;
			}

			for (const_iterator lh_iterator = begin_iterator, rh_iterator = rh.begin_iterator; lh_iterator != end_iterator; ++lh_iterator, ++rh_iterator)
			{
				if (*lh_iterator != *rh_iterator)
				{
					return false;
				}
			}

			return true;
		}



		inline bool operator != (const list& rh) const PLF_NOEXCEPT
		{
			return !(*this == rh);
		}



#ifdef PLF_CPP20_SUPPORT
		[[nodiscard]]
#endif
		inline bool empty() const PLF_NOEXCEPT
		{
			return node_pointer_allocator_pair.total_size == 0;
		}



		inline size_type size() const PLF_NOEXCEPT
		{
			return node_pointer_allocator_pair.total_size;
		}



		inline size_type max_size() const PLF_NOEXCEPT
		{
#ifdef PLF_ALLOCATOR_TRAITS_SUPPORT
			return std::allocator_traits<element_allocator_type>::max_size(*this);
#else
			return element_allocator_type::max_size();
#endif
		}



		inline size_type capacity() const PLF_NOEXCEPT
		{
			return groups.element_allocator_pair.capacity;
		}



		inline size_type memory() const PLF_NOEXCEPT
		{
			return sizeof(*this) + (groups.element_allocator_pair.capacity * sizeof(node)) + (groups.group_allocator_pair.capacity * sizeof(group));
		}



	private:


		struct less
		{
			inline bool operator() (const element_type& a, const element_type& b) const PLF_NOEXCEPT
			{
				return a < b;
			}
		};



		// Function-object to redirect the sort function to sort pointers by the elements they point to, not the pointer value
		template <class comparison_function>
		struct sort_dereferencer
		{
			comparison_function stored_instance;

			explicit sort_dereferencer(const comparison_function& function_instance) PLF_NOEXCEPT:
			stored_instance(function_instance)
			{}

			inline bool operator() (const node_pointer_type first, const node_pointer_type second)
			{
				return stored_instance(first->element, second->element);
			}
		};



	public:


		template <class comparison_function>
		void sort(comparison_function compare)
		{
			if (node_pointer_allocator_pair.total_size < 2)
			{
				return;
			}

			node_pointer_type* const node_pointers = PLF_ALLOCATE(node_pointer_allocator_type, node_pointer_allocator_pair, node_pointer_allocator_pair.total_size, NULL);
			node_pointer_type* node_pointer = node_pointers;


			// According to the C++ standard, construction of a pointer (of any type) may not trigger an exception - hence, no try-catch blocks are necessary for constructing the pointers:
			for (group_pointer_type current_group = groups.block_pointer; current_group != groups.last_endpoint_group; ++current_group)
			{
				const node_pointer_type end = current_group->beyond_end;

				if ((end - current_group->nodes) != current_group->number_of_elements) // If there are erased nodes present in the group
				{
					for (node_pointer_type current_node = current_group->nodes; current_node != end; ++current_node)
					{
						if (current_node->next != NULL) // is not free list node
						{
							PLF_CONSTRUCT(node_pointer_allocator_type, node_pointer_allocator_pair, node_pointer++, current_node);
						}
					}
				}
				else // If no erased nodes present we can avoid the per-node testing
				{
					for (node_pointer_type current_node = current_group->nodes; current_node != end; ++current_node)
					{
						PLF_CONSTRUCT(node_pointer_allocator_type, node_pointer_allocator_pair, node_pointer++, current_node);
					}
				}
			}

			if ((last_endpoint - groups.last_endpoint_group->nodes) != groups.last_endpoint_group->number_of_elements) // If there are erased nodes present in the group
			{
				for (node_pointer_type current_node = groups.last_endpoint_group->nodes; current_node != last_endpoint; ++current_node)
				{
					if (current_node->next != NULL)
					{
						PLF_CONSTRUCT(node_pointer_allocator_type, node_pointer_allocator_pair, node_pointer++, current_node);
					}
				}
			}
			else
			{
				for (node_pointer_type current_node = groups.last_endpoint_group->nodes; current_node != last_endpoint; ++current_node)
				{
					PLF_CONSTRUCT(node_pointer_allocator_type, node_pointer_allocator_pair, node_pointer++, current_node);
				}
			}


#ifndef PLF_SORT_FUNCTION
			std::sort(node_pointers, node_pointers + node_pointer_allocator_pair.total_size, sort_dereferencer<comparison_function>(compare));
#else
			PLF_SORT_FUNCTION(node_pointers, node_pointers + node_pointer_allocator_pair.total_size, sort_dereferencer<comparison_function>(compare));
#endif


			begin_iterator.node_pointer = node_pointers[0];
			begin_iterator.node_pointer->next = node_pointers[1];
			begin_iterator.node_pointer->previous = end_iterator.node_pointer;

			end_node.next = node_pointers[0];
			end_node.previous = node_pointers[node_pointer_allocator_pair.total_size - 1];
			end_node.previous->next = end_iterator.node_pointer;
			end_node.previous->previous = node_pointers[node_pointer_allocator_pair.total_size - 2];

			node_pointer_type* const back = node_pointers + node_pointer_allocator_pair.total_size - 1;

			for (node_pointer = node_pointers + 1; node_pointer != back; ++node_pointer)
			{
				(*node_pointer)->next = *(node_pointer + 1);
				(*node_pointer)->previous = *(node_pointer - 1);

#ifdef PLF_TYPE_TRAITS_SUPPORT
				if PLF_CONSTEXPR(!std::is_trivially_destructible<node_pointer_type>::value)
#endif
				{
					PLF_DESTROY(node_pointer_allocator_type, node_pointer_allocator_pair, node_pointer - 1);
				}
			}

#ifdef PLF_TYPE_TRAITS_SUPPORT
			if PLF_CONSTEXPR(!std::is_trivially_destructible<node_pointer_type>::value)
#endif
			{
				PLF_DESTROY(node_pointer_allocator_type, node_pointer_allocator_pair, back);
			}

			PLF_DEALLOCATE(node_pointer_allocator_type, node_pointer_allocator_pair, node_pointers, node_pointer_allocator_pair.total_size);
		}



		inline void sort()
		{
			sort(less());
		}



		void splice(const const_iterator position, const const_iterator first, const const_iterator last) PLF_NOEXCEPT
		{
			if (begin_iterator == first)
			{
				begin_iterator = last;
			}

			// To avoid pointer aliasing and subsequently increase performance via simultaneous assignments:
			const node_pointer_type first_previous = first.node_pointer->previous;
			const node_pointer_type last_previous = last.node_pointer->previous;
			const node_pointer_type position_previous = position.node_pointer->previous;

			last.node_pointer->previous = first_previous;
			first.node_pointer->previous->next = last.node_pointer;

			last_previous->next = position.node_pointer;
			first.node_pointer->previous = position_previous;

			position_previous->next = first.node_pointer;
			position.node_pointer->previous = last_previous;

			if (begin_iterator == position)
			{
				begin_iterator = first;
			}
		}



		inline void splice(const const_iterator position, const const_iterator location) PLF_NOEXCEPT
		{
			splice(position, location, const_iterator(location.node_pointer->next));
		}



		void reserve(size_type reserve_amount)
		{
			if (reserve_amount == 0 || reserve_amount <= groups.element_allocator_pair.capacity)
			{
				return;
			}
			else if (reserve_amount < PLF_BLOCK_MIN)
			{
				reserve_amount = PLF_BLOCK_MIN;
			}
			else if (reserve_amount > max_size())
			{
				throw std::length_error("Capacity requested via reserve() greater than max_size()");
			}


			if (groups.block_pointer != NULL && node_pointer_allocator_pair.total_size == 0)
			{ // edge case: has been filled with elements then clear()'d - some groups may be smaller than would be desired, should be replaced
				group_size_type end_group_size = static_cast<group_size_type>((groups.block_pointer + groups.size - 1)->beyond_end - (groups.block_pointer + groups.size - 1)->nodes);

				if (reserve_amount > end_group_size && end_group_size != PLF_BLOCK_MAX) // if last group isn't large enough, remove all groups
				{
					reset();
				}
				else
				{
					size_type number_of_full_groups_needed = reserve_amount / PLF_BLOCK_MAX;
					group_size_type remainder = static_cast<group_size_type>(reserve_amount - (number_of_full_groups_needed * PLF_BLOCK_MAX));

					// Remove any max_size groups which're not needed and any groups that're smaller than remainder:
					for (group_pointer_type current_group = groups.block_pointer; current_group < groups.block_pointer + groups.size;)
					{
						const group_size_type current_group_size = static_cast<group_size_type>(groups.block_pointer->beyond_end - groups.block_pointer->nodes);

						if (number_of_full_groups_needed != 0 && current_group_size == PLF_BLOCK_MAX)
						{
							--number_of_full_groups_needed;
							++current_group;
						}
						else if (remainder != 0 && current_group_size >= remainder)
						{
							remainder = 0;
							++current_group;
						}
						else
						{
							groups.remove(current_group);
						}
					}

					last_endpoint = groups.block_pointer->nodes;
				}
			}

			reserve_amount -= groups.element_allocator_pair.capacity;

			// To correct from possible reallocation caused by add_new:
			const difference_type last_endpoint_group_number = groups.last_endpoint_group - groups.block_pointer;

			size_type number_of_full_groups = (reserve_amount / PLF_BLOCK_MAX);
			reserve_amount -= (number_of_full_groups++ * PLF_BLOCK_MAX); // ++ to aid while loop below

			if (groups.block_pointer == NULL) // Previously uninitialized list or reset in above if statement; most common scenario
			{
				if (reserve_amount != 0)
				{
					groups.initialize(static_cast<group_size_type>(((reserve_amount < PLF_BLOCK_MIN) ? PLF_BLOCK_MIN : reserve_amount)));
				}
				else
				{
					groups.initialize(PLF_BLOCK_MAX);
					--number_of_full_groups;
				}
			}
			else if (reserve_amount != 0)
			{ // Create a group at least as large as the last group - may allocate more than necessary, but better solution than creating a very small group in the middle of the group vector, I think:
				const group_size_type last_endpoint_group_capacity = static_cast<group_size_type>(groups.last_endpoint_group->beyond_end - groups.last_endpoint_group->nodes);
				groups.add_new(static_cast<group_size_type>((reserve_amount < last_endpoint_group_capacity) ? last_endpoint_group_capacity : reserve_amount));
			}

			while (--number_of_full_groups != 0)
			{
				groups.add_new(PLF_BLOCK_MAX);
			}

			groups.last_endpoint_group = groups.block_pointer + last_endpoint_group_number;
		}



		inline PLF_FORCE_INLINE void trim() PLF_NOEXCEPT
		{
			groups.trim_unused_groups();
		}



		void shrink_to_fit()
		{
			if ((groups.block_pointer == NULL) | (node_pointer_allocator_pair.total_size == groups.element_allocator_pair.capacity)) // if list is uninitialized or full
			{
				return;
			}
			else if (node_pointer_allocator_pair.total_size == 0) // Edge case
			{
				reset();
				return;
			}
			else if (node_allocator_pair.number_of_erased_nodes == 0 && last_endpoint == groups.last_endpoint_group->beyond_end) //edge case - currently no wasted space except for possible trailing groups
			{
				groups.trim_unused_groups();
				return;
			}

#ifdef PLF_MOVE_SEMANTICS_SUPPORT
			list temp;

#ifdef PLF_TYPE_TRAITS_SUPPORT
			if PLF_CONSTEXPR(std::is_move_assignable<element_type>::value && std::is_move_constructible<element_type>::value) // move elements if possible, otherwise copy them
			{
				temp.range_insert(temp.end_iterator, node_allocator_pair.number_of_erased_nodes, std::make_move_iterator(begin_iterator));
			}
			else
#endif
			{
				temp.range_insert(temp.end_iterator, node_allocator_pair.number_of_erased_nodes, begin_iterator);
			}

			*this = std::move(temp);
#else
			list temp(*this);
			reset();
			swap(temp);
#endif
		}



	private:

		void append_process(list& source) // used by merge and splice
		{
			if (last_endpoint != groups.last_endpoint_group->beyond_end)
			{	// Add unused nodes to group's free list
				const node_pointer_type back_node = last_endpoint - 1;
				for (node_pointer_type current_node = groups.last_endpoint_group->beyond_end - 1; current_node != back_node; --current_node)
				{
					current_node->next = NULL;
					current_node->previous = groups.last_endpoint_group->free_list_head;
					groups.last_endpoint_group->free_list_head = current_node;
				}

				node_allocator_pair.number_of_erased_nodes += static_cast<size_type>(groups.last_endpoint_group->beyond_end - last_endpoint);
			}

			groups.append(source.groups);
			last_endpoint = source.last_endpoint;
			node_pointer_allocator_pair.total_size += source.node_pointer_allocator_pair.total_size;
			source.reset();
		}




	public:

		void splice(iterator position, list& source)
		{
			assert(&source != this);

			if (source.node_pointer_allocator_pair.total_size == 0)
			{
				return;
			}
			else if (node_pointer_allocator_pair.total_size == 0)
			{
#ifdef PLF_MOVE_SEMANTICS_SUPPORT
				* this = std::move(source);
#else
				reset();
				swap(source);
#endif

				return;
			}

			if (position.node_pointer == begin_iterator.node_pointer) // put source groups at front rather than back
			{
				swap(source);
				position.node_pointer = end_iterator.node_pointer;
			}

			position.node_pointer->previous->next = source.begin_iterator.node_pointer;
			source.begin_iterator.node_pointer->previous = position.node_pointer->previous;
			position.node_pointer->previous = source.end_node.previous;
			source.end_node.previous->next = position.node_pointer;

			append_process(source);
		}



		template <class comparison_function>
		void merge(list& source, comparison_function compare)
		{
			splice((source.node_pointer_allocator_pair.total_size >= node_pointer_allocator_pair.total_size) ? end_iterator : begin_iterator, source);
			sort(compare);
		}



		void merge(list& source)
		{
			assert(&source != this);

			if (source.node_pointer_allocator_pair.total_size == 0)
			{
				return;
			}
			else if (node_pointer_allocator_pair.total_size == 0)
			{
#ifdef PLF_MOVE_SEMANTICS_SUPPORT
				* this = std::move(source);
#else
				reset();
				swap(source);
#endif

				return;
			}

			node_pointer_type current1 = begin_iterator.node_pointer->next, current2 = source.begin_iterator.node_pointer->next;
			node_pointer_type previous = source.begin_iterator.node_pointer;
			const node_pointer_type source_end = source.end_iterator.node_pointer, this_end = end_iterator.node_pointer;

			begin_iterator.node_pointer->next = source.begin_iterator.node_pointer;
			source.begin_iterator.node_pointer->previous = begin_iterator.node_pointer;


			while ((current1 != this_end) & (current2 != source_end))
			{
				previous->next = current1;
				current1->previous = previous;
				previous = current1;
				current1 = current1->next;

				previous->next = current2;
				current2->previous = previous;
				previous = current2;
				current2 = current2->next;
			}

			if (current1 != this_end)
			{
				previous->next = current1;
				current1->previous = previous;
			}
			else
			{
				end_node.previous = source.end_node.previous;
				source.end_node.previous->next = end_iterator.node_pointer;
			}

			append_process(source);
		}



		void reverse() PLF_NOEXCEPT
		{
			// Note: Because current_node->next has to be read during swapping in this process anyway, including a test to figure out whether or not a given group has erased elements within it, and thus avoid per-node tests, is actually detrimental to performance according to benchmarks. This is unlike sort() where current_node->next is not used in the rest of the process and avoiding per-node tests of it's value is therefore beneficial in benchmarks.
			if (node_pointer_allocator_pair.total_size > 1)
			{
				for (group_pointer_type current_group = groups.block_pointer; current_group != groups.last_endpoint_group; ++current_group)
				{
					const node_pointer_type end = current_group->beyond_end;

					for (node_pointer_type current_node = current_group->nodes; current_node != end; ++current_node)
					{
						if (current_node->next != NULL) // ie. is not free list node
						{ // swap the pointers:
							const node_pointer_type temp = current_node->next;
							current_node->next = current_node->previous;
							current_node->previous = temp;
						}
					}
				}

				for (node_pointer_type current_node = groups.last_endpoint_group->nodes; current_node != last_endpoint; ++current_node)
				{
					if (current_node->next != NULL)
					{
						const node_pointer_type temp = current_node->next;
						current_node->next = current_node->previous;
						current_node->previous = temp;
					}
				}

				const node_pointer_type temp = end_node.previous;
				end_node.previous = begin_iterator.node_pointer;
				begin_iterator.node_pointer = temp;

				end_node.previous->next = end_iterator.node_pointer;
				begin_iterator.node_pointer->previous = end_iterator.node_pointer;
			}
		}



	private:

		// Used by unique()
		struct eq
		{
			inline bool operator() (const element_type& a, const element_type& b) const PLF_NOEXCEPT
			{
				return a == b;
			}
		};



		// Used by remove()
		struct eq_to
		{
			const element_type value;

			explicit eq_to(const element_type store_value) :
				value(store_value)
			{}

			inline bool operator() (const element_type compare_value) const PLF_NOEXCEPT
			{
				return value == compare_value;
			}
		};



	public:

		template <class comparison_function>
		size_type unique(comparison_function compare)
		{
			const size_type original_number_of_elements = node_pointer_allocator_pair.total_size;

			if (original_number_of_elements > 1)
			{
				element_type* previous = &(begin_iterator.node_pointer->element);

				for (iterator current = ++iterator(begin_iterator); current != end_iterator;)
				{
					if (compare(*current, *previous))
					{
						current = erase(current);
					}
					else
					{
						previous = &(current++.node_pointer->element);
					}
				}
			}

			return original_number_of_elements - node_pointer_allocator_pair.total_size;
		}



		inline size_type unique()
		{
			return unique(eq());
		}



		template <class predicate_function>
		size_type remove_if(predicate_function predicate)
		{
			const size_type original_number_of_elements = node_pointer_allocator_pair.total_size;

			if (original_number_of_elements != 0)
			{
				for (group_pointer_type current_group = groups.block_pointer; current_group != groups.last_endpoint_group; ++current_group)
				{
					group_size_type num_elements = current_group->number_of_elements;
					const node_pointer_type end = current_group->beyond_end;

					if (end - current_group->nodes != num_elements) // If there are erased nodes present in the group
					{
						for (node_pointer_type current_node = current_group->nodes; current_node != end; ++current_node)
						{
							if (current_node->next != NULL && predicate(current_node->element)) // is not free list node and validates predicate
							{
								erase(current_node);

								if (--num_elements == 0) // ie. group will be empty (and removed) now - nothing left to iterate over
								{
									--current_group; // As current group has been removed, subsequent groups have already shifted back by one, hence, the ++ to the current group in the for loop is unnecessary, and negated here
									break;
								}
							}
						}
					}
					else // No erased nodes in group
					{
						for (node_pointer_type current_node = current_group->nodes; current_node != end; ++current_node)
						{
							if (predicate(current_node->element))
							{
								erase(current_node);

								if (--num_elements == 0)
								{
									--current_group;
									break;
								}
							}
						}
					}
				}

				group_size_type num_elements = groups.last_endpoint_group->number_of_elements;

				if (last_endpoint - groups.last_endpoint_group->nodes != num_elements) // If there are erased nodes present in the group
				{
					for (node_pointer_type current_node = groups.last_endpoint_group->nodes; current_node != last_endpoint; ++current_node)
					{
						if (current_node->next != NULL && predicate(current_node->element))
						{
							erase(current_node);

							if (--num_elements == 0)
							{
								break;
							}
						}
					}
				}
				else
				{
					for (node_pointer_type current_node = groups.last_endpoint_group->nodes; current_node != last_endpoint; ++current_node)
					{
						if (predicate(current_node->element))
						{
							erase(current_node);

							if (--num_elements == 0)
							{
								break;
							}
						}
					}
				}
			}

			return original_number_of_elements - node_pointer_allocator_pair.total_size;
		}



		inline size_type remove(const element_type& value)
		{
			return remove_if(eq_to(value));
		}



		void resize(const size_type number_of_elements, const element_type& value = element_type())
		{
			if (node_pointer_allocator_pair.total_size == number_of_elements)
			{
				return;
			}
			else if (number_of_elements == 0)
			{
				clear();
				return;
			}
			else if (node_pointer_allocator_pair.total_size < number_of_elements)
			{
				insert(end_iterator, number_of_elements - node_pointer_allocator_pair.total_size, value);
			}
			else // ie. node_pointer_allocator_pair.total_size > number_of_elements
			{
				const_iterator current(end_node.previous);

				for (size_type number_to_remove = node_pointer_allocator_pair.total_size - number_of_elements; number_to_remove != 0; --number_to_remove)
				{
					const node_pointer_type temp = current.node_pointer->previous;
					erase(current);
					current.node_pointer = temp;
				}
			}
		}



		// Range assign:

		template <class iterator_type>
		inline void assign(const typename plf_enable_if_c<!std::numeric_limits<iterator_type>::is_integer, iterator_type>::type first, const iterator_type last)
		{
			clear();
			insert(end_iterator, first, last);
			groups.trim_unused_groups();
		}




#ifdef PLF_CPP20_SUPPORT
		// Support for differing iterator types eg. sentinels:
		template <class iterator_type1, class iterator_type2>
		requires (std::equality_comparable_with<iterator_type1, iterator_type2> && !std::integral<iterator_type1> && !std::integral<iterator_type2>)
			inline void assign(const iterator_type1 first, const iterator_type2 last)
		{
			clear();
			insert(end_iterator, first, last);
			groups.trim_unused_groups();
		}
#endif



		// Fill assign:

		inline void assign(const size_type number_of_elements, const element_type& value)
		{
			clear();
			insert(end_iterator, number_of_elements, value);
			groups.trim_unused_groups();
		}



#ifdef PLF_INITIALIZER_LIST_SUPPORT
		// Initializer-list assign:

		inline void assign(const std::initializer_list<element_type>& element_list)
		{
			clear();
			range_insert(end_iterator, static_cast<size_type>(element_list.size()), element_list.begin());
			groups.trim_unused_groups();
		}
#endif



		inline allocator_type get_allocator() const PLF_NOEXCEPT
		{
			return element_allocator_type();
		}



		iterator unordered_find_single(const element_type& element_to_match) const PLF_NOEXCEPT
		{
			if (node_pointer_allocator_pair.total_size != 0)
			{
				for (group_pointer_type current_group = groups.block_pointer; current_group != groups.last_endpoint_group; ++current_group)
				{
					const node_pointer_type end = current_group->beyond_end;

					if (end - current_group->nodes != current_group->number_of_elements) // If there are erased nodes present in the group
					{
						for (node_pointer_type current_node = current_group->nodes; current_node != end; ++current_node)
						{
							if (current_node->next != NULL && current_node->element == element_to_match) // is not free list node and matches element
							{
								return iterator(current_node);
							}
						}
					}
					else // No erased nodes in group
					{
						for (node_pointer_type current_node = current_group->nodes; current_node != end; ++current_node)
						{
							if (current_node->element == element_to_match)
							{
								return iterator(current_node);
							}
						}
					}
				}

				if (last_endpoint - groups.last_endpoint_group->nodes != groups.last_endpoint_group->number_of_elements)
				{
					for (node_pointer_type current_node = groups.last_endpoint_group->nodes; current_node != last_endpoint; ++current_node)
					{
						if (current_node->next != NULL && current_node->element == element_to_match)
						{
							return iterator(current_node);
						}
					}
				}
				else
				{
					for (node_pointer_type current_node = groups.last_endpoint_group->nodes; current_node != last_endpoint; ++current_node)
					{
						if (current_node->element == element_to_match)
						{
							return iterator(current_node);
						}
					}
				}
			}

			return end_iterator;
		}



		list<iterator> unordered_find_multiple(const element_type& element_to_match, size_type number_to_find) const
		{
			list<iterator> return_list;

			if (node_pointer_allocator_pair.total_size != 0)
			{
				for (group_pointer_type current_group = groups.block_pointer; current_group != groups.last_endpoint_group; ++current_group)
				{
					const node_pointer_type end = current_group->beyond_end;

					if (end - current_group->nodes != current_group->number_of_elements)
					{
						for (node_pointer_type current_node = current_group->nodes; current_node != end; ++current_node)
						{
							if (current_node->next != NULL && current_node->element == element_to_match)
							{
								return_list.push_back(iterator(current_node));

								if (--number_to_find == 0)
								{
									return return_list;
								}
							}
						}
					}
					else // No erased nodes in group
					{
						for (node_pointer_type current_node = current_group->nodes; current_node != end; ++current_node)
						{
							if (current_node->element == element_to_match)
							{
								return_list.push_back(iterator(current_node));

								if (--number_to_find == 0)
								{
									return return_list;
								}
							}
						}
					}
				}

				if (last_endpoint - groups.last_endpoint_group->nodes != groups.last_endpoint_group->number_of_elements)
				{
					for (node_pointer_type current_node = groups.last_endpoint_group->nodes; current_node != last_endpoint; ++current_node)
					{
						if (current_node->next != NULL && current_node->element == element_to_match)
						{
							return_list.push_back(iterator(current_node));

							if (--number_to_find == 0)
							{
								return return_list;
							}
						}
					}
				}
				else
				{
					for (node_pointer_type current_node = groups.last_endpoint_group->nodes; current_node != last_endpoint; ++current_node)
					{
						if (current_node->element == element_to_match)
						{
							return_list.push_back(iterator(current_node));

							if (--number_to_find == 0)
							{
								return return_list;
							}
						}
					}
				}
			}

			return return_list;
		}



		list<iterator> unordered_find_all(const element_type& element_to_match) const
		{
			list<iterator> return_list;

			if (node_pointer_allocator_pair.total_size != 0)
			{
				for (group_pointer_type current_group = groups.block_pointer; current_group != groups.last_endpoint_group; ++current_group)
				{
					const node_pointer_type end = current_group->beyond_end;

					if (end - current_group->nodes != current_group->number_of_elements)
					{
						for (node_pointer_type current_node = current_group->nodes; current_node != end; ++current_node)
						{
							if (current_node->next != NULL && current_node->element == element_to_match)
							{
								return_list.push_back(iterator(current_node));
							}
						}
					}
					else // No erased nodes in group
					{
						for (node_pointer_type current_node = current_group->nodes; current_node != end; ++current_node)
						{
							if (current_node->element == element_to_match)
							{
								return_list.push_back(iterator(current_node));
							}
						}
					}
				}

				if (last_endpoint - groups.last_endpoint_group->nodes != groups.last_endpoint_group->number_of_elements)
				{
					for (node_pointer_type current_node = groups.last_endpoint_group->nodes; current_node != last_endpoint; ++current_node)
					{
						if (current_node->next != NULL && current_node->element == element_to_match)
						{
							return_list.push_back(iterator(current_node));
						}
					}
				}
				else
				{
					for (node_pointer_type current_node = groups.last_endpoint_group->nodes; current_node != last_endpoint; ++current_node)
					{
						if (current_node->element == element_to_match)
						{
							return_list.push_back(iterator(current_node));
						}
					}
				}
			}

			return return_list;
		}



		void swap(list& source) PLF_NOEXCEPT_SWAP(allocator_type)
		{
#ifdef PLF_MOVE_SEMANTICS_SUPPORT
			list temp(std::move(source));
			source = std::move(*this);
			*this = std::move(temp);
#else
			groups.swap(source.groups);

			const node_pointer_type swap_end_node_previous = end_node.previous, swap_last_endpoint = last_endpoint;
			const iterator swap_begin_iterator = begin_iterator;
			const size_type swap_total_size = node_pointer_allocator_pair.total_size, swap_number_of_erased_nodes = node_allocator_pair.number_of_erased_nodes;

			last_endpoint = source.last_endpoint;
			end_node.next = begin_iterator.node_pointer = (source.begin_iterator.node_pointer != source.end_iterator.node_pointer) ? source.begin_iterator.node_pointer : end_iterator.node_pointer;
			end_node.previous = (source.begin_iterator.node_pointer != source.end_iterator.node_pointer) ? source.end_node.previous : end_iterator.node_pointer;
			end_node.previous->next = begin_iterator.node_pointer->previous = end_iterator.node_pointer;
			node_pointer_allocator_pair.total_size = source.node_pointer_allocator_pair.total_size;
			node_allocator_pair.number_of_erased_nodes = source.node_allocator_pair.number_of_erased_nodes;

			source.last_endpoint = swap_last_endpoint;
			source.end_node.next = source.begin_iterator.node_pointer = (swap_begin_iterator.node_pointer != end_iterator.node_pointer) ? swap_begin_iterator.node_pointer : source.end_iterator.node_pointer;
			source.end_node.previous = (swap_begin_iterator.node_pointer != end_iterator.node_pointer) ? swap_end_node_previous : source.end_iterator.node_pointer;
			source.end_node.previous->next = source.begin_iterator.node_pointer->previous = source.end_iterator.node_pointer;
			source.node_pointer_allocator_pair.total_size = swap_total_size;
			source.node_allocator_pair.number_of_erased_nodes = swap_number_of_erased_nodes;
#endif
		}

	}; // end of plf::list


} // plf namespace


namespace std
{
	template <class element_type, class element_allocator_type>
	inline void swap(plf::list<element_type, element_allocator_type>& a, plf::list<element_type, element_allocator_type>& b) PLF_NOEXCEPT_SWAP(element_allocator_type)
	{
		a.swap(b);
	}



	template <class element_type, class element_allocator_type, class predicate_function>
	inline typename plf::list<element_type, element_allocator_type>::size_type erase_if(plf::list<element_type, element_allocator_type>& container, predicate_function predicate)
	{
		return container.remove_if(predicate);
	}



	template <class element_type, class element_allocator_type>
	inline typename plf::list<element_type, element_allocator_type>::size_type erase(plf::list<element_type, element_allocator_type>& container, const element_type& value)
	{
		return container.remove(value);
	}
}


#undef PLF_BLOCK_MAX
#undef PLF_BLOCK_MIN

#undef PLF_FORCE_INLINE
#undef PLF_ALIGNMENT_SUPPORT
#undef PLF_INITIALIZER_LIST_SUPPORT
#undef PLF_TYPE_TRAITS_SUPPORT
#undef PLF_ALLOCATOR_TRAITS_SUPPORT
#undef PLF_VARIADICS_SUPPORT
#undef PLF_MOVE_SEMANTICS_SUPPORT
#undef PLF_NOEXCEPT
#undef PLF_NOEXCEPT_SWAP
#undef PLF_NOEXCEPT_MOVE_ASSIGN
#undef PLF_CONSTEXPR
#undef PLF_CPP20_SUPPORT
#undef PLF_STATIC_ASSERT

#undef PLF_CONSTRUCT
#undef PLF_DESTROY
#undef PLF_ALLOCATE
#undef PLF_DEALLOCATE

#endif // PLF_LIST_H