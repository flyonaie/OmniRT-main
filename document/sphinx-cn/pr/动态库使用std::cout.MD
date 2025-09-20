**Version of AimRT**
v0.8.1

**Environments**
- gcc
- cmake
- 动态库环境

**Describe the bug**
在动态库中使用 std::cout 导致段错误。具体表现为在动态库（libpb_rpc_pkg.so）中使用 std::cout 时出现 SIGSEGV 段错误。

**To Reproduce**
1. 在动态库模块中使用 std::cout 进行打印
2. 运行程序后出现段错误
3. 错误堆栈显示在 std::ostream::sentry::sentry(std::ostream&) 处崩溃

**Expected behavior**
动态库中的 std::cout 语句能够正常执行，不会导致程序崩溃。

**Actual behavior**
程序在执行到动态库中的 std::cout 语句时发生段错误，堆栈信息显示在 libstdc++.so.6 中的 std::ostream::sentry::sentry 处崩溃。

**Additional context**
解决方案：

1. 修改动态库加载方式（推荐）：
   - 将 `dlopen` 的参数从 `RTLD_NOW | RTLD_LOCAL | RTLD_DEEPBIND` 修改为：
     - 方案1：`RTLD_LAZY | RTLD_GLOBAL`
     - 方案2：`RTLD_NOW | RTLD_LOCAL`

2. 参数说明：
   - RTLD_NOW vs RTLD_LAZY:
     - RTLD_NOW: 立即解析所有符号
     - RTLD_LAZY: 延迟解析符号（加载更快，但可能在运行时出现未定义符号错误）
   
   - RTLD_LOCAL vs RTLD_GLOBAL:
     - RTLD_LOCAL: 符号仅本库可见
     - RTLD_GLOBAL: 符号全局可见（可解决 std::cout 等全局符号共享问题）

   - RTLD_DEEPBIND:
     - 优先使用本库符号
     - 可能导致 std::cout 实例不一致问题

其他尝试过但无效的方案：
1. 强制动态链接 C++ 标准库：
```cmake
target_link_libraries(libpb_rpc_pkg PUBLIC -static-libgcc -static-libstdc++)
```

2. 设置特殊编译选项：
```cmake
target_compile_options(${CUR_TARGET_NAME} PRIVATE
    -fPIC
    -rdynamic
    -fvisibility=hidden
)

set_target_properties(${CUR_TARGET_NAME} PROPERTIES
    LINK_FLAGS "-Wl,--no-as-needed"
)