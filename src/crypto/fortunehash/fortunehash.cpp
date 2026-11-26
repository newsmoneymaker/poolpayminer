/* poolpayminer
 * See fortunehash.h for what this is and why it is its own module, not an alias of ghostrider.
 */

#include "fortunehash.h"

// Reuse the already-vendored sphlib reference primitives from src/crypto/ghostrider -- confirmed
// byte-identical (modulo whitespace/formatting and harmless AVX2/round-count #ifdef guards) against
// FortuneBlock's own src/crypto/{blake,bmw,groestl,jh,keccak,skein,luffa,cubehash,shavite,simd,echo}.c
// by direct diff before trusting this reuse. NOT re-vendored here: these .c files are compiled exactly
// once, inside the ghostrider static library (see src/crypto/ghostrider/CMakeLists.txt) -- this module's
// CMakeLists.txt links against that library for these symbols instead of recompiling them again under a
// second static library, specifically to avoid the duplicate-symbol-across-vendored-libs class of bug
// (see critical-vendored-lib-symbol-collision memory).
#include "../ghostrider/sph_blake.h"
#include "../ghostrider/sph_bmw.h"
#include "../ghostrider/sph_groestl.h"
#include "../ghostrider/sph_jh.h"
#include "../ghostrider/sph_keccak.h"
#include "../ghostrider/sph_skein.h"
#include "../ghostrider/sph_luffa.h"
#include "../ghostrider/sph_cubehash.h"
#include "../ghostrider/sph_shavite.h"
#include "../ghostrider/sph_simd.h"
#include "../ghostrider/sph_echo.h"

#include "base/crypto/Algorithm.h"
#include "base/io/log/Log.h"
#include "backend/cpu/Cpu.h"
#include "crypto/cn/CnHash.h"
#include "crypto/cn/CnCtx.h"
#include "crypto/cn/CryptoNight.h"

#include <cstring>


#define CORE_HASH(i, x) static void h##i(const uint8_t* data, size_t size, uint8_t* output) \
{ \
    sph_##x##_context ctx; \
    sph_##x##_init(&ctx); \
    sph_##x(&ctx, data, size); \
    sph_##x##_close(&ctx, output); \
}

CORE_HASH( 0, blake512   );
CORE_HASH( 1, bmw512     );
CORE_HASH( 2, groestl512 );
CORE_HASH( 3, jh512      );
CORE_HASH( 4, keccak512  );
CORE_HASH( 5, skein512   );
CORE_HASH( 6, luffa512   );
CORE_HASH( 7, cubehash512);
CORE_HASH( 8, shavite512 );
CORE_HASH( 9, simd512    );
CORE_HASH(10, echo512    );

#undef CORE_HASH

using core_hash_func = void (*)(const uint8_t* data, size_t size, uint8_t* output);
// Order MUST match FortuneBlock's own HashSelection::algoMap exactly (src/hash_selection.h in the coin's
// own repo): 0=Blake,1=Bmw,2=Groestl,3=Jh,4=Keccak,5=Skein,6=Luffa,7=Cubehash,8=Shavite,9=Simd,10=Echo.
static const core_hash_func core_hash[11] = { h0, h1, h2, h3, h4, h5, h6, h7, h8, h9, h10 };


namespace xmrig
{


// Same 6 CN variants as src/crypto/ghostrider/ghostrider.cpp, reused as-is -- confirmed byte-identical cn
// hash implementation (FortuneBlock's cryptonote/slow-hash.c/.h differ from Raptoreum's real upstream only
// in a copyright-header comment, everything else including CN_*_PAGE_SIZE/ITERATIONS/AES_ROUNDS matches
// exactly), so the same Algorithm::CN_GR_0..5 ids/sizes/names apply unchanged here.
static constexpr Algorithm::Id cn_hash[6] = {
    Algorithm::CN_GR_0,
    Algorithm::CN_GR_1,
    Algorithm::CN_GR_2,
    Algorithm::CN_GR_3,
    Algorithm::CN_GR_4,
    Algorithm::CN_GR_5,
};

static constexpr const char* cn_names[6] = {
    "cn/dark (512 KB)",
    "cn/dark-lite (256 KB)",
    "cn/fast (2 MB)",
    "cn/lite (1 MB)",
    "cn/turtle (256 KB)",
    "cn/turtle-lite (128 KB)",
};

static constexpr size_t cn_sizes[6] = {
    Algorithm::l3(Algorithm::CN_GR_0),
    Algorithm::l3(Algorithm::CN_GR_1) / 2,
    Algorithm::l3(Algorithm::CN_GR_2),
    Algorithm::l3(Algorithm::CN_GR_3),
    Algorithm::l3(Algorithm::CN_GR_4),
    Algorithm::l3(Algorithm::CN_GR_5) / 2,
};

static constexpr CnHash::AlgoVariant av_hw_aes[5]   = { CnHash::AV_SINGLE, CnHash::AV_SINGLE, CnHash::AV_DOUBLE, CnHash::AV_TRIPLE, CnHash::AV_QUAD };
static constexpr CnHash::AlgoVariant av_soft_aes[5] = { CnHash::AV_SINGLE_SOFT, CnHash::AV_SINGLE_SOFT, CnHash::AV_DOUBLE_SOFT, CnHash::AV_TRIPLE_SOFT, CnHash::AV_QUAD_SOFT };


namespace fortunehash
{


// Verified (by hand-tracing against FortuneBlock's own HashSelection::getRandomIndexes(), src/hash_selection.cpp
// in the coin's own repo) to produce EXACTLY the same nibble-selection permutation as that function, for the
// same seed bytes. FortuneBlock's version walks uint256::GetNibble(i) for i=63 downto 0; GetNibble() internally
// computes idx2=63-i and extracts the nibble at byte idx2/2 (high nibble if idx2 odd, low if even) -- the 63-i
// substitution means idx2 itself runs 0..63 ASCENDING as the outer i descends, with the SAME even/odd-nibble
// convention as the extraction below (index even -> low nibble, odd -> high nibble, same byte array position
// i/2). The two are the same sequence once that double-reversal is accounted for -- this was not assumed, it
// was traced through by hand before reuse, per the critical-wrong-randomx-variant-bug lesson.
template<size_t N>
static inline void select_indices(uint32_t (&indices)[N], const uint8_t* seed)
{
    bool selected[N] = {};

    uint32_t k = 0;
    for (uint32_t i = 0; i < 64; ++i) {
        const uint8_t index = ((seed[i / 2] >> ((i & 1) * 4)) & 0xF) % N;
        if (!selected[index]) {
            selected[index] = true;
            indices[k++] = index;
            if (k >= N) {
                return;
            }
        }
    }

    for (uint32_t i = 0; i < N; ++i) {
        if (!selected[i]) {
            indices[k++] = i;
        }
    }
}


void hash_octa(const uint8_t* data, size_t size, uint8_t* output, cryptonight_ctx** ctx, bool verbose)
{
    constexpr uint32_t N = 8;

    uint8_t* ctx_memory[N];
    for (size_t i = 0; i < N; ++i) {
        ctx_memory[i] = ctx[i]->memory;
    }

    // hashPrevBlock (this algorithm's selection seed) is at bytes [4; 36) of the 80-byte header, same
    // standard Bitcoin-Core header layout FortuneBlock's own CBlockHeader uses (nVersion[4] +
    // hashPrevBlock[32] + hashMerkleRoot[32] + nTime[4] + nBits[4] + nNonce[4] = 80 bytes) -- confirmed
    // against FortuneBlock's own src/primitives/block.h, not assumed.
    const uint8_t* seed = data + 4;

    uint32_t core_indices[11];
    select_indices(core_indices, seed);

    uint32_t cn_indices[6];
    select_indices(cn_indices, seed);

    // FortuneBlock's HashFortune groups its 11 core-hashes as 5, 5, 1 (not GhostRider's uniform 5, 5, 5)
    // before each of its 3 CN sub-hashes -- see src/hash.h's HashFortune() loop in the coin's own repo.
    static constexpr uint32_t core_offset[3] = { 0, 5, 10 };
    static constexpr uint32_t core_count[3]  = { 5, 5,  1 };

#if defined(XMRIG_ARM) || defined(XMRIG_RISCV)
    uint32_t step[6] = { 1, 1, 1, 1, 1, 1 };
#else
    uint32_t step[6] = { 4, 4, 1, 2, 4, 4 };
#endif

    if (verbose) {
        static uint32_t prev_indices[3];
        if (memcmp(cn_indices, prev_indices, sizeof(prev_indices)) != 0) {
            memcpy(prev_indices, cn_indices, sizeof(prev_indices));
            for (int i = 0; i < 3; ++i) {
                LOG_INFO("fortunehash algo %d: %s", i + 1, cn_names[cn_indices[i]]);
            }
        }
    }

    const CnHash::AlgoVariant* av = Cpu::info()->hasAES() ? av_hw_aes : av_soft_aes;

    uint8_t tmp[64 * N];

    for (size_t part = 0; part < 3; ++part) {

        // Allocate scratchpads
        {
            uint8_t* p = ctx_memory[0];

            for (size_t i = 0, k = 0; i < N; ++i) {
                if ((i % step[cn_indices[part]]) == 0) {
                    k = 0;
                    p = ctx_memory[0];
                }
                else if (p - ctx_memory[k] >= (1 << 21)) {
                    ++k;
                    p = ctx_memory[k];
                }
                ctx[i]->memory = p;
                p += cn_sizes[cn_indices[part]];
            }
        }

        for (size_t i = 0; i < core_count[part]; ++i) {
            for (size_t j = 0; j < N; ++j) {
                core_hash[core_indices[core_offset[part] + i]](data + j * size, size, tmp + j * 64);
            }
            data = tmp;
            size = 64;
        }

        auto f = CnHash::fn(cn_hash[cn_indices[part]], av[step[cn_indices[part]]], Assembly::AUTO);
        for (size_t j = 0; j < N; j += step[cn_indices[part]]) {
            f(tmp + j * 64, 64, output + j * 32, ctx, 0);
        }

        for (size_t j = 0; j < N; ++j) {
            memcpy(tmp + j * 64, output + j * 32, 32);
            memset(tmp + j * 64 + 32, 0, 32);
        }
    }

    for (size_t i = 0; i < N; ++i) {
        ctx[i]->memory = ctx_memory[i];
    }
}


} // namespace fortunehash


} // namespace xmrig
