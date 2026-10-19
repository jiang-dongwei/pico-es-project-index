# BootPartition.cmake - Shared flash memory map for the emuLoader bootloader.
#
# The pico_emuLoader bootloader lives at the very start of flash and stays
# resident; emulators are relinked to run from the application partition right
# after it, so the bootloader can flash an emulator and jump to it while the
# bootloader survives every reset / power-cycle.
#
# This file is the SINGLE SOURCE OF TRUTH for the map. It is included by:
#   - the bootloader (pico_emuLoader/CMakeLists.txt) -> frens_link_as_bootloader()
#   - every emulator built with -DBUILD_FOR_BOOTLOADER=ON -> frens_offset_for_bootloader()
# and the same numbers are passed to the bootloader's C code (boot_config.h) as
# compile definitions, so the linker and the loader can never disagree.
#
# Flash layout (Adafruit Fruit Jam, 16 MB):
#
#   0x10000000  +-----------------------------+  <- bootrom always boots this
#               |   Bootloader (emuLoader)    |     image (the menu/flasher).
#               |        512 KB               |
#   0x10080000  +-----------------------------+  <- FRENS_APP_BASE
#               |   Application partition      |     emulator UF2s land here.
#               |        15.5 MB              |     the bootloader jumps here.
#   0x11000000  +-----------------------------+
#
# Override on the command line for other boards, e.g.:
#   cmake -DFRENS_FLASH_TOTAL=0x800000 ...   # 8 MB board

if(DEFINED _FRENS_BOOTPARTITION_INCLUDED)
    return()
endif()
set(_FRENS_BOOTPARTITION_INCLUDED 1)

set(FRENS_XIP_BASE        "0x10000000" CACHE STRING "RP2350 XIP flash base")
set(FRENS_BOOTLOADER_SIZE "0x80000"    CACHE STRING "Bytes reserved for the resident bootloader (512 KB)")
set(FRENS_FLASH_TOTAL     "0x1000000"  CACHE STRING "Total external flash on the board (Fruit Jam = 16 MB)")

# Derived addresses.
math(EXPR FRENS_APP_BASE "${FRENS_XIP_BASE} + ${FRENS_BOOTLOADER_SIZE}" OUTPUT_FORMAT HEXADECIMAL)
math(EXPR FRENS_APP_SIZE  "${FRENS_FLASH_TOTAL} - ${FRENS_BOOTLOADER_SIZE}" OUTPUT_FORMAT HEXADECIMAL)

# ---------------------------------------------------------------------------
# _frens_relink_flash(<target> <origin_hex> <length_hex>)
#
# Copy the SDK's default RP2350 memory map and rewrite only the FLASH(rx)
# ORIGIN/LENGTH line, then point <target> at it. Adapted from the proven
# rp2350-uf2-bootloader/cmake/patch_memmap.cmake. Keeps us compatible with SDK
# updates (everything else in the script is untouched).
#
# Must be called AFTER the target exists and AFTER pico_sdk_init().
# ---------------------------------------------------------------------------
function(_frens_relink_flash TARGET ORIGIN LENGTH)
    set(_candidates
        # SDK 2.2.x moved the platform linker scripts out of rp2_common.
        "${PICO_SDK_PATH}/src/rp2350/pico_platform/memmap_default.ld"
        "${PICO_SDK_PATH}/src/rp2_common/pico_crt0/rp2350/memmap_default.ld"
        "${PICO_SDK_PATH}/src/rp2_common/pico_standard_link/memmap_default.ld"
    )
    set(_src_ld "")
    foreach(_c IN LISTS _candidates)
        if(EXISTS "${_c}")
            set(_src_ld "${_c}")
            break()
        endif()
    endforeach()
    if(_src_ld STREQUAL "")
        file(GLOB_RECURSE _found
             "${PICO_SDK_PATH}/src/rp2350/*/memmap_default.ld"
             "${PICO_SDK_PATH}/src/rp2_common/*rp2350*/memmap_default.ld")
        if(_found)
            list(GET _found 0 _src_ld)
        endif()
    endif()
    if(_src_ld STREQUAL "")
        message(FATAL_ERROR
            "BootPartition: could not find an RP2350 memmap_default.ld under "
            "${PICO_SDK_PATH}. Build for the rp2350 platform with SDK >= 2.0.0.")
    endif()

    file(READ "${_src_ld}" _ld_text)

    # Newer SDKs pull the FLASH region in from a generated pico_flash_region.ld
    # (which carries the board's full flash size). Replace that INCLUDE with an
    # explicit, capped FLASH region so the bootloader / app partition can never
    # overlap and the linker errors if an image overflows its partition.
    string(REPLACE
        "INCLUDE \"pico_flash_region.ld\""
        "FLASH(rx) : ORIGIN = ${ORIGIN}, LENGTH = ${LENGTH}"
        _ld_text "${_ld_text}")
    # Older SDKs spell the region out inline; rewrite that form too (idempotent
    # if the INCLUDE replacement above already produced this line).
    string(REGEX REPLACE
        "FLASH\\(rx\\)[ \t]*:[^\n]*"
        "FLASH(rx) : ORIGIN = ${ORIGIN}, LENGTH = ${LENGTH}"
        _ld_text "${_ld_text}")

    # Verify the rewrite actually happened. On SDK 2.2.x memmap_default.ld is
    # only 'INCLUDE "memmap_default.incl"' and the FLASH region lives several
    # includes down, in a generated pico_flash_region.ld -- so neither
    # substitution above matches and the image would silently link at
    # 0x10000000, on top of the bootloader.
    if(NOT _ld_text MATCHES "ORIGIN = ${ORIGIN}")
        if(NOT COMMAND pico_add_linker_script_override_path)
            message(FATAL_ERROR
                "BootPartition: could not relocate FLASH in ${_src_ld}, and this "
                "SDK has no pico_add_linker_script_override_path to fall back on. "
                "${TARGET} would link at 0x10000000 and overwrite the bootloader.")
        endif()
        # Sanctioned mechanism: supply our own pico_flash_region.ld ahead of the
        # generated one, leaving the rest of the memmap include tree untouched.
        set(_ovr_dir "${CMAKE_CURRENT_BINARY_DIR}/frens_ldovr_${TARGET}")
        file(MAKE_DIRECTORY "${_ovr_dir}")
        file(WRITE "${_ovr_dir}/pico_flash_region.ld"
             "FLASH(rx) : ORIGIN = ${ORIGIN}, LENGTH = ${LENGTH}\n")
        pico_add_linker_script_override_path(${TARGET} "${_ovr_dir}")
        message(STATUS "BootPartition: ${TARGET} -> ORIGIN=${ORIGIN} LENGTH=${LENGTH} (pico_flash_region.ld override)")
    else()
        set(_out_ld "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_frens_memmap.ld")
        file(WRITE "${_out_ld}" "${_ld_text}")
        message(STATUS "BootPartition: ${TARGET} -> ORIGIN=${ORIGIN} LENGTH=${LENGTH} (from ${_src_ld})")
        pico_set_linker_script(${TARGET} "${_out_ld}")
    endif()

    # Override the SDK's compile-time flash-size assumption to match the actual
    # chip. The board header (e.g. adafruit_fruit_jam.h) may set PICO_FLASH_SIZE_BYTES
    # to a value smaller than the real flash (e.g. 8 MB on a 16 MB board), which
    # makes hard_assert in flash_range_erase / flash_range_program panic when the
    # bootloader or an emulator writes at higher offsets (e.g. near the top of a
    # 16 MB partition). FRENS_FLASH_TOTAL is the single source of truth here.
    target_compile_definitions(${TARGET} PRIVATE
        PICO_FLASH_SIZE_BYTES=${FRENS_FLASH_TOTAL})
    # picotool re-detects the chip from the ELF instead of trusting --family,
    # and assumes RP2040 for images that don't start at 0x10000000. Our
    # relinked images do, so tell it explicitly. Pico SDK 2.3.0+ requires a
    # matching picotool and therefore guarantees this option. This direct path
    # is also important for a clean build: the SDK-provided picotool is not on
    # PATH yet while this function runs, so find_program() alone would omit the
    # argument on the first configure and UF2 conversion would reject RAM
    # segments. Keep the probe only for older SDK/tool combinations.
    if(PICO_PLATFORM MATCHES "rp2350")
        if(PICO_SDK_VERSION_STRING VERSION_GREATER_EQUAL "2.3.0")
            set_target_properties(${TARGET} PROPERTIES
                PICOTOOL_EXTRA_UF2_ARGS "--platform;rp2350")
        else()
            find_program(_frens_picotool picotool)
            if(_frens_picotool)
                # Some 2.2.0 builds report >= 2.2.0 but have no --platform on
                # "uf2 convert", so probe the command rather than the version.
                execute_process(COMMAND ${_frens_picotool} help uf2 convert
                                OUTPUT_VARIABLE _frens_pt_help
                                ERROR_VARIABLE _frens_pt_help_err
                                OUTPUT_STRIP_TRAILING_WHITESPACE)
                if("${_frens_pt_help}${_frens_pt_help_err}" MATCHES "\\-\\-platform")
                    set_target_properties(${TARGET} PROPERTIES
                        PICOTOOL_EXTRA_UF2_ARGS "--platform;rp2350")
                endif()
            endif()
        endif()
    endif()

endfunction()

# Link the bootloader into the reserved boot region at the start of flash.
function(frens_link_as_bootloader TARGET)
    _frens_relink_flash(${TARGET} ${FRENS_XIP_BASE} ${FRENS_BOOTLOADER_SIZE})
endfunction()

# Link an emulator into the application partition (its UF2 will then target the
# partition, and the bootloader writes it verbatim and jumps there).
function(frens_offset_for_bootloader TARGET)
    _frens_relink_flash(${TARGET} ${FRENS_APP_BASE} ${FRENS_APP_SIZE})
endfunction()
