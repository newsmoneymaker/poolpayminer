poolpayminer 6.26.0-epic6  (Linux x86-64)
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
1. Get the epicbox address of your Epic wallet (52 characters, starts with "es"):  epic-wallet address
2. Put it into config.json instead of PUT_YOUR_EPICBOX_ADDRESS_HERE (worker name after "+": ADDRESS+rig1).
   An exchange deposit address that needs a note (payment ID): ADDRESS.NOTE+rig1 or ADDRESS#NOTE+rig1.
3. ./poolpayminer          (or:  ./poolpayminer --epic --tls -o epic.pool-pay.com:3334 -u ADDRESS+rig1 -p x -k )

What you should see:  "use pool epic.pool-pay.com:3334 TLSv1.3 ...", "new job ... algo rx/epic", "READY threads ...",
then "accepted (1/0) ..." (shares accepted by the pool) and "speed ... H/s". Check https://epic.pool-pay.com (Worker Statistics).

Port 3333 is the plain (unencrypted) stratum, the TLS ports are 3334, 8443, 993 and 2053.

VIDEO CARDS: OpenCL and CUDA are built in (off by default). See README-GPU.txt; Epic Cash itself is CPU only.

NOTES
-----
* Needs glibc 2.28 or newer (Debian 10, Ubuntu 18.10 and newer). RandomX needs about 2.3 GB of RAM (fast mode); with less use
  "randomx": {"mode": "light"} in config.json.
* Huge pages make RandomX faster:  sudo sysctl -w vm.nr_hugepages=1280
* Run it as a normal user. If you run it as root, XMRig may change CPU model-specific registers to speed RandomX up; pass
  --randomx-wrmsr=-1 to forbid that.
* Compiled with static OpenSSL and libuv; the OpenCL/CUDA backends are not included. Full source: https://epic.pool-pay.com/source/
* This is a modified XMRig 6.26.0 (GPLv3), not the official XMRig. License: LICENSE; third-party licenses: THIRD-PARTY-NOTICES.txt;
  changes: CHANGES.md.


===================================================== RU =====================================================

poolpayminer (Linux x86-64) - изменённый XMRig 6.26.0 (GPLv3): добавлен протокол Epic Cash (RandomX, rx/epic), остальные алгоритмы
XMRig сохранены. Это НЕ официальный XMRig. Исходный код: https://epic.pool-pay.com/source/

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
2. Впишите его в config.json вместо PUT_YOUR_EPICBOX_ADDRESS_HERE (имя воркера после "+": АДРЕС+rig1).
   Депозитный адрес биржи с меткой (payment ID): АДРЕС.МЕТКА+rig1 или АДРЕС#МЕТКА+rig1.
3. ./poolpayminer

Успех выглядит так: "use pool epic.pool-pay.com:3334 TLSv1.3 ...", "new job ... algo rx/epic", "READY threads ...", затем
"accepted (1/0) ..." - шары приняты пулом. Статистика: https://epic.pool-pay.com (Worker Statistics).

ВИДЕОКАРТЫ: OpenCL и CUDA встроены (по умолчанию выключены), см. README-GPU.txt; сам Epic Cash только на процессоре.

ЗАМЕЧАНИЯ:
* Нужен glibc 2.28 или новее (Debian 10, Ubuntu 18.10 и новее). RandomX требует около 2,3 ГБ ОЗУ; при нехватке: "randomx": {"mode": "light"}.
* Большие страницы ускоряют RandomX:  sudo sysctl -w vm.nr_hugepages=1280
* Запускайте от обычного пользователя. От root XMRig может менять регистры процессора (MSR) для ускорения; --randomx-wrmsr=-1 запрещает это.
* Сборка со статическими OpenSSL и libuv, без OpenCL/CUDA.
