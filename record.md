1、#切换gcc版本
sudo update-alternatives --config gcc  #切换gcc版本的

2、使用AIMRT_INFO之前需要定义GetLogger()，不然报错找不到GetLogger()
inline aimrt::common::util::SimpleLogger& GetLogger() {
  static aimrt::common::util::SimpleLogger logger;
  return logger;
}