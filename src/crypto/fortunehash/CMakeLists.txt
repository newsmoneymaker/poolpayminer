cmake_minimum_required(VERSION 3.10)
project(FortuneHash)

# No sph_*.c files are compiled here -- this links against the `ghostrider` target for those symbols
# (already compiled once inside that static library) instead of recompiling them under a second static
# library, to avoid the duplicate-vendored-symbol class of bug (critical-vendored-lib-symbol-collision).
set(HEADERS
    fortunehash.h
)

set(SOURCES
    fortunehash.cpp
)

include_directories(.)
include_directories(../..)

add_library(fortunehash STATIC ${HEADERS} ${SOURCES})
target_link_libraries(fortunehash PUBLIC ghostrider)
