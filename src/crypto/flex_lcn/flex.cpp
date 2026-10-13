#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include "cnfiles/cnfn.h"
#include "sph/extra.h"
#include "sph/sph_blake.h"
#include "sph/sph_bmw.h"
#include "sph/sph_groestl.h"
#include "sph/sph_keccak.h"
#include "sph/sph_skein.h"
#include "sph/sph_luffa.h"
#include "sph/sph_cubehash.h"
#include "sph/sph_shavite.h"
#include "sph/sph_simd.h"
#include "sph/sph_echo.h"
#include "sph/sph_hamsi.h"
#include "sph/sph_fugue.h"
#include "sph/sph_shabal.h"
#include "sph/sph_whirlpool.h"

enum class Algo {
    BLAKE = 0,
    BMW,
    GROESTL,
    KECCAK,
    SKEIN,
    LUFFA,
    CUBEHASH,
    SHAVITE,
    SIMD,
    ECHO,
    HAMSI,
    FUGUE,
    SHABAL,
    WHIRLPOOL,
    HASH_FUNC_COUNT
};

enum class CNFNAlgo {
    CNFNDark = 0,
    CNFNDarklite,
    CNFNFast,
    CNFNLite,
    CNFNTurtle,
    CNFNTurtlelite,
    CNFN_HASH_FUNC_COUNT
};

void lcnflex_selectAlgo(unsigned char nibble, bool* selectedAlgos, uint8_t* selectedIndex, int algoCount, int& currentCount) {
    uint8_t algoDigit = (nibble & 0x0F) % algoCount;
    if (!selectedAlgos[algoDigit]) {
        selectedAlgos[algoDigit] = true;
        selectedIndex[currentCount] = algoDigit;
        currentCount++;
    }
    algoDigit = (nibble >> 4) % algoCount;
    if (!selectedAlgos[algoDigit]) {
        selectedAlgos[algoDigit] = true;
        selectedIndex[currentCount] = algoDigit;
        currentCount++;
    }
}

void lcnflex_getAlgoString(void* mem, unsigned int size, uint8_t* selectedAlgoOutput, int algoCount) {
    unsigned char* p = static_cast<unsigned char*>(mem);
    unsigned int len = size / 2;
    bool selectedAlgo[15] = { false };
    int selectedCount = 0;

    for (unsigned int i = 0; i < len; ++i) {
        lcnflex_selectAlgo(p[i], selectedAlgo, selectedAlgoOutput, algoCount, selectedCount);
        if (selectedCount == algoCount) {
            break;
        }
    }

    if (selectedCount < algoCount) {
        for (uint8_t i = 0; i < algoCount; ++i) {
            if (!selectedAlgo[i]) {
                selectedAlgoOutput[selectedCount] = i;
                selectedCount++;
            }
        }
    }
}

void lcnflex_print_hex_memory(void* mem, unsigned int size) {
    unsigned char* p = static_cast<unsigned char*>(mem);
    unsigned int len = size / 2;

    for (unsigned int i = 0; i < len; ++i) {
        printf("%02x", p[len - i - 1]);
    }
    printf("\n");
}

void lcnflex_hash(const char* input, int size, unsigned char* output) {
    uint32_t hash[64 / 4];
    sph_blake512_context ctx_blake;
    sph_bmw512_context ctx_bmw;
    sph_groestl512_context ctx_groestl;
    sph_keccak512_context ctx_keccak;
    sph_skein512_context ctx_skein;
    sph_luffa512_context ctx_luffa;
    sph_cubehash512_context ctx_cubehash;
    sph_shavite512_context ctx_shavite;
    sph_simd512_context ctx_simd;
    sph_echo512_context ctx_echo;
    sph_hamsi512_context ctx_hamsi;
    sph_fugue512_context ctx_fugue;
    sph_shabal512_context ctx_shabal;
    sph_whirlpool_context ctx_whirlpool;

    void* in = const_cast<void*>(static_cast<const void*>(input));
    lcnflex_sph_keccak512_init(&ctx_keccak);
    lcnflex_sph_keccak512(&ctx_keccak, in, size);
    lcnflex_sph_keccak512_close(&ctx_keccak, hash);

    uint8_t selectedAlgoOutput[15] = { 0 };
    uint8_t selectedCNAlgoOutput[6] = { 0 };
    lcnflex_getAlgoString(&hash, 64, selectedAlgoOutput, static_cast<int>(Algo::HASH_FUNC_COUNT));
    lcnflex_getAlgoString(&hash, 64, selectedCNAlgoOutput, static_cast<int>(CNFNAlgo::CNFN_HASH_FUNC_COUNT));

    for (int i = 0; i < 18; ++i) {
        uint8_t algo;
        uint8_t cnAlgo;
        int coreSelection;
        int cnSelection = -1;

        if (i < 5) {
            coreSelection = i;
        }
        else if (i < 11) {
            coreSelection = i - 1;
        }
        else {
            coreSelection = i - 2;
        }

        if (i == 5) {
            coreSelection = -1;
            cnSelection = 0;
        }
        if (i == 11) {
            coreSelection = -1;
            cnSelection = 1;
        }
        if (i == 17) {
            coreSelection = -1;
            cnSelection = 2;
        }

        if (coreSelection >= 0) {
            algo = selectedAlgoOutput[static_cast<uint8_t>(coreSelection)];
        }
        else {
            algo = static_cast<uint8_t>(Algo::HASH_FUNC_COUNT) + 1; // skip core hashing for this loop iteration
        }

        if (cnSelection >= 0) {
            cnAlgo = selectedCNAlgoOutput[static_cast<uint8_t>(cnSelection)];
        }
        else {
            cnAlgo = static_cast<uint8_t>(CNFNAlgo::CNFN_HASH_FUNC_COUNT) + 1; // skip cn hashing for this loop iteration
        }

        // selection cnAlgo. if a CN algo is selected then core algo will not be selected
        switch (static_cast<CNFNAlgo>(cnAlgo)) {
            case CNFNAlgo::CNFNDark:
                crypto::lcnflex_cnfn_dark_hash((char*)in, (char*)hash, size, 1);
                break;
            case CNFNAlgo::CNFNDarklite:
                crypto::lcnflex_cnfn_darklite_hash((char*)in, (char*)hash, size, 1);
                break;
            case CNFNAlgo::CNFNFast:
                crypto::lcnflex_cnfn_cnfast_hash((char*)in, (char*)hash, size, 1);
                break;
            case CNFNAlgo::CNFNLite:
                crypto::lcnflex_cnfn_cnlite_hash((char*)in, (char*)hash, size, 1);
                break;
            case CNFNAlgo::CNFNTurtle:
                crypto::lcnflex_cnfn_turtle_hash((char*)in, (char*)hash, size, 1);
                break;
            case CNFNAlgo::CNFNTurtlelite:
                crypto::lcnflex_cnfn_turtlelite_hash((char*)in, (char*)hash, size, 1);
                break;
            default:
                break;
        }

        // selection core algo
        switch (static_cast<Algo>(algo)) {
            case Algo::BLAKE:
                lcnflex_sph_blake512_init(&ctx_blake);
                lcnflex_sph_blake512(&ctx_blake, in, size);
                lcnflex_sph_blake512_close(&ctx_blake, hash);
                break;
            case Algo::BMW:
                lcnflex_sph_bmw512_init(&ctx_bmw);
                lcnflex_sph_bmw512(&ctx_bmw, in, size);
                lcnflex_sph_bmw512_close(&ctx_bmw, hash);
                break;
            case Algo::GROESTL:
                lcnflex_sph_groestl512_init(&ctx_groestl);
                lcnflex_sph_groestl512(&ctx_groestl, in, size);
                lcnflex_sph_groestl512_close(&ctx_groestl, hash);
                break;
            case Algo::KECCAK:
                lcnflex_sph_keccak512_init(&ctx_keccak);
                lcnflex_sph_keccak512(&ctx_keccak, in, size);
                lcnflex_sph_keccak512_close(&ctx_keccak, hash);
                break;
            case Algo::SKEIN:
                lcnflex_sph_skein512_init(&ctx_skein);
                lcnflex_sph_skein512(&ctx_skein, in, size);
                lcnflex_sph_skein512_close(&ctx_skein, hash);
                break;
            case Algo::LUFFA:
                lcnflex_sph_luffa512_init(&ctx_luffa);
                lcnflex_sph_luffa512(&ctx_luffa, in, size);
                lcnflex_sph_luffa512_close(&ctx_luffa, hash);
                break;
            case Algo::CUBEHASH:
                lcnflex_sph_cubehash512_init(&ctx_cubehash);
                lcnflex_sph_cubehash512(&ctx_cubehash, in, size);
                lcnflex_sph_cubehash512_close(&ctx_cubehash, hash);
                break;
            case Algo::SHAVITE:
                lcnflex_sph_shavite512_init(&ctx_shavite);
                lcnflex_sph_shavite512(&ctx_shavite, in, size);
                lcnflex_sph_shavite512_close(&ctx_shavite, hash);
                break;
            case Algo::SIMD:
                lcnflex_sph_simd512_init(&ctx_simd);
                lcnflex_sph_simd512(&ctx_simd, in, size);
                lcnflex_sph_simd512_close(&ctx_simd, hash);
                break;
            case Algo::ECHO:
                lcnflex_sph_echo512_init(&ctx_echo);
                lcnflex_sph_echo512(&ctx_echo, in, size);
                lcnflex_sph_echo512_close(&ctx_echo, hash);
                break;
            case Algo::HAMSI:
                lcnflex_sph_hamsi512_init(&ctx_hamsi);
                lcnflex_sph_hamsi512(&ctx_hamsi, in, size);
                lcnflex_sph_hamsi512_close(&ctx_hamsi, hash);
                break;
            case Algo::FUGUE:
                lcnflex_sph_fugue512_init(&ctx_fugue);
                lcnflex_sph_fugue512(&ctx_fugue, in, size);
                lcnflex_sph_fugue512_close(&ctx_fugue, hash);
                break;
            case Algo::SHABAL:
                lcnflex_sph_shabal512_init(&ctx_shabal);
                lcnflex_sph_shabal512(&ctx_shabal, in, size);
                lcnflex_sph_shabal512_close(&ctx_shabal, hash);
                break;
            case Algo::WHIRLPOOL:
                lcnflex_sph_whirlpool_init(&ctx_whirlpool);
                lcnflex_sph_whirlpool(&ctx_whirlpool, in, size);
                lcnflex_sph_whirlpool_close(&ctx_whirlpool, hash);
                break;
            default:
                break;
        }

        in = static_cast<void*>(hash);
        size = 64;
    }

    lcnflex_sph_keccak256_init(&ctx_keccak);
    lcnflex_sph_keccak256(&ctx_keccak, in, size);
    lcnflex_sph_keccak256_close(&ctx_keccak, hash);
    std::memcpy(output, hash, 32);
}
