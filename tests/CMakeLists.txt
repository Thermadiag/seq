cmake_minimum_required(VERSION 3.8)

project(seq_tests)

add_executable(seq_tests 
"main.cpp" 
"test_any.hpp"
"test_tiered_vector.hpp"
"test_devector.hpp"
"test_hash.hpp"
"test_map.hpp"
"test_mem_pool.hpp"
"test_sequence.hpp"
"test_charconv.hpp"
"test_format.hpp"
"test_tiny_string.hpp"
)

target_compile_features(seq_tests PRIVATE cxx_std_11)

#set the most drastic flags    
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(seq_tests PRIVATE -Werror -Wall -Wextra -Wold-style-cast -UNDEBUG  )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_compile_options(seq_tests PRIVATE /bigobj /WX /W3 /UNDEBUG)
endif()

if (WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
	# mingw
	target_link_options(seq_tests PRIVATE -lKernel32 -lpsapi -lBcrypt)
elseif(UNIX AND (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU"))
    target_link_options(seq_tests PRIVATE  -lpthread )
endif()

target_include_directories(seq_tests PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
#target_include_directories(seq_tests PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/mimalloc/include)


# seq library
add_subdirectory(../ ${CMAKE_CURRENT_BINARY_DIR}/seq)
target_link_libraries(seq_tests PRIVATE seq)  