/* poolpayminer: Scala (XLA, "Panthera" / DefyX) input hash of RandomX.
 *
 * The node's library (Panthera) starts every hash with
 *     tempHash = blake2b(input, 64 bytes)
 *     yespower_hash(tempHash): yespower 1.0, N = 2048, r = 8, no personalisation, 32 bytes written over tempHash[0..31]
 *     k12(tempHash):           KangarooTwelve of all 64 bytes, 32 bytes written over tempHash[0..31]
 * and then fills the scratchpad from the 64 bytes as usual. This file is the last two steps; the blake2b is done by the caller.
 * yespower and KangarooTwelve are the sources of the node's library (BSD and CC0/MIT style licences, see the headers).
 */
#ifndef POOLPAYMINER_XLA_HASH_H
#define POOLPAYMINER_XLA_HASH_H

#ifdef __cplusplus
extern "C" {
#endif

void rx_xla_finish_input_hash(void *tempHash64);

#ifdef __cplusplus
}
#endif

#endif
