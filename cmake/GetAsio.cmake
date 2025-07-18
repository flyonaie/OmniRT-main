include(FetchContent)
include(CheckLocalSource)

# 设置本地源码路径（需要根据实际路径修改）
set(SRC_NAME "asio")
message(STATUS "get ${SRC_NAME} print ...")

#-----------------step1:检查指定目录下的库是否存在-----------------------#
# 清除 find_package 的缓存
unset(${SRC_NAME}_DIR CACHE)
unset(${SRC_NAME}_FOUND CACHE)

# 下面这句话和find_package中PATHS用一个即可
# set(${SRC_NAME}_DIR "${CMAKE_PREFIX_PATH}/${SRC_NAME}" CACHE PATH "Path to ${SRC_NAME} config file" FORCE)
find_package(Threads REQUIRED)
# 只在指定目录中查找库
find_package(${SRC_NAME} QUIET
        NO_DEFAULT_PATH
        PATHS "${CMAKE_PREFIX_PATH}/${SRC_NAME}")

if(${SRC_NAME}_FOUND)
    message(STATUS "${SRC_NAME}found via find_package at: ${${SRC_NAME}_DIR}")
    return()
else()
    message(STATUS "${SRC_NAME} not found via find_package, attempting to build from source")
endif()

#-----------------step2:查本地源码目录是否存-----------------------#
check_local_source(
    SRC_NAME "${SRC_NAME}"
    SOURCE_DIR "${FETCHCONTENT_BASE_DIR}/${SRC_NAME}-src"
    REQUIRED_FILES "include" "src"
    RESULT_VAR "${SRC_NAME}_LOCAL_SOURCE"
)

set(asio_DOWNLOAD_URL
    "https://github.com/chriskohlhoff/asio/archive/asio-1-30-2.tar.gz"
    CACHE STRING "")

message(STATUS "1111asio_LOCAL_SOURCE: ${asio_LOCAL_SOURCE}")
if(asio_LOCAL_SOURCE)
  FetchContent_Declare(
    asio
    SOURCE_DIR ${asio_LOCAL_SOURCE}
    OVERRIDE_FIND_PACKAGE)
else()
  FetchContent_Declare(
    asio
    URL ${asio_DOWNLOAD_URL}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    OVERRIDE_FIND_PACKAGE)
endif()

# Wrap it in a function to restrict the scope of the variables
function(get_asio)
  FetchContent_GetProperties(asio)
  if(NOT asio_POPULATED)
    FetchContent_Populate(asio)

    add_library(asio INTERFACE)
    add_library(asio::asio ALIAS asio)

    target_include_directories(asio INTERFACE $<BUILD_INTERFACE:${asio_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include/asio>)
    target_compile_definitions(asio INTERFACE ASIO_STANDALONE ASIO_NO_DEPRECATED)

    file(GLOB_RECURSE head_files ${asio_SOURCE_DIR}/include/*.hpp ${asio_SOURCE_DIR}/include/*.ipp)
    target_sources(asio INTERFACE FILE_SET HEADERS BASE_DIRS ${asio_SOURCE_DIR}/include FILES ${head_files})

    find_package(Threads REQUIRED)
    target_link_libraries(asio INTERFACE Threads::Threads)

    if(WIN32)
      # macro see @ https://stackoverflow.com/a/40217291/1746503
      macro(get_win32_winnt version)
        if(CMAKE_SYSTEM_VERSION)
          set(ver ${CMAKE_SYSTEM_VERSION})
          string(REGEX MATCH "^([0-9]+).([0-9])" ver ${ver})
          string(REGEX MATCH "^([0-9]+)" verMajor ${ver})
          # Check for Windows 10, b/c we'll need to convert to hex 'A'.
          if("${verMajor}" MATCHES "10")
            set(verMajor "A")
            string(REGEX REPLACE "^([0-9]+)" ${verMajor} ver ${ver})
          endif("${verMajor}" MATCHES "10")
          # Remove all remaining '.' characters.
          string(REPLACE "." "" ver ${ver})
          # Prepend each digit with a zero.
          string(REGEX REPLACE "([0-9A-Z])" "0\\1" ver ${ver})
          set(${version} "0x${ver}")
        endif()
      endmacro()

      if(NOT DEFINED _WIN32_WINNT)
        get_win32_winnt(ver)
        set(_WIN32_WINNT ${ver})
      endif()

      message(STATUS "Set _WIN32_WINNET=${_WIN32_WINNT}")

      target_compile_definitions(asio INTERFACE _WIN32_WINNT=${_WIN32_WINNT} WIN32_LEAN_AND_MEAN)
    endif()

    set_property(TARGET asio PROPERTY EXPORT_NAME asio::asio)
    install(
      TARGETS asio
      EXPORT asio-config
      FILE_SET HEADERS
      DESTINATION include/asio)

    install(EXPORT asio-config DESTINATION lib/cmake/asio)
  endif()
endfunction()

get_asio()

# import targets：
# asio::asio
