cmake_minimum_required(VERSION 3.10)
project(yescrypt C)

# yescrypt (Alexander Peslyak / Solar Designer's scrypt successor) reference implementation and
# libcperciva SHA-256, taken from MateableCoin (mtbc) for yescryptr8/r16, plus a separate,
# self-contained yescryptr32 engine ported from LuckyPepe (lpepe)/WAVI (see yescrypt-r32.c for why
# it is not just the reference engine with a different personalization string). Used by MTBC, FNNC
# (Fennec, and Gold Cash which shares the same algorithm) and LPEPE (see README and
# THIRD-PARTY-NOTICES.txt).
set(SOURCES
    sha256_Y.c
    yescrypt.c
    yescrypt-opt.c
    yescrypt-r32.c
)

set_source_files_properties(${SOURCES} PROPERTIES COMPILE_FLAGS "-O2")
if (CMAKE_C_COMPILER_ID MATCHES GNU OR CMAKE_C_COMPILER_ID MATCHES Clang)
    set_source_files_properties(${SOURCES} PROPERTIES COMPILE_FLAGS "-O2 -w")
endif()

include_directories(.)

add_library(yescrypt STATIC ${SOURCES})
