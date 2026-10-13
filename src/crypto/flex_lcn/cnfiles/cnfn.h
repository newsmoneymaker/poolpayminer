#pragma once

#include <stddef.h>
#include <stdint.h>
#include "crypto/hash-ops.h"

#define CNFN_PAGE_SIZE          2097152
#define CNFN_ITERATIONS          524288
#define CNFN_AES_ROUNDS          131072

#define CNFN_DARK_PAGE_SIZE      524288
#define CNFN_DARK_ITERATIONS     131072
#define CNFN_DARK_AES_ROUNDS      32768

#define CNFN_DARK_LITE_AES_ROUNDS 16384

#define CNFN_FAST_PAGE_SIZE     2097152
#define CNFN_FAST_ITERATIONS     262144
#define CNFN_FAST_AES_ROUNDS     131072

#define CNFN_LITE_PAGE_SIZE     1048576
#define CNFN_LITE_ITERATIONS     262144
#define CNFN_LITE_AES_ROUNDS      65536

#define CNFN_TURTLE_PAGE_SIZE    262144
#define CNFN_TURTLE_ITERATIONS    65536
#define CNFN_TURTLE_AES_ROUNDS    16384

#define CNFN_TURTLE_LITE_AES_ROUNDS 8192

typedef unsigned char BitSequence;
typedef unsigned long long DataLength;

#ifdef __cplusplus

#include <string>

typedef std::string blobdata;

namespace crypto {

extern "C" {

#pragma pack(push, 1)
  class hash {
    char data[HASH_SIZE];
  };
#pragma pack(pop)

  void lcnflex_cnfn_slow_hash(const char* input, char* output, uint32_t len, int variant, uint32_t page_size, uint32_t iterations, size_t aes_rounds);
  void lcnflex_cnfn_fast_hash(const char* input, char* output, uint32_t len);

//-----------------------------------------------------------------------------------
  // NOTE: these thin wrapper names (cnfn_dark_hash etc, NOT the already-prefixed lcnflex_cnfn_slow_hash/
  // lcnflex_cnfn_fast_hash they call) must ALSO be lcnflex_-prefixed: KCN's own cnfiles/cnfn.h defines
  // identically-named "inline"+"extern C" wrappers with the SAME signature but a DIFFERENT underlying call
  // (its cnfn_slow_hash vs our lcnflex_cnfn_slow_hash) -- extern "C" strips the crypto:: namespace mangling,
  // so without this rename the linker would silently keep only one TU's definition for both coins (the exact
  // collision class described in critical-vendored-lib-symbol-collision).
  inline void lcnflex_cnfn_dark_fast_hash(const char* input, char* output, uint32_t len) {
    lcnflex_cnfn_fast_hash(input, output, len);
  }

  inline void lcnflex_cnfn_darklite_fast_hash(const char* input, char* output, uint32_t len) {
    lcnflex_cnfn_fast_hash(input, output, len);
  }

  inline void lcnflex_cnfn_cnfast_fast_hash(const char* input, char* output, uint32_t len) {
    lcnflex_cnfn_fast_hash(input, output, len);
  }

  inline void lcnflex_cnfn_cnlite_fast_hash(const char* input, char* output, uint32_t len) {
    lcnflex_cnfn_fast_hash(input, output, len);
  }

  inline void lcnflex_cnfn_turtle_fast_hash(const char* input, char* output, uint32_t len) {
    lcnflex_cnfn_fast_hash(input, output, len);
  }

  inline void lcnflex_cnfn_turtlelite_fast_hash(const char* input, char* output, uint32_t len) {
    lcnflex_cnfn_fast_hash(input, output, len);
  }

//-----------------------------------------------------------------------------------
  inline void lcnflex_cnfn_dark_hash(const char* input, char* output, uint32_t len, int variant) {
    lcnflex_cnfn_slow_hash(input, output, len, 1, CNFN_DARK_PAGE_SIZE, CNFN_DARK_ITERATIONS, CNFN_DARK_AES_ROUNDS);
  }

  inline void lcnflex_cnfn_darklite_hash(const char* input, char* output, uint32_t len, int variant) {
    lcnflex_cnfn_slow_hash(input, output, len, 1, CNFN_DARK_PAGE_SIZE, CNFN_DARK_ITERATIONS, CNFN_DARK_LITE_AES_ROUNDS);
  }

  inline void lcnflex_cnfn_cnfast_hash(const char* input, char* output, uint32_t len, int variant) {
    lcnflex_cnfn_slow_hash(input, output, len, 1, CNFN_FAST_PAGE_SIZE, CNFN_FAST_ITERATIONS, CNFN_FAST_AES_ROUNDS);
  }

  inline void lcnflex_cnfn_cnlite_hash(const char* input, char* output, uint32_t len, int variant) {
    lcnflex_cnfn_slow_hash(input, output, len, 1, CNFN_LITE_PAGE_SIZE, CNFN_LITE_ITERATIONS, CNFN_LITE_AES_ROUNDS);
  }

  inline void lcnflex_cnfn_turtle_hash(const char* input, char* output, uint32_t len, int variant) {
    lcnflex_cnfn_slow_hash(input, output, len, 1, CNFN_TURTLE_PAGE_SIZE, CNFN_TURTLE_ITERATIONS, CNFN_TURTLE_AES_ROUNDS);
  }

  inline void lcnflex_cnfn_turtlelite_hash(const char* input, char* output, uint32_t len, int variant) {
    lcnflex_cnfn_slow_hash(input, output, len, 1, CNFN_TURTLE_PAGE_SIZE, CNFN_TURTLE_ITERATIONS, CNFN_TURTLE_LITE_AES_ROUNDS);
  }

} // extern
} // namespace

#endif