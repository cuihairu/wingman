cmake_minimum_required(VERSION 3.20)

# Wingman 性能基准测试

project(wingman_benchmarks LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 基准测试可执行文件
add_executable(kvstore_bench
    kvstore_bench.cpp
)

target_include_directories(kvstore_bench PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/wingman/include
)

target_link_libraries(kvstore_bench PRIVATE
    wingman
)

# EventHub 吞吐（同步分发是事件面公共路径）
add_executable(event_hub_bench
    event_hub_bench.cpp
)
target_include_directories(event_hub_bench PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/lib/wingman/include
)
target_link_libraries(event_hub_bench PRIVATE
    wingman
)

# PatternMatcher 匹配核（Android/触发器路径的 findImage 实现；纯合成帧
# 无显示面依赖，尺寸与 vision_bench 对齐便于横向读数）
add_executable(matcher_bench
    matcher_bench.cpp
)
target_include_directories(matcher_bench PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/lib/wingman/include
)
target_link_libraries(matcher_bench PRIVATE
    wingman
)

# Lua 引擎与模块胶水往返（需 wingman::lua 提供引擎自注册）
if(TARGET wingman::lua)
    add_executable(lua_bench
        lua_bench.cpp
    )
    target_include_directories(lua_bench PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/lib/wingman/include
        ${CMAKE_SOURCE_DIR}/libs/lua/include
    )
    target_link_libraries(lua_bench PRIVATE
        wingman
        wingman::lua
    )
endif()

# 视觉链路（截屏/找色/找图）仅在 OpenCV 档编译；无显示面时运行期自跳过
if(WINGMAN_HAS_OPENCV)
    add_executable(vision_bench
        vision_bench.cpp
    )
    target_include_directories(vision_bench PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/lib/wingman/include
    )
    target_link_libraries(vision_bench PRIVATE
        wingman
    )
endif()

# Windows 特定
if(WIN32)
    target_link_libraries(kvstore_bench PRIVATE
        ws2_32
    )
    target_link_libraries(event_hub_bench PRIVATE
        ws2_32
    )
    if(TARGET lua_bench)
        target_link_libraries(lua_bench PRIVATE
            ws2_32
        )
    endif()
    if(TARGET vision_bench)
        target_link_libraries(vision_bench PRIVATE
            ws2_32
        )
    endif()
endif()
