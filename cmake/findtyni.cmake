# 本模块定义了以下变量：
# TINYXML2_FOUND      - 系统中是否找到了TinyXML2
# TINYXML2_INCLUDE_DIR- TinyXML2头文件目录
# TINYXML2_SOURCE_DIR - TinyXML2源文件目录（仅当从源码编译时使用）

# 提供一个选项，决定是否从源码编译TinyXML2
# 如果设置为ON，将直接使用源码而不是系统安装的库
option(TINYXML2_FROM_SOURCE "是否从源码编译TinyXML2" OFF)

# 设置一个变量，用于判断是否从第三方目录查找TinyXML2
set(TINYXML2_FROM_THIRDPARTY OFF)

# 如果THIRDPARTY_TinyXML2被设置为ON或FORCE，则启用第三方模式
if (
    (THIRDPARTY_TinyXML2 STREQUAL "ON") OR
    (THIRDPARTY_TinyXML2 STREQUAL "FORCE")
)
    set(TINYXML2_FROM_THIRDPARTY ON)
endif()

# 如果既不是从源码编译也不是从第三方目录获取
# 则尝试使用CMake的配置文件方式查找系统安装的TinyXML2
if(NOT (TINYXML2_FROM_SOURCE OR TINYXML2_FROM_THIRDPARTY))
    find_package(TinyXML2 CONFIG QUIET)
endif()

# 如果找到了系统安装的TinyXML2，且不是强制使用第三方版本
if(TinyXML2_FOUND AND NOT TINYXML2_FROM_THIRDPARTY)
    message(STATUS "找到系统安装的TinyXML2：${TinyXML2_DIR}")
    
    # 处理新版本TinyXML2（5.0.0及以上）的特殊情况
    # 新版本使用现代CMake的目标导出机制
    if(NOT TINYXML2_LIBRARY)
        # 检查可用的目标名称
        if(TARGET tinyxml2)
            set(TINYXML2_LIBRARY tinyxml2)
        elseif(TARGET tinyxml2::tinyxml2)
            set(TINYXML2_LIBRARY tinyxml2::tinyxml2)
        endif()
    endif()
else()
    # 如果是第三方模式或Android平台，强制从源码编译
    if(TINYXML2_FROM_THIRDPARTY OR ANDROID)
        set(TINYXML2_FROM_SOURCE ON)
        # 查找头文件，不使用CMAKE_FIND_ROOT_PATH
        find_path(TINYXML2_INCLUDE_DIR NAMES tinyxml2.h NO_CMAKE_FIND_ROOT_PATH)
    else()
        # 正常查找头文件
        find_path(TINYXML2_INCLUDE_DIR NAMES tinyxml2.h)
    endif()

    # 根据编译模式决定查找源文件还是库文件
    if(TINYXML2_FROM_SOURCE)
        # 查找源文件
        find_path(TINYXML2_SOURCE_DIR NAMES tinyxml2.cpp NO_CMAKE_FIND_ROOT_PATH)
    else()
        # 查找库文件
        find_library(TINYXML2_LIBRARY tinyxml2)
    endif()

    # 使用CMake标准的包查找处理机制
    include(FindPackageHandleStandardArgs)

    # 根据不同的编译模式设置查找结果
    if(TINYXML2_FROM_SOURCE)
        # 源码模式：需要源文件和头文件都存在
        find_package_handle_standard_args(TinyXML2 DEFAULT_MSG TINYXML2_SOURCE_DIR TINYXML2_INCLUDE_DIR)
        # 将变量标记为高级选项，在cmake-gui中默认不显示
        mark_as_advanced(TINYXML2_INCLUDE_DIR TINYXML2_SOURCE_DIR)
    else()
        # 库模式：需要库文件和头文件都存在
        find_package_handle_standard_args(TinyXML2 DEFAULT_MSG TINYXML2_LIBRARY TINYXML2_INCLUDE_DIR)
        # 将变量标记为高级选项
        mark_as_advanced(TINYXML2_INCLUDE_DIR TINYXML2_LIBRARY)
    endif()
endif()
