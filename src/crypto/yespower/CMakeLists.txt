cmake_minimum_required(VERSION 3.10)
project(yespower C)

# yespower 1.0 by Alexander Peslyak (Solar Designer) and the libcperciva SHA-256, as used by Yenten (see README and THIRD-PARTY-NOTICES.txt)
set(SOURCES
    yespower-opt.c
    sha256.c
)

set_source_files_properties(yespower-opt.c sha256.c PROPERTIES COMPILE_FLAGS "-O2")
if (CMAKE_C_COMPILER_ID MATCHES GNU OR CMAKE_C_COMPILER_ID MATCHES Clang)
    set_source_files_properties(yespower-opt.c sha256.c PROPERTIES COMPILE_FLAGS "-O2 -w")
endif()

include_directories(.)

add_library(yespower STATIC ${SOURCES})
