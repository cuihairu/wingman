include_guard(GLOBAL)

function(wingman_ensure_spdlog)
    if(TARGET spdlog::spdlog_header_only)
        return()
    endif()

    find_package(spdlog CONFIG QUIET)
    if(TARGET spdlog::spdlog_header_only)
        return()
    endif()

    include(FetchContent)

    message(STATUS "spdlog not found via CONFIG, using FetchContent")
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.14.1
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(spdlog)

    if(NOT TARGET spdlog::spdlog_header_only AND TARGET spdlog_header_only)
        add_library(spdlog::spdlog_header_only ALIAS spdlog_header_only)
    endif()
endfunction()
