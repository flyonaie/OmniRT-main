# RPC接口格式规范总结

本文档对比总结了几种主流RPC框架的接口格式特点，包括AimRT/OmniRT、Apollo/CyberRT、ROS2和gRPC等。

## 一、AimRT/OmniRT RPC框架

### 1. 总体架构
- 基于共享内存的高性能通信机制
- C++实现，支持C++11标准
- 采用客户端-服务器模式设计

### 2. 接口格式特点
- **方法注册**：服务端通过`Bind`方法注册RPC服务
- **同步/异步调用**：支持异步RPC调用，结合`RpcResult`处理结果
- **序列化机制**：使用二进制序列化，而非msgpack
- **错误处理**：完善的错误码系统和异常处理

### 3. 消息格式
```cpp
struct RpcMessage {
    RpcHeader header;              ///< 消息头部
    std::string method_name;       ///< 方法名
    std::vector<uint8_t> payload;  ///< 序列化后的负载数据
};
```

### 4. 通信机制
- 基于共享内存队列实现进程间通信
- 使用无锁队列提高性能：`ShmBoundedSpscLockfreeQueue`

### 5. 使用示例

**服务端代码示例**：
```cpp
// 创建RPC服务器
omnirt::rpc::Server srv("rpc_demo_channel");

// 注册一个加法服务
srv.Bind("add", [](int a, int b) -> int {
    return a + b;
});

// 注册一个字符串处理服务
srv.Bind("concat", [](const std::string& a, const std::string& b) -> std::string {
    return a + b;
});

// 启动服务
std::cout << "RPC服务器已启动..." << std::endl;
srv.Start();
```

**客户端代码示例**：
```cpp
// 创建RPC客户端
omnirt::rpc::Client client("rpc_demo_channel");

// 连接到服务器
if (!client.Connect()) {
    std::cerr << "无法连接到RPC服务器" << std::endl;
    return -1;
}

// 同步调用
int result = client.Call<int>("add", 1, 2);
std::cout << "1 + 2 = " << result << std::endl;

// 异步调用
auto future_result = client.AsyncCall<std::string>("concat", "Hello, ", "World!");
// 处理其他任务...
std::string str_result = future_result.Get();
std::cout << "结果: " << str_result << std::endl;
```

## 二、Apollo/CyberRT RPC实现

CyberRT是Apollo自动驾驶平台的基础运行时框架，其RPC实现具有以下特点：

### 1. 总体架构
- 基于Protobuf的服务定义和消息序列化
- 支持同步与异步服务调用
- 支持服务发现与注册

### 2. 接口格式
- 使用Protobuf IDL定义服务接口
- 服务注册采用组件化设计
- 支持服务质量配置(QoS)

### 3. 代表性接口
```cpp
// 服务定义(protobuf)
service HelloService {
  rpc SayHello(HelloRequest) returns (HelloResponse) {}
}

// 服务端实现
class HelloServiceImpl : public apollo::cyber::Component<> {
 public:
  bool Init() override {
    server_ = node_->CreateService<HelloService>("hello_service", 
                                             &HelloServiceImpl::SayHello, this);
    return true;
  }
  void SayHello(const HelloRequest& request, HelloResponse* response) {
    // 处理逻辑
  }
};
```

### 4. 使用示例

**proto文件定义（vehicle_service.proto）**：
```protobuf
syntax = "proto3";

package apollo.cyber.examples;

message VehicleInfo {
  string brand = 1;
  string model = 2;
  int32 year = 3;
  double price = 4;
}

message GetVehicleRequest {
  string id = 1;
}

message GetVehicleResponse {
  VehicleInfo vehicle = 1;
  int32 status = 2;
  string message = 3;
}

service VehicleService {
  rpc GetVehicleInfo(GetVehicleRequest) returns (GetVehicleResponse) {}
}
```

**服务端实现**：
```cpp
#include "cyber/cyber.h"
#include "cyber/examples/proto/vehicle_service.pb.h"

using apollo::cyber::examples::GetVehicleRequest;
using apollo::cyber::examples::GetVehicleResponse;
using apollo::cyber::examples::VehicleInfo;

class VehicleServiceComponent : public apollo::cyber::Component<> {
 public:
  bool Init() override {
    // 创建服务
    vehicle_service_ = node_->CreateService<GetVehicleRequest, GetVehicleResponse>(
        "vehicle_service", 
      [this](const std::shared_ptr<GetVehicleRequest>& request,
                const std::shared_ptr<GetVehicleResponse>& response) {
          return this->HandleRequest(request, response);
        });

    AINFO << "Vehicle service started.";
    return true;
  }

 private:
  bool HandleRequest(const std::shared_ptr<GetVehicleRequest>& request,
                    const std::shared_ptr<GetVehicleResponse>& response) {
    // 处理请求逻辑
    AINFO << "Received request for vehicle ID: " << request->id();
    
    // 模拟数据库查询
    if (request->id() == "001") {
      auto* vehicle = response->mutable_vehicle();
      vehicle->set_brand("BYD");
      vehicle->set_model("Han");
      vehicle->set_year(2023);
      vehicle->set_price(259800.0);
      
      response->set_status(0);
      response->set_message("Success");
    } else {
      response->set_status(1);
      response->set_message("Vehicle not found");
    }
    
    return true;
  }

  std::shared_ptr<apollo::cyber::Service<GetVehicleRequest, GetVehicleResponse>> vehicle_service_;
};

APOLLO_CYBER_REGISTER_COMPONENT(VehicleServiceComponent)
```

**客户端实现**：
```cpp
#include "cyber/cyber.h"
#include "cyber/examples/proto/vehicle_service.pb.h"

using apollo::cyber::examples::GetVehicleRequest;
using apollo::cyber::examples::GetVehicleResponse;

int main(int argc, char* argv[]) {
  // 初始化Apollo Cyber
  apollo::cyber::Init(argv[0]);
  
  // 创建节点
  auto node = apollo::cyber::CreateNode("vehicle_client_node");
  
  // 创建客户端
  auto client = node->CreateClient<GetVehicleRequest, GetVehicleResponse>("vehicle_service");
  
  // 创建请求
  auto request = std::make_shared<GetVehicleRequest>();
  request->set_id("001");
  
  // 同步调用
  auto response = client->SendRequest(request);
  
  // 打印响应
  if (response->status() == 0) {
    const auto& vehicle = response->vehicle();
    std::cout << "Vehicle info: " << vehicle.brand() << " " 
              << vehicle.model() << " (" << vehicle.year() << ")" << std::endl;
    std::cout << "Price: " << vehicle.price() << std::endl;
  } else {
    std::cout << "Error: " << response->message() << std::endl;
  }
  
  apollo::cyber::WaitForShutdown();
  return 0;
}
```

## 三、ROS2 RPC实现

ROS2采用DDS (数据分发服务) 作为中间件，提供了分布式通信框架。

### 1. 总体架构
- 基于IDL定义服务接口
- 使用DDS中间件实现网络通信
- 支持发布/订阅和服务/客户端两种模式

### 2. 接口格式
- 服务定义使用`.srv`文件
- 自动生成客户端和服务端代码
- 支持同步和异步调用

### 3. 代表性接口
```cpp
// 服务定义(.srv文件)
// AddTwoInts.srv
int64 a
int64 b
---
int64 sum

// 服务端实现
class AddServer : public rclcpp::Node {
public:
  AddServer() : Node("add_server") {
    service_ = create_service<example_interfaces::srv::AddTwoInts>(
      "add_two_ints", 
      std::bind(&AddServer::handle_service, this, std::placeholders::_1, std::placeholders::_2));
  }
private:
  void handle_service(
    const std::shared_ptr<example_interfaces::srv::AddTwoInts::Request> request,
    std::shared_ptr<example_interfaces::srv::AddTwoInts::Response> response) {
    response->sum = request->a + request->b;
  }
};
```

### 4. 使用示例

**服务定义文件（sensor_data.srv）**：
```
# 请求部分
string sensor_id  # 传感器ID
int32 data_type   # 数据类型码
float64 time_from # 数据起始时间
float64 time_to   # 数据结束时间
---
# 响应部分
float64[] timestamps   # 数据时间戳数组
float64[] values       # 数据值数组
bool success           # 操作是否成功
string message         # 状态消息
```

**服务端实现（sensor_service_node.cpp）**：
```cpp
#include "rclcpp/rclcpp.hpp"
#include "sensor_interfaces/srv/sensor_data.hpp"

#include <memory>
#include <vector>
#include <cmath>

class SensorServiceNode : public rclcpp::Node
{
public:
  SensorServiceNode() : Node("sensor_service")
  {
    // 创建服务器
    service_ = this->create_service<sensor_interfaces::srv::SensorData>(
      "get_sensor_data",
      std::bind(&SensorServiceNode::handle_service, this,
                std::placeholders::_1, std::placeholders::_2));
                
    RCLCPP_INFO(this->get_logger(), "传感器数据服务已启动");
  }

private:
  // 服务处理函数
  void handle_service(
    const std::shared_ptr<sensor_interfaces::srv::SensorData::Request> request,
    std::shared_ptr<sensor_interfaces::srv::SensorData::Response> response)
  {
    RCLCPP_INFO(this->get_logger(), "收到传感器数据请求，传感器ID: %s", 
               request->sensor_id.c_str());
    
    // 模拟数据查询选取过程
    if (request->sensor_id == "temp_sensor_01")
    {
      // 模拟温度数据
      double interval = (request->time_to - request->time_from) / 10.0;
      
      for (int i = 0; i < 10; i++)
      {
        double timestamp = request->time_from + i * interval;
        response->timestamps.push_back(timestamp);
        
        // 生成模拟数据，简单的正弦波数据
        double value = 23.5 + 2.0 * sin(timestamp / 50.0);
        response->values.push_back(value);
      }
      
      response->success = true;
      response->message = "数据查询成功";
    }
    else
    {
      response->success = false;
      response->message = "未找到指定传感器ID";
    }
    
    RCLCPP_INFO(this->get_logger(), "响应已发送，状态: %s", 
               response->success ? "成功" : "失败");
  }

  rclcpp::Service<sensor_interfaces::srv::SensorData>::SharedPtr service_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<SensorServiceNode>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
```

**客户端实现（sensor_client_node.cpp）**：
```cpp
#include "rclcpp/rclcpp.hpp"
#include "sensor_interfaces/srv/sensor_data.hpp"

#include <chrono>
#include <memory>

using namespace std::chrono_literals;

class SensorClientNode : public rclcpp::Node
{
public:
  SensorClientNode() : Node("sensor_client")
  {
    // 创建客户端
    client_ = this->create_client<sensor_interfaces::srv::SensorData>("get_sensor_data");
    
    // 等待服务可用
    while (!client_->wait_for_service(1s)) {
      if (!rclcpp::ok()) {
        RCLCPP_ERROR(this->get_logger(), "等待服务超时或被中断");
        return;
      }
      RCLCPP_INFO(this->get_logger(), "等待传感器服务可用...");
    }
    
    // 发送请求
    send_request();
  }

private:
  void send_request()
  {
    // 创建请求
    auto request = std::make_shared<sensor_interfaces::srv::SensorData::Request>();
    request->sensor_id = "temp_sensor_01";
    request->data_type = 1; // 温度数据
    request->time_from = 1000.0;
    request->time_to = 2000.0;
    
    RCLCPP_INFO(this->get_logger(), "发送传感器数据请求...");
    
    // 发送异步请求
    auto future = client_->async_send_request(
      request,
      [this](rclcpp::Client<sensor_interfaces::srv::SensorData>::SharedFuture future) {
        auto response = future.get();
        process_response(response);
      });
  }
  
  void process_response(const std::shared_ptr<sensor_interfaces::srv::SensorData::Response> response)
  {
    if (response->success) {
      RCLCPP_INFO(this->get_logger(), "成功获取传感器数据，数据点数: %zu", response->values.size());
      
      // 打印部分数据点
      for (size_t i = 0; i < response->values.size() && i < 3; ++i) {
        RCLCPP_INFO(this->get_logger(), "  时间戳: %.2f, 值: %.2f", 
                   response->timestamps[i], response->values[i]);
      }
    } else {
      RCLCPP_ERROR(this->get_logger(), "获取数据失败: %s", response->message.c_str());
    }
  }

  rclcpp::Client<sensor_interfaces::srv::SensorData>::SharedPtr client_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<SensorClientNode>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
```

## 四、gRPC框架

gRPC是Google开发的高性能、开源的通用RPC框架。

### 1. 总体架构
- 基于HTTP/2协议
- 使用Protobuf进行接口定义和序列化
- 多语言支持

### 2. 接口格式
- 使用`.proto`文件定义服务
- 支持流式RPC (单向流和双向流)
- 支持拦截器机制

### 3. 代表性接口
```proto
// 服务定义
service Greeter {
  rpc SayHello (HelloRequest) returns (HelloReply) {}
  rpc SayHelloStream (stream HelloRequest) returns (stream HelloReply) {}
}

message HelloRequest {
  string name = 1;
}

message HelloReply {
  string message = 1;
}
```

```cpp
// 服务端实现
class GreeterServiceImpl final : public Greeter::Service {
  Status SayHello(ServerContext* context, const HelloRequest* request,
                  HelloReply* reply) override {
    reply->set_message("Hello " + request->name());
    return Status::OK;
  }
};
```

## 五、各框架比较与适用场景

| 框架 | 序列化 | 通信机制 | 特点 | 适用场景 |
| ---- | ------ | -------- | ---- | -------- |
| AimRT | 二进制自定义 | 共享内存 | 高性能、低延迟、适合本地通信 | 同机器多进程通信、实时系统 |
| CyberRT | Protobuf | 多种传输层 | 组件化、面向自动驾驶 | 自动驾驶、复杂系统集成 |
| ROS2 | DDS/CDR | DDS中间件 | 分布式、发布订阅 | 机器人系统、分布式控制 |
| gRPC | Protobuf | HTTP/2 | 跨语言、跨平台、流式处理 | 微服务、跨语言系统、云服务 |

## 六、rpclib框架

rpclib是一个现代C++的msgpack-RPC服务器和客户端库，专注于简洁和易用性。

### 1. 总体架构
- 基于msgpack序列化格式
- 使用现代C++11/14特性实现
- 支持同步调用模式
- 轻量级设计，无外部依赖

### 2. 接口格式
- 服务器端使用`bind`方法注册函数
- 客户端使用`call`方法远程调用
- 支持C++函数、Lambda表达式和std::function
- 支持自动类型转换

### 3. 代表性接口
```cpp
// 服务器端
rpc::server srv(8080);
srv.bind("add", [](int a, int b) { 
    return a + b; 
});

// 客户端
rpc::client c("localhost", 8080);
int result = c.call("add", 2, 3).as<int>();
```

### 4. 使用示例

**服务器端示例（device_server.cpp）**：
```cpp
#include "rpc/server.h"
#include <string>
#include <map>
#include <iostream>

// 模拟设备管理系统
class DeviceManager {
public:
    // 注册新设备
    bool registerDevice(const std::string& id, const std::string& type, const std::string& location) {
        std::cout << "注册设备: " << id << ", 类型: " << type << std::endl;
        
        // 检查设备ID是否已存在
        if (devices_.find(id) != devices_.end()) {
            return false; // 设备ID已存在
        }
        
        // 建立设备信息数据结构
        DeviceInfo info;
        info.type = type;
        info.location = location;
        info.status = "在线";
        
        // 存储设备信息
        devices_[id] = info;
        return true;
    }
    
    // 获取设备状态
    std::string getDeviceStatus(const std::string& id) {
        auto it = devices_.find(id);
        if (it != devices_.end()) {
            return it->second.status;
        }
        return "未找到设备";
    }
    
    // 控制设备
    bool controlDevice(const std::string& id, const std::string& command) {
        std::cout << "控制设备: " << id << ", 命令: " << command << std::endl;
        
        auto it = devices_.find(id);
        if (it == devices_.end()) {
            return false; // 设备不存在
        }
        
        // 模拟设备控制处理
        if (command == "开启") {
            it->second.status = "运行中";
            return true;
        } else if (command == "关闭") {
            it->second.status = "已关闭";
            return true;
        } else {
            return false; // 未知命令
        }
    }
    
    // 获取所有设备ID
    std::vector<std::string> getAllDeviceIds() {
        std::vector<std::string> ids;
        for (const auto& device : devices_) {
            ids.push_back(device.first);
        }
        return ids;
    }
    
private:
    struct DeviceInfo {
        std::string type;
        std::string location;
        std::string status;
    };
    
    std::map<std::string, DeviceInfo> devices_;
};

int main() {
    // 初始化设备管理器
    DeviceManager deviceManager;
    
    // 创建 RPC 服务器并监听端口
    rpc::server server(8080);
    
    // 注册设备管理功能
    server.bind("registerDevice", [&deviceManager](const std::string& id, 
                                             const std::string& type,
                                             const std::string& location) {
        return deviceManager.registerDevice(id, type, location);
    });
    
    // 获取设备状态功能
    server.bind("getDeviceStatus", [&deviceManager](const std::string& id) {
        return deviceManager.getDeviceStatus(id);
    });
    
    // 控制设备功能
    server.bind("controlDevice", [&deviceManager](const std::string& id, const std::string& command) {
        return deviceManager.controlDevice(id, command);
    });
    
    // 获取所有设备ID功能
    server.bind("getAllDeviceIds", [&deviceManager]() {
        return deviceManager.getAllDeviceIds();
    });
    
    std::cout << "设备管理服务器已启动，监听端口 8080..." << std::endl;
    
    // 启动服务器
    server.run();
    
    return 0;
}
```

**客户端示例（device_client.cpp）**：
```cpp
#include "rpc/client.h"
#include <iostream>
#include <string>
#include <vector>

int main() {
    // 创建 RPC 客户端
    rpc::client client("localhost", 8080);
    
    try {
        // 注册新设备
        bool result1 = client.call("registerDevice", "device001", "温度传感器", "生产线-A").as<bool>();
        std::cout << "注册设备 device001: " << (result1 ? "成功" : "失败") << std::endl;
        
        bool result2 = client.call("registerDevice", "device002", "湿度传感器", "生产线-B").as<bool>();
        std::cout << "注册设备 device002: " << (result2 ? "成功" : "失败") << std::endl;
        
        // 控制设备
        bool ctrl_result = client.call("controlDevice", "device001", "开启").as<bool>();
        std::cout << "控制设备 device001: " << (ctrl_result ? "成功" : "失败") << std::endl;
        
        // 获取设备状态
        std::string status = client.call("getDeviceStatus", "device001").as<std::string>();
        std::cout << "设备 device001 状态: " << status << std::endl;
        
        // 获取所有设备ID
        std::vector<std::string> ids = client.call("getAllDeviceIds").as<std::vector<std::string>>();
        std::cout << "所有设备ID:" << std::endl;
        for (const auto& id : ids) {
            std::cout << "  - " << id << std::endl;
        }
        
    } catch (rpc::rpc_error& e) {
        std::cout << "调用失败: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
```

## 七、TRPC框架

TRPC是腾讯开发的多语言、可插拔、高性能的RPC框架，设计基于可插拔的理念。

### 1. 总体架构
- 基于Protocol Buffers实现跨语言服务通信
- 多协议支持，可与不同框架互操作（如gRPC）
- 所有组件都是可插拔的，支持多种微服务组件对接
- 支持流式RPC，适合大文件上传/下载、消息推送、AI语音识别等场景

### 2. 接口格式
- 基于标准的Protocol Buffers定义服务接口
- 请求/响应消息头格式标准化
- 支持流式传输和一路调用
- 丰富的透传信息机制（trans_info）

### 3. 代表性接口
```proto
// 请求消息头定义
message RequestProtocol {
  uint32    version        = 1;   // 协议版本
  uint32    call_type      = 2;   // 调用类型（单向、一路等）
  uint32    request_id     = 3;   // 请求唯一ID
  uint32    timeout        = 4;   // 请求超时时间（毫秒）
  bytes     caller         = 5;   // 调用者名称
  bytes     callee         = 6;   // 被调用者名称
  bytes     func           = 7;   // 被调用的接口名
  map<string, bytes> trans_info = 9;  // 透传信息
}

// 响应消息头定义
message ResponseProtocol {
  uint32    version        = 1;   // 协议版本
  uint32    call_type      = 2;   // 调用类型
  uint32    request_id     = 3;   // 请求唯一ID
  int32     ret            = 4;   // 错误码
  int32     func_ret       = 5;   // 接口错误码
  bytes     error_msg      = 6;   // 错误信息
  map<string, bytes> trans_info = 8;  // 透传信息
}
```

### 4. 使用示例

**服务定义（trpc的Go实现）**：
```proto
// helloworld.proto
syntax = "proto3";

package helloworld;
option go_package = "trpc.group/trpcprotocol/helloworld";

service Greeter {
  // SayHello方法定义
  rpc SayHello(HelloRequest) returns (HelloReply) {}
}

// 请求消息
message HelloRequest {
  string name = 1;
  int32 age = 2;
}

// 响应消息
message HelloReply {
  string message = 1;
}
```

**服务端实现（Go）**：
```go
// server.go
package main

import (
	"context"
	"fmt"

	"trpc.group/trpc-go/trpc-go/errs"
	"trpc.group/trpc-go/trpc-go/log"
	"trpc.group/trpc-go/trpc-go"
	pb "trpc.group/trpcprotocol/helloworld"
)

// 服务实现
type greeterServiceImpl struct{}

// SayHello方法实现
func (s *greeterServiceImpl) SayHello(ctx context.Context, req *pb.HelloRequest) (*pb.HelloReply, error) {
	// 记录请求日志
	log.Infof("received request: name=%s, age=%d", req.Name, req.Age)
	
	// 检验请求参数
	if req.Name == "" {
		return nil, errs.New(1001, "用户名不能为空")
	}
	
	// 构建响应
	return &pb.HelloReply{
		Message: fmt.Sprintf("你好, %s! 你现在%d岁了", req.Name, req.Age),
	}, nil
}

func main() {
	s := trpc.NewServer()
	
	// 注册服务实现
	pb.RegisterGreeterService(s, &greeterServiceImpl{})
	
	// 启动服务并阻塞
	if err := s.Serve(); err != nil {
		log.Fatalf("server terminated: %v", err)
	}
}
```

**客户端实现（Go）**：
```go
// client.go
package main

import (
	"context"
	"fmt"
	"time"

	"trpc.group/trpc-go/trpc-go"
	"trpc.group/trpc-go/trpc-go/client"
	pb "trpc.group/trpcprotocol/helloworld"
)

func main() {
	// 初始化TRPC客户端
	trpc.NewServer()
	
	// 创建客户端代理
	proxy := pb.NewGreeterClientProxy(client.WithTarget("trpc.helloworld.Greeter"))
	
	// 创建请求上下文
	ctx, cancel := context.WithTimeout(context.Background(), time.Second*3)
	defer cancel()
	
	// 发送请求
	req := &pb.HelloRequest{
		Name: "张三",
		Age:  28,
	}
	
	// 调用远程服务
	resp, err := proxy.SayHello(ctx, req)
	if err != nil {
		fmt.Printf("调用失败: %v\n", err)
		return
	}
	
	// 处理响应
	fmt.Printf("服务响应: %s\n", resp.Message)
}
```

## 八、XBot2 RPC框架

XBot2是一个专为机器人应用开发的实时中间件，其RPC实现专注于高性能和实时性。

### 1. 总体架构
- 基于C++模板实现的类型安全RPC框架
- 专为实时机器人控制系统设计
- 服务器-客户端模式，支持同步调用

### 2. 接口格式特点
- **类型安全**：使用C++模板确保类型安全的调用
- **同步调用**：支持带超时的同步调用模式
- **插件机制**：通过插件系统注册服务组件
- **实时性**：设计适用于高优先级实时线程

### 3. 使用示例

**服务器端实现**：
```cpp
bool Server::on_initialize()
{
    // 注册服务
    _srv = advertiseService("set_gain",
                            &Server::srv_handler, this);

    _gain.setZero();

    // 返回服务是否成功注册
    return bool(_srv);
}

void Server::run()
{
    // 处理传入请求
    _srv->run();
}

bool Server::srv_handler(const Eigen::Matrix6d& req, bool& res)
{
    _gain = req;
    res = true;

    // 记录成功消息(高优先级)
    jhigh().jok("successfully set gain to \n{}\n", req);

    return true;
}

// 注册插件
XBOT2_REGISTER_PLUGIN(Server, server)
```

**客户端实现**：
```cpp
bool Client::on_initialize()
{
    // 声明服务客户端
    _cli = serviceClient<Eigen::Matrix6d, bool>("set_gain");

    // 从参数获取调用间隔
    _period_between_calls = 500ms;
    getParam("~period_between_calls", _period_between_calls);

    return bool(_cli);
}

void Client::run()
{
    auto now = chrono::steady_clock::now();

    // 周期性调用控制
    if(now < _last_call_time + _period_between_calls)
    {
        return;
    }

    _last_call_time = now;

    // 准备请求参数
    Eigen::Matrix6d mat;
    mat.setRandom();

    jhigh().jinfo("calling service with matrix: \n{}\n", mat);

    auto tic = timer_clock::now();
    bool result = false;

    // 调用服务，带超时设置
    if(!_cli->call(mat, result, 500ms))
    {
        jerror("call failed \n");
        return;
    }
    std::chrono::duration<double> dt = timer_clock::now() - tic;

    jhigh().jinfo("returned after {} with result {}\n", dt, result);
}

// 注册插件
XBOT2_REGISTER_PLUGIN(Client, client)
```

### 4. XBot2 RPC特点总结

1. **模板化接口**：
   - 服务器端使用`advertiseService<RequestType, ResponseType>(name, handler, context)`注册服务
   - 客户端使用`serviceClient<RequestType, ResponseType>(name)`创建客户端

2. **同步调用模型**：
   - 客户端通过`call(request, response, timeout)`方法发起同步调用
   - 支持超时设置，避免无限等待

3. **插件系统集成**：
   - 服务组件通过`XBOT2_REGISTER_PLUGIN`宏注册到框架
   - 组件生命周期管理：初始化、启动、运行、停止

4. **周期性调用模式**：
   - 支持在控制循环中周期性调用服务
   - 使用时间管理避免过于频繁的调用

5. **实时性考虑**：
   - 区分实时时钟(steady_clock)和高精度时钟(high_resolution_clock)
   - 通过`run()`方法处理请求，适用于实时控制循环

## 九、RPC接口设计最佳实践

### 1. 接口定义
- 使用IDL定义接口，确保语言无关性
- 版本化API设计，确保向后兼容
- 清晰的错误码和状态返回机制

### 2. 性能优化
- 选择合适的序列化方式（权衡性能和跨平台性）
- 合理设置超时和重试机制
- 考虑流式传输大数据

### 3. 安全性
- 支持身份认证和授权
- 数据加密传输
- 限流和熔断机制