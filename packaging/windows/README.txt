poolpayminer 6.26.0-epic6  (TEST BUILD, Windows x64)
=====================================================

poolpayminer is a modified version of XMRig 6.26.0 (https://github.com/xmrig/xmrig, GPLv3).
It adds the Epic Cash stratum protocol (RandomX) and keeps XMRig's other algorithms.
It is NOT the official XMRig.

License: GNU GPL v3 (file LICENSE). Third-party licenses (including OpenSSL): THIRD-PARTY-NOTICES.txt.
Changes made to XMRig: CHANGES.md. The complete source code of this exact build is in
poolpayminer-6.26.0-epic6-src.tar.gz (next to this package).


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

poolpayminer 6.26.0-epic6  (ТЕСТОВАЯ СБОРКА, Windows x64)

poolpayminer - изменённая версия XMRig 6.26.0 (GPLv3): добавлен протокол Epic Cash (RandomX), остальные
алгоритмы XMRig сохранены. Это НЕ официальный XMRig. Лицензия: LICENSE; сторонние лицензии (включая OpenSSL):
THIRD-PARTY-NOTICES.txt; список изменений: CHANGES.md; полный исходный код этой сборки:
poolpayminer-6.26.0-epic6-src.tar.gz.

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

ЗАМЕЧАНИЯ:
* Антивирусы (в том числе Windows Defender) могут помечать любой майнер на основе XMRig. Файл не подписан.
* RandomX требует около 2,3 ГБ ОЗУ. При нехватке: "randomx": {"mode": "light"} в config.json (медленнее).
* Сборка выполнена кросс-компиляцией на Linux. Обычный TCP-вариант работал на Windows у тестера оператора;
  TLS-вариант проверен на Linux (тот же код), на Windows он проверяется впервые.
* Сборка без драйвера MSR и без hwloc.
