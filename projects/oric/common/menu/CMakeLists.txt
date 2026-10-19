cmake_minimum_required(VERSION 3.16)
project(retropicoes_common_menu LANGUAGES C CXX)

enable_testing()

add_library(retropicoes_menu_contract STATIC
    menu_contract.c
    menu_input.c
    menu_state.c
    menu_render.c
    menu_runtime.c
    settings_menu.c)
target_include_directories(retropicoes_menu_contract PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(retropicoes_menu_contract PUBLIC c_std_11)

add_executable(retropicoes_menu_contract_tests
    tests/test_menu_contract.c)
target_link_libraries(retropicoes_menu_contract_tests PRIVATE
    retropicoes_menu_contract)

if(MSVC)
    target_compile_options(retropicoes_menu_contract PRIVATE /W4 /WX)
    target_compile_options(retropicoes_menu_contract_tests PRIVATE /W4 /WX)
else()
    target_compile_options(retropicoes_menu_contract PRIVATE
        -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(retropicoes_menu_contract_tests PRIVATE
        -Wall -Wextra -Wpedantic -Werror)
endif()

add_test(NAME retropicoes_menu_contract_tests
         COMMAND retropicoes_menu_contract_tests)

add_executable(retropicoes_menu_input_tests tests/test_menu_input.c)
target_link_libraries(retropicoes_menu_input_tests PRIVATE
    retropicoes_menu_contract)
if(MSVC)
    target_compile_options(retropicoes_menu_input_tests PRIVATE /W4 /WX)
else()
    target_compile_options(retropicoes_menu_input_tests PRIVATE
        -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME retropicoes_menu_input_tests
         COMMAND retropicoes_menu_input_tests)

add_executable(retropicoes_menu_cpp_contract_tests
    tests/test_cpp_contract.cpp)
target_compile_features(retropicoes_menu_cpp_contract_tests PRIVATE cxx_std_17)
target_link_libraries(retropicoes_menu_cpp_contract_tests PRIVATE
    retropicoes_menu_contract)
if(MSVC)
    target_compile_options(retropicoes_menu_cpp_contract_tests PRIVATE /W4 /WX)
else()
    target_compile_options(retropicoes_menu_cpp_contract_tests PRIVATE
        -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME retropicoes_menu_cpp_contract_tests
         COMMAND retropicoes_menu_cpp_contract_tests)

add_executable(retropicoes_menu_state_tests tests/test_menu_state.c)
target_link_libraries(retropicoes_menu_state_tests PRIVATE
    retropicoes_menu_contract)

add_executable(retropicoes_menu_render_tests tests/test_menu_render.c)
target_link_libraries(retropicoes_menu_render_tests PRIVATE
    retropicoes_menu_contract)

if(MSVC)
    target_compile_options(retropicoes_menu_state_tests PRIVATE /W4 /WX)
    target_compile_options(retropicoes_menu_render_tests PRIVATE /W4 /WX)
else()
    target_compile_options(retropicoes_menu_state_tests PRIVATE
        -Wall -Wextra -Wpedantic -Werror)
    target_compile_options(retropicoes_menu_render_tests PRIVATE
        -Wall -Wextra -Wpedantic -Werror)
endif()

add_test(NAME retropicoes_menu_state_tests
         COMMAND retropicoes_menu_state_tests)
add_test(NAME retropicoes_menu_render_tests
         COMMAND retropicoes_menu_render_tests)

add_executable(retropicoes_menu_runtime_tests tests/test_menu_runtime.c)
target_link_libraries(retropicoes_menu_runtime_tests PRIVATE
    retropicoes_menu_contract)
if(MSVC)
    target_compile_options(retropicoes_menu_runtime_tests PRIVATE /W4 /WX)
else()
    target_compile_options(retropicoes_menu_runtime_tests PRIVATE
        -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME retropicoes_menu_runtime_tests
         COMMAND retropicoes_menu_runtime_tests)

add_executable(retropicoes_settings_menu_tests tests/test_settings_menu.c)
target_link_libraries(retropicoes_settings_menu_tests PRIVATE
    retropicoes_menu_contract)
if(MSVC)
    target_compile_options(retropicoes_settings_menu_tests PRIVATE /W4 /WX)
else()
    target_compile_options(retropicoes_settings_menu_tests PRIVATE
        -Wall -Wextra -Wpedantic -Werror)
endif()
add_test(NAME retropicoes_settings_menu_tests
         COMMAND retropicoes_settings_menu_tests)
