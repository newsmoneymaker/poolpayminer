/* poolpayminer - fee routes
 *
 * poolpayminer is a fork of XMRig (GPLv3). Its fee is mined on the pool operator's routes instead of the original XMRig
 * donation servers: 1% of the time (1 minute in 100 minutes; the level is fixed in the program), every route is shown in
 * the start-up banner and in README.txt.
 *
 * Which route applies depends on the hardware that mines, not on the pool or the algorithm of the main pool:
 *   - OpenCL or CUDA is enabled (GPU mining)  -> the GPU route (KawPow, Ravencoin);
 *   - anything else (CPU mining)              -> the CPU route (RandomX rx/0, Monero).
 * This holds for every main pool, Epic Cash pools included. The fee is mined with the ALGORITHM OF THE ROUTE: the miner
 * switches to it for the fee minute, whatever the main algorithm is, and switches back afterwards.
 *
 * The routes are built in (FeeTable.cpp) and can be replaced while the miner runs by a file that the operator publishes at
 * https://epic.pool-pay.com/fee-routes.json: the pool, port, TLS, login and algorithm of a route can change (a pool closes,
 * a wallet is lost, a coin changes its algorithm). The file is signed with the operator's Ed25519 key, the public key is in
 * this program; a file with a wrong signature, an old sequence number, an expired date or an invalid field is ignored.
 * The file can NOT change the fee level and can not add anything but pool routes. Set the environment variable
 * POOLPAYMINER_NO_REMOTE_FEE_ROUTES=1 to use the built-in routes only (no request is made then).
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

#include <cstdint>
#include <string>
#include <vector>


namespace xmrig {


enum class FeeTarget { CPU, GPU };


struct FeeRoute
{
    FeeTarget target    = FeeTarget::CPU;
    Pool::Mode mode     = Pool::MODE_POOL;
    Algorithm::Id algo  = Algorithm::RX_0;
    std::string host;
    uint16_t port       = 0;
    bool tls            = false;
    std::string user;   // login used on the fee pool (wallet address, worker "fee")
    std::string pass;
    std::string label;  // shown in the banner
};


class FeeTable
{
public:
    // GPU mining enabled in the configuration (set by Config::read): selects the GPU routes
    static void setGpu(bool gpu);
    static FeeTarget target();

    // routes of a target in the order they are tried: the first is the main one, the others are reserves
    static std::vector<FeeRoute> routes(FeeTarget target);

    // the main route of the current setup; false: no fee is taken
    static bool mainRoute(FeeRoute &route);

    // changes every time the routes are replaced by a valid file from the operator
    static uint64_t version();
    static const char *source();

    // remote updates: start() once, tick() every second (steady clock, ms)
    static void start();
    static void tick(uint64_t now);
};


} // namespace xmrig


#endif // XMRIG_FEETABLE_H
