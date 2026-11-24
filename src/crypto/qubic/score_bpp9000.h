#pragma once

#include "score_common.h"
#include "K12AndKeyUtil.h"
#include "task_file.h"

#include <cassert>
#include <cstring>
#include <vector>

// bpp9000: rolling-shift-window "ant colony" scorer. Ported 2026-11-24 directly from qubic/core's own
// current production reference (qatum-protocol-src/cpp/mining/score_bpp9000.hpp, confirmed bit-exact
// against the live network there -- see critical-qatum-addon-old-algorithm-version memory).
// REPLACES this file's own earlier, pre-EP240 implementation (population=64, windowWidth=672, no
// shift/advanceThreshold mechanic at all, wiring read from the task file's topology block) -- that
// version was never updated when qatum-protocol-src's own native addon was, so every real submission
// through it verified false against the live network. Found 2026-11-24 from two real solutions, both
// locally "valid", both rejected by the pool's verify thread as "is solution false".
//
// Key structural differences from the old version, for anyone diffing against it later:
//  - EVERY neuron (not just "input"/"output"/"signal" roles) has 3 neighbours and its own 27-entry
//    LUT; wiring + initial values are derived from a root-material draw (seeded by the identity's
//    public key), NEVER read from the task file. The task file's topology block is accordingly
//    unused by this scorer now (kept in the file format only for backward file-shape compatibility).
//  - The task file's data block supplies ONLY targetOutputs[] (the real target series); controlIndex/
//    outputIndex (the "clock" and graded-output neurons) are derived from the mining seed instead.
//  - score() grades a single WINDOW_WIDTH-wide frame starting at `shift`, not the whole sequence.
//  - advanceShift(): a frame that stays under BPP9000_ADVANCE_THRESHOLD errors can push shift forward
//    (try the next window) up to shiftLimit; Rating{error, shift} ranks a higher shift as strictly
//    better regardless of error (isBetterThan), and equal shift breaks ties on lower error.
namespace score_bpp9000
{

static constexpr unsigned long long NUMBER_OF_INPUT_NEURONS = 18;
static constexpr unsigned long long NUMBER_OF_OUTPUT_NEURONS = 1;
static constexpr unsigned long long POPULATION_THRESHOLD = 1024;
static constexpr unsigned long long NUMBER_OF_NEIGHBORS = 3;
static constexpr unsigned long long NUMBER_OF_MUTATIONS = 1000;
static constexpr unsigned long long MAX_NUMBER_OF_TICKS = 100000;

static constexpr unsigned long long SEQUENCE_LENGTH = 24 * 365 + 24 * 7;   // 8928
static constexpr unsigned long long WINDOW_WIDTH = 24 * 365;              // 8760
static constexpr unsigned long long NUMBER_OF_WINDOWS = SEQUENCE_LENGTH - WINDOW_WIDTH;
static constexpr unsigned long long SHIFT_CAP = 24 * 7;                  // 168
static constexpr unsigned int ADVANCE_THRESHOLD = 3504;
static constexpr unsigned int SOLUTION_THRESHOLD = 4350;                 // BPP9000_SOLUTION_THRESHOLD_DEFAULT; the pool's own job.threshold is authoritative at runtime

static constexpr unsigned char MODE_START = 1;
static constexpr unsigned char MODE_WIRING = 2;
static constexpr unsigned char MODE_LUT = 3;
static constexpr unsigned int MAX_CHANGES_PER_STEP = 10;

template <
    unsigned long long numberOfInputNeurons,
    unsigned long long numberOfOutputNeurons,
    unsigned long long sequenceLength,
    unsigned long long windowWidth,
    unsigned long long maxNumberOfTicks,
    unsigned long long numberOfNeighbors,
    unsigned long long populationThreshold,
    unsigned long long numberOfMutations,
    unsigned int solutionThreshold>
struct Miner
{
    static constexpr unsigned long long maxNumberOfNeurons = populationThreshold;
    static constexpr unsigned long long numberOfWindows = sequenceLength - windowWidth;
    static constexpr unsigned long long shiftCap = NUMBER_OF_WINDOWS < SHIFT_CAP ? NUMBER_OF_WINDOWS : SHIFT_CAP;
    static constexpr unsigned long long lutSize = 27;
    static constexpr unsigned int advanceThreshold = ADVANCE_THRESHOLD;
    static constexpr unsigned long long numberOfLinks = populationThreshold * numberOfNeighbors;

    static constexpr unsigned int INVALID_SCORE_VALUE = 0xFFFFFFFFU;
    static constexpr unsigned int INFINITE_ERROR = 0xFFFFFFFFU;

    static_assert(numberOfNeighbors == 3, "the LUT index is hardcoded for 3 neighbours");
    static_assert(numberOfOutputNeurons == 1, "score() grades only output neuron 0");
    static_assert(numberOfWindows >= 1 && numberOfWindows < sequenceLength, "the emit count must be positive and within the target sequence");
    static_assert(maxNumberOfTicks > shiftCap + windowWidth, "maxNumberOfTicks must exceed the deepest emit count so all emits can fit");
    static_assert(shiftCap >= 1 && shiftCap <= numberOfWindows, "shiftCap must keep the last frame inside the data");
    static_assert(populationThreshold <= 65536, "the transfer index is 16-bit");

    // What a solution is judged by -- matches qubic/core's src/mining/rating.h exactly.
    struct Rating
    {
        static constexpr unsigned int INVALID_ERROR = 0xFFFFFFFFU;
        unsigned int error;
        unsigned int shift;

        bool isValid() const { return error != INVALID_ERROR; }
        bool isBetterThan(const Rating &other) const
        {
            if (shift != other.shift) return shift > other.shift;
            return error < other.error;
        }
        bool errorWorseOrEqual(const Rating &other) const { return error >= other.error; }
        bool isNotWorseThan(const Rating &other) const { return !other.isBetterThan(*this); }
        static Rating worst()
        {
            Rating r; r.error = INVALID_ERROR; r.shift = 0; return r;
        }
    };

    // The inheritable per-identity state: wiring/LUT/initial values. Root-only (protocol v2): never
    // put on the wire, so no canonical-layout conversion is needed here (see Dispatch.cpp's header).
    struct ANN
    {
        unsigned char initialNeuronValues[maxNumberOfNeurons];
        unsigned int neighbor[numberOfLinks];
        unsigned char lut[maxNumberOfNeurons * lutSize];
    };

    struct RootMaterial
    {
        unsigned char lut[maxNumberOfNeurons * lutSize];
        unsigned char start[maxNumberOfNeurons];
        unsigned long long wire[numberOfLinks];
    };

    unsigned int controlIndex = 0;
    unsigned int outputIndex = 0;
    unsigned char targetOutputs[sequenceLength] = {};

    unsigned char curInitial[maxNumberOfNeurons];
    unsigned int neighborIndices[numberOfLinks];
    unsigned char curLut[maxNumberOfNeurons * lutSize];

    unsigned char prevInitial[maxNumberOfNeurons];
    unsigned int prevNeighborIndices[numberOfLinks];
    unsigned char prevLut[maxNumberOfNeurons * lutSize];
    unsigned char bestInitial[maxNumberOfNeurons];
    unsigned int bestNeighborIndices[numberOfLinks];
    unsigned char bestLut[maxNumberOfNeurons * lutSize];

    unsigned char neuronOut[maxNumberOfNeurons];
    unsigned char neuronPrev[maxNumberOfNeurons];

    unsigned long long shift = 0;
    unsigned long long shiftLimit = shiftCap;
    Rating parentRating = Rating::worst();

    RootMaterial rootMaterial;
    static constexpr unsigned long long mutationSeedCount = numberOfMutations * MAX_CHANGES_PER_STEP;
    static constexpr unsigned long long mutationSeedPaddedCount =
        ((mutationSeedCount * sizeof(unsigned long long) + 63) / 64) * 64 / sizeof(unsigned long long);
    unsigned long long mutationSeed[mutationSeedPaddedCount];

    std::vector<unsigned char> poolVec;
    const unsigned char *pRandom2Pool = nullptr;

    void setPool(const unsigned char *pool) { pRandom2Pool = pool; }

    bool initialize(unsigned char *miningSeed, const char *taskFilePath)
    {
        poolVec.resize(POOL_VEC_PADDING_SIZE);
        generateRandom2Pool(miningSeed, poolVec.data());
        pRandom2Pool = poolVec.data();
        // controlIndex/outputIndex are derived from the SAME mining seed (the epoch digest), shared
        // by every identity -- must be set before any score() call, same timing as upstream's
        // ScoreFunction::initMiningData() -> deriveControlOutput().
        deriveControlOutput(miningSeed);
        return loadTaskData(taskFilePath);
    }

    // Only the data block's target column matters now (wiring is root-material-derived, not read
    // from the file) -- still validates magic/version/dimensions/hashes so a corrupt or genuinely
    // wrong-shaped file is rejected rather than silently scoring garbage.
    bool loadTaskData(const char *taskFilePath)
    {
        task_file::TaskFileHeader header;
        if (!task_file::readTaskFileHeader(taskFilePath, &header)) return false;
        if (header.magic != task_file::MAGIC || header.version != task_file::VERSION ||
            header.numInputTrits != numberOfInputNeurons || header.numOutputTrits != numberOfOutputNeurons ||
            header.numPairs < sequenceLength)
            return false;

        const unsigned long long topoBytes = task_file::topologyBytes(
            header.numInputTrits, header.numOutputTrits, header.population, header.numNeighbors);
        const unsigned long long dataBytes = task_file::dataBytes(numberOfInputNeurons, numberOfOutputNeurons, sequenceLength);

        std::vector<unsigned char> dataBlockBuf(dataBytes);
        if (!task_file::readTaskFileBlock(taskFilePath, sizeof(task_file::TaskFileHeader) + topoBytes, dataBlockBuf.data(), dataBytes))
            return false;

        return loadTaskFromMemory(nullptr, dataBlockBuf.data());
    }

    bool loadTaskFromMemory(const unsigned char * /*topoBlock*/, const unsigned char *dataBlock)
    {
        const unsigned long long inBytes = task_file::packedBytes(numberOfInputNeurons);
        const unsigned long long outBytes = task_file::packedBytes(numberOfOutputNeurons);
        const unsigned long long rowBytes = inBytes + outBytes;
        for (unsigned long long t = 0; t < sequenceLength; ++t)
        {
            unsigned char outTrit[numberOfOutputNeurons];
            if (!task_file::unpackTrits(dataBlock + t * rowBytes + inBytes, numberOfOutputNeurons, outTrit))
                return false;
            targetOutputs[t] = outTrit[0];
        }
        return true;
    }

    void deriveControlOutput(const unsigned char *digest)
    {
        unsigned char seedHash[32];
        KangarooTwelve(digest, 32, seedHash, 32);
        unsigned long long material[8];
        unsigned char seedCopy[32];
        memcpy(seedCopy, seedHash, 32);
        random2(seedCopy, pRandom2Pool, (unsigned char *)material, sizeof(material));
        controlIndex = (unsigned int)(material[0] % populationThreshold);
        unsigned int output = (unsigned int)(material[1] % populationThreshold);
        if (output == controlIndex) output = (unsigned int)((output + 1) % populationThreshold);
        outputIndex = output;
    }

    static constexpr unsigned char TRIT_UNKNOWN = 2;

    unsigned int score()
    {
        for (unsigned long long n = 0; n < populationThreshold; ++n) neuronOut[n] = curInitial[n];

        unsigned int failures = 0;
        unsigned long long counter = 0;
        unsigned long long ticks = 0;
        while (counter < shift + windowWidth)
        {
            if (++ticks >= maxNumberOfTicks) return INFINITE_ERROR;

            memcpy(neuronPrev, neuronOut, sizeof(neuronPrev));
            for (unsigned long long n = 0; n < populationThreshold; ++n)
            {
                const unsigned long long t0 = neuronPrev[neighborIndices[n * numberOfNeighbors + 0]];
                const unsigned long long t1 = neuronPrev[neighborIndices[n * numberOfNeighbors + 1]];
                const unsigned long long t2 = neuronPrev[neighborIndices[n * numberOfNeighbors + 2]];
                neuronOut[n] = curLut[n * lutSize + (t0 + 3 * t1 + 9 * t2)];
            }

            if (neuronOut[controlIndex] != TRIT_UNKNOWN)
            {
                if (counter >= shift && neuronOut[outputIndex] != targetOutputs[counter]) failures++;
                counter++;
            }
        }
        return failures;
    }

    void mutateStartState(unsigned long long seed)
    {
        const unsigned long long delta = seed & 1ULL;
        const unsigned long long n = (seed >> 1) % populationThreshold;
        curInitial[n] = (unsigned char)((curInitial[n] + 1 + delta) % 3);
    }

    void mutateWiring(unsigned long long seed)
    {
        const unsigned long long flatSlot = seed % numberOfLinks;
        unsigned int target = (unsigned int)((seed / numberOfLinks) % populationThreshold);
        const unsigned int previous = neighborIndices[flatSlot];
        while (target == previous) target = (unsigned int)((target + 1) % populationThreshold);
        neighborIndices[flatSlot] = target;
    }

    void mutateLut(unsigned long long seed)
    {
        const unsigned long long delta = seed & 1ULL;
        const unsigned long long flatIdx = (seed >> 1) % (maxNumberOfNeurons * lutSize);
        curLut[flatIdx] = (unsigned char)((curLut[flatIdx] + 1 + delta) % 3);
    }

    void mutate(unsigned char mode, unsigned long long seed)
    {
        if (mode == MODE_START) mutateStartState(seed);
        else if (mode == MODE_WIRING) mutateWiring(seed);
        else if (mode == MODE_LUT) mutateLut(seed);
    }

    void deriveRootMaterial(const unsigned char *seed)
    {
        unsigned char rootHash[32];
        KangarooTwelve(seed, 32, rootHash, 32);
        random2(rootHash, pRandom2Pool, (unsigned char *)&rootMaterial, sizeof(rootMaterial));
    }

    void deriveMutationSeeds(const unsigned char *publicKey, const unsigned char *nonce, const unsigned char *anchorTickDigest)
    {
        unsigned char searchHash[32];
        unsigned char combined[96];
        memcpy(combined, publicKey, 32);
        memcpy(combined + 32, nonce, 32);
        combined[32] = 0; combined[33] = 0; combined[34] = 0;
        unsigned int combinedSize = 64;
        if (anchorTickDigest != nullptr)
        {
            memcpy(combined + 64, anchorTickDigest, 32);
            combinedSize = 96;
        }
        KangarooTwelve(combined, combinedSize, searchHash, 32);
        random2(searchHash, pRandom2Pool, (unsigned char *)&mutationSeed, sizeof(mutationSeed));
    }

    void applyRootMaterial()
    {
        for (unsigned long long i = 0; i < maxNumberOfNeurons * lutSize; ++i)
            curLut[i] = (unsigned char)(rootMaterial.lut[i] % 3);
        for (unsigned long long n = 0; n < populationThreshold; ++n)
            curInitial[n] = (unsigned char)(rootMaterial.start[n] % 3);
        for (unsigned long long i = 0; i < numberOfLinks; ++i)
            neighborIndices[i] = (unsigned int)(rootMaterial.wire[i] % populationThreshold);
    }

    void loadAnn(const ANN &ann)
    {
        memcpy(curInitial, ann.initialNeuronValues, sizeof(curInitial));
        memcpy(neighborIndices, ann.neighbor, sizeof(neighborIndices));
        memcpy(curLut, ann.lut, sizeof(curLut));
    }

    void snapshotPrev()
    {
        memcpy(prevInitial, curInitial, sizeof(prevInitial));
        memcpy(prevNeighborIndices, neighborIndices, sizeof(prevNeighborIndices));
        memcpy(prevLut, curLut, sizeof(prevLut));
    }
    void rollbackPrev()
    {
        memcpy(curInitial, prevInitial, sizeof(curInitial));
        memcpy(neighborIndices, prevNeighborIndices, sizeof(neighborIndices));
        memcpy(curLut, prevLut, sizeof(curLut));
    }
    void snapshotBest()
    {
        memcpy(bestInitial, curInitial, sizeof(bestInitial));
        memcpy(bestNeighborIndices, neighborIndices, sizeof(bestNeighborIndices));
        memcpy(bestLut, curLut, sizeof(bestLut));
    }

    static unsigned int changesPerStep(const unsigned char *nonce) { return nonce[1] & 0x0F; }
    static unsigned char modeOf(const unsigned char *nonce) { return (unsigned char)((nonce[1] >> 4) & 0x03); }

    // Matches the pool's own isCanonicalAntNonce exactly: algo id, L in [1, MAX_CHANGES_PER_STEP],
    // mode == WIRING, top two bits of nonce[1] clear, K <= numberOfMutations.
    static bool isCanonicalAntNonce(const unsigned char *nonce)
    {
        if (nonce[0] != 1) return false;
        const unsigned int L = nonce[1] & 0x0F;
        const unsigned char mode = (unsigned char)((nonce[1] >> 4) & 0x03);
        if (L < 1 || L > MAX_CHANGES_PER_STEP || mode != MODE_WIRING || (nonce[1] & 0xC0) != 0) return false;
        return (unsigned long long)nonce[2] <= numberOfMutations;
    }

    Rating advanceShift()
    {
        for (;;)
        {
            const unsigned int frameError = score();
            if (frameError > advanceThreshold) return Rating{frameError, (unsigned int)shift};
            if (shift >= shiftLimit) return Rating{frameError, (unsigned int)shift};
            shift++;
        }
    }

    Rating computeScoreFromCurrent(unsigned int L, unsigned long long K, unsigned char mode)
    {
        Rating cur = advanceShift();
        Rating best = cur;
        parentRating = cur;
        snapshotBest();

        for (unsigned long long s = 0; s < numberOfMutations; ++s)
        {
            snapshotPrev();
            const unsigned long long prevShift = shift;

            for (unsigned int i = 0; i < L; ++i)
                mutate(mode, mutationSeed[s * MAX_CHANGES_PER_STEP + i]);

            const Rating r = advanceShift();

            if (s < K)
            {
                if (r.isValid() && r.errorWorseOrEqual(cur)) cur = r;
                else { rollbackPrev(); shift = prevShift; }
            }
            else
            {
                if (r.isValid() && r.isNotWorseThan(cur)) cur = r;
                else { rollbackPrev(); shift = prevShift; }

                if (cur.isValid() && cur.isNotWorseThan(best)) { best = cur; snapshotBest(); }
            }
        }

        shift = best.shift;
        return best;
    }

    Rating computeScoreFromParentShift(unsigned long long parentShift, unsigned long long limit, unsigned int L, unsigned long long K, unsigned char mode)
    {
        parentRating = Rating::worst();
        if (limit > shiftCap || parentShift > limit) return Rating::worst();
        shiftLimit = limit;
        shift = parentShift;
        return computeScoreFromCurrent(L, K, mode);
    }

    void deriveRootANN(const unsigned char *publicKey, ANN &out)
    {
        deriveRootMaterial(publicKey);
        applyRootMaterial();
        memcpy(out.initialNeuronValues, curInitial, sizeof(curInitial));
        memcpy(out.neighbor, neighborIndices, sizeof(neighborIndices));
        memcpy(out.lut, curLut, sizeof(curLut));
    }

    void getBestANN(ANN &out)
    {
        memcpy(out.initialNeuronValues, bestInitial, sizeof(bestInitial));
        memcpy(out.neighbor, bestNeighborIndices, sizeof(bestNeighborIndices));
        memcpy(out.lut, bestLut, sizeof(bestLut));
    }

    // Root-only (protocol v2): parentShift is always 0, the root's shift is 0 by definition.
    unsigned int computeScoreFromParent(const ANN &parentAnn, const unsigned char *publicKey,
        const unsigned char *nonce, const unsigned char *anchorTickDigest)
    {
        if (!isCanonicalAntNonce(nonce)) return INVALID_SCORE_VALUE;
        loadAnn(parentAnn);
        deriveMutationSeeds(publicKey, nonce, anchorTickDigest);

        const unsigned int L = changesPerStep(nonce);
        const unsigned long long K = nonce[2];
        const unsigned char mode = modeOf(nonce);

        const Rating r = computeScoreFromParentShift(0, shiftCap, L, K, mode);
        return r.isValid() ? r.error : INVALID_SCORE_VALUE;
    }
};

} // namespace score_bpp9000
