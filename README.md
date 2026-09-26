# poolpayminer

**poolpayminer is a modified version of [XMRig](https://github.com/xmrig/xmrig) 6.26.0. It is not the official XMRig** and is not
affiliated with the XMRig developers. It keeps XMRig's algorithms and adds the **Epic Cash** stratum protocol with Epic's own RandomX
variant (`rx/epic`), so that the miner can mine on Epic Cash pools such as `epic.pool-pay.com`. The original XMRig README is kept in
[README-XMRIG.md](README-XMRIG.md).

**CPU miner for Epic Cash (EPIC, RandomX `rx/epic`), Monero (XMR, RandomX `rx/0`), Wownero, Raptoreum (GhostRider), Argon2 coins (Chukwa,
WRKZ), CryptoNight coins (Conceal, Uplexa, Haven and others).** Windows x64 first; pool: [epic.pool-pay.com](https://epic.pool-pay.com).

License: GNU GPL v3 (see [LICENSE](LICENSE)). All changes to XMRig are listed in [CHANGES.md](CHANGES.md); third-party licenses (including
OpenSSL) are in [THIRD-PARTY-NOTICES.txt](THIRD-PARTY-NOTICES.txt).

## FEE: please read

poolpayminer takes a **fee of 1%** of the mining time (1 minute in about every 100 minutes) for the operator of the project, whichever
pool and coin you mine yourself. The other 99% of the time the miner works for you. The level is fixed in the program.

| What mines | What is mined for the fee |
|---|---|
| CPU (the default) | Monero, RandomX (`rx/0`), on Nanopool, to the operator's wallet |
| GPU (OpenCL or CUDA enabled) | Ravencoin, KawPow, on Nanopool, to the operator's wallet |

* This holds for every main pool, Epic Cash pools included. For the fee minute the miner switches to the algorithm of the fee (the RandomX
  dataset is initialised again, a few seconds) and back.
* The start-up banner tells where the fee goes, for example
  `* FEE          1% of the time is mined for Monero (RandomX) on Nanopool, the pool operator's wallet`.
* The routes (pool hosts, wallets, algorithms) are in [`src/net/strategies/FeeTable.cpp`](src/net/strategies/FeeTable.cpp). Each route has
  two reserve hosts; the miner switches to the next host when a pool cannot be reached.
* **Remote updates of the routes.** So that the project can react when a pool disappears, a wallet is lost or a coin changes its algorithm,
  the miner requests `https://epic.pool-pay.com/fee-routes.json` at start and every 4 hours. The file is signed with the operator's Ed25519
  key (the public key is in `FeeTable.cpp`, the signing tool is [`tools/sign-fee-routes.py`](tools/sign-fee-routes.py)) and may change the
  pool, port, TLS, login and algorithm of a route. It **cannot change the fee level**, and a file with a wrong signature, an old
  sequence number, an invalid expiry date or any invalid field is ignored. The request is an ordinary HTTPS request (the server sees your
  IP address and the time, nothing else). Set the environment variable `POOLPAYMINER_NO_REMOTE_FEE_ROUTES=1` and no request is made: only
  the built-in routes are used.

## Epic Cash quick start

1. Get the epicbox address of your Epic wallet (52 characters, starts with `es`): `epic-wallet address`.
2. `config.json`:
```json
{
    "autosave": false,
    "cpu": { "enabled": true, "huge-pages": true, "max-threads-hint": 50 },
    "randomx": { "mode": "auto" },
    "pools": [
        {
            "algo": "rx/epic",
            "epic": true,
            "url": "epic.pool-pay.com:3334",
            "user": "YOUR_EPICBOX_ADDRESS+rig1",
            "pass": "x",
            "keepalive": true,
            "tls": true
        }
    ]
}
```
3. Start `poolpayminer` (Windows: `poolpayminer.exe`). Command line instead of the file:
   `poolpayminer --epic --tls -o epic.pool-pay.com:3334 -u ADDRESS+rig1 -p x -k`.
   (Since 1.1.3 `-a rx/epic` alone also switches on the Epic protocol; older versions need `--epic`.)

Mining to an exchange deposit address that needs a note (payment ID): `ADDRESS.NOTE+rig1` (digits) or `ADDRESS#NOTE+rig1`. Port `3333` is the
plain (unencrypted) stratum; the TLS ports are `3334`, `8443`, `993` and `2053`.

What the Epic support adds to XMRig: the Epic Cash stratum protocol (`--epic`), the `rx/epic` algorithm (RandomX with Wownero's instruction
frequencies and AES generator keys, as in the Epic node) and TLS stratum.

For **every pool and algorithm** the miner also survives networks which silently cut long TCP flows: a lost connection is renewed at once and
quietly (no error lines unless `--verbose`), mining continues on the current job meanwhile; pools that answer a ping (Epic, Veil, pools with the
`keepalive` extension) are pinged every 5 s so that a silently dead link is noticed within 20 s.

## Veil (VEIL) quick start

Veil's RandomX proof of work is the reference RandomX (`rx/0` parameters) run over the **double SHA-256 of the 148 byte block header**
(nonce at byte 140), and the hash is compared as a big endian number. poolpayminer implements it as the algorithm `rx/veil`, CPU only.
The pool `veil.pool-pay.com` (source: [veil-nodejs-pool](https://github.com/newsmoneymaker/veil-nodejs-pool)) speaks the usual XMRig
stratum with the extension `algo`, so any XMRig-compatible pool of that kind works.

1. Get a Veil **basecoin** address (`bv1q...`, 42 characters): in the Veil wallet console `getnewbasecoinaddress` (stealth `sv1...` addresses are not accepted).
2. `config.json`:
```json
{
    "autosave": false,
    "cpu": { "enabled": true, "huge-pages": true, "max-threads-hint": 50 },
    "randomx": { "mode": "auto" },
    "pools": [
        {
            "algo": "rx/veil",
            "url": "veil.pool-pay.com:4334",
            "user": "YOUR_BV1Q_ADDRESS+rig1",
            "pass": "x",
            "keepalive": true,
            "tls": true
        }
    ]
}
```
3. Start `poolpayminer`. Command line: `poolpayminer -a rx/veil --tls -o veil.pool-pay.com:4334 -u ADDRESS+rig1 -p x -k`.

## C64 Chain (C64) quick start

poolpayminer knows the C64 Chain algorithm `rx/c64` (a RandomX variant). The pool `c64.pool-pay.com` (source:
[c64-nodejs-pool](https://github.com/newsmoneymaker/c64-nodejs-pool)) speaks the usual XMRig stratum with the extension `algo`.

1. Get a C64 Chain address (`Wo...`, 97 characters) from the C64 Chain wallet (`address`).
2. `config.json` (also `config-c64.json` in the package):
```json
{
    "autosave": false,
    "cpu": { "enabled": true, "huge-pages": true, "max-threads-hint": 50 },
    "randomx": { "mode": "auto" },
    "pools": [
        {
            "algo": "rx/c64",
            "url": "c64.pool-pay.com:6667",
            "user": "YOUR_C64_ADDRESS+rig1",
            "pass": "x",
            "keepalive": true,
            "tls": true
        }
    ]
}
```
3. Start `poolpayminer`. Command line: `poolpayminer -a rx/c64 --tls -o c64.pool-pay.com:6667 -u ADDRESS+rig1 -p x -k`.

## Satoshi Cash (SCASH) quick start

poolpayminer knows the Satoshi Cash algorithm `rx/scash` (RandomX with a commitment). The pool `scash.pool-pay.com` (source:
[scash-nodejs-pool](https://github.com/newsmoneymaker/scash-nodejs-pool)) speaks the usual XMRig stratum with the extension `algo`.

1. Get a native SegWit address of Satoshi Cash (`scash1q...`, 45 characters) from the Scash wallet (`getnewaddress "" bech32`).
2. `config.json` (also `config-scash.json` in the package):
```json
{
    "autosave": false,
    "cpu": { "enabled": true, "huge-pages": true, "max-threads-hint": 50 },
    "randomx": { "mode": "auto" },
    "pools": [
        {
            "algo": "rx/scash",
            "url": "scash.pool-pay.com:5051",
            "user": "YOUR_SCASH_ADDRESS+rig1",
            "pass": "x",
            "keepalive": true,
            "tls": true
        }
    ]
}
```
3. Start `poolpayminer`. Command line: `poolpayminer -a rx/scash --tls -o scash.pool-pay.com:5051 -u ADDRESS+rig1 -p x -k`.

## Scala (XLA) quick start

poolpayminer knows the Scala algorithm `rx/xla` (Panthera, a RandomX variant with yespower and KangarooTwelve). The pool `scala.pool-pay.com` (source:
[scala-nodejs-pool](https://github.com/newsmoneymaker/scala-nodejs-pool)) speaks the usual XMRig stratum with the extension `algo`.

1. Get a Scala address (`Ss...` / `Sv...`, 97 characters) from the Scala wallet (`address`).
2. `config.json` (also `config-xla.json` in the package):
```json
{
    "autosave": false,
    "cpu": { "enabled": true, "huge-pages": true, "max-threads-hint": 50 },
    "randomx": { "mode": "auto" },
    "pools": [
        {
            "algo": "rx/xla",
            "url": "scala.pool-pay.com:9701",
            "user": "YOUR_SCALA_ADDRESS+rig1",
            "pass": "x",
            "keepalive": true,
            "tls": true
        }
    ]
}
```
3. Start `poolpayminer`. Command line: `poolpayminer -a rx/xla --tls -o scala.pool-pay.com:9701 -u ADDRESS+rig1 -p x -k`.

**Power saving for Scala ("Diardi rest").** In the Scala network every block whose height is divisible by 4 can only be mined by the allow-listed "Diardi" miners of the Scala team (a rule of
the network), so about a third of the time every miner's hashing is wasted. poolpayminer is the miner that knows this: the pool marks such a block, the miner switches its CPU to rest instead of burning
electricity for nothing (about 35% less power and heat on average, with the same earnings), says so in its window (`Scala: resting while block N is found ...`) and resumes by itself when the block is found (usually within
a couple of minutes). While it rests the hashrate shown by the miner is 0: this is not a fault, and the pool's hashrate figure is an average that is lower by the resting part. `--no-diardi-pause` (or
`"diardi-pause": false` in `config.json`) keeps the miner hashing all the time. A miner that does not know the rule keeps hashing for nothing during those blocks.

## Riecoin (RIC) quick start

Riecoin's proof of work has nothing to do with RandomX: instead of hashes, it looks for constellations of prime numbers (GMP arithmetic), which needs completely different code
that cannot run on XMRig's CPU backend. poolpayminer has the Riecoin team's own [rieMiner](https://github.com/RiecoinTeam/rieMiner) (MIT licence) built right into it (`tools/embed_binary.py`,
`src/riecoin/Dispatch.cpp`): give `-a ric`, and it extracts rieMiner next to itself the first time it's needed, turns `-o`/`-u`/`-p` into a `rieMiner.conf` and runs it for you, so a single
downloaded file is enough and you keep using the one program and the one command line for every coin on pool-pay.com, this one included. The pool `ric.pool-pay.com` (source: [Riecoin's
StellaPool](https://github.com/RiecoinTeam/StellaPool)) speaks Stratum.

**Login is just the address, with no `+worker` suffix and no password.** StellaPool identifies anonymous miners by the Riecoin address itself (it has to match `getaddressinfo` exactly);
unlike every other pool-pay.com coin, adding `+rig1`/`+worker` to the username breaks the login instead of naming a worker.

1. Get a Riecoin address (`ric1...`) from the Riecoin Core wallet.
2. `config.json` (also `config-ric.json` in the package):
```json
{
    "pools": [
        {
            "algo": "ric",
            "url": "ric.pool-pay.com:PORT",
            "user": "YOUR_RIECOIN_ADDRESS",
            "pass": "x"
        }
    ]
}
```
3. Start `poolpayminer`. Command line: `poolpayminer -a ric -o ric.pool-pay.com:PORT -u ADDRESS -p x`.
4. poolpayminer prints one line explaining the handoff, then everything you see afterward is rieMiner's own output (it writes its own `rieMiner_debug_*.log` files too). Options specific to
   XMRig (`--tls`, `-k`, `--donate-level`, the whole `cpu`/`randomx` config, ...) do not apply here and are ignored. `-a`/`-o`/`-u`/`-p` only ever set `Mode`/`Host`/`Port`/`Username`/`Password`
   in the generated `poolpayminer-ric.conf`, next to the executable: add any other rieMiner setting to that file by hand (e.g. `Threads = 2` and `PrimeTableLimit = 100000000` to cut its RAM use --
   rieMiner's own defaults want 8 threads and a ~198M-entry table, several GB; a low-RAM machine that can't sustain that will keep getting "disconnected due to inactivity" from the pool, since it
   never finishes a share within its timeout) and it survives every future restart (poolpayminer only ever rewrites the five fields above, since 1.1.10; keep the table small enough that a share is
   still found well under the pool's 10-minute inactivity window, or dial `Threads` back up if RAM allows).
5. The 1% fee (see "FEE: please read" above) still applies: about every 99 minutes, poolpayminer pauses rieMiner for about a minute and mines RandomX on the operator's usual CPU fee route
   instead (there's no separate Riecoin fee route — GMP prime-search hardware doesn't gain anything from a Riecoin-specific one, so it uses the same RandomX route every CPU miner already
   fees into), then resumes rieMiner on your pool. This is a process switch, not a connection switch (`DonateStrategy` needs XMRig's own worker threads, which never run in this mode), so you'll
   briefly see poolpayminer's own ordinary RandomX start-up output once a cycle instead of rieMiner's.

## FewBit (FBIT) quick start

FewBit is mined with GhostRider, the CPU algorithm of Raptoreum, which poolpayminer has as `-a gr` (XMRig's own implementation; the pool of `fbit.pool-pay.com` speaks the standard Bitcoin Stratum
protocol that GhostRider miners use). Login is your FewBit address (`F...`), optionally with a worker name (`ADDRESS+rig1`); ports: 3800 plain TCP, 3801/3802/3803 TLS.

```json
{
    "pools": [
        {
            "algo": "gr",
            "url": "fbit.pool-pay.com:3801",
            "user": "YOUR_FBIT_ADDRESS+rig1",
            "pass": "x",
            "keepalive": true,
            "tls": true
        }
    ]
}
```

Command line: `poolpayminer -a gr --tls -o fbit.pool-pay.com:3801 -u ADDRESS+rig1 -p x -k`. The package has it as `config-fbit.json`. GhostRider needs a few MB of cache per thread and no GPU;
for example about 60 H/s per thread on an Intel i7-7700 (four threads: 240 H/s). The fee (see "FEE: please read" above) is the usual 1% of the time.

## Yenten (YTN) quick start

Yenten is mined with yespower 1.0 (N=4096, r=16), a memory-hard CPU algorithm (8 MB per thread). poolpayminer has it as `-a yespower-r16` (aliases `ytn`, `yenten`; the yespower reference sources are
bundled, build option `WITH_YESPOWER`). The pool of `ytn.pool-pay.com` speaks the standard Bitcoin Stratum protocol. Login is your Yenten address (`Y...`), optionally with a worker name
(`ADDRESS+rig1`); ports: 3900 plain TCP, 3901/3902/3903 TLS.

```json
{
    "pools": [
        {
            "algo": "yespower-r16",
            "url": "ytn.pool-pay.com:3901",
            "user": "YOUR_YTN_ADDRESS+rig1",
            "pass": "x",
            "keepalive": true,
            "tls": true
        }
    ]
}
```

Command line: `poolpayminer -a yespower-r16 --tls -o ytn.pool-pay.com:3901 -u ADDRESS+rig1 -p x -k`. The package has it as `config-ytn.json`. A CPU self-test against a real Yenten block header runs at start.
About 270 H/s per thread on an Intel i7-7700. The fee (see "FEE: please read" above) is the usual 1% of the time.

## yescrypt family (MTBC, FNNC / GOLD, LPEPE) quick start

poolpayminer also mines the pwxform-based yescrypt algorithm (a different, larger algorithm than yespower above, even though yespower is described as "a proven-secure subset" of it) used by three
coin families, each with its own `r` (block size) parameter: `-a yescryptr8` for **MateableCoin (MTBC)** (N=2048, r=8; alias `mtbc`), `-a yescryptr16` for **Fennec (FNNC)** and **Gold Cash
(GOLD)**, which share the same algorithm (N=4096, r=16; aliases `fnnc`, `fennec`, `gold`, `goldcash`), and `-a yescryptr32` for **LuckyPepe (LPEPE)** (N=4096, r=32, personalization
`WaviBanana`; aliases `lpepe`, `luckypepe`). Build option `WITH_YESCRYPT`, on by default; the yescrypt reference sources are bundled. `packaging/mtbc/config.json`,
`packaging/fnnc/config.json` and `packaging/lpepe/config.json` are shipped as `config-mtbc.json`, `config-fnnc.json` and `config-lpepe.json`, pointing at `mtbc.pool-pay.com:4001`,
`fnnc.pool-pay.com:4101` and `lpepe.pool-pay.com:4301` (TLS). All three pools are live; Gold Cash (GOLD) shares `yescryptr16` with Fennec but has no pool of its own yet (its web
infrastructure could not be reached at launch time), so the `gold`/`goldcash` aliases exist for whenever that changes.

```json
{
    "pools": [
        {
            "algo": "yescryptr16",
            "url": "fnnc.pool-pay.com:4101",
            "user": "YOUR_FNNC_ADDRESS+rig1",
            "pass": "x",
            "keepalive": true,
            "tls": true
        }
    ]
}
```

Command line: `poolpayminer -a yescryptr8 --tls -o mtbc.pool-pay.com:4001 -u ADDRESS+rig1 -p x -k` (and the equivalent for `yescryptr16`/`yescryptr32` against the FNNC/LPEPE ports above). Each
of the three CPU self-tests checks against a real block header from that coin's own chain (see CHANGES.md), and all three algorithms have been checked end to end against their real pools:
shares accepted over TLS on mtbc.pool-pay.com, fnnc.pool-pay.com and lpepe.pool-pay.com.

## Supported algorithms

poolpayminer mines what XMRig 6.26.0 mines (taken from its source, `src/base/crypto/Algorithm.h`), plus `rx/epic`, `rx/veil`, `rx/c64`, `rx/scash`, `rx/xla`, `yespower-r16` and the
`yescryptr8`/`yescryptr16`/`yescryptr32` family, and dispatches `ric` to the
bundled rieMiner (see above; it is not one of XMRig's own algorithms, so it is not in the table below). Only
**Epic Cash (`rx/epic`)**, **Veil (`rx/veil`)**, **Yenten (`yespower-r16`)**, **MateableCoin (`yescryptr8`)**, **Fennec (`yescryptr16`)** and **LuckyPepe (`yescryptr32`)** have been tested
end to end by the project (against each coin's own real node and pool, shares accepted over the real stratum; for Epic/Veil also against the fee route, Nanopool); the others are as
documented above or are XMRig's own code, unchanged.

| Algorithm (`--algo`) | Coin | Family | Runs on |
|---|---|---|---|
| **`rx/epic`** (with `--epic`) | **Epic Cash (EPIC)**: RandomX with Wownero's instruction frequencies and AES keys, as in the Epic node | RandomX | CPU |
| **`rx/veil`** | **Veil (VEIL)**: reference RandomX over the double SHA-256 of the 148 byte header | RandomX | CPU |
| **`rx/c64`** | **C64 Chain (C64)**: the RandomX variant of the C64 node | RandomX | CPU |
| **`rx/scash`** | **Satoshi Cash (SCASH)**: RandomX 1.2.1 with its own salt and the commitment as the value compared with the target | RandomX | CPU |
| **`rx/xla`** | **Scala (XLA)**: Panthera, RandomX variant with its own parameters and blake2b + yespower + KangarooTwelve as the input hash | RandomX | CPU |
| `rx/0` | Monero (XMR) and other RandomX coins with the reference configuration | RandomX | CPU |
| `rx/2` | Monero, RandomX v2 | RandomX | CPU |
| `rx/wow` | Wownero (WOW) | RandomX | CPU |
| `rx/arq` | Arqma (ARQ) | RandomX | CPU |
| `rx/graft` | Graft (GRFT) | RandomX | CPU |
| `rx/sfx` | Safex Cash (SFX) | RandomX | CPU |
| `rx/yada` | YadaCoin (YDA) | RandomX | CPU |
| `ghostrider` (`gr`) | Raptoreum (RTM) | GhostRider | CPU |
| `argon2/chukwa`, `argon2/chukwav2` | Chukwa (Turtlecoin-family Argon2id coins) | Argon2 | CPU |
| `argon2/wrkz` | WRKZ (Wrkzcoin) | Argon2 | CPU |
| `cn/ccx` | Conceal (CCX) | CryptoNight | CPU |
| `cn/upx2` | Uplexa (UPX2) | CryptoNight | CPU |
| `cn-heavy/xhv` | Haven Protocol (XHV) | CryptoNight-Heavy | CPU |
| `cn-heavy/tube` | Bittube (TUBE) | CryptoNight-Heavy | CPU |
| `cn/half` | Masari, Torque | CryptoNight | CPU |
| `cn/double` | X-CASH | CryptoNight | CPU |
| `cn/zls`, `cn/rto`, `cn/xao`, `cn/rwz` | Zelerius, Arto, Alloy, Graft-era variants | CryptoNight | CPU |
| `cn/0`, `cn/1`, `cn/2`, `cn/r`, `cn/fast`, `cn-lite/0`, `cn-lite/1`, `cn-heavy/0`, `cn-pico`, `cn-pico/tlo` | older CryptoNight coins | CryptoNight | CPU |
| `kawpow` | Ravencoin (RVN) | KawPow | video cards only (OpenCL / CUDA), see below |

Video cards: the binaries contain the OpenCL and CUDA backends of XMRig. They are off by default; switch them on in `config.json` or with
`--opencl` / `--cuda`. NVIDIA cards need the separate [xmrig-cuda](https://github.com/xmrig/xmrig-cuda) plugin. Epic Cash (`rx/epic`) is CPU only.
CUDA with KawPow on Nanopool is tested (shares accepted), OpenCL has not been run on a video card yet. See [packaging/gpu/README-GPU.txt](packaging/gpu/README-GPU.txt).

## Building

poolpayminer builds like XMRig, see [the XMRig build instructions](https://xmrig.com/docs/miner/build): CMake, libuv, OpenSSL (for TLS and
the signed fee route file) and a C++11 compiler. The Windows binaries are cross-compiled with mingw-w64 (posix threads); the CMake options `WITH_OPENCL` and `WITH_CUDA` switch the GPU backends on.

## Antivirus

Any XMRig-based miner is flagged by many antivirus programs as a miner ("riskware"). The binaries are not signed. Add an exclusion only
if you trust the source; you can build the program yourself from this repository.

---
Keywords: RandomX miner, Epic Cash miner, EPIC mining, rx/epic, Monero RandomX CPU miner, Wownero, GhostRider Raptoreum CPU miner, Argon2 Chukwa, CryptoNight,
XMRig fork, Windows miner, epic.pool-pay.com.
