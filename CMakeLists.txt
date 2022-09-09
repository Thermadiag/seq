cmake_minimum_required(VERSION 3.1)

project(seq VERSION 1.0.0 LANGUAGES CXX)

include(GNUInstallDirs)

add_library(seq INTERFACE)


target_include_directories(seq INTERFACE
                           "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/seq>"
                           "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")

list(APPEND headers "${CMAKE_CURRENT_SOURCE_DIR}/seq/any.hpp"
					"${CMAKE_CURRENT_SOURCE_DIR}/seq/bits.hpp"
                    "${CMAKE_CURRENT_SOURCE_DIR}/seq/charconv.hpp"
					"${CMAKE_CURRENT_SOURCE_DIR}/seq/deque_allocator.hpp"
                    "${CMAKE_CURRENT_SOURCE_DIR}/seq/devector.hpp"
					"${CMAKE_CURRENT_SOURCE_DIR}/seq/flat_map.hpp"
					"${CMAKE_CURRENT_SOURCE_DIR}/seq/format.hpp"
					"${CMAKE_CURRENT_SOURCE_DIR}/seq/hash.hpp"
					"${CMAKE_CURRENT_SOURCE_DIR}/seq/memory.hpp"
					"${CMAKE_CURRENT_SOURCE_DIR}/seq/ordered_map.hpp"
					"${CMAKE_CURRENT_SOURCE_DIR}/seq/pdqsort.hpp"
					"${CMAKE_CURRENT_SOURCE_DIR}/seq/seq.hpp"
					"${CMAKE_CURRENT_SOURCE_DIR}/seq/sequence.hpp"
					"${CMAKE_CURRENT_SOURCE_DIR}/seq/tagged_pointer.hpp"
					"${CMAKE_CURRENT_SOURCE_DIR}/seq/testing.hpp"
					"${CMAKE_CURRENT_SOURCE_DIR}/seq/tiered_vector.hpp"
					"${CMAKE_CURRENT_SOURCE_DIR}/seq/tiny_string.hpp"
					"${CMAKE_CURRENT_SOURCE_DIR}/seq/type_traits.hpp"
					"${CMAKE_CURRENT_SOURCE_DIR}/seq/utils.hpp"
					)
target_sources(seq INTERFACE "$<BUILD_INTERFACE:${headers}>")



# Installation (only compatible with CMake version >= 3.3)
if(${CMAKE_VERSION} VERSION_GREATER "3.2")
    include(CMakePackageConfigHelpers)

    ## Install include directory and potential natvis file
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/seq"
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")



    ## Create and install seqConfig.cmake
    configure_package_config_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/seqConfig.cmake.in"
                                  "${CMAKE_CURRENT_BINARY_DIR}/seqConfig.cmake"
                                  INSTALL_DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/cmake/seq")

    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/seqConfig.cmake"
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/cmake/seq")


    ## Create local seqTargets.cmake
    export(TARGETS seq NAMESPACE seq:: FILE "${CMAKE_CURRENT_BINARY_DIR}/seqTargets.cmake")

    ## Create and install global seqTargets.cmake
    install(TARGETS seq
            EXPORT seqTargets)

    install(EXPORT seqTargets
            NAMESPACE seq::
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/cmake/seq")



    ## Create and install seqConfigVersion.cmake
    # seq is header-only and does not depend on the architecture.
    # Remove CMAKE_SIZEOF_VOID_P from seqConfigVersion.cmake so that a
    # seqConfig.cmake generated for a 64 bit target can be used for 32 bit
    # targets and vice versa.
    set(CMAKE_SIZEOF_VOID_P_BACKUP ${CMAKE_SIZEOF_VOID_P})
    unset(CMAKE_SIZEOF_VOID_P)
    write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/seqConfigVersion.cmake"
                                     COMPATIBILITY SameMajorVersion)
    set(CMAKE_SIZEOF_VOID_P ${CMAKE_SIZEOF_VOID_P_BACKUP})

    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/seqConfigVersion.cmake"
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/cmake/seq")
endif()