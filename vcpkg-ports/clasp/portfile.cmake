# CLASP Port for Wingman Overlay Ports
# 简化版本 - 只安装头文件和库，手动创建 CMake 配置

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

# 配置并构建 clasp
vcpkg_configure_cmake(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DCLASP_BUILD_EXAMPLES=OFF
        -DCLASP_BUILD_TESTS=OFF
)

vcpkg_install_cmake()

# 检查安装的库文件
message(STATUS "Checking for installed library files...")
file(GLOB_RECURSE _clasp_libs "${CURRENT_PACKAGES_DIR}/lib/*clasp*")
message(STATUS "Found clasp libraries: ${_clasp_libs}")

# 手动创建 CMake 配置文件
# 使用 find_library 来定位库文件，而不是硬编码路径
file(WRITE "${CURRENT_PACKAGES_DIR}/share/clasp/claspConfig.cmake"
"include(CMakeFindDependencyMacro)

# 设置导入目标
if(NOT TARGET clasp::clasp)
    add_library(clasp::clasp STATIC IMPORTED GLOBAL)

    # 设置包含目录
    set_target_properties(clasp::clasp PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES \"\${CMAKE_CURRENT_LIST_DIR}/../../include\"
    )

    # 在配置时查找库文件
    # vcpkg 将库安装到 lib/ 目录
    find_library(_clasp_lib_PATH
        NAMES clasp libclasp
        PATHS \"\${CMAKE_CURRENT_LIST_DIR}/../../lib\"
        NO_DEFAULT_PATH
        REQUIRED
    )

    # 为所有配置类型设置库位置
    set_target_properties(clasp::clasp PROPERTIES
        IMPORTED_LOCATION_DEBUG \"\${_clasp_lib_PATH}\"
        IMPORTED_LOCATION_RELEASE \"\${_clasp_lib_PATH}\"
        IMPORTED_LOCATION_MINSIZEREL \"\${_clasp_lib_PATH}\"
        IMPORTED_LOCATION_RELWITHDEBINFO \"\${_clasp_lib_PATH}\"
    )
endif()
")

# 创建版本文件
file(WRITE "${CURRENT_PACKAGES_DIR}/share/clasp/claspConfigVersion.cmake"
"set(PACKAGE_VERSION 0.1.0)
set(PACKAGE_VERSION_EXACT FALSE)
set(PACKAGE_VERSION_COMPATIBLE FALSE)

if(PACKAGE_FIND_VERSION VERSION_EQUAL PACKAGE_VERSION)
    set(PACKAGE_VERSION_COMPATIBLE TRUE)
elseif(PACKAGE_FIND_VERSION_MAJOR EQUAL 0)
    set(PACKAGE_VERSION_COMPATIBLE TRUE)
endif()
")

# 安装许可证
if(EXISTS "${SOURCE_PATH}/LICENSE")
    vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
elseif(EXISTS "${SOURCE_PATH}/LICENSE.txt")
    vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE.txt")
endif()
