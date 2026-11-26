/* poolpayminer
 *
 * FortuneBlock (FTB)'s own proof-of-work hash, "HashFortune" -- NOT GhostRider, despite sharing the same
 * kind of core-hash/CN-sub-hash construction and reusing the same sph_* primitives and CnHash
 * infrastructure as src/crypto/ghostrider. Confirmed by reading FortuneBlock's own node source
 * (src/hash.h, src/hash_selection.h/.cpp in the coin's own repo) directly:
 *   - 11 core hash algorithms (Blake/Bmw/Groestl/Jh/Keccak/Skein/Luffa/Cubehash/Shavite/Simd/Echo --
 *     deliberately missing Raptoreum/GhostRider's extra 4: Hamsi, Fugue, Shabal, Whirlpool)
 *   - 3 CN (CryptoNote) sub-hashes chosen from the same 6-variant pool as GhostRider
 *     (dark/dark-lite/fast/lite/turtle/turtle-lite -- confirmed byte-identical cn_slow_hash() parameters
 *     via diffing FortuneBlock's cryptonote/slow-hash.c/.h against Raptoreum's real upstream source)
 *   - rounds grouped as 5 core-hashes, 1 CN, 5 core-hashes, 1 CN, 1 core-hash, 1 CN (14 rounds total,
 *     NOT GhostRider's uniform 5/5/5 core-hashes with 18 rounds total)
 *   - final output is the LAST CN hash's result truncated to 256 bits (round index 13, not 17)
 * See the critical-wrong-randomx-variant-bug project memory: a "looks like the same family" algorithm is
 * not assumed bit-identical here, every piece above was individually verified against FortuneBlock's own
 * source before being reused.
 */

#ifndef XMRIG_FORTUNEHASH_H
#define XMRIG_FORTUNEHASH_H


#include <cstddef>
#include <cstdint>


struct cryptonight_ctx;


namespace xmrig
{


namespace fortunehash
{


// Deliberately no HelperThread/benchmark()/create_helper_thread() here (unlike ghostrider::) -- this is a
// correctness-first port with the simple, non-HWLOC-tuned octa hashing path only. The 2x-throughput
// helper-thread optimization ghostrider.cpp has can be added later once this is proven correct against a
// real synced FortuneBlock node's blocks.
void hash_octa(const uint8_t* data, size_t size, uint8_t* output, cryptonight_ctx** ctx, bool verbose = true);


} // namespace fortunehash


} // namespace xmrig

#endif // XMRIG_FORTUNEHASH_H
