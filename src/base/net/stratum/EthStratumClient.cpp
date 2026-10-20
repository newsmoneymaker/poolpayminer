/* XMRig
 * Copyright (c) 2018-2021 SChernykh   <https://github.com/SChernykh>
 * Copyright (c) 2016-2021 XMRig       <https://github.com/xmrig>, <support@xmrig.com>
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <cinttypes>
#include <iomanip>
#include <sstream>
#include <stdexcept>


#include "base/net/stratum/EthStratumClient.h"
#include "3rdparty/libethash/endian.h"
#include "3rdparty/rapidjson/document.h"
#include "3rdparty/rapidjson/error/en.h"
#include "3rdparty/rapidjson/stringbuffer.h"
#include "3rdparty/rapidjson/writer.h"
#include "base/io/json/Json.h"
#include "base/io/json/JsonRequest.h"
#include "base/io/log/Log.h"
#include "base/kernel/interfaces/IClientListener.h"
#include "net/JobResult.h"

#ifdef XMRIG_ALGO_GHOSTRIDER
#include <cmath>

extern "C" {
#include "crypto/ghostrider/sph_sha2.h"
}

#include "base/tools/Cvt.h"
#include <openssl/evp.h>

// Kylacoin (KCN) and Lyncoin (LCN) only: both forks' CTransaction::ComputeHash()/CMutableTransaction::GetHash()
// use "Hash3Writer" (src/hash.h): double, real NIST SHA3-256 (domain separator 0x06, confirmed in each node's
// own src/crypto/sha3.cpp) -- NOT Keccak-256 or Bitcoin's usual double-SHA256. Confirmed directly from both
// nodes' sources (kcn-src, lcn-src): Kylacoin applies this to EVERY transaction's id unconditionally; Lyncoin
// only to transactions whose own version field is 8, which is why Lyncoin's pool (lcn-nodejs-pool) forces the
// coinbase to tx version 8 while Kylacoin's (kcn-nodejs-pool) does not need to -- the client here doesn't need
// to know that detail, it only needs to know WHICH algorithm's coinbase leaf hash is sha3d instead of sha256d.
// Only this ONE leaf (the coinbase, i.e. the buffer hashed here from coinb1+extranonce+coinb2) changes hash
// algo; the merkle tree's own combining step downstream (the sha256d(merkle_root, merkle_root, 64) loop over
// the branches) stays sha256d unconditionally on both coins -- confirmed in each node's own consensus/merkle.cpp:
// ComputeMerkleRoot() always calls SHA256D64 to combine, regardless of any transaction's version; only the
// per-tx leaf hash is coin/version-conditional, and any OTHER (non-coinbase) transaction in a block already
// arrives from the node's own getblocktemplate with its real, already-correctly-hashed txid -- only the
// coinbase is built fresh here and needs its hash computed to match.
// OpenSSL (already linked for TLS) has had EVP_sha3_256() since 1.1.1 -- no second SHA3 implementation needed.
static bool sha3d_flex_coinbase(uint8_t *out32, const uint8_t *data, size_t size)
{
    uint8_t tmp[32];
    unsigned int len = 0;

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) {
        return false;
    }

    bool ok = (EVP_DigestInit_ex(ctx, EVP_sha3_256(), nullptr) == 1) &&
              (EVP_DigestUpdate(ctx, data, size) == 1) &&
              (EVP_DigestFinal_ex(ctx, tmp, &len) == 1);

    if (ok) {
        EVP_MD_CTX_reset(ctx);
        ok = (EVP_DigestInit_ex(ctx, EVP_sha3_256(), nullptr) == 1) &&
             (EVP_DigestUpdate(ctx, tmp, sizeof(tmp)) == 1) &&
             (EVP_DigestFinal_ex(ctx, out32, &len) == 1);
    }

    EVP_MD_CTX_free(ctx);
    return ok;
}
#endif



xmrig::EthStratumClient::EthStratumClient(int id, const char *agent, IClientListener *listener) :
    Client(id, agent, listener)
{
}


int64_t xmrig::EthStratumClient::submit(const JobResult& result)
{
#   ifndef XMRIG_PROXY_PROJECT
    if ((m_state != ConnectedState) || !m_authorized) {
        return -1;
    }
#   endif

    if (result.diff == 0) {
        LOG_ERR("%s " RED("result.diff is 0"), tag());
        close();

        return -1;
    }

    using namespace rapidjson;

    Document doc(kObjectType);
    auto& allocator = doc.GetAllocator();

    Value params(kArrayType);
    params.PushBack(m_user.toJSON(), allocator);
    params.PushBack(result.jobId.toJSON(), allocator);

#   ifdef XMRIG_ALGO_XCB
    if (m_pool.algorithm().id() == Algorithm::RX_XCB) {
        // Core Blockchain (XCB): [user, job_id, nonce]. Pool already knows the job's sealhash from
        // job_id, no need to echo a header/mix hash back (there is no "mix digest" concept here at all --
        // RandomY has no DAG). nonce is sent as plain big-endian hex text of the integer value this
        // miner searched -- "0x" + 16 hex digits, most-significant-digit-first -- which is EXACTLY what
        // go-core's hexutil decodes back into the same 8 raw bytes BigEndian.PutUint64() would produce,
        // so the pool can forward it to cryptore_submitWork() unchanged (see xcb-nodejs-pool's pool.js
        // and xcb-project.md for the full byte-order derivation: this miner's own nonce-splicing
        // machinery writes the SAME integer value as LittleEndian bytes into the hash seed, which is
        // what go-core's own RandomY() does internally too -- no reversal needed anywhere on this path).
        std::stringstream s;
        s << "0x" << std::hex << std::setw(16) << std::setfill('0') << result.nonce;
        params.PushBack(Value(s.str().c_str(), allocator), allocator);

        JsonRequest::create(doc, m_sequence, "mining.submit", params);
        // RX_XCB's hash is read big-endian (bigEndianValue(), top 8 bytes), not little-endian bytes
        // 24..31 like result.actualDiff() assumes -- compute the display-only "actual diff" the same way
        // Job::bigEndianValue() consumers do, so logs show a sane number instead of a bytes-24..31 read
        // of the wrong end of the hash.
        m_results[m_sequence] = SubmitResult(m_sequence, result.diff, Job::toDiff(Job::epicValue(result.result())), 0, result.backend);

        return send(doc);
    }
#   endif

#   ifdef XMRIG_ALGO_VERUSHASH
    if (m_pool.algorithm().id() == Algorithm::VERUSHASH) {
        // VerusCoin's real submit shape (equi_stratum_submit(), see vrsc-project.md): [worker, job_id,
        // ntime, nonce, solution]. `nonce` here is the client's FULL extranonce2 region (EN2_SIZE bytes,
        // matching m_extraNonce2Size) -- this miner only ever varies the first 4 bytes of it (its own
        // per-job search counter, result.nonce) and leaves the rest zero, which is a protocol-valid
        // choice (the server only cares that the submitted bytes are internally consistent with what it
        // gets hashed, not that the miner used the entire assigned search space).
        params.PushBack(Value(m_ntime.data(), allocator), allocator);

        // REAL BUG, found via a live round-trip test against vrsc-nodejs-pool's real pool.js
        // (2026-10-21): `result.nonce` was formatted as a big-endian hex TEXT representation of the
        // integer (`std::hex << result.nonce`), e.g. value 0x12345678 -> "12345678". But the header's
        // own nonce field (Job::nonceOffset(), written by the generic WorkerJob::writeUnaligned
        // machinery, same as every other algorithm here) stores it as the raw NATIVE (little-endian on
        // x86) bytes of that uint32 -- e.g. the same 0x12345678 sits in the header as bytes
        // 78 56 34 12. The pool's onSubmit() does a plain `Buffer.from(nonceHex, 'hex')` on whatever
        // hex string arrives, so the old "12345678" text reconstructed the BYTE-REVERSED value
        // (12 34 56 78) -- a nonce the miner never actually hashed with. Every single submitted share
        // failed the pool's independent re-check ("Low difficulty share") as a result, even though the
        // miner's own self-test and internal target comparison were both already correct. Fixed by
        // hex-encoding the raw native bytes directly (Cvt::toHex), the same pattern every other
        // Bitcoin-stratum-family coin here already uses for its own nonce field (see DaemonClient.cpp/
        // SelfSelectClient.cpp: `Cvt::toHex(..., reinterpret_cast<const uint8_t*>(&result.nonce), 4)`).
        uint32_t nonceLE = static_cast<uint32_t>(result.nonce);
        String nonceHex = Cvt::toHex(reinterpret_cast<const uint8_t *>(&nonceLE), sizeof(nonceLE));
        std::string nonceStr(nonceHex.data(), nonceHex.size());
        for (uint64_t i = 8; i < m_extraNonce2Size * 2; ++i) nonceStr += '0';
        params.PushBack(Value(nonceStr.c_str(), allocator), allocator);

        // Solution is fixed per job and not threaded through JobResult (which only carries the fields it
        // needs to build the submit, not a full Job copy) -- read it from m_job directly, same as m_ntime
        // above. A race against a brand new job arriving between "share found" and "submit sent" is
        // already accepted by this file for m_ntime; the pool-side job_id check (vrsc-nodejs-pool's
        // pool.js onSubmit()) rejects a truly stale submit either way.
        params.PushBack(Value(Cvt::toHex(m_job.verusSolution()).data(), allocator), allocator);

        JsonRequest::create(doc, m_sequence, "mining.submit", params);
        m_results[m_sequence] = SubmitResult(m_sequence, result.diff, result.actualDiff(), 0, result.backend);

        return send(doc);
    }
#   endif

#   ifdef XMRIG_ALGO_GHOSTRIDER
    if (Algorithm::isBitcoinStratum(m_pool.algorithm().id())) {
        params.PushBack(Value("00000000000000000000000000000000", static_cast<uint32_t>(m_extraNonce2Size * 2)), allocator);
        params.PushBack(Value(m_ntime.data(), allocator), allocator);

        std::stringstream s;
        s << std::hex << std::setw(8) << std::setfill('0') << result.nonce;
        params.PushBack(Value(s.str().c_str(), allocator), allocator);
    }
    else
#   endif
    {
        std::stringstream s;
        s << "0x" << std::hex << std::setw(16) << std::setfill('0') << result.nonce;
        params.PushBack(Value(s.str().c_str(), allocator), allocator);

        s.str(std::string());
        s << "0x";
        for (size_t i = 0; i < 32; ++i) {
            const uint32_t k = result.headerHash()[i];
            s << std::hex << std::setw(2) << std::setfill('0') << k;
        }
        params.PushBack(Value(s.str().c_str(), allocator), allocator);

        s.str(std::string());
        s << "0x";
        for (size_t i = 0; i < 32; ++i) {
            const uint32_t k = result.mixHash()[i];
            s << std::hex << std::setw(2) << std::setfill('0') << k;
        }
        params.PushBack(Value(s.str().c_str(), allocator), allocator);
    }

    JsonRequest::create(doc, m_sequence, "mining.submit", params);

    uint64_t actual_diff;

#   ifdef XMRIG_ALGO_GHOSTRIDER
    if (Algorithm::isBitcoinStratum(result.algorithm.id())) {
        actual_diff = reinterpret_cast<const uint64_t*>(result.result())[3];
    }
    else
#   endif
    {
        actual_diff = ethash_swap_u64(*((uint64_t*)result.result()));
    }

    actual_diff = actual_diff ? (uint64_t(-1) / actual_diff) : 0;

#   ifdef XMRIG_PROXY_PROJECT
    m_results[m_sequence] = SubmitResult(m_sequence, result.diff, actual_diff, result.id, 0);
#   else
    m_results[m_sequence] = SubmitResult(m_sequence, result.diff, actual_diff, 0, result.backend);
#   endif

    return send(doc);
}


void xmrig::EthStratumClient::login()
{
    m_results.clear();

    subscribe();
    authorize();
}


void xmrig::EthStratumClient::onClose()
{
    m_authorized = false;
    Client::onClose();
}


bool xmrig::EthStratumClient::handleResponse(int64_t id, const rapidjson::Value &result, const rapidjson::Value &error)
{
    auto it = m_callbacks.find(id);
    if (it != m_callbacks.end()) {
        const uint64_t elapsed = Chrono::steadyMSecs() - it->second.ts;

        if (error.IsArray() || error.IsObject() || error.IsString()) {
            it->second.callback(error, false, elapsed);
        }
        else {
            it->second.callback(result, true, elapsed);
        }

        m_callbacks.erase(it);

        return true;
    }

    return handleSubmitResponse(id, errorMessage(error));
}


void xmrig::EthStratumClient::parseNotification(const char *method, const rapidjson::Value &params, const rapidjson::Value &)
{
    if (strcmp(method, "mining.set_target") == 0) {
        // VERUSHASH and RX_XCB only (every other Bitcoin-stratum coin here uses mining.set_difficulty
        // instead, handled below) -- see vrsc-nodejs-pool's pool.js / xcb-nodejs-pool's pool.js: a real
        // 256 bit target, plain big-endian hex text (not byte-reversed, it's a number not a buffer), 64
        // hex chars. Reused as-is for XCB -- same "keep the top 8 bytes as a uint64" simplification.
        bool wantsTarget = m_pool.algorithm().id() == Algorithm::VERUSHASH;
#       ifdef XMRIG_ALGO_XCB
        wantsTarget = wantsTarget || m_pool.algorithm().id() == Algorithm::RX_XCB;
#       endif
        if (wantsTarget && params.IsArray() && params.Size() > 0 && params[0].IsString()) {
            const char *targetHex = params[0].GetString();
            if (strlen(targetHex) == 64) {
                uint64_t top = 0;
                bool ok = true;
                for (int i = 0; i < 16 && ok; ++i) {
                    int c = targetHex[i];
                    int v = (c >= '0' && c <= '9') ? (c - '0') : (c >= 'a' && c <= 'f') ? (c - 'a' + 10) : (c >= 'A' && c <= 'F') ? (c - 'A' + 10) : -1;
                    if (v < 0) { ok = false; break; }
                    top = (top << 4) | static_cast<uint64_t>(v);
                }
                if (ok) {
                    m_nextTarget = top;
                }
            }
        }
        return;
    }

    if (strcmp(method, "mining.set_extranonce") == 0) {
        if (!params.IsArray()) {
            LOG_ERR("%s " RED("invalid mining.set_extranonce notification: params is not an array"), tag());
            return;
        }

        auto arr = params.GetArray();

        if (arr.Empty()) {
            LOG_ERR("%s " RED("invalid mining.set_extranonce notification: params array is empty"), tag());
            return;
        }

        setExtraNonce(arr[0]);
    }

#   ifdef XMRIG_ALGO_GHOSTRIDER
    if (strcmp(method, "mining.set_difficulty") == 0) {
        if (!params.IsArray()) {
            LOG_ERR("%s " RED("invalid mining.set_difficulty notification: params is not an array"), tag());
            return;
        }

        if (!Algorithm::isBitcoinStratum(m_pool.algorithm().id())) {
            return;
        }

        auto arr = params.GetArray();

        if (arr.Empty()) {
            LOG_ERR("%s " RED("invalid mining.set_difficulty notification: params array is empty"), tag());
            return;
        }

        if (!arr[0].IsDouble() && !arr[0].IsUint64()) {
            LOG_ERR("%s " RED("invalid mining.set_difficulty notification: difficulty is not a number"), tag());
            return;
        }

        const double diff = arr[0].IsDouble() ? arr[0].GetDouble() : arr[0].GetUint64();
        m_nextDifficulty = static_cast<uint64_t>(ceil(diff * 65536.0));
    }
#   endif

    if (strcmp(method, "mining.notify") == 0) {
        if (!params.IsArray()) {
            LOG_ERR("%s " RED("invalid mining.notify notification: params is not an array"), tag());
            return;
        }

        auto arr = params.GetArray();

        auto algo = m_pool.algorithm();
        if (!algo.isValid()) {
            algo = m_pool.coin().algorithm();
        }

        const size_t min_arr_size = (algo.id() == Algorithm::VERUSHASH) ? 9 : (algo.id() == Algorithm::RX_XCB) ? 5 : (Algorithm::isBitcoinStratum(algo.id()) ? 8 : 6);

        if (arr.Size() < min_arr_size) {
            LOG_ERR("%s " RED("invalid mining.notify notification: params array has wrong size"), tag());
            return;
        }

        if (!arr[0].IsString()) {
            LOG_ERR("%s " RED("invalid mining.notify notification: invalid job id"), tag());
            return;
        }

        Job job;
        job.setId(arr[0].GetString());

        job.setAlgorithm(algo);
        job.setExtraNonce(m_extraNonce.second);

        std::stringstream s;

#       ifdef XMRIG_ALGO_XCB
        if (algo.id() == Algorithm::RX_XCB) {
            // Core Blockchain (XCB): this project's OWN invented wire shape (xcb-nodejs-pool's pool.js
            // controls both ends, no third-party compatibility needed) mirroring the node's real
            // cryptore_getWork() shape ([sealHash, seedHash(vestigial, not sent), target, blockNumber]):
            // [job_id, sealHashHex(64 chars), targetHex(64 chars), heightHex, clean_jobs]. No
            // coinbase/merkle/header packing at all -- the node builds the whole block, the pool/miner
            // only ever handle a 32 byte sealhash + an 8 byte nonce (see RX_XCB's nonceOffset()=32).
            if (!arr[1].IsString() || strlen(arr[1].GetString()) != 64) {
                LOG_ERR("%s " RED("invalid mining.notify notification: invalid sealhash"), tag());
                return;
            }

            // blob = sealhash(32 bytes) || nonce placeholder(8 bytes). extranonce1 (assigned per miner in
            // mining.subscribe's response, see xcb-nodejs-pool's pool.js onSubscribe()) is copied into
            // the START of that 8 byte field so different miners don't redundantly search the same nonce
            // range -- same anti-collision pattern as VERUSHASH's own notify branch above. The generic
            // nonce-splicing machinery (WorkerJob::nextRound) only varies the bits nonceMask() leaves free
            // (everything below extranonce1's own bits, via Job::nonceMask()'s extraNonce().size()*4
            // shift), never touching the extranonce1 bytes already written here.
            std::string blob = arr[1].GetString();
            blob.resize(80, '0'); // 40 bytes: 32 byte sealhash + 8 byte nonce field
            job.setBlob(blob.c_str());

            const size_t enSize = m_extraNonce.second.size() / 2;
            if (enSize > 0 && enSize <= 8) {
                Buffer en1 = Cvt::fromHex(m_extraNonce.second.data(), m_extraNonce.second.size());
                memcpy(job.blob() + 32, en1.data(), en1.size());
            }

            if (m_nextTarget) {
                job.setRawTarget(m_nextTarget);
                job.setDiff(Job::toDiff(m_nextTarget));
            }

            if (arr.Size() > 3 && arr[3].IsString()) {
                job.setHeight(strtoull(arr[3].GetString(), nullptr, 16));
            }

            m_job = std::move(job);

            bool ok = true;
            m_listener->onVerifyAlgorithm(this, algo, &ok);
            if (!ok) {
                if (!isQuiet()) {
                    LOG_ERR("[%s] incompatible/disabled algorithm \"%s\" detected, reconnect", url(), algo.name());
                }
                close();
                return;
            }

            if (!m_authorized) {
                m_authorized = true;
                m_listener->onLoginSuccess(this);
            }

            m_listener->onJobReceived(this, m_job, params);
            return;
        }
#       endif

#       ifdef XMRIG_ALGO_VERUSHASH
        if (algo.id() == Algorithm::VERUSHASH) {
            // VerusCoin's REAL wire protocol (confirmed from monkins1010/ccminer's equi/equi-stratum.cpp,
            // NOT classic Bitcoin stratum -- see vrsc-project.md): [job_id, version, prevhash, merkleroot,
            // finalSaplingRoot, ntime, nbits, clean_jobs, solution]. No coinb1/coinb2/branches -- the pool
            // already computed the whole merkle root server-side from its own fixed coinbase. Byte-order
            // convention (big endian hex text for version/ntime/nbits, every 4 byte word reversed for the
            // three hash fields, solution passed through as plain hex) is this pool's OWN choice, matching
            // vrsc-nodejs-pool's blockBuilder.js notifyParams() exactly -- there is no third-party wire
            // compatibility requirement to satisfy here, only agreement with this project's own pool.
            if (!arr[1].IsString() || !arr[2].IsString() || !arr[3].IsString() || !arr[4].IsString() ||
                !arr[5].IsString() || !arr[6].IsString() || !arr[8].IsString()) {
                LOG_ERR("%s " RED("invalid mining.notify notification: invalid param array"), tag());
                return;
            }

            if (strlen(arr[1].GetString()) != 8 || strlen(arr[2].GetString()) != 64 ||
                strlen(arr[3].GetString()) != 64 || strlen(arr[4].GetString()) != 64 ||
                strlen(arr[5].GetString()) != 8 || strlen(arr[6].GetString()) != 8) {
                LOG_ERR("%s " RED("invalid mining.notify notification: wrong field length"), tag());
                return;
            }

            if (!job.setVerusSolution(arr[8].GetString())) {
                LOG_ERR("%s " RED("invalid mining.notify notification: bad solution (expected 1344 bytes)"), tag());
                return;
            }

            // version (4) | prevhash (32) | merkleroot (32) | finalSaplingRoot (32), each hash field
            // word-reversed on the wire -- undo that reversal while writing, same convention every other
            // Bitcoin-stratum coin here already uses for its own hash fields.
            constexpr size_t kVerusHeaderSize = 140; // version(4)+prevhash(32)+merkleroot(32)+finalSaplingRoot(32)+ntime(4)+nbits(4)+nonce(32)
            Buffer blob(kVerusHeaderSize, 0);

            auto writeBigEndianHexAsLE32 = [](uint8_t *dst, const char *hex) {
                const uint32_t v = static_cast<uint32_t>(strtoul(hex, nullptr, 16));
                dst[0] = static_cast<uint8_t>(v);
                dst[1] = static_cast<uint8_t>(v >> 8);
                dst[2] = static_cast<uint8_t>(v >> 16);
                dst[3] = static_cast<uint8_t>(v >> 24);
            };
            auto writeWordReversedHash = [](uint8_t *dst, const char *hex) {
                Buffer buf = Cvt::fromHex(hex, 64);
                for (size_t i = 0; i < 32; i += 4) {
                    dst[i + 0] = buf[i + 3];
                    dst[i + 1] = buf[i + 2];
                    dst[i + 2] = buf[i + 1];
                    dst[i + 3] = buf[i + 0];
                }
            };

            writeBigEndianHexAsLE32(blob.data() + 0, arr[1].GetString());      // version
            writeWordReversedHash(blob.data() + 4, arr[2].GetString());        // hashPrevBlock
            writeWordReversedHash(blob.data() + 36, arr[3].GetString());       // hashMerkleRoot
            writeWordReversedHash(blob.data() + 68, arr[4].GetString());       // hashFinalSaplingRoot
            writeBigEndianHexAsLE32(blob.data() + 100, arr[5].GetString());    // nTime
            writeBigEndianHexAsLE32(blob.data() + 104, arr[6].GetString());    // nBits

            // nNonce (32 bytes, offset 108): extranonce1 at the start, the rest (this miner's own
            // extranonce2 -- EN2_SIZE bytes, matching m_extraNonce2Size) left zero here; CpuWorker fills
            // in its own per-thread search nonce within that zeroed region at hash time (see
            // CpuWorker.cpp's VERUSHASH case) -- extranonce1/extranonce2 have NO relationship to the
            // coinbase/merkle root under this protocol, unlike classic Bitcoin stratum.
            const size_t enSize = m_extraNonce.second.size() / 2;
            if (enSize > 0 && enSize <= 12) {
                Buffer en1 = Cvt::fromHex(m_extraNonce.second.data(), m_extraNonce.second.size());
                memcpy(blob.data() + 108, en1.data(), en1.size());
            }

            job.setBlob(Cvt::toHex(blob.data(), blob.size()).data());
            m_ntime = arr[5].GetString();
            if (m_nextTarget) {
                job.setRawTarget(m_nextTarget);
                job.setDiff(Job::toDiff(m_nextTarget));
            }
            m_job = std::move(job);

            bool ok = true;
            m_listener->onVerifyAlgorithm(this, algo, &ok);
            if (!ok) {
                if (!isQuiet()) {
                    LOG_ERR("[%s] incompatible/disabled algorithm \"%s\" detected, reconnect", url(), algo.name());
                }
                close();
                return;
            }

            if (!m_authorized) {
                m_authorized = true;
                m_listener->onLoginSuccess(this);
            }

            m_listener->onJobReceived(this, m_job, params);
            return;
        }
#       endif

#       ifdef XMRIG_ALGO_GHOSTRIDER
        if (Algorithm::isBitcoinStratum(algo.id())) {
            // Raptoreum uses Bitcoin's Stratum protocol
            // https://en.bitcoinwiki.org/wiki/Stratum_mining_protocol#mining.notify

            if (!arr[1].IsString() || !arr[2].IsString() || !arr[3].IsString() || !arr[4].IsArray() || !arr[5].IsString() || !arr[6].IsString() || !arr[7].IsString()) {
                LOG_ERR("%s " RED("invalid mining.notify notification: invalid param array"), tag());
                return;
            }

            // Version
            s << arr[5].GetString();

            // Previous block hash
            s << arr[1].GetString();

            // Merkle tree root
            std::string blob = arr[2].GetString();
            blob += m_extraNonce.second;
            blob.append(m_extraNonce2Size * 2, '0');
            blob += arr[3].GetString();

            uint8_t merkle_root[64];

            Buffer buf = Cvt::fromHex(blob.c_str(), blob.length());

            // Get height from coinbase
            {
                uint8_t* p = buf.data() + 32;
                uint8_t* m = p + 128;

                while ((p < m) && (*p != 0xff)) ++p;
                while ((p < m) && (*p == 0xff)) ++p;

                if ((p < m) && (*(p - 1) == 0xff) && (*(p - 2) == 0xff)) {
                    uint32_t height = *reinterpret_cast<uint16_t*>(p + 2);
                    switch (*(p + 1)) {
                    case 4:
                        height += *reinterpret_cast<uint16_t*>(p + 4) * 0x10000UL;
                        break;
                    case 3:
                        height += *(p + 4) * 0x10000UL;
                        break;
                    }
                    job.setHeight(height);
                }
                else {
                    job.setHeight(0);
                }
            }

            // the coinbase's own leaf hash: sha3d for Kylacoin and Lyncoin (see sha3d_flex_coinbase() comment
            // above), sha256d (Bitcoin-standard) for every other Bitcoin-stratum coin.
            if (algo.id() == Algorithm::FLEX_KCN || algo.id() == Algorithm::FLEX_LCN) {
                if (!sha3d_flex_coinbase(merkle_root, buf.data(), buf.size())) {
                    LOG_ERR("%s " RED("sha3 coinbase hash failed"), tag());
                    return;
                }
            }
            else {
                sha256d(merkle_root, buf.data(), static_cast<int>(buf.size()));
            }

            auto merkle_branches = arr[4].GetArray();
            for (int i = 0, n = merkle_branches.Size(); i < n; ++i) {
                auto& b = merkle_branches[i];
                buf = b.IsString() ? Cvt::fromHex(b.GetString(), b.GetStringLength()) : Buffer();
                if (buf.size() != 32) {
                    LOG_ERR("%s " RED("invalid mining.notify notification: param 4 is invalid"), tag());
                    return;
                }
                memcpy(merkle_root + 32, buf.data(), 32);
                sha256d(merkle_root, merkle_root, 64);
            }

            s << Cvt::toHex(merkle_root, 32);

            // ntime
            m_ntime = arr[7].GetString();
            s << m_ntime;

            // nbits
            s << arr[6].GetString();

            blob = s.str();

            if (blob.size() != 76 * 2) {
                LOG_ERR("%s " RED("invalid mining.notify notification: invalid blob size"), tag());
                return;
            }

            // zeros up to 80 bytes
            blob.resize(80 * 2, '0');

            // Invert byte order (no idea why, but it's done in Bitcoin's Stratum)
            buf = Cvt::fromHex(blob.c_str(), blob.length());
            for (size_t i = 0; i < 80; i += sizeof(uint32_t)) {
                uint32_t& k = *reinterpret_cast<uint32_t*>(buf.data() + i);
                if ((i < 36) || (i >= 68)) {
                    k = ethash_swap_u32(k);
                }
            }
            blob = Cvt::toHex(buf.data(), buf.size());

            job.setBlob(blob.c_str());
            job.setDiff(m_nextDifficulty);
        }
        else
#       endif
        {
            // header hash (32 bytes)
            s << arr[1].GetString();

            // nonce template (8 bytes)
            for (uint64_t i = 0, k = m_extraNonce.first; i < sizeof(m_extraNonce.first); ++i, k >>= 8) {
                s << std::hex << std::setw(2) << std::setfill('0') << (k & 0xFF);
            }

            std::string blob = s.str();

            // zeros up to 76 bytes
            blob.resize(76 * 2, '0');
            job.setBlob(blob.c_str());

            std::string target_str = arr[3].GetString();
            target_str.resize(16, '0');
            const uint64_t target = strtoull(target_str.c_str(), nullptr, 16);
            job.setDiff(Job::toDiff(target));

            job.setHeight(arr[5].GetUint64());
        }

        bool ok = true;
        m_listener->onVerifyAlgorithm(this, algo, &ok);

        if (!ok) {
            if (!isQuiet()) {
                LOG_ERR("[%s] incompatible/disabled algorithm \"%s\" detected, reconnect", url(), algo.name());
            }
            close();
            return;
        }

        if (m_job != job) {
            m_job = std::move(job);

            // Workaround for nanopool.org, mining.notify received before mining.authorize response.
            if (!m_authorized) {
                m_authorized = true;
                m_listener->onLoginSuccess(this);
            }

            m_listener->onJobReceived(this, m_job, params);
        }
        else {
            if (!isQuiet()) {
                LOG_WARN("%s " YELLOW("duplicate job received, reconnect"), tag());
            }
            close();
        }
    }
}


void xmrig::EthStratumClient::setExtraNonce(const rapidjson::Value &nonce)
{
    if (!nonce.IsString()) {
        throw std::runtime_error("invalid mining.subscribe response: extra nonce is not a string");
    }

    const char *s = nonce.GetString();
    size_t len    = nonce.GetStringLength();

    // Skip "0x"
    if ((len >= 2) && (s[0] == '0') && (s[1] == 'x')) {
        s += 2;
        len -= 2;
    }

    if (len & 1) {
        throw std::runtime_error("invalid mining.subscribe response: extra nonce has an odd number of hex chars");
    }

    if (len > 8) {
        throw std::runtime_error("Invalid mining.subscribe response: extra nonce is too long");
    }

    std::string extra_nonce_str(s);
    extra_nonce_str.resize(16, '0');

    LOG_DEBUG("[%s] extra nonce set to %s", url(), s);

    m_extraNonce = { std::stoull(extra_nonce_str, nullptr, 16), s };
}


const char *xmrig::EthStratumClient::errorMessage(const rapidjson::Value &error)
{
    if (error.IsArray() && error.GetArray().Size() > 1) {
        auto &value = error.GetArray()[1];
        if (value.IsString()) {
            return value.GetString();
        }
    }

    if (error.IsString()) {
        return error.GetString();
    }

    if (error.IsObject()) {
        return Json::getString(error, "message");
    }

    return nullptr;
}


void xmrig::EthStratumClient::authorize()
{
    using namespace rapidjson;

    Document doc(kObjectType);
    auto &allocator = doc.GetAllocator();

    Value params(kArrayType);
    params.PushBack(m_user.toJSON(), allocator);
    params.PushBack(m_password.toJSON(), allocator);

    JsonRequest::create(doc, m_sequence, "mining.authorize", params);

    send(doc, [this](const rapidjson::Value& result, bool success, uint64_t elapsed) { onAuthorizeResponse(result, success, elapsed); });
}


void xmrig::EthStratumClient::onAuthorizeResponse(const rapidjson::Value &result, bool success, uint64_t)
{
    try {
        if (!success) {
            const auto message = errorMessage(result);
            if (message) {
                throw std::runtime_error(message);
            }

            throw std::runtime_error("mining.authorize call failed");
        }

        if (!result.IsBool()) {
            throw std::runtime_error("invalid mining.authorize response: result is not a boolean");
        }

        if (!result.GetBool()) {
            throw std::runtime_error("login failed");
        }
    } catch (const std::exception &ex) {
        LOG_ERR("%s " RED_BOLD("%s"), tag(), ex.what());

        close();
        return;
    }

    LOG_DEBUG("[%s] login succeeded", url());

    if (!m_authorized) {
        m_authorized = true;
        m_listener->onLoginSuccess(this);
    }
}


void xmrig::EthStratumClient::onSubscribeResponse(const rapidjson::Value &result, bool success, uint64_t)
{
    if (!success) {
        return;
    }

    try {
        if (!result.IsArray()) {
            throw std::runtime_error("invalid mining.subscribe response: result is not an array");
        }

        auto arr = result.GetArray();

        if (arr.Size() <= 1) {
            throw std::runtime_error("invalid mining.subscribe response: result array is too short");
        }

        setExtraNonce(arr[1]);

#       ifdef XMRIG_ALGO_GHOSTRIDER
        if ((arr.Size() > 2) && (arr[2].IsUint())) {
            m_extraNonce2Size = arr[2].GetUint();
        }
#       endif

        if (m_pool.isNicehash()) {
            using namespace rapidjson;
            Document doc(kObjectType);
            Value params(kArrayType);
            JsonRequest::create(doc, m_sequence, "mining.extranonce.subscribe", params);
            send(doc);
        }
    } catch (const std::exception &ex) {
        LOG_ERR("%s " RED("%s"), tag(), ex.what());

        m_extraNonce = { 0, {} };
    }
}


void xmrig::EthStratumClient::subscribe()
{
    using namespace rapidjson;

    Document doc(kObjectType);
    auto &allocator = doc.GetAllocator();

    Value params(kArrayType);
    params.PushBack(StringRef(agent()), allocator);

    JsonRequest::create(doc, m_sequence, "mining.subscribe", params);

    send(doc, [this](const rapidjson::Value& result, bool success, uint64_t elapsed) { onSubscribeResponse(result, success, elapsed); });
}
