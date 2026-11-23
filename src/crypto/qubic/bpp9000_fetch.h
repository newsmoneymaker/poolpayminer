#pragma once

// Live bpp9000 task-data fetch: talks directly to a real Qubic node (the binary peer protocol on
// port 21841, NOT the Qatum pool line protocol) to get the CURRENT first-frame targets via
// REQUEST_BPP9000_TASK_DATA (80)/RESPOND_BPP9000_TASK_DATA (81), the same RPC
// qatum-protocol-src/cpp/fetch_bpp9000_task.cc already uses as a standalone offline tool.
//
// Why this exists: the real Qatum protocol only ever sends topologyHash/dataHash to a client, never
// the task bytes themselves (confirmed: grepped qatum-events.ts/qatum-interface.ts, no raw task-data
// field exists anywhere in it) -- and the underlying data rotates HOURLY (tied to the BTC price feed,
// per qatum-protocol-src's own cpp/q.cc comment). A client with no live-fetch path silently scores
// against a dead task forever once its bundled snapshot goes stale. See
// critical-qubic-task-data-staleness memory for the full investigation; this is the real fix (chosen
// over the cheaper hash-check-and-refuse alternative).
//
// Topology (the ANN wiring) is reused verbatim from whatever task file is currently loaded -- only
// the data block (the target series) actually rotates; see score_task_file's own split in
// qatum-protocol-src and this project's task_file.h.
#include "network.h"
#include "task_file.h"
#include "K12AndKeyUtil.h"
#include "score_bpp9000.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#   include <windows.h>
#else
#   include <cstdio>
#endif

namespace qubic_live_fetch {

constexpr int QUBIC_NODE_PORT = 21841;
constexpr unsigned char REQUEST_SYSTEM_INFO = 46;
constexpr unsigned char RESPOND_SYSTEM_INFO = 47;
constexpr unsigned char REQUEST_BPP9000_TASK_DATA = 80;
constexpr unsigned char RESPOND_BPP9000_TASK_DATA = 81;
constexpr unsigned int BPP9000_TASK_DATA_MAX_TARGETS = 24 * 365 + 24 * 7;
// tick(4) + anchorTick(4) + epoch(2) + valid(1) + reserved(1) + baseHour(4) + shiftLimit(4)
constexpr unsigned int RESPOND_BPP9000_TASK_DATA_HEADER_SIZE = 20;

#pragma pack(push, 1)
struct CurrentSystemInfoMin
{
    short version;
    unsigned short epoch;
    unsigned int tick;
    unsigned int initialTick;
    unsigned int latestCreatedTick;
    unsigned short initialMillisecond;
    unsigned char initialSecond, initialMinute, initialHour, initialDay, initialMonth, initialYear;
    unsigned int numberOfEntities;
    unsigned int numberOfTransactions;
    unsigned char randomMiningSeed[32];
    int solutionThreshold;
};

struct RespondBpp9000TaskDataMin
{
    unsigned int tick;
    unsigned int anchorTick;
    unsigned short epoch;
    unsigned char valid;
    unsigned char reserved;
    unsigned int baseHour;
    unsigned int shiftLimit;
    unsigned char targets[BPP9000_TASK_DATA_MAX_TARGETS];
};
#pragma pack(pop)


// One request/response exchange on an already-connected socket. Reads the fixed 8-byte header first
// (so the exact payload size is known from it), then exactly that many more bytes --
// ServerSocket::receiveData() blocks for an exact byte count, so this never depends on how TCP
// happens to chunk the underlying stream, unlike a "read everything available, then scan for
// packet boundaries" approach. Skips over any other packet type the node sends first (a few rounds
// of chatter is normal), up to a small guard limit.
inline bool requestOnce(ServerSocket &sock, unsigned char requestType, const unsigned char *extraPayload, unsigned int extraPayloadSize,
                         unsigned char expectedResponseType, unsigned char *outPayload, unsigned int outPayloadCapacity, unsigned int &outPayloadSize)
{
    RequestResponseHeader header;
    const unsigned int totalSize = (unsigned int)sizeof(RequestResponseHeader) + extraPayloadSize;
    header.setSize(totalSize);
    header.randomizeDejavu();
    header.setType(requestType);

    std::vector<unsigned char> packet(totalSize);
    memcpy(packet.data(), &header, sizeof(RequestResponseHeader));
    if (extraPayloadSize) {
        memcpy(packet.data() + sizeof(RequestResponseHeader), extraPayload, extraPayloadSize);
    }

    if (!sock.sendData((char *)packet.data(), totalSize)) {
        return false;
    }

    for (int guard = 0; guard < 8; ++guard) {
        RequestResponseHeader respHeader;
        if (!sock.receiveData((char *)&respHeader, sizeof(respHeader))) {
            return false;
        }
        const unsigned int respSize = respHeader.size();
        if (respSize < sizeof(RequestResponseHeader)) {
            return false;
        }
        const unsigned int payloadSize = respSize - (unsigned int)sizeof(RequestResponseHeader);

        if (payloadSize == 0) {
            if (respHeader.type() == expectedResponseType) {
                outPayloadSize = 0;
                return true;
            }
            continue;
        }

        std::vector<unsigned char> payload(payloadSize);
        if (!sock.receiveData((char *)payload.data(), payloadSize)) {
            return false;
        }

        if (respHeader.type() == expectedResponseType) {
            if (payloadSize > outPayloadCapacity) {
                return false;
            }
            memcpy(outPayload, payload.data(), payloadSize);
            outPayloadSize = payloadSize;
            return true;
        }
        // Not the packet we're after (peer-exchange chatter, etc.) -- keep reading.
    }
    return false;
}


// Fetches the current bpp9000 first-frame targets from a real node, builds a complete task file
// (topology copied from existingTaskPath, data block built from the fresh targets -- see this file's
// header comment), and atomically writes it to outTaskPath. On success, outDataHashFirstFrame
// receives K12(targets[0..WINDOW_WIDTH)), the SAME hash the Qatum pool's SubscribePacket reports as
// `dataHash` (see critical-bpp9000-datahash-formula memory) -- NOT the task file's own internal
// dataHash field (that one hashes the packed/padded full data block, a file-format integrity check,
// a different value used only by loadTaskData() to validate the file itself).
inline bool fetchAndWriteLiveTaskFile(const char *nodeIp, const char *existingTaskPath, const char *outTaskPath,
                                       unsigned char outDataHashFirstFrame[32])
{
    ServerSocket sock;
    if (!sock.establishConnection(nodeIp, QUBIC_NODE_PORT)) {
        return false;
    }

    unsigned char sysInfoBuf[256];
    unsigned int sysInfoSize = 0;
    if (!requestOnce(sock, REQUEST_SYSTEM_INFO, nullptr, 0, RESPOND_SYSTEM_INFO, sysInfoBuf, sizeof(sysInfoBuf), sysInfoSize)
        || sysInfoSize < sizeof(CurrentSystemInfoMin)) {
        sock.closeConnection();
        return false;
    }
    const CurrentSystemInfoMin *sysInfo = (const CurrentSystemInfoMin *)sysInfoBuf;
    const unsigned int tick = sysInfo->tick;
    if (tick == 0) {
        sock.closeConnection();
        return false;
    }

    std::vector<unsigned char> taskBuf(sizeof(RespondBpp9000TaskDataMin));
    unsigned int taskPayloadSize = 0;
    const bool gotTaskData = requestOnce(sock, REQUEST_BPP9000_TASK_DATA, (const unsigned char *)&tick, sizeof(tick),
                                          RESPOND_BPP9000_TASK_DATA, taskBuf.data(), (unsigned int)taskBuf.size(), taskPayloadSize);
    sock.closeConnection();
    if (!gotTaskData || taskPayloadSize < RESPOND_BPP9000_TASK_DATA_HEADER_SIZE) {
        return false;
    }

    const RespondBpp9000TaskDataMin *resp = (const RespondBpp9000TaskDataMin *)taskBuf.data();
    const unsigned int targetCount = taskPayloadSize - RESPOND_BPP9000_TASK_DATA_HEADER_SIZE;
    if (!resp->valid || targetCount < score_bpp9000::WINDOW_WIDTH) {
        return false;
    }

    task_file::TaskFileHeader srcHeader;
    if (!task_file::readTaskFileHeader(existingTaskPath, &srcHeader)) {
        return false;
    }
    const unsigned long long topoBytes = task_file::topologyBytes(srcHeader.numInputTrits, srcHeader.numOutputTrits, srcHeader.population, srcHeader.numNeighbors);
    std::vector<unsigned char> topoBlock(topoBytes);
    if (!task_file::readTaskFileBlock(existingTaskPath, sizeof(task_file::TaskFileHeader), topoBlock.data(), topoBytes)) {
        return false;
    }

    const unsigned long long T = score_bpp9000::SEQUENCE_LENGTH;
    const unsigned long long inBytes = task_file::packedBytes(score_bpp9000::NUMBER_OF_INPUT_NEURONS);
    const unsigned long long outBytes = task_file::packedBytes(score_bpp9000::NUMBER_OF_OUTPUT_NEURONS);
    const unsigned long long rowBytes = inBytes + outBytes;
    std::vector<unsigned char> dataBlock((size_t)(T * rowBytes), 0);
    for (unsigned long long t = 0; t < T; ++t) {
        const unsigned char outTrit = (t < targetCount) ? resp->targets[t] : 0;
        // Only the output trit (the actual target value) is real; the input portion of every row
        // stays zero -- loadTaskFromMemory never reads it for scoring, see
        // critical-bpp9000-datahash-formula memory.
        task_file::packTrits(&outTrit, 1, dataBlock.data() + (size_t)(t * rowBytes) + (size_t)(rowBytes - 1));
    }

    task_file::TaskFileHeader outHeader;
    memset(&outHeader, 0, sizeof(outHeader));
    outHeader.magic = task_file::MAGIC;
    outHeader.version = task_file::VERSION;
    outHeader.numInputTrits = (unsigned int)score_bpp9000::NUMBER_OF_INPUT_NEURONS;
    outHeader.numOutputTrits = (unsigned int)score_bpp9000::NUMBER_OF_OUTPUT_NEURONS;
    outHeader.numPairs = T;
    outHeader.population = srcHeader.population;
    outHeader.numNeighbors = srcHeader.numNeighbors;
    KangarooTwelve(topoBlock.data(), (unsigned int)topoBlock.size(), outHeader.topologyHash, task_file::DATA_HASH_SIZE);
    KangarooTwelve(dataBlock.data(), (unsigned int)dataBlock.size(), outHeader.dataHash, task_file::DATA_HASH_SIZE);
    KangarooTwelve(resp->targets, (unsigned int)score_bpp9000::WINDOW_WIDTH, outDataHashFirstFrame, task_file::DATA_HASH_SIZE);

    // Write to a temp path then rename over the real one, so a worker thread reloading mid-write can
    // never observe a half-written file.
    const std::string tmpPath = std::string(outTaskPath) + ".tmp";
    if (!task_file::writeTaskFile(tmpPath.c_str(), outHeader, topoBlock.data(), topoBlock.size(), dataBlock.data(), dataBlock.size())) {
        return false;
    }
#ifdef _WIN32
    return MoveFileExA(tmpPath.c_str(), outTaskPath, MOVEFILE_REPLACE_EXISTING) != 0;
#else
    return rename(tmpPath.c_str(), outTaskPath) == 0;
#endif
}

} // namespace qubic_live_fetch
