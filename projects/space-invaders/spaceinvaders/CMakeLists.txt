cmake_minimum_required(VERSION 3.16)
project(retropicoes_space_invaders_core C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(retropicoes_arcade_template
    ../arcade-template/src/arcade_machine.c
    ../arcade-template/src/arcade_frame_handoff.c)
target_include_directories(retropicoes_arcade_template PUBLIC
    ../arcade-template/include)

add_library(superzazu_i8080
    third_party/superzazu8080/i8080.c)
target_include_directories(superzazu_i8080 PUBLIC
    third_party/superzazu8080)

add_library(space_invaders_cpu
    core/space_invaders_cpu.c
    core/space_invaders_board.c)
target_include_directories(space_invaders_cpu PUBLIC core)
target_link_libraries(space_invaders_cpu PUBLIC
    retropicoes_arcade_template superzazu_i8080)

foreach(target retropicoes_arcade_template superzazu_i8080 space_invaders_cpu)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Werror)
    endif()
endforeach()

enable_testing()
add_executable(space_invaders_cpu_tests tests/test_space_invaders_cpu.c)
target_link_libraries(space_invaders_cpu_tests PRIVATE space_invaders_cpu)
if(MSVC)
    target_compile_options(space_invaders_cpu_tests PRIVATE /W4 /WX)
else()
    target_compile_options(space_invaders_cpu_tests PRIVATE
        -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME space_invaders_cpu_tests COMMAND space_invaders_cpu_tests)

add_executable(space_invaders_board_tests tests/test_space_invaders_board.c)
target_link_libraries(space_invaders_board_tests PRIVATE space_invaders_cpu)
if(MSVC)
    target_compile_options(space_invaders_board_tests PRIVATE /W4 /WX)
else()
    target_compile_options(space_invaders_board_tests PRIVATE
        -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME space_invaders_board_tests COMMAND space_invaders_board_tests)

add_executable(space_invaders_video_tests
    tests/test_video_adapter.c
    platform/retropicoes/video_adapter.c)
target_include_directories(space_invaders_video_tests PRIVATE
    platform/retropicoes)
if(MSVC)
    target_compile_options(space_invaders_video_tests PRIVATE /W4 /WX)
else()
    target_compile_options(space_invaders_video_tests PRIVATE
        -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME space_invaders_video_tests COMMAND space_invaders_video_tests)

add_executable(space_invaders_input_tests
    tests/test_input_adapter.c
    platform/retropicoes/input_adapter.c)
target_include_directories(space_invaders_input_tests PRIVATE
    platform/retropicoes
    ../compat)
target_link_libraries(space_invaders_input_tests PRIVATE space_invaders_cpu)
if(MSVC)
    target_compile_options(space_invaders_input_tests PRIVATE /W4 /WX)
else()
    target_compile_options(space_invaders_input_tests PRIVATE
        -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME space_invaders_input_tests COMMAND space_invaders_input_tests)

add_executable(space_invaders_audio_tests
    tests/test_audio_adapter.c
    platform/retropicoes/audio_adapter.c)
target_include_directories(space_invaders_audio_tests PRIVATE
    platform/retropicoes)
if(MSVC)
    target_compile_options(space_invaders_audio_tests PRIVATE /W4 /WX)
else()
    target_compile_options(space_invaders_audio_tests PRIVATE
        -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME space_invaders_audio_tests COMMAND space_invaders_audio_tests)

add_executable(space_invaders_state_tests
    tests/test_state_adapter.c
    platform/retropicoes/state_adapter.c)
target_include_directories(space_invaders_state_tests PRIVATE
    platform/retropicoes)
target_link_libraries(space_invaders_state_tests PRIVATE space_invaders_cpu)
if(MSVC)
    target_compile_options(space_invaders_state_tests PRIVATE /W4 /WX)
else()
    target_compile_options(space_invaders_state_tests PRIVATE
        -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME space_invaders_state_tests COMMAND space_invaders_state_tests)

add_executable(space_invaders_perf_tests
    tests/test_perf_report.c
    platform/retropicoes/perf_report.c)
target_include_directories(space_invaders_perf_tests PRIVATE
    platform/retropicoes)
if(MSVC)
    target_compile_options(space_invaders_perf_tests PRIVATE /W4 /WX)
else()
    target_compile_options(space_invaders_perf_tests PRIVATE
        -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME space_invaders_perf_tests COMMAND space_invaders_perf_tests)

add_executable(space_invaders_handoff_tests tests/test_frame_handoff.c)
target_link_libraries(space_invaders_handoff_tests PRIVATE
    retropicoes_arcade_template)
if(MSVC)
    target_compile_options(space_invaders_handoff_tests PRIVATE /W4 /WX)
else()
    target_compile_options(space_invaders_handoff_tests PRIVATE
        -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME space_invaders_handoff_tests COMMAND space_invaders_handoff_tests)

add_executable(space_invaders_frontend_tests
    tests/test_frontend.c
    platform/retropicoes/frontend.c
    platform/retropicoes/input_adapter.c
    platform/retropicoes/audio_adapter.c
    platform/retropicoes/video_adapter.c)
target_include_directories(space_invaders_frontend_tests PRIVATE
    platform/retropicoes
    ../compat)
target_link_libraries(space_invaders_frontend_tests PRIVATE space_invaders_cpu)
if(MSVC)
    target_compile_options(space_invaders_frontend_tests PRIVATE /W4 /WX)
else()
    target_compile_options(space_invaders_frontend_tests PRIVATE
        -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME space_invaders_frontend_tests COMMAND space_invaders_frontend_tests)
