cmake_minimum_required(VERSION 3.10)
project(flex_lcn)

# Lyncoin's OWN Flex sources (copied verbatim from the coin's own node, src/crypto/flex/ on srv2's lcn-src --
# see critical-wrong-randomx-variant-bug lesson: never borrow a sibling coin's hash code, and never reuse
# Kylacoin's own src/crypto/flex/ for this -- confirmed genuinely different cnfn.c/aesb.c/flex.cpp content
# between the two coins (different md5, different cnfn_*_hash signatures: LCN uses the plain cnfn_*_hash
# calls, KCN uses the _scratch variants with an external scratchpad buffer)). Same file list as Kylacoin's
# own vendored copy, for the same reason (matches the node's own build file list) -- gost_streebog.c/haval.c/
# jh.c/tiger.c/sph_shabal's 192/224/384 variants are dead code w.r.t. flex_hash's actual 18-round Algo switch,
# but are compiled anyway for fidelity with the node's own Makefile. aes_helper.c/md_helper.c/haval_helper.c/
# hamsi_helper.c/sph_sha2big.c are textually #included by their parent .c files, not separate translation
# units -- excluded from SOURCES on purpose.
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
    # LCN's own Flex vendors nearly the exact same classic SPH hash library (same symbol names) as BOTH
    # GhostRider (src/crypto/ghostrider/) AND Kylacoin's own already-vendored Flex port (src/crypto/flex/,
    # itself already renamed with a flex_ prefix for the same reason) -- all three are statically linked
    # into the same xmrig binary. Every exported symbol in this directory (sph_*, cnfn_*, aesb_*, oaes_*,
    # keccak*, LYRA2*, SKEIN_*_IV_*, sha256_init/transform, the flex_hash entry point itself, etc. -- 411
    # distinct names, confirmed via `nm -g --defined-only` diffed against every already-linked vendor lib)
    # was renamed directly in source with an lcnflex_ prefix. A few files (whirlpool.c, sha2.c, haval_helper.c,
    # hamsi_helper.c) generate their function bodies via X-macro token-pasting (SPH_XCAT(sph_, HASH) in
    # md_helper.c) so the plain name never appears as literal text for a source-level rename to catch --
    # lcnflex_rename.h covers that gap: #define-renaming the plain name catches it even when token-pasted.
    set_source_files_properties(${SOURCES} PROPERTIES COMPILE_FLAGS "-O2 -w -maes -msse4.1 -include ${CMAKE_CURRENT_SOURCE_DIR}/sph/lcnflex_rename.h")
endif()

add_library(flex_lcn STATIC ${SOURCES})
