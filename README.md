# poolpayminer

**poolpayminer is a modified version of [XMRig](https://github.com/xmrig/xmrig) 6.26.0. It is not the official XMRig** and is not
affiliated with the XMRig developers. It keeps XMRig's algorithms and adds the **Epic Cash** stratum protocol with Epic's own RandomX
variant (`rx/epic`), so that the miner can mine on Epic Cash pools such as `epic.pool-pay.com`. The original XMRig README is kept in
[README-XMRIG.md](README-XMRIG.md).

**CPU miner for Epic Cash (EPIC, RandomX `rx/epic`), Monero (XMR, RandomX `rx/0`), Raptoreum (GhostRider), Argon2 coins (Chukwa,
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

What the Epic support adds to XMRig: the Epic Cash stratum protocol (`--epic`), the `rx/epic` algorithm (RandomX with the instruction
frequencies and AES generator keys of the Epic node) and TLS stratum.

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

## Supported algorithms

poolpayminer mines what XMRig 6.26.0 mines (taken from its source, `src/base/crypto/Algorithm.h`), plus `rx/epic`, `rx/veil` and `rx/c64`. Only
**Epic Cash (`rx/epic`)** and **Veil (`rx/veil`)** have been tested end to end by the project (against the real Epic node and the pool
`epic.pool-pay.com`; for Veil against the project's pool code, whose hashing was checked against real Veil mainnet RandomX blocks; and, for the fee,
Nanopool); all other algorithms are XMRig's code, unchanged.

| Algorithm (`--algo`) | Coin | Family | Runs on |
|---|---|---|---|
| **`rx/epic`** (with `--epic`) | **Epic Cash (EPIC)**: RandomX with the Epic node's instruction frequencies and AES keys | RandomX | CPU |
| **`rx/veil`** | **Veil (VEIL)**: reference RandomX over the double SHA-256 of the 148 byte header | RandomX | CPU |
| **`rx/c64`** | **C64 Chain (C64)**: the RandomX variant of the C64 node | RandomX | CPU |
| `rx/0` | Monero (XMR) and other RandomX coins with the reference configuration | RandomX | CPU |
| `rx/2` | Monero, RandomX v2 | RandomX | CPU |
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
Keywords: RandomX miner, Epic Cash miner, EPIC mining, rx/epic, Monero RandomX CPU miner, GhostRider Raptoreum CPU miner, Argon2 Chukwa, CryptoNight,
XMRig fork, Windows miner, epic.pool-pay.com.
