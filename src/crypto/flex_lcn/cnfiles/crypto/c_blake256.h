#ifndef _BLAKE256_H_
#define _BLAKE256_H_

#include <stdint.h>

typedef struct {
  uint32_t h[8], s[4], t[2];
  int buflen, nullt;
  uint8_t buf[64];
} state;

typedef struct {
  state inner;
  state outer;
} hmac_state;

void lcnflex_blake256_init(state *);
void lcnflex_blake224_init(state *);

void lcnflex_blake256_update(state *, const uint8_t *, uint64_t);
void lcnflex_blake224_update(state *, const uint8_t *, uint64_t);

void lcnflex_blake256_final(state *, uint8_t *);
void lcnflex_blake224_final(state *, uint8_t *);

void lcnflex_blake256_hash(uint8_t *, const uint8_t *, uint64_t);
void lcnflex_blake224_hash(uint8_t *, const uint8_t *, uint64_t);

/* HMAC functions: */

void lcnflex_hmac_blake256_init(hmac_state *, const uint8_t *, uint64_t);
void lcnflex_hmac_blake224_init(hmac_state *, const uint8_t *, uint64_t);

void lcnflex_hmac_blake256_update(hmac_state *, const uint8_t *, uint64_t);
void lcnflex_hmac_blake224_update(hmac_state *, const uint8_t *, uint64_t);

void lcnflex_hmac_blake256_final(hmac_state *, uint8_t *);
void lcnflex_hmac_blake224_final(hmac_state *, uint8_t *);

void lcnflex_hmac_blake256_hash(uint8_t *, const uint8_t *, uint64_t, const uint8_t *, uint64_t);
void lcnflex_hmac_blake224_hash(uint8_t *, const uint8_t *, uint64_t, const uint8_t *, uint64_t);

#endif /* _BLAKE256_H_ */
