cmake_minimum_required(VERSION 3.20)

# ========== wingman::proto ==========
# Protobuf 协议的 C++ 封装

project(wingman_proto LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找 Protobuf（可选）
find_package(Protobuf QUIET)

if(Protobuf_FOUND)
    set(PROTOBUF_SUPPORT ON)
    message(STATUS "Protobuf found, building with protobuf support")

    # Proto 文件目录
    set(PROTO_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../proto")
    set(GENERATED_PROTOBUF_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

    # 收集所有 proto 文件
    file(GLOB PROTO_FILES "${PROTO_PATH}/*.proto")

    # 生成 Protobuf C++ 代码
    set(PROTO_GENERATED_SRCS "")
    set(PROTO_GENERATED_HDRS "")

    foreach(PROTO_FILE ${PROTO_FILES})
        get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)

        set(PROTO_SRC ${GENERATED_PROTOBUF_DIR}/${PROTO_NAME}.pb.cpp)
        set(PROTO_HDR ${GENERATED_PROTOBUF_DIR}/${PROTO_NAME}.pb.h)

        add_custom_command(
            OUTPUT ${PROTO_SRC} ${PROTO_HDR}
            COMMAND protobuf::protoc
            ARGS --cpp_out=${GENERATED_PROTOBUF_DIR} -I${PROTO_PATH} ${PROTO_FILE}
            DEPENDS ${PROTO_FILE}
            COMMENT "Generating protobuf files for ${PROTO_NAME}"
        )

        list(APPEND PROTO_GENERATED_SRCS ${PROTO_SRC})
        list(APPEND PROTO_GENERATED_HDRS ${PROTO_HDR})
    endforeach()

    # 创建静态库（带 Protobuf）
    add_library(wingman::proto STATIC
        ${PROTO_GENERATED_SRCS}
        src/proto_wrapper.cpp
    )

    target_include_directories(wingman::proto PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${GENERATED_PROTOBUF_DIR}
    )

    target_link_libraries(wingman::proto PUBLIC
        protobuf::libprotobuf
    )

    target_compile_definitions(wingman::proto PUBLIC WINGMAN_HAS_PROTOBUF)
else()
    set(PROTOBUF_SUPPORT OFF)
    message(WARNING "Protobuf not found, building without protobuf support")

    # 创建静态库（不带 Protobuf - JSON only）
    add_library(wingman::proto STATIC
        src/proto_wrapper_json.cpp
    )

    target_include_directories(wingman::proto PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    # 添加 nlohmann_json 支持
    find_package(nlohmann_json CONFIG QUIET)
    if(nlohmann_json_FOUND)
        target_link_libraries(wingman::proto PUBLIC nlohmann_json::nlohmann_json)
        target_compile_definitions(wingman::proto PUBLIC WINGMAN_HAS_JSON)
    endif()
endif()

# 导出库
add_library(wingman::proto ALIAS wingman::proto)
