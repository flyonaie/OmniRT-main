#pragma once

#include <fstream>
#include <iostream>
#include <string>
#include <stdexcept>
#include <optional>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <mutex>
#include <atomic>
#include "nlohmann/json_fwd.hpp"
#include "nlohmann/json.hpp"

/**
 * @brief json的adl_serializer特化
 * 
 */
namespace nlohmann {
    template<typename T, size_t M, size_t N>
    struct adl_serializer<T[M][N]> {
        static void to_json(json& j, const T (&arr)[M][N]) {
            j = json::array();
            for (size_t i = 0; i < M; i++) {
                json row = json::array();
                for (size_t j = 0; j < N; j++) {
                    row.push_back(arr[i][j]);
                }
                j.push_back(row);
            }
        }

        static void from_json(const json& j, T (&arr)[M][N]) {
            for (size_t i = 0; i < M && i < j.size(); i++) {
                for (size_t j2 = 0; j2 < N && j2 < j[i].size(); j2++) {
                    arr[i][j2] = j[i][j2].get<T>();
                }
            }
        }
    };
}


using json = nlohmann::json;

/**
 * @brief json配置类
 * 
 */
class JsonConf {
public:
    // 构造函数：加载指定JSON文件
    explicit JsonConf(const std::string& file_path)
        : file_path_(file_path) {
            LoadFromFile(file_path_);
    }

    // 从JSON文件加载内容到指定结构体（需要结构体已使用NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE定义）
    template <typename T>
    bool InitConfig(const std::string& file_path, T* ptr) {
        if (!ptr) return false;
        
        // 加载JSON文件
        std::ifstream file(file_path);
        if (!file) {
            std::cerr << "无法打开文件: " << file_path << std::endl;
            return false;
        }
        
        try {
            // 解析JSON
            json j = json::parse(file);
            
            // 使用nlohmann/json的from_json自动将JSON转换为结构体
            *ptr = j.get<T>();
            return true;
        } catch (const json::exception& e) {
            std::cerr << "解析JSON到结构体失败: " << e.what() << std::endl;
            return false;
        }
    }
    
    // 从当前已加载的JSON对象解析到指定结构体
    template <typename T>
    bool InitConfigFromLoaded(T* ptr) {
        if (!ptr) return false;
        
        try {
            // 使用nlohmann/json的自动转换功能
            *ptr = params_.get<T>();
            return true;
        } catch (const json::exception& e) {
            std::cerr << "解析JSON到结构体失败: " << e.what() << std::endl;
            return false;
        }
    }
    
    // 直接返回类型的GetValue重载版本，如果没有找到或类型不匹配则返回默认构造的对象
    template <typename T>
    T GetValue(const std::string& key) const {
        if (!params_.contains(key)) return T{};
        try {
            return params_.at(key).get<T>();
        } catch (const json::exception& e) {
            std::cerr << "Type mismatch for key '" << key << "': " << e.what() << std::endl;
            return T{};
        }
    }

    template <typename T>
    bool GetValue(const std::string& key, T& value) const {
        if (!params_.contains(key)) return false;
        try {
            value = params_.at(key).get<T>();
            return true;
        } catch (const json::exception& e) {
            std::cerr << "Type mismatch for key '" << key << "': " << e.what() << std::endl;
            return false;
        }
    }

    // 设置参数值（支持任意可序列化类型）
    template <typename T>
    void SetValue(const std::string& key, const T& value) {
        params_[key] = value;
    }

    json GetObject(const std::string& key) const {
        return params_.at(key);
    }

    // 强制刷新到文件
    void Save() const { 
        std::ofstream file(file_path_);
        if (!file) throw std::runtime_error("Failed to open config file");
        file << params_.dump(4);  // 带缩进的格式化输出
    }

    // 重新加载文件内容
    void Reload() {
        LoadFromFile(file_path_);
        // 文件加载后会自动同步到共享内存
    }
    
    std::string FileDump() const {
        return params_.dump(4);
    }
    
private:
    bool LoadFromFile(const std::string& filename) {
        std::cout << "_____loadFromFile: " << filename << std::endl;
        std::ifstream file(filename);
        if (!file) {
            // 文件不存在则初始化空JSON
            params_ = json::object();
            std::cerr << "无法打开文件: " << filename << std::endl;
            return false;
        }

        try {
            params_ = json::parse(file);
            // 显示一些基本信息
            std::cout << "JSON文件加载成功!" << std::endl;
            std::cout << "params_.dump(): " << params_.dump() << std::endl;
        } catch (const json::parse_error& e) {
            std::cerr << "处理JSON时发生错误: " << e.what() << std::endl;
            return false;
        }
        return true;
    }
private:
    json params_; // 本地JSON对象副本
    std::string file_path_; 
};