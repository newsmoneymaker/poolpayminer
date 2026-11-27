/* poolpayminer
 *
 * Cascoin (CAS)'s CPU proof-of-work, "MinotaurX" -- confirmed by reading Cascoin's own node source
 * directly (github.com/Casraw/Cascoin, forked from litecoincash-project/litecoincash):
 * src/crypto/minotaurx/minotaur.h (the coin's own name for this exact file) and src/primitives/block.cpp's
 * CBlockHeader::GetPoWHash(), which calls Minotaur(BEGIN(nVersion), END(nNonce), true) for every
 * POW_TYPE_MINOTAURX block -- the standard 80-byte Bitcoin-style header, "X" (hardened) variant always.
 *
 * Algorithm (see minotaurx.cpp for the exact wiring, copied verbatim from the coin's own source):
 *   - Initial SHA512 of the 80-byte header -> 64-byte running state.
 *   - A FIXED 22-node binary tree ("Torture Garden"), hand-wired, not derived from any runtime data.
 *     Every root-to-leaf path is exactly 7 nodes.
 *   - Each node's hash-function choice = (initial SHA512 hash).byte[node index] % 16, chosen ONCE up
 *     front from the initial hash's own bytes (not re-derived mid-traversal).
 *   - 16 candidate primitives (each a full SPH hash over the running 64-byte state, 512-bit output):
 *     Blake, Bmw, Cubehash, Echo, Fugue, Groestl, Hamsi, Sha512, Jh, Keccak, Luffa, Shabal, Shavite,
 *     Simd, Skein, Whirlpool -- in that exact index order (0-15).
 *   - Traversal: at each node, hash the running state with that node's chosen primitive, then branch
 *     left/right by whether the LAST byte of the result is even/odd; recurse to a leaf.
 *   - The "X": node 21 (the gate every path reaches) is NOT modulo-selected like the other 21 nodes --
 *     it is FORCED to a 17th option, yespower (YESPOWER_1_0, N=2048, r=8, pers="et in arcadia ego",
 *     17 bytes), the ASIC/GPU-hardening gate. This is the only node that needs yespower.
 *   - Final output: the leaf's 512-bit state, truncated to 256 bits.
 *
 * This is NOT GhostRider/fortunehash (no CryptoNight sub-hashes at all, no octa/8-lane batching needed --
 * single-lane like x16rs/yespower), but it REUSES 15 of the 16 SPH primitives above as already-vendored
 * standalone .c/.h files in src/crypto/ghostrider/ (confirmed via direct inspection: blake/bmw/cubehash/
 * echo/groestl/jh/keccak/luffa/shavite/simd/skein were already used by fortunehash; fugue/hamsi/sha2/
 * shabal/whirlpool are ghostrider's own "extra" primitives, present there too) -- NOT re-vendored here,
 * see minotaurx.cpp's own comment and critical-vendored-lib-symbol-collision. The yespower core is
 * likewise reused from src/crypto/yespower/ (already used by yespowerR16/yespowerTIDE/yespowerADVC in
 * CpuWorker.cpp), just with this algorithm's own params.
 */

#ifndef XMRIG_MINOTAURX_H
#define XMRIG_MINOTAURX_H


#include <cstddef>
#include <cstdint>


namespace xmrig
{


namespace minotaurx
{


// Single-lane (N == 1), over the 80-byte Cascoin block header. Always the "X" (yespower-hardened) gate --
// Cascoin's own consensus never calls plain (non-X) Minotaur for real blocks, see GetPoWHash() above.
void hash(const uint8_t* data, size_t size, uint8_t* output);


} // namespace minotaurx


} // namespace xmrig


#endif // XMRIG_MINOTAURX_H
