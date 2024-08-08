cmake_minimum_required(VERSION 3.8)

# This test assume that seq is installed in build/install_seq folder
set(CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../build/install_seq/lib/cmake")

# Create project
project(cmake_test)

# Find seq package
find_package(seq REQUIRED seq seq_static )

# Print folders
message(STATUS "seq library :${SEQ_STATIC_LIBRARY}")
message(STATUS "seq include: ${SEQ_INCLUDE_DIR}")
message(STATUS "seq binary: ${SEQ_BIN_DIR}")



# Create target using seq_static
add_executable (test_import_static main.cpp)
# Various unrelated stuff
set_property(TARGET test_import_static PROPERTY CXX_STANDARD 14)
if (WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
	# mingw
	target_link_options(test_import_static PRIVATE -lKernel32 -lpsapi -lBcrypt )
endif()
if (NOT WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
	set_target_properties(test_import_static PROPERTIES INSTALL_RPATH "$ORIGIN")
endif()

# Link with seq_static
target_link_libraries(test_import_static PRIVATE seq_static)
# MANDATORY: add SEQ_STATIC definition to preprocessor
target_compile_definitions(test_import_static PRIVATE -DSEQ_STATIC)
# Install
install (TARGETS test_import_static RUNTIME DESTINATION ${SEQ_BIN_DIR} )



# create target using seq
add_executable (test_import_shared main.cpp)
# Various unrelated stuff
set_property(TARGET test_import_shared PROPERTY CXX_STANDARD 14)
if (WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
	# mingw
	target_link_options(test_import_shared PRIVATE -lKernel32 -lpsapi -lBcrypt )
endif()
if (NOT WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
	set_target_properties(test_import_shared PROPERTIES INSTALL_RPATH "$ORIGIN")
endif()
# Link with shared version of seq
target_link_libraries(test_import_shared PRIVATE seq)
# Install
install (TARGETS test_import_shared RUNTIME DESTINATION ${SEQ_BIN_DIR} )



# create target using header only mode
add_executable (test_header_only main.cpp)
set_property(TARGET test_header_only PROPERTY CXX_STANDARD 14)
# Various unrelated stuff
if (WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
	# mingw
	target_link_options(test_header_only PRIVATE -lKernel32 -lpsapi -lBcrypt )
endif()
if (NOT WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
	set_target_properties(test_header_only PROPERTIES INSTALL_RPATH "$ORIGIN")
endif()
#MANDATORY: define SEQ_HEADER_ONLY, and add SEQ_INCLUDE_DIR as include directory
target_include_directories(test_header_only PRIVATE ${SEQ_INCLUDE_DIR})
target_compile_definitions(test_header_only PRIVATE -DSEQ_HEADER_ONLY)
# Install
install (TARGETS test_header_only RUNTIME DESTINATION ${SEQ_BIN_DIR} )
