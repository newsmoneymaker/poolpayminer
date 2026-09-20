/* poolpayminer - fee routes: built-in table and signed updates from the operator, see FeeTable.h
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "net/strategies/FeeTable.h"
#include "3rdparty/rapidjson/document.h"
#include "base/io/log/Log.h"
#include "base/io/log/Tags.h"
#include "base/tools/Chrono.h"

#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>

#if defined(XMRIG_FEATURE_TLS) && defined(XMRIG_FEATURE_HTTP)
#   define POOLPAYMINER_REMOTE_FEE_ROUTES
#   include "base/kernel/interfaces/IHttpListener.h"
#   include "base/net/http/Fetch.h"
#   include "base/net/http/HttpData.h"
#   include <openssl/evp.h>
#endif


// Public key (Ed25519, 32 bytes as hex) that the file of routes must be signed with. The test build has its own key.
#ifdef POOLPAYMINER_TEST_CPU_ROUTE
#   define POOLPAYMINER_FEE_PUBKEY_HEX "330f41726909b7fdbe85979feaf689a3981386b3e500a6c2ffb70ef3be917c20"
#else
#   define POOLPAYMINER_FEE_PUBKEY_HEX "6553e96e4ca9f185b28db45e7601e249ef212cda8d234448eaf8371797ca571e"
#endif


// The operator's wallets (fee goes here, worker name "fee"). Nanopool takes "wallet.worker" as the login.
#define POOLPAYMINER_XMR_LOGIN "86xLVPPdcaCCvrHj612CrSUWvHCFS6f25ipkJUo21VZ8Gni5689gFobF5JNDAqTawMbyqSnRcHCSvbVpFrRNF8ZsL1Nwrov.fee"
#define POOLPAYMINER_RVN_LOGIN "RHeavLeNcykcBoeb9eHnBXMzbnX7T3Ra6y.fee"

#define POOLPAYMINER_XMR_LABEL "Monero (RandomX) on Nanopool, the pool operator's wallet"
#define POOLPAYMINER_RVN_LABEL "Ravencoin (KawPow) on Nanopool, the pool operator's wallet"


namespace xmrig {


namespace {


constexpr uint64_t kFirstFetchDelay = 20 * 1000;                // after the start
constexpr uint64_t kRefreshInterval = 4 * 3600 * 1000ULL;       // after a valid file
constexpr uint64_t kRetryInterval   = 30 * 60 * 1000ULL;        // after a failure
constexpr size_t kMaxBody           = 16 * 1024;
constexpr size_t kMaxRoutes         = 12;
constexpr uint64_t kMaxValidity     = 200ULL * 24 * 3600;       // a file may not claim to be valid for longer (seconds)


FeeRoute route(FeeTarget target, Pool::Mode mode, Algorithm::Id algo, const char *host, uint16_t port, const char *user, const char *label)
{
    FeeRoute r;
    r.target = target;
    r.mode   = mode;
    r.algo   = algo;
    r.host   = host;
    r.port   = port;
    r.tls    = true;
    r.user   = user;
    r.pass   = "x";
    r.label  = label;

    return r;
}


// Routes of one target are tried in this order: the first is the main one, the following ones are reserves that the miner
// switches to when the previous pool cannot be reached.
std::vector<FeeRoute> builtin()
{
    std::vector<FeeRoute> v;

#   ifdef POOLPAYMINER_TEST_CPU_ROUTE
    {
        FeeRoute r = route(FeeTarget::CPU, Pool::MODE_POOL, Algorithm::RX_0, "127.0.0.1", 14421, "testfee", "TEST ROUTE 127.0.0.1:14421 (rx/0)");
        r.tls = false;
        v.push_back(r);
    }
#   else
    // CPU miners: RandomX (rx/0), Monero
    v.push_back(route(FeeTarget::CPU, Pool::MODE_POOL, Algorithm::RX_0, "xmr-eu1.nanopool.org",      10343, POOLPAYMINER_XMR_LOGIN, POOLPAYMINER_XMR_LABEL));
    v.push_back(route(FeeTarget::CPU, Pool::MODE_POOL, Algorithm::RX_0, "xmr-eu2.nanopool.org",      10343, POOLPAYMINER_XMR_LOGIN, POOLPAYMINER_XMR_LABEL));
    v.push_back(route(FeeTarget::CPU, Pool::MODE_POOL, Algorithm::RX_0, "xmr-us-east1.nanopool.org", 10343, POOLPAYMINER_XMR_LOGIN, POOLPAYMINER_XMR_LABEL));
#   endif

    // GPU miners (OpenCL / CUDA enabled): KawPow, Ravencoin
    v.push_back(route(FeeTarget::GPU, Pool::MODE_AUTO_ETH, Algorithm::KAWPOW_RVN, "rvn-eu1.nanopool.org",      10443, POOLPAYMINER_RVN_LOGIN, POOLPAYMINER_RVN_LABEL));
    v.push_back(route(FeeTarget::GPU, Pool::MODE_AUTO_ETH, Algorithm::KAWPOW_RVN, "rvn-eu2.nanopool.org",      10443, POOLPAYMINER_RVN_LOGIN, POOLPAYMINER_RVN_LABEL));
    v.push_back(route(FeeTarget::GPU, Pool::MODE_AUTO_ETH, Algorithm::KAWPOW_RVN, "rvn-us-east1.nanopool.org", 10443, POOLPAYMINER_RVN_LOGIN, POOLPAYMINER_RVN_LABEL));

    return v;
}


struct State
{
    bool gpu                    = false;
    std::vector<FeeRoute> current;
    uint64_t version            = 1;
    std::string source          = "built-in";
    uint64_t sequence           = 0;        // of the last accepted file
    bool started                = false;
    bool busy                   = false;
    uint64_t next               = 0;
};


State &state()
{
    static State s;
    if (s.current.empty()) {
        s.current = builtin();
    }

    return s;
}


bool usable(const FeeRoute &r)
{
    return !r.host.empty() && r.port;
}


} // namespace


void FeeTable::setGpu(bool gpu)
{
    state().gpu = gpu;
}


FeeTarget FeeTable::target()
{
    return state().gpu ? FeeTarget::GPU : FeeTarget::CPU;
}


std::vector<FeeRoute> FeeTable::routes(FeeTarget target)
{
    std::vector<FeeRoute> out;
    for (const FeeRoute &r : state().current) {
        if (r.target == target && usable(r)) {
            out.push_back(r);
        }
    }

    return out;
}


bool FeeTable::mainRoute(FeeRoute &route)
{
    const auto list = routes(target());
    if (list.empty()) {
        return false;
    }

    route = list.front();
    return true;
}


uint64_t FeeTable::version()
{
    return state().version;
}


const char *FeeTable::source()
{
    return state().source.c_str();
}


#ifndef POOLPAYMINER_REMOTE_FEE_ROUTES

void FeeTable::start()
{
}


void FeeTable::tick(uint64_t)
{
}

#else


namespace {


bool hexValue(const char *hex, uint8_t *out, size_t size)
{
    for (size_t i = 0; i < size; ++i) {
        unsigned v = 0;
        for (int k = 0; k < 2; ++k) {
            const char c = hex[i * 2 + k];
            unsigned d;
            if (c >= '0' && c <= '9')      d = c - '0';
            else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
            else return false;
            v = v * 16 + d;
        }
        out[i] = static_cast<uint8_t>(v);
    }

    return true;
}


bool base64Decode(const char *text, size_t size, std::string &out)
{
    static const char *kAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    out.clear();
    uint32_t acc = 0;
    int bits     = 0;
    size_t pad   = 0;

    for (size_t i = 0; i < size; ++i) {
        const char c = text[i];
        if (c == '=') {
            ++pad;
            continue;
        }
        if (c == '\n' || c == '\r' || c == ' ') {
            continue;
        }

        const char *p = pad ? nullptr : strchr(kAlphabet, c);
        if (!p || !c) {
            return false;
        }

        acc = (acc << 6) | static_cast<uint32_t>(p - kAlphabet);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<char>((acc >> bits) & 0xff));
        }
    }

    return pad < 3;
}


bool verifySignature(const std::string &message, const std::string &signature)
{
    uint8_t pub[32];
    if (signature.size() != 64 || !hexValue(POOLPAYMINER_FEE_PUBKEY_HEX, pub, sizeof(pub))) {
        return false;
    }

    EVP_PKEY *key   = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr, pub, sizeof(pub));
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();

    bool ok = key && ctx
        && EVP_DigestVerifyInit(ctx, nullptr, nullptr, nullptr, key) == 1
        && EVP_DigestVerify(ctx, reinterpret_cast<const unsigned char *>(signature.data()), signature.size(),
                            reinterpret_cast<const unsigned char *>(message.data()), message.size()) == 1;

    if (ctx) {
        EVP_MD_CTX_free(ctx);
    }
    if (key) {
        EVP_PKEY_free(key);
    }

    return ok;
}


bool textOk(const char *s, size_t maxLen)
{
    const size_t n = strlen(s);
    if (n == 0 || n > maxLen) {
        return false;
    }

    for (size_t i = 0; i < n; ++i) {
        if (static_cast<unsigned char>(s[i]) < 0x20 || static_cast<unsigned char>(s[i]) > 0x7e) {
            return false;
        }
    }

    return true;
}


bool hostOk(const char *s)
{
    const size_t n = strlen(s);
    if (n == 0 || n > 100) {
        return false;
    }

    for (size_t i = 0; i < n; ++i) {
        const char c = s[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-')) {
            return false;
        }
    }

    return true;
}


// One route of the file; the whole file is refused when any route is wrong
bool parseRoute(const rapidjson::Value &v, FeeRoute &r)
{
    if (!v.IsObject()) {
        return false;
    }

    const auto str = [&v](const char *name) -> const char * {
        return v.HasMember(name) && v[name].IsString() ? v[name].GetString() : nullptr;
    };

    const char *target = str("target");
    const char *algo   = str("algo");
    const char *mode   = str("mode");
    const char *host   = str("host");
    const char *user   = str("user");
    const char *pass   = str("pass");
    const char *label  = str("label");

    if (!target || !algo || !mode || !host || !user || !label || !v.HasMember("port") || !v["port"].IsUint() || !v.HasMember("tls") || !v["tls"].IsBool()) {
        return false;
    }

    if (strcmp(target, "cpu") == 0)      r.target = FeeTarget::CPU;
    else if (strcmp(target, "gpu") == 0) r.target = FeeTarget::GPU;
    else return false;

    if (strcmp(mode, "pool") == 0)          r.mode = Pool::MODE_POOL;
    else if (strcmp(mode, "auto_eth") == 0) r.mode = Pool::MODE_AUTO_ETH;
    else if (strcmp(mode, "epic") == 0)     r.mode = Pool::MODE_EPIC;
    else return false;

    const Algorithm a(algo);
    if (!a.isValid() || (r.target == FeeTarget::CPU && a.family() == Algorithm::KAWPOW)) {
        return false;
    }

    const unsigned port = v["port"].GetUint();
    if (!port || port > 65535 || !hostOk(host) || !textOk(user, 200) || !textOk(label, 120) || (pass && !textOk(pass, 64))) {
        return false;
    }

    r.algo  = a.id();
    r.host  = host;
    r.port  = static_cast<uint16_t>(port);
    r.tls   = v["tls"].GetBool();
    r.user  = user;
    r.pass  = pass ? pass : "x";
    r.label = label;

    return true;
}


// The file: {"data": "<base64 of the payload>", "sig": "<base64 of the Ed25519 signature of the payload bytes>"}
// The payload: {"v":1, "seq":N, "expires":<unix seconds>, "routes":[{target, algo, mode, host, port, tls, user, pass, label}, ...]}
bool apply(const std::string &body, const char *&why)
{
    State &s = state();

    if (body.empty() || body.size() > kMaxBody) {
        why = "wrong size";
        return false;
    }

    rapidjson::Document envelope;
    envelope.Parse(body.c_str());
    if (envelope.HasParseError() || !envelope.IsObject() || !envelope.HasMember("data") || !envelope["data"].IsString() || !envelope.HasMember("sig") || !envelope["sig"].IsString()) {
        why = "not a signed file";
        return false;
    }

    std::string payload;
    std::string signature;
    if (!base64Decode(envelope["data"].GetString(), envelope["data"].GetStringLength(), payload)
        || !base64Decode(envelope["sig"].GetString(), envelope["sig"].GetStringLength(), signature)) {
        why = "bad encoding";
        return false;
    }

    if (!verifySignature(payload, signature)) {
        why = "invalid signature";
        return false;
    }

    rapidjson::Document doc;
    doc.Parse(payload.c_str());
    if (doc.HasParseError() || !doc.IsObject() || !doc.HasMember("v") || !doc["v"].IsUint() || doc["v"].GetUint() != 1
        || !doc.HasMember("seq") || !doc["seq"].IsUint64() || !doc.HasMember("expires") || !doc["expires"].IsUint64()
        || !doc.HasMember("routes") || !doc["routes"].IsArray()) {
        why = "unsupported content";
        return false;
    }

    const uint64_t sequence = doc["seq"].GetUint64();
    const uint64_t expires  = doc["expires"].GetUint64();
    const uint64_t now      = static_cast<uint64_t>(time(nullptr));

    if (sequence <= s.sequence) {
        why = "not newer than the file in use";
        return false;
    }

    if (expires <= now || expires > now + kMaxValidity) {
        why = "expired or an unreasonable validity";
        return false;
    }

    const auto &list = doc["routes"];
    if (list.Size() == 0 || list.Size() > kMaxRoutes) {
        why = "wrong number of routes";
        return false;
    }

    std::vector<FeeRoute> remote;
    for (rapidjson::SizeType i = 0; i < list.Size(); ++i) {
        FeeRoute r;
        if (!parseRoute(list[i], r)) {
            why = "an invalid route";
            return false;
        }

        remote.push_back(r);
    }

    // the file replaces the routes of the targets it mentions, the others stay as they are
    bool cpu = false;
    bool gpu = false;
    for (const FeeRoute &r : remote) {
        (r.target == FeeTarget::CPU ? cpu : gpu) = true;
    }

    std::vector<FeeRoute> merged = remote;
    for (const FeeRoute &r : builtin()) {
        if ((r.target == FeeTarget::CPU && !cpu) || (r.target == FeeTarget::GPU && !gpu)) {
            merged.push_back(r);
        }
    }

    s.current  = merged;
    s.sequence = sequence;
    s.source   = "updated by the operator, sequence " + std::to_string(sequence);
    ++s.version;

    return true;
}


// The address of the file (the test build may take it from the environment)
struct FileUrl
{
    std::string host  = "epic.pool-pay.com";
    uint16_t port     = 443;
    std::string path  = "/fee-routes.json";
    bool tls          = true;
};


FileUrl fileUrl()
{
    FileUrl u;

#   ifdef POOLPAYMINER_TEST_CPU_ROUTE
    const char *env = getenv("POOLPAYMINER_FEE_ROUTES_URL");     // http://127.0.0.1:8099/fee-routes.json
    if (env && strncmp(env, "http://", 7) == 0) {
        std::string rest = env + 7;
        const size_t slash = rest.find('/');
        std::string hostPort = rest.substr(0, slash);
        u.path = slash == std::string::npos ? "/" : rest.substr(slash);
        const size_t colon = hostPort.find(':');
        u.host = hostPort.substr(0, colon);
        u.port = colon == std::string::npos ? 80 : static_cast<uint16_t>(atoi(hostPort.c_str() + colon + 1));
        u.tls  = false;
    }
#   endif

    return u;
}


class Listener : public IHttpListener
{
protected:
    void onHttpData(const HttpData &data) override
    {
        State &s = state();
        s.busy = false;

        const char *why = "the server did not answer";
        if (data.status == 200 && apply(data.body, why)) {
            s.next = Chrono::steadyMSecs() + kRefreshInterval;
            LOG_NOTICE("%s " WHITE_BOLD("fee routes: %s"), Tags::network(), s.source.c_str());
            return;
        }

        s.next = Chrono::steadyMSecs() + kRetryInterval;

        // a file that is not newer is the normal case between two updates: no noise
        if (data.status == 200 && strcmp(why, "not newer than the file in use") == 0) {
            s.next = Chrono::steadyMSecs() + kRefreshInterval;
            return;
        }

        LOG_VERBOSE("%s " YELLOW("fee routes: file ignored (%s), the routes in use stay: %s"), Tags::network(), why, s.source.c_str());
    }
};


std::shared_ptr<IHttpListener> &listener()
{
    static std::shared_ptr<IHttpListener> l = std::make_shared<Listener>();
    return l;
}


} // namespace


void FeeTable::start()
{
    State &s = state();

    const char *off = getenv("POOLPAYMINER_NO_REMOTE_FEE_ROUTES");
    if (s.started || (off && off[0] && strcmp(off, "0") != 0)) {
        return;
    }

    s.started = true;
    s.next    = Chrono::steadyMSecs() + kFirstFetchDelay;

#   ifdef POOLPAYMINER_TEST_CPU_ROUTE
    s.next = Chrono::steadyMSecs() + 2000;
#   endif
}


void FeeTable::tick(uint64_t now)
{
    State &s = state();

    if (!s.started || s.busy || now < s.next) {
        return;
    }

    s.busy = true;
    s.next = now + kRetryInterval;

    const FileUrl u = fileUrl();
    FetchRequest req(HTTP_GET, u.host.c_str(), u.port, u.path.c_str(), u.tls, true);
    req.timeout = 20 * 1000;

    fetch("fee-routes", std::move(req), listener());
}


#endif // POOLPAYMINER_REMOTE_FEE_ROUTES


} // namespace xmrig
