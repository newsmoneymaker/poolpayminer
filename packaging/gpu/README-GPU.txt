poolpayminer: video cards (OpenCL and CUDA)
============================================

The program can also mine on video cards:
  * OpenCL: AMD cards (also NVIDIA and Intel through their drivers). The library (OpenCL.dll on Windows, libOpenCL.so on Linux) comes with
    the video driver.
  * CUDA:   NVIDIA cards, through the separate XMRig plugin (xmrig-cuda.dll on Windows, libxmrig-cuda.so on Linux), see below.
Video cards are OFF unless you switch them on in config.json ("opencl" / "cuda") or with --opencl / --cuda.

IMPORTANT
* Epic Cash (rx/epic) is a CPU algorithm: this program can NOT mine Epic Cash on a video card.
* Video cards mine the other algorithms of XMRig: KawPow (Ravencoin), RandomX (standard variants), CryptoNight and others.
* Tested so far: CUDA (NVIDIA) with KawPow on Nanopool, shares accepted. OpenCL has not been run on a video card by the project yet.

FEE (see README.txt): when a video-card backend (OpenCL or CUDA) is enabled, the fee minute (1 minute in about 100) is mined on Ravencoin (KawPow)
on Nanopool, for the operator's Ravencoin wallet, on your video cards. If the CPU is enabled too, the CPU is idle during that minute. The start-up
banner says so.

QUICK START, KawPow on an OpenCL video card
1. Put your Ravencoin address into config-kawpow-opencl.json instead of YOUR_RAVENCOIN_ADDRESS (the worker name after the dot).
2. Check that the card is found:  poolpayminer --opencl --print-platforms      then start:  poolpayminer -c config-kawpow-opencl.json

NVIDIA with CUDA (usually faster than OpenCL on NVIDIA)
1. Get the plugin that fits your driver from https://github.com/xmrig/xmrig-cuda/releases (Windows: xmrig-cuda 6.22.1 for cuda10_2, cuda11_8
   or cuda12_9; Linux: build it from the source there). Put xmrig-cuda.dll (Linux: libxmrig-cuda.so) and the DLLs beside it next to the
   program. The plugin is a separate program by the XMRig developers (GPLv3); it is not part of this package.
2. Start:  poolpayminer -c config-kawpow-cuda.json      (address as above)

Other pools: change "url", "user" and "algo" in the json file. Everything else works as in XMRig: https://xmrig.com/docs

====================================== RU ======================================

poolpayminer: видеокарты (OpenCL и CUDA)

Программа умеет майнить и на видеокартах: OpenCL (AMD, а также NVIDIA и Intel через их драйверы; библиотека OpenCL.dll в Windows и libOpenCL.so в Linux
идёт с драйвером) и CUDA (NVIDIA, через отдельный плагин XMRig: xmrig-cuda.dll в Windows, libxmrig-cuda.so в Linux). Видеокарты выключены, пока вы не
включите их в config.json ("opencl" / "cuda") или ключами --opencl / --cuda.

ВАЖНО
* Epic Cash (rx/epic) - алгоритм для процессора: майнить Epic Cash на видеокарте эта программа НЕ умеет.
* Видеокарты майнят другие алгоритмы XMRig: KawPow (Ravencoin), RandomX (стандартные варианты), CryptoNight и другие.
* Проверено: CUDA (NVIDIA), KawPow на Nanopool, шары принимаются. OpenCL на видеокарте проектом ещё не запускался.

КОМИССИЯ (см. README.txt): при включённой видеокарте (OpenCL или CUDA) минута комиссии (1 минута примерно из 100) майнится на Ravencoin (KawPow) на Nanopool,
на кошелёк оператора, на ваших видеокартах. Если включён и процессор, он в эту минуту простаивает. Баннер запуска пишет об этом.

БЫСТРЫЙ СТАРТ, KawPow с OpenCL: впишите адрес Ravencoin в config-kawpow-opencl.json (имя воркера после точки), проверьте карту
"poolpayminer --opencl --print-platforms" и запустите "poolpayminer -c config-kawpow-opencl.json".

NVIDIA с CUDA: возьмите плагин под ваш драйвер на https://github.com/xmrig/xmrig-cuda/releases (Windows: xmrig-cuda 6.22.1, cuda10_2 / cuda11_8 / cuda12_9;
Linux: соберите из исходников там же), положите xmrig-cuda.dll (Linux: libxmrig-cuda.so) и соседние DLL рядом с программой (плагин - отдельная
программа разработчиков XMRig, GPLv3, в архив не входит) и запустите "poolpayminer -c config-kawpow-cuda.json".
