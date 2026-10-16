/* Core Blockchain (XCB) "RandomY" hasher -- thin wrapper around the real, vendored (symbol-renamed)
 * go-randomy/librandomx library, now built by this project itself from real upstream source. See
 * xcb_randomy_upstream.h and xcb-project.md for the full story.
 *
 * Usage (one VM per CPU worker thread -- randomx_vm is NOT safe for concurrent use from multiple threads,
 * the shared cache/dataset behind it is read-only after init and IS safe to share):
 *   xcb_randomy::setFullMode(bool); // OPTIONAL. Call BEFORE the first createVm() anywhere in the process
 *                                   // (e.g. once, when a RANDOMY job first arrives) to choose full/dataset
 *                                   // mode (true, the default if never called) vs cache-only/light mode
 *                                   // (false). Has NO effect once the first createVm() call has already
 *                                   // latched the decision via std::call_once -- matches this project's
 *                                   // own "randomx":{"mode":"light"/"fast"} config convention used for
 *                                   // every other RandomX-family coin here.
 *   void *vm = xcb_randomy::createVm();     // first call anywhere lazily builds the one shared, fixed-key
 *                                            // cache (ASCII "56789", never rotated -- see go-randomy's own
 *                                            // vm.go NewRandomYVMWithKeyAndMutex()) AND, in full mode, the
 *                                            // one shared ~264 MiB dataset (built once, multi-threaded,
 *                                            // across all available CPU threads -- the one genuinely slow
 *                                            // startup cost); safe to call from many threads concurrently
 *                                            // (std::call_once-guarded). Falls back to light mode
 *                                            // transparently if the dataset allocation fails (e.g. low
 *                                            // RAM) -- never silently wrong, just slower.
 *   xcb_randomy::hash(vm, blob40, out32);    // blob40 = sealhash(32 bytes) || nonce(8 bytes, native x86
 *                                            // little-endian -- exactly what this project's generic
 *                                            // nonce-splicing machinery already writes into Job::blob()
 *                                            // at nonceOffset()=32/nonceSize()=8 for RX_XCB, see Job.cpp).
 *                                            // Internally: out = RandomX(SHA3-512(blob40)), matching
 *                                            // go-randomy's own RandomY() in vm.go exactly -- identical
 *                                            // result whether vm is a light-mode or full-mode VM, by
 *                                            // RandomX's own design guarantee (verified explicitly, see
 *                                            // xcb-project.md for the real before/after numbers).
 *   xcb_randomy::destroyVm(vm);              // per-thread, on worker shutdown.
 */

#ifndef XMRIG_XCB_RANDOMY_H
#define XMRIG_XCB_RANDOMY_H

#include <cstdint>

namespace xcb_randomy {

// Must be called before the FIRST createVm() call in the process to have any effect (see usage note
// above). Default (never called) is full/dataset mode (true) -- that is what actually fixes the real,
// absurdly-low cache-only hashrate; pass false to force the old light/cache-only behavior back on, e.g.
// as a low-RAM fallback, matching this project's own light-mode convention for every other RandomX coin.
void setFullMode(bool full);

void *createVm();
void destroyVm(void *vm);

// blob40 must point to exactly 40 readable bytes (sealhash[0..32) || nonce[32..40)); out must point to
// 32 writable bytes.
void hash(void *vm, const uint8_t *blob40, uint8_t *out32);

// Self-test: real vector cross-checked against go-randomy's own randomy_test.go (key="test key 000",
// input="This is a test" -> hash 0e9ab13e4b337c866e17445621be8d180c0a18a325932078dc33f7ac69ff17e8), run
// through THIS wrapper's exact code path (own one-off cache/vm, same calculate_hash call) -- not just the
// raw library. ALSO builds a one-off full/dataset-mode VM on the SAME key+input and checks it gives the
// BYTE-IDENTICAL hash (RandomX's own design guarantee between light and full modes) -- this exercises the
// real dataset-mode code path on every startup, regardless of which mode setFullMode() actually selected
// for production use. Returns true iff the renamed, vendored library still behaves identically in BOTH
// modes.
bool selfTest();

} // namespace xcb_randomy

#endif // XMRIG_XCB_RANDOMY_H
