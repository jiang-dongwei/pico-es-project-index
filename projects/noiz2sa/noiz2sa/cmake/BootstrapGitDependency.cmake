cmake_minimum_required(VERSION 3.20)

# Reproducible git dependency bootstrap.
#
# Stream policy: several git commands write to stdout AND stderr on their
# *success* path -- "git clone" prints progress, and
# "git checkout --detach" prints the carried-over modified files to stdout
# plus "HEAD is now at ..." to stderr. Any caller that redirects this process
# (PowerShell 5.1 turns redirected native stderr into a terminating error,
# CI log capture, "Tee-Object", ...) would abort the build on a command that
# actually succeeded. Each git invocation therefore captures both streams into
# variables and only surfaces them through its own FATAL_ERROR message.

find_package(Git REQUIRED)

if(NOT DEFINED DEPENDENCY_NAME OR NOT DEFINED DEPENDENCY_REPOSITORY OR
   NOT DEFINED DEPENDENCY_COMMIT OR NOT DEFINED DEPENDENCY_SOURCE_DIR OR
   NOT DEFINED DEPENDENCY_REQUIRED_FILE)
    message(FATAL_ERROR "BootstrapGitDependency.cmake is missing parameters")
endif()

get_filename_component(DEPENDENCY_SOURCE_DIR
                       "${DEPENDENCY_SOURCE_DIR}" ABSOLUTE)
if(NOT EXISTS "${DEPENDENCY_SOURCE_DIR}/.git")
    get_filename_component(_parent "${DEPENDENCY_SOURCE_DIR}" DIRECTORY)
    file(MAKE_DIRECTORY "${_parent}")
    message(STATUS "Cloning ${DEPENDENCY_NAME} into ${DEPENDENCY_SOURCE_DIR}")
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" clone --no-tags --quiet
                "${DEPENDENCY_REPOSITORY}" "${DEPENDENCY_SOURCE_DIR}"
        RESULT_VARIABLE _clone_rc
        ERROR_VARIABLE _clone_err)
    if(NOT _clone_rc EQUAL 0)
        message(FATAL_ERROR
            "Failed to clone ${DEPENDENCY_NAME} (${_clone_rc})\n${_clone_err}")
    endif()
endif()

set(_patch_already_applied FALSE)
if(DEFINED DEPENDENCY_PATCH)
    if(NOT EXISTS "${DEPENDENCY_PATCH}")
        message(FATAL_ERROR "${DEPENDENCY_NAME} patch is missing")
    endif()
    file(SHA256 "${DEPENDENCY_PATCH}" _patch_sha256)
    string(TOLOWER "${_patch_sha256}" _patch_sha256)
    if(NOT "${_patch_sha256}" STREQUAL "${DEPENDENCY_PATCH_SHA256}")
        message(FATAL_ERROR
            "${DEPENDENCY_NAME} patch checksum mismatch")
    endif()
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" apply --reverse --check
                "${DEPENDENCY_PATCH}"
        WORKING_DIRECTORY "${DEPENDENCY_SOURCE_DIR}"
        RESULT_VARIABLE _reverse_rc
        OUTPUT_QUIET ERROR_QUIET)
    if(_reverse_rc EQUAL 0)
        set(_patch_already_applied TRUE)
    endif()
endif()

execute_process(
    COMMAND "${GIT_EXECUTABLE}" status --porcelain
    WORKING_DIRECTORY "${DEPENDENCY_SOURCE_DIR}"
    OUTPUT_VARIABLE _status
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _status_rc
    ERROR_VARIABLE _status_err)
if(NOT _status_rc EQUAL 0)
    message(FATAL_ERROR
        "Cannot inspect ${DEPENDENCY_NAME}\n${_status_err}")
endif()
if(NOT _status STREQUAL "" AND NOT _patch_already_applied)
    message(FATAL_ERROR
        "Refusing to overwrite dirty ${DEPENDENCY_NAME}:\n${_status}")
endif()

execute_process(
    COMMAND "${GIT_EXECUTABLE}" checkout --detach "${DEPENDENCY_COMMIT}"
    WORKING_DIRECTORY "${DEPENDENCY_SOURCE_DIR}"
    RESULT_VARIABLE _checkout_rc
    OUTPUT_VARIABLE _checkout_out
    ERROR_VARIABLE _checkout_err)
if(NOT _checkout_rc EQUAL 0)
    message(FATAL_ERROR
        "Cannot check out ${DEPENDENCY_NAME} ${DEPENDENCY_COMMIT}\n"
        "${_checkout_out}${_checkout_err}")
endif()

execute_process(
    COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
    WORKING_DIRECTORY "${DEPENDENCY_SOURCE_DIR}"
    OUTPUT_VARIABLE _head
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_VARIABLE _head_err)
if(NOT "${_head}" STREQUAL "${DEPENDENCY_COMMIT}")
    message(FATAL_ERROR
        "Unexpected ${DEPENDENCY_NAME} HEAD: ${_head}\n${_head_err}")
endif()

if(DEFINED DEPENDENCY_PATCH AND NOT _patch_already_applied)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" apply --check "${DEPENDENCY_PATCH}"
        WORKING_DIRECTORY "${DEPENDENCY_SOURCE_DIR}"
        RESULT_VARIABLE _check_rc
        OUTPUT_VARIABLE _check_out
        ERROR_VARIABLE _check_err)
    if(NOT _check_rc EQUAL 0)
        message(FATAL_ERROR
            "${DEPENDENCY_NAME} patch does not apply to the pinned commit\n"
            "${_check_out}${_check_err}")
    endif()
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" apply --whitespace=nowarn
                "${DEPENDENCY_PATCH}"
        WORKING_DIRECTORY "${DEPENDENCY_SOURCE_DIR}"
        RESULT_VARIABLE _apply_rc
        OUTPUT_VARIABLE _apply_out
        ERROR_VARIABLE _apply_err)
    if(NOT _apply_rc EQUAL 0)
        message(FATAL_ERROR
            "Failed to patch ${DEPENDENCY_NAME}\n"
            "${_apply_out}${_apply_err}")
    endif()
endif()
if(NOT EXISTS "${DEPENDENCY_SOURCE_DIR}/${DEPENDENCY_REQUIRED_FILE}")
    message(FATAL_ERROR
        "${DEPENDENCY_NAME} is missing ${DEPENDENCY_REQUIRED_FILE}")
endif()

message(STATUS "${DEPENDENCY_NAME} ready: ${DEPENDENCY_SOURCE_DIR}")
message(STATUS "${DEPENDENCY_NAME} commit: ${_head}")
if(DEFINED DEPENDENCY_PATCH)
    message(STATUS "${DEPENDENCY_NAME} patch: ${_patch_sha256}")
endif()
