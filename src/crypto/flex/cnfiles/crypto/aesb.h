#pragma once

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef void (*aesb_single_round_fn)(const uint8_t *in, uint8_t *out,
                                      const uint8_t *expandedKey);
typedef void (*aesb_pseudo_round_x8_fn)(const uint8_t *in, uint8_t *out,
                                         const uint8_t *expandedKey);
typedef void (*aesb_pseudo_round_x8_xor_fn)(uint8_t *inout,
                                             const uint8_t *xorData,
                                             const uint8_t *expandedKey);

/* Original scalar ABI. These always remain available. */
void aesb_single_round(const uint8_t *in, uint8_t *out, uint8_t *expandedKey);
void aesb_pseudo_round(const uint8_t *in, uint8_t *out, uint8_t *expandedKey);

/* Resolve once per CNFN invocation, then call the returned function directly. */
aesb_single_round_fn aesb_get_single_round_fn(void);
aesb_pseudo_round_x8_fn aesb_get_pseudo_round_x8_fn(void);
aesb_pseudo_round_x8_xor_fn aesb_get_pseudo_round_x8_xor_fn(void);
int aesb_has_hardware_aes(void);
const char *aesb_backend_name(void);

#if defined(__cplusplus)
}
#endif
