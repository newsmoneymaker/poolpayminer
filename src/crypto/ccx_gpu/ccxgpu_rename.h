/* Auto-generated: force-included into every vendored .c/.cpp file under src/crypto/ccx_gpu/ only. These
   are Conceal (CCX)'s real CryptoNight-GPU PoW sources (Ryo Currency Project, Copyright (c) 2019 -- see
   each file's own header), vendored verbatim from ConcealNetwork/conceal-core's src/crypto/pow_hash/ and
   src/crypto/keccak.c, needed because the real network hard-forked to block_major_version 8 (CN-GPU, not
   CRYPTONIGHT_CONCEAL/cn_ccx) and poolpayminer had no implementation of it at all. The C++ class/template
   code (cn_slow_hash.hpp and its 3 .cpp files) is wrapped in its own `namespace ccxgpu` and needs no
   renaming; only the plain-C keccak.c/aux_hash.c symbols below (extern "C", unmangled, called unqualified
   from inside that namespace) can collide with this project's own same-named CN finalizer functions
   (blake256_hash/skein_hash/groestl_hash/jh_hash/keccak/keccakf already exist under other vendored coins,
   e.g. kryptokrona's xkr_rename.h) -- renamed here the same way, confirmed via `nm -g --defined-only`.
   See critical-vendored-lib-symbol-collision. */
#ifndef CCXGPU_RENAME_H
#define CCXGPU_RENAME_H

#define keccak                      ccxgpu_keccak
#define keccakf                     ccxgpu_keccakf
#define keccak1600                  ccxgpu_keccak1600
#define keccakf_rndc                ccxgpu_keccakf_rndc
#define keccakf_rotc                ccxgpu_keccakf_rotc
#define keccakf_piln                ccxgpu_keccakf_piln

#define blake256_compress           ccxgpu_blake256_compress
#define blake256_hash               ccxgpu_blake256_hash
#define groestl_hash                ccxgpu_groestl_hash
#define jh_hash                     ccxgpu_jh_hash
#define skein_hash                  ccxgpu_skein_hash
#define sigma                       ccxgpu_sigma
#define cst                         ccxgpu_cst
#define T                           ccxgpu_T
#define JH256_H0                    ccxgpu_JH256_H0
#define E8_bitslice_roundconstant   ccxgpu_E8_bitslice_roundconstant
#define SKEIN_512_IV_256            ccxgpu_SKEIN_512_IV_256

#endif /* CCXGPU_RENAME_H */
