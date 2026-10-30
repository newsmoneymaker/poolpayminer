#ifndef XMRIG_CCX_GPU_HASH_H
#define XMRIG_CCX_GPU_HASH_H


#include <cstddef>
#include <cstdint>


// Conceal (CCX)'s real CryptoNight-GPU hash (Ryo Currency Project's cn_pow_hash_v3 =
// cn_slow_hash<2 MB, 0xC000 iterations, version 2>, vendored verbatim under src/crypto/ccx_gpu/, see
// ccxgpu_rename.h): the real algorithm since the network's block_major_version 8 hard fork (LWMA1, CN-GPU,
// Halving) -- NOT Algorithm::CN_CCX (CRYPTONIGHT_CONCEAL), which only matches the OLD, pre-hard-fork chain.
// 32 byte output, no blake/groestl/jh/skein family-select finalizer (unlike CN_CCX): just keccak-200 ->
// explode -> the real FP div/sqrt-heavy GPU-favoring main loop -> implode -> keccakf -> raw first 32 bytes.
void ccxGpuHash(const uint8_t *input, size_t size, uint8_t *output);

bool ccxGpuSelfTest();


#endif /* XMRIG_CCX_GPU_HASH_H */
