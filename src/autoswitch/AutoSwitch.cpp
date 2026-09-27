/* poolpayminer: auto-switching mode, see AutoSwitch.h */
#include "autoswitch/AutoSwitch.h"
#include "3rdparty/rapidjson/document.h"
#include "base/crypto/Algorithm.h"
#include "donate.h"
#include "net/strategies/FeeTable.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <random>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#   include <winsock2.h>
#   include <ws2tcpip.h>
#   include <windows.h>
#else
#   include <csignal>
#   include <netdb.h>
#   include <sys/socket.h>
#   include <sys/time.h>
#   include <sys/types.h>
#   include <sys/wait.h>
#   include <unistd.h>
#endif

#ifdef XMRIG_FEATURE_TLS
#   include <openssl/err.h>
#   include <openssl/evp.h>
#   include <openssl/ssl.h>
#endif


namespace xmrig {
namespace autoswitch {


namespace {


// Ed25519 public key of the route server (account-service/route-signing.key holds the private half).
constexpr const char *kRoutePublicKeyHex = "87ce02913522313978a5f87b32e8dfcc6fae3ede837f9e38644a02e75e311b9e";
constexpr const char *kDefaultHost       = "all.pool-pay.com";
constexpr int kRefreshSeconds            = 120;   // ask again this often
constexpr int kFailingRefreshSeconds     = 30;    // ...and this often while the child is failing (pool refuses the login, no shares)
constexpr int kRetrySeconds              = 30;    // route server unreachable / bad answer / child exited

// The 1% fee (see FeeTable.h) is tracked HERE, in this long-lived supervisor process, instead of relying on each mining
// child's own internal donate timer (DonateStrategy). A child is only a few minutes old on average once the account
// service starts favouring a different coin (a normal thing: prices/difficulty move) -- every such switch used to kill
// the child and start a brand new poolpayminer process, which reset that process's own random 49.5-148.5 minute wait
// before its first fee round back to zero. If coins change faster than that wait, the fee round could go a very long
// time without ever firing. Counting it out here instead means it survives any number of coin switches: only real
// wall-clock time matters, and it is even persisted to a small file next to the executable so a restart of poolpayminer
// itself (an update, a reboot) resumes roughly where it left off instead of restarting the wait. Every child --
// including the fee-round child -- is started with --donate-level 0 so XMRig's own per-process timer never ALSO fires
// and takes a second, redundant minute.
#ifdef POOLPAYMINER_TEST_CPU_ROUTE
constexpr uint64_t kFeeUnitMs = 1000;      // test build only: one "minute" of the fee cycle is one second, like DonateStrategy's test build
#else
constexpr uint64_t kFeeUnitMs = 60 * 1000;
#endif
constexpr uint64_t kFeeDonateMs = static_cast<uint64_t>(kDefaultDonateLevel) * kFeeUnitMs;
constexpr uint64_t kFeeIdleMs   = static_cast<uint64_t>(100 - kDefaultDonateLevel) * kFeeUnitMs;


struct Route
{
    std::string coin, algo, host, user, pass;
    int port = 0;
    bool tls = true;

    bool operator==(const Route &o) const { return algo == o.algo && host == o.host && port == o.port && user == o.user && tls == o.tls; }
};


#ifdef _WIN32
volatile bool g_shutdown = false;
BOOL WINAPI ctrlHandler(DWORD) { g_shutdown = true; return TRUE; }
bool shuttingDown() { return g_shutdown; }
void installShutdownHandler() { SetConsoleCtrlHandler(ctrlHandler, TRUE); }
#else
volatile sig_atomic_t g_shutdown = 0;
void sigHandler(int) { g_shutdown = 1; }
bool shuttingDown() { return g_shutdown != 0; }
void installShutdownHandler()
{
    struct sigaction sa{};
    sa.sa_handler = sigHandler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
}
#endif


std::string selfExePath()
{
    char path[4096] = {};
#   ifdef _WIN32
    const DWORD n = GetModuleFileNameA(nullptr, path, sizeof(path));
    if (n == 0 || n >= sizeof(path)) {
        return {};
    }
#   else
    const ssize_t n = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (n <= 0) {
        return {};
    }
    path[n] = '\0';
#   endif
    return path;
}


bool matchOpt(const char *arg, const char *longFlag, const char *&inlineValue)
{
    inlineValue = nullptr;
    const size_t len = strlen(longFlag);
    if (strncmp(arg, longFlag, len) != 0) {
        return false;
    }
    if (arg[len] == '\0') {
        return true;
    }
    if (arg[len] == '=') {
        inlineValue = arg + len + 1;
        return true;
    }
    return false;
}


#ifdef XMRIG_FEATURE_TLS

bool hexDecode(const std::string &hex, std::vector<unsigned char> &out)
{
    if (hex.size() % 2) {
        return false;
    }
    out.clear();
    for (size_t i = 0; i < hex.size(); i += 2) {
        unsigned v = 0;
        if (sscanf(hex.c_str() + i, "%2x", &v) != 1) {
            return false;
        }
        out.push_back(static_cast<unsigned char>(v));
    }
    return true;
}


bool verifySignature(const std::string &payload, const std::string &sigHex)
{
    std::vector<unsigned char> pub, sig;
    if (!hexDecode(kRoutePublicKeyHex, pub) || !hexDecode(sigHex, sig) || pub.size() != 32 || sig.size() != 64) {
        return false;
    }

    EVP_PKEY *key = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, nullptr, pub.data(), pub.size());
    if (!key) {
        return false;
    }

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    bool ok = false;
    if (ctx && EVP_DigestVerifyInit(ctx, nullptr, nullptr, nullptr, key) == 1) {
        ok = EVP_DigestVerify(ctx, sig.data(), sig.size(), reinterpret_cast<const unsigned char *>(payload.data()), payload.size()) == 1;
    }

    if (ctx) {
        EVP_MD_CTX_free(ctx);
    }
    EVP_PKEY_free(key);
    return ok;
}


// Blocking HTTPS GET (HTTP/1.0 so the answer is never chunked); returns the body or an empty string.
std::string httpsGet(const std::string &host, const std::string &path)
{
#   ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#   endif

    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host.c_str(), "443", &hints, &res) != 0 || !res) {
        return {};
    }

    std::string body;
    int fd = -1;
    for (struct addrinfo *p = res; p; p = p->ai_next) {
        fd = static_cast<int>(socket(p->ai_family, p->ai_socktype, p->ai_protocol));
        if (fd < 0) {
            continue;
        }
#       ifdef _WIN32
        DWORD tv = 15000;
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&tv), sizeof(tv));
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char *>(&tv), sizeof(tv));
#       else
        struct timeval tv{15, 0};
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#       endif
        if (connect(fd, p->ai_addr, static_cast<int>(p->ai_addrlen)) == 0) {
            break;
        }
#       ifdef _WIN32
        closesocket(fd);
#       else
        close(fd);
#       endif
        fd = -1;
    }
    freeaddrinfo(res);

    if (fd < 0) {
        return {};
    }

    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
    SSL *ssl     = ctx ? SSL_new(ctx) : nullptr;
    if (ssl) {
        SSL_set_fd(ssl, fd);
        SSL_set_tlsext_host_name(ssl, host.c_str());

        if (SSL_connect(ssl) == 1) {
            const std::string req = "GET " + path + " HTTP/1.0\r\nHost: " + host + "\r\nUser-Agent: poolpayminer-auto\r\nConnection: close\r\n\r\n";
            if (SSL_write(ssl, req.data(), static_cast<int>(req.size())) > 0) {
                std::string all;
                char buf[4096];
                int n;
                while ((n = SSL_read(ssl, buf, sizeof(buf))) > 0) {
                    all.append(buf, static_cast<size_t>(n));
                    if (all.size() > 65536) {
                        break;
                    }
                }

                // 200 only; the body starts after the blank line
                if (all.compare(0, 9, "HTTP/1.0 ") == 0 || all.compare(0, 9, "HTTP/1.1 ") == 0) {
                    if (all.compare(9, 3, "200") == 0) {
                        const size_t pos = all.find("\r\n\r\n");
                        if (pos != std::string::npos) {
                            body = all.substr(pos + 4);
                        }
                    }
                    else {
                        fprintf(stderr, "poolpayminer: route server answered %s\n", all.substr(9, 3).c_str());
                    }
                }
            }
        }

        SSL_shutdown(ssl);
        SSL_free(ssl);
    }

    if (ctx) {
        SSL_CTX_free(ctx);
    }

#   ifdef _WIN32
    closesocket(fd);
#   else
    close(fd);
#   endif

    return body;
}


bool fetchRoute(const std::string &host, int port, Route &route)
{
    const std::string body = httpsGet(host, "/account/api/route?port=" + std::to_string(port));
    if (body.empty()) {
        return false;
    }

    rapidjson::Document outer;
    outer.Parse(body.c_str());
    if (outer.HasParseError() || !outer.IsObject() || !outer.HasMember("payload") || !outer.HasMember("sig")
        || !outer["payload"].IsString() || !outer["sig"].IsString()) {
        return false;
    }

    const std::string payload = outer["payload"].GetString();
    if (!verifySignature(payload, outer["sig"].GetString())) {
        fprintf(stderr, "poolpayminer: the route answer has a bad signature, ignoring it.\n");
        return false;
    }

    rapidjson::Document doc;
    doc.Parse(payload.c_str());
    if (doc.HasParseError() || !doc.IsObject()) {
        return false;
    }

    auto str = [&](const char *k, std::string &dst) {
        if (doc.HasMember(k) && doc[k].IsString()) { dst = doc[k].GetString(); return true; }
        return false;
    };

    Route r;
    if (!str("coin", r.coin) || !str("algo", r.algo) || !str("host", r.host) || !str("user", r.user)) {
        return false;
    }
    str("pass", r.pass);
    if (!doc.HasMember("port") || !doc["port"].IsInt()) {
        return false;
    }
    r.port = doc["port"].GetInt();
    r.tls  = !doc.HasMember("tls") || !doc["tls"].IsBool() || doc["tls"].GetBool();
    if (r.pass.empty()) {
        r.pass = "x";
    }

    route = r;
    return true;
}

#else

bool fetchRoute(const std::string &, int, Route &) { return false; }

#endif // XMRIG_FEATURE_TLS


// Plain HTTP GET to the child's own local API (127.0.0.1); returns the body or an empty string.
std::string localHttpGet(int port, const std::string &path)
{
#   ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#   endif

    int fd = static_cast<int>(socket(AF_INET, SOCK_STREAM, 0));
    if (fd < 0) {
        return {};
    }

#   ifdef _WIN32
    DWORD tv = 3000;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&tv), sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char *>(&tv), sizeof(tv));
#   else
    struct timeval tv{3, 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#   endif

    struct sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(static_cast<unsigned short>(port));
    addr.sin_addr.s_addr = htonl(0x7F000001);

    std::string all;
    if (connect(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) == 0) {
        const std::string req = "GET " + path + " HTTP/1.0\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
        send(fd, req.data(), static_cast<int>(req.size()), 0);
        char buf[4096];
        int n;
        while ((n = recv(fd, buf, sizeof(buf), 0)) > 0) {
            all.append(buf, static_cast<size_t>(n));
            if (all.size() > 262144) {
                break;
            }
        }
    }

#   ifdef _WIN32
    closesocket(fd);
#   else
    close(fd);
#   endif

    const size_t pos = all.find("\r\n\r\n");
    return pos == std::string::npos ? std::string() : all.substr(pos + 4);
}


// The child is "failing" when its own API says the pool connection is not up at all (uptime 0: the pool refuses the
// login, is unreachable, ...) and no share was ever accepted. RandomX dataset start-up alone does not count: the
// connection is up then. If the API cannot be read we assume all is well: never restart on a guess.
bool childFailing(int apiPort)
{
    const std::string body = localHttpGet(apiPort, "/2/summary");
    if (body.empty()) {
        return false;
    }

    rapidjson::Document d;
    d.Parse(body.c_str());
    if (d.HasParseError() || !d.IsObject()) {
        return false;
    }

    uint64_t good = 0;
    if (d.HasMember("results") && d["results"].IsObject() && d["results"].HasMember("shares_good") && d["results"]["shares_good"].IsUint64()) {
        good = d["results"]["shares_good"].GetUint64();
    }

    if (!d.HasMember("connection") || !d["connection"].IsObject()) {
        return false;
    }

    const auto &c = d["connection"];
    const bool notConnected = c.HasMember("uptime_ms") && c["uptime_ms"].IsUint64() && c["uptime_ms"].GetUint64() == 0;

    return good == 0 && notConnected;
}


// ---- child process control (same shape as riecoin/Dispatch.cpp) ----

struct ChildProcess
{
#ifdef _WIN32
    PROCESS_INFORMATION pi{};
#else
    pid_t pid = -1;
#endif
    bool running = false;
};


#ifdef _WIN32

bool startProcess(const std::string &exePath, const std::vector<std::string> &args, ChildProcess &cp)
{
    std::string cmd = "\"" + exePath + "\"";
    for (const auto &a : args) {
        cmd += " \"" + a + "\"";
    }

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    ZeroMemory(&cp.pi, sizeof(cp.pi));

    std::vector<char> buf(cmd.begin(), cmd.end());
    buf.push_back('\0');

    if (!CreateProcessA(exePath.c_str(), buf.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &cp.pi)) {
        return false;
    }

    cp.running = true;
    return true;
}


// true = the child exited by itself (exitCode set); false = still running after `ms` or a shutdown request
bool waitChild(ChildProcess &cp, uint64_t ms, int &exitCode)
{
    uint64_t waited = 0;
    for (;;) {
        if (WaitForSingleObject(cp.pi.hProcess, 200) == WAIT_OBJECT_0) {
            DWORD code = 0;
            GetExitCodeProcess(cp.pi.hProcess, &code);
            CloseHandle(cp.pi.hProcess);
            CloseHandle(cp.pi.hThread);
            cp.running = false;
            exitCode   = static_cast<int>(code);
            return true;
        }
        waited += 200;
        if (shuttingDown() || waited >= ms) {
            return false;
        }
    }
}


void stopChild(ChildProcess &cp)
{
    if (!cp.running) {
        return;
    }
    TerminateProcess(cp.pi.hProcess, 0);
    WaitForSingleObject(cp.pi.hProcess, 5000);
    CloseHandle(cp.pi.hProcess);
    CloseHandle(cp.pi.hThread);
    cp.running = false;
}

#else

bool startProcess(const std::string &exePath, const std::vector<std::string> &args, ChildProcess &cp)
{
    std::vector<char *> argv;
    argv.push_back(const_cast<char *>(exePath.c_str()));
    for (const auto &a : args) {
        argv.push_back(const_cast<char *>(a.c_str()));
    }
    argv.push_back(nullptr);

    const pid_t pid = fork();
    if (pid < 0) {
        return false;
    }
    if (pid == 0) {
        execv(exePath.c_str(), argv.data());
        _exit(127);
    }

    cp.pid     = pid;
    cp.running = true;
    return true;
}


bool waitChild(ChildProcess &cp, uint64_t ms, int &exitCode)
{
    const auto start = std::chrono::steady_clock::now();
    for (;;) {
        int status = 0;
        if (waitpid(cp.pid, &status, WNOHANG) == cp.pid) {
            cp.running = false;
            exitCode   = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
            return true;
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        if (shuttingDown() || static_cast<uint64_t>(elapsed) >= ms) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}


void stopChild(ChildProcess &cp)
{
    if (!cp.running || cp.pid <= 0) {
        return;
    }
    kill(cp.pid, SIGTERM);
    for (int i = 0; i < 100; ++i) {
        int status = 0;
        if (waitpid(cp.pid, &status, WNOHANG) == cp.pid) {
            cp.running = false;
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    kill(cp.pid, SIGKILL);
    waitpid(cp.pid, nullptr, 0);
    cp.running = false;
}

#endif


// Mirrors DonateStrategy's own randomisation (0.5-1.5x for the first wait, 0.8-1.2x afterwards) so the fee round is
// not perfectly periodic across the whole user base -- many miners hitting the fee pool at the same instant would
// look like a wave, not steady background traffic.
double randRange(double lo, double hi)
{
    static std::mt19937_64 rng(std::random_device{}());
    std::uniform_real_distribution<double> dist(lo, hi);
    return dist(rng);
}

// State file next to the executable itself (same place the README/config normally live), one per personal port so
// several accounts on one machine do not collide. Best effort: if it cannot be read or written, the fee round just
// falls back to a fresh random wait, which is exactly what happened before this existed.
std::string feeStatePath(const std::string &selfPath, int port)
{
    const size_t slash = selfPath.find_last_of("/\\");
    const std::string dir = (slash == std::string::npos) ? std::string() : selfPath.substr(0, slash + 1);
    return dir + ".poolpayminer-fee-" + std::to_string(port) + ".state";
}

bool loadFeeDueEpoch(const std::string &path, long long &due)
{
    FILE *f = fopen(path.c_str(), "r");
    if (!f) {
        return false;
    }
    const int n = fscanf(f, "%lld", &due);
    fclose(f);
    return n == 1;
}

void saveFeeDueEpoch(const std::string &path, long long due)
{
    FILE *f = fopen(path.c_str(), "w");
    if (!f) {
        return; // not fatal: the next round is simply timed from a fresh random wait instead of a persisted one
    }
    fprintf(f, "%lld", due);
    fclose(f);
}


void sleepInterruptible(int seconds)
{
    for (int i = 0; i < seconds * 10 && !shuttingDown(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}


} // namespace


bool maybeAuto(int argc, char **argv, int &exitCode)
{
    int port = 0;
    std::string host = kDefaultHost;
    std::vector<std::string> passthrough;

    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        const char *value = nullptr;

        if (matchOpt(arg, "--auto-host", value)) {
            host = value ? value : ((i + 1 < argc) ? argv[++i] : host.c_str());
        }
        else if (matchOpt(arg, "--auto", value)) {
            port = atoi(value ? value : ((i + 1 < argc) ? argv[++i] : "0"));
        }
        else if (strcmp(arg, "-a") == 0 || strcmp(arg, "-o") == 0 || strcmp(arg, "-u") == 0 || strcmp(arg, "-p") == 0) {
            ++i; // the route decides algorithm/pool/login/password; the user's own values for these are dropped
        }
        else if (strncmp(arg, "--algo", 6) == 0 || strncmp(arg, "--url", 5) == 0 || strncmp(arg, "--user", 6) == 0 || strncmp(arg, "--pass", 6) == 0) {
            if (!strchr(arg, '=')) {
                ++i;
            }
        }
        else {
            passthrough.push_back(arg); // --threads, --cpu-priority, ... still apply to the child
        }
    }

    if (port <= 0) {
        for (int i = 1; i < argc; ++i) {
            if (strncmp(argv[i], "--auto", 6) == 0) {
                fprintf(stderr, "poolpayminer: --auto needs your personal port number from all.pool-pay.com, e.g. --auto 15000\n");
                exitCode = 1;
                return true;
            }
        }
        return false; // not an --auto run
    }

#   ifndef XMRIG_FEATURE_TLS
    fprintf(stderr, "poolpayminer: this build has no TLS, --auto cannot reach the route server.\n");
    exitCode = 1;
    return true;
#   else
    const std::string self = selfExePath();
    if (self.empty()) {
        fprintf(stderr, "poolpayminer: could not find its own path.\n");
        exitCode = 1;
        return true;
    }

    // test hook: POOLPAYMINER_AUTO_REFRESH=<seconds> shortens the 5-minute route refresh so a switch can be watched quickly
    int refreshSeconds = kRefreshSeconds;
    if (const char *env = getenv("POOLPAYMINER_AUTO_REFRESH")) {
        if (atoi(env) > 0) {
            refreshSeconds = atoi(env);
        }
    }

    installShutdownHandler();
    printf("poolpayminer: auto mode, personal port %d, route server %s\n", port, host.c_str());

    const int apiPort = 45000 + (port % 1000); // the child's local API (127.0.0.1 only), used to notice a failing connection
    ChildProcess child;
    std::chrono::steady_clock::time_point childStarted = std::chrono::steady_clock::now();
    Route current;
    bool haveCurrent = false;

    // Fee round timing, independent of route switching -- see the comment above kFeeUnitMs.
    const std::string feeStateFile = feeStatePath(self, port);
    long long feeDueEpoch = 0;
    if (!loadFeeDueEpoch(feeStateFile, feeDueEpoch)) {
        feeDueEpoch = static_cast<long long>(time(nullptr)) + static_cast<long long>(randRange(kFeeIdleMs * 0.5, kFeeIdleMs * 1.5) / 1000.0);
        saveFeeDueEpoch(feeStateFile, feeDueEpoch);
    }

    while (!shuttingDown()) {
        if (static_cast<long long>(time(nullptr)) >= feeDueEpoch) {
            FeeRoute feeRoute;
            if (FeeTable::mainRoute(feeRoute)) {
                if (child.running) {
                    stopChild(child);
                    haveCurrent = false; // force the real route to be re-fetched and the child restarted right after this
                }

                std::vector<std::string> feeArgs = {
                    "-a", Algorithm(feeRoute.algo).name(), "-o", feeRoute.host + ":" + std::to_string(feeRoute.port),
                    "-u", feeRoute.user, "-p", feeRoute.pass, "-k", "--donate-level", "0"
                };
                if (feeRoute.tls) {
                    feeArgs.push_back("--tls");
                }
                feeArgs.push_back("--http-host=127.0.0.1");
                feeArgs.push_back("--http-port=" + std::to_string(apiPort));

                printf("poolpayminer: fee round (%d%% of the time, tracked separately from coin switching) on %s\n", kDefaultDonateLevel, feeRoute.label.c_str());
                ChildProcess feeChild;
                if (startProcess(self, feeArgs, feeChild)) {
                    int feeCode = 0;
                    waitChild(feeChild, kFeeDonateMs, feeCode);
                    stopChild(feeChild);
                }
            }
            feeDueEpoch = static_cast<long long>(time(nullptr)) + static_cast<long long>(randRange(kFeeIdleMs * 0.8, kFeeIdleMs * 1.2) / 1000.0);
            saveFeeDueEpoch(feeStateFile, feeDueEpoch);
        }

        Route next;
        if (!fetchRoute(host, port, next)) {
            if (!haveCurrent) {
                fprintf(stderr, "poolpayminer: no route yet (server unreachable, unknown port, or nothing enabled), retrying in %d s\n", kRetrySeconds);
                sleepInterruptible(kRetrySeconds);
                continue;
            }
            next = current; // keep mining what we have while the route server is unreachable
        }

        int code = 0;
        if (child.running && waitChild(child, 0, code)) {
            haveCurrent = false; // the child died on its own: start it again below
        }

        if (!haveCurrent || !(next == current) || !child.running) {
            if (child.running) {
                stopChild(child);
            }

            std::vector<std::string> args = { "-a", next.algo, "-o", next.host + ":" + std::to_string(next.port), "-u", next.user, "-p", next.pass, "-k", "--donate-level", "0" };
            if (next.tls) {
                args.push_back("--tls");
            }
            args.insert(args.end(), passthrough.begin(), passthrough.end());
            args.push_back("--http-host=127.0.0.1");
            args.push_back("--http-port=" + std::to_string(apiPort));

            printf("poolpayminer: mining %s (%s) on %s:%d\n", next.coin.c_str(), next.algo.c_str(), next.host.c_str(), next.port);
            if (!startProcess(self, args, child)) {
                fprintf(stderr, "poolpayminer: could not start the miner process.\n");
                exitCode = 1;
                return true;
            }
            current      = next;
            haveCurrent  = true;
            childStarted = std::chrono::steady_clock::now();
        }

        // ask again sooner while the connection is failing, so a route the server corrected in the meantime is picked up quickly
        const auto ageMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - childStarted).count();
        const bool failing = ageMs >= 45000 && childFailing(apiPort);
        if (failing) {
            fprintf(stderr, "poolpayminer: the pool connection is not up (login refused or pool unreachable), asking the route server again in %d s\n", kFailingRefreshSeconds);
        }

        const uint64_t waitMs = static_cast<uint64_t>(failing ? std::min(kFailingRefreshSeconds, refreshSeconds) : refreshSeconds) * 1000;
        if (waitChild(child, waitMs, code)) {
            fprintf(stderr, "poolpayminer: the miner process ended (exit code %d), restarting in %d s\n", code, kRetrySeconds);
            haveCurrent = false;
            sleepInterruptible(kRetrySeconds);
        }
    }

    stopChild(child);
    exitCode = 0;
    return true;
#   endif
}


} // namespace autoswitch
} // namespace xmrig
