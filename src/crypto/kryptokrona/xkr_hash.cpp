#include "xkr_hash.h"


extern "C" void xkr_cn_slow_hash(const void *data, size_t length, char *hash,
                                  int light, int variant, int prehashed,
                                  uint32_t page_size, uint32_t scratchpad, uint32_t iterations);


void xkrHash(const uint8_t *input, size_t size, uint8_t *output)
{
    // light=1, variant=2, prehashed=0, page_size=scratchpad=262144, iterations=131072 -- exactly
    // Crypto::cn_turtle_lite_slow_hash_v2() in the coin's own src/crypto/hash.h.
    xkr_cn_slow_hash(input, size, reinterpret_cast<char *>(output), 1, 2, 0, 262144, 262144, 131072);
}
