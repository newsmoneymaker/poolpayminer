if (WITH_TLS)
    set(OPENSSL_ROOT_DIR ${XMRIG_DEPS})

    if (WIN32)
        set(OPENSSL_USE_STATIC_LIBS TRUE)
        set(OPENSSL_MSVC_STATIC_RT TRUE)

        set(EXTRA_LIBS ${EXTRA_LIBS} crypt32)
    elseif (APPLE)
        set(OPENSSL_USE_STATIC_LIBS TRUE)
    endif()

    if (BUILD_STATIC)
        set(OPENSSL_USE_STATIC_LIBS TRUE)
    endif()

    # Real bug fixed 2026-10-23 (poolpayminer 1.1.30): CMake's FindOpenSSL module never located the
    # already-built MinGW OpenSSL static libs here when cross-compiling for Windows (OPENSSL_ROOT_DIR
    # alone wasn't enough -- find_package's own probing logic doesn't reliably cross-compile-detect a
    # prebuilt target lib without explicit path hints), so WITH_TLS silently fell back to OFF on every
    # past Windows build. Give it the exact paths directly when they're already there.
    if (WIN32 AND EXISTS "${XMRIG_DEPS}/lib/libcrypto.a" AND EXISTS "${XMRIG_DEPS}/lib/libssl.a")
        set(OPENSSL_INCLUDE_DIR "${XMRIG_DEPS}/include")
        set(OPENSSL_CRYPTO_LIBRARY "${XMRIG_DEPS}/lib/libcrypto.a")
        set(OPENSSL_SSL_LIBRARY "${XMRIG_DEPS}/lib/libssl.a")
    endif()

    find_package(OpenSSL)

    if (OPENSSL_FOUND)
        set(TLS_SOURCES
            src/base/net/stratum/Tls.cpp
            src/base/net/stratum/Tls.h
            src/base/net/tls/ServerTls.cpp
            src/base/net/tls/ServerTls.h
            src/base/net/tls/TlsConfig.cpp
            src/base/net/tls/TlsConfig.h
            src/base/net/tls/TlsContext.cpp
            src/base/net/tls/TlsContext.h
            src/base/net/tls/TlsGen.cpp
            src/base/net/tls/TlsGen.h
            )

        include_directories(${OPENSSL_INCLUDE_DIR})

        if (WITH_HTTP)
            set(TLS_SOURCES ${TLS_SOURCES}
                src/base/net/https/HttpsClient.cpp
                src/base/net/https/HttpsClient.h
                src/base/net/https/HttpsContext.cpp
                src/base/net/https/HttpsContext.h
                src/base/net/https/HttpsServer.cpp
                src/base/net/https/HttpsServer.h
                )
        endif()
    else()
        message(FATAL_ERROR "OpenSSL NOT found: use `-DWITH_TLS=OFF` to build without TLS support")
    endif()

    add_definitions(/DXMRIG_FEATURE_TLS)
else()
    set(TLS_SOURCES "")
    set(OPENSSL_LIBRARIES "")
    remove_definitions(/DXMRIG_FEATURE_TLS)

    if (WITH_HTTP)
        set(TLS_SOURCES ${TLS_SOURCES}
            src/base/net/http/HttpServer.cpp
            src/base/net/http/HttpServer.h
            )
    endif()

    set(CMAKE_PROJECT_NAME "${CMAKE_PROJECT_NAME}-notls")
endif()
