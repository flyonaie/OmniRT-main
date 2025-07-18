#include <iostream>
#include "../src/server.h"

// 定义POD结构体，注意这些结构体必须是平凡可复制的(trivially copyable)
struct DATA_A {
    int a;
    int b;
};

struct DATA_B {
    int m;
    int n;
    DATA_A cc;
};

/**
 * @brief 简单的无参数函数示例
 */
void foo() {
    std::cout << "foo函数被调用!" << std::endl;
}

/**
 * @brief 带有基本类型和结构体参数的函数示例
 * 
 * @param a 整数参数1
 * @param b 整数参数2
 * @param data_b 结构体参数
 * @return int 计算结果
 */
int calc(int a, int b, DATA_B data_b) {
    return a + b + data_b.m + data_b.n + data_b.cc.a + data_b.cc.b;
}


int main(int argc, char *argv[]) {
    try {
        // 创建一个RPC服务器，使用默认配置
        omnirt::rpc::Server srv("rpc_demo_channel");
        
        // 绑定一个普通函数到名称"foo"
        srv.Bind("foo", &foo);

        // 绑定一个lambda函数到名称"add"
        auto add_lambda = [](int a, int b) -> int {
            std::cout << "执行加法操作: " << a << " + " << b << std::endl;
            return a + b;
        };
        // 将lambda转换为std::function然后绑定
        std::function<int(int, int)> add_func = add_lambda;
        srv.Bind("add", add_func);

        // 绑定带结构体参数的函数
        srv.Bind("calc", &calc);
        
        std::cout << "RPC服务器已启动，使用高性能二进制序列化方式.." << std::endl;
        std::cout << "等待客户端连接..." << std::endl;
        
        // 运行服务器循环（阻塞）
        srv.Run();
    }
    catch (const std::exception& e) {
        std::cerr << "服务器错误: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}