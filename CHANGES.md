# The pool-pay.com fee on Riecoin (1.1.8)

* `-a ric` (see 1.1.7 below) handed the whole run to the bundled `rieMiner` for as long as the process lived, so it never carried the same fee every other algorithm pays: XMRig's own
  `DonateStrategy`/`FeeTable` split the *network connection* between the user's pool and the operator's route while its own worker threads keep hashing underneath, which only works because
  those threads exist; a run dispatched to `rieMiner` never starts them.
* `src/riecoin/Dispatch.cpp` now manages `rieMiner` as a child process instead of a single blocking call (`fork`/`execv` and `waitpid` on Linux, `CreateProcess`/`TerminateProcess` on Windows), on
  the same 99/1 schedule as `DonateStrategy` (`kFeeUnitMs`, `kDonateLevel`, mirrored from `donate.h`/`DonateStrategy.cpp`): after ~99 minutes it stops `rieMiner`, relaunches this same binary for
  ~1 minute with an ordinary `-a rx/0 -o ... -u ... -p ...` command line built from `FeeTable::mainRoute()` (the operator's CPU RandomX route, the same one every other coin's CPU miners already
  fee into), then stops that and resumes `rieMiner` on the user's pool. `Ctrl+C`/SIGTERM/SIGINT stop whichever child is currently running and exit cleanly instead of leaving it behind.
* `POOLPAYMINER_TEST_CPU_ROUTE` (the existing macro `FeeTable`/`DonateStrategy` use for their own test builds) also shortens `Dispatch.cpp`'s "minute" to a second here, for testing the switch
  without waiting ~100 real minutes.

# Riecoin (ric): one miner, two engines (1.1.7)

* New algorithm `ric` for Riecoin, but it is not RandomX and does not run on XMRig's own code: Riecoin's proof of work looks for constellations of prime numbers (GMP), a different kind of
  computation entirely. poolpayminer now ships the Riecoin team's own [rieMiner](https://github.com/RiecoinTeam/rieMiner) (MIT licence) next to itself, statically built for Linux x64 and
  Windows x64. Give `-a ric`, and `src/riecoin/Dispatch.cpp` (checked at the very start of `main()`, before any of XMRig's own argument parsing, so every other algorithm is untouched) turns
  `-o`/`-u`/`-p` (or the `pools[0]` of `-c config.json`) into a `rieMiner.conf` next to the executable and runs the bundled `rieMiner`/`rieMiner.exe` with inherited console output, returning
  its exit code. The result is one program and one familiar command line for every pool-pay.com coin, Riecoin included, even though the actual mining code underneath is a separate project.
* `packaging/ric/config.json`: ready config for ric.pool-pay.com.

# The algorithm rx/xla for Scala (1.1.6)

* New algorithm `rx/xla` (`Algorithm::RX_XLA`, aliases `randomx/xla`, `panthera`): Scala's variant of RandomX ("Panthera" / DefyX). Configuration `RandomX_ConfigurationXla`: Argon2 salt
  `DefyXScala\x13`, 128 MiB cache with 2 accesses, 32 MiB dataset base, scratchpads 64 KiB / 128 KiB / 256 KiB, 64 instruction programs, 1024 iterations, 4 programs per hash. The
  input of every hash is `blake2b` followed by yespower 1.0 (N = 2048, r = 8) and KangarooTwelve (`src/crypto/randomx/xla`, the sources of the node's library).
  Checked against the node's own library: the hashes of the miner and of the library are equal for the same job, and blocks built by the pool and found by this miner are accepted by a mock node that
  verifies with the library.
* RandomX changes needed for that: `ArgonMemory`, `CacheAccesses` and `DatasetBaseSize` are members of the configuration (constants before), the x86 JIT emits copies of the dataset read code
  with the dataset mask of the configuration (`codeReadDatasetTweaked` and the like, made in `Apply()`), `CacheLineAlignMask` is taken from the current configuration.
* Power saving for Scala: the miner rests at "Diardi" blocks (`--no-diardi-pause`, config `"diardi-pause": false` to switch it off): every block whose height is divisible by 4 can only be mined by the allow-listed
  miners of the Scala team, so hashing for it is useless. The pool marks such a job (`"diardi": true`, `Job::isDiardi()`); the miner pauses (`Network::setJob`), says why in its window
  ("Scala: resting while block N is found ...") and resumes by itself with the next job ("Scala: the Diardi block was found, mining resumes"). The hashrate shown by the miner drops to 0 meanwhile,
  the fee (donation) rounds are not affected. Without the flag from the pool nothing changes.
* `packaging/xla/config.json`: ready config for scala.pool-pay.com.

# The algorithm rx/scash for Satoshi Cash (1.1.5)

* New algorithm `rx/scash` (`Algorithm::RX_SCASH`, alias `randomx/scash`, `randomscash`): Satoshi Cash (SCASH) proof of work, RandomX 1.2.1 with the Argon2 salt
  `RandomX-Scash\x01` and the commitment: the value compared with the target is `randomx_calculate_commitment(header, hash)`, the hash itself goes into the block header.
  Configuration: `RandomX_ConfigurationScash` (`Tweak_V2_COMMITMENT`). The job is the 112 byte block header with the nonce at offset 76; the result carries the commitment
  as the share value and the RandomX hash as the extra field `commitment` (`JobResult::hasCommitment`). Checked against the real node (`scashd`, regtest): blocks found by the miner
  are accepted by the node.
* `packaging/scash/config.json`: ready config for scash.pool-pay.com.

# The algorithm rx/c64 for C64 Chain (1.1.4)

* New algorithm `rx/c64` (`Algorithm::RX_C64`, alias `randomx/c64`, `randomc64`): the RandomX variant of the C64 Chain node (parameters and AES generator keys as in the node's library;
  checked against the node's own library and the proof of work of real C64 mainnet blocks, and by mining real shares against a pool that validates with the node's library).
  Configuration: `RandomX_ConfigurationC64`.
* `packaging/c64/config.json`: ready config for c64.pool-pay.com.

# Errors of a pool that was never reachable are shown; -a rx/epic implies --epic (1.1.3)

* The quiet handling of lost connections (1.1.2) now applies only to a pool in which the miner has already logged in once. The errors of a pool that was
  never reachable (`connect error`, `DNS error`, `read error`) are shown again; before, a wrong address or a closed port left the window silent.
  Code: `Client::m_confirmed`.
* `-a rx/epic` (or `"algo": "rx/epic"`) switches on the Epic stratum by itself. Without `--epic` the miner used the usual XMRig protocol against the Epic
  node: the login went through, no job ever arrived, the connection was closed after 9 s and reopened, and nothing was printed. Code: `Pool::Pool(json)`.

# Quiet reconnects for every pool and algorithm (1.1.2)

* A connection that dies (some networks reset or silently drop long-lived TCP flows) is renewed at once and quietly for **every pool and every
  algorithm**, not only for Epic: no "read error", no "no active pools, stop mining" line, mining goes on with the current job meanwhile (up to 60 s).
  The messages are shown with `--verbose`. Code: `Pool::isResilient()`, `Client::read/reconnect/tick`, `Network::onPause/onActive`.
* Pinging the pool every 5 s and renewing a link that has been silent for 20 s is done only for pools that are known to answer a ping: Epic
  pools, Veil (`rx/veil`) and every pool that announces the `keepalive` extension (`Client::pingable()`). Any other pool is not pinged and not
  declared dead (it may say nothing for minutes between jobs); it only gets the quiet reconnect.
* Tested against a proxy that reset the connection every 25 s and against one that silently dropped all traffic: the miner reconnected each time
  within a second, printed no error lines, and shares kept being accepted.

# Windows 7 compatibility (1.1.1)

* The Windows builds are linked with libuv 1.48.0. That version calls `GetSystemTimePreciseAsFileTime` (Windows 8 and newer) directly, so the
  program did not start on Windows 7 ("entry point ... not found in kernel32.dll"). The patch `packaging/windows/libuv-1.48.0-windows7.patch`
  looks the function up at run time and falls back to `GetSystemTimeAsFileTime`. Apply it to the libuv 1.48.0 source before building the Windows
  binary (`patch -p1 < libuv-1.48.0-windows7.patch` in the libuv directory, paths are `a/src/win/util.c`). No change for Linux.

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
