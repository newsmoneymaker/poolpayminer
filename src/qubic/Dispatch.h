/* poolpayminer: Qubic dispatch.
 *
 * Qubic's "useful proof of work" (the bpp9000 scorer, vendored under src/crypto/qubic/, see
 * score_bpp9000.h) has nothing to do with any of XMRig's hash algorithms: one attempt is a multi-second
 * to multi-minute CPU walk of a small recurrent ternary network, not a hash compare, so it does not fit
 * XMRig's Algorithm::Id/Job/Pool abstraction (built around millions of cheap hashes/sec against a
 * share-difficulty target). Like Riecoin (see src/riecoin/Dispatch.h), Qubic support is a self-contained
 * side path that main() checks before any of XMRig's own argument parsing or App/Entry startup runs.
 *
 * Unlike Riecoin, there is no third-party binary to wrap: the actual scoring code is vendored C++
 * (src/crypto/qubic/score_bpp9000.h and its dependencies, pulled from the official Qiner reference miner,
 * github.com/qubic/Qiner, Anti-Military License). This module owns a plain TCP/JSON connection to
 * pool-pay.com's OWN Qubic stratum (NOT qubic.li directly -- see qubic-project.md: our pool is a single
 * ordinary qubic.li worker account on the backend, and runs its own standard share-based pool for our
 * customers, exactly like every other coin in this project), a small thread pool that repeatedly calls
 * the vendored scorer's findSolution() against the current job, and submits any solution found back over
 * the same connection.
 *
 * The 1% dev fee is handled exactly like Riecoin's: for 1% of the time this process briefly relaunches
 * ITSELF with "-a rx/0 -o <fee route> -u ... -p ...", which goes through poolpayminer's completely normal
 * startup path (maybeDispatch() returns false for rx/0), then kills that child and resumes Qubic mining.
 *
 * This is checked before any of XMRig's own argument parsing runs, so it changes nothing for every other
 * algorithm: maybeDispatch() returns false immediately unless the algorithm is Qubic's.
 */
#ifndef POOLPAYMINER_QUBIC_DISPATCH_H
#define POOLPAYMINER_QUBIC_DISPATCH_H


namespace xmrig {
namespace qubic {


// Returns true if this run was (or should have been) Qubic mining: main() must return exitCode at once
// without starting XMRig. Returns false to continue starting poolpayminer normally (not a Qubic request).
bool maybeDispatch(int argc, char **argv, int &exitCode);


} // namespace qubic
} // namespace xmrig


#endif /* POOLPAYMINER_QUBIC_DISPATCH_H */
