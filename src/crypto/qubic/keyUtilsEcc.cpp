// getPublicKeyFromPrivateKey, split out of keyUtils.cpp on 2026-11-24: the only key-derivation function
// that needs FourQ elliptic-curve math (ecc_mul_fixed/encode), hence the only one that needs this
// translation unit's ADX/BMI2/RDRAND compile flags. See FourQCurve.h's comment for the full story.
// poolpayminer's Qubic mode is direct-address only (no custodial seed/private-key path), so in normal
// operation this function is never actually called -- it exists for API completeness / a future
// custodial mode, not on any current mining path.
#include "FourQCurve.h"
#include "keyUtils.h"

void getPublicKeyFromPrivateKey(const unsigned char* privateKey, unsigned char* publicKey)
{
    point_t P;
    ecc_mul_fixed((unsigned long long*)privateKey, P); // Compute public key
    encode(P, publicKey);
}
