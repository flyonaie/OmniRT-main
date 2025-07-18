# OmniRT Architecture

## Module System Architecture

```mermaid
graph TB
    subgraph ModuleManager
        MM[ModuleManager]
        MW[ModuleWrapper]
        CP[CoreProxy]
    end
    
    subgraph Module
        MB[ModuleBase]
        CM[Custom Module]
    end
    
    subgraph Core Services
        Config[Configurator]
        Logger[Logger]
        Exec[ExecutorManager]
        Alloc[Allocator]
        RPC[RPC Handle]
        Channel[Channel Handle]
        Param[Parameter Handle]
    end
    
    MM -->|manages| MW
    MW -->|contains| CP
    MW -->|wraps| CM
    CM -->|inherits| MB
    CP -->|provides| Core Services
    MB -->|uses| CP
```

## Components Description

### ModuleManager
- Manages the lifecycle of all modules
- Handles module registration and initialization
- Controls module states (PreInit -> Init -> Start -> Shutdown)

### Module
- ModuleBase: Abstract interface for all modules
- Custom Module: Concrete implementation of modules

### Core Services
- Provides essential services for modules:
  - Configuration management
  - Logging facilities
  - Execution management
  - Memory allocation
  - RPC communication
  - Channel handling
  - Parameter management

