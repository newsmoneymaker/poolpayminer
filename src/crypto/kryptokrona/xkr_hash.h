#ifndef XMRIG_XKR_HASH_H
#define XMRIG_XKR_HASH_H


#include <cstddef>
#include <cstdint>


// Kryptokrona (XKR): real cn_turtle_lite_slow_hash_v2 as used by the coin's own node (src/crypto/hash.h --
// light=1, variant=2, page_size=scratchpad=262144 (256 KB), iterations=131072), vendored verbatim from
// src/crypto/slow-hash.c (see kryptokrona-project.md). NOT the same algorithm as this project's own
// Algorithm::CN_PICO_0 ("cn-pico"/"cryptonight-turtle") despite matching memory/iteration/mask parameters --
// cross-checked on the real node's own compiled code against this project's CN_PICO_0 self-test vector and
// confirmed they produce DIFFERENT hashes for the same input, so a real standalone vendor port was required
// instead of reusing CN_PICO_0 (see critical-wrong-randomx-variant-bug).
void xkrHash(const uint8_t *input, size_t size, uint8_t *output);


#endif /* XMRIG_XKR_HASH_H */
