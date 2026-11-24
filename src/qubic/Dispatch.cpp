/* poolpayminer: Qubic dispatch, see Dispatch.h
 *
 * Speaks the real Qatum protocol (github.com/hackerby888/qatum-protocol, MIT license) directly --
 * newline-delimited single-level JSON over a plain TCP socket. NOT qubic.li's older "Pool Connect"
 * WS endpoint (wss://wps.qubic.li/stratum): that one still hands out StratumJob messages but REFUSES
 * every submit for the live network ("STRATUM does not yet support ant-colony (EP>=228)"), confirmed
 * by a real submit attempt against it. Qatum is the protocol the network currently accepts solutions
 * through, confirmed by: (a) a live TCP connect to server.qatum.org:7777, (b) this project's own
 * vendored score_bpp9000.h (pulled from github.com/hackerby888/Qiner, the client Qatum's own README
 * names) reproducing three real depth-0 golden-vector rows (test/data/gt_ant_production.csv, copied
 * verbatim from the qatum-protocol repo) bit-for-bit, and (c) the bundled mainnet task file
 * (src/crypto/qubic/bpp9000-mainnet.task, copied from that same repo's data/bpp9000.task) hashing to
 * EXACTLY the topologyHash/dataHash a live Qatum SubscribePacket response reports. See
 * qubic-project.md for the full trail.
 *
 * Mines ONLY root (depth-0) children for now (protocolVersion 2, deliberately, even though this
 * vendored client could announce 3): a depth>0 child needs the parent LUT converted from the node's
 * "canonical" layout (dense by updated-neuron position) to this scorer's absolute-neuron-indexed
 * layout first, which is not yet implemented here, and the Qatum docs are explicit that getting that
 * wrong "scores a different network with no error" -- silently wrong instead of loudly broken. Root
 * mining is simpler and already verified correct; tree mining (deeper, better-paying) is a later
 * improvement once that conversion is written and tested against real depth>0 golden vectors too.
 *
 * Like Riecoin (src/riecoin/Dispatch.h), this is a self-contained side path main() checks before any
 * of XMRig's own argument parsing or App/Entry startup runs: bpp9000 attempts cost whole seconds to
 * minutes of CPU each (not hashes/sec), so it doesn't fit Algorithm::Id/Job/Pool. The 1% dev fee works
 * exactly like Riecoin's: for 1% of the time this process relaunches ITSELF with "-a rx/0 -o <fee
 * route> ...", which goes through poolpayminer's completely normal startup (maybeDispatch() returns
 * false for rx/0), then kills that child and resumes.
 */
#include "qubic/Dispatch.h"
#include "base/io/json/Json.h"
#include "net/strategies/FeeTable.h"
#include "crypto/qubic/score_bpp9000.h"
#include "crypto/qubic/keyUtils.h"
#include "crypto/qubic/qatum.h"
#include "crypto/qubic/bpp9000_fetch.h"
#include "3rdparty/rapidjson/document.h"

#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <queue>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#   include <windows.h>
#else
#   include <csignal>
#   include <sys/wait.h>
#   include <unistd.h>
#endif


namespace xmrig {
namespace qubic {
namespace {


// Same fee level/unit as Riecoin's dispatcher and DonateStrategy: 1% of the time.
constexpr int kDonateLevel = 1;
#ifdef POOLPAYMINER_TEST_CPU_ROUTE
constexpr uint64_t kFeeUnitMs = 1000;
#else
constexpr uint64_t kFeeUnitMs = 60 * 1000;
#endif

// Deliberately root-only (v2) -- see the file header. Announcing 3 without the canonical-layout
// conversion would silently mine against the wrong network on any job that actually has a parent.
constexpr int kQatumProtocolVersion = 2;

// The real production instantiation: must match the vendored score_bpp9000.h's own namespace
// constants exactly -- these come from github.com/hackerby888/Qiner, confirmed correct against real
// golden-vector data and the live network's own task-file hashes (see file header). Do NOT "fix" these
// to look more like the upstream qubic/Qiner or qubic/core repos' numbers: those are either stale or a
// not-yet-deployed future version, confirmed by direct testing.
using QubicScorer = score_bpp9000::Miner<
    score_bpp9000::NUMBER_OF_INPUT_NEURONS,
    score_bpp9000::NUMBER_OF_OUTPUT_NEURONS,
    score_bpp9000::SEQUENCE_LENGTH,
    score_bpp9000::WINDOW_WIDTH,
    score_bpp9000::MAX_NUMBER_OF_TICKS,
    score_bpp9000::NUMBER_OF_NEIGHBORS,
    score_bpp9000::POPULATION_THRESHOLD,
    score_bpp9000::NUMBER_OF_MUTATIONS,
    score_bpp9000::SOLUTION_THRESHOLD>;


#ifdef _WIN32
volatile bool g_shutdown = false;
BOOL WINAPI ctrlHandler(DWORD)
{
    g_shutdown = true;
    return TRUE;
}
#else
volatile sig_atomic_t g_shutdown = 0;
void sigHandler(int)
{
    g_shutdown = 1;
}
#endif


void installShutdownHandler()
{
#ifdef _WIN32
    SetConsoleCtrlHandler(ctrlHandler, TRUE);
#else
    struct sigaction sa{};
    sa.sa_handler = sigHandler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
#endif
}


bool shuttingDown()
{
#ifdef _WIN32
    return g_shutdown;
#else
    return g_shutdown != 0;
#endif
}


bool isQubicAlgo(const std::string &algo)
{
    std::string a;
    a.reserve(algo.size());
    for (char c : algo) a.push_back(static_cast<char>(::tolower(static_cast<unsigned char>(c))));

    return a == "qubic" || a == "bpp9000" || a == "qubic/bpp9000";
}


bool matchOpt(const char *arg, const char *shortFlag, const char *longFlag, const char *&inlineValue)
{
    inlineValue = nullptr;

    if (shortFlag && strcmp(arg, shortFlag) == 0) {
        return true;
    }

    if (longFlag) {
        const size_t len = strlen(longFlag);
        if (strncmp(arg, longFlag, len) == 0) {
            if (arg[len] == '\0') {
                return true;
            }
            if (arg[len] == '=') {
                inlineValue = arg + len + 1;
                return true;
            }
        }
    }

    return false;
}


std::string selfExePath()
{
    char path[4096] = {};
#ifdef _WIN32
    const DWORD n = GetModuleFileNameA(nullptr, path, sizeof(path));
    if (n == 0 || n >= sizeof(path)) {
        return {};
    }
#else
    const ssize_t n = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (n <= 0) {
        return {};
    }
    path[n] = '\0';
#endif
    return path;
}


std::string exeDir(const std::string &exePath)
{
    const size_t pos = exePath.find_last_of("/\\");
    if (pos == std::string::npos) {
        return {};
    }
    return exePath.substr(0, pos + 1);
}


// Set once in maybeDispatch() before any worker thread starts; read-only after that. This is the
// BUNDLED snapshot's path -- the fallback/initial topology source. What worker threads actually load
// is g_liveTask.activePath below, which the live-fetch thread can swap at runtime.
std::string g_taskFilePath;

// Real Qubic nodes to fetch fresh bpp9000 task data from directly (the binary peer protocol, port
// 21841 -- NOT the Qatum pool connection). Same bootstrap list the pool's own node-manager uses, see
// qatum-protocol-src/.env's NODE_IPS -- reusing an already-trusted source rather than inventing a new
// one.
const std::vector<std::string> kBootstrapNodeIps = {"82.197.173.130", "195.154.148.68", "90.11.201.149"};

// Stay well under the hourly dataHash rotation found in this project's own research
// (critical-qubic-task-data-staleness memory) so the loaded task is never far behind.
constexpr int kLiveFetchIntervalSeconds = 45 * 60;

// Shared between the live-fetch thread (the only writer) and worker threads (readers, via
// currentActivePath()/taskVersion()). version bumps whenever activePath actually changes to a
// genuinely different task (compared by the protocol-level first-frame hash, not just "a fetch
// happened") -- worker threads reload the scorer only when this, or the job's own seedVersion,
// changes.
struct LiveTaskState
{
    std::mutex mutex;
    std::string activePath;
    std::atomic<uint64_t> version{0};
    unsigned char lastFirstFrameHash[32] = {};
    bool haveFirstFrameHash = false;

    std::string currentActivePath()
    {
        std::lock_guard<std::mutex> lock(mutex);
        return activePath;
    }
};
LiveTaskState g_liveTask;

// Updated whenever the Qatum pool's own SubscribePacket reports a dataHash (its protocol-level hash
// of the live first-frame targets, same quantity fetchAndWriteLiveTaskFile() computes into
// outDataHashFirstFrame -- see critical-bpp9000-datahash-formula memory). Used only as an
// informational cross-check logged by the live-fetch thread, never to gate mining: the node fetch
// itself is the authoritative source once it succeeds.
std::mutex g_poolHashMutex;
bool g_havePoolDataHash = false;
unsigned char g_poolDataHash[32] = {};


void liveFetchThreadProc()
{
    const std::string livePath = g_taskFilePath + ".live";

    while (!shuttingDown()) {
        bool fetchedThisRound = false;

        for (const std::string &ip : kBootstrapNodeIps) {
            if (shuttingDown()) break;

            const std::string topoSrc = g_liveTask.currentActivePath();
            unsigned char firstFrameHash[32];

            if (qubic_live_fetch::fetchAndWriteLiveTaskFile(ip.c_str(), topoSrc.c_str(), livePath.c_str(), firstFrameHash)) {
                bool changed = false;
                {
                    std::lock_guard<std::mutex> lock(g_liveTask.mutex);
                    changed = !g_liveTask.haveFirstFrameHash || memcmp(g_liveTask.lastFirstFrameHash, firstFrameHash, 32) != 0;
                    if (changed) {
                        memcpy(g_liveTask.lastFirstFrameHash, firstFrameHash, 32);
                        g_liveTask.haveFirstFrameHash = true;
                        g_liveTask.activePath = livePath;
                        g_liveTask.version.fetch_add(1);
                    }
                }

                if (changed) {
                    printf("poolpayminer: qubic: live task data refreshed from node %s (first-frame hash %s)\n",
                        ip.c_str(), qatum::toHex(firstFrameHash, 32).c_str());

                    std::lock_guard<std::mutex> poolLock(g_poolHashMutex);
                    if (g_havePoolDataHash) {
                        const bool matchesPool = memcmp(g_poolDataHash, firstFrameHash, 32) == 0;
                        printf("poolpayminer: qubic: live task data %s the pool's last-reported dataHash\n",
                            matchesPool ? "MATCHES" : "does NOT match");
                    }
                }
                fetchedThisRound = true;
                break;
            }
        }

        if (!fetchedThisRound && !shuttingDown()) {
            fprintf(stderr, "poolpayminer: qubic: could not fetch live task data from any known node this round; "
                "mining continues against the last loaded task file (may be stale)\n");
        }

        for (int i = 0; i < kLiveFetchIntervalSeconds && !shuttingDown(); ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}


void splitHostPort(std::string url, std::string &host, std::string &port)
{
    const size_t scheme = url.find("://");
    if (scheme != std::string::npos) {
        url = url.substr(scheme + 3);
    }

    const size_t colon = url.find_last_of(':');
    if (colon == std::string::npos) {
        host = url;
        port.clear();
        return;
    }

    host = url.substr(0, colon);
    port = url.substr(colon + 1);
}


std::vector<std::string> feeArgs(const FeeRoute &route)
{
    std::vector<std::string> args = {
        "-a", "rx/0",
        "-o", route.host + ":" + std::to_string(route.port),
        "-u", route.user,
        "-p", route.pass.empty() ? "x" : route.pass,
    };
    if (route.tls) {
        args.push_back("--tls");
    }
    return args;
}


#ifdef _WIN32

struct ChildProcess { PROCESS_INFORMATION pi{}; bool running = false; };

std::string quoteArg(const std::string &s) { return "\"" + s + "\""; }

bool startProcess(const std::string &exePath, const std::vector<std::string> &args, ChildProcess &cp)
{
    std::string cmd = quoteArg(exePath);
    for (const auto &a : args) { cmd += ' '; cmd += quoteArg(a); }

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

void runFor(ChildProcess &cp, uint64_t ms)
{
    WaitForSingleObject(cp.pi.hProcess, static_cast<DWORD>(ms));
}

void stopProcess(ChildProcess &cp, DWORD timeoutMs)
{
    if (!cp.running) return;
    TerminateProcess(cp.pi.hProcess, 0);
    WaitForSingleObject(cp.pi.hProcess, timeoutMs);
    CloseHandle(cp.pi.hProcess);
    CloseHandle(cp.pi.hThread);
    cp.running = false;
}

#else

struct ChildProcess { pid_t pid = -1; bool running = false; };

bool startProcess(const std::string &exePath, const std::vector<std::string> &args, ChildProcess &cp)
{
    std::vector<char *> argv;
    argv.push_back(const_cast<char *>(exePath.c_str()));
    for (const auto &a : args) argv.push_back(const_cast<char *>(a.c_str()));
    argv.push_back(nullptr);

    const pid_t pid = fork();
    if (pid < 0) return false;

    if (pid == 0) {
        execv(exePath.c_str(), argv.data());
        _exit(127);
    }

    cp.pid = pid;
    cp.running = true;
    return true;
}

void runFor(ChildProcess &cp, uint64_t ms)
{
    const auto start = std::chrono::steady_clock::now();
    for (;;) {
        int status = 0;
        const pid_t r = waitpid(cp.pid, &status, WNOHANG);
        if (r == cp.pid) { cp.running = false; return; }
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        if (static_cast<uint64_t>(elapsed) >= ms) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void stopProcess(ChildProcess &cp, int timeoutMs)
{
    if (!cp.running || cp.pid <= 0) return;
    kill(cp.pid, SIGTERM);

    const auto start = std::chrono::steady_clock::now();
    for (;;) {
        int status = 0;
        const pid_t r = waitpid(cp.pid, &status, WNOHANG);
        if (r == cp.pid) { cp.running = false; return; }
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        if (elapsed >= timeoutMs) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    kill(cp.pid, SIGKILL);
    waitpid(cp.pid, nullptr, 0);
    cp.running = false;
}

#endif


// ---------------------------------------------------------------------------------------------------
// Job state shared between the Qatum connector thread and the mining worker threads.
// ---------------------------------------------------------------------------------------------------
struct JobState
{
    bool valid = false;
    std::string jobId;
    std::string computorIdStr;          // 60-char identity, echoed back on submit
    unsigned char computorPubkey[32] = {};
    unsigned char seed[32]           = {}; // epoch spectrum digest; seeds the random2 pool
    unsigned char anchorDigest[32]   = {};
    unsigned int threshold = 0;          // submit bound (pool difficulty); score must be <= this
    uint64_t version = 0;                 // bumped on every new job or seed change
    uint64_t seedVersion = 0;             // bumped only when `seed` changes (forces scorer re-init)
};

struct PendingSubmit
{
    std::string jobId;
    std::string computorIdStr;
    unsigned char seed[32];
    unsigned char nonce[32];
    unsigned int claimedScore;
};

struct SharedState
{
    std::mutex jobMutex;
    JobState job;

    std::mutex submitMutex;
    std::queue<PendingSubmit> submits;

    std::atomic<uint64_t> attempts{0};
    std::atomic<uint64_t> solutionsFound{0};

    void setJob(const JobState &j)
    {
        std::lock_guard<std::mutex> lock(jobMutex);
        job = j;
    }

    JobState currentJob()
    {
        std::lock_guard<std::mutex> lock(jobMutex);
        return job;
    }

    void invalidateJob()
    {
        std::lock_guard<std::mutex> lock(jobMutex);
        job.valid = false;
    }

    void queueSubmit(const PendingSubmit &s)
    {
        std::lock_guard<std::mutex> lock(submitMutex);
        submits.push(s);
    }
};


// Canonical nonce for this network: nonce[0]=1 (bpp9000), nonce[1]=L in [1,10], nonce[2]=K in [0,100]
// (the explore-phase length -- NOT reserved; pinning it to 0 gives up the anti-attractor explore
// phase), nonce[3..31] free search-path entropy. See score_bpp9000.h's isCanonicalAntNonce().
void randomNonce(std::mt19937_64 &rng, unsigned char nonce[32])
{
    for (int i = 0; i < 4; ++i) {
        const uint64_t r = rng();
        memcpy(nonce + i * 8, &r, 8);
    }
    nonce[0] = 1;
    // nonce[1] packs L into the low 4 bits AND the mode (WIRING=2, the only mode this scorer/the real
    // node currently implements) into bits 4-5, top two bits clear -- matching
    // score_bpp9000::Miner::isCanonicalAntNonce()/the pool's own isCanonicalAntNonce() exactly. A plain
    // L byte here (the OLD encoding) passes the OLD check but the real node/pool reject every single
    // submission as non-canonical -- found 2026-11-24 from a real miner's 100%-reproducible rejections.
    const unsigned char L = static_cast<unsigned char>(1 + (rng() % 10));   // L in [1,10]
    constexpr unsigned char kModeWiring = 2;
    nonce[1] = static_cast<unsigned char>((kModeWiring << 4) | L);
    nonce[2] = static_cast<unsigned char>(rng() % 101);        // K in [0,100]
}


void workerThreadProc(SharedState &state)
{
    std::mt19937_64 rng(std::random_device{}());
    std::unique_ptr<QubicScorer> scorer;
    uint64_t loadedSeedVersion = 0;
    uint64_t loadedTaskVersion = 0;

    while (!shuttingDown()) {
        const JobState job = state.currentJob();
        if (!job.valid) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            continue;
        }

        const uint64_t taskVersion = g_liveTask.version.load();
        if (!scorer || job.seedVersion != loadedSeedVersion || taskVersion != loadedTaskVersion) {
            const std::string taskFilePath = g_liveTask.currentActivePath();
            scorer.reset(new QubicScorer());
            unsigned char seedCopy[32];
            memcpy(seedCopy, job.seed, 32);
            if (!scorer->initialize(seedCopy, taskFilePath.c_str())) {
                fprintf(stderr, "poolpayminer: qubic: failed to load task file %s, retrying\n", taskFilePath.c_str());
                scorer.reset();
                std::this_thread::sleep_for(std::chrono::seconds(2));
                continue;
            }
            loadedSeedVersion = job.seedVersion;
            loadedTaskVersion = taskVersion;
        }

        QubicScorer::ANN rootAnn;
        scorer->deriveRootANN(job.computorPubkey, rootAnn);

        unsigned char nonce[32];
        randomNonce(rng, nonce);

        const unsigned int score = scorer->computeScoreFromParent(rootAnn, job.computorPubkey, nonce, job.anchorDigest);
        state.attempts.fetch_add(1);

        // The job may have rotated while this attempt ran (each attempt costs real seconds): a stale
        // solution for an old anchor/seed would just be rejected by the pool, so drop it here instead
        // of spending a submit round trip on it.
        if (job.version != state.currentJob().version) {
            continue;
        }

        if (score != QubicScorer::INVALID_SCORE_VALUE && score <= job.threshold) {
            state.solutionsFound.fetch_add(1);
            PendingSubmit submit;
            submit.jobId = job.jobId;
            submit.computorIdStr = job.computorIdStr;
            memcpy(submit.seed, job.seed, 32);
            memcpy(submit.nonce, nonce, 32);
            submit.claimedScore = score;
            state.queueSubmit(submit);
        }
    }
}


} // namespace


bool maybeDispatch(int argc, char **argv, int &exitCode)
{
    std::string algo, url, user, pass, configPath;
    bool haveAlgo = false;
    bool useTls = false;
    unsigned int threads = 0;

    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        const char *inlineValue = nullptr;

        if (strcmp(arg, "--tls") == 0) {
            useTls = true;
        }
        else if (matchOpt(arg, "-a", "--algo", inlineValue)) {
            haveAlgo = true;
            algo = inlineValue ? inlineValue : ((i + 1 < argc) ? argv[++i] : "");
        }
        else if (matchOpt(arg, "-o", "--url", inlineValue)) {
            url = inlineValue ? inlineValue : ((i + 1 < argc) ? argv[++i] : "");
        }
        else if (matchOpt(arg, "-u", "--user", inlineValue)) {
            user = inlineValue ? inlineValue : ((i + 1 < argc) ? argv[++i] : "");
        }
        else if (matchOpt(arg, "-p", "--pass", inlineValue)) {
            pass = inlineValue ? inlineValue : ((i + 1 < argc) ? argv[++i] : "");
        }
        else if (matchOpt(arg, "-c", "--config", inlineValue)) {
            configPath = inlineValue ? inlineValue : ((i + 1 < argc) ? argv[++i] : "");
        }
        else if (matchOpt(arg, "-t", "--threads", inlineValue)) {
            const std::string v = inlineValue ? inlineValue : ((i + 1 < argc) ? argv[++i] : "0");
            threads = static_cast<unsigned int>(std::atoi(v.c_str()));
        }
    }

    if (!configPath.empty() && (url.empty() || user.empty() || !haveAlgo)) {
        rapidjson::Document doc;
        if (Json::get(configPath.c_str(), doc) && doc.IsObject()) {
            const auto poolsIt = doc.FindMember("pools");
            if (poolsIt != doc.MemberEnd() && poolsIt->value.IsArray() && !poolsIt->value.Empty()) {
                const auto &pool = poolsIt->value[0];
                if (!haveAlgo) {
                    if (const char *a = Json::getString(pool, "algo")) { algo = a; haveAlgo = true; }
                }
                if (url.empty()) {
                    if (const char *u = Json::getString(pool, "url")) url = u;
                }
                if (user.empty()) {
                    if (const char *u = Json::getString(pool, "user")) user = u;
                }
                if (pass.empty()) {
                    if (const char *p = Json::getString(pool, "pass")) pass = p;
                }
                if (!useTls) {
                    useTls = Json::getBool(pool, "tls", false);
                }
            }
        }
    }

    if (!haveAlgo || !isQubicAlgo(algo)) {
        return false;
    }

    printf("poolpayminer: algorithm \"%s\" is Qubic's useful proof of work (bpp9000, ant-colony root mining via Qatum): mining in-process.\n", algo.c_str());

    if (url.empty()) {
        fprintf(stderr, "poolpayminer: -o/--url (a Qatum pool host:port) is required for Qubic.\n");
        exitCode = 1;
        return true;
    }
    if (user.empty()) {
        fprintf(stderr, "poolpayminer: -u/--user (your 60-character Qubic wallet id) is required for Qubic.\n");
        exitCode = 1;
        return true;
    }

    if (threads == 0) {
        threads = std::thread::hardware_concurrency();
        if (threads == 0) threads = 1;
    }

    std::string host, port;
    splitHostPort(url, host, port);
    if (port.empty()) port = "7777";

    const std::string self = selfExePath();
    const std::string dir  = exeDir(self);
    g_taskFilePath = dir + "bpp9000-mainnet.task";
    {
        std::ifstream check(g_taskFilePath, std::ios::binary);
        if (!check.good()) {
            fprintf(stderr, "poolpayminer: qubic: mainnet task file not found at %s (expected next to this executable). Redownload the poolpayminer package.\n", g_taskFilePath.c_str());
            exitCode = 1;
            return true;
        }
    }
    g_liveTask.activePath = g_taskFilePath; // bundled snapshot, until the live-fetch thread swaps in something fresher

    FeeRoute route;
    const bool haveFee = !self.empty() && FeeTable::mainRoute(route) && route.target == FeeTarget::CPU;
    const uint64_t idleMs   = haveFee ? (100 - kDonateLevel) * kFeeUnitMs : 0;
    const uint64_t donateMs = haveFee ? kDonateLevel * kFeeUnitMs : 0;
    const std::vector<std::string> fee = haveFee ? feeArgs(route) : std::vector<std::string>{};

    if (haveFee) {
        printf("poolpayminer: fee is %d%% of the time, mined as RandomX (rx/0) on %s\n", kDonateLevel, route.label.c_str());
    }
    else {
        fprintf(stderr, "poolpayminer: could not resolve this program's own path or a fee route; mining Qubic fee-free this run.\n");
    }

    installShutdownHandler();
#ifdef _WIN32
    { WSADATA wsaData; WSAStartup(MAKEWORD(2, 2), &wsaData); }
#endif

    SharedState state;
    std::vector<std::thread> workers;
    for (unsigned int i = 0; i < threads; ++i) {
        workers.emplace_back(workerThreadProc, std::ref(state));
    }
    std::thread liveFetchThread(liveFetchThreadProc);

    printf("poolpayminer: qubic: %u mining thread(s), connecting to %s:%s%s as wallet \"%s\"\n", threads, host.c_str(), port.c_str(), useTls ? " (TLS)" : "", user.c_str());

    auto mainStart = std::chrono::steady_clock::now();
    uint64_t jobVersionCounter = 0;
    uint64_t seedVersionCounter = 0;
    unsigned char lastSeed[32] = {};
    bool haveLastSeed = false;

    while (!shuttingDown()) {
        if (haveFee) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - mainStart).count();
            if (static_cast<uint64_t>(elapsed) >= idleMs) {
                state.invalidateJob();

                ChildProcess feeProc;
                if (startProcess(self, fee, feeProc)) {
                    runFor(feeProc, donateMs);
                    stopProcess(feeProc, 5000);
                }

                mainStart = std::chrono::steady_clock::now();
                if (shuttingDown()) break;
                continue;
            }
        }

        qatum::Client client;
        if (!client.connect(host.c_str(), std::atoi(port.c_str()), useTls)) {
            fprintf(stderr, "poolpayminer: qubic: could not connect to %s:%s, retrying in 5s\n", host.c_str(), port.c_str());
            std::this_thread::sleep_for(std::chrono::seconds(5));
            continue;
        }

        // Pinned to v2 (root-only) deliberately -- see this file's header comment.
        {
            char packet[512];
            snprintf(packet, sizeof(packet), "{\"id\":%d,\"wallet\":\"%s\",\"worker\":\"%s\",\"protocolVersion\":%d}",
                qatum::EVENT_SUBSCRIBE, user.c_str(), pass.empty() ? "poolpayminer" : pass.c_str(), kQatumProtocolVersion);
            if (!client.send(packet)) {
                client.close();
                continue;
            }
        }

        JobState job;
        bool subscribed = false;

        while (!shuttingDown() && client.connected) {
            if (haveFee) {
                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - mainStart).count();
                if (static_cast<uint64_t>(elapsed) >= idleMs) break; // drop out to the fee round above
            }

            std::string line;
            if (!client.poll(line)) {
                fprintf(stderr, "poolpayminer: qubic: disconnected from pool, reconnecting\n");
                break;
            }

            if (!line.empty()) {
                int id = -1;
                qatum::getInt(line, "id", id);

                if (id == qatum::EVENT_SUBSCRIBE) {
                    if (!qatum::isTrue(line, "result")) {
                        std::string err;
                        qatum::getString(line, "error", err);
                        fprintf(stderr, "poolpayminer: qubic: pool refused subscribe: %s\n", err.c_str());
                        client.close();
                        break;
                    }
                    subscribed = true;
                    printf("poolpayminer: qubic: subscribed\n");

                    // The pool reports its OWN protocol-level dataHash here (K12 of its current
                    // first-frame targets, same quantity the live-fetch thread computes) -- purely
                    // informational: logged by the live-fetch thread as a cross-check, never used to
                    // gate mining. See critical-bpp9000-datahash-formula memory.
                    std::string poolDataHashHex;
                    if (qatum::getString(line, "dataHash", poolDataHashHex)) {
                        unsigned char poolHash[32];
                        if (qatum::parseHex(poolDataHashHex, poolHash, 32)) {
                            std::lock_guard<std::mutex> lock(g_poolHashMutex);
                            memcpy(g_poolDataHash, poolHash, 32);
                            g_havePoolDataHash = true;
                        }
                    }
                }
                else if (id == qatum::EVENT_NEW_DIFFICULTY) {
                    unsigned int diff = 0;
                    if (qatum::getUInt(line, "difficulty", diff)) {
                        std::lock_guard<std::mutex> lock(state.jobMutex);
                        state.job.threshold = diff;
                    }
                }
                else if (id == qatum::EVENT_NEW_COMPUTOR_ID) {
                    std::string idStr;
                    if (qatum::getString(line, "computorId", idStr) && idStr.size() == 60) {
                        job.computorIdStr = idStr;
                        getPublicKeyFromIdentity(idStr.c_str(), job.computorPubkey);
                    }
                }
                else if (id == qatum::EVENT_NEW_SEED) {
                    std::string seedHex;
                    if (qatum::getString(line, "seed", seedHex) && qatum::parseHex(seedHex, job.seed, 32)) {
                        if (!haveLastSeed || memcmp(job.seed, lastSeed, 32) != 0) {
                            memcpy(lastSeed, job.seed, 32);
                            haveLastSeed = true;
                            seedVersionCounter++;
                        }
                    }
                }
                else if (id == qatum::EVENT_NEW_JOB) {
                    std::string jobId, idStr, seedHex, anchorHex;
                    qatum::getString(line, "jobId", jobId);
                    qatum::getString(line, "computorId", idStr);
                    qatum::getString(line, "seed", seedHex);
                    qatum::getString(line, "anchorDigest", anchorHex);
                    // The job packet carries its own threshold (qatum-events.ts's NewJobPacket).
                    // Read it here instead of relying solely on the separate EVENT_NEW_DIFFICULTY
                    // message: setJob() below does a full struct assignment (job = j), which would
                    // otherwise silently reset threshold back to this local job's stale/default value
                    // (0) on every single job update, permanently defeating the score<=threshold
                    // check (every future submit would be rejected, 0 being an unreachable perfect
                    // score) -- confirmed live: the pool logged a real threshold (4350) for this exact
                    // job while this client printed threshold=0 before this fix.
                    unsigned int threshold = job.threshold;
                    qatum::getUInt(line, "threshold", threshold);

                    if (!jobId.empty() && idStr.size() == 60 && !seedHex.empty() && !anchorHex.empty()) {
                        job.jobId = jobId;
                        job.computorIdStr = idStr;
                        getPublicKeyFromIdentity(idStr.c_str(), job.computorPubkey);
                        qatum::parseHex(seedHex, job.seed, 32);
                        qatum::parseHex(anchorHex, job.anchorDigest, 32);
                        job.threshold = threshold;
                        if (!haveLastSeed || memcmp(job.seed, lastSeed, 32) != 0) {
                            memcpy(lastSeed, job.seed, 32);
                            haveLastSeed = true;
                            seedVersionCounter++;
                        }

                        job.valid = true;
                        job.version = ++jobVersionCounter;
                        job.seedVersion = seedVersionCounter;
                        state.setJob(job);
                        printf("poolpayminer: qubic: new job (threshold=%u)\n", job.threshold);
                    }
                }
                else if (id == qatum::EVENT_SUBMIT) {
                    if (!qatum::isTrue(line, "result")) {
                        std::string err;
                        qatum::getString(line, "error", err);
                        fprintf(stderr, "poolpayminer: qubic: submit rejected: %s\n", err.c_str());
                    }
                    else {
                        printf("poolpayminer: qubic: solution accepted!\n");
                    }
                }
            }

            {
                std::lock_guard<std::mutex> lock(state.submitMutex);
                while (!state.submits.empty()) {
                    const PendingSubmit s = state.submits.front();
                    state.submits.pop();
                    client.submit(s.jobId, s.computorIdStr, qatum::toHex(s.seed, 32), s.nonce, s.claimedScore);
                }
            }

            // Always pace this loop, subscribed or not: without a sleep here, an idle connection
            // (nothing to read, nothing to submit) turns this into a tight select()-only busy loop
            // that pins a CPU core for no reason and showed real EINTR noise under load during
            // testing -- see qatum.h's poll() comment on EINTR for the other half of that fix.
            if (line.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }

        client.close();
        state.invalidateJob();
        if (!shuttingDown()) {
            std::this_thread::sleep_for(std::chrono::seconds(3));
        }
    }

    for (auto &t : workers) {
        t.join();
    }
    liveFetchThread.join();

    exitCode = 0;
    return true;
}


} // namespace qubic
} // namespace xmrig
