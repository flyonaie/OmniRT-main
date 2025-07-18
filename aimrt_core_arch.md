# AimRT Core Architecture

## 1. 核心组件结构
```mermaid
graph TB
    AimRTCore[AimRTCore<br>运行时核心]
    
    subgraph Managers[核心管理器]
        CM[ConfiguratorManager<br>配置管理器]
        PM[PluginManager<br>插件管理器]
        EM[ExecutorManager<br>执行器管理器]
        LM[LoggerManager<br>日志管理器]
        AM[AllocatorManager<br>内存分配管理器]
        PaM[ParameterManager<br>参数管理器]
        MM[ModuleManager<br>模块管理器]
    end
    
    subgraph Executors[特殊执行器]
        MTE[MainThreadExecutor<br>主线程执行器]
        GTE[GuardThreadExecutor<br>守护线程执行器]
    end
    
    AimRTCore --> Managers
    AimRTCore --> Executors
    
    classDef core fill:#f96,stroke:#333,stroke-width:4px
    classDef manager fill:#9cf,stroke:#333,stroke-width:2px
    classDef executor fill:#9f9,stroke:#333,stroke-width:2px
    
    class AimRTCore core
    class CM,PM,EM,LM,AM,PaM,MM manager
    class MTE,GTE executor
```

## 2. 状态转换流程
```mermaid
stateDiagram-v2
    [*] --> PreInit
    
    state "初始化阶段" as Init {
        PreInit --> PreInitConfigurator
        PreInitConfigurator --> PostInitConfigurator
        PostInitConfigurator --> PreInitPlugin
        PreInitPlugin --> PostInitPlugin
        PostInitPlugin --> PreInitModules
        PreInitModules --> PostInitModules
        PostInitModules --> PostInit
    }
    
    state "启动阶段" as Start {
        PostInit --> PreStart
        PreStart --> PreStartConfigurator
        PreStartConfigurator --> PostStartConfigurator
        PostStartConfigurator --> PreStartPlugin
        PreStartPlugin --> PostStartPlugin
        PostStartPlugin --> PostStart
    }
    
    state "关闭阶段" as Shutdown {
        PostStart --> PreShutdown
        PreShutdown --> PreShutdownModules
        PreShutdownModules --> PostShutdownModules
        PostShutdownModules --> PostShutdown
    }
    
    PostShutdown --> [*]
```

## 3. 核心功能特性

1. **生命周期管理**
- 完整的初始化-启动-关闭流程
- 支持同步和异步启动
- 状态转换钩子机制

2. **资源管理**
- 配置管理
- 插件系统
- 执行器调度
- 日志系统
- 内存分配
- 参数管理
- 模块管理

3. **线程管理**
- 主线程执行器
- 守护线程执行器
- 可扩展的执行器管理

4. **扩展性设计**
- 插件化架构
- 模块化管理
- 配置驱动
