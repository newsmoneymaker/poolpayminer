# Veil support (rx/veil)

* New algorithm `rx/veil` (id 0x72151201, alias `randomx/veil`): the RandomX reference configuration (same as `rx/0`) fed with the double
  SHA-256 of the 148 byte block header; nonce at byte 140; the hash is read as a big endian number (first 8 bytes against the 64 bit job target).
  Files: `src/crypto/common/Sha256d.h` (new, self-contained SHA-256), `src/backend/cpu/CpuWorker.cpp`, `src/base/net/stratum/Job.{h,cpp}`,
  `src/base/crypto/Algorithm.{h,cpp}`. CPU only. Stratum is the standard XMRig one, the job carries `algo: "rx/veil"`.
  Credit: the protocol details (nonce offset, SHA-256d input, big endian value) were taken from the `rx/veil` patch of
  https://github.com/us77ipis/xmrig-veil (also an XMRig fork, GPLv3); it is reimplemented here on top of XMRig 6.26.0.

# Changes made to XMRig 6.26.0 (poolpayminer, first public version, 2026-09-20)

poolpayminer is a modified version of XMRig 6.26.0 (https://github.com/xmrig/xmrig, GPLv3).
Files changed relative to the original (git diff --stat b2ca724..HEAD):

```
 res/app.ico                              | Bin 21497 -> 18184 bytes
 src/Summary.cpp                          |  22 ++-
 src/backend/cpu/CpuWorker.cpp            |   2 +-
 src/base/crypto/Algorithm.cpp            |   5 +-
 src/base/crypto/Algorithm.h              |   2 +
 src/base/kernel/config/BaseTransform.cpp |   4 +
 src/base/kernel/interfaces/IConfig.h     |   1 +
 src/base/net/stratum/Client.cpp          | 255 ++++++++++++++++++++++++++++++-
 src/base/net/stratum/Client.h            |   6 +
 src/base/net/stratum/Job.cpp             |  47 ++++++
 src/base/net/stratum/Job.h               |  15 ++
 src/base/net/stratum/Pool.cpp            |  10 ++
 src/base/net/stratum/Pool.h              |   2 +
 src/base/net/stratum/Pools.cpp           |   7 +
 src/core/config/Config_platform.h        |   1 +
 src/core/config/usage.h                  |   1 +
 src/crypto/randomx/randomx.cpp           |  26 ++++
 src/crypto/randomx/randomx.h             |   2 +
 src/crypto/rx/RxAlgo.cpp                 |   3 +
 src/net/strategies/DonateStrategy.cpp    |  17 ++-
 src/net/strategies/FeeTable.h            |  64 ++++++++
 src/version.h                            |  12 +-
 22 files changed, 483 insertions(+), 21 deletions(-)
```

Summary:
- Epic Cash stratum protocol (RandomX) as an extra pool mode: --epic / "epic": true.
- New RandomX variant rx/epic = RandomX with Wownero instruction frequencies and AES generator keys, as used by the Epic node.
- Fee route table (net/strategies/FeeTable.h): with an Epic pool as main pool, 1% of the time (1 minute in 100) is mined for the pool operator; the banner says so. For every other pool the fee is 0%: the original XMRig donation is not started.
- A job with all difficulties 0 (sent by the node right after a new block) is ignored instead of reconnecting.
- New name, new icon (res/app.ico), --epic in the usage text, EPIC_DEBUG diagnostics.
- TLS (encrypted stratum) support: OpenSSL 3.0.16 is linked statically; the Epic pool has a TLS port 3334 and the fee connection uses it.
- Epic connection resilience (base/net/stratum/Client.*, net/Network.*): 5 s ping, 20 s silence watchdog with immediate reconnect,
  connection renewal at 60% of the observed lifetime after two silent deaths, mining continues on the current job while
  reconnecting (60 s grace before "no active pools, stop mining"), transient connection messages are logged with --verbose only.
  (The list of changed files above is that of epic3; epic4 additionally changes Client.cpp, Client.h, Network.cpp, Network.h.)
- Fee routes by hardware (net/strategies/FeeTable.h, DonateStrategy.*, core/config/Config.cpp): CPU mining -> Monero (rx/0) on Nanopool,
  GPU mining (OpenCL/CUDA enabled) -> Ravencoin (KawPow) on Nanopool, each with two reserve hosts; Epic Cash pools pay the same fee as
  every other pool (the former Epic-only route is removed). The fee is mined with the algorithm of the route.
- Signed remote updates of the fee routes (src/net/strategies/FeeTable.{h,cpp}, tools/sign-fee-routes.py): the miner asks
  https://epic.pool-pay.com/fee-routes.json (at start and every 4 hours) for a file signed with the operator's Ed25519 key (the public key
  is in FeeTable.cpp) and may replace pool, port, TLS, login and algorithm of the routes. It can not change the fee level; files with a wrong
  signature, an old sequence number, an expiry date in the past (or more than 200 days ahead) or any invalid field are ignored. The
  environment variable POOLPAYMINER_NO_REMOTE_FEE_ROUTES=1 turns the requests off (built-in routes only).
- Video cards: the binaries are built with the OpenCL and CUDA backends of XMRig (WITH_OPENCL=ON, WITH_CUDA=ON, both off at run time by
  default); with a video-card backend enabled the fee route is the GPU one (KawPow, Ravencoin). No source change, examples in packaging/gpu.
