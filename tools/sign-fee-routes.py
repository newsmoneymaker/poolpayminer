#!/usr/bin/env python3
"""Signs the fee routes file that poolpayminer downloads from https://epic.pool-pay.com/fee-routes.json (see src/net/strategies/FeeTable.h).

  sign-fee-routes.py <private-key.pem> <routes.json> <sequence> [--days N] > fee-routes.json

routes.json is a list of routes: [{"target":"cpu|gpu","algo":"rx/0","mode":"pool|auto_eth|epic","host":"...","port":10343,
"tls":true,"user":"WALLET.fee","pass":"x","label":"text shown in the banner"}, ...]. The routes of a target that appears in the file
replace the built-in routes of that target; targets that are not mentioned keep the built-in ones. The file can NOT change the fee level.
The sequence number must grow with every file (a miner ignores files that are not newer than the one it has). The default validity is
30 days: publish a fresh file (higher sequence) before it runs out, otherwise the miners fall back to the built-in routes.
The output is {"data": base64(payload), "sig": base64(Ed25519 signature of the payload bytes)}."""
import base64, json, sys, time
from cryptography.hazmat.backends import default_backend
from cryptography.hazmat.primitives import serialization

def main():
    args = sys.argv[1:]
    days = 30
    if '--days' in args:
        i = args.index('--days'); days = int(args[i + 1]); del args[i:i + 2]
    if len(args) != 3:
        sys.exit(__doc__)
    key_path, routes_path, seq = args[0], args[1], int(args[2])
    key = serialization.load_pem_private_key(open(key_path, 'rb').read(), password=None, backend=default_backend())
    routes = json.load(open(routes_path))
    payload = json.dumps({'v': 1, 'seq': seq, 'expires': int(time.time()) + days * 86400, 'routes': routes},
                         separators=(',', ':'), ensure_ascii=True).encode()
    sig = key.sign(payload)
    print(json.dumps({'data': base64.b64encode(payload).decode(), 'sig': base64.b64encode(sig).decode()}))

main()
