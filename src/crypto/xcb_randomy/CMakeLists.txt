cmake_minimum_required(VERSION 3.10)
project(xcb_randomy)

# Core Blockchain (XCB) "RandomY". See xcb_randomy_upstream.h / xcb-project.md for the full story: this
# is the REAL go-randomy/librandomx prebuilt static library (github.com/core-coin/RandomY), not a
# reimplementation and not this project's own vendored RandomX engine (src/crypto/randomx/) -- RandomY's
# real configuration differs from the reference RandomX config in more than the Argon2 salt, and
# reconstructing it inside this project's own engine produced a wrong hash for the standard RandomX
# README test vector. Every randomx_* symbol in the prebuilt .a was renamed xcbrndy_* via
# `objcopy --redefine-syms` (a binary rename, the library's machine code itself is untouched) to avoid
# colliding with this project's own, differently-shaped randomx_* symbols in the same xmrig binary.
#
# Linux x86_64 only for now -- the Windows build of this coin's support has not been done this pass
# (see xcb-project.md's "Not yet done" section); CMake will fail loudly for any other target rather than
# silently skip XCB support.
set(SOURCES
    xcb_randomy.cpp
)

add_library(xcb_randomy STATIC ${SOURCES})

if (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    target_link_libraries(xcb_randomy PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/lib/linux-x86_64/librandomy_xcb.a)
else()
    message(FATAL_ERROR "WITH_XCB is only supported on Linux x86_64 so far -- see xcb-project.md")
endif()

target_include_directories(xcb_randomy PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)
