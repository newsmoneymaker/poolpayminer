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

#include <cassert>
#include <thread>
#include <mutex>


#include "backend/cpu/Cpu.h"
#include "backend/cpu/CpuWorker.h"
#include "base/tools/Alignment.h"
#include "base/tools/Chrono.h"
#include "core/config/Config.h"
#include "core/Miner.h"
#include "crypto/cn/CnCtx.h"
#include "crypto/cn/CryptoNight_test.h"
#include "crypto/cn/CryptoNight.h"
#include "crypto/common/Nonce.h"
#include "crypto/common/Sha256d.h"
#include "crypto/common/VirtualMemory.h"
#include "crypto/rx/Rx.h"
#include "crypto/rx/RxCache.h"
#include "crypto/rx/RxDataset.h"
#include "crypto/rx/RxVm.h"
#include "crypto/ghostrider/ghostrider.h"

#ifdef XMRIG_ALGO_YESPOWER
extern "C" {
#   include "crypto/yespower/yespower.h"
}
#endif

#ifdef XMRIG_ALGO_YESCRYPT
extern "C" {
#   include "crypto/yescrypt/yescrypt.h"
}
#endif
#include "net/JobResults.h"


#ifdef XMRIG_ALGO_RANDOMX
#   include "crypto/randomx/randomx.h"
#endif


#ifdef XMRIG_FEATURE_BENCHMARK
#   include "backend/common/benchmark/BenchState.h"
#endif


namespace xmrig {

static constexpr uint32_t kReserveCount = 32768;

#ifdef XMRIG_ALGO_YESPOWER
// Yenten: yespower 1.0, N=4096, r=16, no personalization, over the 80-byte block header
static void yespowerHash(const uint8_t *input, size_t size, uint8_t *output)
{
    static const yespower_params_t params = { YESPOWER_1_0, 4096, 16, nullptr, 0 };

    if (yespower_tls(input, size, &params, reinterpret_cast<yespower_binary_t *>(output)) != 0) {
        memset(output, 0xFF, 32);
    }
}


static bool yespowerSelfTest()
{
    // header of a real Yenten block (height 2287592) and its yespower 1.0 hash (equal to the pool's helper and to the node)
    static const uint8_t input[80] = {
        0x00, 0x00, 0x00, 0x20, 0x87, 0x8c, 0x36, 0xc3, 0x05, 0x3c, 0xe0, 0x42, 0x37, 0x4f, 0xe9, 0x38,
        0x19, 0x5f, 0xdc, 0x9b, 0x69, 0xca, 0x7f, 0xbe, 0x40, 0x31, 0xac, 0x6b, 0x2c, 0xb3, 0x34, 0x5f,
        0x0b, 0xc7, 0xa9, 0x10, 0x73, 0x66, 0xc1, 0x87, 0x84, 0x2b, 0xf7, 0x41, 0x77, 0x5c, 0xf1, 0xa7,
        0xc5, 0x8f, 0xde, 0x45, 0x5c, 0x3b, 0xbb, 0x36, 0xe2, 0xd4, 0x17, 0xbe, 0x49, 0x7b, 0x40, 0xa8,
        0x21, 0x50, 0xf6, 0x20, 0x5d, 0x8b, 0xb5, 0x6a, 0x95, 0x33, 0x74, 0x1d, 0x56, 0x05, 0x00, 0x20
    };
    static const uint8_t expected[32] = {
        0x37, 0xca, 0x64, 0x26, 0x26, 0xba, 0xe9, 0x87, 0x50, 0x98, 0x6e, 0x1c, 0x37, 0x65, 0x82, 0x77,
        0xdc, 0x76, 0xf4, 0x25, 0x05, 0x84, 0x4d, 0xa2, 0x40, 0x13, 0x85, 0x59, 0x35, 0x00, 0x00, 0x00
    };

    uint8_t output[32];
    yespowerHash(input, sizeof(input), output);

    return memcmp(output, expected, sizeof(output)) == 0;
}
#endif


#ifdef XMRIG_ALGO_YESCRYPT
// MateableCoin (MTBC): yescryptr8, yescrypt (pwxform), N=2048, r=8, p=1, self-salted, over the
// 80-byte block header (see src/crypto/yescrypt/yescrypt.c, yescrypt_hash_r8()).
static void yescryptR8Hash(const uint8_t *input, size_t size, uint8_t *output)
{
    yescrypt_hash_r8(reinterpret_cast<const char *>(input), size, reinterpret_cast<char *>(output));
}


// Fennec (FNNC) / Gold Cash (GOLD): yescryptr16, yescrypt (pwxform), N=4096, r=16, p=1, self-salted
// (see src/crypto/yescrypt/yescrypt.c, yescrypt_hash_r16()).
static void yescryptR16Hash(const uint8_t *input, size_t size, uint8_t *output)
{
    yescrypt_hash_r16(reinterpret_cast<const char *>(input), size, reinterpret_cast<char *>(output));
}


// LuckyPepe (LPEPE), originally WAVI: yescryptr32, yescrypt (pwxform), N=4096, r=32, p=1,
// self-salted, personalization "WaviBanana" (see src/crypto/yescrypt/yescrypt-r32.c).
static void yescryptR32Hash(const uint8_t *input, size_t size, uint8_t *output)
{
    yescrypt_hash_r32(input, size, output);
}


// Each self-test below now checks a real mainnet block header of that coin (found once each pool
// was up and its own hasher had been checked against several real blocks): MTBC height 4,613,037,
// FNNC height 100, LPEPE height 1,000. The expected digest is that pool's own independently-built
// hasher output on the same header, confirmed to match this integration byte for byte.
static bool yescryptR8SelfTest()
{
    static const uint8_t input[80] = {
        0x00, 0x02, 0x00, 0x20, 0x4d, 0x8a, 0x50, 0x3c, 0xce, 0x7c, 0x13, 0xd4, 0x52, 0x43, 0xe0, 0x06,
        0xd8, 0xa1, 0x14, 0x36, 0x2e, 0xd9, 0xf7, 0xc4, 0x10, 0x42, 0x70, 0x1d, 0xfe, 0x89, 0x0b, 0x62,
        0xd8, 0x31, 0xda, 0x9c, 0x42, 0xce, 0xc0, 0xfa, 0xe0, 0xd1, 0x55, 0x0f, 0xe2, 0xa4, 0xe1, 0xe2,
        0x18, 0xf1, 0x06, 0xac, 0x6c, 0x8c, 0xfc, 0x73, 0xab, 0x68, 0x53, 0x5e, 0x69, 0xcf, 0xf6, 0x0f,
        0x7d, 0x60, 0x82, 0x87, 0x3d, 0xb9, 0xb6, 0x6a, 0x11, 0x9e, 0x07, 0x1e, 0x80, 0x00, 0x0d, 0x2c,
    };

    static const uint8_t expected[32] = {
        0xb1, 0x51, 0xee, 0x7c, 0xc5, 0xbc, 0x53, 0xf3, 0x3b, 0x63, 0x62, 0xe0, 0xff, 0x48, 0x8d, 0x53,
        0xe1, 0xc9, 0xbd, 0x41, 0x16, 0x58, 0x00, 0x1f, 0x38, 0x5f, 0x7b, 0x5b, 0x17, 0x07, 0x00, 0x00,
    };

    uint8_t output[32];
    yescryptR8Hash(input, sizeof(input), output);

    return memcmp(output, expected, sizeof(output)) == 0;
}


static bool yescryptR16SelfTest()
{
    static const uint8_t input[80] = {
        0x00, 0x00, 0x00, 0x20, 0x51, 0xa1, 0x8f, 0x8d, 0x11, 0x72, 0x0f, 0x43, 0x8a, 0x0d, 0x47, 0x7b,
        0x79, 0x2a, 0xa3, 0xc0, 0xd1, 0x71, 0xa1, 0xce, 0x94, 0x68, 0x0f, 0xe8, 0x09, 0x8d, 0x2c, 0xbd,
        0x0d, 0xf4, 0xfc, 0x7a, 0xc0, 0x49, 0xd5, 0x8c, 0x78, 0x75, 0x3f, 0x9f, 0x85, 0x55, 0x30, 0x0c,
        0x62, 0xf7, 0xea, 0xbe, 0x3e, 0xc9, 0x3b, 0x41, 0xb8, 0x9b, 0xbe, 0x5e, 0x88, 0xc8, 0x4b, 0x8b,
        0xf8, 0x1c, 0x7a, 0x78, 0x4c, 0xa0, 0x7b, 0x63, 0x22, 0x73, 0x01, 0x1f, 0x18, 0x3d, 0x00, 0x00,
    };

    static const uint8_t expected[32] = {
        0xbf, 0x75, 0xcb, 0xc1, 0x32, 0xb7, 0x23, 0xeb, 0xcc, 0xf4, 0x9f, 0x00, 0x3b, 0x15, 0xc6, 0x8d,
        0x8a, 0xc5, 0xf3, 0xe8, 0x75, 0x57, 0x76, 0xe9, 0x19, 0xe1, 0xd2, 0x08, 0x1d, 0xec, 0x00, 0x00,
    };

    uint8_t output[32];
    yescryptR16Hash(input, sizeof(input), output);

    return memcmp(output, expected, sizeof(output)) == 0;
}


static bool yescryptR32SelfTest()
{
    static const uint8_t input[80] = {
        0x00, 0x00, 0x00, 0x20, 0x8c, 0xef, 0x70, 0xbd, 0x67, 0x08, 0x38, 0x64, 0xea, 0x16, 0xb8, 0x69,
        0x1a, 0xb0, 0x41, 0xec, 0xd8, 0xa4, 0x43, 0x9a, 0xda, 0x63, 0xf9, 0x1b, 0x85, 0x59, 0x35, 0x9b,
        0xa7, 0x2a, 0xdf, 0xa2, 0x6b, 0x45, 0x27, 0x9d, 0xb7, 0xac, 0xa2, 0xdd, 0x70, 0x0d, 0x04, 0x3b,
        0xe6, 0xc1, 0x4e, 0x17, 0x76, 0x87, 0xfb, 0x1e, 0x39, 0xdb, 0x2b, 0x2d, 0x3d, 0x01, 0xf9, 0xeb,
        0x73, 0xed, 0x0a, 0xbf, 0xef, 0x86, 0x9b, 0x69, 0xff, 0xff, 0x00, 0x20, 0x99, 0x00, 0x00, 0x00,
    };

    static const uint8_t expected[32] = {
        0x44, 0x74, 0xa9, 0x62, 0x71, 0x8a, 0x93, 0xc4, 0xc7, 0xad, 0x74, 0x85, 0xf3, 0xf2, 0xa9, 0xb7,
        0xfc, 0xa8, 0x14, 0xc1, 0x35, 0xa6, 0x5a, 0x14, 0x62, 0x93, 0xb0, 0x35, 0x56, 0x72, 0xfe, 0x00,
    };

    uint8_t output[32];
    yescryptR32Hash(input, sizeof(input), output);

    return memcmp(output, expected, sizeof(output)) == 0;
}
#endif



#ifdef XMRIG_ALGO_CN_HEAVY
static std::mutex cn_heavyZen3MemoryMutex;
VirtualMemory* cn_heavyZen3Memory = nullptr;
#endif

} // namespace xmrig



template<size_t N>
xmrig::CpuWorker<N>::CpuWorker(size_t id, const CpuLaunchData &data) :
    Worker(id, data.affinity, data.priority),
    m_algorithm(data.algorithm),
    m_assembly(data.assembly),
    m_hwAES(data.hwAES),
    m_yield(data.yield),
    m_av(data.av()),
    m_miner(data.miner),
    m_threads(data.threads),
    m_ctx()
{
#   ifdef XMRIG_ALGO_CN_HEAVY
    // cn-heavy optimization for Zen3 CPUs
    const auto arch = Cpu::info()->arch();
    const uint32_t model = Cpu::info()->model();
    const bool is_vermeer = (arch == ICpuInfo::ARCH_ZEN3) && (model == 0x21);
    const bool is_raphael = (arch == ICpuInfo::ARCH_ZEN4) && (model == 0x61);
    if ((N == 1) && (m_av == CnHash::AV_SINGLE) && (m_algorithm.family() == Algorithm::CN_HEAVY) && (m_assembly != Assembly::NONE) && (is_vermeer || is_raphael)) {
        std::lock_guard<std::mutex> lock(cn_heavyZen3MemoryMutex);
        if (!cn_heavyZen3Memory) {
            // Round up number of threads to the multiple of 8
            const size_t num_threads = ((m_threads + 7) / 8) * 8;
            cn_heavyZen3Memory = new VirtualMemory(m_algorithm.l3() * num_threads, data.hugePages, false, false, node(), VirtualMemory::kDefaultHugePageSize);
        }
        m_memory = cn_heavyZen3Memory;
    }
    else
#   endif
    {
        m_memory = new VirtualMemory(m_algorithm.l3() * N, data.hugePages, false, true, node(), VirtualMemory::kDefaultHugePageSize);
    }

#   ifdef XMRIG_ALGO_GHOSTRIDER
    m_ghHelper = ghostrider::create_helper_thread(affinity(), data.priority, data.affinities);
#   endif
}


template<size_t N>
xmrig::CpuWorker<N>::~CpuWorker()
{
#   ifdef XMRIG_ALGO_RANDOMX
    RxVm::destroy(m_vm);
#   endif

    CnCtx::release(m_ctx, N);

#   ifdef XMRIG_ALGO_CN_HEAVY
    if (m_memory != cn_heavyZen3Memory)
#   endif
    {
        delete m_memory;
    }

#   ifdef XMRIG_ALGO_GHOSTRIDER
    ghostrider::destroy_helper_thread(m_ghHelper);
#   endif
}


#ifdef XMRIG_ALGO_RANDOMX
template<size_t N>
void xmrig::CpuWorker<N>::allocateRandomX_VM()
{
    RxDataset *dataset = Rx::dataset(m_job.currentJob(), node());

    while (dataset == nullptr) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        if (Nonce::sequence(Nonce::CPU) == 0) {
            return;
        }

        dataset = Rx::dataset(m_job.currentJob(), node());
    }

    if (!m_vm) {
        // Try to allocate scratchpad from dataset's 1 GB huge pages, if normal huge pages are not available
        uint8_t* scratchpad = m_memory->isHugePages() ? m_memory->scratchpad() : dataset->tryAllocateScrathpad();
        m_vm = RxVm::create(dataset, scratchpad ? scratchpad : m_memory->scratchpad(), !m_hwAES, m_assembly, node());
    }
    else if (!dataset->get() && (m_job.currentJob().seed() != m_seed)) {
        // Update RandomX light VM with the new seed
        randomx_vm_set_cache(m_vm, dataset->cache()->get());
    }
    m_seed = m_job.currentJob().seed();
}
#endif


template<size_t N>
bool xmrig::CpuWorker<N>::selfTest()
{
#   ifdef XMRIG_ALGO_RANDOMX
    if (m_algorithm.family() == Algorithm::RANDOM_X) {
        return N == 1;
    }
#   endif

    allocateCnCtx();

#   ifdef XMRIG_ALGO_YESPOWER
    if (m_algorithm.family() == Algorithm::YESPOWER) {
        return (N == 1) && yespowerSelfTest();
    }
#   endif

#   ifdef XMRIG_ALGO_YESCRYPT
    if (m_algorithm.family() == Algorithm::YESCRYPT) {
        if (N != 1) {
            return false;
        }

        switch (m_algorithm.id()) {
        case Algorithm::YESCRYPT_R8:
            return yescryptR8SelfTest();
        case Algorithm::YESCRYPT_R16:
            return yescryptR16SelfTest();
        case Algorithm::YESCRYPT_R32:
            return yescryptR32SelfTest();
        default:
            return false;
        }
    }
#   endif

#   ifdef XMRIG_ALGO_GHOSTRIDER
    if (m_algorithm.family() == Algorithm::GHOSTRIDER) {
        return (N == 8) && verify(Algorithm::GHOSTRIDER_RTM, test_output_gr);
    }
#   endif

    if (m_algorithm.family() == Algorithm::CN) {
        const bool rc = verify(Algorithm::CN_0,      test_output_v0)   &&
                        verify(Algorithm::CN_1,      test_output_v1)   &&
                        verify(Algorithm::CN_2,      test_output_v2)   &&
                        verify(Algorithm::CN_FAST,   test_output_msr)  &&
                        verify(Algorithm::CN_XAO,    test_output_xao)  &&
                        verify(Algorithm::CN_RTO,    test_output_rto)  &&
                        verify(Algorithm::CN_HALF,   test_output_half) &&
                        verify2(Algorithm::CN_R,     test_output_r)    &&
                        verify(Algorithm::CN_RWZ,    test_output_rwz)  &&
                        verify(Algorithm::CN_ZLS,    test_output_zls)  &&
                        verify(Algorithm::CN_CCX,    test_output_ccx)  &&
                        verify(Algorithm::CN_DOUBLE, test_output_double);

        return rc;
    }

#   ifdef XMRIG_ALGO_CN_LITE
    if (m_algorithm.family() == Algorithm::CN_LITE) {
        return verify(Algorithm::CN_LITE_0,    test_output_v0_lite) &&
               verify(Algorithm::CN_LITE_1,    test_output_v1_lite);
    }
#   endif

#   ifdef XMRIG_ALGO_CN_HEAVY
    if (m_algorithm.family() == Algorithm::CN_HEAVY) {
        return verify(Algorithm::CN_HEAVY_0,    test_output_v0_heavy)  &&
               verify(Algorithm::CN_HEAVY_XHV,  test_output_xhv_heavy) &&
               verify(Algorithm::CN_HEAVY_TUBE, test_output_tube_heavy);
    }
#   endif

#   ifdef XMRIG_ALGO_CN_PICO
    if (m_algorithm.family() == Algorithm::CN_PICO) {
        return verify(Algorithm::CN_PICO_0, test_output_pico_trtl) &&
               verify(Algorithm::CN_PICO_TLO, test_output_pico_tlo);
    }
#   endif

#   ifdef XMRIG_ALGO_CN_FEMTO
    if (m_algorithm.family() == Algorithm::CN_FEMTO) {
        return verify(Algorithm::CN_UPX2, test_output_femto_upx2);
    }
#   endif

#   ifdef XMRIG_ALGO_ARGON2
    if (m_algorithm.family() == Algorithm::ARGON2) {
        return verify(Algorithm::AR2_CHUKWA, argon2_chukwa_test_out) &&
               verify(Algorithm::AR2_CHUKWA_V2, argon2_chukwa_v2_test_out) &&
               verify(Algorithm::AR2_WRKZ, argon2_wrkz_test_out);
    }
#   endif

    return false;
}


template<size_t N>
void xmrig::CpuWorker<N>::hashrateData(uint64_t &hashCount, uint64_t &, uint64_t &rawHashes) const
{
    hashCount = m_count;
    rawHashes = m_count;
}


template<size_t N>
void xmrig::CpuWorker<N>::start()
{
    while (Nonce::sequence(Nonce::CPU) > 0) {
        if (Nonce::isPaused()) {
            do {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            while (Nonce::isPaused() && Nonce::sequence(Nonce::CPU) > 0);

            if (Nonce::sequence(Nonce::CPU) == 0) {
                break;
            }

            consumeJob();
        }

#       ifdef XMRIG_ALGO_RANDOMX
        bool first = true;
        alignas(64) uint64_t tempHash[8] = {};

        size_t prev_job_size = 0;
        alignas(64) uint8_t prev_job[Job::kMaxBlobSize] = {};
#       endif

        while (!Nonce::isOutdated(Nonce::CPU, m_job.sequence())) {
            const Job &job = m_job.currentJob();

            if (job.algorithm().l3() != m_algorithm.l3()) {
                break;
            }

            uint32_t current_job_nonces[N];
            for (size_t i = 0; i < N; ++i) {
                current_job_nonces[i] = readUnaligned(m_job.nonce(i));
            }

#           ifdef XMRIG_FEATURE_BENCHMARK
            if (m_benchSize) {
                if (current_job_nonces[0] >= m_benchSize) {
                    return BenchState::done();
                }

                // Make each hash dependent on the previous one in single thread benchmark to prevent cheating with multiple threads
                if (m_threads == 1) {
                    *(uint64_t*)(m_job.blob()) ^= BenchState::data();
                }
            }
#           endif

            bool valid = true;

            uint8_t miner_signature_saved[64];

#           ifdef XMRIG_ALGO_RANDOMX
            uint8_t* miner_signature_ptr = m_job.blob() + m_job.nonceOffset() + m_job.nonceSize();
            if (job.algorithm().family() == Algorithm::RANDOM_X) {
                // Veil: RandomX is fed with the double SHA-256 of the whole 148 byte header (nonce included), not with the header
                const bool veil = job.isVeil();
                alignas(16) uint8_t veilInput[32];

                if (first) {
                    first = false;
                    if (job.hasMinerSignature()) {
                        job.generateMinerSignature(m_job.blob(), job.size(), miner_signature_ptr);
                    }

                    if (veil) {
                        Sha256d::hash(m_job.blob(), job.size(), veilInput);
                        randomx_calculate_hash_first(m_vm, tempHash, veilInput, sizeof(veilInput));
                    }
                    else {
                        randomx_calculate_hash_first(m_vm, tempHash, m_job.blob(), job.size());
                    }

                    if (RandomX_CurrentConfig.Tweak_V2_COMMITMENT) {
                        prev_job_size = job.size();
                        memcpy(prev_job, m_job.blob(), prev_job_size);
                    }
                }

                if (!nextRound()) {
                    break;
                }

                if (job.hasMinerSignature()) {
                    memcpy(miner_signature_saved, miner_signature_ptr, sizeof(miner_signature_saved));
                    job.generateMinerSignature(m_job.blob(), job.size(), miner_signature_ptr);
                }

                if (veil) {
                    Sha256d::hash(m_job.blob(), job.size(), veilInput);
                    randomx_calculate_hash_next(m_vm, tempHash, veilInput, sizeof(veilInput), m_hash);
                }
                else {
                    randomx_calculate_hash_next(m_vm, tempHash, m_job.blob(), job.size(), m_hash);
                }

                if (RandomX_CurrentConfig.Tweak_V2_COMMITMENT) {
                    memcpy(m_commitment, m_hash, RANDOMX_HASH_SIZE);
                    randomx_calculate_commitment(prev_job, prev_job_size, m_hash, m_hash);
                    prev_job_size = job.size();
                    memcpy(prev_job, m_job.blob(), prev_job_size);
                }
            }
            else
#           endif
            {
                switch (job.algorithm().family()) {

#               ifdef XMRIG_ALGO_YESPOWER
                case Algorithm::YESPOWER:
                    if (N == 1) {
                        yespowerHash(m_job.blob(), job.size(), m_hash);
                    }
                    else {
                        valid = false;
                    }
                    break;
#               endif

#               ifdef XMRIG_ALGO_YESCRYPT
                case Algorithm::YESCRYPT:
                    if (N == 1) {
                        switch (job.algorithm().id()) {
                        case Algorithm::YESCRYPT_R8:
                            yescryptR8Hash(m_job.blob(), job.size(), m_hash);
                            break;
                        case Algorithm::YESCRYPT_R16:
                            yescryptR16Hash(m_job.blob(), job.size(), m_hash);
                            break;
                        case Algorithm::YESCRYPT_R32:
                            yescryptR32Hash(m_job.blob(), job.size(), m_hash);
                            break;
                        default:
                            valid = false;
                            break;
                        }
                    }
                    else {
                        valid = false;
                    }
                    break;
#               endif

#               ifdef XMRIG_ALGO_GHOSTRIDER
                case Algorithm::GHOSTRIDER:
                    if (N == 8) {
                        ghostrider::hash_octa(m_job.blob(), job.size(), m_hash, m_ctx, m_ghHelper);
                    }
                    else {
                        valid = false;
                    }
                    break;
#               endif

                default:
                    fn(job.algorithm())(m_job.blob(), job.size(), m_hash, m_ctx, job.height());
                    break;
                }

                if (!nextRound()) {
                    break;
                };
            }

            if (valid) {
                for (size_t i = 0; i < N; ++i) {
                    const uint64_t value = job.bigEndianValue() ? Job::epicValue(m_hash + (i * 32)) : *reinterpret_cast<uint64_t*>(m_hash + (i * 32) + 24);

#                   ifdef XMRIG_FEATURE_BENCHMARK
                    if (m_benchSize) {
                        if (current_job_nonces[i] < m_benchSize) {
                            BenchState::add(value);
                        }
                    }
                    else
#                   endif

                    if (value < job.target()) {
                        uint8_t* extra_data = nullptr;

                        if (job.algorithm().family() == Algorithm::RANDOM_X) {
                            if (RandomX_CurrentConfig.Tweak_V2_COMMITMENT) {
                                extra_data = m_commitment;
                            }
                            else if (job.hasMinerSignature()) {
                                extra_data = miner_signature_saved;
                            }
                        }

                        JobResults::submit(job, current_job_nonces[i], m_hash + (i * 32), extra_data);
                    }
                }
                m_count += N;
            }

            if (m_yield) {
                std::this_thread::yield();
            }
        }

        if (!Nonce::isPaused()) {
            consumeJob();
        }
    }
}


template<size_t N>
bool xmrig::CpuWorker<N>::nextRound()
{
#   ifdef XMRIG_FEATURE_BENCHMARK
    const uint32_t count = m_benchSize ? 1U : kReserveCount;
#   else
    constexpr uint32_t count = kReserveCount;
#   endif

    if (!m_job.nextRound(count, 1)) {
        JobResults::done(m_job.currentJob());

        return false;
    }

    return true;
}


template<size_t N>
bool xmrig::CpuWorker<N>::verify(const Algorithm &algorithm, const uint8_t *referenceValue)
{
#   ifdef XMRIG_ALGO_GHOSTRIDER
    if (algorithm == Algorithm::GHOSTRIDER_RTM) {
        uint8_t blob[N * 80] = {};
        for (size_t i = 0; i < N; ++i) {
            blob[i * 80 + 0] = static_cast<uint8_t>(i);
            blob[i * 80 + 4] = 0x10;
            blob[i * 80 + 5] = 0x02;
        }

        uint8_t hash1[N * 32] = {};
        ghostrider::hash_octa(blob, 80, hash1, m_ctx, 0, false);

        for (size_t i = 0; i < N; ++i) {
            blob[i * 80 + 0] = static_cast<uint8_t>(i);
            blob[i * 80 + 4] = 0x43;
            blob[i * 80 + 5] = 0x05;
        }

        uint8_t hash2[N * 32] = {};
        ghostrider::hash_octa(blob, 80, hash2, m_ctx, 0, false);

        for (size_t i = 0; i < N * 32; ++i) {
            if ((hash1[i] ^ hash2[i]) != referenceValue[i]) {
                return false;
            }
        }

        return true;
    }
#   endif

    cn_hash_fun func = fn(algorithm);
    if (!func) {
        return false;
    }

    func(test_input, 76, m_hash, m_ctx, 0);
    return memcmp(m_hash, referenceValue, sizeof m_hash) == 0;
}


template<size_t N>
bool xmrig::CpuWorker<N>::verify2(const Algorithm &algorithm, const uint8_t *referenceValue)
{
    cn_hash_fun func = fn(algorithm);
    if (!func) {
        return false;
    }

    for (size_t i = 0; i < (sizeof(cn_r_test_input) / sizeof(cn_r_test_input[0])); ++i) {
        const size_t size = cn_r_test_input[i].size;
        for (size_t k = 0; k < N; ++k) {
            memcpy(m_job.blob() + (k * size), cn_r_test_input[i].data, size);
        }

        func(m_job.blob(), size, m_hash, m_ctx, cn_r_test_input[i].height);

        for (size_t k = 0; k < N; ++k) {
            if (memcmp(m_hash + k * 32, referenceValue + i * 32, sizeof m_hash / N) != 0) {
                return false;
            }
        }
    }

    return true;
}


namespace xmrig {

template<>
bool CpuWorker<1>::verify2(const Algorithm &algorithm, const uint8_t *referenceValue)
{
    cn_hash_fun func = fn(algorithm);
    if (!func) {
        return false;
    }

    for (size_t i = 0; i < (sizeof(cn_r_test_input) / sizeof(cn_r_test_input[0])); ++i) {
        func(cn_r_test_input[i].data, cn_r_test_input[i].size, m_hash, m_ctx, cn_r_test_input[i].height);

        if (memcmp(m_hash, referenceValue + i * 32, sizeof m_hash) != 0) {
            return false;
        }
    }

    return true;
}

} // namespace xmrig


template<size_t N>
void xmrig::CpuWorker<N>::allocateCnCtx()
{
    if (m_ctx[0] == nullptr) {
        int shift = 0;

#       ifdef XMRIG_ALGO_CN_HEAVY
        // cn-heavy optimization for Zen3 CPUs
        if (m_memory == cn_heavyZen3Memory) {
            shift = (id() / 8) * m_algorithm.l3() * 8 + (id() % 8) * 64;
        }
#       endif

        CnCtx::create(m_ctx, m_memory->scratchpad() + shift, m_algorithm.l3(), N);
    }
}


template<size_t N>
void xmrig::CpuWorker<N>::consumeJob()
{
    if (Nonce::sequence(Nonce::CPU) == 0) {
        return;
    }

    auto job = m_miner->job();

#   ifdef XMRIG_FEATURE_BENCHMARK
    m_benchSize          = job.benchSize();
    const uint32_t count = m_benchSize ? 1U : kReserveCount;
#   else
    constexpr uint32_t count = kReserveCount;
#   endif

    m_job.add(job, count, Nonce::CPU);

#   ifdef XMRIG_ALGO_RANDOMX
    if (m_job.currentJob().algorithm().family() == Algorithm::RANDOM_X) {
        allocateRandomX_VM();
    }
    else
#   endif
    {
        allocateCnCtx();
    }
}


namespace xmrig {

template class CpuWorker<1>;
template class CpuWorker<2>;
template class CpuWorker<3>;
template class CpuWorker<4>;
template class CpuWorker<5>;
template class CpuWorker<8>;

} // namespace xmrig

