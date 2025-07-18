### 1.aimrt_core_instance_init

### 2.core_manager_init
2.1.yaml_cfg_parse
2.2.core_proxy_init
1、core_proxy_init
2.3.core_proxy_map_init

### 3.module_wrapper_init
#### 3.1.core_manager_proxy_init(gen base_t)
1、make_unique->configurator_proxy_gen->configurator_base
2、make_unique->executor_proxy_gen->executor_base
3、make_unique->logger_proxy_gen->logger_base
4、make_unique->parameter_proxy_gen->parameter_base
5、make_unique->channel_proxy_gen->channel_base
6、make_unique->rpc_proxy_gen->rpc_base

#### 3.2.module_core_proxy_cfg(core_mgr_proxy_gen)
1、每个模块都要配置代理
#### 3.3.module_base_init(this, core_base)
#### 3.4.module_user_init(CoreRef(core_base))
1、CoreRef->ConfiguratorRef(configurator_base)
2、CoreRef->AllocatorRef(allocator_base)
3、CoreRef->ExecutorManagerRef(executor_manager_base)->ExecutorRef(executor_base)
4、CoreRef->LoggerRef(logger_base)
5、CoreRef->ChannelHandleRef(channel_handle_base)->PublisherRef(channel_publisher_base)&SubscriberRef(channel_subscriber_base)
5.1、PublisherProxy获取或新建（由用户程序中决定）
5.2、SubscriberProxy获取或新建（由用户程序中决定）
6、CoreRef->RpcHandleRef(rpc_handle_base)
7、CoreRef->ParameterHandleRef(parameter_handle_base)

### 对外接口采用ref引用类模式，对base_t进行封装；底层实现采用proxy类模式，实例化时通过make_unique生成proxy，生成base
interface
|  ref(wrapper)
module_base&core_base
|  proxy(gen)
implement


设计模式是C++中实现接口桥接的常用方式,允许C风格的接口(aimrt_channel_handle_base_t)能够调用C++类的成员函数
### 设计模式举例
------------------------------------------------------------------------------------------
----------------------------------interface layer-----------------------------------------
------------------------------------------------------------------------------------------
```c
// 定义接口
typedef struct {
      /**
   * @brief Get demo value from core
   *
   */
    void (*get_demo_value)(void *impl,int *);

    /// Implement pointer
    void *impl;
}aimrt_demo_base_t;

typedef struct {
  /**
   * @brief Function to get demo
   * @note
   * Input 1: Implement pointer to demo manager handle
   * Input 2: Demo name
   */
  const aimrt_demo_base_t* (*get_demo)(void* impl, aimrt_string_view_t demo_name);

  /// Implement pointer
  void* impl;
} aimrt_demo_manager_base_t;
```

```cpp
// 基于aimrt_demo_base_t实现接口C++封装
class DemoRef {
 public:
  DemoRef() = default;
  explicit DemoRef(const aimrt_demo_base_t* base_ptr)
      : base_ptr_(base_ptr) {}
  ~DemoRef() = default;
  explicit operator bool() const { return (base_ptr_ != nullptr); }
  const aimrt_demo_base_t* NativeHandle() const { return base_ptr_; }
  void GetDemoValue(int *value) {
    AIMRT_ASSERT(base_ptr_, "Reference is null.");
    base_ptr_->get_demo_value(base_ptr_->impl, value);
  }
  private:
  const aimrt_demo_base_t* base_ptr_ = nullptr;
}
```

------------------------------------------------------------------------------------------
-------------------------------------impl layer-------------------------------------------
------------------------------------------------------------------------------------------
```cpp
// proxy
class DemoProxy {
 public:
  explicit DemoProxy()
      : base_(GenBase(this)) {}
  ~DemoProxy() = default;

  DemoProxy(const DemoProxy&) = delete;
  DemoProxy& operator=(const DemoProxy&) = delete;

  const aimrt_demo_base_t* NativeHandle() const { return &base_; }

 private:
  static void GetDemoValue(int *value) {
    *value = 2025;
    return;
  }

  static aimrt_demo_base_t GenBase(void* impl) {
    return aimrt_demo_base_t{
        .get_demo_value = [](void* impl, int* value) -> void* {
          return static_cast<DemoProxy*>(impl)->GetDemoValue(value);
        },
        .impl = impl};
  }

 private:
  const aimrt_demo_base_t base_;
};

class DemoManagerProxy {
 public:
  using DemoProxyMap = std::unordered_map<
      std::string,
      std::unique_ptr<DemoProxy>,
      aimrt::common::util::StringHash,
      aimrt::common::util::StringEqual>;

 public:
  explicit DemoManagerProxy(const DemoProxyMap& demo_proxy_map)
      : demo_proxy_map_(demo_proxy_map),
        base_(GenBase(this)) {}

  ~DemoManagerProxy() = default;

  DemoManagerProxy(const DemoManagerProxy&) = delete;
  DemoManagerProxy& operator=(const DemoManagerProxy&) = delete;

  const aimrt_demo_manager_base_t* NativeHandle() const { 
    return &base_; }
    
 private:
  const aimrt_demo_base_t* GetDemo(aimrt_string_view_t demo_name) const {
    auto finditr = demo_proxy_map_.find(std::string(aimrt::util::ToStdStringView(demo_name)));
    if (finditr != demo_proxy_map_.end()) return finditr->second->NativeHandle();

    AIMRT_WARN("Can not find demo '{}'.", aimrt::util::ToStdStringView(demo_name));

    return nullptr;
  }

  static aimrt_demo_manager_base_t GenBase(void* impl) {
    return aimrt_demo_manager_base_t{
        .get_demo = [](void* impl, aimrt_string_view_t demo_name)->const aimrt_demo_base_t* {
          return static_cast<DemoManagerProxy*>(impl)->GetDemo(demo_name);
        },
        .impl = impl};
  }

 private:
  const DemoProxyMap& demo_proxy_map_;
  const aimrt_demo_manager_base_t base_;
};
```

```cpp
// base class
class DemoBase {
 public:
  DemoBase() = default;
  virtual ~DemoBase() = default;

  DemoBase(const DemoBase&) = delete;
  DemoBase& operator=(const DemoBase&) = delete;

  virtual void Initialize(std::string_view name, YAML::Node options_node) = 0;
  virtual void Start() = 0;
  virtual void Shutdown() = 0;

  virtual void GetDemoValue(int *value) noexcept = 0;
};
```

```cpp
// demo class
class FirstDemo : public DemoBase {
 public:
  FirstDemo()
      : logger_ptr_(std::make_shared<aimrt::common::util::LoggerWrapper>()) {}
  ~FirstDemo() override = default;

  void Initialize(std::string_view name, YAML::Node options_node) override;
  void Start() override;
  void Shutdown() override;

  void GetDemoValue(int *value) noexcept override;
};
```

```cpp
// demo manager class
class DemoManager {
 public:
  using DemoGenFunc = std::function<std::unique_ptr<DemoBase>()>;

 public:
  DemoManager()
      : logger_ptr_(std::make_shared<aimrt::common::util::LoggerWrapper>()) {}
  ~DemoManager() = default;

  DemoManager(const DemoManager&) = delete;
  DemoManager& operator=(const DemoManager&) = delete;

  void Initialize(YAML::Node options_node);
  void Start();
  void Shutdown();

  void RegisterDemoGenFunc(std::string_view type,
                           DemoGenFunc&& demo_gen_func);

  const DemoManagerProxy& GetDemoManagerProxy(const util::ModuleDetailInfo& module_info);
  const DemoManagerProxy& GetDemoManagerProxy(std::string_view module_name = "core") {
    return GetDemoManagerProxy(
        util::ModuleDetailInfo{.name = std::string(module_name), .pkg_path = "core"});
  }

  aimrt::demo::DemoRef GetDemo(std::string_view demo_name);

 private:
  void RegisterFirstDemoGenFunc();

 private:
  std::unordered_map<std::string, DemoGenFunc> demo_gen_func_map_;

  std::unordered_map<
      std::string,
      std::unique_ptr<DemoProxy>,
      aimrt::common::util::StringHash,
      aimrt::common::util::StringEqual>
      demo_proxy_map_;

  std::unordered_map<
      std::string,
      std::unique_ptr<DemoManagerProxy>>
      demo_manager_proxy_map_;
}
```
//////////
服务初始化流程：
FirstDemo实例生成函数注册
FirstDemo实例化生成
FirstDemo初始化
DemoProxy实例化生成
存储入库

模块初始化流程：
用户module注册
module加载
module实例化
module_proxy配置(获取/生成)
module_with_proxy初始化
存储入库