cmake_minimum_required(VERSION 3.8)

project(seq_tests)

option(TEST_CVECTOR "Test cvector class" ON)

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
"test_cvector.hpp"
)

target_compile_features(seq_tests PRIVATE cxx_std_11)

if(TEST_CVECTOR)
	add_definitions(-DTEST_CVECTOR)
endif()

#set the most drastic flags    
if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_compile_options(seq_tests PRIVATE /bigobj /WX /W3 /UNDEBUG)
else()
    target_compile_options(seq_tests PRIVATE -std=c++11 -Werror -Wall -Wextra -Wold-style-cast -march=native -UNDEBUG  )
endif()

if (WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
	# mingw
	target_link_options(seq_tests PRIVATE -lKernel32 -lpsapi -lBcrypt)
elseif(UNIX AND (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU"))
    target_link_options(seq_tests PRIVATE  -lpthread )
endif()

add_definitions(-DSEQ_HEADER_ONLY)

target_include_directories(seq_tests PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(seq_tests PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/../seq)
#target_include_directories(seq_tests PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/mimalloc/include)


# seq library
#add_subdirectory(../ ${CMAKE_CURRENT_BINARY_DIR}/seq)
#target_link_libraries(seq_tests PRIVATE seq)  
