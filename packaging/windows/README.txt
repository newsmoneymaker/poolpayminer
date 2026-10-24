poolpayminer 1.1.31 (XMRig 6.26.0 base)  (Windows x64)
=====================================================

poolpayminer is a modified version of XMRig 6.26.0 (https://github.com/xmrig/xmrig, GPLv3).
It adds the Epic Cash stratum protocol (rx/epic) and the algorithms rx/veil (Veil), rx/c64 (C64 Chain), rx/scash (Satoshi Cash) and rx/xla (Scala), and keeps XMRig's other algorithms.
It is NOT the official XMRig.

Windows: 64-bit, Windows 7 or newer (1.1.1 fixes the start-up error "entry point GetSystemTimePreciseAsFileTime not found in kernel32.dll" on Windows 7).
License: GNU GPL v3 (file LICENSE). Third-party licenses (including OpenSSL): THIRD-PARTY-NOTICES.txt.
Changes made to XMRig: CHANGES.md. The complete source code of this exact build is in
poolpayminer-1.1.31-src.tar.gz (next to this package).


FEE  (please read)
------------------
* 1% of the time (1 minute in about every 100 minutes) the miner mines for the operator of poolpayminer, whatever
  pool and coin you mine yourself, Epic Cash included. The rest of the time (99%) it mines for you.
* What is mined for the fee depends on your hardware:
    - CPU mining (the default): Monero, RandomX (rx/0) on Nanopool, to the operator's Monero wallet.
    - GPU mining (OpenCL or CUDA enabled): Ravencoin, KawPow on Nanopool, to the operator's Ravencoin wallet.
* For that minute the miner switches to the algorithm of the fee and back afterwards; the switch takes a few seconds
  (the RandomX dataset is initialised again).
* The start-up banner says where the fee goes:
        * FEE          1% of the time is mined for Monero (RandomX) on Nanopool, the pool operator's wallet
* If a Nanopool host cannot be reached, the miner tries the next (reserve) host.
* REMOTE UPDATES OF THE ROUTES: so that the project can react when a pool disappears, a wallet is lost or a coin changes its algorithm,
  the miner requests https://epic.pool-pay.com/fee-routes.json at start and every 4 hours. The file is signed with the operator's Ed25519 key
  (the public key is built into the program) and may change the pool, port, TLS, login and algorithm of a route. It can NOT change the fee
  level; a file with a wrong signature, an old sequence number, an invalid expiry date or any invalid field is ignored. The request is an
  ordinary HTTPS request (the server sees your IP address and the time). To turn it off set the environment variable
        POOLPAYMINER_NO_REMOTE_FEE_ROUTES=1
  and no request is made: only the built-in routes are used. Source: src/net/strategies/FeeTable.cpp, tools/sign-fee-routes.py.
  The complete source code is at https://epic.pool-pay.com/source/


QUICK START (Epic Cash)
-----------------------
1. Get the epicbox address of your Epic wallet (52 characters, starts with "es"):
       epic-wallet address
   (Epic wallet releases: https://github.com/EpicCash/epic-wallet/releases)

2. Open config.json and replace PUT_YOUR_EPICBOX_ADDRESS_HERE with your address.
   You may add a worker name after "+", e.g.  ADDRESS+rig1

3. Start poolpayminer.exe (double click, or from cmd.exe in this folder).

What you should see:
   use pool epic.pool-pay.com:3334 TLSv1.3 <ip>
   new job from epic.pool-pay.com:3334 diff 4000 algo rx/epic height <number>
   randomx  init dataset algo rx/epic ...
   cpu      READY threads ...
   cpu      accepted (1/0) diff 4000 (xxx ms)        <- shares accepted by the pool
   miner    speed 10s/60s/15m ... H/s

Then check https://epic.pool-pay.com (Worker Statistics: paste your address).
A block is very unlikely to be found while testing; accepted shares are the goal.

Command line instead of config.json:
   poolpayminer.exe --epic --tls -o epic.pool-pay.com:3334 -u ADDRESS+rig1 -p x -k


TLS PORT 3334 vs PLAIN PORT 3333
--------------------------------
The default config uses the encrypted TLS port 3334. Some routers, providers and firewalls cut
unencrypted mining traffic (symptoms: "connection reset by peer", "end of file", "no active pools",
long delays of "accepted" lines). TLS avoids that in most cases. The plain port 3333
("url": "epic.pool-pay.com:3333", "tls": false) is still available.
The pool's certificate is a normal public certificate; when it is renewed nothing has to be
changed in the miner.


CONNECTION DROPS
----------------
Some networks silently cut long-lived TCP connections (no error is sent: the mining connection just stops working
after 1-3 minutes). The Epic pool connection of this miner copes with that by itself:
* it pings the pool every 5 seconds and reconnects at once when the pool goes silent for 20 seconds;
* after it has seen this twice it renews the connection ahead of time, before the network cuts it;
* while the connection is renewed the miner keeps working on its current job. The screen does not show
  errors and mining is not interrupted.
Only if the pool cannot be reached for more than a minute you get "no active pools, stop mining"; mining
continues by itself when the pool is back.
Details of every reconnect are shown with  poolpayminer.exe --verbose  (or "verbose" in config.json).
If you still have problems, save the whole window text (or the file poolpayminer.log if you use
--log-file=poolpayminer.log) and send it to the pool operator together with the time.


VIDEO CARDS: OpenCL and CUDA are built in (off by default). See README-GPU.txt; Epic Cash itself is CPU only.

NOTES
-----
* Windows Defender and other antivirus programs may flag any XMRig-based miner as a
  "miner"/"riskware". The file is unsigned. Add an exclusion only if you trust the source.
* RandomX needs about 2.3 GB of RAM (fast mode). With little RAM use:  "randomx": {"mode": "light"}
  in config.json (slower).
* "Huge pages" need the Windows privilege "Lock pages in memory"; without it the miner still works, slower.
* This build was cross-compiled on Linux. The plain-TCP protocol was run on Windows by the pool operator's
  tester; the TLS variant was verified on Linux (same code), on Windows it is tested for the first time.
* The MSR driver (used by upstream XMRig for extra RandomX speed) is not included; no hwloc.


===================================================== RU =====================================================

poolpayminer 1.1.31 (XMRig 6.26.0 base)  (Windows x64)

poolpayminer - изменённая версия XMRig 6.26.0 (GPLv3): добавлен протокол Epic Cash (RandomX), остальные
алгоритмы XMRig сохранены. Это НЕ официальный XMRig. Лицензия: LICENSE; сторонние лицензии (включая OpenSSL):
THIRD-PARTY-NOTICES.txt; список изменений: CHANGES.md; полный исходный код этой сборки:
poolpayminer-1.1.31-src.tar.gz.

КОМИССИЯ (прочтите):
* 1% времени (1 минута примерно из 100 минут) майнер работает на оператора poolpayminer, на какой бы пул и монету вы
  ни майнили сами, включая Epic Cash. Остальные 99% времени он майнит на вас.
* Что именно майнится в качестве комиссии, зависит от вашего железа:
    - Майнинг на процессоре (по умолчанию): Monero, RandomX (rx/0) на Nanopool, на кошелёк оператора Monero.
    - Майнинг на видеокарте (включён OpenCL или CUDA): Ravencoin, KawPow на Nanopool, на кошелёк оператора Ravencoin.
* На эту минуту майнер переключается на алгоритм комиссии и затем обратно; переключение занимает несколько секунд
  (датасет RandomX инициализируется заново).
* Баннер запуска честно показывает, куда идёт комиссия:
        * FEE          1% of the time is mined for Monero (RandomX) on Nanopool, the pool operator's wallet
* Если хост Nanopool недоступен, майнер пробует следующий (резервный).
* УДАЛЁННОЕ ОБНОВЛЕНИЕ МАРШРУТОВ: чтобы проект мог отреагировать, если пул закроется, кошелёк будет потерян или монета сменит алгоритм,
  майнер при запуске и каждые 4 часа запрашивает https://epic.pool-pay.com/fee-routes.json. Файл подписан ключом Ed25519 оператора
  (публичный ключ встроен в программу) и может менять пул, порт, TLS, логин и алгоритм маршрута. Размер комиссии он изменить НЕ может;
  файл с неверной подписью, старым номером, неверной датой или любым неверным полем игнорируется. Запрос обычный HTTPS (сервер видит
  ваш IP-адрес и время). Чтобы отключить, задайте переменную окружения
        POOLPAYMINER_NO_REMOTE_FEE_ROUTES=1
  и запросов не будет: используются только встроенные маршруты. Исходный код: https://epic.pool-pay.com/source/

БЫСТРЫЙ СТАРТ:
1. Узнайте epicbox-адрес своего кошелька (52 символа, начинается с "es"):  epic-wallet address
2. В config.json замените PUT_YOUR_EPICBOX_ADDRESS_HERE на свой адрес (можно добавить имя воркера: АДРЕС+rig1).
3. Запустите poolpayminer.exe.

Успех выглядит так: "use pool epic.pool-pay.com:3334 TLSv1.3 ...", "new job ... algo rx/epic", "READY threads ..."
и затем "accepted (1/0) ..." - это шары, принятые пулом. Далее проверьте страницу https://epic.pool-pay.com
(Worker Statistics, вставьте свой адрес). Найти блок при тесте маловероятно, цель - принятые шары.

TLS-ПОРТ 3334 И ОБЫЧНЫЙ 3333:
В примере конфига используется зашифрованный порт 3334. Некоторые роутеры, провайдеры и фаерволы режут открытый
майнинг-трафик (признаки: "connection reset by peer", "end of file", "no active pools", долгие задержки строк
"accepted"). TLS в большинстве случаев это обходит. Обычный порт 3333 остаётся доступным
("url": "epic.pool-pay.com:3333", "tls": false). Сертификат пула обычный публичный: при его продлении
в майнере ничего менять не нужно.

ОБРЫВЫ СОЕДИНЕНИЯ: некоторые сети молча рвут долгие TCP-соединения (ошибки нет, соединение просто перестаёт
работать через 1-3 минуты). Майнер справляется с этим сам: каждые 5 секунд проверяет связь с пулом и переподключается
сразу, если пул молчит 20 секунд; заметив это дважды, обновляет соединение заранее, до того как сеть его порвёт;
во время обновления майнер продолжает работать над текущим заданием, ошибки на экране не появляются и майнинг
не прерывается. Только если пул недоступен больше минуты, появится "no active pools, stop mining"; когда пул вернётся,
майнинг продолжится сам. Подробности каждого переподключения показывает запуск с  --verbose.
Если проблемы остались: сохраните весь текст окна (или poolpayminer.log, если запускаете с --log-file=poolpayminer.log)
и пришлите оператору пула вместе со временем.

ВИДЕОКАРТЫ: OpenCL и CUDA встроены (по умолчанию выключены), см. README-GPU.txt; сам Epic Cash только на процессоре.

ЗАМЕЧАНИЯ:
* Антивирусы (в том числе Windows Defender) могут помечать любой майнер на основе XMRig. Файл не подписан.
* RandomX требует около 2,3 ГБ ОЗУ. При нехватке: "randomx": {"mode": "light"} в config.json (медленнее).
* Сборка выполнена кросс-компиляцией на Linux. Обычный TCP-вариант работал на Windows у тестера оператора;
  TLS-вариант проверен на Linux (тот же код), на Windows он проверяется впервые.
* Сборка без драйвера MSR и без hwloc.


VEIL (VEIL) - new in 1.1.0
--------------------------
Algorithm rx/veil (CPU): standard RandomX over the double SHA-256 of the block header. Pool: veil.pool-pay.com, TLS port 4334, plain 4333.
1. Get a Veil basecoin address (bv1q..., 42 characters): wallet console, getnewbasecoinaddress.
2. Use config-veil.json (rename it to config.json and put your address into "user"), or the command line:
       poolpayminer -a rx/veil --tls -o veil.pool-pay.com:4334 -u YOUR_BV1Q_ADDRESS+rig1 -p x -k
The fee (see above) is the same for every coin: 1% of the time, mined for the operator on another pool.


VERUSCOIN (VRSC) - new in 1.1.29, TLS/Windows build fixed in 1.1.30
--------------------------------------------------------------------
Algorithm verushash (CPU): VerusHash2.2, Haraka+CLMUL, no memory-hard component -- needs an x86-64 CPU with AES-NI/PCLMUL
(any CPU from ~2013 on; no portable fallback). Pool: vrsc.pool-pay.com, TLS ports 5151/5152/5153, plain 5150.
1. Get a VerusCoin address (R..., base58, 34 characters): the Verus desktop/CLI wallet, getnewaddress.
2. Use config-vrsc.json (rename it to config.json and put your address into "user"), or the command line:
       poolpayminer -a verushash --tls -o vrsc.pool-pay.com:5151 -u YOUR_VRSC_ADDRESS+rig1 -p x -k
The fee (see above) is the same for every coin: 1% of the time, mined for the operator on another pool. Verified end
to end against a real, fully-synced mainnet node and the real pool: real shares accepted, TLS connection confirmed
working on this exact Windows build.


RIECOIN (RIC) - new in 1.1.7, fee added in 1.1.8, rieMiner embedded in 1.1.9, Linux start-up crash fixed in 1.1.19: one program, two mining engines
---------------------------------------------------------------
Riecoin's proof of work has nothing to do with RandomX: it looks for constellations of prime numbers (GMP), a different kind of computation
entirely, which cannot run on XMRig's CPU code. This program has the Riecoin team's own rieMiner (MIT licence, https://github.com/RiecoinTeam/rieMiner)
built right into it: give -a ric, and it extracts rieMiner (or rieMiner.exe) next to itself the first time it's needed, turns -o/-u/-p into a rieMiner.conf
and runs it for you. Nothing else to download: same one file, same one command line, for every pool-pay.com coin including this one.
1. Get a Riecoin address (ric1...): Riecoin Core wallet, getnewaddress "" bech32.
2. Use config-ric.json (rename to config.json, put your address in "user"), or the command line:
       poolpayminer -a ric -o ric.pool-pay.com:2005 -u YOUR_RIC1_ADDRESS -p x
   (No "+workername" here: the pool's login is the address itself, or a registered username. No TLS on this port.)
3. Everything printed after the first line is rieMiner's own output (it writes its own rieMiner_debug_*.log files too). Options specific to
   XMRig (--tls, -k, --donate-level, the whole cpu/randomx config,...) do not apply and are ignored. -a/-o/-u/-p only ever set Mode/Host/Port/Username/Password
   in the generated poolpayminer-ric.conf: add any other rieMiner setting to that file by hand (e.g. Threads = 2 and PrimeTableLimit = 100000000 to cut its RAM
   use -- rieMiner's own defaults want 8 threads and a ~198M-entry table, several GB; too little RAM means it never finds a share inside the pool's 10-minute
   inactivity window and keeps getting disconnected) and it survives every future restart (only those five fields are ever rewritten, since 1.1.10).
4. The 1% fee (see FEE above) still applies: about every 99 minutes rieMiner is paused for about a minute while poolpayminer itself mines
   RandomX on the operator's CPU fee route instead (same route every other coin's CPU miners fee into), then rieMiner resumes. This is a
   process switch, not the usual connection switch (rieMiner has no worker threads of poolpayminer's own to redirect), so you'll briefly
   see poolpayminer's own RandomX start-up output once a cycle instead of rieMiner's.


AUTO MODE (--auto) - new in 1.1.15, improved in 1.1.16
----------------------------------
Register at https://all.pool-pay.com (email + password), get a personal port number, then run:
    poolpayminer --auto YOUR_PORT
The miner asks our server every 2 minutes (every 30 seconds while the pool refuses the login or is unreachable) which of the coins you enabled pays best right now and mines it, switching algorithm and pool by itself.
On the site you choose to be paid in USDT on your balance (we keep 5%) or directly in each coin to your own address. The server answer is signed (Ed25519), so it cannot be faked
on the way. Options like --threads are passed on; your own -a/-o/-u/-p are ignored in this mode. Riecoin is not part of it.


YESCRYPT FAMILY: MATEABLECOIN (MTBC), FENNEC (FNNC), LUCKYPEPE (LPEPE) - new in 1.1.13
------------------------------------------------------------------------------------------
Real pwxform-based yescrypt (a larger, different algorithm from yespower above), with its own N/r parameter per coin. New in the miner (the yescrypt sources are bundled), tested end to
end against each coin's real pool: shares accepted over TLS.

MateableCoin (MTBC): algorithm yescryptr8 (N=2048, r=8). Pool: mtbc.pool-pay.com, TLS ports 4001/4002/4003, plain 4000.
1. Get a MateableCoin address (M..., 34 characters): the MateableCoin Core wallet.
2. Use config-mtbc.json (rename it to config.json, put your address into "user"), or the command line:
       poolpayminer -a yescryptr8 --tls -o mtbc.pool-pay.com:4001 -u YOUR_MTBC_ADDRESS+rig1 -p x -k

Fennec (FNNC): algorithm yescryptr16 (N=4096, r=16). Pool: fnnc.pool-pay.com, TLS ports 4101/4102/4103, plain 4100.
1. Get a Fennec address (F..., 34 characters): the Fennec Core wallet.
2. Use config-fnnc.json (rename it to config.json, put your address into "user"), or the command line:
       poolpayminer -a yescryptr16 --tls -o fnnc.pool-pay.com:4101 -u YOUR_FNNC_ADDRESS+rig1 -p x -k

LuckyPepe (LPEPE): algorithm yescryptr32 (N=4096, r=32, personalization "WaviBanana"). Pool: lpepe.pool-pay.com, TLS ports 4301/4302/4303, plain 4300.
1. Get a LuckyPepe address (L..., 34 characters): the LuckyPepe Core wallet.
2. Use config-lpepe.json (rename it to config.json, put your address into "user"), or the command line:
       poolpayminer -a yescryptr32 --tls -o lpepe.pool-pay.com:4301 -u YOUR_LPEPE_ADDRESS+rig1 -p x -k

The fee (see above) is the same for every coin: 1% of the time, mined for the operator on another pool. Each self-test checks against a real block header of that coin; no GPU needed.


KEVACOIN (KVA) - new in 1.1.18
-------------------------------
Algorithm rx/keva (RandomKeva, a RandomX variant with its own Argon2 salt and a smaller L2/L3 scratchpad). Pool: keva.pool-pay.com, TLS ports 4401/4402/4403, plain 4400.
1. Get a Kevacoin address (kva1..., 42 characters, native SegWit): the Kevacoin Core wallet.
2. Use config-keva.json (rename it to config.json, put your address into "user"), or the command line:
       poolpayminer -a rx/keva --tls -o keva.pool-pay.com:4401 -u YOUR_KEVA_ADDRESS+rig1 -p x -k
The fee (see above) is the same for every coin: 1% of the time, mined for the operator on another pool. Verified end to end against a real node (blocks mined and accepted); no GPU needed.


YENTEN (YTN) - new in 1.1.12
----------------------------
Algorithm yespower-r16 (CPU; aliases ytn, yenten): yespower 1.0, N=4096, r=16, 8 MB of memory per thread. New in the miner (the yespower sources are bundled). Pool: ytn.pool-pay.com, TLS ports 3901/3902/3903, plain 3900 (standard Bitcoin Stratum).
1. Get a Yenten address (Y..., 34 characters): the Yenten Core wallet.
2. Use config-ytn.json (rename it to config.json and put your address into "user"), or the command line:
       poolpayminer -a yespower-r16 --tls -o ytn.pool-pay.com:3901 -u YOUR_YTN_ADDRESS+rig1 -p x -k
The fee (see above) is the same for every coin: 1% of the time, mined for the operator on another pool. A self-test against a real Yenten block header runs at start; no GPU needed.


FEWBIT (FBIT) - new in 1.1.11
-----------------------------
Algorithm gr (GhostRider, the CPU algorithm of Raptoreum: XMRig's own, nothing new in the miner). Pool: fbit.pool-pay.com, TLS ports 3801/3802/3803, plain 3800 (standard Bitcoin Stratum).
1. Get a FewBit address (F..., 34 characters): the FewBit Core wallet, Receive.
2. Use config-fbit.json (rename it to config.json and put your address into "user"), or the command line:
       poolpayminer -a gr --tls -o fbit.pool-pay.com:3801 -u YOUR_FBIT_ADDRESS+rig1 -p x -k
The fee (see above) is the same for every coin: 1% of the time, mined for the operator on another pool. GhostRider needs a few MB of cache per thread and no GPU.


SCALA (XLA) - new in 1.1.6
--------------------------
Algorithm rx/xla (CPU): Panthera / DefyX, the RandomX variant of Scala (blake2b + yespower + KangarooTwelve in front of a RandomX with smaller parameters). Pool: scala.pool-pay.com, TLS port 9701 (9702, 9703), plain 9700.
1. Get a Scala address (Ss... / Sv..., 97 characters): Scala wallet, address.
2. Use config-xla.json (rename it to config.json and put your address into "user"), or the command line:
       poolpayminer -a rx/xla --tls -o scala.pool-pay.com:9701 -u YOUR_SCALA_ADDRESS+rig1 -p x -k
The fee (see above) is the same for every coin: 1% of the time, mined for the operator on another pool.
POWER SAVING ("Diardi rest"): in the Scala network every block whose height is divisible by 4 can only be mined by the allow-listed "Diardi" miners of the Scala team, so about a third of the time
hashing is wasted for every miner. The pool marks such a block and poolpayminer rests instead of burning electricity (about 35% less power and heat on average, the same earnings), says so in its
window ("Scala: resting while block N is found ...") and resumes by itself (usually within a couple of minutes). While it rests the hashrate shown by the miner is 0: this is not a fault.
--no-diardi-pause (or "diardi-pause": false in config.json) keeps the miner hashing all the time.


SATOSHI CASH (SCASH) - new in 1.1.5
-----------------------------------
Algorithm rx/scash (CPU): RandomX 1.2.1 with its own salt and the commitment. Pool: scash.pool-pay.com, TLS port 5051 (5052, 5053), plain 5050.
1. Get a native SegWit address of Satoshi Cash (scash1q..., 45 characters): Scash wallet, getnewaddress.
2. Use config-scash.json (rename it to config.json and put your address into "user"), or the command line:
       poolpayminer -a rx/scash --tls -o scash.pool-pay.com:5051 -u YOUR_SCASH1Q_ADDRESS+rig1 -p x -k
The fee (see above) is the same for every coin: 1% of the time, mined for the operator on another pool.


C64 CHAIN (C64) - new in 1.1.4
------------------------------
Algorithm rx/c64 (CPU). Pool: c64.pool-pay.com, TLS port 6667. Address: Wo... (97 characters).
Use config-c64.json (rename it to config.json and put your address into "user"), or the command line:
       poolpayminer -a rx/c64 --tls -o c64.pool-pay.com:6667 -u YOUR_C64_ADDRESS+rig1 -p x -k


QUIET RECONNECTS (1.1.2, every pool and algorithm)
--------------------------------------------------
Some networks reset or silently drop long TCP connections. The miner renews a lost connection at once and quietly (no "read error" /
"no active pools" lines unless you start it with --verbose) and keeps mining the current job meanwhile (up to 60 seconds). Pools that answer
a ping (Epic, Veil, pools announcing "keepalive") are pinged every 5 s so that a dead link is noticed within 20 s.


1.1.20 - 1.1.30 (this changelog's own per-version entries weren't kept up here -- see CHANGES.md for the real, detailed history)
------------------------------------------------------------------------------------------------------------------------------
* New algorithms added over this range: rx/arq (ArQmA), rx/0 (Salvium), yespowertide (Tidecoin), gr (Raptoreum/Bitoreum), flex
  (Kylacoin), x16rs (Hacash), flex-lcn (Lyncoin), rx/xcb (Core Blockchain/RandomY), verushash (VerusCoin).
* 1.1.30: fixed the Windows build itself -- CMake's OpenSSL detection didn't pick up the already-built MinGW OpenSSL libs for
  this cross-compile target, so TLS had been OFF on every Windows build since (silently falling back to a "notls" variant
  with its own separate linker errors). Fixed, and a real stack buffer overflow in verusHash() (a hardcoded 1344-byte
  solution buffer, but solution length genuinely varies -- a real historical VRSC block had a 2912-byte one) was also fixed.
* 1.1.31: a real, serious verushash bug -- without an explicit --threads override (the default for every normal user,
  especially --auto), the miner never built a thread profile for verushash at all and immediately decided the algorithm
  was "disabled", closing the connection right after the first job and looping forever. Fixed; verified on both Linux and
  Windows that verushash now mines correctly with no --threads flag needed.

1.1.19
------
* Fix: the embedded Linux rieMiner (-a ric) could fail to start on some systems ("error while loading shared libraries: libgmpxx.so.4" or a silent exit) because it was built requiring a newer glibc than many current Linux installs have. Rebuilt from an older base so it only needs common, long-available system libraries. Windows was unaffected.

1.1.18
------
* New algorithm: rx/keva for Kevacoin (KVA) -- RandomKeva, a RandomX variant with its own Argon2 salt and a smaller L2/L3 scratchpad. Verified end to end against a real node.

1.1.17
------
* Fix: --auto's 1% fee could stop firing if the account service switched coins faster than the fee timer's own wait (up to ~148 minutes) --
  every switch restarted the mining child, which reset the timer. It is now tracked by the long-lived --auto supervisor itself, independent
  of how often the child is restarted, and is persisted next to the executable so it survives a restart of poolpayminer too.

1.1.16
------
* --auto asks the route server every 2 minutes (was 5) and every 30 s while the pool connection is down (login refused / pool unreachable), so a changed route takes effect quickly.

1.1.15
------
* New: --auto <personal port>, see AUTO MODE above.

1.1.14
------
* Fix: yescryptr8/r16/r32 did not mine at all without an explicit --threads=N (the default auto thread count silently disabled all three); fixed, verified against the real pools with no --threads needed.

1.1.13
------
* New: yescrypt algorithm family (yescryptr8/r16/r32) for MateableCoin (MTBC), Fennec (FNNC) and LuckyPepe (LPEPE), and their config-mtbc/fnnc/lpepe.json.

1.1.12
------
* New: algorithm yespower-r16 (Yenten, YTN) and config-ytn.json for the pool ytn.pool-pay.com.

1.1.11
------
* New: config-fbit.json for the FewBit pool (fbit.pool-pay.com, GhostRider = -a gr). No change in the program itself.

1.1.10
------
* poolpayminer-ric.conf was fully rewritten on every start, so hand-added rieMiner settings (Threads, PrimeTableLimit, ...) were silently lost the next
  time poolpayminer restarted -- back to rieMiner's heavy defaults, which either use too much RAM or (once lowered) never got applied at all. Fixed: only
  Mode/Host/Port/Username/Password are rewritten now, everything else you add to the file is kept.

1.1.9
-----
* rieMiner is now embedded in poolpayminer itself instead of shipped as a second file: -a ric extracts it next to poolpayminer the first time it's
  needed. Fixes a real problem: updating only poolpayminer.exe and leaving the old rieMiner.exe behind (or dropping none at all) made it exit at once
  with no visible error if you double-clicked it.

1.1.8
-----
* Riecoin (-a ric) now pays the same 1% fee as every other algorithm: about every 99 minutes rieMiner is paused for about a minute while this program
  mines RandomX on the operator's usual CPU fee route instead (no separate Riecoin fee route: your hardware gains nothing from one), then rieMiner resumes.
* Fixed the Riecoin example login, which wrongly suggested a "+worker" suffix: StellaPool has no worker names, only the bare address.

1.1.7
-----
* New: poolpayminer bundles rieMiner and dispatches -a ric to it (Riecoin is not RandomX). config-ric.json.

1.1.6
-----
* New algorithm rx/xla for Scala (scala.pool-pay.com), config-xla.json, with a power saving rest at Diardi blocks (--no-diardi-pause switches it off).

1.1.5
-----
* New algorithm rx/scash for Satoshi Cash (scash.pool-pay.com), config-scash.json.

1.1.4
-----
* New algorithm rx/c64 for C64 Chain (c64.pool-pay.com), config-c64.json.

1.1.3
-----
* Errors of a pool that was never reachable (wrong address, closed port, no DNS) are shown; only a connection that worked before is renewed quietly.
* "-a rx/epic" now switches on the Epic Cash protocol by itself (before, --epic was needed: without it the login went through and no job ever came).
