cmake_minimum_required(VERSION 3.20)

# Reproducible libBulletML fetch: download the upstream author's 0.0.6
# tarball, verify its SHA-256, extract and lay out the flat include root the
# engine expects ("bulletml/bulletmlparser.h" and "tinyxml/tinyxml.h").
# Only the tinyxml parser units are used; the yggdrasil and xerces parser
# variants are extracted but never compiled.

if(NOT DEFINED NOIZ2SA_DEPS_DIR)
    message(FATAL_ERROR "BootstrapLibBulletML.cmake requires NOIZ2SA_DEPS_DIR")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/RetroPicoESVersions.cmake")

get_filename_component(_deps_root "${NOIZ2SA_DEPS_DIR}" ABSOLUTE)
set(_tarball "${_deps_root}/libbulletml-0.0.6.tar.bz2")
set(_out_dir "${_deps_root}/libbulletml")

if(NOT EXISTS "${_out_dir}/bulletml/bulletmlparser.h" OR
   NOT EXISTS "${_out_dir}/bulletml/tinyxml/tinyxml.h")
    file(DOWNLOAD "${NOIZ2SA_BULLETML_URL}" "${_tarball}"
         TIMEOUT 120
         STATUS _dl_status)
    list(GET _dl_status 0 _dl_rc)
    if(NOT _dl_rc EQUAL 0)
        message(FATAL_ERROR
            "libBulletML download failed: ${_dl_status}")
    endif()
    file(SHA256 "${_tarball}" _sha256)
    string(TOLOWER "${_sha256}" _sha256)
    if(NOT "${_sha256}" STREQUAL "${NOIZ2SA_BULLETML_SHA256}")
        message(FATAL_ERROR
            "libBulletML tarball SHA-256 mismatch: expected "
            "${NOIZ2SA_BULLETML_SHA256}, got ${_sha256}")
    endif()
    file(REMOVE_RECURSE "${_deps_root}/libbulletml-tmp" "${_out_dir}")
    file(ARCHIVE_EXTRACT
         INPUT "${_tarball}"
         DESTINATION "${_deps_root}/libbulletml-tmp")
    # Tarball layout: bulletml/src/*.cpp|.h and bulletml/src/tinyxml/.
    file(COPY "${_deps_root}/libbulletml-tmp/bulletml/src/"
         DESTINATION "${_out_dir}/bulletml")
    file(REMOVE_RECURSE "${_deps_root}/libbulletml-tmp")
endif()

if(NOT EXISTS "${_out_dir}/bulletml/bulletmlparser.h" OR
   NOT EXISTS "${_out_dir}/bulletml/bulletmlparser-tinyxml.h" OR
   NOT EXISTS "${_out_dir}/bulletml/tinyxml/tinyxml.h" OR
   NOT EXISTS "${_out_dir}/bulletml/bulletmlrunner.h")
    message(FATAL_ERROR "libBulletML layout is incomplete after bootstrap")
endif()

message(STATUS "libBulletML 0.0.6 ready: ${_out_dir}")
