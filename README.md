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
electricity for nothing (about 35% less power and heat on average, with the same earnings), says so in its window, says so in its window (`Scala: resting while block N is found ...`) and resumes by itself when the block is found (usually within
a couple of minutes). While it rests the hashrate shown by the miner is 0: this is not a fault, and the pool's hashrate figure is an average that is lower by the resting part. `--no-diardi-pause` (or
`"diardi-pause": false` in `config.json`) keeps the miner hashing all the time. A miner that does not know the rule keeps hashing for nothing during those blocks.

## Supported algorithms

poolpayminer mines what XMRig 6.26.0 mines (taken from its source, `src/base/crypto/Algorithm.h`), plus `rx/epic`, `rx/veil`, `rx/c64`, `rx/scash` and `rx/xla`. Only
**Epic Cash (`rx/epic`)** and **Veil (`rx/veil`)** have been tested end to end by the project (against the real Epic node and the pool
`epic.pool-pay.com`; for Veil against the project's pool code, whose hashing was checked against real Veil mainnet RandomX blocks; and, for the fee,
Nanopool); all other algorithms are XMRig's code, unchanged.

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
