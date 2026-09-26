/* poolpayminer: auto-switching mode ("--auto <personal port>").
 *
 * The user registers on all.pool-pay.com, gets a personal port number, and starts
 *     poolpayminer --auto 15000
 * This wrapper (it runs before XMRig's own startup, like riecoin::maybeDispatch) asks
 *     https://all.pool-pay.com/account/api/route?port=15000
 * which coin currently pays best among the coins that user enabled (see the website's profitability table), plus the
 * pool, algorithm and login to use, and runs an ordinary poolpayminer child process with exactly those settings. Every
 * few minutes it asks again and, when the answer changed, replaces the child. The children are normal runs, so the
 * usual 1% fee logic applies to them unchanged.
 *
 * The answer is an Ed25519-signed JSON payload; the public key is compiled in, so nobody on the network path can
 * redirect a miner to another pool or wallet by faking the response.
 */
#ifndef POOLPAYMINER_AUTOSWITCH_H
#define POOLPAYMINER_AUTOSWITCH_H


namespace xmrig {
namespace autoswitch {


// Returns true if this run is an --auto run (main() must then return exitCode at once). Returns false to continue
// with the normal startup.
bool maybeAuto(int argc, char **argv, int &exitCode);


} // namespace autoswitch
} // namespace xmrig


#endif /* POOLPAYMINER_AUTOSWITCH_H */
