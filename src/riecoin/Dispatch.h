/* poolpayminer: Riecoin dispatch.
 *
 * Riecoin's proof of work has nothing to do with RandomX (it looks for constellations of prime numbers, using
 * GMP): it cannot run on XMRig's CPU backend. The Riecoin team's own miner, rieMiner, does that job, and its
 * source is public (MIT-style licence, https://github.com/RiecoinTeam/rieMiner). This wrapper is what makes
 * poolpayminer "one miner" from the user's point of view: given -a ric (or a config.json pool with "algo": "ric"),
 * it turns -o/-u/-p into a rieMiner.conf and runs the copy of rieMiner shipped next to this executable, so the
 * same poolpayminer(.exe) the user already has starts the right engine either way.
 *
 * It also carries the same fee that every other algorithm pays (see net/strategies/DonateStrategy.h /
 * FeeTable.h): 1% of the time, rieMiner is paused and this same binary mines RandomX on the operator's CPU fee
 * route instead, exactly like the ordinary XMRig donate mechanism does by switching connections for CPU/GPU
 * miners of every other coin. rieMiner is not linked into poolpayminer (no shared threads to hand hashes back
 * and forth), so the split is done by managing rieMiner as a child process instead of by switching a connection.
 *
 * This is checked before any of XMRig's own argument parsing runs, so it changes nothing for every other
 * algorithm: maybeDispatch() returns false immediately unless the algorithm is Riecoin's.
 */
#ifndef POOLPAYMINER_RIECOIN_DISPATCH_H
#define POOLPAYMINER_RIECOIN_DISPATCH_H


namespace xmrig {
namespace riecoin {


// Returns true if this run was (or should have been) handed off to rieMiner: main() must return exitCode at once
// without starting XMRig. Returns false to continue starting poolpayminer normally (not a Riecoin request).
bool maybeDispatch(int argc, char **argv, int &exitCode);


} // namespace riecoin
} // namespace xmrig


#endif /* POOLPAYMINER_RIECOIN_DISPATCH_H */
