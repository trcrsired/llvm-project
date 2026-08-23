#===--- CMakeLists.txt - Herbception error-domain runtime -------------===#
#
# Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
#===----------------------------------------------------------------------===#
#
# libherbceptions — the error-domain runtime used by the 'throws'/'fails{E}'
# herbception exception model. Also a first-class LLVM runtime: add
# "libherbceptions" to LLVM_ENABLE_RUNTIMES.
#
#==========================================================================#

cmake_minimum_required(VERSION 3.20)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(libherbceptions CXX)
endif()

set(HERBCEPTIONS_VERSION 1.0.0)
string(REGEX REPLACE "^([0-9]+)\\..*" "\\1" HERBCEPTIONS_VERSION_MAJOR
  ${HERBCEPTIONS_VERSION})

option(HERBCEPTIONS_BUILD_SHARED "Build the shared library" ON)
option(HERBCEPTIONS_BUILD_STATIC "Build the static library" ON)
option(HERBCEPTIONS_FREESTANDING "Header-only freestanding build" OFF)
option(HERBCEPTIONS_BUILD_TESTS "Build the tests" OFF)
option(HERBCEPTIONS_BUILD_FUZZERS "Build the fuzz targets" OFF)
option(HERBCEPTIONS_FUZZ_ENGINE "Link fuzz targets against libFuzzer" OFF)

set(HERBCEPTIONS_SOURCES
  src/posix.cpp
  src/parse.cpp
  src/cmath_errc.cpp
  src/itanium_exception_ptr.cpp
  src/msvc_exception_ptr.cpp
  src/win32.cpp
  src/nt.cpp
  src/com.cpp
  src/wine.cpp
)

# Generated configuration header, picked up by <herbceptions/error> through
# __has_include(<__libherbceptions_config_site.h>) (mirrors libc++).
set(HERBCEPTIONS_CONFIG_INCLUDE "${CMAKE_CURRENT_BINARY_DIR}/include")
configure_file(config.h.in
  "${HERBCEPTIONS_CONFIG_INCLUDE}/__libherbceptions_config_site.h"
  @ONLY
)

add_library(herbceptions_headers INTERFACE)
target_include_directories(herbceptions_headers INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${HERBCEPTIONS_CONFIG_INCLUDE}>
  $<INSTALL_INTERFACE:include>
)

# Freestanding build: header-only, no runtime sources.
if(HERBCEPTIONS_FREESTANDING)
  add_library(herbceptions ALIAS herbceptions_headers)
  add_library(herbceptions_static ALIAS herbceptions_headers)
  target_compile_definitions(herbceptions_headers INTERFACE
    __HERBCEPTIONS_FREESTANDING__=1)
  return()
endif()

add_library(herbceptions SHARED ${HERBCEPTIONS_SOURCES})
add_library(herbceptions_static STATIC ${HERBCEPTIONS_SOURCES})
foreach(target herbceptions herbceptions_static)
  target_link_libraries(${target} PUBLIC herbceptions_headers)
  target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_compile_definitions(${target} PRIVATE _HERBCEPTIONS_BUILDING_RUNTIME=1)
endforeach()
set_target_properties(herbceptions PROPERTIES
  VERSION ${HERBCEPTIONS_VERSION}
  SOVERSION ${HERBCEPTIONS_VERSION_MAJOR}
)
if(CMAKE_SYSTEM_NAME STREQUAL "Windows" AND HERBCEPTIONS_BUILD_SHARED)
  # The DLL's import library would collide with the static archive.
  set_target_properties(herbceptions_static PROPERTIES
    OUTPUT_NAME herbceptions_static)
else()
  set_target_properties(herbceptions_static PROPERTIES
    OUTPUT_NAME herbceptions)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  # The runtime reaches the process heap through ntdll directly.
  target_link_libraries(herbceptions PRIVATE ntdll)
  target_link_libraries(herbceptions_static PRIVATE ntdll)
endif()

install(DIRECTORY include/
  DESTINATION include
  FILES_MATCHING PATTERN "*.h"
  PATTERN "error"
)
install(FILES "${HERBCEPTIONS_CONFIG_INCLUDE}/__libherbceptions_config_site.h"
  DESTINATION include
)
install(TARGETS herbceptions herbceptions_static
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin
)

# Tests (opt-in).
if(HERBCEPTIONS_BUILD_TESTS AND NOT HERBCEPTIONS_FREESTANDING)
  add_executable(herbceptions_domain_test test/domain_test.cpp)
  target_link_libraries(herbceptions_domain_test PRIVATE herbceptions)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(herbceptions_domain_test PRIVATE
      -fherbceptions -fexceptions)
  endif()
  add_test(NAME herbceptions_domain_test COMMAND herbceptions_domain_test)
endif()

# Fuzz targets (opt-in): LLVMFuzzerTestOneInput drivers. With
# HERBCEPTIONS_FUZZ_ENGINE=ON they link against libFuzzer, whose runtime is
# built against libc++; otherwise a standalone replay/mutation driver from
# fuzz/fuzz_harness.h takes over.
if(HERBCEPTIONS_BUILD_FUZZERS AND NOT HERBCEPTIONS_FREESTANDING)
  set(HERBCEPTIONS_LIBCXX_DIR
    "$ENV{HOME}/toolchains/llvm/x86_64-linux-gnu/x86_64-linux-gnu/usr"
    CACHE PATH "libc++ installation used by the fuzzer runtime")

  function(add_herbceptions_fuzzer name)
    add_executable(${name} fuzz/${name}.cpp)
    target_include_directories(${name} PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(${name} PRIVATE herbceptions)
    if(HERBCEPTIONS_FUZZ_ENGINE)
      target_compile_definitions(${name} PRIVATE
        HERBCEPTIONS_HAVE_FUZZ_ENGINE=1)
      # Only libc++ may be visible: mixing it with libstdc++ breaks.
      target_compile_options(${name} PRIVATE -fsanitize=fuzzer
        -nostdinc++
        "-isystem${HERBCEPTIONS_LIBCXX_DIR}/include/c++/v1")
      target_link_options(${name} PRIVATE -fsanitize=fuzzer
        -nostdlib++
        "-L${HERBCEPTIONS_LIBCXX_DIR}/lib" -lc++ -lc++abi)
    endif()
    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
      target_link_libraries(${name} PRIVATE ntdll)
    endif()
  endfunction()

  # Platform selection: trust CMAKE_SYSTEM_NAME and the compiler frontend
  # variant; WIN32/MSVC lie for cross builds.
  add_herbceptions_fuzzer(error_codes_fuzzer)
  if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
      add_herbceptions_fuzzer(msvc_exception_record_fuzzer)
    else()
      add_herbceptions_fuzzer(itanium_exception_record_fuzzer)
    endif()
  else()
    add_herbceptions_fuzzer(itanium_exception_record_fuzzer)
  endif()
endif()
