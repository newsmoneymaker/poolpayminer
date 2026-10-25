/* Auto-generated: force-included into every file under src/crypto/kryptokrona/ only, to keep this coin's
   own vendored CryptoNote crypto (copied verbatim from the Kryptokrona (XKR) node's own src/crypto/, see
   kryptokrona-project.md) from colliding with the near-identical, same-named classic CryptoNote crypto
   already vendored elsewhere in this project (src/crypto/flex/, src/crypto/flex_lcn/, GhostRider's own SPH
   library, and XMRig's own built-in cn/ implementation) -- all statically linked into the same xmrig binary.
   Every symbol below was renamed with an xkr_ prefix, confirmed via `nm -g --defined-only` on this exact
   source set. See critical-wrong-randomx-variant-bug / critical-vendored-lib-symbol-collision. */
#ifndef XKR_RENAME_H
#define XKR_RENAME_H

#define aesb_pseudo_round       xkr_aesb_pseudo_round
#define aesb_single_round       xkr_aesb_single_round
#define blake256_compress       xkr_blake256_compress
#define blake256_final          xkr_blake256_final
#define blake256_final_h        xkr_blake256_final_h
#define blake256_hash           xkr_blake256_hash
#define blake256_init           xkr_blake256_init
#define blake256_update         xkr_blake256_update
#define cn_fast_hash            xkr_cn_fast_hash
#define cn_slow_hash            xkr_cn_slow_hash
#define cpuid                   xkr_cpuid
#define cst                     xkr_cst
#define E8_bitslice_roundconstant xkr_E8_bitslice_roundconstant
#define groestl                 xkr_groestl
#define hash_extra_blake        xkr_hash_extra_blake
#define hash_extra_groestl      xkr_hash_extra_groestl
#define hash_extra_jh           xkr_hash_extra_jh
#define hash_extra_skein        xkr_hash_extra_skein
#define hash_permutation        xkr_hash_permutation
#define hash_process            xkr_hash_process
#define hp_allocated            xkr_hp_allocated
#define hp_state                xkr_hp_state
#define indices_cyclic          xkr_indices_cyclic
#define JH224_H0                xkr_JH224_H0
#define JH256_H0                xkr_JH256_H0
#define JH384_H0                xkr_JH384_H0
#define JH512_H0                xkr_JH512_H0
#define jh_hash                 xkr_jh_hash
#define keccak                  xkr_keccak
#define keccak1600              xkr_keccak1600
#define keccakf                 xkr_keccakf
#define keccakf_piln            xkr_keccakf_piln
#define keccakf_rndc            xkr_keccakf_rndc
#define keccakf_rotc            xkr_keccakf_rotc
#define oaes_alloc              xkr_oaes_alloc
#define oaes_decrypt            xkr_oaes_decrypt
#define oaes_encrypt            xkr_oaes_encrypt
#define oaes_encryption_round   xkr_oaes_encryption_round
#define oaes_free               xkr_oaes_free
#define oaes_key_export         xkr_oaes_key_export
#define oaes_key_export_data    xkr_oaes_key_export_data
#define oaes_key_gen_128        xkr_oaes_key_gen_128
#define oaes_key_gen_192        xkr_oaes_key_gen_192
#define oaes_key_gen_256        xkr_oaes_key_gen_256
#define oaes_key_import         xkr_oaes_key_import
#define oaes_key_import_data    xkr_oaes_key_import_data
#define oaes_pseudo_encrypt_ecb xkr_oaes_pseudo_encrypt_ecb
#define oaes_set_option         xkr_oaes_set_option
#define oaes_sprintf            xkr_oaes_sprintf
#define shift_Values            xkr_shift_Values
#define sigma                   xkr_sigma
#define SKEIN1024_IV_1024       xkr_SKEIN1024_IV_1024
#define SKEIN1024_IV_384        xkr_SKEIN1024_IV_384
#define SKEIN1024_IV_512        xkr_SKEIN1024_IV_512
#define SKEIN_256_IV_128        xkr_SKEIN_256_IV_128
#define SKEIN_256_IV_160        xkr_SKEIN_256_IV_160
#define SKEIN_256_IV_224        xkr_SKEIN_256_IV_224
#define SKEIN_256_IV_256        xkr_SKEIN_256_IV_256
#define SKEIN_512_IV_128        xkr_SKEIN_512_IV_128
#define SKEIN_512_IV_160        xkr_SKEIN_512_IV_160
#define SKEIN_512_IV_224        xkr_SKEIN_512_IV_224
#define SKEIN_512_IV_256        xkr_SKEIN_512_IV_256
#define SKEIN_512_IV_384        xkr_SKEIN_512_IV_384
#define SKEIN_512_IV_512        xkr_SKEIN_512_IV_512
#define skein_hash              xkr_skein_hash
#define slow_hash_allocate_state xkr_slow_hash_allocate_state
#define slow_hash_free_state    xkr_slow_hash_free_state
#define T                       xkr_T
#define t_fn                    xkr_t_fn
#define tree_branch             xkr_tree_branch
#define tree_depth              xkr_tree_depth
#define tree_hash               xkr_tree_hash
#define tree_hash_from_branch   xkr_tree_hash_from_branch

#endif /* XKR_RENAME_H */
