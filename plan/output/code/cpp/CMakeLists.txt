cmake_minimum_required(VERSION 3.20)
project(fpsu_benchmark VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# libOTe paths (built from source at ../libOTe)
# ---------------------------------------------------------------------------
set(LIBOTE_ROOT "${CMAKE_SOURCE_DIR}/../libOTe")
set(LIBOTE_BUILD "${LIBOTE_ROOT}/out/build/osx")
set(LIBOTE_STAGE "${LIBOTE_ROOT}/out/install/osx")

# Include dirs follow libOTe convention: parent of the lib/ directory
# (so that <libOTe/config.h> maps to <parent>/libOTe/config.h)
set(LIBOTE_INCLUDE_DIRS
    # libOTe: source + build (for generated config.h)
    "${LIBOTE_ROOT}"                  # <libOTe/...>  → ROOT/libOTe/...
    "${LIBOTE_BUILD}"                 # <libOTe/config.h> → BUILD/libOTe/config.h
    # cryptoTools: source + build
    "${LIBOTE_ROOT}/cryptoTools"      # <cryptoTools/...> → ROOT/cryptoTools/cryptoTools/...
    "${LIBOTE_BUILD}/cryptoTools"     # generated config
    # coproto: source + build
    "${LIBOTE_ROOT}/out/coproto"      # <coproto/...> → ROOT/out/coproto/coproto/...
    "${LIBOTE_BUILD}/coproto"         # generated config
    # macoro: source + build
    "${LIBOTE_ROOT}/out/macoro"       # <macoro/...> → ROOT/out/macoro/macoro/...
    "${LIBOTE_BUILD}/macoro"          # generated config
    # third-party
    "${LIBOTE_STAGE}/include"         # function2
)

# Static libraries
set(LIBOTE_LIBS
    "${LIBOTE_BUILD}/libOTe/liblibOTe.a"
    "${LIBOTE_BUILD}/cryptoTools/cryptoTools/libcryptoTools.a"
    "${LIBOTE_BUILD}/coproto/coproto/libcoproto.a"
    "${LIBOTE_BUILD}/macoro/macoro/libmacoro.a"
)

# ---------------------------------------------------------------------------
# System deps (only OpenSSL needed; libOTe was built with ENABLE_BOOST=OFF)
# ---------------------------------------------------------------------------
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# ---------------------------------------------------------------------------
# Benchmark target (OT primitives + IBLT + protocol in-process)
# ---------------------------------------------------------------------------
add_executable(benchmark benchmark.cpp iblt.cpp socket.cpp protocol.cpp)
target_include_directories(benchmark PRIVATE ${LIBOTE_INCLUDE_DIRS})
target_link_libraries(benchmark PRIVATE
    ${LIBOTE_LIBS}
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    "${LIBOTE_STAGE}/lib/libsodium.dylib"
)
target_compile_options(benchmark PRIVATE -O3 -march=native+crypto)

# ---------------------------------------------------------------------------
# fpsu_net target (network benchmark: TCP IBLT exchange + protocol)
# ---------------------------------------------------------------------------
add_executable(fpsu_net fpsu_net.cpp iblt.cpp socket.cpp protocol.cpp)
target_include_directories(fpsu_net PRIVATE ${LIBOTE_INCLUDE_DIRS})
target_link_libraries(fpsu_net PRIVATE
    ${LIBOTE_LIBS}
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    "${LIBOTE_STAGE}/lib/libsodium.dylib"
)
target_compile_options(fpsu_net PRIVATE -O3 -march=native+crypto)

# ---------------------------------------------------------------------------
# fpsu_full target (full protocol with OT/OPRF, single-process coroutine)
# ---------------------------------------------------------------------------
add_executable(fpsu_full fpsu_full.cpp iblt.cpp protocol.cpp)
target_include_directories(fpsu_full PRIVATE ${LIBOTE_INCLUDE_DIRS})
target_link_libraries(fpsu_full PRIVATE
    ${LIBOTE_LIBS}
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
    "${LIBOTE_STAGE}/lib/libsodium.dylib"
)
target_compile_options(fpsu_full PRIVATE -O3 -march=native+crypto)

if(APPLE)
    target_link_libraries(benchmark PRIVATE
        "-framework CoreFoundation"
        "-framework Security"
    )
    target_link_libraries(fpsu_net PRIVATE
        "-framework CoreFoundation"
        "-framework Security"
    )
    target_link_libraries(fpsu_full PRIVATE
        "-framework CoreFoundation"
        "-framework Security"
    )
endif()

message(STATUS "libOTe build: ${LIBOTE_BUILD}")
