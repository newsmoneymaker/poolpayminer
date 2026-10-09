/*
---------------------------------------------------------------------------
Copyright (c) 1998-2013, Brian Gladman, Worcester, UK. All rights reserved.

The redistribution and use of this software (with or without changes)
is allowed without the payment of fees or royalties provided that:

  source code distributions include the above copyright notice, this
  list of conditions and the following disclaimer;

  binary distributions include the above copyright notice, this list
  of conditions and the following disclaimer in their documentation.

This software is provided 'as is' with no explicit or implied warranties
in respect of its operation, including, but not limited to, correctness
and fitness for purpose.
---------------------------------------------------------------------------
Issue Date: 20/12/2007
*/

#include <stdint.h>
#include <string.h>
#include "aesb.h"

#if defined(FLEX_FORCE_SOFTWARE_AES)
#define AESB_X86 0
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
#include <intrin.h>
#include <immintrin.h>
#define AESB_X86 1
#elif (defined(__GNUC__) || defined(__clang__)) && (defined(__x86_64__) || defined(__i386__))
#include <immintrin.h>
#define AESB_X86 1
#else
#define AESB_X86 0
#endif

#if AESB_X86 && (defined(__GNUC__) || defined(__clang__))
#define AESB_TARGET_AES __attribute__((target("aes,sse2")))
#define AESB_TARGET_VAES256 __attribute__((target("vaes,avx2")))
#define AESB_TARGET_VAES512 __attribute__((target("vaes,avx512f")))
#else
#define AESB_TARGET_AES
#define AESB_TARGET_VAES256
#define AESB_TARGET_VAES512
#endif

#if defined(__cplusplus)
extern "C"
{
#endif

#define TABLE_ALIGN     32
#define WPOLY           0x011b
#define N_COLS          4
#define AES_BLOCK_SIZE  16
#define RC_LENGTH       (5 * (AES_BLOCK_SIZE / 4 - 2))

#if defined(_MSC_VER)
#define ALIGN __declspec(align(TABLE_ALIGN))
#elif defined(__GNUC__)
#define ALIGN __attribute__ ((aligned(16)))
#else
#define ALIGN
#endif

#if defined(__GNUC__) || defined(__clang__)
typedef uint32_t aesb_u32_alias __attribute__((may_alias));
#else
typedef uint32_t aesb_u32_alias;
#endif

#define rf1(r,c) (r)
#define word_in(x,c) (*((const aesb_u32_alias*)(x)+(c)))
#define word_out(x,c,v) (*((aesb_u32_alias*)(x)+(c)) = (v))

#define s(x,c) x[c]
#define si(y,x,c) (s(y,c) = word_in(x, c))
#define so(y,x,c) word_out(y, c, s(x,c))
#define state_in(y,x) si(y,x,0); si(y,x,1); si(y,x,2); si(y,x,3)
#define state_out(y,x)  so(y,x,0); so(y,x,1); so(y,x,2); so(y,x,3)
#define round(rm,y,x,k) rm(y,x,k,0); rm(y,x,k,1); rm(y,x,k,2); rm(y,x,k,3)
#define to_byte(x) ((x) & 0xff)
#define bval(x,n) to_byte((x) >> (8 * (n)))

#define fwd_var(x,r,c)\
 ( r == 0 ? ( c == 0 ? s(x,0) : c == 1 ? s(x,1) : c == 2 ? s(x,2) : s(x,3))\
 : r == 1 ? ( c == 0 ? s(x,1) : c == 1 ? s(x,2) : c == 2 ? s(x,3) : s(x,0))\
 : r == 2 ? ( c == 0 ? s(x,2) : c == 1 ? s(x,3) : c == 2 ? s(x,0) : s(x,1))\
 :          ( c == 0 ? s(x,3) : c == 1 ? s(x,0) : c == 2 ? s(x,1) : s(x,2)))

#define fwd_rnd(y,x,k,c)  (s(y,c) = (k)[c] ^ four_tables(x,t_use(f,n),fwd_var,rf1,c))

#define sb_data(w) {\
    w(0x63), w(0x7c), w(0x77), w(0x7b), w(0xf2), w(0x6b), w(0x6f), w(0xc5),\
    w(0x30), w(0x01), w(0x67), w(0x2b), w(0xfe), w(0xd7), w(0xab), w(0x76),\
    w(0xca), w(0x82), w(0xc9), w(0x7d), w(0xfa), w(0x59), w(0x47), w(0xf0),\
    w(0xad), w(0xd4), w(0xa2), w(0xaf), w(0x9c), w(0xa4), w(0x72), w(0xc0),\
    w(0xb7), w(0xfd), w(0x93), w(0x26), w(0x36), w(0x3f), w(0xf7), w(0xcc),\
    w(0x34), w(0xa5), w(0xe5), w(0xf1), w(0x71), w(0xd8), w(0x31), w(0x15),\
    w(0x04), w(0xc7), w(0x23), w(0xc3), w(0x18), w(0x96), w(0x05), w(0x9a),\
    w(0x07), w(0x12), w(0x80), w(0xe2), w(0xeb), w(0x27), w(0xb2), w(0x75),\
    w(0x09), w(0x83), w(0x2c), w(0x1a), w(0x1b), w(0x6e), w(0x5a), w(0xa0),\
    w(0x52), w(0x3b), w(0xd6), w(0xb3), w(0x29), w(0xe3), w(0x2f), w(0x84),\
    w(0x53), w(0xd1), w(0x00), w(0xed), w(0x20), w(0xfc), w(0xb1), w(0x5b),\
    w(0x6a), w(0xcb), w(0xbe), w(0x39), w(0x4a), w(0x4c), w(0x58), w(0xcf),\
    w(0xd0), w(0xef), w(0xaa), w(0xfb), w(0x43), w(0x4d), w(0x33), w(0x85),\
    w(0x45), w(0xf9), w(0x02), w(0x7f), w(0x50), w(0x3c), w(0x9f), w(0xa8),\
    w(0x51), w(0xa3), w(0x40), w(0x8f), w(0x92), w(0x9d), w(0x38), w(0xf5),\
    w(0xbc), w(0xb6), w(0xda), w(0x21), w(0x10), w(0xff), w(0xf3), w(0xd2),\
    w(0xcd), w(0x0c), w(0x13), w(0xec), w(0x5f), w(0x97), w(0x44), w(0x17),\
    w(0xc4), w(0xa7), w(0x7e), w(0x3d), w(0x64), w(0x5d), w(0x19), w(0x73),\
    w(0x60), w(0x81), w(0x4f), w(0xdc), w(0x22), w(0x2a), w(0x90), w(0x88),\
    w(0x46), w(0xee), w(0xb8), w(0x14), w(0xde), w(0x5e), w(0x0b), w(0xdb),\
    w(0xe0), w(0x32), w(0x3a), w(0x0a), w(0x49), w(0x06), w(0x24), w(0x5c),\
    w(0xc2), w(0xd3), w(0xac), w(0x62), w(0x91), w(0x95), w(0xe4), w(0x79),\
    w(0xe7), w(0xc8), w(0x37), w(0x6d), w(0x8d), w(0xd5), w(0x4e), w(0xa9),\
    w(0x6c), w(0x56), w(0xf4), w(0xea), w(0x65), w(0x7a), w(0xae), w(0x08),\
    w(0xba), w(0x78), w(0x25), w(0x2e), w(0x1c), w(0xa6), w(0xb4), w(0xc6),\
    w(0xe8), w(0xdd), w(0x74), w(0x1f), w(0x4b), w(0xbd), w(0x8b), w(0x8a),\
    w(0x70), w(0x3e), w(0xb5), w(0x66), w(0x48), w(0x03), w(0xf6), w(0x0e),\
    w(0x61), w(0x35), w(0x57), w(0xb9), w(0x86), w(0xc1), w(0x1d), w(0x9e),\
    w(0xe1), w(0xf8), w(0x98), w(0x11), w(0x69), w(0xd9), w(0x8e), w(0x94),\
    w(0x9b), w(0x1e), w(0x87), w(0xe9), w(0xce), w(0x55), w(0x28), w(0xdf),\
    w(0x8c), w(0xa1), w(0x89), w(0x0d), w(0xbf), w(0xe6), w(0x42), w(0x68),\
    w(0x41), w(0x99), w(0x2d), w(0x0f), w(0xb0), w(0x54), w(0xbb), w(0x16) }

#define rc_data(w) {\
    w(0x01), w(0x02), w(0x04), w(0x08), w(0x10),w(0x20), w(0x40), w(0x80),\
    w(0x1b), w(0x36) }

#define bytes2word(b0, b1, b2, b3) (((uint32_t)(b3) << 24) | \
    ((uint32_t)(b2) << 16) | ((uint32_t)(b1) << 8) | (b0))

#define h0(x)   (x)
#define w0(p)   bytes2word(p, 0, 0, 0)
#define w1(p)   bytes2word(0, p, 0, 0)
#define w2(p)   bytes2word(0, 0, p, 0)
#define w3(p)   bytes2word(0, 0, 0, p)

#define u0(p)   bytes2word(f2(p), p, p, f3(p))
#define u1(p)   bytes2word(f3(p), f2(p), p, p)
#define u2(p)   bytes2word(p, f3(p), f2(p), p)
#define u3(p)   bytes2word(p, p, f3(p), f2(p))

#define v0(p)   bytes2word(fe(p), f9(p), fd(p), fb(p))
#define v1(p)   bytes2word(fb(p), fe(p), f9(p), fd(p))
#define v2(p)   bytes2word(fd(p), fb(p), fe(p), f9(p))
#define v3(p)   bytes2word(f9(p), fd(p), fb(p), fe(p))

#define f2(x)   ((x<<1) ^ (((x>>7) & 1) * WPOLY))
#define f4(x)   ((x<<2) ^ (((x>>6) & 1) * WPOLY) ^ (((x>>6) & 2) * WPOLY))
#define f8(x)   ((x<<3) ^ (((x>>5) & 1) * WPOLY) ^ (((x>>5) & 2) * WPOLY) ^ (((x>>5) & 4) * WPOLY))
#define f3(x)   (f2(x) ^ x)
#define f9(x)   (f8(x) ^ x)
#define fb(x)   (f8(x) ^ f2(x) ^ x)
#define fd(x)   (f8(x) ^ f4(x) ^ x)
#define fe(x)   (f8(x) ^ f4(x) ^ f2(x))

#define t_dec(m,n) t_##m##n
#define t_set(m,n) t_##m##n
#define t_use(m,n) t_##m##n

#define d_4(t,n,b,e,f,g,h) ALIGN const t n[4][256] = { b(e), b(f), b(g), b(h) }

#define four_tables(x,tab,vf,rf,c) \
    (tab[0][bval(vf(x,0,c),rf(0,c))] \
    ^ tab[1][bval(vf(x,1,c),rf(1,c))] \
    ^ tab[2][bval(vf(x,2,c),rf(2,c))] \
    ^ tab[3][bval(vf(x,3,c),rf(3,c))])

d_4(uint32_t, t_dec(f,n), sb_data, u0, u1, u2, u3);

static void aesb_single_round_soft(const uint8_t *in, uint8_t *out,
                                    const uint8_t *expandedKey)
{
    uint32_t b0[4], b1[4];
    const aesb_u32_alias *kp = (const aesb_u32_alias *) expandedKey;
    state_in(b0, in);

    round(fwd_rnd, b1, b0, kp);

    state_out(out, b1);
}

static void aesb_pseudo_round_soft(const uint8_t *in, uint8_t *out,
                                   const uint8_t *expandedKey)
{
    uint32_t b0[4], b1[4];
    const aesb_u32_alias *kp = (const aesb_u32_alias *) expandedKey;
    state_in(b0, in);

    round(fwd_rnd, b1, b0, kp);
    round(fwd_rnd, b0, b1, kp + 1 * N_COLS);
    round(fwd_rnd, b1, b0, kp + 2 * N_COLS);
    round(fwd_rnd, b0, b1, kp + 3 * N_COLS);
    round(fwd_rnd, b1, b0, kp + 4 * N_COLS);
    round(fwd_rnd, b0, b1, kp + 5 * N_COLS);
    round(fwd_rnd, b1, b0, kp + 6 * N_COLS);
    round(fwd_rnd, b0, b1, kp + 7 * N_COLS);
    round(fwd_rnd, b1, b0, kp + 8 * N_COLS);
    round(fwd_rnd, b0, b1, kp + 9 * N_COLS);

    state_out(out, b0);
}

static void aesb_pseudo_round_x8_soft(const uint8_t *in, uint8_t *out,
                                      const uint8_t *expandedKey)
{
    unsigned i;
    for (i = 0; i < 8; ++i) {
        aesb_pseudo_round_soft(in + i * AES_BLOCK_SIZE,
                              out + i * AES_BLOCK_SIZE, expandedKey);
    }
}

static void aesb_pseudo_round_x8_xor_soft(uint8_t *inout,
                                          const uint8_t *xorData,
                                          const uint8_t *expandedKey)
{
    unsigned i;
    for (i = 0; i < 8; ++i) {
        uint64_t d0, d1, s0, s1;
        uint8_t *dst = inout + i * AES_BLOCK_SIZE;
        const uint8_t *src = xorData + i * AES_BLOCK_SIZE;
        memcpy(&d0, dst + 0, sizeof(d0));
        memcpy(&d1, dst + 8, sizeof(d1));
        memcpy(&s0, src + 0, sizeof(s0));
        memcpy(&s1, src + 8, sizeof(s1));
        d0 ^= s0;
        d1 ^= s1;
        memcpy(dst + 0, &d0, sizeof(d0));
        memcpy(dst + 8, &d1, sizeof(d1));
        aesb_pseudo_round_soft(dst, dst, expandedKey);
    }
}

#if AESB_X86
AESB_TARGET_AES
static void aesb_single_round_aesni(const uint8_t *in, uint8_t *out,
                                    const uint8_t *expandedKey)
{
    const __m128i x = _mm_loadu_si128((const __m128i *) in);
    const __m128i k = _mm_loadu_si128((const __m128i *) expandedKey);
    _mm_storeu_si128((__m128i *) out, _mm_aesenc_si128(x, k));
}

AESB_TARGET_AES
static void aesb_pseudo_round_x8_aesni(const uint8_t *in, uint8_t *out,
                                       const uint8_t *expandedKey)
{
    __m128i x0 = _mm_loadu_si128((const __m128i *)(in + 0 * AES_BLOCK_SIZE));
    __m128i x1 = _mm_loadu_si128((const __m128i *)(in + 1 * AES_BLOCK_SIZE));
    __m128i x2 = _mm_loadu_si128((const __m128i *)(in + 2 * AES_BLOCK_SIZE));
    __m128i x3 = _mm_loadu_si128((const __m128i *)(in + 3 * AES_BLOCK_SIZE));
    __m128i x4 = _mm_loadu_si128((const __m128i *)(in + 4 * AES_BLOCK_SIZE));
    __m128i x5 = _mm_loadu_si128((const __m128i *)(in + 5 * AES_BLOCK_SIZE));
    __m128i x6 = _mm_loadu_si128((const __m128i *)(in + 6 * AES_BLOCK_SIZE));
    __m128i x7 = _mm_loadu_si128((const __m128i *)(in + 7 * AES_BLOCK_SIZE));
    unsigned r;

    for (r = 0; r < 10; ++r) {
        const __m128i k = _mm_loadu_si128((const __m128i *)(expandedKey + r * AES_BLOCK_SIZE));
        x0 = _mm_aesenc_si128(x0, k);
        x1 = _mm_aesenc_si128(x1, k);
        x2 = _mm_aesenc_si128(x2, k);
        x3 = _mm_aesenc_si128(x3, k);
        x4 = _mm_aesenc_si128(x4, k);
        x5 = _mm_aesenc_si128(x5, k);
        x6 = _mm_aesenc_si128(x6, k);
        x7 = _mm_aesenc_si128(x7, k);
    }

    _mm_storeu_si128((__m128i *)(out + 0 * AES_BLOCK_SIZE), x0);
    _mm_storeu_si128((__m128i *)(out + 1 * AES_BLOCK_SIZE), x1);
    _mm_storeu_si128((__m128i *)(out + 2 * AES_BLOCK_SIZE), x2);
    _mm_storeu_si128((__m128i *)(out + 3 * AES_BLOCK_SIZE), x3);
    _mm_storeu_si128((__m128i *)(out + 4 * AES_BLOCK_SIZE), x4);
    _mm_storeu_si128((__m128i *)(out + 5 * AES_BLOCK_SIZE), x5);
    _mm_storeu_si128((__m128i *)(out + 6 * AES_BLOCK_SIZE), x6);
    _mm_storeu_si128((__m128i *)(out + 7 * AES_BLOCK_SIZE), x7);
}

AESB_TARGET_AES
static void aesb_pseudo_round_x8_xor_aesni(uint8_t *inout,
                                           const uint8_t *xorData,
                                           const uint8_t *expandedKey)
{
    __m128i x0 = _mm_xor_si128(_mm_loadu_si128((const __m128i *)(inout + 0 * AES_BLOCK_SIZE)), _mm_loadu_si128((const __m128i *)(xorData + 0 * AES_BLOCK_SIZE)));
    __m128i x1 = _mm_xor_si128(_mm_loadu_si128((const __m128i *)(inout + 1 * AES_BLOCK_SIZE)), _mm_loadu_si128((const __m128i *)(xorData + 1 * AES_BLOCK_SIZE)));
    __m128i x2 = _mm_xor_si128(_mm_loadu_si128((const __m128i *)(inout + 2 * AES_BLOCK_SIZE)), _mm_loadu_si128((const __m128i *)(xorData + 2 * AES_BLOCK_SIZE)));
    __m128i x3 = _mm_xor_si128(_mm_loadu_si128((const __m128i *)(inout + 3 * AES_BLOCK_SIZE)), _mm_loadu_si128((const __m128i *)(xorData + 3 * AES_BLOCK_SIZE)));
    __m128i x4 = _mm_xor_si128(_mm_loadu_si128((const __m128i *)(inout + 4 * AES_BLOCK_SIZE)), _mm_loadu_si128((const __m128i *)(xorData + 4 * AES_BLOCK_SIZE)));
    __m128i x5 = _mm_xor_si128(_mm_loadu_si128((const __m128i *)(inout + 5 * AES_BLOCK_SIZE)), _mm_loadu_si128((const __m128i *)(xorData + 5 * AES_BLOCK_SIZE)));
    __m128i x6 = _mm_xor_si128(_mm_loadu_si128((const __m128i *)(inout + 6 * AES_BLOCK_SIZE)), _mm_loadu_si128((const __m128i *)(xorData + 6 * AES_BLOCK_SIZE)));
    __m128i x7 = _mm_xor_si128(_mm_loadu_si128((const __m128i *)(inout + 7 * AES_BLOCK_SIZE)), _mm_loadu_si128((const __m128i *)(xorData + 7 * AES_BLOCK_SIZE)));
    unsigned r;

    for (r = 0; r < 10; ++r) {
        const __m128i k = _mm_loadu_si128((const __m128i *)(expandedKey + r * AES_BLOCK_SIZE));
        x0 = _mm_aesenc_si128(x0, k);
        x1 = _mm_aesenc_si128(x1, k);
        x2 = _mm_aesenc_si128(x2, k);
        x3 = _mm_aesenc_si128(x3, k);
        x4 = _mm_aesenc_si128(x4, k);
        x5 = _mm_aesenc_si128(x5, k);
        x6 = _mm_aesenc_si128(x6, k);
        x7 = _mm_aesenc_si128(x7, k);
    }

    _mm_storeu_si128((__m128i *)(inout + 0 * AES_BLOCK_SIZE), x0);
    _mm_storeu_si128((__m128i *)(inout + 1 * AES_BLOCK_SIZE), x1);
    _mm_storeu_si128((__m128i *)(inout + 2 * AES_BLOCK_SIZE), x2);
    _mm_storeu_si128((__m128i *)(inout + 3 * AES_BLOCK_SIZE), x3);
    _mm_storeu_si128((__m128i *)(inout + 4 * AES_BLOCK_SIZE), x4);
    _mm_storeu_si128((__m128i *)(inout + 5 * AES_BLOCK_SIZE), x5);
    _mm_storeu_si128((__m128i *)(inout + 6 * AES_BLOCK_SIZE), x6);
    _mm_storeu_si128((__m128i *)(inout + 7 * AES_BLOCK_SIZE), x7);
}

#if defined(__GNUC__) || defined(__clang__)
AESB_TARGET_VAES256
static void aesb_pseudo_round_x8_vaes256(const uint8_t *in, uint8_t *out,
                                         const uint8_t *expandedKey)
{
    __m256i x0 = _mm256_loadu_si256((const __m256i *)(in + 0));
    __m256i x1 = _mm256_loadu_si256((const __m256i *)(in + 32));
    __m256i x2 = _mm256_loadu_si256((const __m256i *)(in + 64));
    __m256i x3 = _mm256_loadu_si256((const __m256i *)(in + 96));
    unsigned r;

    for (r = 0; r < 10; ++r) {
        const __m128i rk = _mm_loadu_si128((const __m128i *)(expandedKey + r * AES_BLOCK_SIZE));
        const __m256i k = _mm256_broadcastsi128_si256(rk);
        x0 = _mm256_aesenc_epi128(x0, k);
        x1 = _mm256_aesenc_epi128(x1, k);
        x2 = _mm256_aesenc_epi128(x2, k);
        x3 = _mm256_aesenc_epi128(x3, k);
    }

    _mm256_storeu_si256((__m256i *)(out + 0), x0);
    _mm256_storeu_si256((__m256i *)(out + 32), x1);
    _mm256_storeu_si256((__m256i *)(out + 64), x2);
    _mm256_storeu_si256((__m256i *)(out + 96), x3);
}

AESB_TARGET_VAES256
static void aesb_pseudo_round_x8_xor_vaes256(uint8_t *inout,
                                             const uint8_t *xorData,
                                             const uint8_t *expandedKey)
{
    __m256i x0 = _mm256_xor_si256(_mm256_loadu_si256((const __m256i *)(inout + 0)), _mm256_loadu_si256((const __m256i *)(xorData + 0)));
    __m256i x1 = _mm256_xor_si256(_mm256_loadu_si256((const __m256i *)(inout + 32)), _mm256_loadu_si256((const __m256i *)(xorData + 32)));
    __m256i x2 = _mm256_xor_si256(_mm256_loadu_si256((const __m256i *)(inout + 64)), _mm256_loadu_si256((const __m256i *)(xorData + 64)));
    __m256i x3 = _mm256_xor_si256(_mm256_loadu_si256((const __m256i *)(inout + 96)), _mm256_loadu_si256((const __m256i *)(xorData + 96)));
    unsigned r;

    for (r = 0; r < 10; ++r) {
        const __m128i rk = _mm_loadu_si128((const __m128i *)(expandedKey + r * AES_BLOCK_SIZE));
        const __m256i k = _mm256_broadcastsi128_si256(rk);
        x0 = _mm256_aesenc_epi128(x0, k);
        x1 = _mm256_aesenc_epi128(x1, k);
        x2 = _mm256_aesenc_epi128(x2, k);
        x3 = _mm256_aesenc_epi128(x3, k);
    }

    _mm256_storeu_si256((__m256i *)(inout + 0), x0);
    _mm256_storeu_si256((__m256i *)(inout + 32), x1);
    _mm256_storeu_si256((__m256i *)(inout + 64), x2);
    _mm256_storeu_si256((__m256i *)(inout + 96), x3);
}

AESB_TARGET_VAES512
static void aesb_pseudo_round_x8_vaes512(const uint8_t *in, uint8_t *out,
                                         const uint8_t *expandedKey)
{
    __m512i x0 = _mm512_loadu_si512((const void *)(in + 0));
    __m512i x1 = _mm512_loadu_si512((const void *)(in + 64));
    unsigned r;

    for (r = 0; r < 10; ++r) {
        const __m128i rk = _mm_loadu_si128((const __m128i *)(expandedKey + r * AES_BLOCK_SIZE));
        const __m512i k = _mm512_broadcast_i32x4(rk);
        x0 = _mm512_aesenc_epi128(x0, k);
        x1 = _mm512_aesenc_epi128(x1, k);
    }

    _mm512_storeu_si512((void *)(out + 0), x0);
    _mm512_storeu_si512((void *)(out + 64), x1);
}

AESB_TARGET_VAES512
static void aesb_pseudo_round_x8_xor_vaes512(uint8_t *inout,
                                             const uint8_t *xorData,
                                             const uint8_t *expandedKey)
{
    __m512i x0 = _mm512_xor_si512(_mm512_loadu_si512((const void *)(inout + 0)), _mm512_loadu_si512((const void *)(xorData + 0)));
    __m512i x1 = _mm512_xor_si512(_mm512_loadu_si512((const void *)(inout + 64)), _mm512_loadu_si512((const void *)(xorData + 64)));
    unsigned r;

    for (r = 0; r < 10; ++r) {
        const __m128i rk = _mm_loadu_si128((const __m128i *)(expandedKey + r * AES_BLOCK_SIZE));
        const __m512i k = _mm512_broadcast_i32x4(rk);
        x0 = _mm512_aesenc_epi128(x0, k);
        x1 = _mm512_aesenc_epi128(x1, k);
    }

    _mm512_storeu_si512((void *)(inout + 0), x0);
    _mm512_storeu_si512((void *)(inout + 64), x1);
}
#endif
#endif

void aesb_single_round(const uint8_t *in, uint8_t *out, uint8_t *expandedKey)
{
    aesb_single_round_soft(in, out, expandedKey);
}

void aesb_pseudo_round(const uint8_t *in, uint8_t *out, uint8_t *expandedKey)
{
    aesb_pseudo_round_soft(in, out, expandedKey);
}

#if AESB_X86 && defined(__APPLE__) && \
    (defined(__GNUC__) || defined(__clang__))

static void aesb_cpuid(uint32_t leaf, uint32_t subleaf,
                       uint32_t *eax, uint32_t *ebx,
                       uint32_t *ecx, uint32_t *edx)
{
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;

#if defined(__i386__) && defined(__PIC__)
    __asm__ volatile(
        "xchgl %%ebx, %1\\n\\t"
        "cpuid\\n\\t"
        "xchgl %%ebx, %1"
        : "=a"(a), "=&r"(b), "=c"(c), "=d"(d)
        : "0"(leaf), "2"(subleaf)
        : "cc"
    );
#else
    __asm__ volatile(
        "cpuid"
        : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
        : "0"(leaf), "2"(subleaf)
        : "cc"
    );
#endif

    *eax = a;
    *ebx = b;
    *ecx = c;
    *edx = d;
}

static uint64_t aesb_xgetbv0(void)
{
    uint32_t eax;
    uint32_t edx;

    __asm__ volatile(
        ".byte 0x0f, 0x01, 0xd0"
        : "=a"(eax), "=d"(edx)
        : "c"(0)
        : "cc"
    );

    return ((uint64_t)edx << 32) | eax;
}

#endif

static int aesb_cpu_has_aes(void)
{
#if AESB_X86 && defined(__APPLE__) && \
    (defined(__GNUC__) || defined(__clang__))
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;

    aesb_cpuid(0, 0, &eax, &ebx, &ecx, &edx);
    if (eax < 1) {
        return 0;
    }

    aesb_cpuid(1, 0, &eax, &ebx, &ecx, &edx);
    return (ecx & (1u << 25)) != 0;
#elif AESB_X86 && (defined(__GNUC__) || defined(__clang__))
    return __builtin_cpu_supports("aes") != 0;
#elif AESB_X86 && defined(_MSC_VER)
    int regs[4];
    __cpuid(regs, 1);
    return (regs[2] & (1 << 25)) != 0;
#else
    return 0;
#endif
}

static int aesb_cpu_has_vaes256(void)
{
#if AESB_X86 && defined(__APPLE__) && \
    (defined(__GNUC__) || defined(__clang__))
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t max_leaf;
    uint64_t xcr0;

    aesb_cpuid(0, 0, &max_leaf, &ebx, &ecx, &edx);
    if (max_leaf < 7) {
        return 0;
    }

    aesb_cpuid(1, 0, &eax, &ebx, &ecx, &edx);
    if ((ecx & ((1u << 27) | (1u << 28))) !=
        ((1u << 27) | (1u << 28))) {
        return 0;
    }

    xcr0 = aesb_xgetbv0();
    if ((xcr0 & 0x6u) != 0x6u) {
        return 0;
    }

    aesb_cpuid(7, 0, &eax, &ebx, &ecx, &edx);
    return (ebx & (1u << 5)) != 0 &&
           (ecx & (1u << 9)) != 0;
#elif AESB_X86 && (defined(__GNUC__) || defined(__clang__)) && (!defined(__GNUC__) || __GNUC__ >= 9)
    return __builtin_cpu_supports("vaes") &&
           __builtin_cpu_supports("avx2");
#else
    // GCC < 9 (this host: 8.3) doesn't recognize "vaes" as a valid __builtin_cpu_supports() string
    // (hard compile error, not a runtime miss) -- just report no VAES256 fast path on those compilers.
    // Correctness is unaffected: aesb_get_pseudo_round_x8_fn() falls back to plain AES-NI, same hash output.
    return 0;
#endif
}

static int aesb_cpu_has_vaes512(void)
{
#if AESB_X86 && defined(__APPLE__) && \
    (defined(__GNUC__) || defined(__clang__))
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t max_leaf;
    uint64_t xcr0;

    aesb_cpuid(0, 0, &max_leaf, &ebx, &ecx, &edx);
    if (max_leaf < 7) {
        return 0;
    }

    aesb_cpuid(1, 0, &eax, &ebx, &ecx, &edx);
    if ((ecx & ((1u << 27) | (1u << 28))) !=
        ((1u << 27) | (1u << 28))) {
        return 0;
    }

    xcr0 = aesb_xgetbv0();
    if ((xcr0 & 0xe6u) != 0xe6u) {
        return 0;
    }

    aesb_cpuid(7, 0, &eax, &ebx, &ecx, &edx);
    return (ebx & (1u << 16)) != 0 &&
           (ecx & (1u << 9)) != 0;
#elif AESB_X86 && (defined(__GNUC__) || defined(__clang__)) && (!defined(__GNUC__) || __GNUC__ >= 9)
    return __builtin_cpu_supports("vaes") &&
           __builtin_cpu_supports("avx512f");
#else
    // see aesb_cpu_has_vaes256() above -- same GCC < 9 compile-error avoidance, same correctness note.
    return 0;
#endif
}

int aesb_has_hardware_aes(void)
{
    return aesb_cpu_has_aes();
}

aesb_single_round_fn aesb_get_single_round_fn(void)
{
#if AESB_X86
    if (aesb_cpu_has_aes()) return aesb_single_round_aesni;
#endif
    return aesb_single_round_soft;
}

aesb_pseudo_round_x8_fn aesb_get_pseudo_round_x8_fn(void)
{
#if AESB_X86 && (defined(__GNUC__) || defined(__clang__))
    if (aesb_cpu_has_vaes512()) return aesb_pseudo_round_x8_vaes512;
    if (aesb_cpu_has_vaes256()) return aesb_pseudo_round_x8_vaes256;
#endif
#if AESB_X86
    if (aesb_cpu_has_aes()) return aesb_pseudo_round_x8_aesni;
#endif
    return aesb_pseudo_round_x8_soft;
}

aesb_pseudo_round_x8_xor_fn aesb_get_pseudo_round_x8_xor_fn(void)
{
#if AESB_X86 && (defined(__GNUC__) || defined(__clang__))
    if (aesb_cpu_has_vaes512()) return aesb_pseudo_round_x8_xor_vaes512;
    if (aesb_cpu_has_vaes256()) return aesb_pseudo_round_x8_xor_vaes256;
#endif
#if AESB_X86
    if (aesb_cpu_has_aes()) return aesb_pseudo_round_x8_xor_aesni;
#endif
    return aesb_pseudo_round_x8_xor_soft;
}

const char *aesb_backend_name(void)
{
#if AESB_X86 && (defined(__GNUC__) || defined(__clang__))
    if (aesb_cpu_has_vaes512()) return "VAES-512";
    if (aesb_cpu_has_vaes256()) return "VAES-256";
#endif
#if AESB_X86
    if (aesb_cpu_has_aes()) return "AES-NI";
#endif
    return "software";
}


#if defined(__cplusplus)
}
#endif
