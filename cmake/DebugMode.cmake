# CANNON_DUEL_DEBUG_MODE — dev panel (desktop) + log overlay (todas as plataformas).
# Usado por CMakeLists.txt (desktop) e android/CMakeLists.txt (Android).
option(CANNON_DUEL_DEBUG_MODE "Compile dev panel and on-screen log overlay" ON)

if(NOT CANNON_DUEL_DEBUG_MODE)
    list(FILTER CANNON_DUEL_SOURCES EXCLUDE REGEX ".*/DebugLog\\.cpp$")
    list(FILTER CANNON_DUEL_SOURCES EXCLUDE REGEX ".*/AndroidLogHook\\.cpp$")
endif()

# Chamar depois de add_executable/add_library.
function(cannon_duel_apply_debug_mode target)
    if(CANNON_DUEL_DEBUG_MODE)
        target_compile_definitions(${target} PRIVATE CANNON_DUEL_DEBUG_MODE=1)
    else()
        target_compile_definitions(${target} PRIVATE CANNON_DUEL_DEBUG_MODE=0)
    endif()
endfunction()
