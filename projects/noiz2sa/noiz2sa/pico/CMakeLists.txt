cmake_minimum_required(VERSION 3.20)

set(projectname noiz2sa)
set(HW_CONFIG 16 CACHE STRING "RetroPicoES hardware configuration")
set(BUILD_FOR_BOOTLOADER OFF CACHE BOOL "Relink into pico-bootLoader application partition")
set(PICO_SHARED_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../xrick/pico_shared"
    CACHE PATH "Pinned shared RetroPicoES board layer")
set(PICO_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../_deps/pico-sdk-2.3.0"
    CACHE PATH "Pinned Pico SDK")

if(NOT HW_CONFIG EQUAL 16)
    message(FATAL_ERROR "noiz2sa requires RetroPicoES HW_CONFIG=16")
endif()
if(NOT EXISTS "${PICO_SHARED_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "pico_shared is missing: ${PICO_SHARED_SOURCE_DIR}")
endif()
if(NOT EXISTS "${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
    message(FATAL_ERROR "Pico SDK is missing: ${PICO_SDK_PATH}")
endif()

set(ENV{PICO_SDK_PATH} "${PICO_SDK_PATH}")
set(PICO_BOARD_HEADER_DIRS "${PICO_SHARED_SOURCE_DIR}/boards")
include("${PICO_SHARED_SOURCE_DIR}/BoardConfigs.cmake")
include("${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
project(${projectname} C CXX ASM)
pico_sdk_init()

add_executable(${projectname}
    main.c
    ../port/target/compat_pico.c
)
target_include_directories(${projectname} PRIVATE
    ../port/include
    ../../compat
    ${PICO_SHARED_SOURCE_DIR}
)
target_compile_definitions(${projectname} PRIVATE
    SDLPoP_PLATFORM_PICO=1
    HW_CONFIG=${HW_CONFIG}
    RETROPICOES_BOOTLOADER_IMAGE=$<BOOL:${BUILD_FOR_BOOTLOADER}>
    HSTX=1
    RETROPICOES_HSTX_RGB565=1
    NOHSTXCLOCK=1
)
add_subdirectory("${PICO_SHARED_SOURCE_DIR}" pico_shared EXCLUDE_FROM_ALL)
target_link_libraries(${projectname} PRIVATE pico_stdlib)
pico_enable_stdio_uart(${projectname} 1)
pico_enable_stdio_usb(${projectname} 0)
# Extra UF2 outputs are enabled after the target links cleanly. Keeping the
# first milestone to ELF/BIN avoids forcing picotool's host-side self-test
# during every iteration on this workstation.
