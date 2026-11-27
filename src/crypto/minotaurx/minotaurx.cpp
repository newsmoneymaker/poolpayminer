/* poolpayminer
 * See minotaurx.h for what this is. Tree wiring and algo-selection byte offsets below are copied
 * VERBATIM from Cascoin's own src/crypto/minotaurx/minotaur.h (LinkNodes() calls and the
 * `garden.nodes[i].algo = hash.ByteAt(i) % MINOTAUR_ALGO_COUNT` loop) -- not re-derived, to avoid any
 * transcription drift from the real consensus rule.
 */

#include "minotaurx.h"

// Reuse the already-vendored sphlib primitives from src/crypto/ghostrider -- NOT re-vendored here, see
// minotaurx.h's header comment and critical-vendored-lib-symbol-collision memory. This module's
// CMakeLists.txt links against the `ghostrider` static library for these symbols instead of recompiling
// the same .c files a second time under a second library.
#include "../ghostrider/sph_blake.h"
#include "../ghostrider/sph_bmw.h"
#include "../ghostrider/sph_cubehash.h"
#include "../ghostrider/sph_echo.h"
#include "../ghostrider/sph_fugue.h"
#include "../ghostrider/sph_groestl.h"
#include "../ghostrider/sph_hamsi.h"
#include "../ghostrider/sph_jh.h"
#include "../ghostrider/sph_keccak.h"
#include "../ghostrider/sph_luffa.h"
#include "../ghostrider/sph_shabal.h"
#include "../ghostrider/sph_shavite.h"
#include "../ghostrider/sph_simd.h"
#include "../ghostrider/sph_skein.h"
#include "../ghostrider/sph_whirlpool.h"

extern "C" {
#include "../yespower/yespower.h"
}

// sph_sha2.c in src/crypto/ghostrider/ only implements SHA-224/256 (a modernized rewrite, no
// SHA-384/512 at all -- confirmed empty via `nm`) -- GhostRider itself never needed the 512-bit
// variant, so nobody ever vendored it there. Rather than vendor yet another sphlib copy of SHA-512
// (several already exist scattered across this tree, see x16rs/sha3/sha2big.c) for one single
// standard, unmodified hash with no coin-specific tweak at all, use OpenSSL's own SHA512() directly
// (already linked into this binary for TLS, see cmake/OpenSSL.cmake) -- correctness-equivalent and
// avoids reintroducing the vendored-symbol-collision class of bug for a primitive this generic.
#include <openssl/sha.h>

#include <cstring>
#include <cstdlib>


namespace xmrig
{
namespace minotaurx
{


// 16 candidate hash functions, each run over the running 64-byte (512-bit) state. Order MUST match
// Cascoin's own GetHash() switch exactly (minotaur.h): 0=Blake,1=Bmw,2=Cubehash,3=Echo,4=Fugue,
// 5=Groestl,6=Hamsi,7=Sha512,8=Jh,9=Keccak,10=Luffa,11=Shabal,12=Shavite,13=Simd,14=Skein,15=Whirlpool.
#define CORE_HASH(i, x) static void h##i(const uint8_t* data, uint8_t* output) \
{ \
    sph_##x##_context ctx; \
    sph_##x##_init(&ctx); \
    sph_##x(&ctx, data, 64); \
    sph_##x##_close(&ctx, output); \
}

CORE_HASH( 0, blake512);
CORE_HASH( 1, bmw512);
CORE_HASH( 2, cubehash512);
CORE_HASH( 3, echo512);
CORE_HASH( 4, fugue512);
CORE_HASH( 5, groestl512);
CORE_HASH( 6, hamsi512);
CORE_HASH( 8, jh512);
CORE_HASH( 9, keccak512);
CORE_HASH(10, luffa512);
CORE_HASH(11, shabal512);
CORE_HASH(12, shavite512);
CORE_HASH(13, simd512);
CORE_HASH(14, skein512);

#undef CORE_HASH

// Whirlpool has no "512" suffix in sphlib (only one output size exists) -- matches Cascoin's own usage.
static void h15(const uint8_t* data, uint8_t* output)
{
    sph_whirlpool_context ctx;
    sph_whirlpool_init(&ctx);
    sph_whirlpool(&ctx, data, 64);
    sph_whirlpool_close(&ctx, output);
}

// Index 7: SHA-512, via OpenSSL (see the include comment above for why).
static void h7(const uint8_t* data, uint8_t* output)
{
    SHA512(data, 64, output);
}

using core_hash_func = void (*)(const uint8_t* data, uint8_t* output);
static const core_hash_func core_hash[16] = { h0, h1, h2, h3, h4, h5, h6, h7, h8, h9, h10, h11, h12, h13, h14, h15 };

// MinotaurX's own yespower gate ("the X"): YESPOWER_1_0, N=2048, r=8, pers="et in arcadia ego" (17 bytes).
static void yespowerGate(const uint8_t* data, uint8_t* output)
{
    static const uint8_t pers[] = "et in arcadia ego";
    static const yespower_params_t params = { YESPOWER_1_0, 2048, 8, pers, 17 };

    if (yespower_tls(data, 64, &params, reinterpret_cast<yespower_binary_t*>(output)) != 0) {
        memset(output, 0xFF, 32);
    }
}

struct TortureNode
{
    unsigned int algo;
    TortureNode* childLeft;
    TortureNode* childRight;
};

static inline void linkNodes(TortureNode* parent, TortureNode* childLeft, TortureNode* childRight)
{
    parent->childLeft  = childLeft;
    parent->childRight = childRight;
}

// Traverse the garden starting at a given node, overwriting `state` (64 bytes) with the running hash at
// each step. Branch left/right by whether the LAST byte of the just-computed hash is even/odd. The
// "hardened" gate (node 21 forced to yespower, see minotaurxHash below) produces only 32 bytes --
// callers must zero-pad state[32..64) before that call since yespower's output is 256 bits, not 512.
static void traverseGarden(TortureNode* node, uint8_t* state)
{
    uint8_t partial[64] = {0};

    if (node->algo == 16) {
        // yespower gate: 256-bit output, zero-padded to the 64-byte running state (no further children).
        yespowerGate(state, partial);
    }
    else {
        core_hash[node->algo](state, partial);
    }

    memcpy(state, partial, 64);

    TortureNode* next = (state[63] % 2 == 0) ? node->childLeft : node->childRight;
    if (next != nullptr) {
        traverseGarden(next, state);
    }
}

void hash(const uint8_t* data, size_t size, uint8_t* output)
{
    // Build the 22-node tree exactly as Cascoin's own Minotaur<T>() does. Both sides of nodes 19 and 20
    // lead to 21, and 21 has no children -- every path through the garden is exactly 7 nodes.
    TortureNode nodes[22];
    linkNodes(&nodes[0],  &nodes[1],  &nodes[2]);
    linkNodes(&nodes[1],  &nodes[3],  &nodes[4]);
    linkNodes(&nodes[2],  &nodes[5],  &nodes[6]);
    linkNodes(&nodes[3],  &nodes[7],  &nodes[8]);
    linkNodes(&nodes[4],  &nodes[9],  &nodes[10]);
    linkNodes(&nodes[5],  &nodes[11], &nodes[12]);
    linkNodes(&nodes[6],  &nodes[13], &nodes[14]);
    linkNodes(&nodes[7],  &nodes[15], &nodes[16]);
    linkNodes(&nodes[8],  &nodes[15], &nodes[16]);
    linkNodes(&nodes[9],  &nodes[15], &nodes[16]);
    linkNodes(&nodes[10], &nodes[15], &nodes[16]);
    linkNodes(&nodes[11], &nodes[17], &nodes[18]);
    linkNodes(&nodes[12], &nodes[17], &nodes[18]);
    linkNodes(&nodes[13], &nodes[17], &nodes[18]);
    linkNodes(&nodes[14], &nodes[17], &nodes[18]);
    linkNodes(&nodes[15], &nodes[19], &nodes[20]);
    linkNodes(&nodes[16], &nodes[19], &nodes[20]);
    linkNodes(&nodes[17], &nodes[19], &nodes[20]);
    linkNodes(&nodes[18], &nodes[19], &nodes[20]);
    linkNodes(&nodes[19], &nodes[21], &nodes[21]);
    linkNodes(&nodes[20], &nodes[21], &nodes[21]);
    nodes[21].childLeft  = nullptr;
    nodes[21].childRight = nullptr;

    // Initial SHA512 of the variable-length input -> 64-byte running state.
    uint8_t state[64] = {0};
    SHA512(data, size, state);

    // Assign each node's algo from the INITIAL hash's own bytes (fixed once per input, not re-derived
    // mid-traversal). Node 21 (the gate every path reaches) is forced to 16 (yespower) -- this is
    // ALWAYS the "X" (hardened) variant: Cascoin's own consensus (GetPoWHash) never mines plain Minotaur.
    for (int i = 0; i < 22; ++i) {
        nodes[i].algo = static_cast<unsigned int>(state[i]) % 16;
    }
    nodes[21].algo = 16;

    traverseGarden(&nodes[0], state);

    memcpy(output, state, 32);
}


} // namespace minotaurx
} // namespace xmrig
