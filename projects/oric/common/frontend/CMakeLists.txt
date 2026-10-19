cmake_minimum_required(VERSION 3.16)
project(retropicoes_common_frontend LANGUAGES C)

enable_testing()

if(NOT TARGET retropicoes_menu_contract)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../menu
                     ${CMAKE_CURRENT_BINARY_DIR}/common-menu)
endif()

add_library(retropicoes_frontend STATIC
    rom_browser.c
    frontend_shell.c
    settings_host.c)
target_include_directories(retropicoes_frontend PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(retropicoes_frontend PUBLIC
    retropicoes_menu_contract)
target_compile_features(retropicoes_frontend PUBLIC c_std_11)

add_executable(retropicoes_rom_browser_tests
    tests/test_rom_browser.c)
target_link_libraries(retropicoes_rom_browser_tests PRIVATE
    retropicoes_frontend)

add_executable(retropicoes_frontend_shell_tests
    tests/test_frontend_shell.c)
target_link_libraries(retropicoes_frontend_shell_tests PRIVATE
    retropicoes_frontend)

add_executable(retropicoes_settings_host_tests
    tests/test_settings_host.c)
target_link_libraries(retropicoes_settings_host_tests PRIVATE
    retropicoes_frontend)

foreach(target retropicoes_frontend
               retropicoes_rom_browser_tests
               retropicoes_frontend_shell_tests
               retropicoes_settings_host_tests)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Werror)
    endif()
endforeach()

add_test(NAME retropicoes_rom_browser_tests
         COMMAND retropicoes_rom_browser_tests)
add_test(NAME retropicoes_frontend_shell_tests
         COMMAND retropicoes_frontend_shell_tests)
add_test(NAME retropicoes_settings_host_tests
         COMMAND retropicoes_settings_host_tests)
