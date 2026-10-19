file(READ "${CMAKE_CURRENT_LIST_DIR}/../core-manifest.json" ORIC_MANIFEST)
string(JSON ORIC_CONTRACT GET "${ORIC_MANIFEST}" frontendContractVersion)
file(READ "${CMAKE_CURRENT_LIST_DIR}/../../common/frontend/frontend_shell.h" ORIC_FRONTEND_HEADER)
if(NOT ORIC_CONTRACT EQUAL 2 OR
   NOT ORIC_FRONTEND_HEADER MATCHES "RETROPICOES_FRONTEND_CONTRACT_VERSION 2u")
    message(FATAL_ERROR "Oric requires public frontend contract v2; coordinate contract changes with main")
endif()
