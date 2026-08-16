#===--- CMakeLists.txt - Herbception error-domain runtime -------------===#
#
# Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
#===----------------------------------------------------------------------===#
#
# libherbceptions — the error-domain runtime used by the 'throws'/'fails{E}'
# herbception exception model. It provides the error_domain_singleton vtables
# for the standard error domains:
#
#   posix  (std::errc)      — always available
#   cxa_exception_code     — always available (legacy C++ EH carrier)
#   win32, nt, com, wine   — only on _WIN32 / __CYGWIN__ targets
#
# Build targets:
#   - herbceptions            shared library (DLL on Windows)
#   - herbceptions_static     static library
#   - herbceptions_headers    interface target with just the headers
#
# Options:
#   - HERBCEPTIONS_BUILD_SHARED (default ON)  build the shared library
#   - HERBCEPTIONS_BUILD_STATIC (default ON)  build the static library
#   - HERBCEPTIONS_FREESTANDING (default OFF) build a header-only interface
#     library with no compiled sources (no libc / OS dependencies), for
#     freestanding environments.
#
#===----------------------------------------------------------------------===#

cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(libherbceptions CXX)
endif()

option(HERBCEPTIONS_BUILD_SHARED "Build the shared libherbceptions library" ON)
option(HERBCEPTIONS_BUILD_STATIC "Build the static libherbceptions library" ON)
option(HERBCEPTIONS_FREESTANDING
       "Build a header-only freestanding libherbceptions (no compiled sources)"
       OFF)
option(HERBCEPTIONS_BUILD_TESTS "Build the libherbceptions tests" ON)

set(HERBCEPTIONS_HEADERS
  include/herbceptions/error
  include/herbceptions/__details/posix.h
  include/herbceptions/__details/win32.h
  include/herbceptions/__details/nt.h
  include/herbceptions/__details/com.h
  include/herbceptions/__details/wine.h
  include/herbceptions/__details/cxa_exception_code.h
)

# One translation unit per error domain, so a static archive only pulls in the
# domains actually used (reducing binary bloat).
set(HERBCEPTIONS_SOURCES
  src/posix.cpp
  src/cxa_exception.cpp
  src/win32.cpp
  src/nt.cpp
  src/com.cpp
  src/wine.cpp
)

add_library(herbceptions_headers INTERFACE)
target_include_directories(herbceptions_headers INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_compile_features(herbceptions_headers INTERFACE cxx_std_23)

# The freestanding build is header-only: it exposes the types and the
# error_domain_singleton interface, but does not compile any runtime sources
# (a freestanding environment provides its own domain singletons via the weak
# ABI entry points, or simply does not use the runtime vtable at all).
if(HERBCEPTIONS_FREESTANDING)
  add_library(herbceptions ALIAS herbceptions_headers)
  add_library(herbceptions_static ALIAS herbceptions_headers)
  target_compile_definitions(herbceptions_headers INTERFACE
    __HERBCEPTIONS_FREESTANDING__=1)
  return()
endif()

function(add_herbceptions_library target kind)
  add_library(${target} ${kind} ${HERBCEPTIONS_SOURCES})
  target_include_directories(${target} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  )
  target_include_directories(${target} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )
  target_compile_features(${target} PUBLIC cxx_std_23)
  target_compile_definitions(${target} PRIVATE _HERBCEPTIONS_BUILDING_RUNTIME=1)

  if(MSVC)
    # Don't export standard headers' symbols on Windows.
    target_compile_options(${target} PRIVATE /Zl)
  endif()
endfunction()

if(HERBCEPTIONS_BUILD_SHARED)
  add_herbceptions_library(herbceptions SHARED)
  set_target_properties(herbceptions PROPERTIES
    OUTPUT_NAME herbceptions
    VERSION 1.0.0
    SOVERSION 1
  )
endif()

if(HERBCEPTIONS_BUILD_STATIC)
  add_herbceptions_library(herbceptions_static STATIC)
  set_target_properties(herbceptions_static PROPERTIES OUTPUT_NAME herbceptions)
endif()

# Install rules.
install(DIRECTORY include/
  DESTINATION include
  FILES_MATCHING PATTERN "*.h"
  PATTERN "error"
)
if(HERBCEPTIONS_BUILD_SHARED)
  install(TARGETS herbceptions
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
  )
endif()
if(HERBCEPTIONS_BUILD_STATIC)
  install(TARGETS herbceptions_static ARCHIVE DESTINATION lib)
endif()

# -----------------------------------------------------------------------------
# Tests
# -----------------------------------------------------------------------------
if(HERBCEPTIONS_BUILD_TESTS AND NOT HERBCEPTIONS_FREESTANDING)
  include(CTest)
  if(BUILD_TESTING)
    set(HERBCEPTIONS_TEST_LIB herbceptions)
    if(NOT HERBCEPTIONS_BUILD_SHARED AND HERBCEPTIONS_BUILD_STATIC)
      set(HERBCEPTIONS_TEST_LIB herbceptions_static)
    endif()

    add_executable(herbceptions_domain_test test/domain_test.cpp)
    target_link_libraries(herbceptions_domain_test PRIVATE ${HERBCEPTIONS_TEST_LIB})
    target_compile_features(herbceptions_domain_test PRIVATE cxx_std_26)
    # The herbception type traits and the fails{E} syntax require the
    # compiler's -fherbceptions flag.
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      target_compile_options(herbceptions_domain_test PRIVATE
        -fherbceptions -fexceptions)
    endif()

    add_test(NAME herbceptions_domain_test COMMAND herbceptions_domain_test)

    # The ntkernel table test includes the private src/ntkernel.h directly and
    # needs the src dir on the include path (no linking required).
    add_executable(herbceptions_ntkernel_table_test test/ntkernel_table_test.cpp)
    target_include_directories(herbceptions_ntkernel_table_test PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/src
      ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_compile_features(herbceptions_ntkernel_table_test PRIVATE cxx_std_23)
    add_test(NAME herbceptions_ntkernel_table_test
             COMMAND herbceptions_ntkernel_table_test)
  endif()
endif()
