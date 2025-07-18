#include <iostream>
#include <vector>
#include "MachineParamsTypeDef.h"
// #include "MachineParamsTypeDef_json.h"

bool Initialize(const std::string& config_file) {
  JsonConf jsonconf(config_file);

  // 方法1：使用get<json>()和手动访问字段
//   std::cout << "方法1：手动访问JSON字段" << std::endl;
//   auto stJointOpt = jsonconf.get<nlohmann::json>("stJoint");
//   if (stJointOpt.has_value()) {
//       auto& stJoint = stJointOpt.value();
//       // 访问dAbsZero数组
//       if (stJoint.contains("dAbsZero")) {
//           auto dAbsZeroArr = stJoint["dAbsZero"].get<std::vector<double>>();
//           std::cout << "  dAbsZero[0]: " << dAbsZeroArr[0] << std::endl;
//           std::cout << "  数组长度: " << dAbsZeroArr.size() << std::endl;
//       }
//   }

  std::cout << "\n方法2：使用结构体自动序列化" << std::endl;
  // 方法2：创建一个S_JOINT_PARAMS结构体实例
  auto jsonJoint = jsonconf.GetObject("stJoint");
  std::cout << "jsonJoint: " << jsonJoint.dump(4) << std::endl;
  
  // 方法1：通过引用参数获取值
  robot_bodyparams::S_JOINT_PARAMS data;
  jsonconf.GetValue<robot_bodyparams::S_JOINT_PARAMS>("stJoint", data);
  std::cout << "方法1 (引用传递): data.dAbsZero[0] = " << data.dAbsZero[0] << std::endl;

  
  // 方法3：您期望的方式，直接获取结构体（我们创建辅助函数实现）
  robot_bodyparams::S_JOINT_PARAMS data2 = jsonconf.GetValue<robot_bodyparams::S_JOINT_PARAMS>("stJoint");
  std::cout << "方法3 (直接获取): data2.dAbsZero[0] = " << data2.dAbsZero[0] << std::endl;
  
  // 显示文件内容摘要
  std::string file_dump = jsonconf.FileDump().substr(0, 100) + "...";
  std::cout << "\nJSON文件内容摘要: " << file_dump << std::endl;

  std::cout << "初始化成功！" << std::endl;

  return true;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
        return 1;
    }
    std::string config_file = argv[1];
    Initialize(config_file);
    return 0;
}
