# Cascoin (CAS)'s CPU PoW, MinotaurX: reuses ghostrider's sph_* primitives plus the yespower module's
# core for its final hardening gate (see src/crypto/minotaurx/minotaurx.h) -- needs BOTH WITH_GHOSTRIDER
# and WITH_YESPOWER on (both default ON in this fleet's builds). Included AFTER both cmake/ghostrider.cmake
# and cmake/yespower.cmake in the root CMakeLists.txt specifically so the `ghostrider`/`yespower` targets
# this module links against already exist by the time its own add_subdirectory() runs.
if (WITH_GHOSTRIDER AND WITH_YESPOWER)
    add_definitions(/DXMRIG_ALGO_MINOTAURX)
    add_subdirectory(src/crypto/minotaurx)
    set(MINOTAURX_LIBRARY minotaurx)
else()
    remove_definitions(/DXMRIG_ALGO_MINOTAURX)
    set(MINOTAURX_LIBRARY "")
endif()
