# Copyright (c) 2023, AgiBot Inc.
# All rights reserved.
# 版权声明：2023年，AgiBot公司版权所有

# /**
#  * @brief 为指定proto路径添加protobuf代码生成目标
# @details 为指定目录下的所有Protocol Buffer文件（.proto文件）生成对应的C++代码，并将这些生成的代码编译成一个共享库
# */
function(add_protobuf_gencode_target_for_proto_path)
  # 解析参数：TARGET_NAME为单值参数；PROTO_PATH、GENCODE_PATH、DEP_PROTO_TARGETS和OPTIONS为多值参数
  cmake_parse_arguments(ARG "" "TARGET_NAME" "PROTO_PATH;GENCODE_PATH;DEP_PROTO_TARGETS;OPTIONS" ${ARGN})
  
  # 检查生成代码目录是否存在，不存在则创建
  if(NOT EXISTS ${ARG_GENCODE_PATH})
    file(MAKE_DIRECTORY ${ARG_GENCODE_PATH})
  endif()

  # 收集所有依赖的proto目标，包括直接依赖和这些依赖的二级依赖
  set(ALL_DEP_PROTO_TARGETS ${ARG_DEP_PROTO_TARGETS})
  foreach(CUR_DEP_PROTO_TARGET ${ARG_DEP_PROTO_TARGETS})
    # 获取每个依赖项的二级依赖
    get_target_property(SECONDARY_DEP_PROTO_TARGET ${CUR_DEP_PROTO_TARGET} DEP_PROTO_TARGETS)
    # 将二级依赖添加到总依赖列表
    list(APPEND ALL_DEP_PROTO_TARGETS ${SECONDARY_DEP_PROTO_TARGET})
  endforeach()
  # 移除重复的依赖项
  list(REMOVE_DUPLICATES ALL_DEP_PROTO_TARGETS)

  # 构建protoc外部proto路径参数
  set(PROTOC_EXTERNAL_PROTO_PATH_ARGS)
  foreach(CUR_DEP_PROTO_TARGET ${ALL_DEP_PROTO_TARGETS})
    # 获取每个依赖项的proto路径
    get_target_property(DEP_PROTO_TARGET_PROTO_PATH ${CUR_DEP_PROTO_TARGET} PROTO_PATH)
    # 构建protoc的--proto_path参数
    list(APPEND PROTOC_EXTERNAL_PROTO_PATH_ARGS "--proto_path=${DEP_PROTO_TARGET_PROTO_PATH}")
  endforeach()
  # 移除重复的proto路径
  list(REMOVE_DUPLICATES PROTOC_EXTERNAL_PROTO_PATH_ARGS)

  # 获取全局设置的protoc命名空间属性
  get_property(PROTOC_NAMESPACE GLOBAL PROPERTY PROTOC_NAMESPACE_PROPERTY)

  # 如果没有设置特定的protoc命名空间，则使用默认的"protobuf"
  if(NOT PROTOC_NAMESPACE)
    set(CUR_PROTOC_NAMESPACE "protobuf")
  else()
    set(CUR_PROTOC_NAMESPACE ${PROTOC_NAMESPACE})
  endif()

  # 初始化生成源文件和头文件的列表
  set(GEN_SRCS)
  set(GEN_HDRS)

  # 递归查找指定路径下所有的proto文件
  file(GLOB_RECURSE PROTO_FILES ${ARG_PROTO_PATH}/*.proto)
  foreach(PROTO_FILE ${PROTO_FILES})
    # 提取proto文件名（不含路径和扩展名）
    string(REGEX REPLACE ".+/(.+)\\..*" "\\1" PROTO_FILE_NAME ${PROTO_FILE})
    # 生成源文件和头文件的完整路径
    set(GEN_SRC "${ARG_GENCODE_PATH}/${PROTO_FILE_NAME}.pb.cc")
    set(GEN_HDR "${ARG_GENCODE_PATH}/${PROTO_FILE_NAME}.pb.h")

    # 将生成的文件路径添加到列表
    list(APPEND GEN_SRCS ${GEN_SRC})
    list(APPEND GEN_HDRS ${GEN_HDR})

    # 添加自定义命令，调用protoc编译器生成C++代码
    add_custom_command(
      OUTPUT ${GEN_SRC} ${GEN_HDR}#定义了此命令将生成的输出文件，即.pb.cc源文件和.pb.h头文件
      COMMAND ${CMAKE_COMMAND} -E echo "正在执行 protoc: $<TARGET_FILE:protoc>"
      COMMAND $<TARGET_FILE:protoc> ${ARG_OPTIONS} --proto_path=${ARG_PROTO_PATH} ${PROTOC_EXTERNAL_PROTO_PATH_ARGS} --cpp_out=${ARG_GENCODE_PATH} ${PROTO_FILE}#定义要执行的命令及其参数
      DEPENDS ${PROTO_FILE} protoc#定义了此命令的依赖项，即.proto文件和protoc库
      COMMENT "运行protoc编译器，参数：${ARG_OPTIONS} --proto_path=${ARG_PROTO_PATH} ${PROTOC_EXTERNAL_PROTO_PATH_ARGS} --cpp_out=${ARG_GENCODE_PATH} ${PROTO_FILE}"#定义了此命令的注释，用于描述命令的功能
      VERBATIM)#定义了此命令是否使用verbatim模式，即是否保留命令中的空格和换行符
  endforeach()

  # 注释掉的静态库选项
  # add_library(${ARG_TARGET_NAME} STATIC)
  # 创建共享库
  add_library(${ARG_TARGET_NAME} SHARED)
  # 设置编译选项：启用位置无关代码（Position Independent Code）和默认可见性
  target_compile_options(${ARG_TARGET_NAME}
    PRIVATE
      -fPIC
      -fvisibility=default)

  # 设置库的源文件
  target_sources(${ARG_TARGET_NAME} PRIVATE ${GEN_SRCS})
  # 设置库的头文件，便于安装和引用
  target_sources(${ARG_TARGET_NAME} PUBLIC FILE_SET HEADERS BASE_DIRS ${ARG_GENCODE_PATH} FILES ${GEN_HDRS})
  # 设置库的包含目录，区分构建时和安装后的路径
  target_include_directories(${ARG_TARGET_NAME} PUBLIC $<BUILD_INTERFACE:${ARG_GENCODE_PATH}> $<INSTALL_INTERFACE:include/${ARG_TARGET_NAME}>)

  # 链接protobuf库和所有依赖的proto目标
  target_link_libraries(${ARG_TARGET_NAME} PUBLIC protobuf::libprotobuf ${ALL_DEP_PROTO_TARGETS})

  # 将proto路径和依赖项作为目标属性保存，便于后续使用
  set_target_properties(${ARG_TARGET_NAME} PROPERTIES PROTO_PATH ${ARG_PROTO_PATH})
  set_target_properties(${ARG_TARGET_NAME} PROPERTIES DEP_PROTO_TARGETS "${ALL_DEP_PROTO_TARGETS}")

endfunction()

# /**
#  * @brief 为单个proto文件添加protobuf代码生成目标
#  * @details 与上一个函数类似，但只处理一个指定的proto文件，而不是整个目录
#  */
function(add_protobuf_gencode_target_for_one_proto_file)
  # 解析参数：TARGET_NAME为单值参数；PROTO_FILE、GENCODE_PATH、DEP_PROTO_TARGETS和OPTIONS为多值参数
  cmake_parse_arguments(ARG "" "TARGET_NAME" "PROTO_FILE;GENCODE_PATH;DEP_PROTO_TARGETS;OPTIONS" ${ARGN})

  # 检查生成代码目录是否存在，不存在则创建
  if(NOT EXISTS ${ARG_GENCODE_PATH})
    file(MAKE_DIRECTORY ${ARG_GENCODE_PATH})
  endif()

  # 收集所有依赖的proto目标，包括直接依赖和这些依赖的二级依赖
  set(ALL_DEP_PROTO_TARGETS ${ARG_DEP_PROTO_TARGETS})
  foreach(CUR_DEP_PROTO_TARGET ${ARG_DEP_PROTO_TARGETS})
    # 获取每个依赖项的二级依赖
    get_target_property(SECONDARY_DEP_PROTO_TARGET ${CUR_DEP_PROTO_TARGET} DEP_PROTO_TARGETS)
    # 将二级依赖添加到总依赖列表
    list(APPEND ALL_DEP_PROTO_TARGETS ${SECONDARY_DEP_PROTO_TARGET})
  endforeach()
  # 移除重复的依赖项
  list(REMOVE_DUPLICATES ALL_DEP_PROTO_TARGETS)

  # 构建protoc外部proto路径参数
  set(PROTOC_EXTERNAL_PROTO_PATH_ARGS)
  foreach(CUR_DEP_PROTO_TARGET ${ALL_DEP_PROTO_TARGETS})
    # 获取每个依赖项的proto路径
    get_target_property(DEP_PROTO_TARGET_PROTO_PATH ${CUR_DEP_PROTO_TARGET} PROTO_PATH)
    # 构建protoc的--proto_path参数
    list(APPEND PROTOC_EXTERNAL_PROTO_PATH_ARGS "--proto_path=${DEP_PROTO_TARGET_PROTO_PATH}")
  endforeach()
  # 移除重复的proto路径
  list(REMOVE_DUPLICATES PROTOC_EXTERNAL_PROTO_PATH_ARGS)

  # 获取全局设置的protoc命名空间属性
  get_property(PROTOC_NAMESPACE GLOBAL PROPERTY PROTOC_NAMESPACE_PROPERTY)

  # 如果没有设置特定的protoc命名空间，则使用默认的"protobuf"
  if(NOT PROTOC_NAMESPACE)
    set(CUR_PROTOC_NAMESPACE "protobuf")
  else()
    set(CUR_PROTOC_NAMESPACE ${PROTOC_NAMESPACE})
  endif()

  # 提取proto文件名（不含路径和扩展名）
  string(REGEX REPLACE ".+/(.+)\\..*" "\\1" PROTO_FILE_NAME ${ARG_PROTO_FILE})
  # 提取proto文件所在的路径
  string(REGEX REPLACE "(.+)/(.+)\\..*" "\\1" PROTO_PATH ${ARG_PROTO_FILE})
  # 生成源文件和头文件的完整路径
  set(GEN_SRC "${ARG_GENCODE_PATH}/${PROTO_FILE_NAME}.pb.cc")
  set(GEN_HDR "${ARG_GENCODE_PATH}/${PROTO_FILE_NAME}.pb.h")

  # 添加自定义命令，调用protoc编译器生成C++代码
  add_custom_command(
    OUTPUT ${GEN_SRC} ${GEN_HDR}
    COMMAND ${CMAKE_COMMAND} -E echo "正在执行 protoc: $<TARGET_FILE:protoc>"
    COMMAND $<TARGET_FILE:protoc> ${ARG_OPTIONS} --proto_path=${PROTO_PATH} ${PROTOC_EXTERNAL_PROTO_PATH_ARGS} --cpp_out=${ARG_GENCODE_PATH} ${ARG_PROTO_FILE}
    DEPENDS ${ARG_PROTO_FILE} protoc
    COMMENT "运行protoc编译器，参数：${ARG_OPTIONS} --proto_path=${PROTO_PATH} ${PROTOC_EXTERNAL_PROTO_PATH_ARGS} --cpp_out=${ARG_GENCODE_PATH} ${ARG_PROTO_FILE}"
    VERBATIM)

  # 注释掉的静态库选项
  # add_library(${ARG_TARGET_NAME} STATIC)
  # 创建共享库
  add_library(${ARG_TARGET_NAME} SHARED)
  # 设置编译选项：启用位置无关代码（Position Independent Code）和默认可见性
  target_compile_options(${ARG_TARGET_NAME}
    PRIVATE
      -fPIC
      -fvisibility=default)

  # 设置库的源文件
  target_sources(${ARG_TARGET_NAME} PRIVATE ${GEN_SRC})
  # 设置库的头文件，便于安装和引用
  target_sources(${ARG_TARGET_NAME} PUBLIC FILE_SET HEADERS BASE_DIRS ${ARG_GENCODE_PATH} FILES ${GEN_HDR})
  # 设置库的包含目录，区分构建时和安装后的路径
  target_include_directories(${ARG_TARGET_NAME} PUBLIC $<BUILD_INTERFACE:${ARG_GENCODE_PATH}> $<INSTALL_INTERFACE:include/${ARG_TARGET_NAME}>)

  # 链接protobuf库和所有依赖的proto目标
  target_link_libraries(${ARG_TARGET_NAME} PUBLIC protobuf::libprotobuf ${ALL_DEP_PROTO_TARGETS})

  # 将proto路径和依赖项作为目标属性保存，便于后续使用
  set_target_properties(${ARG_TARGET_NAME} PROPERTIES PROTO_PATH ${PROTO_PATH})
  set_target_properties(${ARG_TARGET_NAME} PROPERTIES DEP_PROTO_TARGETS "${ALL_DEP_PROTO_TARGETS}")

endfunction()