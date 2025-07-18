#include <iostream>
#include "../src/client.h"

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
 * @brief 演示异步调用方法
 * 
 * @param client RPC客户端
 */
void demoAsyncCall(omnirt::rpc::Client& client) {
    std::cout << "\n===== 演示异步调用 =====" << std::endl;
    
    try {
        // 发起异步调用
        std::cout << "异步调用add函数: 10 + 20" << std::endl;
        auto future = client.AsyncCall<int>("add", 10, 20);
        
        // 模拟同时进行其他操作
        std::cout << "异步调用进行中，执行其他操作..." << std::endl;
        for (int i = 0; i < 5; i++) {
            std::cout << "."; 
            std::cout.flush();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::cout << std::endl;
        
        // 等待并获取结果
        if (future->Wait(std::chrono::milliseconds(1000))) {
            int result = future->GetResult();
            std::cout << "异步调用结果: 10 + 20 = " << result << std::endl;
        } else {
            std::cout << "异步调用超时!" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "异步调用错误: " << e.what() << std::endl;
    }
}

/**
 * @brief 进行性能测试
 * 
 * @param client RPC客户端
 * @param iterations 测试次数
 */
void performanceTest(omnirt::rpc::Client& client, int iterations) {
    std::cout << "\n===== 性能测试 =====" << std::endl;
    std::cout << "进行 " << iterations << " 次'add'函数调用" << std::endl;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; i++) {
        int result = client.Call<int>("add", i, i);
        if (result != i + i) {
            std::cout << "计算错误! " << i << "+" << i << " = " << result << std::endl;
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    double total_seconds = duration.count() / 1000000.0;
    double calls_per_second = iterations / total_seconds;
    
    std::cout << "总时间: " << total_seconds << " 秒" << std::endl;
    std::cout << "平均调用时间: " << (duration.count() / iterations) << " 微秒" << std::endl;
    std::cout << "每秒调用次数: " << calls_per_second << std::endl;
}

int main() {
    try {
        // 创建一个连接到指定通道的RPC客户端
        omnirt::rpc::Client client("rpc_demo_channel");
        
        // 等待连接到服务端
        std::cout << "连接到RPC服务器..." << std::endl;
        if (!client.WaitForConnection(std::chrono::milliseconds(5000))) {
            std::cerr << "错误: 无法连接到RPC服务器，请确保服务器已启动" << std::endl;
            return 1;
        }
        
        std::cout << "成功连接到RPC服务器" << std::endl;
        
        // 1. 调用简单函数
        std::cout << "\n调用foo函数..." << std::endl;
        client.Call<void>("foo");
        std::cout << "调用成功, 服务器应该已打印消息" << std::endl;
        
        // 2. 调用带参数的函数
        std::cout << "\n调用add函数: 2 + 3" << std::endl;
        int add_result = client.Call<int>("add", 2, 3);
        std::cout << "结果是: " << add_result << std::endl;
        
        // 3. 调用带结构体参数的函数
        std::cout << "\n调用calc函数..." << std::endl;
        DATA_B data_b;
        data_b.m = 10;
        data_b.n = 20;
        data_b.cc.a = 5;
        data_b.cc.b = 15;
        
        int calc_result = client.Call<int>("calc", 5, 15, data_b);
        std::cout << "计算结果: 5 + 15 + 10 + 20 + 5 + 15 = " << calc_result << std::endl;
        
        // 4. 演示异步调用
        demoAsyncCall(client);
        
        // 5. 进行简单的性能测试
        performanceTest(client, 1000);
        
        std::cout << "\nRPC测试完成" << std::endl;
    }
    catch (const omnirt::rpc::RpcException& e) {
        std::cerr << "RPC错误 (错误码" << static_cast<int>(e.GetErrorCode()) << "): " 
                  << e.what() << std::endl;
        return 1;
    }
    catch (const std::exception& e) {
        std::cerr << "程序错误: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}