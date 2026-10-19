cmake_minimum_required(VERSION 3.20)
include("${CMAKE_CURRENT_LIST_DIR}/cmake/RetroPicoESVersions.cmake")

set(NOIZ2SA_DEPS_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../_deps"
    CACHE PATH "noiz2sa external dependency root")
get_filename_component(NOIZ2SA_DEPS_DIR "${NOIZ2SA_DEPS_DIR}" ABSOLUTE)
get_filename_component(NOIZ2SA_PORT_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

set(NOIZ2SA_PORT_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/port/include")

# ---------------------------------------------------------------------------
# Pinned engine: clone, verify commit, apply the recorded main-rename patch
# with forward/reverse checks.
# ---------------------------------------------------------------------------
set(NOIZ2SA_SOURCE_DIR "${NOIZ2SA_DEPS_DIR}/noiz2sa" CACHE PATH
    "Pinned noiz2sa engine source")
set(DEPENDENCY_NAME noiz2sa)
set(DEPENDENCY_REPOSITORY "${NOIZ2SA_REPOSITORY}")
set(DEPENDENCY_COMMIT "${NOIZ2SA_COMMIT}")
set(DEPENDENCY_SOURCE_DIR "${NOIZ2SA_SOURCE_DIR}")
set(DEPENDENCY_PATCH "${NOIZ2SA_PATCH}")
set(DEPENDENCY_PATCH_SHA256 "${NOIZ2SA_PATCH_SHA256}")
set(DEPENDENCY_REQUIRED_FILE "src/noiz2sa.c")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/BootstrapGitDependency.cmake")

# ---------------------------------------------------------------------------
# libBulletML and stb_vorbis.
# ---------------------------------------------------------------------------
include("${CMAKE_CURRENT_LIST_DIR}/cmake/BootstrapLibBulletML.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/BootstrapStbVorbis.cmake")

project(noiz2sa_retropicoes_host LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

add_subdirectory("${NOIZ2SA_PORT_ROOT}/compat"
                 "${CMAKE_BINARY_DIR}/compat-v1")

set(NOIZ2SA_ENGINE_SOURCES
    ${NOIZ2SA_SOURCE_DIR}/src/attractmanager.c
    ${NOIZ2SA_SOURCE_DIR}/src/background.c
    ${NOIZ2SA_SOURCE_DIR}/src/bonus.c
    ${NOIZ2SA_SOURCE_DIR}/src/clrtbl.c
    ${NOIZ2SA_SOURCE_DIR}/src/degutil.c
    ${NOIZ2SA_SOURCE_DIR}/src/frag.c
    ${NOIZ2SA_SOURCE_DIR}/src/letterrender.c
    ${NOIZ2SA_SOURCE_DIR}/src/noiz2sa.c
    ${NOIZ2SA_SOURCE_DIR}/src/rand.c
    ${NOIZ2SA_SOURCE_DIR}/src/ship.c
    ${NOIZ2SA_SOURCE_DIR}/src/shot.c
    ${NOIZ2SA_SOURCE_DIR}/src/vector.c
    ${NOIZ2SA_SOURCE_DIR}/src/barragemanager.cc
    ${NOIZ2SA_SOURCE_DIR}/src/foe.cc
    ${NOIZ2SA_SOURCE_DIR}/src/foecommand.cc
)

set(NOIZ2SA_BULLETML_SOURCES
    ${NOIZ2SA_DEPS_DIR}/libbulletml/bulletml/bulletmlparser.cpp
    ${NOIZ2SA_DEPS_DIR}/libbulletml/bulletml/bulletmlparser-tinyxml.cpp
    ${NOIZ2SA_DEPS_DIR}/libbulletml/bulletml/bulletmltree.cpp
    ${NOIZ2SA_DEPS_DIR}/libbulletml/bulletml/bulletmlrunner.cpp
    ${NOIZ2SA_DEPS_DIR}/libbulletml/bulletml/bulletmlrunnerimpl.cpp
    ${NOIZ2SA_DEPS_DIR}/libbulletml/bulletml/calc.cpp
    ${NOIZ2SA_DEPS_DIR}/libbulletml/bulletml/formula-variables.cpp
    ${NOIZ2SA_DEPS_DIR}/libbulletml/bulletml/tinyxml/tinyxml.cpp
    ${NOIZ2SA_DEPS_DIR}/libbulletml/bulletml/tinyxml/tinyxmlparser.cpp
    ${NOIZ2SA_DEPS_DIR}/libbulletml/bulletml/tinyxml/tinyxmlerror.cpp
)

# ---------------------------------------------------------------------------
# Target layering (S17-C refactor). The previous layout compiled every unit
# into one executable target, so every include directory and every workaround
# flag hit every translation unit, and S17-D had no reusable piece to build on.
#
#   noiz2sa_engine_headers   INTERFACE  upstream include contract + flags
#   noiz2sa_port_headers     INTERFACE  the port's public headers (SDL shim)
#   noiz2sa_game             STATIC     engine + BulletML + SDL adapters
#   noiz2sa_host_backend     STATIC     libc compat v1 backend (host only)
#   noiz2sa_stub_backend     STATIC     ISO-C-only compat v1 backend (S17-D probe)
#   noiz2sa_host_smoke       EXE        S17-B/C host replay + digest + PNG
#   noiz2sa_meminfo          EXE        S17-B header-size evidence
#   noiz2sa_target_probe     EXE        S17-D boundary check: game + stub backend
#
# The engine and the SDL replacement adapters must share one archive: the
# adapters implement interfaces the engine declares (drawLine, flipScreen,
# SDL_GetTicks, ...) while also reading engine globals (status, color, ...),
# so separate archives would form a static-link cycle that single-pass
# archive scanning cannot resolve. "noiz2sa_game" is therefore the unit that
# a target build (S17-D) reuses verbatim, supplying its own backend in place
# of noiz2sa_host_backend.
# ---------------------------------------------------------------------------

# Upstream include contract. The flags here are not cosmetic: GCC 14 stopped
# leaking <cstring> declarations into the bison-generated calc.cpp, and the
# bundled vintage boost lightweight mutex picks an MSVC-only spinlock on
# Win64. Both are properties of consuming these headers, so they travel with
# this interface target instead of being attached to one executable.
add_library(noiz2sa_engine_headers INTERFACE)
add_library(RetroPicoES::noiz2sa_engine_headers ALIAS noiz2sa_engine_headers)

target_include_directories(noiz2sa_engine_headers INTERFACE
    ${NOIZ2SA_SOURCE_DIR}/src
    ${NOIZ2SA_DEPS_DIR}/libbulletml
    ${NOIZ2SA_DEPS_DIR}/libbulletml/bulletml
    ${NOIZ2SA_DEPS_DIR}/stb
)

target_compile_definitions(noiz2sa_engine_headers INTERFACE
    BOOST_LWM_USE_CRITICAL_SECTION
)

target_compile_options(noiz2sa_engine_headers INTERFACE
    $<$<COMPILE_LANGUAGE:CXX>:-include cstring>
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-declarations>
)

# The port's public headers. This directory must stay first on every include
# path so "SDL.h" resolves to the port shim rather than a system SDL.
add_library(noiz2sa_port_headers INTERFACE)
add_library(RetroPicoES::noiz2sa_port_headers ALIAS noiz2sa_port_headers)

target_include_directories(noiz2sa_port_headers INTERFACE
    ${NOIZ2SA_PORT_INCLUDE_DIR}
)

# Engine + BulletML + stb_vorbis + the port's SDL/screen/sound replacement.
# Identical on the host and on the target.
add_library(noiz2sa_game STATIC
    ${NOIZ2SA_ENGINE_SOURCES}
    ${NOIZ2SA_BULLETML_SOURCES}
    ${NOIZ2SA_DEPS_DIR}/stb/stb_vorbis.c
    port/src/sdl_shim.c
    port/src/screen.c
    port/src/sound_stub.c
)
add_library(RetroPicoES::noiz2sa_game ALIAS noiz2sa_game)

target_link_libraries(noiz2sa_game PUBLIC
    noiz2sa_engine_headers
    noiz2sa_port_headers
    retropicoes_compat_v1
)

# Host (libc) compat v1 backend. Replaced, never shared, on the target.
add_library(noiz2sa_host_backend STATIC
    port/host/compat_host.c
)
add_library(RetroPicoES::noiz2sa_host_backend ALIAS noiz2sa_host_backend)

target_link_libraries(noiz2sa_host_backend PUBLIC retropicoes_compat_v1)

# S17-B/C host evidence target: deterministic replay, state/frame digest and
# optional PNG dump.
add_executable(noiz2sa_host_smoke
    port/host/noiz2sa_host_main.c
    port/host/noiz2sa_state_digest.cpp
    port/host/noiz2sa_png_dump.c
)

target_link_libraries(noiz2sa_host_smoke PRIVATE
    noiz2sa_game
    noiz2sa_host_backend
)

# S17-B memory layout evidence: header-only sizes and budget assertions.
add_executable(noiz2sa_meminfo tests/test_memory_layout.cpp)

target_link_libraries(noiz2sa_meminfo PRIVATE
    noiz2sa_engine_headers
    noiz2sa_port_headers
)

# ---------------------------------------------------------------------------
# S17-D boundary check. Two properties, both decided by the toolchain rather
# than asserted in prose:
#
#   * noiz2sa_stub_backend implements compat v1 with ISO C alone and links
#     *only* retropicoes_compat_v1 -- no engine headers, no port headers.
#     That is the exact shape the RP2350B backend must have.
#
#   * noiz2sa_target_probe is noiz2sa_game plus that backend, so a game
#     archive that dragged in any host-only symbol would fail to link here,
#     on the host, without waiting for S17-D hardware.
#
# scripts/test-target-boundary.ps1 drives both and compares the probe's
# framebuffer digest against noiz2sa_host_smoke's, which is why the two
# runners share a replay scenario.
# ---------------------------------------------------------------------------
add_library(noiz2sa_stub_backend STATIC
    port/target/compat_stub.c
)
add_library(RetroPicoES::noiz2sa_stub_backend ALIAS noiz2sa_stub_backend)

target_link_libraries(noiz2sa_stub_backend PUBLIC retropicoes_compat_v1)

add_executable(noiz2sa_target_probe
    port/target/noiz2sa_target_probe.c
)

target_link_libraries(noiz2sa_target_probe PRIVATE
    noiz2sa_game
    noiz2sa_stub_backend
)
