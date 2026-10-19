cmake_minimum_required(VERSION 3.20)

set(projectname spaceinvaders)
set(HW_CONFIG 16 CACHE STRING "RetroPicoES hardware configuration")
set(USE_HSTX 1 CACHE BOOL "Use RP2350 HSTX output")
set(FORCE_DVI 0 CACHE BOOL "Use DVI video-only mode")
option(BUILD_FOR_BOOTLOADER
       "Relink Space Invaders into pico-bootLoader's application partition" ON)

if(NOT HW_CONFIG EQUAL 16)
    message(FATAL_ERROR "This target is only for RetroPicoES v2.1 HW_CONFIG=16")
endif()

get_filename_component(SPACEINVADERS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
get_filename_component(WORKSPACE_ROOT "${SPACEINVADERS_ROOT}/.." ABSOLUTE)
if(NOT DEFINED RETROPICOES_PLATFORM_BASE)
    set(RETROPICOES_PLATFORM_BASE "${WORKSPACE_ROOT}/digger")
endif()
get_filename_component(RETROPICOES_PLATFORM_BASE
                       "${RETROPICOES_PLATFORM_BASE}" ABSOLUTE)
set(PICO_SHARED_PATH "${RETROPICOES_PLATFORM_BASE}/pico_shared")
set(COMMON_PLATFORM "${RETROPICOES_PLATFORM_BASE}/platform/retropicoes")
set(COMMON_MENU "${WORKSPACE_ROOT}/common/menu")
set(COMMON_FRONTEND "${WORKSPACE_ROOT}/common/frontend")

if(NOT EXISTS "${PICO_SHARED_PATH}/BoardConfigs.cmake")
    message(FATAL_ERROR "Patched pico_shared HW16 platform baseline is missing")
endif()
if(NOT DEFINED PICO_PIO_USB_PATH)
    set(PICO_PIO_USB_PATH "${WORKSPACE_ROOT}/_deps/Pico-PIO-USB")
endif()
if(NOT DEFINED RETROPICOES_TUSB_XINPUT_PATH)
    set(RETROPICOES_TUSB_XINPUT_PATH "${WORKSPACE_ROOT}/_deps/tusb_xinput")
endif()

if(DEFINED PICO_SDK_PATH AND NOT PICO_SDK_PATH STREQUAL "")
    set(ENV{PICO_SDK_PATH} "${PICO_SDK_PATH}")
elseif(DEFINED ENV{PICO_SDK_PATH})
    set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
else()
    message(FATAL_ERROR "PICO_SDK_PATH must point to Pico SDK 2.3.0+")
endif()

include("${PICO_SHARED_PATH}/BoardConfigs.cmake")
list(PREPEND PICO_BOARD_HEADER_DIRS "${PICO_SHARED_PATH}/boards")
include("${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
set(FAMILY rp2040)
include("${PICO_SDK_PATH}/lib/tinyusb/hw/bsp/family_support.cmake")

project(${projectname} C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

add_compile_definitions(
    PICO_DEFAULT_UART=0
    PICO_DEFAULT_UART_TX_PIN=${PICO_DEFAULT_UART_TX_PIN}
    PICO_DEFAULT_UART_RX_PIN=${PICO_DEFAULT_UART_RX_PIN})

pico_sdk_init()
if(ENABLE_PIO_USB EQUAL 1)
    check_and_add_pico_pio_usb_support()
endif()
if(PICO_SDK_VERSION_STRING VERSION_LESS "2.3.0" OR
   NOT TARGET hardware_psram OR NOT PICO_BOARD STREQUAL "pico2b")
    message(FATAL_ERROR "Space Invaders HW16 requires Pico SDK 2.3.0+ and pico2b")
endif()

add_executable(${projectname}
    ${SPACEINVADERS_ROOT}/platform/retropicoes/main.cpp
    ${SPACEINVADERS_ROOT}/platform/retropicoes/frontend.c
    ${SPACEINVADERS_ROOT}/platform/retropicoes/video_adapter.c
    ${SPACEINVADERS_ROOT}/platform/retropicoes/input_adapter.c
    ${SPACEINVADERS_ROOT}/platform/retropicoes/audio_adapter.c
    ${SPACEINVADERS_ROOT}/platform/retropicoes/state_adapter.c
    ${SPACEINVADERS_ROOT}/platform/retropicoes/perf_report.c
    ${SPACEINVADERS_ROOT}/platform/retropicoes/settings_adapter.c
    ${COMMON_MENU}/menu_contract.c
    ${COMMON_MENU}/menu_input.c
    ${COMMON_MENU}/menu_state.c
    ${COMMON_MENU}/menu_render.c
    ${COMMON_MENU}/settings_menu.c
    ${COMMON_FRONTEND}/rom_browser.c
    ${COMMON_FRONTEND}/frontend_shell.c
    ${COMMON_FRONTEND}/settings_host.c
    ${SPACEINVADERS_ROOT}/core/space_invaders_board.c
    ${SPACEINVADERS_ROOT}/core/space_invaders_cpu.c
    ${SPACEINVADERS_ROOT}/third_party/superzazu8080/i8080.c
    ${WORKSPACE_ROOT}/arcade-template/src/arcade_machine.c
    ${WORKSPACE_ROOT}/compat/video.c
    ${WORKSPACE_ROOT}/compat/audio.c
    ${COMMON_PLATFORM}/runtime.cpp
    ${COMMON_PLATFORM}/fatfs_compat.c
    ${COMMON_PLATFORM}/input_backend.cpp
    ${COMMON_PLATFORM}/input_mapping.c
    ${COMMON_PLATFORM}/video_backend.c
    ${COMMON_PLATFORM}/audio_backend.c
    ${PICO_SHARED_PATH}/FrensHelpers.cpp
    ${PICO_SHARED_PATH}/gamepad.cpp
    ${PICO_SHARED_PATH}/hid_app.cpp
    ${PICO_SHARED_PATH}/nespad.cpp
    ${PICO_SHARED_PATH}/drivers/pico_psram/PicoPlusPsram.cpp)

add_subdirectory("${PICO_SHARED_PATH}" "${CMAKE_CURRENT_BINARY_DIR}/pico_shared"
                 EXCLUDE_FROM_ALL)
add_subdirectory("${RETROPICOES_TUSB_XINPUT_PATH}"
                 "${CMAKE_CURRENT_BINARY_DIR}/tusb_xinput")

pico_set_program_name(${projectname} "spaceinvaders")
pico_set_program_version(${projectname} "s12-menu1")
pico_enable_stdio_uart(${projectname} 1)
pico_enable_stdio_usb(${projectname} 0)

target_compile_definitions(${projectname} PRIVATE
    SDLPoP_PLATFORM_PICO=1
    RETROPICOES_BUILD_BANNER="SPACEINVADERS BUILD: s12-menu1"
    RETROPICOES_SYSTEM_CLOCK_KHZ=252000u
    RETROPICOES_DIRECT_SRAM_RENDER=1
    RETROPICOES_WAIT_READY_US=20000u
    RETROPICOES_BOOTLOADER_IMAGE=$<BOOL:${BUILD_FOR_BOOTLOADER}>
    RETROPICOES_ENABLE_DIRECT_LOADER_RETURN=0
    RETROPICOES_LOADER_RETURN_HOLD_MS=1500u
    HSTX=1 RETROPICOES_HSTX_RGB565=1 NOHSTXCLOCK=1
    FORCE_DVI=$<BOOL:${FORCE_DVI}> HW_CONFIG=${HW_CONFIG}
    GPIOHSTXD0=${GPIOHSTXD0} GPIOHSTXD1=${GPIOHSTXD1}
    GPIOHSTXD2=${GPIOHSTXD2} GPIOHSTXCK=${GPIOHSTXCK}
    GPIOHSTXINVERTED=${GPIOHSTXINVERTED}
    SDCARD_PIN_CS=${SD_CS} SDCARD_PIN_SCK=${SD_SCK}
    SDCARD_PIN_MOSI=${SD_MOSI} SDCARD_PIN_MISO=${SD_MISO}
    SDCARD_SPI=${SD_SPI} SDCARD_PIO=pio0 LED_GPIO_PIN=${LED_GPIO_PIN}
    NES_PIN_CLK=${NES_CLK} NES_PIN_DATA=${NES_DATA} NES_PIN_LAT=${NES_LAT}
    NES_PIO=${NES_PIO} NES_PIN_CLK_1=${NES_CLK_1}
    NES_PIN_DATA_1=${NES_DATA_1} NES_PIN_LAT_1=${NES_LAT_1}
    NES_PIO_1=${NES_PIO_1} WII_PIN_SDA=${WII_SDA}
    WII_PIN_SCL=${WII_SCL} WIIPAD_I2C=${WIIPAD_I2C}
    PIO_USB_USE_PIO=${PIO_USB_USE_PIO}
    CFG_TUH_RPI_PIO_USB=${ENABLE_PIO_USB}
    PICO_DEFAULT_PIO_USB_DP_PIN=${PIO_DP_PLUS_PIN}
    PICO_SECOND_PIO_USB_DP_PIN=${PIO_DP_PLUS_PIN_2}
    PICO_PIO_USE_GPIO_BASE=1 PICO_RP2040_USB_DEVICE_ENUMERATION_FIX=1
    PSRAM_CS_PIN=${PSRAM_CS_PIN})

target_include_directories(${projectname} PRIVATE
    ${SPACEINVADERS_ROOT}/core
    ${SPACEINVADERS_ROOT}/third_party/superzazu8080
    ${SPACEINVADERS_ROOT}/platform/retropicoes
    ${COMMON_MENU}
    ${COMMON_FRONTEND}
    ${WORKSPACE_ROOT}/arcade-template/include
    ${WORKSPACE_ROOT}/compat
    ${COMMON_PLATFORM}
    ${PICO_SHARED_PATH}
    ${PICO_SHARED_PATH}/drivers/i2c_bus_recovery
    ${PICO_SHARED_PATH}/drivers/pico_audio_i2s
    ${PICO_SHARED_PATH}/drivers/pico_hdmi
    ${PICO_SHARED_PATH}/drivers/pico_psram
    ${PICO_SHARED_PATH}/drivers/tlv320dac3100)

set_source_files_properties(
    ${SPACEINVADERS_ROOT}/core/space_invaders_board.c
    ${SPACEINVADERS_ROOT}/core/space_invaders_cpu.c
    ${SPACEINVADERS_ROOT}/third_party/superzazu8080/i8080.c
    ${SPACEINVADERS_ROOT}/platform/retropicoes/video_adapter.c
    PROPERTIES COMPILE_OPTIONS "-O3")
target_compile_options(${projectname} PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-Werror=implicit-function-declaration>
    -ffunction-sections -fdata-sections)

target_link_libraries(${projectname} PRIVATE
    pico_stdlib hardware_clocks hardware_psram hardware_xip_cache
    hardware_adc hardware_dma hardware_i2c hardware_pio hardware_pwm
    hardware_spi hardware_uart pico_multicore pico_rand pico_fatfs pico_hdmi
    lwmem tinyusb_host tinyusb_board tinyusb_pico_pio_usb xinput_host m)
target_link_options(${projectname} PRIVATE
    -Xlinker --print-memory-usage --data-sections)

if(BUILD_FOR_BOOTLOADER)
    include("${PICO_SHARED_PATH}/BootPartition.cmake")
    frens_offset_for_bootloader(${projectname})
endif()
pico_add_extra_outputs(${projectname})

message(STATUS "RetroPicoES HW_CONFIG=${HW_CONFIG}, board=${PICO_BOARD}")
message(STATUS "Space Invaders video=224x256 scaled to 210x240 RGB565")
message(STATUS "Space Invaders audio=22050Hz mono to 44100Hz HDMI on core1")
message(STATUS "BOOTLOADER=${BUILD_FOR_BOOTLOADER}")
