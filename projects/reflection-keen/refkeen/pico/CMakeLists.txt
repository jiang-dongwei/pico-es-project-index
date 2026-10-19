cmake_minimum_required(VERSION 3.20)

set(projectname refkeen)
set(HW_CONFIG 16 CACHE STRING "RetroPicoES hardware configuration")
option(REFKEEN_INDEXED_FAST_PATH "Use experimental indexed video path" OFF)
set(USE_HSTX 1 CACHE BOOL "Use RP2350 HSTX output")
set(FORCE_DVI 0 CACHE BOOL "Use DVI video-only mode")
option(BUILD_FOR_BOOTLOADER
       "Relink Reflection Keen into the Loader application partition" ON)

if(NOT HW_CONFIG EQUAL 16)
    message(FATAL_ERROR "Reflection Keen supports only RetroPicoES HW_CONFIG=16")
endif()

get_filename_component(REFKEEN_WORKSPACE "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(REFKEEN_UPSTREAM_PATH "${REFKEEN_WORKSPACE}/_deps/reflectionhle-upstream"
    CACHE PATH "Pinned ReflectionHLE release-20260606 checkout")
set(REFKEEN_PICO_SHARED_PATH "" CACHE PATH
    "Read-only path to the main window's patched RetroPicoES pico_shared checkout")
set(PICO_PIO_USB_PATH "${REFKEEN_WORKSPACE}/_deps/Pico-PIO-USB" CACHE PATH
    "Pinned Pico-PIO-USB checkout")
set(RETROPICOES_TUSB_XINPUT_PATH "${REFKEEN_WORKSPACE}/_deps/tusb_xinput"
    CACHE PATH "Pinned tusb_xinput checkout")

if(NOT EXISTS "${REFKEEN_UPSTREAM_PATH}/src/kdreams/kd_main.c")
    message(FATAL_ERROR "Pinned ReflectionHLE source is missing; run the S20 bootstrap first")
endif()
if(REFKEEN_PICO_SHARED_PATH STREQUAL "" OR
   NOT EXISTS "${REFKEEN_PICO_SHARED_PATH}/BoardConfigs.cmake")
    message(FATAL_ERROR
        "REFKEEN_PICO_SHARED_PATH must name a main-window-owned patched pico_shared checkout")
endif()
if(NOT DEFINED PICO_SDK_PATH OR PICO_SDK_PATH STREQUAL "")
    if(DEFINED ENV{PICO_SDK_PATH})
        set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
    else()
        message(FATAL_ERROR "PICO_SDK_PATH must point to Pico SDK 2.3.0+")
    endif()
endif()
set(ENV{PICO_SDK_PATH} "${PICO_SDK_PATH}")

include("${REFKEEN_PICO_SHARED_PATH}/BoardConfigs.cmake")
# BoardConfigs predates external shared-tree selection and appends
# ${CMAKE_SOURCE_DIR}/pico_shared/boards.  The parallel-task rules prohibit this
# port from creating or editing that shared tree, so add the selected read-only
# checkout explicitly before the SDK resolves PICO_BOARD=pico2b.
list(APPEND PICO_BOARD_HEADER_DIRS "${REFKEEN_PICO_SHARED_PATH}/boards")
include("${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
set(FAMILY rp2040)
include("${PICO_SDK_PATH}/lib/tinyusb/hw/bsp/family_support.cmake")

project(${projectname} C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

add_compile_definitions(
    PICO_DEFAULT_UART=0
    PICO_DEFAULT_UART_TX_PIN=${PICO_DEFAULT_UART_TX_PIN}
    PICO_DEFAULT_UART_RX_PIN=${PICO_DEFAULT_UART_RX_PIN}
)

pico_sdk_init()
if(ENABLE_PIO_USB EQUAL 1)
    check_and_add_pico_pio_usb_support()
endif()
if(PICO_SDK_VERSION_STRING VERSION_LESS "2.3.0" OR
   NOT TARGET hardware_psram OR NOT PICO_BOARD STREQUAL "pico2b")
    message(FATAL_ERROR "Reflection Keen HW16 requires Pico SDK 2.3.0+ and pico2b")
endif()

set(U "${REFKEEN_UPSTREAM_PATH}/src")
configure_file("${REFKEEN_UPSTREAM_PATH}/be_git_id.h.in"
               "${CMAKE_CURRENT_BINARY_DIR}/be_git_id.h")

# Exact release-20260606 base set, minus the audited SDL/launcher/filesystem/
# alt-control boundary.  Keep this explicit: a glob would silently absorb a new
# upstream TU without assigning its platform ownership.
set(REFKEEN_ENGINE_SOURCES
    ${U}/backend/audio/be_audio_digi.c
    ${U}/backend/audio/be_audio_init.c
    ${U}/backend/audio/be_audio_main_thread.c
    ${U}/backend/audio/be_audio_mixer.c
    ${U}/backend/audio/be_audio_opl.c
    ${U}/backend/audio/be_audio_pcspkr.c
    ${U}/backend/audio/be_audio_resampling.c
    ${U}/backend/cfg/be_cfg.c
    ${U}/backend/filesystem/be_filesystem_file.c
    ${U}/backend/filesystem/be_filesystem_file_ops.c
    ${U}/backend/filesystem/be_filesystem_gameinst.c
    ${U}/backend/filesystem/be_filesystem_mods.c
    ${U}/backend/filesystem/be_filesystem_root_paths.c
    ${U}/backend/gamedefs/be_gamedefs_accessors.c
    ${U}/backend/gamedefs/be_gamedefs_prep.c
    ${U}/backend/input/be_input.c
    ${U}/backend/input/be_input_tables.c
    ${U}/backend/input/be_input_ui.c
    ${U}/backend/misc/be_misc_steamcfg.c
    ${U}/backend/misc/be_misc_lpt_null.c
    ${U}/backend/startup/be_startup.c
    ${U}/backend/startup/be_startup_embedded_data.c
    ${U}/backend/timing/be_timing.c
    ${U}/backend/timing/be_timing_callback.c
    ${U}/backend/video/be_video_emu.c
    ${U}/backend/video/be_video.c
    ${U}/backend/video/be_video_fonts.c
    ${U}/backend/video/be_video_textures.c
    ${U}/backend/video/be_video_ui.c
    ${U}/be_cross.c
    ${U}/be_cross_doszeroseg.c
    ${U}/be_cross_emm.c
    ${U}/be_cross_mem.c
    ${U}/be_cross_xmm.c
    ${U}/crc32/crc32.c
    ${U}/depklite/depklite.c
    ${U}/nukedopl/opl3.c
    ${U}/unlzexe/unlzexe.c
)

set(REFKEEN_GAME_SOURCES
    ${U}/kdreams/gelib.c
    ${U}/kdreams/id_ca.c
    ${U}/kdreams/id_in.c
    ${U}/kdreams/id_mm.c
    ${U}/kdreams/id_rf_a.c
    ${U}/kdreams/id_rf.c
    ${U}/kdreams/id_sd.c
    ${U}/kdreams/id_us_a.c
    ${U}/kdreams/id_us.c
    ${U}/kdreams/id_vw_a.c
    ${U}/kdreams/id_vw_ac.c
    ${U}/kdreams/id_vw_ae.c
    ${U}/kdreams/id_vw.c
    ${U}/kdreams/jam_io.c
    ${U}/kdreams/kd_act1.c
    ${U}/kdreams/kd_act2.c
    ${U}/kdreams/kd_demo.c
    ${U}/kdreams/kd_keen.c
    ${U}/kdreams/kd_main.c
    ${U}/kdreams/kd_play.c
    ${U}/kdreams/key_scancode_converters.c
    ${U}/kdreams/lzhuf.c
    ${U}/kdreams/soft.c
    ${U}/kdreams/statetype_ptr_conversion.c
    ${U}/kdreams/lscr/loadscn2.c
)

set(REFKEEN_PORT_SOURCES
    ../platform/retropicoes/main.c
    ../platform/retropicoes/be_startup_port.c
    ../platform/retropicoes/be_events_port.c
    ../platform/retropicoes/be_filesystem_dir.c
    ../platform/retropicoes/be_app_paths.c
    ../platform/retropicoes/fatfs_compat.c
    ../platform/retropicoes/path_mapping.c
    ../platform/retropicoes/stdio_compat.c
    ../platform/retropicoes/input_mapping.c
    ../platform/retropicoes/event_source.c
    ../platform/retropicoes/altcontrol_stub.c
    ../platform/retropicoes/frontend.c
    ../platform/retropicoes/runtime.cpp
    ../platform/retropicoes/input_backend.cpp
    ../platform/retropicoes/audio_backend.c
    ../platform/retropicoes/video_backend.c
    ../platform/retropicoes/indexed_video.c
)

set(RETROPICOES_COMMON_MENU "${REFKEEN_WORKSPACE}/common/menu")
set(RETROPICOES_COMMON_FRONTEND "${REFKEEN_WORKSPACE}/common/frontend")
set(REFKEEN_SHARED_FRONTEND_SOURCES
    ${RETROPICOES_COMMON_MENU}/menu_contract.c
    ${RETROPICOES_COMMON_MENU}/menu_input.c
    ${RETROPICOES_COMMON_MENU}/menu_state.c
    ${RETROPICOES_COMMON_MENU}/menu_render.c
    ${RETROPICOES_COMMON_MENU}/settings_menu.c
    ${RETROPICOES_COMMON_FRONTEND}/native_game_settings.c
    ${RETROPICOES_COMMON_FRONTEND}/frontend_shell.c
    ${RETROPICOES_COMMON_FRONTEND}/settings_host.c
)

# Keep configuring while a backend is being implemented so every completed TU
# receives a real Pico SDK compile. Missing files are excluded loudly; the
# unresolved platform ABI then prevents the link and therefore prevents a fake
# UF2 from being produced.
set(REFKEEN_EXISTING_PORT_SOURCES)
foreach(source IN LISTS REFKEEN_PORT_SOURCES)
    if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/${source}")
        list(APPEND REFKEEN_EXISTING_PORT_SOURCES ${source})
    else()
        message(WARNING "Reflection Keen platform source not implemented: ${source}")
    endif()
endforeach()

add_executable(${projectname}
    ${REFKEEN_ENGINE_SOURCES}
    ${REFKEEN_GAME_SOURCES}
    ${REFKEEN_EXISTING_PORT_SOURCES}
    ${REFKEEN_SHARED_FRONTEND_SOURCES}
    ${REFKEEN_PICO_SHARED_PATH}/FrensHelpers.cpp
    ${REFKEEN_PICO_SHARED_PATH}/gamepad.cpp
    ${REFKEEN_PICO_SHARED_PATH}/hid_app.cpp
    ${REFKEEN_PICO_SHARED_PATH}/nespad.cpp
    ${REFKEEN_PICO_SHARED_PATH}/drivers/pico_psram/PicoPlusPsram.cpp
)

add_subdirectory("${REFKEEN_PICO_SHARED_PATH}"
                 "${CMAKE_CURRENT_BINARY_DIR}/pico_shared" EXCLUDE_FROM_ALL)
add_subdirectory("${RETROPICOES_TUSB_XINPUT_PATH}"
                 "${CMAKE_CURRENT_BINARY_DIR}/tusb_xinput")

target_compile_definitions(${projectname} PRIVATE
    REFKEEN_INDEXED_FAST_PATH=$<BOOL:${REFKEEN_INDEXED_FAST_PATH}>
    REFKEEN_VER_KDREAMS=1
    REFKEEN_HAS_VER_KDREAMS=1
    REFKEEN_DIAG_DISABLE_HDMI_AUDIO=1
    RETROPICOES_ENABLE_DIRECT_LOADER_RETURN=0
    RETROPICOES_BOOTLOADER_IMAGE=$<BOOL:${BUILD_FOR_BOOTLOADER}>
    HSTX=1
    RETROPICOES_HSTX_RGB565=1
    NOHSTXCLOCK=1
    FORCE_DVI=$<BOOL:${FORCE_DVI}>
    HW_CONFIG=${HW_CONFIG}
    GPIOHSTXD0=${GPIOHSTXD0}
    GPIOHSTXD1=${GPIOHSTXD1}
    GPIOHSTXD2=${GPIOHSTXD2}
    GPIOHSTXCK=${GPIOHSTXCK}
    GPIOHSTXINVERTED=${GPIOHSTXINVERTED}
    SDCARD_PIN_CS=${SD_CS}
    SDCARD_PIN_SCK=${SD_SCK}
    SDCARD_PIN_MOSI=${SD_MOSI}
    SDCARD_PIN_MISO=${SD_MISO}
    SDCARD_SPI=${SD_SPI}
    SDCARD_PIO=pio0
    NES_PIN_CLK=${NES_CLK}
    NES_PIN_DATA=${NES_DATA}
    NES_PIN_LAT=${NES_LAT}
    NES_PIO=${NES_PIO}
    NES_PIN_CLK_1=${NES_CLK_1}
    NES_PIN_DATA_1=${NES_DATA_1}
    NES_PIN_LAT_1=${NES_LAT_1}
    NES_PIO_1=${NES_PIO_1}
    PIO_USB_USE_PIO=${PIO_USB_USE_PIO}
    CFG_TUH_RPI_PIO_USB=${ENABLE_PIO_USB}
    PICO_DEFAULT_PIO_USB_DP_PIN=${PIO_DP_PLUS_PIN}
    PICO_SECOND_PIO_USB_DP_PIN=${PIO_DP_PLUS_PIN_2}
    PICO_PIO_USE_GPIO_BASE=1
    PSRAM_CS_PIN=${PSRAM_CS_PIN}
)

target_include_directories(${projectname} PRIVATE
    ../host/include
    ../host/shim
    ../platform/retropicoes
    ${REFKEEN_WORKSPACE}/compat
    ${RETROPICOES_COMMON_MENU}
    ${RETROPICOES_COMMON_FRONTEND}
    ${U}
    ${U}/kdreams
    ${U}/kdreams/lscr
    ${REFKEEN_PICO_SHARED_PATH}
    ${REFKEEN_PICO_SHARED_PATH}/drivers/pico_fatfs
    ${REFKEEN_PICO_SHARED_PATH}/drivers/pico_audio_i2s
    ${REFKEEN_PICO_SHARED_PATH}/drivers/tlv320dac3100
    ${REFKEEN_PICO_SHARED_PATH}/drivers/i2c_bus_recovery
    ${REFKEEN_PICO_SHARED_PATH}/drivers/pico_hdmi
    ${REFKEEN_PICO_SHARED_PATH}/drivers/pico_psram
    ${PICO_PIO_USB_PATH}/src
)

# newlib stdio is not FatFS. Redirect only pinned upstream TUs; the Pico SDK,
# common frontend and port diagnostics continue to use their native stdio.
set_source_files_properties(${REFKEEN_ENGINE_SOURCES} ${REFKEEN_GAME_SOURCES}
    PROPERTIES COMPILE_DEFINITIONS
    "fopen=retropicoes_fopen;fclose=retropicoes_fclose;fread=retropicoes_fread;fwrite=retropicoes_fwrite;fseek=retropicoes_fseek;ftell=retropicoes_ftell;fgetc=retropicoes_fgetc;fgets=retropicoes_fgets;feof=retropicoes_feof;ferror=retropicoes_ferror;fprintf=retropicoes_fprintf;remove=retropicoes_remove;rename=retropicoes_rename;mkdir=retropicoes_mkdir"
    COMPILE_OPTIONS "-include;${CMAKE_CURRENT_LIST_DIR}/../platform/retropicoes/upstream_heap.h"
)

# Upstream be_video_ui.c relies on SDL's transitive string declarations.
# The Pico shim deliberately keeps SDL minimal, so supply the C header without
# carrying a local patch against the pinned upstream tree.
set_source_files_properties(${U}/backend/video/be_video_ui.c
    PROPERTIES COMPILE_OPTIONS "-include;string.h")

if(REFKEEN_INDEXED_FAST_PATH)
    target_link_options(${projectname} PRIVATE
        "LINKER:--wrap=BEL_ST_UpdateHostDisplay")
endif()

set_source_files_properties(${REFKEEN_EXISTING_PORT_SOURCES}
    PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-Werror;-Wno-unused-function")

target_compile_options(${projectname} PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-Werror=implicit-function-declaration>
    -fno-common -ffunction-sections -fdata-sections
)

target_link_libraries(${projectname} PRIVATE
    pico_stdlib hardware_psram hardware_clocks hardware_dma hardware_i2c
    hardware_pio hardware_pwm hardware_spi pico_multicore pico_fatfs
    pico_hdmi pico_rand lwmem tinyusb_host tinyusb_board m
    tinyusb_pico_pio_usb xinput_host
)

target_link_options(${projectname} PRIVATE
    "-T${CMAKE_CURRENT_LIST_DIR}/psram_bss.ld"
    -Xlinker --print-memory-usage
)

pico_set_program_name(${projectname} "refkeen")
pico_set_program_version(${projectname} "s20-c19")
pico_enable_stdio_uart(${projectname} 1)
pico_enable_stdio_usb(${projectname} 0)
# The ELF intentionally contains non-loadable symbols in memory-mapped PSRAM.
# Picotool's ELF validator rejects its unrelated SRAM NOBITS segment, so emit
# UF2 from the flash-only BIN at the boot-partition origin instead.
set(PICO_NO_UF2 1)
pico_add_extra_outputs(${projectname})
if(BUILD_FOR_BOOTLOADER)
    set(REFKEEN_UF2_ORIGIN 0x10080000)
else()
    set(REFKEEN_UF2_ORIGIN 0x10000000)
endif()
add_custom_command(TARGET ${projectname} POST_BUILD
    COMMAND $<TARGET_FILE:picotool> uf2 convert --quiet
            "${CMAKE_CURRENT_BINARY_DIR}/${projectname}.bin"
            "${CMAKE_CURRENT_BINARY_DIR}/${projectname}.uf2"
            -o ${REFKEEN_UF2_ORIGIN} --family rp2350-arm-s --abs-block
    BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/${projectname}.uf2"
    VERBATIM)

if(BUILD_FOR_BOOTLOADER)
    include("${REFKEEN_PICO_SHARED_PATH}/BootPartition.cmake")
    frens_offset_for_bootloader(${projectname})
endif()
