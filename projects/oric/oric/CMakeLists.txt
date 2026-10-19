cmake_minimum_required(VERSION 3.20)
project(oric_host C CXX)
include(cmake/frontend_contract.cmake)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
enable_testing()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/make_test_tapes.py
    --output ${CMAKE_CURRENT_BINARY_DIR}/test-tapes COMMAND_ERROR_IS_FATAL ANY)
add_library(oric_core core/oric.c)
target_include_directories(oric_core PUBLIC core third_party/chips)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(oric_core PRIVATE -Wall -Wextra -Werror -O2)
endif()
add_executable(oric_tests tests/test_oric.c)
target_link_libraries(oric_tests PRIVATE oric_core)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(oric_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
endif()
add_test(NAME oric_core COMMAND oric_tests ${CMAKE_CURRENT_BINARY_DIR}/test-tapes)
add_executable(oric_rom_probe tools/rom_probe.c)
target_link_libraries(oric_rom_probe PRIVATE oric_core)
set(MENU ${CMAKE_CURRENT_SOURCE_DIR}/../common/menu)
set(FRONTEND ${CMAKE_CURRENT_SOURCE_DIR}/../common/frontend)
add_executable(oric_frontend_tests tests/test_frontend.c platform/retropicoes/adapter.c
    ${MENU}/menu_contract.c ${MENU}/menu_input.c ${MENU}/menu_state.c
    ${MENU}/menu_render.c ${MENU}/settings_menu.c ${FRONTEND}/rom_browser.c
    ${FRONTEND}/frontend_shell.c ${FRONTEND}/settings_host.c)
target_include_directories(oric_frontend_tests PRIVATE platform/retropicoes ../compat ${MENU} ${FRONTEND})
target_compile_options(oric_frontend_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
add_test(NAME oric_frontend COMMAND oric_frontend_tests)
add_executable(oric_target_tests tests/test_target.cpp platform/retropicoes/adapter.c ../compat/audio.c
    ${MENU}/menu_contract.c ${MENU}/menu_input.c ${MENU}/menu_state.c
    ${MENU}/menu_render.c ${MENU}/settings_menu.c ${FRONTEND}/rom_browser.c
    ${FRONTEND}/frontend_shell.c ${FRONTEND}/settings_host.c)
target_include_directories(oric_target_tests PRIVATE tests/stubs platform/retropicoes
    ../compat ../digger/platform/retropicoes ${MENU} ${FRONTEND})
target_link_libraries(oric_target_tests PRIVATE oric_core)
target_compile_options(oric_target_tests PRIVATE -Wall -Wextra -Werror -UNDEBUG)
add_test(NAME oric_target_adapter COMMAND oric_target_tests)
set_tests_properties(oric_target_adapter PROPERTIES TIMEOUT 15)
add_executable(oric_xip_guard_tests tests/test_xip_guard.cpp
    platform/retropicoes/xip_startup_guard.cpp)
target_include_directories(oric_xip_guard_tests PRIVATE tests)
target_compile_definitions(oric_xip_guard_tests PRIVATE ORIC_XIP_GUARD_TEST=1)
target_compile_options(oric_xip_guard_tests PRIVATE -Wall -Wextra -Werror)
add_test(NAME oric_xip_guard COMMAND oric_xip_guard_tests)
