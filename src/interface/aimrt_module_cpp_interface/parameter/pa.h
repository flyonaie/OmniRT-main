#include <fstream>
#include <string>
#include <stdexcept>
#include <optional>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class ParameterManager {
public:
    // 构造函数：加载指定JSON文件
    explicit ParameterManager(const std::string& file_path)
        : file_path_(file_path) {
        loadFromFile();
    }

    // 获取参数值（模板函数自动类型推导）
    template <typename T>
    std::optional<T> get(const std::string& key) const {
        if (!params_.contains(key)) return std::nullopt;
        try {
            return params_.at(key).get<T>();
        } catch (const json::exception& e) {
            throw std::runtime_error("Type mismatch for key '" + key + "': " + e.what());
        }
    }

    // 设置参数值（支持任意可序列化类型）
    template <typename T>
    void set(const std::string& key, const T& value) {
        params_[key] = value;
        // 可考虑异步保存或由调用方显式保存
    }

    // 强制刷新到文件
    void save() const {
        std::ofstream file(file_path_);
        if (!file) throw std::runtime_error("Failed to open config file");
        file << params_.dump(4);  // 带缩进的格式化输出
    }

    // 重新加载文件内容
    void reload() {
        loadFromFile();
    }

private:
    void loadFromFile() {
        std::ifstream file(file_path_);
        if (!file) {
            // 文件不存在则初始化空JSON
            params_ = json::object();
            return;
        }

        try {
            params_ = json::parse(file);
        } catch (const json::parse_error& e) {
            throw std::runtime_error("JSON parse error: " + std::string(e.what()));
        }
    }

    json params_;
    std::string file_path_;
};