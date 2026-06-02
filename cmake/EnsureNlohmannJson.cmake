include_guard(GLOBAL)

function(wingman_ensure_nlohmann_json)
    if(TARGET nlohmann_json::nlohmann_json)
        return()
    endif()

    find_package(nlohmann_json CONFIG QUIET)
    if(TARGET nlohmann_json::nlohmann_json)
        return()
    endif()

    include(FetchContent)

    message(STATUS "nlohmann_json not found via CONFIG, using FetchContent")
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(nlohmann_json)
endfunction()
