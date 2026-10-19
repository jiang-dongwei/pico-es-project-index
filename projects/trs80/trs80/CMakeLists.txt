cmake_minimum_required(VERSION 3.16)
project(trs80_core C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(RETROPICOES_COMMON_MENU_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../common/menu"
    CACHE PATH "Path to the RetroPicoES common menu module")
if(NOT EXISTS "${RETROPICOES_COMMON_MENU_DIR}/menu_contract.c")
    message(FATAL_ERROR "RETROPICOES_COMMON_MENU_DIR is invalid")
endif()
set(RETROPICOES_COMMON_FRONTEND_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../common/frontend"
    CACHE PATH "Path to the RetroPicoES common frontend module")
if(NOT EXISTS "${RETROPICOES_COMMON_FRONTEND_DIR}/frontend_shell.c")
    message(FATAL_ERROR "RETROPICOES_COMMON_FRONTEND_DIR is invalid")
endif()

add_library(retropicoes_common_menu
    ${RETROPICOES_COMMON_MENU_DIR}/menu_contract.c
    ${RETROPICOES_COMMON_MENU_DIR}/menu_input.c
    ${RETROPICOES_COMMON_MENU_DIR}/menu_state.c
    ${RETROPICOES_COMMON_MENU_DIR}/menu_render.c
    ${RETROPICOES_COMMON_MENU_DIR}/menu_runtime.c)
target_include_directories(retropicoes_common_menu PUBLIC
    ${RETROPICOES_COMMON_MENU_DIR})

add_library(retropicoes_common_frontend
    ${RETROPICOES_COMMON_FRONTEND_DIR}/rom_browser.c
    ${RETROPICOES_COMMON_FRONTEND_DIR}/frontend_shell.c
    ${RETROPICOES_COMMON_FRONTEND_DIR}/settings_host.c)
target_include_directories(retropicoes_common_frontend PUBLIC
    ${RETROPICOES_COMMON_FRONTEND_DIR})
target_link_libraries(retropicoes_common_frontend PUBLIC
    retropicoes_common_menu)

add_library(trs80_core core/trs80.c core/disk.c third_party/xtrs/z80.c)
target_include_directories(trs80_core PUBLIC core PRIVATE third_party/xtrs)

add_library(trs80_retropicoes_contract
    platform/retropicoes/contract.c
    platform/retropicoes/virtual_keyboard.c
    platform/retropicoes/video_adapter.c
    third_party/xtrs/trs_chars.c)
target_include_directories(trs80_retropicoes_contract PUBLIC
    platform/retropicoes ${RETROPICOES_COMMON_MENU_DIR}
    ${RETROPICOES_COMMON_FRONTEND_DIR})
target_link_libraries(trs80_retropicoes_contract PUBLIC
    retropicoes_common_frontend trs80_core)

foreach(target retropicoes_common_menu retropicoes_common_frontend trs80_core
               trs80_retropicoes_contract)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Werror)
    endif()
endforeach()

enable_testing()
add_executable(trs80_disk_tests tests/test_disk.c)
target_link_libraries(trs80_disk_tests PRIVATE trs80_core)
target_compile_options(trs80_disk_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME trs80_disk_controller COMMAND trs80_disk_tests)
add_executable(trs80_av_tests tests/test_av.c tests/video_reference.c)
target_link_libraries(trs80_av_tests PRIVATE trs80_retropicoes_contract)
target_compile_options(trs80_av_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME trs80_video_audio COMMAND trs80_av_tests)
add_executable(trs80_tests tests/test_core.c)
target_link_libraries(trs80_tests PRIVATE
    trs80_core trs80_retropicoes_contract)
if(MSVC)
    target_compile_options(trs80_tests PRIVATE /W4 /WX)
else()
    target_compile_options(trs80_tests PRIVATE
        -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME trs80_tests COMMAND trs80_tests)
add_executable(trs80_machine_tests tests/test_machine.c)
target_link_libraries(trs80_machine_tests PRIVATE trs80_core)
if(MSVC)
    target_compile_options(trs80_machine_tests PRIVATE /W4 /WX)
else()
    target_compile_options(trs80_machine_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
set(machine_cases cpu memory keyboard loader interrupt determinism)
set(case_index 0)
foreach(case_name IN LISTS machine_cases)
    math(EXPR case_index "${case_index} + 1")
    add_test(NAME trs80_${case_name} COMMAND trs80_machine_tests ${case_index})
endforeach()
set(TRS80_RUNTIME_DATA "${CMAKE_CURRENT_SOURCE_DIR}/../_deps/runtime-data"
    CACHE PATH "Optional downloaded local test media")
foreach(model 1 3 4)
    if(EXISTS "${TRS80_RUNTIME_DATA}/bios/model${model}.rom")
        add_test(NAME trs80_bios_model${model}
            COMMAND trs80_runtime_probe ${model} "${TRS80_RUNTIME_DATA}/bios/model${model}.rom")
    endif()
endforeach()
add_executable(trs80_runtime_probe tests/probe_runtime.c)
foreach(pair "1;ld1-531;1" "3;ld3-531;3" "4;ld4-631;3")
    list(GET pair 0 model)
    list(GET pair 1 disk)
    list(GET pair 2 format)
    if(EXISTS "${TRS80_RUNTIME_DATA}/bios/model${model}.rom" AND
       EXISTS "${TRS80_RUNTIME_DATA}/roms/${disk}/${disk}.dsk")
        add_test(NAME trs80_disk_boot_model${model} COMMAND trs80_runtime_probe
            ${model} "${TRS80_RUNTIME_DATA}/bios/model${model}.rom"
            "${TRS80_RUNTIME_DATA}/roms/${disk}/${disk}.dsk" ${format})
    endif()
endforeach()
target_link_libraries(trs80_runtime_probe PRIVATE trs80_retropicoes_contract)
if(MSVC)
    target_compile_options(trs80_runtime_probe PRIVATE /W4 /WX)
else()
    target_compile_options(trs80_runtime_probe PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

if(EXISTS "${TRS80_RUNTIME_DATA}/roms/gobbler4/gobbler4.dsk" AND
   EXISTS "${TRS80_RUNTIME_DATA}/bios/model4.rom")
    add_test(NAME trs80_gobbler4_boot COMMAND trs80_runtime_probe
        4 "${TRS80_RUNTIME_DATA}/bios/model4.rom"
        "${TRS80_RUNTIME_DATA}/roms/gobbler4/gobbler4.dsk" 3)
    set_tests_properties(trs80_gobbler4_boot PROPERTIES ENVIRONMENT "TRS80_PROBE_BOOT_COLUMNS=64")
endif()
