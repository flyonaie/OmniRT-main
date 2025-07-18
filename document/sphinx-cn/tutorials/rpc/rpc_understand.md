# RPC模块架构与使用教程

## 1. 模块架构

### 1.1 核心组件

#### RPC管理器 (`RpcManager`)
- 作为整个RPC系统的核心控制器
- 负责RPC服务的生命周期管理（初始化、启动、关闭）
- 管理RPC后端、过滤器和句柄代理
- 提供配置接口，支持通过YAML进行灵活配置

#### RPC后端 (`RpcBackendBase`)
- 定义RPC后端的基础接口
- 提供本地RPC实现 (`LocalRpcBackend`)
- 支持扩展不同的RPC协议实现

#### 后端管理器 (`RpcBackendManager`)
- 管理多个RPC后端实例
- 处理后端的注册和调用逻辑
- 提供后端选择和负载均衡能力

#### 句柄代理 (`RpcHandleProxy`)
- 封装RPC调用接口
- 简化RPC服务的注册和调用过程
- 支持异步调用模式

### 1.2 特性亮点

- **多后端支持**
  - 支持本地和远程RPC调用
  - 可扩展新的RPC协议后端
  - 支持后端动态切换

- **过滤器机制**
  - 支持客户端和服务端过滤器
  - 可用于实现日志、监控、认证等功能
  - 支持过滤器链式处理

- **异步调用**
  - 支持异步RPC调用
  - 提供Future接口
  - 支持超时控制

- **配置灵活**
  - 通过YAML配置文件进行配置
  - 支持动态启用/禁用后端和过滤器
  - 支持细粒度的服务配置

## 2. 快速上手指南

### 2.1 初始化RPC系统

```cpp
// 1. 创建RPC管理器实例
auto rpc_manager = std::make_unique<RpcManager>();

// 2. 准备配置选项
YAML::Node options;
options["backends_options"] = YAML::Node();
options["backends_options"][0]["type"] = "local";
options["backends_options"][0]["options"] = YAML::Node();

// 3. 初始化RPC管理器
rpc_manager->Initialize(options);

// 4. 启动RPC服务
rpc_manager->Start();
```

### 2.2 注册RPC服务

```cpp
// 1. 获取RPC句柄代理
const auto& rpc_proxy = rpc_manager->GetRpcHandleProxy("your_module");

// 2. 定义服务实现
class YourService {
public:
    int32_t Add(int32_t a, int32_t b) {
        return a + b;
    }
};

// 3. 注册服务
auto service = std::make_shared<YourService>();
rpc_proxy.RegisterService("math_service", service);
```

### 2.3 调用RPC服务

```cpp
// 1. 获取目标模块的RPC句柄代理
const auto& rpc_proxy = rpc_manager->GetRpcHandleProxy("target_module");

// 2. 同步调用
int32_t result = rpc_proxy.Call<int32_t>("math_service.Add", 1, 2);

// 3. 异步调用
auto future = rpc_proxy.AsyncCall<int32_t>("math_service.Add", 1, 2);
result = future.get();
```

## 3. 源码阅读指南

### 3.1 核心文件

1. **rpc_manager.h/cc**
   - RPC系统的入口点
   - 了解整体架构和核心接口

2. **rpc_backend_base.h**
   - RPC后端的抽象接口
   - 理解后端扩展机制

3. **rpc_handle_proxy.h**
   - RPC调用的代理实现
   - 学习服务注册和调用方式

4. **local_rpc_backend.h/cc**
   - 本地RPC实现示例
   - 参考具体后端实现方式

### 3.2 阅读顺序建议

1. 从`rpc_manager.h`开始，了解系统整体架构
2. 查看`rpc_backend_base.h`，理解后端接口设计
3. 学习`rpc_handle_proxy.h`，掌握使用方法
4. 参考`local_rpc_backend.h/cc`的实现细节

## 4. 最佳实践

### 4.1 配置管理
- 使用YAML配置文件管理RPC设置
- 根据需求启用适当的后端和过滤器
- 合理配置超时和重试策略

### 4.2 错误处理
- 妥善处理RPC调用异常
- 实现适当的重试机制
- 添加必要的日志记录

### 4.3 性能优化
- 合理使用异步调用
- 避免频繁创建和销毁连接
- 适当配置并发参数

## 5. 常见问题

### 5.1 服务注册失败
- 检查服务名称是否正确
- 确认服务实现是否完整
- 验证RPC管理器是否正确初始化

### 5.2 调用超时
- 检查网络连接状态
- 确认超时配置是否合理
- 验证服务端处理能力

### 5.3 性能问题
- 使用异步调用替代同步调用
- 优化服务端处理逻辑
- 调整并发参数配置