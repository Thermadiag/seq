cmake_minimum_required(VERSION 3.8)

project(seq_benchs)

add_executable(seq_benchs 
"main.cpp" 
"bench_text_stream.hpp"
"bench_tiny_string.hpp"
"bench_tiered_vector.hpp"
"bench_hash.hpp"
"bench_map.hpp"
)

target_compile_features(seq_benchs PRIVATE cxx_std_11)


if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(seq_benchs PRIVATE -O3 -DNDEBUG )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_compile_options(seq_benchs PRIVATE /O2 /DNDEBUG)
endif()

#target_include_directories(seq_benchs PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/fast_float-main/include/fast_float)
target_include_directories(seq_benchs PRIVATE ${CMAKE_CURRENT_BINARY_DIR})


# seq library
add_subdirectory(../ ${CMAKE_CURRENT_BINARY_DIR}/seq)
target_link_libraries(seq_benchs PRIVATE seq)  


