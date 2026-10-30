cmake_minimum_required(VERSION 3.10)
project(ccx_gpu)

# Conceal (CCX)'s real CryptoNight-GPU PoW, vendored verbatim from ConcealNetwork/conceal-core's
# src/crypto/pow_hash/ (Ryo Currency Project) + src/crypto/keccak.c -- see ccxgpu_rename.h and
# ccx_gpu_hash.h. The C++ class/template files are wrapped in their own `namespace ccxgpu` (collision-safe
# by construction); only the plain-C keccak.c/aux_hash.c symbols need the force-included rename header,
# same convention as src/crypto/kryptokrona/ -- see critical-vendored-lib-symbol-collision.
set(VENDORED_C_SOURCES
    keccak.c
    aux_hash.c
)

set(SOURCES
    ccx_gpu_hash.cpp
    cn_slow_hash_soft.cpp
    cn_slow_hash_hard_intel.cpp
    cn_slow_hash_intel_avx2.cpp
)

include_directories(.)

if (CMAKE_C_COMPILER_ID MATCHES GNU OR CMAKE_C_COMPILER_ID MATCHES Clang)
    set_source_files_properties(${VENDORED_C_SOURCES} PROPERTIES COMPILE_FLAGS "-O2 -w -std=gnu11 -maes -mavx2 -msse4.1 -mssse3 -include ${CMAKE_CURRENT_SOURCE_DIR}/ccxgpu_rename.h")
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES GNU OR CMAKE_CXX_COMPILER_ID MATCHES Clang)
    set_source_files_properties(${SOURCES} PROPERTIES COMPILE_FLAGS "-O2 -maes -mavx2 -msse4.1 -mssse3 -include ${CMAKE_CURRENT_SOURCE_DIR}/ccxgpu_rename.h")
endif()

add_library(ccx_gpu STATIC ${SOURCES} ${VENDORED_C_SOURCES})
