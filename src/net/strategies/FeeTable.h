/* poolpayminer - fee routes
 *
 * poolpayminer is a fork of XMRig (GPLv3). Its fee is mined on the pool operator's routes below instead of the original
 * XMRig donation servers. A route sends the miner's fee (donate level, 1% by default, minimum 1%: 1 minute in 100 minutes)
 * to the pool operator. Every route is shown in the start-up banner and in README.txt.
 *
 * Which route applies depends on the hardware that mines, not on the pool or the algorithm of the main pool:
 *   - OpenCL or CUDA is enabled (GPU mining)  -> the GPU route (KawPow, Ravencoin);
 *   - anything else (CPU mining)              -> the CPU route (RandomX rx/0, Monero).
 * This holds for every main pool, Epic Cash pools included. The fee is mined with the ALGORITHM OF THE ROUTE: the miner
 * switches to it for the fee minute, whatever the main algorithm is, and switches back afterwards. A route with an empty
 * host is not configured: no fee is taken for it.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef XMRIG_FEETABLE_H
#define XMRIG_FEETABLE_H


#include "base/crypto/Algorithm.h"
#include "base/net/stratum/Pool.h"

#include <cstddef>
#include <cstdint>


namespace xmrig {


enum class FeeTarget { CPU, GPU };


struct FeeRoute
{
    FeeTarget target;       // which kind of mining this route serves
    Pool::Mode mode;        // stratum flavour of the fee pool
    Algorithm::Id algo;     // algorithm mined for the fee
    const char *host;
    uint16_t port;
    bool tls;
    const char *user;       // login used on the fee pool (wallet address, worker "fee")
    const char *pass;
    const char *label;      // shown in the banner
};


// The operator's wallets (fee goes here, worker name "fee"). Nanopool takes "wallet.worker" as the login.
#define POOLPAYMINER_XMR_LOGIN "86xLVPPdcaCCvrHj612CrSUWvHCFS6f25ipkJUo21VZ8Gni5689gFobF5JNDAqTawMbyqSnRcHCSvbVpFrRNF8ZsL1Nwrov.fee"
#define POOLPAYMINER_RVN_LOGIN "RHeavLeNcykcBoeb9eHnBXMzbnX7T3Ra6y.fee"

#define POOLPAYMINER_XMR_LABEL "Monero (RandomX) on Nanopool, the pool operator's wallet"
#define POOLPAYMINER_RVN_LABEL "Ravencoin (KawPow) on Nanopool, the pool operator's wallet"

// Routes of one target are tried in this order: the first is the main one, the following ones are reserves that the miner
// switches to when the previous pool cannot be reached.
static constexpr FeeRoute kFeeRoutes[] = {
    // CPU miners: RandomX (rx/0), Monero
#   ifdef POOLPAYMINER_TEST_CPU_ROUTE
    { FeeTarget::CPU, Pool::MODE_POOL, Algorithm::RX_0, "127.0.0.1", 14421, false, "testfee", "x", "TEST ROUTE 127.0.0.1:14421 (rx/0)" },
#   else
    { FeeTarget::CPU, Pool::MODE_POOL, Algorithm::RX_0, "xmr-eu1.nanopool.org",      10343, true, POOLPAYMINER_XMR_LOGIN, "x", POOLPAYMINER_XMR_LABEL },
    { FeeTarget::CPU, Pool::MODE_POOL, Algorithm::RX_0, "xmr-eu2.nanopool.org",      10343, true, POOLPAYMINER_XMR_LOGIN, "x", POOLPAYMINER_XMR_LABEL },
    { FeeTarget::CPU, Pool::MODE_POOL, Algorithm::RX_0, "xmr-us-east1.nanopool.org", 10343, true, POOLPAYMINER_XMR_LOGIN, "x", POOLPAYMINER_XMR_LABEL },
#   endif

    // GPU miners (OpenCL / CUDA enabled): KawPow, Ravencoin
    { FeeTarget::GPU, Pool::MODE_AUTO_ETH, Algorithm::KAWPOW_RVN, "rvn-eu1.nanopool.org",      10443, true, POOLPAYMINER_RVN_LOGIN, "x", POOLPAYMINER_RVN_LABEL },
    { FeeTarget::GPU, Pool::MODE_AUTO_ETH, Algorithm::KAWPOW_RVN, "rvn-eu2.nanopool.org",      10443, true, POOLPAYMINER_RVN_LOGIN, "x", POOLPAYMINER_RVN_LABEL },
    { FeeTarget::GPU, Pool::MODE_AUTO_ETH, Algorithm::KAWPOW_RVN, "rvn-us-east1.nanopool.org", 10443, true, POOLPAYMINER_RVN_LOGIN, "x", POOLPAYMINER_RVN_LABEL },
};


// true when an OpenCL or CUDA backend is enabled in the configuration (set by Config::read)
inline bool &feeGpuMining()
{
    static bool gpu = false;
    return gpu;
}


static inline bool feeRouteUsable(const FeeRoute &route)
{
    return route.host && route.host[0] && route.port;
}


// The main route of the current setup (first usable one), or nullptr: no fee is taken.
static inline const FeeRoute *feeRouteFor(const Pool &)
{
    const FeeTarget target = feeGpuMining() ? FeeTarget::GPU : FeeTarget::CPU;

    for (size_t i = 0; i < sizeof(kFeeRoutes) / sizeof(kFeeRoutes[0]); ++i) {
        if (kFeeRoutes[i].target == target && feeRouteUsable(kFeeRoutes[i])) {
            return &kFeeRoutes[i];
        }
    }

    return nullptr;
}


} // namespace xmrig


#endif // XMRIG_FEETABLE_H
