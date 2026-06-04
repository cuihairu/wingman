vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO lua/lua
    REF v${VERSION}
    SHA512 37de2902a08bf788f6c68c8ecdffb15a4a642366b89401b7c34c54c75d37fd2cc5c753e0884a927467f0bc2dcb93de46b8f8aa77a752645f8a4137aec1c00c54
    PATCHES
        vs2015-impl-c99.patch
        fix-ios-system.patch
        uwp-no-popen.diff
)
file(COPY "${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt" DESTINATION "${SOURCE_PATH}")

if(NOT VCPKG_TARGET_IS_WINDOWS)
    vcpkg_find_acquire_program(PKGCONFIG)
    set(ENV{PKG_CONFIG} "${PKGCONFIG}")
endif()

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        cpp     COMPILE_AS_CPP
        tools   INSTALL_TOOLS
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        ${FEATURE_OPTIONS}
        -DVERSION=${VERSION}
    OPTIONS_DEBUG
        -DINSTALL_TOOLS=OFF
)
vcpkg_cmake_install()
vcpkg_copy_pdbs()
vcpkg_cmake_config_fixup(PACKAGE_NAME unofficial-lua CONFIG_PATH share/unofficial-lua)

if("tools" IN_LIST FEATURES)
    vcpkg_copy_tools(TOOL_NAMES lua luac  AUTO_CLEAN)
endif()

if(VCPKG_TARGET_IS_WINDOWS AND VCPKG_LIBRARY_LINKAGE STREQUAL "dynamic")
    vcpkg_replace_string("${CURRENT_PACKAGES_DIR}/include/luaconf.h" "defined(LUA_BUILD_AS_DLL)" "1")
endif()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/vcpkg-cmake-wrapper.cmake" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")

file(READ "${SOURCE_PATH}/doc/readme.html" readme)
string(REGEX REPLACE "^.*<H2><A NAME=\"license\">License</A></H2>" "" license "${readme}")
string(REGEX REPLACE [[<A HREF="([^"]+)">([^<]*)</A>]] "\\2 [\\1]" license "${license}")
string(REGEX REPLACE "<[^>]*>" "" license "${license}")
string(REGEX REPLACE "\n\n+" "\n\n" license "${license}")
file(WRITE "${CURRENT_PACKAGES_DIR}/share/${PORT}/copyright" "${license}")
