#ifndef XMRIG_VERUS_H
#define XMRIG_VERUS_H

#include <cstddef>

// VerusHash2.2 (VerusCoin / VRSC) entry point for future CpuWorker.cpp wiring -- NOT yet called from
// anywhere in this codebase. See Algorithm.h's VERUSHASH comment and vrsc-project.md for what's still
// missing (the mining loop itself: constructing a 140 byte Zcash-style header with the 32 byte nonce
// field, placing extranonce1/extranonce2 and the search counter inside it, and handling the extra
// "solution" field from the stratum job -- this function only does the hash itself).
//
// `data` must be the exact bytes VerusHash2.2 hashes: the 140 byte header followed by the
// compactsize-prefixed solution (see vrsc-nodejs-pool's blockBuilder.js hashableBuffer(), which this
// must stay byte-compatible with). `out` receives the 32 byte hash.
//
// Calls CVerusHashV2::init() the first time (not thread-safe to call concurrently with itself -- call
// once at startup, same as every other algo's one-time init here) and aborts with a clear message if
// the CPU lacks AES-NI/PCLMUL (no portable/ARM fallback was ported -- see vrsc-project.md).
void verushash2_2_init();
void verushash2_2(const unsigned char *data, size_t len, unsigned char *out);

#endif
