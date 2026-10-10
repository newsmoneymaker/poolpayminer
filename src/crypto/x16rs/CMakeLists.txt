cmake_minimum_required(VERSION 3.10)
project(x16rs C)

# Hacash's own X16RS hash, vendored verbatim from the x16rs-sys crate (crates.io =0.1.2) that the
# real Hacash node links -- never hand-modified, never borrowed from another coin. x16rs.c
# amalgamates its whole classic-SPH chain via #include (one translation unit); the same chain is
# already vendored under the same symbol names in src/crypto/ghostrider/ and
# src/crypto/flex/sph/, so x16rs_rename.h (force-included below) renames every overlapping symbol
# before this TU is compiled -- see critical-vendored-lib-symbol-collision memory. Verified after
# renaming: output still matches the known-good vector (height 789517 live block, nonce 12345).
set(SOURCES
    x16rs.c
)

include_directories(.)
include_directories(sha3)

if (CMAKE_C_COMPILER_ID MATCHES GNU OR CMAKE_C_COMPILER_ID MATCHES Clang)
    set_source_files_properties(${SOURCES} PROPERTIES COMPILE_FLAGS "-O2 -w -include ${CMAKE_CURRENT_SOURCE_DIR}/x16rs_rename.h")
endif()

add_library(x16rs STATIC ${SOURCES})
