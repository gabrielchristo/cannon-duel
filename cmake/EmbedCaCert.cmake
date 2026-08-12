# Embute assets/certs/cacert.pem no alvo via xxd -i.
# Usado por Android e (futuro) iOS — curl compilado estaticamente nessas
# plataformas não tem repositório de CAs do SO; o PEM vai direto no .so/.a.
#
# Uso:
#   include(${PROJECT_ROOT}/cmake/EmbedCaCert.cmake)
#   cannon_duel_embed_cacert(MeuAlvo ${PROJECT_ROOT})

function(cannon_duel_embed_cacert TARGET PROJECT_ROOT)
    set(CACERT_PEM "${PROJECT_ROOT}/assets/certs/cacert.pem")
    if(NOT EXISTS "${CACERT_PEM}")
        message(FATAL_ERROR
            "assets/certs/cacert.pem ausente em ${PROJECT_ROOT}.\n"
            "  curl -o assets/certs/cacert.pem https://curl.se/ca/cacert.pem")
    endif()

    set(CACERT_EMBED_C "${CMAKE_CURRENT_BINARY_DIR}/CaCertEmbedded.c")
    add_custom_command(
        OUTPUT "${CACERT_EMBED_C}"
        COMMAND xxd -i cacert.pem "${CACERT_EMBED_C}"
        DEPENDS "${CACERT_PEM}"
        WORKING_DIRECTORY "${PROJECT_ROOT}/assets/certs"
        COMMENT "Embutindo cacert.pem em ${TARGET}"
        VERBATIM
    )

    target_sources(${TARGET} PRIVATE "${CACERT_EMBED_C}")
    target_compile_definitions(${TARGET} PRIVATE CANNON_DUEL_HAS_EMBEDDED_CA=1)
endfunction()
