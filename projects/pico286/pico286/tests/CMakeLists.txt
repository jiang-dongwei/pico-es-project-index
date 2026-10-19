cmake_minimum_required(VERSION 3.20)
project(pico286_host_tests C)

enable_testing()

if(NOT DEFINED PICO286_UPSTREAM_DIR)
    message(FATAL_ERROR "PICO286_UPSTREAM_DIR must point to the pinned pico-286 checkout")
endif()

set(PICO286_CONTROLLER_TEST
    "${PICO286_UPSTREAM_DIR}/tests/i82_controllers_test.c")
if(NOT EXISTS "${PICO286_CONTROLLER_TEST}")
    message(FATAL_ERROR "Missing upstream controller test: ${PICO286_CONTROLLER_TEST}")
endif()

add_executable(pico286_i82_test "${PICO286_CONTROLLER_TEST}")
if(MSVC)
    target_compile_options(pico286_i82_test PRIVATE /W4 /WX)
else()
    target_compile_options(pico286_i82_test PRIVATE
        -Wall -Wextra -Werror -Wconversion -Wshadow)
endif()

add_test(NAME pico286_i82_controllers COMMAND pico286_i82_test)

add_executable(pico286_platform_contract
    test_platform_contract.c
    ../platform/retropicoes/platform_contract.c)
target_include_directories(pico286_platform_contract PRIVATE
    ../platform/retropicoes)
if(MSVC)
    target_compile_options(pico286_platform_contract PRIVATE /W4 /WX)
else()
    target_compile_options(pico286_platform_contract PRIVATE
        -Wall -Wextra -Werror -Wconversion -Wshadow)
endif()
add_test(NAME pico286_platform_contract COMMAND pico286_platform_contract)

add_executable(pico286_board_profile
    test_board_profile.c
    ../platform/retropicoes/board_profile.c
    ../platform/retropicoes/memory_layout.c)
target_include_directories(pico286_board_profile PRIVATE
    ../platform/retropicoes)
if(MSVC)
    target_compile_options(pico286_board_profile PRIVATE /W4 /WX)
else()
    target_compile_options(pico286_board_profile PRIVATE
        -Wall -Wextra -Werror -Wconversion -Wshadow)
endif()
add_test(NAME pico286_board_profile COMMAND pico286_board_profile)

add_executable(pico286_memory_layout
    test_memory_layout.c
    ../platform/retropicoes/memory_layout.c)
target_include_directories(pico286_memory_layout PRIVATE
    ../platform/retropicoes)
if(MSVC)
    target_compile_options(pico286_memory_layout PRIVATE /W4 /WX)
else()
    target_compile_options(pico286_memory_layout PRIVATE
        -Wall -Wextra -Werror -Wconversion -Wshadow)
endif()
add_test(NAME pico286_memory_layout COMMAND pico286_memory_layout)

add_executable(pico286_video_scanline
    test_video_scanline.c
    ../platform/retropicoes/video_scanline.c)
target_include_directories(pico286_video_scanline PRIVATE
    ../platform/retropicoes)
if(MSVC)
    target_compile_options(pico286_video_scanline PRIVATE /W4 /WX)
else()
    target_compile_options(pico286_video_scanline PRIVATE
        -Wall -Wextra -Werror -Wconversion -Wshadow)
endif()
add_test(NAME pico286_video_scanline COMMAND pico286_video_scanline)

add_executable(pico286_core_events
    test_core_events.c
    ../platform/retropicoes/core_events.c)
target_include_directories(pico286_core_events PRIVATE
    ../platform/retropicoes)
if(MSVC)
    target_compile_options(pico286_core_events PRIVATE /W4 /WX)
else()
    target_compile_options(pico286_core_events PRIVATE
        -Wall -Wextra -Werror -Wconversion -Wshadow)
endif()
add_test(NAME pico286_core_events COMMAND pico286_core_events)
