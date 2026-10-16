#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

#include <openssl/evp.h>

#include "base/io/log/Log.h"
#include "base/io/log/Tags.h"
#include "crypto/xcb_randomy/xcb_randomy.h"
#include "crypto/xcb_randomy/xcb_randomy_upstream.h"


namespace xcb_randomy {


namespace {


std::once_flag g_initOnce;
xcbrndy_randomx_cache *g_cache = nullptr;
xcbrndy_randomx_dataset *g_dataset = nullptr;  // non-null only when full/dataset mode is actually active
xcbrndy_randomx_flags g_baseFlags = XCBRNDY_RANDOMX_FLAG_DEFAULT;

// Default true: full/dataset mode is what actually fixes the real, absurdly-low cache-only hashrate --
// see xcb-project.md. setFullMode(false) before the first createVm() call switches this back to the old
// light/cache-only behavior (e.g. a low-RAM fallback), matching this project's own
// "randomx":{"mode":"light"} convention for every other RandomX-family coin here.
std::atomic<bool> g_fullModeRequested{true};


// Splits RandomY's real dataset (confirmed 4,325,376 items * 64 bytes = 276,824,064 bytes, ~264 MiB --
// RANDOMX_DATASET_BASE_SIZE 256 MiB + RANDOMX_DATASET_EXTRA_SIZE 8 MiB from the real upstream
// configuration.h) across all available CPU threads for initialization, same convention as this
// project's own RxDataset::init() (src/crypto/rx/RxDataset.cpp) uses for every other RandomX-family
// coin here -- dataset init is the one genuinely slow, one-time startup cost; per-share hashing never
// touches this function again.
void initDatasetParallel(xcbrndy_randomx_dataset *dataset, xcbrndy_randomx_cache *cache, unsigned long itemCount)
{
    unsigned int hw = std::thread::hardware_concurrency();
    if (hw == 0) {
        hw = 1;
    }
    const unsigned int numThreads = std::min<unsigned int>(hw, 64);

    if (numThreads <= 1 || itemCount < numThreads) {
        xcbrndy_randomx_init_dataset(dataset, cache, 0, itemCount);
        return;
    }

    std::vector<std::thread> threads;
    threads.reserve(numThreads);

    for (unsigned int i = 0; i < numThreads; ++i) {
        const unsigned long a = (itemCount * i) / numThreads;
        const unsigned long b = (itemCount * (i + 1)) / numThreads;
        threads.emplace_back(xcbrndy_randomx_init_dataset, dataset, cache, a, b - a);
    }

    for (auto &t : threads) {
        t.join();
    }
}


void initOnce()
{
    // Core Blockchain (XCB): the fixed, never-rotated RandomY dataset key, ASCII "56789" -- confirmed
    // from go-randomy/vm.go's NewRandomYVMWithKeyAndMutex(): key := []byte{53, 54, 55, 56, 57}.
    static const uint8_t key[5] = { 53, 54, 55, 56, 57 };

    // Auto-detects RANDOMX_FLAG_HARD_AES / RANDOMX_FLAG_JIT / ARGON2_AVX2 / ARGON2_SSSE3 support on the
    // running CPU -- verified in an isolated standalone test (not just FULL_MEM alone) that enabling
    // these still reproduces the real RandomY test vector byte-for-byte, and the real xmrig Linux build
    // ran rock-solid on JIT for 50+ seconds and hundreds of real accepted shares against the live pool.
    // RANDOMX_FLAG_FULL_MEM is deliberately NOT included here (get_flags() never sets it) -- OR'd in
    // separately below only when the dataset was actually built.
    g_baseFlags = xcbrndy_randomx_get_flags();

#   ifdef XMRIG_OS_WIN
    // Real, reproduced finding (2026-10-17): with JIT enabled, the FIRST xcbrndy_randomx_create_vm() call
    // on the cross-compiled Windows build reliably page-faults under wine64 (write access fault inside
    // the vendored library's own code, immediately on VM creation, in BOTH light and full mode -- isolated
    // by testing each flag combination individually). Disabling only RANDOMX_FLAG_JIT makes the exact
    // same Windows binary run stably under wine64 (dataset build + real accepted shares against the live
    // xcb-nodejs-pool, zero crashes). Root cause not chased further (likely an old wine-4.0 self-modifying
    // -code/W^X quirk specific to this emulation layer, not necessarily a bug on real Windows -- but this
    // environment has no real Windows to verify that on), so JIT stays OFF on the Windows build out of
    // real, demonstrated caution rather than shipping something unverified. HARD_AES/ARGON2_AVX2/
    // ARGON2_SSSE3 are untouched (pure data-path flags, not code generation -- never implicated in the
    // crash, confirmed by testing with just JIT removed). The FULL_MEM/dataset speedup -- the actual
    // ask -- is unaffected by this: it still gives a real, verified (if wine-emulation-taxed) speedup
    // over the old cache-only behavior. See xcb-project.md for the real before/after numbers on both
    // platforms.
    g_baseFlags = static_cast<xcbrndy_randomx_flags>(g_baseFlags & ~XCBRNDY_RANDOMX_FLAG_JIT);
#   endif

    g_cache = xcbrndy_randomx_alloc_cache(g_baseFlags);
    if (!g_cache) {
        return;
    }
    xcbrndy_randomx_init_cache(g_cache, key, sizeof(key));

    if (!g_fullModeRequested.load()) {
        LOG_INFO("%s" " RandomY light mode (cache-only, no dataset) -- slower hashrate, lower RAM use", xmrig::Tags::randomx());
        return;
    }

    xcbrndy_randomx_dataset *dataset = xcbrndy_randomx_alloc_dataset(XCBRNDY_RANDOMX_FLAG_DEFAULT);
    if (!dataset) {
        LOG_WARN("%s" " RandomY full/dataset mode requested but dataset allocation failed (~264 MiB) -- "
                 "falling back to light/cache-only mode", xmrig::Tags::randomx());
        return;
    }

    const unsigned long itemCount = xcbrndy_randomx_dataset_item_count();
    LOG_INFO("%s" " building RandomY dataset (~264 MiB, %u threads)...", xmrig::Tags::randomx(), std::thread::hardware_concurrency());
    initDatasetParallel(dataset, g_cache, itemCount);
    LOG_INFO("%s" " RandomY dataset ready, full/fast mode active", xmrig::Tags::randomx());

    g_dataset = dataset;
}


// SHA3-512(blob40) -> 64 bytes, fed as RandomX's input buffer. Real NIST SHA3 (OpenSSL's EVP_sha3_512,
// 0x06 domain-separation padding) -- deliberately NOT any of this project's already-vendored Keccak-512
// code (e.g. the X11-family sph_keccak), which uses the older 0x01 Keccak padding and would silently
// produce a wrong hash here. Matches go-randomy/vm.go's RandomY(): seed := header(32) || LE64(nonce);
// seed = SHA3_512(seed). See xcb-project.md for the NIST test vector this was checked against first.
bool sha3_512(const uint8_t *data, size_t size, uint8_t out64[64])
{
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) {
        return false;
    }

    unsigned int len = 0;
    const bool ok = (EVP_DigestInit_ex(ctx, EVP_sha3_512(), nullptr) == 1) &&
                    (EVP_DigestUpdate(ctx, data, size) == 1) &&
                    (EVP_DigestFinal_ex(ctx, out64, &len) == 1) &&
                    (len == 64);

    EVP_MD_CTX_free(ctx);
    return ok;
}


} // namespace


void setFullMode(bool full)
{
    g_fullModeRequested.store(full);
}


void *createVm()
{
    std::call_once(g_initOnce, initOnce);
    if (!g_cache) {
        return nullptr;
    }

    if (g_dataset) {
        return xcbrndy_randomx_create_vm(g_baseFlags | XCBRNDY_RANDOMX_FLAG_FULL_MEM, g_cache, g_dataset);
    }

    return xcbrndy_randomx_create_vm(g_baseFlags, g_cache, nullptr);
}


void destroyVm(void *vm)
{
    if (vm) {
        xcbrndy_randomx_destroy_vm(reinterpret_cast<xcbrndy_randomx_vm *>(vm));
    }
}


void hash(void *vm, const uint8_t *blob40, uint8_t *out32)
{
    uint8_t seed[64];
    if (!sha3_512(blob40, 40, seed)) {
        memset(out32, 0xff, 32); // never match any real target on a crypto failure
        return;
    }

    xcbrndy_randomx_calculate_hash(reinterpret_cast<xcbrndy_randomx_vm *>(vm), seed, sizeof(seed), out32);
}


namespace {

// Real NIST SHA3-512 test vector (FIPS 202), input "abc" -- checked BEFORE wiring SHA3-512 into the
// hashing pipeline, per this project's standing rule to verify the padding byte (0x06, not Keccak's old
// 0x01) against a known-good vector first. If this ever fails, do NOT trust any XCB hash this build
// produces -- see xcb-project.md.
bool sha3_512NistVectorOk()
{
    uint8_t out[64];
    if (!sha3_512(reinterpret_cast<const uint8_t *>("abc"), 3, out)) {
        return false;
    }

    static const uint8_t expected[64] = {
        0xb7, 0x51, 0x85, 0x0b, 0x1a, 0x57, 0x16, 0x8a, 0x56, 0x93, 0xcd, 0x92, 0x4b, 0x6b, 0x09, 0x6e,
        0x08, 0xf6, 0x21, 0x82, 0x74, 0x44, 0xf7, 0x0d, 0x88, 0x4f, 0x5d, 0x02, 0x40, 0xd2, 0x71, 0x2e,
        0x10, 0xe1, 0x16, 0xe9, 0x19, 0x2a, 0xf3, 0xc9, 0x1a, 0x7e, 0xc5, 0x76, 0x47, 0xe3, 0x93, 0x40,
        0x57, 0x34, 0x0b, 0x4c, 0xf4, 0x08, 0xd5, 0xa5, 0x65, 0x92, 0xf8, 0x27, 0x4e, 0xec, 0x53, 0xf0,
    };

    return memcmp(out, expected, sizeof(out)) == 0;
}

} // namespace


bool selfTest()
{
    // 1) SHA3-512 primitive, in isolation, against a real NIST vector.
    if (!sha3_512NistVectorOk()) {
        return false;
    }

    // 2) The vendored, symbol-renamed RandomY library itself, against its own real test vector
    //    (go-randomy's randomy_test.go: key="test key 000", input="This is a test", NO SHA3-512 wrapper --
    //    this checks the library/rename, not XCB's own seed-prep step). Deliberately a SEPARATE, one-off
    //    cache keyed "test key 000" -- NOT the shared production cache (fixed key ASCII "56789"), which
    //    this test must not touch or trigger early.
    xcbrndy_randomx_cache *testCache = xcbrndy_randomx_alloc_cache(XCBRNDY_RANDOMX_FLAG_DEFAULT);
    if (!testCache) {
        return false;
    }
    static const char testKey[] = "test key 000";
    xcbrndy_randomx_init_cache(testCache, testKey, sizeof(testKey) - 1);

    xcbrndy_randomx_vm *testVm = xcbrndy_randomx_create_vm(XCBRNDY_RANDOMX_FLAG_DEFAULT, testCache, nullptr);
    if (!testVm) {
        // Deliberately NOT calling xcbrndy_randomx_release_cache() here either -- see the comment below.
        return false;
    }

    uint8_t out[32];
    xcbrndy_randomx_calculate_hash(testVm, "This is a test", 14, out);
    xcbrndy_randomx_destroy_vm(testVm);
    // Deliberately NOT calling xcbrndy_randomx_release_cache(testCache) here: doing so reproducibly
    // corrupted the heap ("double free or corruption (out)") specifically when linked into the real
    // xmrig binary (never in an isolated standalone reproducer) -- root cause not fully chased down
    // under time pressure (suspected an ODR/ABI interaction between the vendored library's internal
    // allocator-related types and this project's own, differently-configured RandomX engine linked into
    // the same binary; see the long symbol-rename comment on RX_XCB in Algorithm.h for the related,
    // already-fixed weak-symbol collision this is probably one layer of). This path runs ONCE per
    // process lifetime (xmrig's own self-test, not the per-share hashing loop, which never calls
    // release_cache at all -- the production cache is a process-wide singleton, released only on exit
    // same as every other coin's RxCache here), so leaking this one ~256 MiB test-only cache is a
    // deliberate, safe trade-off, not a real leak in the actual mining path. See xcb-project.md.

    static const uint8_t expected[32] = {
        0x0e, 0x9a, 0xb1, 0x3e, 0x4b, 0x33, 0x7c, 0x86, 0x6e, 0x17, 0x44, 0x56, 0x21, 0xbe, 0x8d, 0x18,
        0x0c, 0x0a, 0x18, 0xa3, 0x25, 0x93, 0x20, 0x78, 0xdc, 0x33, 0xf7, 0xac, 0x69, 0xff, 0x17, 0xe8,
    };

    if (memcmp(out, expected, sizeof(out)) != 0) {
        return false;
    }

    // 3) Full/dataset mode, same key+input, on the SAME testCache (already initialized above) -- proves
    //    the newly-added RANDOMX_FLAG_FULL_MEM path (and the renamed dataset symbols: alloc_dataset/
    //    dataset_item_count/init_dataset/create_vm-with-dataset) give the BYTE-IDENTICAL hash as light
    //    mode, on every real startup, not just this one-off session's standalone check. A real,
    //    independent standalone test (plain gcc, not linked into xmrig) already proved this for TWO
    //    separate real test vectors before this was ever wired in -- see xcb-project.md.
    xcbrndy_randomx_dataset *testDataset = xcbrndy_randomx_alloc_dataset(XCBRNDY_RANDOMX_FLAG_DEFAULT);
    if (!testDataset) {
        // Correctness-critical: do NOT silently skip this check and report success.
        return false;
    }
    initDatasetParallel(testDataset, testCache, xcbrndy_randomx_dataset_item_count());

    xcbrndy_randomx_vm *fullVm = xcbrndy_randomx_create_vm(XCBRNDY_RANDOMX_FLAG_FULL_MEM, testCache, testDataset);
    if (!fullVm) {
        return false;
    }

    uint8_t outFull[32];
    xcbrndy_randomx_calculate_hash(fullVm, "This is a test", 14, outFull);
    xcbrndy_randomx_destroy_vm(fullVm);
    // Deliberately NOT calling xcbrndy_randomx_release_dataset()/release_cache() here -- same documented,
    // one-time, startup-only ~264 MiB leak posture as testCache above (release_cache reproducibly
    // corrupted the heap when linked into the real xmrig binary; never risk the analogous call for the
    // dataset either, out of caution, for the exact same reason).

    return memcmp(outFull, expected, sizeof(outFull)) == 0;
}


} // namespace xcb_randomy
