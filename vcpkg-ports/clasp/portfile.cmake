# CLASP Port for Wingman Overlay Ports
# 使用 vcpkg 标准方式

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
        HEAD_REF main
    )
endif()

vcpkg_configure_cmake(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DCLASP_BUILD_EXAMPLES=OFF
        -DCLASP_ENABLE_COVERAGE=OFF
)

vcpkg_install_cmake()

# vcpkg 会自动处理 CMake targets，但我们需要确保配置文件正确
# 检查并复制 CMake 配置文件到正确的位置
file(GLOB _cmake_files "${CURRENT_PACKAGES_DIR}/lib/cmake/clasp/*")
if(_cmake_files)
    file(COPY ${_cmake_files} DESTINATION "${CURRENT_PACKAGES_DIR}/share/clasp")
endif()

# 安装许可证
if(EXISTS "${SOURCE_PATH}/LICENSE")
    vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
elseif(EXISTS "${SOURCE_PATH}/LICENSE.txt")
    vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE.txt")
endif()
