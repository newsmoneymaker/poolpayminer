cmake_minimum_required(VERSION 3.10)
project(kryptokrona)

# Kryptokrona (XKR)'s OWN real cn_turtle_lite_slow_hash_v2, copied verbatim from the coin's own node source
# (src/crypto/ on srv3, 213.239.211.209) -- see kryptokrona-project.md and critical-wrong-randomx-variant-bug.
# Deliberately excludes crypto-ops.c/crypto-ops-data.c (Ed25519 curve math, confirmed via `nm -u` that no file
# in this set actually references ge_*/sc_*/fe_*) to keep the vendored surface minimal.
set(VENDORED_SOURCES
    aesb.c
    blake256.c
    groestl.c
    hash.c
    hash-extra-blake.c
    hash-extra-groestl.c
    hash-extra-jh.c
    hash-extra-skein.c
    jh.c
    keccak.c
    oaes_lib.c
    skein.c
    slow-hash.c
    tree-hash.c
)

include_directories(.)
include_directories(Common)

if (CMAKE_C_COMPILER_ID MATCHES GNU OR CMAKE_C_COMPILER_ID MATCHES Clang)
    # Every exported symbol in this directory (aesb_*, oaes_*, hash_*, cn_slow_hash, keccak*, skein_hash,
    # groestl, jh_hash, blake256_*, tree_hash, SKEIN_*_IV_*, JH*_H0, etc.) was renamed with an xkr_ prefix,
    # both in source text AND via this force-included header (covers macro/token-paste-generated names a
    # plain text rename can't reach) -- see xkr_rename.h and critical-vendored-lib-symbol-collision.
    # -std=gnu11: hash-ops.h uses static_assert(), a C11 feature -- the mingw cross compiler's default C
    # standard doesn't enable it the way the Linux host's gcc 8.3 default (gnu11) implicitly does.
    set_source_files_properties(${VENDORED_SOURCES} PROPERTIES COMPILE_FLAGS "-O2 -w -std=gnu11 -maes -msse2 -msse4.1 -mssse3 -include ${CMAKE_CURRENT_SOURCE_DIR}/xkr_rename.h")
endif()

add_library(kryptokrona STATIC xkr_hash.cpp ${VENDORED_SOURCES})
