#include <cstring>

#include "ccx_gpu_hash.h"
#include "cn_slow_hash.hpp"


// Cross-checked against a pristine, unmodified build of the same upstream files (ConcealNetwork/conceal-core
// src/crypto/pow_hash/, hardware AES-NI+AVX2 path -- the path any present-day x86_64 CPU takes) for 3 inputs;
// see ccx_project.md / critical-wrong-randomx-variant-bug for why this cross-check matters.
static const uint8_t test_input_empty[1]  = { 0 }; // length 0 is passed explicitly below, content unused
static const char    test_input_tis[]     = "This is a test";
static const uint8_t test_output_empty[32] = {
    0x55, 0x5e, 0x0a, 0xee, 0x78, 0x79, 0x31, 0x6d, 0x7d, 0xef, 0xf7, 0x72, 0x97, 0x3c, 0xb9, 0x11,
    0x8e, 0x38, 0x95, 0x70, 0x9d, 0xb2, 0x54, 0x7a, 0xc0, 0x72, 0xd5, 0xb9, 0x13, 0x10, 0x01, 0xd8
};
static const uint8_t test_output_tis[32] = {
    0x23, 0x7c, 0x8b, 0x8d, 0xd2, 0x5d, 0xb6, 0x78, 0xa9, 0xb9, 0x35, 0x38, 0xb9, 0x60, 0xd3, 0x7f,
    0x66, 0x3e, 0x1a, 0x0c, 0x1a, 0xf4, 0xbc, 0x1d, 0xab, 0xed, 0x48, 0x7d, 0x60, 0x62, 0xcc, 0x87
};
static const uint8_t test_input_zero76[76] = { 0 };
static const uint8_t test_output_zero76[32] = {
    0x23, 0x24, 0x05, 0xb4, 0xd6, 0xdb, 0x9e, 0xbf, 0x06, 0xa9, 0xbf, 0xb0, 0xd5, 0x0e, 0x0e, 0x73,
    0xfe, 0x21, 0x2b, 0xca, 0x7a, 0x02, 0x6a, 0x7e, 0x6b, 0xf4, 0xff, 0x1f, 0xe5, 0xd8, 0x8c, 0xa2
};


void ccxGpuHash(const uint8_t *input, size_t size, uint8_t *output)
{
    static thread_local ccxgpu::cn_pow_hash_v3 ctx;
    ctx.hash(input, size, output);
}


bool ccxGpuSelfTest()
{
    uint8_t out[32];

    ccxGpuHash(test_input_empty, 0, out);
    if (memcmp(out, test_output_empty, 32) != 0) {
        return false;
    }

    ccxGpuHash(reinterpret_cast<const uint8_t *>(test_input_tis), sizeof(test_input_tis) - 1, out);
    if (memcmp(out, test_output_tis, 32) != 0) {
        return false;
    }

    ccxGpuHash(test_input_zero76, sizeof(test_input_zero76), out);
    if (memcmp(out, test_output_zero76, 32) != 0) {
        return false;
    }

    return true;
}
