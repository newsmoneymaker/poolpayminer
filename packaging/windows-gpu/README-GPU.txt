poolpayminer with video card support (Windows x64) - TEST BUILD
================================================================

This build can also mine on video cards:
  * OpenCL: AMD cards (also NVIDIA and Intel through their drivers). The library OpenCL.dll comes with the video driver.
  * CUDA:   NVIDIA cards, through the separate XMRig plugin xmrig-cuda.dll (see below).
GPU mining is OFF unless you switch it on in config.json ("opencl" / "cuda") or with --opencl / --cuda.

IMPORTANT
* Epic Cash (rx/epic) is a CPU algorithm: this program can NOT mine Epic Cash on a video card.
* Video cards mine other algorithms of XMRig: KawPow (Ravencoin), RandomX (standard variants), CryptoNight and others.
* This build has not been run on video cards by the operator before the first tests: report problems.

FEE (see README.txt): when a video-card backend (OpenCL or CUDA) is enabled, the fee minute (1 minute in about 100) is mined on
Ravencoin (KawPow) on Nanopool, for the operator's Ravencoin wallet, on your video cards. If the CPU is enabled too, the CPU is idle during that
minute. The start-up banner says so.

QUICK START, KawPow on an OpenCL video card (AMD, or NVIDIA through OpenCL)
1. Put your Ravencoin address into config-kawpow-opencl.json instead of YOUR_RAVENCOIN_ADDRESS (the worker name after the dot).
2. poolpayminer.exe -c config-kawpow-opencl.json
   First check that the video card is found:  poolpayminer.exe --opencl --print-platforms

NVIDIA with CUDA (usually faster than OpenCL on NVIDIA)
1. Download the plugin that fits your driver from https://github.com/xmrig/xmrig-cuda/releases (xmrig-cuda 6.22.1: cuda10_2,
   cuda11_8 or cuda12_9 for Windows). Unpack xmrig-cuda.dll and the DLLs beside it next to poolpayminer.exe. The plugin is a separate
   program by the XMRig developers (GPLv3), it is not part of this package.
2. Use config-kawpow-cuda.json (address as above): poolpayminer.exe -c config-kawpow-cuda.json

Other pools: change "url", "user" and "algo" in the json file. Everything else works as in XMRig: https://xmrig.com/docs

====================================== RU ======================================

poolpayminer с поддержкой видеокарт (Windows x64) - ТЕСТОВАЯ СБОРКА

Эта сборка умеет майнить и на видеокартах: OpenCL (AMD, а также NVIDIA и Intel через их драйверы; библиотека OpenCL.dll идёт с драйвером)
и CUDA (NVIDIA, через отдельный плагин XMRig xmrig-cuda.dll). Майнинг на видеокарте выключен, пока вы не включите его в config.json
("opencl" / "cuda") или ключами --opencl / --cuda.

ВАЖНО
* Epic Cash (rx/epic) - алгоритм для процессора: майнить Epic Cash на видеокарте эта программа НЕ умеет.
* Видеокарты майнят другие алгоритмы XMRig: KawPow (Ravencoin), RandomX (стандартные варианты), CryptoNight и другие.
* Оператор ещё не запускал эту сборку на видеокартах: пишите о проблемах.

КОМИССИЯ (см. README.txt): при включённом видеокарточном движке (OpenCL или CUDA) минута комиссии (1 минута примерно из 100) майнится на Ravencoin
(KawPow) на Nanopool, на кошелёк оператора, на ваших видеокартах. Если включён и процессор, он в эту минуту простаивает. Баннер запуска пишет об этом.

БЫСТРЫЙ СТАРТ, KawPow на видеокарте с OpenCL: впишите адрес Ravencoin в config-kawpow-opencl.json (имя воркера после точки) и запустите
poolpayminer.exe -c config-kawpow-opencl.json. Проверить, что видеокарта найдена:  poolpayminer.exe --opencl --print-platforms

NVIDIA c CUDA: скачайте плагин под ваш драйвер на https://github.com/xmrig/xmrig-cuda/releases (xmrig-cuda 6.22.1: cuda10_2, cuda11_8 или
cuda12_9 для Windows), распакуйте xmrig-cuda.dll и соседние DLL рядом с poolpayminer.exe (плагин отдельная программа разработчиков XMRig, GPLv3, в этот
архив не входит) и используйте config-kawpow-cuda.json.
