# Copyright 2026 ALTESSA SOLUTIONS INC.
# Use of this source code is governed by an MIT-style license that can be
# found in the LICENSE file.

cmake_minimum_required(VERSION 3.21)

# The version lives in the public header and nowhere else: read out of it
# here rather than written a second time, so that a release is one edit and the
# library cannot report a number its package does not carry.
foreach(part MAJOR MINOR PATCH)
    file(STRINGS include/snnote/snnote.h line
         REGEX "^#define SNNOTE_VERSION_${part} [0-9]+$")
    if(NOT line MATCHES "^#define SNNOTE_VERSION_${part} ([0-9]+)$")
        message(FATAL_ERROR "SNNOTE_VERSION_${part} not found in include/snnote/snnote.h")
    endif()
    set(SNNOTE_VERSION_${part} "${CMAKE_MATCH_1}")
endforeach()

project(snnote
    VERSION ${SNNOTE_VERSION_MAJOR}.${SNNOTE_VERSION_MINOR}.${SNNOTE_VERSION_PATCH}
    DESCRIPTION "Reading the Supernote .note and .mark notebook format"
    HOMEPAGE_URL "https://github.com/altessa-s/snnote-lib"
    LANGUAGES C)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(SNNOTE_BUILD_TESTS "Build the test suite" ${PROJECT_IS_TOP_LEVEL})
option(SNNOTE_BUILD_TOOLS "Build the sndump command-line tool" ${PROJECT_IS_TOP_LEVEL})
option(SNNOTE_WERROR "Turn compiler warnings into errors" OFF)
option(SNNOTE_LTO "Link-time optimization for optimized shared builds" ON)
option(SNNOTE_BUILD_FUZZERS "Build the libFuzzer targets (clang only)" OFF)

# ---------------------------------------------------------------------------
# The library
# ---------------------------------------------------------------------------

set(SNNOTE_SOURCES
    src/sn_support.c src/sn_decimal.c src/sn_bytes.c src/sn_log.c src/sn_model.c
    src/sn_metadata.c src/sn_base64.c src/sn_json.c src/sn_lz4.c
    src/sn_rle.c src/sn_raster.c src/sn_strokes.c src/sn_annotations.c src/sn_parser.c)
set(SNNOTE_PUBLIC_HEADERS
    include/snnote/snnote.h include/snnote/export.h include/snnote/types.h
    include/snnote/log.h include/snnote/parser.h include/snnote/raster.h)

# Honours BUILD_SHARED_LIBS: static by default, shared with
# -DBUILD_SHARED_LIBS=ON. One target either way, so the exported CMake package
# and the pkg-config file describe whatever was built.
add_library(snnote ${SNNOTE_SOURCES})
add_library(SNNote::snnote ALIAS snnote)

target_include_directories(snnote
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(SNNOTE_SOVERSION ${PROJECT_VERSION_MAJOR})
if(PROJECT_VERSION_MAJOR EQUAL 0)
    set(SNNOTE_SOVERSION "0.${PROJECT_VERSION_MINOR}")
endif()

set_target_properties(snnote PROPERTIES
    C_STANDARD 23
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
    # The plumbing — the arena and the byte reader — is not part of
    # the contract and must not reach a consumer's symbol table.
    C_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    POSITION_INDEPENDENT_CODE ON
    VERSION ${PROJECT_VERSION}
    SOVERSION ${SNNOTE_SOVERSION}
    PUBLIC_HEADER "${SNNOTE_PUBLIC_HEADERS}")

if(BUILD_SHARED_LIBS)
    target_compile_definitions(snnote PRIVATE SNNOTE_BUILDING_SHARED)
    if(APPLE)
        # GCC otherwise appends runtime exports even with an explicit symbol list.
        if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
            target_link_options(snnote PRIVATE -nodefaultexport)
        endif()
        set(SNNOTE_EXPORTS "")
        foreach(header IN LISTS SNNOTE_PUBLIC_HEADERS)
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${header}")
            file(READ "${header}" declarations)
            string(REGEX MATCHALL "SN_API[^;]*sn_[A-Za-z0-9_]+[ \t\r\n]*\\(" declarations "${declarations}")
            foreach(declaration IN LISTS declarations)
                string(REGEX MATCH "sn_[A-Za-z0-9_]+[ \t\r\n]*\\(" symbol "${declaration}")
                string(REGEX REPLACE "[ \t\r\n]*\\($" "" symbol "${symbol}")
                string(APPEND SNNOTE_EXPORTS "_${symbol}\n")
            endforeach()
        endforeach()
        file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/snnote.exports" "${SNNOTE_EXPORTS}")
        target_link_options(snnote PRIVATE
            "LINKER:-exported_symbols_list,${CMAKE_CURRENT_BINARY_DIR}/snnote.exports")
        set_property(TARGET snnote APPEND PROPERTY LINK_DEPENDS
            "${CMAKE_CURRENT_BINARY_DIR}/snnote.exports")
    endif()
endif()

# Link-time optimization for the shared library only: a static archive built
# with it holds compiler bitcode that another compiler cannot link.
if(BUILD_SHARED_LIBS AND SNNOTE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT SNNOTE_IPO OUTPUT SNNOTE_IPO_ERROR LANGUAGES C)
    if(SNNOTE_IPO)
        set_target_properties(snnote PROPERTIES
            INTERPROCEDURAL_OPTIMIZATION_RELEASE ON
            INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON
            INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL ON)
    else()
        message(STATUS "snnote: link-time optimization unavailable: ${SNNOTE_IPO_ERROR}")
    endif()
endif()

# The warning set, and it is one set: the library is built with clang or
# gcc on every platform, Windows included. MSVC's C compiler stops at the
# first `constexpr`, `nullptr`, `[[nodiscard]]` and typed enum — all of them
# C23, all of them throughout — and a build that carries a second warning
# dialect for a compiler that cannot read the code is a build with a branch
# nobody can exercise.
target_compile_options(snnote PRIVATE
    -Wall -Wextra -Wshadow -Wconversion -Wsign-conversion
    -Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith -Wcast-qual)
if(SNNOTE_WERROR)
    target_compile_options(snnote PRIVATE -Werror)
endif()
# `isfinite` lives in libm on the platforms that still split it out.
include(CheckLibraryExists)
check_library_exists(m ceil "" SNNOTE_HAVE_LIBM)
if(SNNOTE_HAVE_LIBM)
    target_link_libraries(snnote PUBLIC m)
endif()

# ---------------------------------------------------------------------------
# The tool
# ---------------------------------------------------------------------------

if(SNNOTE_BUILD_TOOLS)
    add_executable(sndump tools/sndump.c tests/sn_composition.c)
    target_include_directories(sndump PRIVATE tests)
    target_link_libraries(sndump PRIVATE snnote)
    set_target_properties(sndump PROPERTIES C_STANDARD 23 C_STANDARD_REQUIRED ON
                                            C_EXTENSIONS OFF)
endif()

# ---------------------------------------------------------------------------
# The tests
# ---------------------------------------------------------------------------

if(SNNOTE_BUILD_TESTS)
    enable_testing()
    include(CheckLanguage)
    check_language(CXX)
    if(CMAKE_CXX_COMPILER)
        enable_language(CXX)
        add_executable(consumer_cpp EXCLUDE_FROM_ALL tests/consumer.cpp)
        target_link_libraries(consumer_cpp PRIVATE snnote)
        set_target_properties(consumer_cpp PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
        add_test(NAME consumer_cpp COMMAND consumer_cpp "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/supernote-typescript/heading-n5-20260016-backgrounds-marker.note")
    endif()

    # The tests reach past the public API — a count that wraps and a UTF-8
    # repair are read where the arithmetic is, not through a whole page — so
    # they compile the sources again rather than link the installed contract.
    add_library(snnote_testing STATIC EXCLUDE_FROM_ALL ${SNNOTE_SOURCES} tests/sn_composition.c)
    target_include_directories(snnote_testing PUBLIC tests)
    target_compile_definitions(snnote_testing PUBLIC SNNOTE_TESTING)
    target_include_directories(snnote_testing PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src)
    set_target_properties(snnote_testing PROPERTIES
        C_STANDARD 23 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
    if(SNNOTE_HAVE_LIBM)
        target_link_libraries(snnote_testing PUBLIC m)
    endif()

    foreach(suite corpus metadata rle lz4 base64 json palette bitmap strokes classification annotations recovery mutation log geometry allocation decimal log_threads errors highlights recognition budget abi fuzz_harness)
        add_executable(test_${suite} EXCLUDE_FROM_ALL tests/test_${suite}.c)
        target_link_libraries(test_${suite} PRIVATE snnote_testing)
        target_include_directories(test_${suite} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
        target_compile_definitions(test_${suite} PRIVATE
            SN_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
        set_target_properties(test_${suite} PROPERTIES
            C_STANDARD 23 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
        add_test(NAME ${suite} COMMAND test_${suite})
    endforeach()

    add_executable(generate_seeds EXCLUDE_FROM_ALL tests/generate_seeds.c)
    target_link_libraries(generate_seeds PRIVATE snnote_testing)
    set_target_properties(generate_seeds PROPERTIES C_STANDARD 23 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/seeds")
    add_custom_target(seed_corpus
        COMMAND generate_seeds "${CMAKE_CURRENT_BINARY_DIR}/seeds"
        DEPENDS generate_seeds)

    target_compile_definitions(test_mutation PRIVATE SN_SEED_DIR="${CMAKE_CURRENT_BINARY_DIR}/seeds")
    add_dependencies(test_mutation seed_corpus)

    include(CheckIncludeFile)
    check_include_file(threads.h SNNOTE_HAVE_THREADS_H)
    find_package(Threads QUIET)
    if(Threads_FOUND)
        target_link_libraries(test_log_threads PRIVATE Threads::Threads)
        # Prefer the API intercepted by ThreadSanitizer. glibc's thrd_create
        # can call an internal pthread entry point, bypassing TSan's
        # pthread_create interceptor and leaving workers unregistered.
        if(CMAKE_USE_PTHREADS_INIT)
            target_compile_definitions(test_log_threads PRIVATE SN_HAVE_PTHREADS)
        elseif(SNNOTE_HAVE_THREADS_H)
            target_compile_definitions(test_log_threads PRIVATE SN_HAVE_C_THREADS)
        endif()
    endif()

    if(SNNOTE_BUILD_TOOLS)
        add_executable(test_sndump EXCLUDE_FROM_ALL tests/test_sndump.c)
        target_link_libraries(test_sndump PRIVATE snnote_testing)
        set_target_properties(test_sndump PROPERTIES
            C_STANDARD 23 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
        add_test(NAME sndump COMMAND test_sndump)
        add_test(NAME dumps COMMAND ${CMAKE_COMMAND}
            -DTOOL=$<TARGET_FILE:sndump> -DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}
            -DBINARY=${CMAKE_CURRENT_BINARY_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_dumps.cmake)
    endif()

    # Damaged input must fail the test instead of hanging the CI worker.
    set_tests_properties(recovery PROPERTIES TIMEOUT 30)

    # The parser against bytes nobody wrote. Off by default and clang-only:
    # libFuzzer is a clang runtime, and the sanitizers it wants are the CI
    # job's to ask for. The sample fixtures are its seed corpus.
    if(SNNOTE_BUILD_FUZZERS)
        add_executable(fuzz_parse tests/fuzz_parse.c)
        target_link_libraries(fuzz_parse PRIVATE snnote_testing)
        target_compile_options(fuzz_parse PRIVATE -fsanitize=fuzzer)
        target_link_options(fuzz_parse PRIVATE -fsanitize=fuzzer)
        set_target_properties(fuzz_parse PROPERTIES
            C_STANDARD 23 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
    endif()

    # `ctest` builds nothing on its own, and a suite that was never compiled is
    # a suite that silently did not run.
    add_custom_target(build_tests DEPENDS seed_corpus
        test_corpus test_metadata test_rle test_lz4 test_base64 test_json test_palette test_bitmap test_strokes test_classification test_annotations test_recovery test_mutation test_log test_geometry test_allocation test_decimal test_log_threads test_errors test_highlights test_recognition test_budget test_abi test_fuzz_harness)
    if(CMAKE_CXX_COMPILER)
        add_dependencies(build_tests consumer_cpp)
    endif()
    if(SNNOTE_BUILD_TOOLS)
        add_dependencies(build_tests sndump test_sndump)
    endif()
    # Under a multi-configuration generator — Xcode, Visual Studio — a
    # `ctest` that is not told which configuration to run finds no tests at
    # all and says so five times, so the configuration is passed along. Passed
    # conditionally: under a single-configuration generator with no build type
    # set, `$<CONFIG>` is empty, and `-C ""` is an error of its own. The
    # expression yields the two arguments or nothing, and COMMAND_EXPAND_LISTS
    # is what lets "nothing" vanish rather than arrive as an empty argument.
    add_custom_target(check
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
                "$<$<BOOL:$<CONFIG>>:-C;$<CONFIG>>"
        DEPENDS build_tests
        COMMAND_EXPAND_LISTS)
endif()

# ---------------------------------------------------------------------------
# Installation
# ---------------------------------------------------------------------------

install(FILES LICENSE COPYRIGHT DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/snnote)

install(TARGETS snnote
    EXPORT SNNoteTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/snnote)

if(SNNOTE_BUILD_TOOLS)
    install(TARGETS sndump RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

install(EXPORT SNNoteTargets
    FILE SNNoteTargets.cmake
    NAMESPACE SNNote::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SNNote)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/SNNoteConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/SNNoteConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SNNote)

set(SNNOTE_COMPATIBILITY SameMajorVersion)
if(PROJECT_VERSION_MAJOR EQUAL 0)
    set(SNNOTE_COMPATIBILITY SameMinorVersion)
endif()

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/SNNoteConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ${SNNOTE_COMPATIBILITY})

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/SNNoteConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/SNNoteConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SNNote)

# pkg-config, for the builds that never learned CMake.
#
# The way back from the `.pc` to the prefix is worked out here rather than
# written as a fixed `../..`: CMAKE_INSTALL_LIBDIR is `lib` on one
# distribution, `lib64` on another and `lib/x86_64-linux-gnu` on a third, and
# only one of those is two levels down.
file(RELATIVE_PATH SNNOTE_PC_PREFIX_RELATIVE
     "${CMAKE_INSTALL_FULL_LIBDIR}/pkgconfig" "${CMAKE_INSTALL_PREFIX}")
if(SNNOTE_PC_PREFIX_RELATIVE STREQUAL "")
    set(SNNOTE_PC_PREFIX_RELATIVE ".")
endif()
string(REGEX REPLACE "/$" "" SNNOTE_PC_PREFIX_RELATIVE "${SNNOTE_PC_PREFIX_RELATIVE}")

set(SNNOTE_PC_LIBS_PRIVATE "")
if(SNNOTE_HAVE_LIBM)
    set(SNNOTE_PC_LIBS_PRIVATE "-lm")
endif()
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/snnote.pc.in
               ${CMAKE_CURRENT_BINARY_DIR}/snnote.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/snnote.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
