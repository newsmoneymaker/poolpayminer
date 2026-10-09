// Copyright (c) 2021 The Raptoreum Project
// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
// Portions Copyright (c) 2018 The Monero developers
// Portions Copyright (c) 2018 The TurtleCoin Developers
// Portions Copyright (c) 2024 Flex Labs Developers

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "cnfn.h"
#include "crypto/oaes_lib.h"
#include "crypto/aesb.h"
#include "crypto/c_keccak.h"
#include "crypto/c_groestl.h"
#include "crypto/c_blake256.h"
#include "crypto/c_skein.h"
#include "crypto/int-util.h"
#include "crypto/variant2_int_sqrt.h"

#if defined(_MSC_VER)
#include <malloc.h>
#endif

#if !defined(FLEX_FORCE_SOFTWARE_AES) && \
    ((defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))) || \
     ((defined(__GNUC__) || defined(__clang__)) && (defined(__x86_64__) || defined(__i386__))))
#include <immintrin.h>
#define CNFN_X86_AES 1
#else
#define CNFN_X86_AES 0
#endif

#if CNFN_X86_AES && (defined(__GNUC__) || defined(__clang__))
#define CNFN_TARGET_AES __attribute__((target("aes,sse2")))
#else
#define CNFN_TARGET_AES
#endif

#define AES_BLOCK_SIZE  16
#define AES_KEY_SIZE    32 /*16*/
#define INIT_SIZE_BLK   8
#define INIT_SIZE_BYTE  (INIT_SIZE_BLK * AES_BLOCK_SIZE)

#define VARIANT1_1(p) \
  do if (variant == 1) \
  { \
    const uint8_t tmp = ((const uint8_t*)(p))[11]; \
    static const uint32_t table = 0x75310; \
    const uint8_t index = (((tmp >> 3) & 6) | (tmp & 1)) << 1; \
    ((uint8_t*)(p))[11] = tmp ^ ((table >> index) & 0x30); \
  } while(0)

#define VARIANT1_2(p) \
   do if (variant == 1) \
   { \
     ((uint64_t*)p)[1] ^= tweak1_2; \
   } while(0)

#define VARIANT1_INIT() \
  if (variant == 1 && len < 43) \
  { \
    fprintf(stderr, "CNFN variant 1 needs at least 43 bytes of data"); \
    _exit(1); \
  } \
  const uint64_t tweak1_2 = (variant == 1) ? *(const uint64_t*)(((const uint8_t*)input)+35) ^ state.hs.w[24] : 0

#define U64(p) ((uint64_t*)(p))

#define VARIANT2_INIT(b, state) \
  uint64_t division_result; \
  uint64_t sqrt_result; \
  do if (variant >= 2) \
  { \
    U64(b)[2] = state.hs.w[8] ^ state.hs.w[10]; \
    U64(b)[3] = state.hs.w[9] ^ state.hs.w[11]; \
    division_result = state.hs.w[12]; \
    sqrt_result = state.hs.w[13]; \
  } while (0)

#define VARIANT2_SHUFFLE_ADD(base_ptr, offset, a, b) \
  do if (variant >= 2) \
  { \
    uint64_t* chunk1 = U64((base_ptr) + ((offset) ^ 0x10)); \
    uint64_t* chunk2 = U64((base_ptr) + ((offset) ^ 0x20)); \
    uint64_t* chunk3 = U64((base_ptr) + ((offset) ^ 0x30)); \
    \
    const uint64_t chunk1_old[2] = { chunk1[0], chunk1[1] }; \
    \
    chunk1[0] = chunk3[0] + U64(b + 16)[0]; \
    chunk1[1] = chunk3[1] + U64(b + 16)[1]; \
    \
    chunk3[0] = chunk2[0] + U64(a)[0]; \
    chunk3[1] = chunk2[1] + U64(a)[1]; \
    \
    chunk2[0] = chunk1_old[0] + U64(b)[0]; \
    chunk2[1] = chunk1_old[1] + U64(b)[1]; \
    } while (0)

#define VARIANT2_INTEGER_MATH_DIVISION_STEP(b, ptr) \
  ((uint64_t*)(b))[0] ^= division_result ^ (sqrt_result << 32); \
  { \
    const uint64_t dividend = ((uint64_t*)(ptr))[1]; \
    const uint32_t divisor = (((uint32_t*)(ptr))[0] + (uint32_t)(sqrt_result << 1)) | 0x80000001UL; \
    division_result = ((uint32_t)(dividend / divisor)) + \
                     (((uint64_t)(dividend % divisor)) << 32); \
  } \
  const uint64_t sqrt_input = ((uint64_t*)(ptr))[0] + division_result

#define VARIANT2_INTEGER_MATH(b, ptr) \
    do if (variant >= 2) \
    { \
      VARIANT2_INTEGER_MATH_DIVISION_STEP(b, ptr); \
      VARIANT2_INTEGER_MATH_SQRT_STEP_FP64(); \
      VARIANT2_INTEGER_MATH_SQRT_FIXUP(sqrt_result); \
    } while (0)

#define VARIANT2_2() \
  do if (variant >= 2) { \
    ((uint64_t*)(long_state + ((j * AES_BLOCK_SIZE) ^ 0x10)))[0] ^= hi; \
    ((uint64_t*)(long_state + ((j * AES_BLOCK_SIZE) ^ 0x10)))[1] ^= lo; \
    hi ^= ((uint64_t*)(long_state + ((j * AES_BLOCK_SIZE) ^ 0x20)))[0]; \
    lo ^= ((uint64_t*)(long_state + ((j * AES_BLOCK_SIZE) ^ 0x20)))[1]; \
  } while (0)

#pragma pack(push, 1)
union cnfn_slow_hash_state {
    union hash_state hs;
    struct {
        uint8_t k[64];
        uint8_t init[INIT_SIZE_BYTE];
    };
};
#pragma pack(pop)

static void do_blake_hash(const void* input, size_t len, char* output) {
    flex_blake256_hash((uint8_t*)output, input, len);
}

void do_groestl_hash(const void* input, size_t len, char* output) {
    flex_groestl(input, len * 8, (uint8_t*)output);
}

static void do_skein_hash(const void* input, size_t len, char* output) {
    int r = c_skein_hash(8 * HASH_SIZE, input, 8 * len, (uint8_t*)output);
    assert(SKEIN_SUCCESS == r);
    (void)r;
}

static void (* const extra_hashes[3])(const void *, size_t, char *) = {
    do_blake_hash, do_groestl_hash, do_skein_hash
};

static inline size_t e2i(const uint8_t* a, size_t count) {
    return (*((uint64_t*) a) / AES_BLOCK_SIZE) & (count - 1);
}

static inline void copy_block(uint8_t* dst, const uint8_t* src) {
    ((uint64_t*) dst)[0] = ((uint64_t*) src)[0];
    ((uint64_t*) dst)[1] = ((uint64_t*) src)[1];
}

static inline void xor_blocks_dst(const uint8_t* a, const uint8_t* b, uint8_t* dst) {
    ((uint64_t*) dst)[0] = ((uint64_t*) a)[0] ^ ((uint64_t*) b)[0];
    ((uint64_t*) dst)[1] = ((uint64_t*) a)[1] ^ ((uint64_t*) b)[1];
}

#if CNFN_X86_AES
CNFN_TARGET_AES
static void cnfn_mix_v1_aesni(uint8_t* long_state, uint32_t iterations,
                               size_t aes_rounds, const uint8_t* a_init,
                               const uint8_t* b_init, uint64_t tweak1_2)
{
  uint64_t a0 = ((const uint64_t*)a_init)[0];
  uint64_t a1 = ((const uint64_t*)a_init)[1];
  uint64_t b0 = ((const uint64_t*)b_init)[0];
  uint64_t b1 = ((const uint64_t*)b_init)[1];
  const size_t address_mask = aes_rounds - 1;
  uint32_t i;

  for (i = 0; i < iterations; ++i) {
    uint64_t c_words[2];
    uint64_t c0, c1;
    size_t j = (size_t)((a0 >> 4) & address_mask);
    uint64_t* dst;
    uint64_t t0, t1;
    uint64_t hi;
    uint64_t lo;
    __m128i x = _mm_loadu_si128((const __m128i*)(long_state + j * AES_BLOCK_SIZE));
    const __m128i key = _mm_set_epi64x((long long)a1, (long long)a0);
    const __m128i cv = _mm_aesenc_si128(x, key);

    _mm_storeu_si128((__m128i*)c_words, cv);
    c0 = c_words[0];
    c1 = c_words[1];

    dst = (uint64_t*)(long_state + j * AES_BLOCK_SIZE);
    dst[0] = c0 ^ b0;
    dst[1] = c1 ^ b1;

    /* VARIANT1_1, specialized for variant == 1. */
    {
      uint8_t* p = (uint8_t*)dst;
      const uint8_t tmp = p[11];
      static const uint32_t table = 0x75310;
      const uint8_t index = (uint8_t)((((tmp >> 3) & 6) | (tmp & 1)) << 1);
      p[11] = (uint8_t)(tmp ^ ((table >> index) & 0x30));
    }

    j = (size_t)((c0 >> 4) & address_mask);
    dst = (uint64_t*)(long_state + j * AES_BLOCK_SIZE);
    t0 = dst[0];
    t1 = dst[1];

    lo = mul128(c0, t0, &hi);
    a0 += hi;
    a1 += lo;

    /* Store the pre-xor A value, then apply VARIANT1_2 to scratchpad word 1. */
    dst[0] = a0;
    dst[1] = a1 ^ tweak1_2;

    a0 ^= t0;
    a1 ^= t1;
    b0 = c0;
    b1 = c1;
  }
}
#endif

void cnfn_slow_hash_scratch(const char* input, char* output, uint32_t len, int variant,
                              uint32_t page_size, uint32_t iterations, size_t aes_rounds,
                              uint8_t* long_state, size_t scratchpad_size)
{
  union cnfn_slow_hash_state state;
  uint8_t text[INIT_SIZE_BYTE];
  uint8_t a[AES_BLOCK_SIZE];
  uint8_t b[AES_BLOCK_SIZE * 2];
  uint8_t c[AES_BLOCK_SIZE];
  uint8_t aes_key[AES_KEY_SIZE];
  oaes_ctx* aes_ctx;
  const aesb_single_round_fn single_round = aesb_get_single_round_fn();
  const aesb_pseudo_round_x8_fn pseudo_round_x8 = aesb_get_pseudo_round_x8_fn();
  const aesb_pseudo_round_x8_xor_fn pseudo_round_x8_xor = aesb_get_pseudo_round_x8_xor_fn();
  const size_t init_rounds = (page_size / INIT_SIZE_BYTE);
  size_t i, j;

  if (long_state == NULL || scratchpad_size < page_size) {
    fprintf(stderr, "CNFN scratchpad is too small: have %zu, need %u\n",
            scratchpad_size, page_size);
    abort();
  }

  hash_process(&state.hs, (const uint8_t*) input, len);
  memcpy(text, state.init, INIT_SIZE_BYTE);
  memcpy(aes_key, state.hs.b, AES_KEY_SIZE);
  aes_ctx = (oaes_ctx*) oaes_alloc();
  if (aes_ctx == NULL) {
    fprintf(stderr, "CNFN failed to allocate AES context\n");
    abort();
  }

  VARIANT1_INIT();
  VARIANT2_INIT(b, state);

  oaes_key_import_data(aes_ctx, aes_key, AES_KEY_SIZE);
  for (i = 0; i < init_rounds; i++) {
    /* Eight independent AES states: AES-NI/VAES backends interleave/vectorize them. */
    pseudo_round_x8(text, text, aes_ctx->key->exp_data);
    memcpy(&long_state[i * INIT_SIZE_BYTE], text, INIT_SIZE_BYTE);
  }

  for (i = 0; i < 16; i++) {
    a[i] = state.k[i] ^ state.k[32 + i];
    b[i] = state.k[16 + i] ^ state.k[48 + i];
  }

#if CNFN_X86_AES
  if (variant == 1 && aesb_has_hardware_aes()) {
    /* Flex uses variant 1. Keep the serial dependency chain in registers and
       issue AESENC directly, avoiding one indirect call and variant branches
       on every iteration. */
    cnfn_mix_v1_aesni(long_state, iterations, aes_rounds, a, b, tweak1_2);
  } else
#endif
  {
    for (i = 0; i < iterations; i++) {
      /* Dependency chain: address -> read value ------+
      * written value <-+ hard function (AES or MUL) <+
      * next address  <-+
      */
      /* Iteration 1 */
      j = e2i(a, aes_rounds);
      single_round(&long_state[j * AES_BLOCK_SIZE], c, a);
      VARIANT2_SHUFFLE_ADD(long_state, j * AES_BLOCK_SIZE, a, b);
      xor_blocks_dst(c, b, &long_state[j * AES_BLOCK_SIZE]);
      VARIANT1_1((uint8_t*)&long_state[j * AES_BLOCK_SIZE]);

      /* Iteration 2 */
      j = e2i(c, aes_rounds);

      {
        uint64_t* dst = (uint64_t*)&long_state[j * AES_BLOCK_SIZE];
        uint64_t t[2];
        uint64_t hi;
        uint64_t lo;

        t[0] = dst[0];
        t[1] = dst[1];

        VARIANT2_INTEGER_MATH(t, c);

        lo = mul128(((uint64_t*)c)[0], t[0], &hi);

        VARIANT2_2();
        VARIANT2_SHUFFLE_ADD(long_state, j * AES_BLOCK_SIZE, a, b);

        ((uint64_t*)a)[0] += hi;
        ((uint64_t*)a)[1] += lo;

        dst[0] = ((uint64_t*)a)[0];
        dst[1] = ((uint64_t*)a)[1];

        ((uint64_t*)a)[0] ^= t[0];
        ((uint64_t*)a)[1] ^= t[1];
      }

      VARIANT1_2((uint8_t*)&long_state[j * AES_BLOCK_SIZE]);
      copy_block(b + AES_BLOCK_SIZE, b);
      copy_block(b, c);
    }
  }

  memcpy(text, state.init, INIT_SIZE_BYTE);
  oaes_key_import_data(aes_ctx, &state.hs.b[32], AES_KEY_SIZE);
  for (i = 0; i < init_rounds; i++) {
    pseudo_round_x8_xor(text, &long_state[i * INIT_SIZE_BYTE], aes_ctx->key->exp_data);
  }

  memcpy(state.init, text, INIT_SIZE_BYTE);
  hash_permutation(&state.hs);
  extra_hashes[state.hs.b[0] & 2](&state, 200, output);
  oaes_free((OAES_CTX **) &aes_ctx);
}

void cnfn_slow_hash(const char* input, char* output, uint32_t len, int variant,
                    uint32_t page_size, uint32_t iterations, size_t aes_rounds)
{
  uint8_t* long_state = (uint8_t*) malloc(page_size);
  if (long_state == NULL) {
    fprintf(stderr, "CNFN failed to allocate %u-byte scratchpad\n", page_size);
    abort();
  }

  cnfn_slow_hash_scratch(input, output, len, variant, page_size, iterations,
                         aes_rounds, long_state, page_size);
  free(long_state);
}

void cnfn_fast_hash(const char* input, char* output, uint32_t len) {
    union hash_state state;
    hash_process(&state, (const uint8_t*) input, len);
    memcpy(output, &state, HASH_SIZE);
}
