/* XMRig
 * Copyright (c) 2019      Howard Chu  <https://github.com/hyc>
 * Copyright (c) 2018-2023 SChernykh   <https://github.com/SChernykh>
 * Copyright (c) 2016-2023 XMRig       <https://github.com/xmrig>, <support@xmrig.com>
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

#ifndef XMRIG_NETWORK_H
#define XMRIG_NETWORK_H


#include "3rdparty/rapidjson/fwd.h"
#include "base/api/interfaces/IApiListener.h"
#include "base/kernel/interfaces/IBaseListener.h"
#include "base/kernel/interfaces/IStrategyListener.h"
#include "base/kernel/interfaces/ITimerListener.h"
#include "base/tools/Object.h"
#include "interfaces/IJobResultListener.h"


#include <cstdint>
#include <vector>


namespace xmrig {


class Controller;
class IStrategy;
class NetworkState;


class Network : public IJobResultListener, public IStrategyListener, public IBaseListener, public ITimerListener, public IApiListener
{
public:
    XMRIG_DISABLE_COPY_MOVE_DEFAULT(Network)

    Network(Controller *controller);
    ~Network() override;

    inline IStrategy *strategy() const { return m_strategy; }

    void connect();
    void execCommand(char command);

protected:
    inline void onTimer(const Timer *) override { tick(); }

    void onActive(IStrategy *strategy, IClient *client) override;
    void onConfigChanged(Config *config, Config *previousConfig) override;
    void onJob(IStrategy *strategy, IClient *client, const Job &job, const rapidjson::Value &params) override;
    void onJobResult(const JobResult &result) override;
    void onLogin(IStrategy *strategy, IClient *client, rapidjson::Document &doc, rapidjson::Value &params) override;
    void onPause(IStrategy *strategy) override;
    void onResultAccepted(IStrategy *strategy, IClient *client, const SubmitResult &result, const char *error) override;
    void onVerifyAlgorithm(IStrategy *strategy, const  IClient *client, const Algorithm &algorithm, bool *ok) override;

#   ifdef XMRIG_FEATURE_API
    void onRequest(IApiRequest &request) override;
#   endif

private:
    constexpr static int kTickInterval = 1 * 1000;
    constexpr static uint64_t kEpicGrace = 60 * 1000;   // Epic: keep mining the current job this long while the pool connection is renewed

    void setJob(IClient *client, const Job &job, bool donate);
    void tick();

#   ifdef XMRIG_FEATURE_API
    void getConnection(rapidjson::Value &reply, rapidjson::Document &doc, int version) const;
    void getResults(rapidjson::Value &reply, rapidjson::Document &doc, int version) const;
#   endif

    Controller *m_controller;
    IStrategy *m_donate     = nullptr;
    IStrategy *m_strategy   = nullptr;
    NetworkState *m_state   = nullptr;
    Timer *m_timer          = nullptr;
    bool m_resilient        = false;    // a lost connection is renewed quietly, mining goes on meanwhile (every pool)
    bool m_quietJob         = false;    // the next job is the one we already had, delivered again after a reconnect
    bool m_diardiPaused     = false;    // Scala: the miner rests while a block of the allow-listed Diardi miners is found
    uint64_t m_graceUntil   = 0;        // Epic: connection is being renewed, mining goes on until this moment
    uint64_t m_height       = 0;        // height of the last job of the main pool
};


} // namespace xmrig


#endif // XMRIG_NETWORK_H
