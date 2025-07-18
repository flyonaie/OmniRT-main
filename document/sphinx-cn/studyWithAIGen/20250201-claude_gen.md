# ExecutorProxy和ExecutorRef的区别

## 概述
ExecutorProxy和ExecutorRef是两个不同用途的执行器相关类，它们各自有其特定的应用场景。

## 详细对比

### ExecutorProxy（执行器代理）
- 是一个包装类，用于将C++的ExecutorBase转换为C接口（aimrt_executor_base_t）
- 主要用于跨语言调用，比如当其他语言需要调用C++的执行器功能时
- 包含了一系列C风格的函数指针，如type、name、execute等
- 不能被复制或赋值（删除了拷贝构造函数和赋值运算符）

### ExecutorRef（执行器引用）
- 是一个轻量级的引用类型，用于引用一个执行器实例
- 通过ExecutorManager::GetExecutor()获得
- 主要用于C++内部使用，提供了更友好的C++接口
- 可以直接使用C++的类型系统和特性

## 使用场景
- 如果你在写C++代码，一般使用ExecutorRef
- 如果你需要提供给其他语言调用（比如C、Python等），则需要使用ExecutorProxy

## 关于ExecutorProxy构造函数的说明

对于下面的代码：
```cpp
auto proxy_ptr = std::make_unique<ExecutorProxy>(executor_ptr.get());
// 而不是
auto proxy_ptr = std::make_unique<ExecutorProxy>(executor_ptr);
```

这是因为：

1. ExecutorProxy的构造函数接收的是原始指针（ExecutorBase*）：
```cpp
explicit ExecutorProxy(ExecutorBase* executor_ptr)
    : base_(GenBase(executor_ptr)) {}
```

2. 如果直接传入unique_ptr，会有以下问题：
   - 类型不匹配：unique_ptr不能隐式转换为原始指针
   - 所有权语义：直接传入unique_ptr会导致所有权转移，这不是我们想要的
   - ExecutorProxy只需要使用ExecutorBase的功能，不需要拥有它

3. 正确的做法是：
   - 使用get()获取原始指针
   - 保持executor_ptr的所有权在ExecutorManager中
   - ExecutorProxy只是一个包装器，不应该管理ExecutorBase的生命周期

这种设计确保了：
- ExecutorBase的生命周期由ExecutorManager通过unique_ptr管理
- ExecutorProxy可以安全地使用ExecutorBase的功能，而不用担心生命周期问题

## 设计思路
这种设计模式很常见，通过Proxy模式来适配不同语言接口，而通过Ref来提供原生语言的便利性。

## ExecutorRef空引用的问题

在ExecutorManager中有这样的代码：
```cpp
aimrt::executor::ExecutorRef ExecutorManager::GetExecutor(
    std::string_view executor_name) {
  auto finditr = executor_proxy_map_.find(std::string(executor_name));
  if (finditr != executor_proxy_map_.end())
    return aimrt::executor::ExecutorRef(finditr->second->NativeHandle());

  AIMRT_WARN("Get executor failed, executor name '{}'", executor_name);

  return aimrt::executor::ExecutorRef();  // 返回空引用
}
```

当找不到相应的执行器名称时，返回一个空的ExecutorRef会带来以下问题：

1. 断言错误：
   - ExecutorRef的所有操作方法（如Type()、Name()等）都会先检查base_ptr_是否为空
   - 如果base_ptr_为空，会触发AIMRT_ASSERT断言错误
   ```cpp
   std::string_view Type() const {
     AIMRT_ASSERT(base_ptr_, "Reference is null.");
     return aimrt::util::ToStdStringView(base_ptr_->type(base_ptr_->impl));
   }
   ```

2. 潜在的空指针访问：
   - 如果调用者没有检查ExecutorRef的有效性就直接使用
   - 可能导致空指针解引用，造成程序崩溃

3. 错误处理不完整：
   - 仅记录了警告日志，但没有给出明确的错误信息
   - 调用者可能不知道获取失败的具体原因

建议的改进方式：

1. 使用异常处理：
```cpp
aimrt::executor::ExecutorRef ExecutorManager::GetExecutor(
    std::string_view executor_name) {
  auto finditr = executor_proxy_map_.find(std::string(executor_name));
  if (finditr != executor_proxy_map_.end())
    return aimrt::executor::ExecutorRef(finditr->second->NativeHandle());

  throw std::runtime_error(
      fmt::format("Executor not found: {}", executor_name));
}
```

2. 使用Optional返回值：
```cpp
std::optional<aimrt::executor::ExecutorRef> ExecutorManager::GetExecutor(
    std::string_view executor_name) {
  auto finditr = executor_proxy_map_.find(std::string(executor_name));
  if (finditr != executor_proxy_map_.end())
    return aimrt::executor::ExecutorRef(finditr->second->NativeHandle());

  AIMRT_WARN("Get executor failed, executor name '{}'", executor_name);
  return std::nullopt;
}
```

3. 确保调用者总是检查ExecutorRef的有效性：
```cpp
auto executor = manager.GetExecutor("worker");
if (executor) {  // 使用operator bool()检查有效性
  executor.Execute(task);
} else {
  // 处理错误情况
}