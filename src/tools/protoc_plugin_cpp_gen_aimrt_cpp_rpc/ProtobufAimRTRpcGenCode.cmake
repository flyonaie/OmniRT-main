# Copyright (c) 2023, AgiBot Inc.
# 版权声明，指明此文件归AgiBot Inc.所有

# All rights reserved.
# 保留所有权利的标准声明

# 为proto文件添加AIMRT RPC代码生成目标的函数定义
function(add_protobuf_aimrt_rpc_gencode_target_for_proto_files)
  # 使用cmake_parse_arguments解析函数参数，分为无值参数、单值参数和多值参数三类
  # TARGET_NAME为单值参数，PROTO_FILES等为多值参数，${ARGN}表示传入的所有参数
  cmake_parse_arguments(ARG "" "TARGET_NAME" "PROTO_FILES;GENCODE_PATH;DEP_PROTO_TARGETS;OPTIONS" ${ARGN})

  # 检查生成代码的目标路径是否存在，如不存在则创建该目录
  if(NOT EXISTS ${ARG_GENCODE_PATH})
    file(MAKE_DIRECTORY ${ARG_GENCODE_PATH})
  endif()

  # 初始化总依赖proto目标列表，首先包含直接依赖
  set(ALL_DEP_PROTO_TARGETS ${ARG_DEP_PROTO_TARGETS})
  # 遍历所有直接依赖的proto目标
  foreach(CUR_DEP_PROTO_TARGET ${ARG_DEP_PROTO_TARGETS})
    # 获取当前依赖proto目标的次级依赖目标（递归依赖处理）
    get_target_property(SECONDARY_DEP_PROTO_TARGET ${CUR_DEP_PROTO_TARGET} DEP_PROTO_TARGETS)
    # 将次级依赖添加到总依赖列表中
    list(APPEND ALL_DEP_PROTO_TARGETS ${SECONDARY_DEP_PROTO_TARGET})
  endforeach()
  # 移除依赖列表中的重复项，确保每个依赖只处理一次
  list(REMOVE_DUPLICATES ALL_DEP_PROTO_TARGETS)

  # 初始化protoc编译器的外部proto路径参数列表
  set(PROTOC_EXTERNAL_PROTO_PATH_ARGS)
  # 遍历所有依赖的proto目标（包括直接和递归的）
  foreach(CUR_DEP_PROTO_TARGET ${ALL_DEP_PROTO_TARGETS})
    # 获取当前依赖目标的proto文件路径
    get_target_property(DEP_PROTO_TARGET_PROTO_PATH ${CUR_DEP_PROTO_TARGET} PROTO_PATH)
    # 将该路径添加到protoc的参数列表，用于解析import语句
    list(APPEND PROTOC_EXTERNAL_PROTO_PATH_ARGS "--proto_path=${DEP_PROTO_TARGET_PROTO_PATH}")
  endforeach()
  # 移除重复的proto路径参数，优化编译性能
  list(REMOVE_DUPLICATES PROTOC_EXTERNAL_PROTO_PATH_ARGS)

  # 获取全局定义的protoc命名空间属性
  get_property(PROTOC_NAMESPACE GLOBAL PROPERTY PROTOC_NAMESPACE_PROPERTY)

  # 设置当前使用的protoc命名空间，如全局未定义则使用默认值"protobuf"
  if(NOT PROTOC_NAMESPACE)
    set(CUR_PROTOC_NAMESPACE "protobuf")
  else()
    set(CUR_PROTOC_NAMESPACE ${PROTOC_NAMESPACE})
  endif()

  # 初始化生成的源文件和头文件列表
  set(GEN_SRCS)
  set(GEN_HDRS)

  # 遍历处理每个指定的proto文件
  foreach(PROTO_FILE ${ARG_PROTO_FILES})
    # 使用正则表达式提取proto文件名（不含路径和扩展名）
    string(REGEX REPLACE ".+/(.+)\\..*" "\\1" PROTO_FILE_NAME ${PROTO_FILE})
    # 使用正则表达式提取proto文件所在的目录路径
    string(REGEX REPLACE "(.+)/(.+)\\..*" "\\1" PROTO_FILE_PATH ${PROTO_FILE})
    # 构建生成的源文件和头文件的完整路径名
    set(GEN_SRC "${ARG_GENCODE_PATH}/${PROTO_FILE_NAME}.aimrt_rpc.pb.cc")
    set(GEN_HDR "${ARG_GENCODE_PATH}/${PROTO_FILE_NAME}.aimrt_rpc.pb.h")

    # 将生成的源文件和头文件添加到相应的列表中
    list(APPEND GEN_SRCS ${GEN_SRC})
    list(APPEND GEN_HDRS ${GEN_HDR})

    # 为每个proto文件添加自定义命令，调用protoc编译器生成代码
    # add_custom_command(
    #   # 指定此命令生成的文件
    #   OUTPUT ${GEN_SRC} ${GEN_HDR}
    #   # 调用protoc命令，设置各种参数和插件
    #   COMMAND protoc ARGS ${ARG_OPTIONS} --proto_path ${PROTO_FILE_PATH} ${PROTOC_EXTERNAL_PROTO_PATH_ARGS} --aimrt_rpc_out ${ARG_GENCODE_PATH}
    #           --plugin=protoc-gen-aimrt_rpc=$<TARGET_FILE:aimrt::tools::protoc_plugin_cpp_gen_aimrt_cpp_rpc> ${PROTO_FILE}
    #   # 指定命令的依赖项，包括proto文件和AIMRT RPC插件
    #   DEPENDS ${PROTO_FILE} aimrt::tools::protoc_plugin_cpp_gen_aimrt_cpp_rpc
    #   # 执行命令时的说明信息
    #   COMMENT
    #     "Running protoc, args: ${ARG_OPTIONS} --proto_path ${PROTO_FILE_PATH} ${PROTOC_EXTERNAL_PROTO_PATH_ARGS} --aimrt_rpc_out ${ARG_GENCODE_PATH} --plugin=protoc-gen-aimrt_rpc=$<TARGET_FILE:aimrt::tools::protoc_plugin_cpp_gen_aimrt_cpp_rpc> ${PROTO_FILE}"
    #   # 指示使用原始字符串，避免转义问题
    #   VERBATIM)

    add_custom_command(
      # 指定此命令生成的文件
      OUTPUT ${GEN_SRC} ${GEN_HDR}
      # 调用protoc命令，设置各种参数和插件
      COMMAND ${CMAKE_COMMAND} -E echo "正在执行 protoc: $<TARGET_FILE:protoc>"
      COMMAND $<TARGET_FILE:protoc> ${ARG_OPTIONS} --proto_path ${PROTO_FILE_PATH} ${PROTOC_EXTERNAL_PROTO_PATH_ARGS} --aimrt_rpc_out ${ARG_GENCODE_PATH}
              --plugin=protoc-gen-aimrt_rpc=$<TARGET_FILE:aimrt::tools::protoc_plugin_cpp_gen_aimrt_cpp_rpc> ${PROTO_FILE}
      # 指定命令的依赖项，包括proto文件和AIMRT RPC插件
      DEPENDS ${PROTO_FILE} protoc aimrt::tools::protoc_plugin_cpp_gen_aimrt_cpp_rpc
      # 执行命令时的说明信息
      COMMENT
        "Running protoc, args: ${ARG_OPTIONS} --proto_path ${PROTO_FILE_PATH} ${PROTOC_EXTERNAL_PROTO_PATH_ARGS} --aimrt_rpc_out ${ARG_GENCODE_PATH} --plugin=protoc-gen-aimrt_rpc=$<TARGET_FILE:aimrt::tools::protoc_plugin_cpp_gen_aimrt_cpp_rpc> ${PROTO_FILE}"
      # 指示使用原始字符串，避免转义问题
      VERBATIM)

  endforeach()

  # 创建共享库目标（注释掉的是静态库选项）
  # add_library(${ARG_TARGET_NAME} STATIC)
  add_library(${ARG_TARGET_NAME} SHARED)

  # 设置目标的源文件（.cc文件）为私有源
  target_sources(${ARG_TARGET_NAME} PRIVATE ${GEN_SRCS})
  # 设置目标的头文件及基础目录为公共头文件，允许其他目标访问
  target_sources(${ARG_TARGET_NAME} PUBLIC FILE_SET HEADERS BASE_DIRS ${ARG_GENCODE_PATH} FILES ${GEN_HDRS})

  # 设置目标的包含目录，区分构建环境和安装环境的路径
  target_include_directories(${ARG_TARGET_NAME} PUBLIC $<BUILD_INTERFACE:${ARG_GENCODE_PATH}> $<INSTALL_INTERFACE:include/${ARG_TARGET_NAME}>)

  # 为目标链接AIMRT模块的protobuf接口库和所有依赖的proto目标
  target_link_libraries(${ARG_TARGET_NAME} PUBLIC aimrt::interface::aimrt_module_protobuf_interface ${ALL_DEP_PROTO_TARGETS})

endfunction()