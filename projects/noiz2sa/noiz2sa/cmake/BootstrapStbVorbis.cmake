cmake_minimum_required(VERSION 3.20)

# Reproducible stb_vorbis fetch: download the pinned raw file, verify its
# SHA-256 and place it under _deps/stb/. The file is public domain; see the
# license block at the end of the file itself.

if(NOT DEFINED NOIZ2SA_DEPS_DIR)
    message(FATAL_ERROR "BootstrapStbVorbis.cmake requires NOIZ2SA_DEPS_DIR")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/RetroPicoESVersions.cmake")

get_filename_component(_deps_root "${NOIZ2SA_DEPS_DIR}" ABSOLUTE)
set(_out_file "${_deps_root}/stb/stb_vorbis.c")

if(NOT EXISTS "${_out_file}")
    file(MAKE_DIRECTORY "${_deps_root}/stb")
    file(DOWNLOAD "${NOIZ2SA_STB_VORBIS_URL}" "${_out_file}"
         TIMEOUT 120
         STATUS _dl_status)
    list(GET _dl_status 0 _dl_rc)
    if(NOT _dl_rc EQUAL 0)
        message(FATAL_ERROR "stb_vorbis download failed: ${_dl_status}")
    endif()
    file(SHA256 "${_out_file}" _sha256)
    string(TOLOWER "${_sha256}" _sha256)
    if(NOT "${_sha256}" STREQUAL "${NOIZ2SA_STB_VORBIS_SHA256}")
        message(FATAL_ERROR
            "stb_vorbis SHA-256 mismatch: expected "
            "${NOIZ2SA_STB_VORBIS_SHA256}, got ${_sha256}")
    endif()
endif()

message(STATUS "stb_vorbis ready: ${_out_file}")
