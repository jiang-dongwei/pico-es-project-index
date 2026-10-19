cmake_minimum_required(VERSION 3.13)

add_library(retropicoes_compat_v1 STATIC
    audio.c
    video.c
)

add_library(RetroPicoES::compat_v1 ALIAS retropicoes_compat_v1)

target_include_directories(retropicoes_compat_v1 PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)

target_compile_features(retropicoes_compat_v1 PUBLIC c_std_11)
target_compile_definitions(retropicoes_compat_v1 PUBLIC
    RETROPICOES_COMPAT_VERSION=10000
)
