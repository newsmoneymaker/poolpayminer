// Copyright (c) 2012-2013 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "hash-ops.h"
#include "c_keccak.h"

void lcnflex_hash_permutation(union hash_state *state) {
  lcnflex_keccakf((uint64_t*)state, 24);
}

void lcnflex_hash_process(union hash_state *state, const uint8_t *buf, size_t count) {
  lcnflex_keccak1600(buf, count, (uint8_t*)state);
}

void lcnflex_cn_fast_hash(const void *data, size_t length, char *hash) {
  union hash_state state;
  lcnflex_hash_process(&state, data, length);
  memcpy(hash, &state, HASH_SIZE);
}
