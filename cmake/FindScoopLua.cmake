# FindScoopLua.cmake - Find Lua installed via Scoop

find_path(LUA_INCLUDE_DIR
    NAMES lua.h luaconf.h lualib.h
    PATHS
        "$ENV{USERPROFILE}/scoop/apps/lua/current/include"
        "C:/Users/admin/scoop/apps/lua/current/include"
        "$ENV{USERPROFILE}/scoop/apps/lua/5.4.8-1/include"
    DOC "Lua include directory"
)

find_library(LUA_LIBRARY
    NAMES lua liblua lua54
    PATHS
        "$ENV{USERPROFILE}/scoop/apps/lua/current/lib"
        "C:/Users/admin/scoop/apps/lua/current/lib"
        "$ENV{USERPROFILE}/scoop/apps/lua/5.4.8-1/lib"
    DOC "Lua library"
)

find_library(LUA_LIBRARY_DEBUG
    NAMES lua liblua lua54
    PATHS
        "$ENV{USERPROFILE}/scoop/apps/lua/current/lib"
        "C:/Users/admin/scoop/apps/lua/current/lib"
        "$ENV{USERPROFILE}/scoop/apps/lua/5.4.8-1/lib"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Lua
    REQUIRED_VARS LUA_INCLUDE_DIR LUA_LIBRARY
)

if(LUA_FOUND)
    set(LUA_LIBRARIES ${LUA_LIBRARY})
    set(LUA_INCLUDE_DIRS "${LUA_INCLUDE_DIR}")
    message(STATUS "Found Lua (Scoop): ${LUA_INCLUDE_DIR}")
    message(STATUS "Lua library: ${LUA_LIBRARY}")
endif()

mark_as_advanced(LUA_INCLUDE_DIR LUA_LIBRARY LUA_LIBRARY_DEBUG)
