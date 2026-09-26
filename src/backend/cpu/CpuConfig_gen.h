/* XMRig
 * Copyright (c) 2018-2024 SChernykh   <https://github.com/SChernykh>
 * Copyright (c) 2016-2024 XMRig       <https://github.com/xmrig>, <support@xmrig.com>
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

#ifndef XMRIG_CPUCONFIG_GEN_H
#define XMRIG_CPUCONFIG_GEN_H


#include "backend/common/Threads.h"
#include "backend/cpu/Cpu.h"
#include "backend/cpu/CpuThreads.h"


namespace xmrig {


static inline size_t generate(const char *key, Threads<CpuThreads> &threads, const Algorithm &algorithm, uint32_t limit)
{
    if (threads.isExist(algorithm) || threads.has(key)) {
        return 0;
    }

    return threads.move(key, Cpu::info()->threads(algorithm, limit));
}


template<Algorithm::Family FAMILY>
static inline size_t generate(Threads<CpuThreads> &, uint32_t) { return 0; }


template<>
size_t inline generate<Algorithm::CN>(Threads<CpuThreads> &threads, uint32_t limit)
{
    size_t count = 0;

    count += generate(Algorithm::kCN, threads, Algorithm::CN_1, limit);

    if (!threads.isExist(Algorithm::CN_0)) {
        threads.disable(Algorithm::CN_0);
        ++count;
    }

    return count;
}


#ifdef XMRIG_ALGO_CN_LITE
template<>
size_t inline generate<Algorithm::CN_LITE>(Threads<CpuThreads> &threads, uint32_t limit)
{
    size_t count = 0;

    count += generate(Algorithm::kCN_LITE, threads, Algorithm::CN_LITE_1, limit);

    if (!threads.isExist(Algorithm::CN_LITE_0)) {
        threads.disable(Algorithm::CN_LITE_0);
        ++count;
    }

    return count;
}
#endif


#ifdef XMRIG_ALGO_CN_HEAVY
template<>
size_t inline generate<Algorithm::CN_HEAVY>(Threads<CpuThreads> &threads, uint32_t limit)
{
    return generate(Algorithm::kCN_HEAVY, threads, Algorithm::CN_HEAVY_0, limit);
}
#endif


#ifdef XMRIG_ALGO_CN_PICO
template<>
size_t inline generate<Algorithm::CN_PICO>(Threads<CpuThreads> &threads, uint32_t limit)
{
    return generate(Algorithm::kCN_PICO, threads, Algorithm::CN_PICO_0, limit);
}
#endif


#ifdef XMRIG_ALGO_CN_FEMTO
template<>
size_t inline generate<Algorithm::CN_FEMTO>(Threads<CpuThreads>& threads, uint32_t limit)
{
    return generate(Algorithm::kCN_UPX2, threads, Algorithm::CN_UPX2, limit);
}
#endif


#ifdef XMRIG_ALGO_RANDOMX
template<>
size_t inline generate<Algorithm::RANDOM_X>(Threads<CpuThreads> &threads, uint32_t limit)
{
    size_t count = 0;
    auto cpuInfo = Cpu::info();
    auto wow     = cpuInfo->threads(Algorithm::RX_WOW, limit);

    if (!threads.isExist(Algorithm::RX_ARQ)) {
        auto arq = cpuInfo->threads(Algorithm::RX_ARQ, limit);
        if (arq == wow) {
            threads.setAlias(Algorithm::RX_ARQ, Algorithm::kRX_WOW);
            ++count;
        }
        else {
            count += threads.move(Algorithm::kRX_ARQ, std::move(arq));
        }
    }

    if (!threads.isExist(Algorithm::RX_WOW)) {
        count += threads.move(Algorithm::kRX_WOW, std::move(wow));
    }

    count += generate(Algorithm::kRX, threads, Algorithm::RX_0, limit);

    return count;
}
#endif


#ifdef XMRIG_ALGO_ARGON2
template<>
size_t inline generate<Algorithm::ARGON2>(Threads<CpuThreads> &threads, uint32_t limit)
{
    return generate(Algorithm::kAR2, threads, Algorithm::AR2_CHUKWA_V2, limit);
}
#endif


#ifdef XMRIG_ALGO_GHOSTRIDER
template<>
size_t inline generate<Algorithm::GHOSTRIDER>(Threads<CpuThreads>& threads, uint32_t limit)
{
    return generate(Algorithm::kGHOSTRIDER, threads, Algorithm::GHOSTRIDER_RTM, limit);
}
#endif


#ifdef XMRIG_ALGO_YESPOWER
template<>
size_t inline generate<Algorithm::YESPOWER>(Threads<CpuThreads>& threads, uint32_t limit)
{
    return generate(Algorithm::kYESPOWER, threads, Algorithm::YESPOWER_R16, limit);
}
#endif


#ifdef XMRIG_ALGO_YESCRYPT
template<>
size_t inline generate<Algorithm::YESCRYPT>(Threads<CpuThreads>& threads, uint32_t limit)
{
    // Unlike yespower-r16 (the family name IS the one member's exact name, so the lookup in Threads::profileName() finds it
    // directly) yescrypt has three real, differently sized variants and none of their names contain "/" (the split that lets
    // RandomX's "rx/..." ids share a generic "rx" profile) -- registering one shared profile under the family name "yescrypt"
    // left profileName("yescryptr8"/"yescryptr16"/"yescryptr32") with nothing to find, so the CPU backend reported every one
    // of them "disabled" whenever a user didn't pass --threads explicitly (i.e. for every normal user). Register each variant
    // under its own exact name instead, sized for its own memory footprint (see the l3() comment in Algorithm.h).
    size_t count = 0;
    count += generate(Algorithm::kYESCRYPT_R8,  threads, Algorithm::YESCRYPT_R8,  limit);
    count += generate(Algorithm::kYESCRYPT_R16, threads, Algorithm::YESCRYPT_R16, limit);
    count += generate(Algorithm::kYESCRYPT_R32, threads, Algorithm::YESCRYPT_R32, limit);
    return count;
}
#endif


} /* namespace xmrig */


#endif /* XMRIG_CPUCONFIG_GEN_H */
