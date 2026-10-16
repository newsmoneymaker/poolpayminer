cmake_minimum_required(VERSION 3.10)
project(verus)

# VerusHash2.2 (VerusCoin / VRSC). Sources in crypto/ are vendored+stripped from VerusCoin's own node
# (src/crypto/verus_hash.{h,cpp}, verus_clhash.{h,cpp}, haraka.{h,c}) -- NOT a from-scratch
# reimplementation. Byte-verified against the real node's own compiled output across multiple test
# vectors before vendoring -- see vrsc-project.md. Haraka's generic plain-C symbols are renamed
# verushash_* to avoid colliding with another vendored coin's crypto (critical-vendored-lib-symbol-
# collision memory) -- EXCEPT the bare "rc" global, left deliberately un-renamed: see the long comment
# at the top of crypto/haraka.h for why renaming it silently breaks the hash.
set(SOURCES
    verus.cpp
    crypto/haraka.c
    crypto/verus_hash.cpp
    crypto/verus_clhash.cpp
    crypto/verus_clhash_support.cpp
)

include_directories(.)
include_directories(crypto)

if (CMAKE_C_COMPILER_ID MATCHES GNU OR CMAKE_C_COMPILER_ID MATCHES Clang)
    # ISA-optimized path only (AES-NI + PCLMUL, no AVX2 needed) -- no portable/ARM fallback was ported.
    # CVerusHash(V2)::init() aborts with a clear message on a CPU that lacks these instead of silently
    # computing a wrong hash.
    set_source_files_properties(${SOURCES} PROPERTIES COMPILE_FLAGS "-O2 -maes -mpclmul -msse4.2 -mssse3")
endif()

add_library(verus STATIC ${SOURCES})
