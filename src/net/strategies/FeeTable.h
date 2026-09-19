/* poolpayminer - fee routes
 *
 * poolpayminer is a fork of XMRig (GPLv3). The original XMRig donation goes to the XMRig developers
 * and is kept unchanged for every pool that has no route in this table.
 *
 * A route sends the miner's fee (donate level, 1% by default, minimum 1%: 1 minute in 100 minutes)
 * to the pool operator instead. Every route is shown in the start-up banner and in README.txt.
 *
 * Adding a route = adding a line to kFeeRoutes below.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef XMRIG_FEETABLE_H
#define XMRIG_FEETABLE_H


#include "base/net/stratum/Pool.h"

#include <cstddef>
#include <cstdint>


namespace xmrig {


struct FeeRoute
{
    Pool::Mode mode;        // pool mode of the miner's main pool that this route applies to
    const char *host;
    uint16_t port;
    bool tls;
    const char *user;       // login used on the fee pool
    const char *label;      // shown in the banner
};


static constexpr FeeRoute kFeeRoutes[] = {
    // Epic Cash: shares are mined on the operator's pool under the pool wallet's epicbox address (worker "fee")
    { Pool::MODE_EPIC, "epic.pool-pay.com", 3334, true,
      "esYd2vznULSZPn8yQG1SpViF1xEdSs4Fa4n91tkhdSxZCNVdkBt4@epicbox.epiccash.com+fee",
      "epic.pool-pay.com (Epic Cash pool operator)" },
};


static inline const FeeRoute *feeRouteFor(const Pool &pool)
{
    for (size_t i = 0; i < sizeof(kFeeRoutes) / sizeof(kFeeRoutes[0]); ++i) {
        if (kFeeRoutes[i].mode == pool.mode()) {
            return &kFeeRoutes[i];
        }
    }

    return nullptr;
}


} // namespace xmrig


#endif // XMRIG_FEETABLE_H
