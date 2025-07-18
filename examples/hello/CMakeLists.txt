# Get the current folder name
string(REGEX REPLACE ".*/\(.*\)" "\\1" CUR_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# Set namespace


# Get namespace
# Get namespace
get_namespace(CUR_SUPERIOR_NAMESPACE)
message(STATUS "CUR_SUPERIOR_NAMESPACE: ${CUR_SUPERIOR_NAMESPACE}")
string(REPLACE "::" "_" CUR_SUPERIOR_NAMESPACE_UNDERLINE ${CUR_SUPERIOR_NAMESPACE})

# Set target name
set(CUR_TARGET_NAME ${CUR_SUPERIOR_NAMESPACE_UNDERLINE}_hello)
set(CUR_TARGET_ALIAS_NAME ${CUR_SUPERIOR_NAMESPACE}::hello)
message(STATUS "CUR_TARGET_NAME: ${CUR_TARGET_NAME}")

# Add executable target
add_executable(${CUR_TARGET_NAME} hello.cpp)
add_executable(${CUR_TARGET_ALIAS_NAME} ALIAS ${CUR_TARGET_NAME})

# Set include directories
target_include_directories(${CUR_TARGET_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Link libraries if needed
target_link_libraries(${CUR_TARGET_NAME} PRIVATE yaml-cpp::yaml-cpp)

# Install the executable
install(TARGETS ${CUR_TARGET_NAME}
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)
