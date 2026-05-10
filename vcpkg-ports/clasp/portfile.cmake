# CLASP Port for Wingman Overlay Ports
# 支持本地开发或从 GitHub 获取

# 方式1: 从本地 sibling 目录获取（开发时）
set(CLASP_LOCAL_DIR "$ENV{USERPROFILE}/Workspaces/clasp")
if(NOT EXISTS "${CLASP_LOCAL_DIR}")
    # macOS/Linux 路径
    set(CLASP_LOCAL_DIR "$ENV{HOME}/Workspaces/clasp")
endif()

# 方式2: 从环境变量指定
if(DEFINED ENV{CLASP_SOURCE_DIR})
    set(CLASP_LOCAL_DIR "$ENV{CLASP_SOURCE_DIR}")
endif()

if(EXISTS "${CLASP_LOCAL_DIR}")
    message(STATUS "Using local clasp from: ${CLASP_LOCAL_DIR}")
    set(SOURCE_PATH "${CLASP_LOCAL_DIR}")
else()
    # 方式3: 从 GitHub 获取
    vcpkg_from_github(
        OUT_SOURCE_PATH SOURCE_PATH
        REPO cuihairu/clasp
        REF main
        HEAD_REF main
    )
endif()

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DCLASP_BUILD_EXAMPLES=OFF
        -DCLASP_BUILD_TESTS=OFF
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/clasp)

# 处理头文件 - 确保所有 clasp/*.hpp 都被复制
file(GLOB HEADER_FILES "${SOURCE_PATH}/include/clasp/*.hpp")
if(HEADER_FILES)
    file(INSTALL ${HEADER_FILES}
         DESTINATION "${CURRENT_PACKAGES_DIR}/include/clasp")
endif()

# 处理可能的子目录
if(EXISTS "${SOURCE_PATH}/include/clasp/clasp")
    file(GLOB RECURSE SUB_HEADERS "${SOURCE_PATH}/include/clasp/clasp/*.hpp")
    if(SUB_HEADERS)
        file(INSTALL ${SUB_HEADERS}
             DESTINATION "${CURRENT_PACKAGES_DIR}/include/clasp/clasp")
    endif()
endif()

# 安装许可证
if(EXISTS "${SOURCE_PATH}/LICENSE")
    vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
elseif(EXISTS "${SOURCE_PATH}/LICENSE.txt")
    vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE.txt")
endif()
