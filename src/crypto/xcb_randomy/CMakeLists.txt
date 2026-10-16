cmake_minimum_required(VERSION 3.10)
project(xcb_randomy)

# Core Blockchain (XCB) "RandomY". See xcb_randomy_upstream.h / xcb-project.md for the full story.
#
# IMPORTANT, updated 2026-10-16: the Linux library below is NOT the real go-randomy prebuilt binary
# anymore -- it (and the Windows one) is built from the REAL, unmodified upstream C/C++ source tree
# (github.com/core-coin/RandomY, the actual source go-randomy's own CI compiles into its prebuilt
# releases), compiled fresh by this project itself, then put through the exact same
# `objcopy --redefine-syms` binary rename pass (strong/non-COMDAT defined symbols only -- see below) that
# was previously applied to the downloaded prebuilt binary. This was done specifically to unblock a real
# Windows build without ever downloading go-randomy's own prebuilt Windows binary (that action is
# correctly off-limits here) -- compiling THEIR OWN real source with this project's already-trusted
# mingw-w64 cross-compiler is a different, legitimate action. Verified byte-for-byte: both the Linux and
# Windows libraries built this way reproduce the real go-randomy test vector (key="test key 000",
# input="This is a test" -> 0e9ab13e4b337c866e17445621be8d180c0a18a325932078dc33f7ac69ff17e8), the Windows
# one confirmed by actually EXECUTING the cross-compiled PE binary under wine64, not just checking that it
# links.
#
# Every strong (non-weak on ELF / non-COMDAT-section on PE-COFF) GLOBALLY DEFINED symbol was renamed
# randomx_* -> xcbrndy_* via `objcopy --redefine-syms` (a binary rename after compiling, not a source
# change) to avoid colliding with this project's own, differently-shaped randomx_* symbols already in the
# same xmrig binary (see [[critical-vendored-lib-symbol-collision]]). Symbols living in COMDAT-folded
# sections (PE/COFF's equivalent of ELF weak/vague-linkage symbols -- C++ template instantiations, inline
# member functions, typeinfo/vtables emitted into `.text$<mangled-name>`-style sections with
# LINK_ONCE_DISCARD) were deliberately LEFT UNRENAMED on both platforms, mirroring the hard-won Linux
# lesson that renaming those causes an ODR/weak-merge violation (a process-wide crash on ELF; the
# equivalent risk on PE/COFF would be the linker no longer being able to COMDAT-fold two supposedly
# identical copies of the same template/inline code into one). On Windows this was identified via
# `objdump -h` section flags (LINK_ONCE_DISCARD + the `$mangledname` group suffix), not via nm's W/w
# weak-symbol marker (PE/COFF doesn't expose vague linkage that way).
set(SOURCES
    xcb_randomy.cpp
)

add_library(xcb_randomy STATIC ${SOURCES})

if (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    target_link_libraries(xcb_randomy PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/lib/linux-x86_64/librandomy_xcb.a)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows" AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    target_link_libraries(xcb_randomy PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/lib/windows-x86_64/librandomy_xcb.a)
else()
    message(FATAL_ERROR "WITH_XCB is only supported on Linux x86_64 and Windows x86_64 so far -- see xcb-project.md")
endif()

target_include_directories(xcb_randomy PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)
