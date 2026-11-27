cmake_minimum_required(VERSION 3.10)
project(MinotaurX)

# No sph_*.c files or yespower-opt.c are compiled here -- this links against the already-built
# `ghostrider` (sph_* primitives) and `yespower` (the hardening gate) static libraries instead of
# recompiling either a second time, to avoid the duplicate-vendored-symbol class of bug
# (critical-vendored-lib-symbol-collision).
set(HEADERS
    minotaurx.h
)

set(SOURCES
    minotaurx.cpp
)

include_directories(.)
include_directories(../..)

add_library(minotaurx STATIC ${HEADERS} ${SOURCES})
target_link_libraries(minotaurx PUBLIC ghostrider yespower)
