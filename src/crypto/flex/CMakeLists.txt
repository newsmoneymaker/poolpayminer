cmake_minimum_required(VERSION 3.10)
project(flex)

# Kylacoin/Lyncoin's own Flex sources (copied verbatim from the coin's own node, src/crypto/flex/ -- see
# critical-wrong-randomx-variant-bug lesson: never borrow a sibling coin's hash code). Same file list as the
# pool's own server-side share-validation hasher (kcn-nodejs-pool/hasher/Makefile), known to build and match the
# node's own GetPoWHash(). aes_helper.c/md_helper.c/haval_helper.c/hamsi_helper.c/sph_sha2big.c are textually
# #included by their parent .c files, not separate translation units -- excluded from SOURCES on purpose.
set(SOURCES
    flex.cpp
    sph/blake.c
    sph/bmw.c
    sph/cubehash.c
    sph/echo.c
    sph/extra.c
    sph/fugue.c
    sph/gost_streebog.c
    sph/groestl.c
    sph/hamsi.c
    sph/haval.c
    sph/jh.c
    sph/keccak.c
    sph/luffa.c
    sph/lyra2.c
    sph/sha2.c
    sph/shabal.c
    sph/shavite.c
    sph/simd.c
    sph/skein.c
    sph/sph_sha2.c
    sph/sponge.c
    sph/tiger.c
    sph/whirlpool.c
    cnfiles/cnfn.c
    cnfiles/crypto/aesb.c
    cnfiles/crypto/c_blake256.c
    cnfiles/crypto/c_groestl.c
    cnfiles/crypto/c_jh.c
    cnfiles/crypto/c_keccak.c
    cnfiles/crypto/c_skein.c
    cnfiles/crypto/oaes_lib.c
    cnfiles/crypto/hash.c
)

include_directories(.)
include_directories(sph)
include_directories(cnfiles)
include_directories(cnfiles/crypto)

if (CMAKE_C_COMPILER_ID MATCHES GNU OR CMAKE_C_COMPILER_ID MATCHES Clang)
    # GhostRider (src/crypto/ghostrider/) vendors the exact same classic SPH hash library under the exact same
    # symbol names (sph_blake512_init, sph_keccak512, ...). Both libraries are statically linked into the same
    # xmrig binary; most of flex's own sph/*.c occurrences were renamed directly in source (see the symbol-rename
    # pass in flex.cpp/sph/*.{c,h}), but a few files (whirlpool.c, sha2.c) generate their sph_* function bodies via
    # X-macro token-pasting (SPH_XCAT(sph_, HASH) in md_helper.c) so the plain name never appears as literal text
    # -- those kept defining the UN-renamed names, which GhostRider's archive satisfied first at link time, so
    # flex_hash() was silently calling GHOSTRIDER'S sph_whirlpool/etc instead of its own (produced a real but
    # WRONG hash, confirmed by comparing an identical standalone build of libflex.a against the embedded one).
    # flex_rename.h covers that gap: #define-renaming the plain name catches it even when token-pasted.
    set_source_files_properties(${SOURCES} PROPERTIES COMPILE_FLAGS "-O2 -w -maes -msse4.1 -include ${CMAKE_CURRENT_SOURCE_DIR}/sph/flex_rename.h")
endif()

add_library(flex STATIC ${SOURCES})
