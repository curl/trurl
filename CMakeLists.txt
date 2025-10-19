#***************************************************************************
#                             _                   _
#  Project                   | |_ _ __ _   _ _ __| |
#                            | __| '__| | | | '__| |
#                            | |_| |  | |_| | |  | |
#                             \__|_|   \__,_|_|  |_|
#
# Copyright (C) Viktor Szakats
#
# This software is licensed as described in the file COPYING, which
# you should have received as part of this distribution. The terms
# are also available at https://curl.se/docs/copyright.html.
#
# You may opt to use, copy, modify, merge, publish, distribute and/or sell
# copies of the Software, and permit persons to whom the Software is
# furnished to do so, under the terms of the COPYING file.
#
# This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
# KIND, either express or implied.
#
# SPDX-License-Identifier: curl
#
###########################################################################

#
# Options:
#
# - `TRURL_EMABLE_MANUAL`: Build the trurl man page (requires Perl). Default: `ON`
# - `TRURL_COMPLETION_ZSH`: Install zsh completions (requires POSIX shell). Default: `OFF`
# - `TRURL_TESTS`: Run tests (requires Python). Default: `ON`
#   Test targets: `trurl-test`, `trurl-test-memory`.
# - `TURLS_DISABLE_INSTALL`: Disable installation targets. Default `OFF`
# - `TRURL_WERROR`: Turn compiler warnings into errors. Default: `OFF`
# - `CURL_INCLUDE_DIR`: Absolute path to curl include directory.
# - `CURL_LIBRARY`: Absolute path to libcurl ibrary.
#

cmake_minimum_required(VERSION 3.17 FATAL_ERROR)
message(STATUS "Using CMake version ${CMAKE_VERSION}")

set(_version_regex "#define TRURL_VERSION_TXT \"([0-9.]+)\"")
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/version.h" trurl_version_str REGEX "${_version_regex}")
string(REGEX REPLACE "${_version_regex}" "\\1" trurl_version_str "${trurl_version_str}")
unset(_version_regex)

message(STATUS "trurl version=[${trurl_version_str}]")

project(trurl VERSION "${trurl_version_str}" LANGUAGES C)

# Install options

option(TURLS_DISABLE_INSTALL "Disable installation targets" OFF)
if(NOT TRURL_DISABLE_INSTALL)
  include(GNUInstallDirs)
endif()

# Compiler options

set(_picky "")

option(TRURL_WERROR "Turn compiler warnings into errors" OFF)
if(TRURL_WERROR)
  if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
    set(CMAKE_COMPILE_WARNING_AS_ERROR ON)
  elseif(MSVC)
    list(APPEND _picky "-WX")
  else()  # llvm/clang and gcc style options
    list(APPEND _picky "-Werror")
  endif()

  if((CMAKE_C_COMPILER_ID STREQUAL "GNU" AND
      CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 5.0) OR
     CMAKE_C_COMPILER_ID MATCHES "Clang")
    list(APPEND _picky "-pedantic-errors")
  endif()
endif()

if(MSVC)
  list(APPEND _picky "-W4")  # Use the highest warning level for Visual Studio.
elseif(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID MATCHES "Clang")
  list(APPEND _picky "-W" "-Wall" "-Wshadow" "-pedantic")
  list(APPEND _picky "-Wconversion" "-Wmissing-prototypes" "-Wwrite-strings" "-Wsign-compare" "-Wno-sign-conversion")
endif()

string(REPLACE ";" " " _picky_tmp "${_picky}")
message(STATUS "Picky compiler options: ${_picky_tmp}")
set_property(DIRECTORY APPEND PROPERTY COMPILE_OPTIONS "${_picky}")

# Build

find_package(CURL REQUIRED)
if(CURL_INCLUDE_DIR AND CURL_LIBRARY)  # Custom configuration
  # CMake's FindCURL.cmake module (as of v4.1.2), may leave a 'curl' library
  # in the `INTERFACE_LINK_LIBRARIES` property, while telling it to look for
  # libcurl at a specific locations via custom `CURL_*` variables. It seems to
  # be picking up the stray library from `pkg-config`. Seen in a cross-build
  # from Unix to Windows. Linking then fails because the library is not found
  # by the linker on a default path. Work around by deleting the stray library.
  # Another workaround is '-DPKG_CONFIG_EXECUTABLE=nonexistent' or similar ways
  # to prevent pkg-config detecting a different libcurl.
  set_target_properties(CURL::libcurl PROPERTIES INTERFACE_LINK_LIBRARIES "")
endif()

add_executable(trurl "trurl.c" "version.h")
target_link_libraries(trurl PRIVATE CURL::libcurl)

if(NOT TRURL_DISABLE_INSTALL)
  install(TARGETS trurl DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# Manual

option(TRURL_EMABLE_MANUAL "Build the trurl man page (requires Perl)" ON)
if(TRURL_EMABLE_MANUAL)
  find_package(Perl)
  if(NOT Perl_FOUND)
    message(WARNING "Perl not found, cannot build the man page.")
  else()
    set(_man_trurl "${CMAKE_CURRENT_BINARY_DIR}/trurl.1")
    add_custom_command(OUTPUT "${_man_trurl}"
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMAND "${PERL_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/scripts/cd2nroff" "trurl.md" > "${_man_trurl}"
      DEPENDS "${PROJECT_SOURCE_DIR}/scripts/cd2nroff" "trurl.md"
      VERBATIM
    )
    add_custom_target(trurl-man ALL DEPENDS "${_man_trurl}")

    if(NOT TRURL_DISABLE_INSTALL)
      install(FILES "${_man_trurl}" DESTINATION "${CMAKE_INSTALL_MANDIR}/man1")
    endif()
  endif()
endif()

# Completion

option(TRURL_COMPLETION_ZSH "Install zsh completions (requires POSIX shell)" OFF)
if(TRURL_COMPLETION_ZSH)
  find_program(SH_EXECUTABLE "sh")
  mark_as_advanced(SH_EXECUTABLE)
  if(NOT SH_EXECUTABLE)
    message(WARNING "POSIX shell not found, cannot generate completion script")
  else()
    set(_completion_zsh "${CMAKE_CURRENT_BINARY_DIR}/_trurl.zsh")
    add_custom_command(OUTPUT "${_completion_zsh}"
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMAND "${SH_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_completions.sh" "trurl.md" > "${_completion_zsh}"
      DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_completions.sh" "trurl.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/_trurl.zsh.in"
      VERBATIM
    )
    add_custom_target(trurl-completion-zsh ALL DEPENDS "${_completion_zsh}")

    if(NOT TRURL_DISABLE_INSTALL)
      install(FILES "${_completion_zsh}" DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/zsh/site-functions")
    endif()
  endif()
endif()

# Tests

option(TRURL_TESTS "Run tests (requires Python)" ON)
if(TRURL_TESTS)
  find_package(Python)
  if(NOT Python_FOUND)
    message(WARNING "Python not found, cannot run tests")
  else()
    add_custom_target(trurl-test
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      COMMAND "${Python_EXECUTABLE}" "test.py" "--trurl=$<TARGET_FILE:trurl>"
      DEPENDS "trurl" "test.py" "tests.json"
      VERBATIM USES_TERMINAL
    )
    if(NOT APPLE AND NOT WIN32)
      add_custom_target(trurl-test-memory
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMAND "${Python_EXECUTABLE}" "test.py" "--trurl=$<TARGET_FILE:trurl>" --with-valgrind
        DEPENDS "trurl" "test.py" "tests.json"
        VERBATIM USES_TERMINAL
      )
    endif()
  endif()
endif()
