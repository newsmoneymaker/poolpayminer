if (WITH_GHOSTRIDER)
    add_definitions(/DXMRIG_ALGO_GHOSTRIDER)
    add_subdirectory(src/crypto/ghostrider)
    set(GHOSTRIDER_LIBRARY ghostrider)

    # FortuneBlock's own HashFortune algorithm (NOT GhostRider -- a different core-hash count/round
    # structure, see Algorithm::FORTUNEHASH_FTB) rides on the same WITH_GHOSTRIDER/XMRIG_ALGO_GHOSTRIDER
    # flag for now since it reuses this module's sph_* primitives and CnHash infrastructure, rather than
    # adding a brand-new CMake option for a single coin.
    add_subdirectory(src/crypto/fortunehash)
    set(FORTUNEHASH_LIBRARY fortunehash)
else()
    remove_definitions(/DXMRIG_ALGO_GHOSTRIDER)
    set(GHOSTRIDER_LIBRARY "")
    set(FORTUNEHASH_LIBRARY "")
endif()
