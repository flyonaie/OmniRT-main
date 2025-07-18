/**
 * @file main.cc
 * @brief AimRT运行时框架的主程序入口
 * @details 这个文件实现了AimRT运行时框架的主程序入口点，包括：
 *          - 命令行参数的解析和处理
 *          - 信号处理机制的设置
 *          - 核心运行时的初始化和启动
 *          - 配置文件的加载和导出
 *          - 初始化报告的生成
 * @copyright Copyright (c) 2023, AgiBot Inc.
 */

#include <csignal>
#include <fstream>
#include <iostream>
#include <string.h>
#include <sys/prctl.h>

#include "gflags/gflags.h"

#include "core/aimrt_core.h"
#include "core/util/version.h"

/**
 * @brief 命令行参数定义
 * @{
 */
//! 配置文件路径
DEFINE_string(cfg_file_path, "", "config file path");
//! 进程名称，用于在ps命令中显示
DEFINE_string(process_name, "", "process name shown in ps command");

//! 是否导出配置文件
DEFINE_bool(dump_cfg_file, false, "dump config file");
//! 导出配置文件的路径
DEFINE_string(dump_cfg_file_path, "./dump_cfg.yaml", "dump config file path");

//! 是否导出初始化报告
DEFINE_bool(dump_init_report, false, "dump init report");
//! 导出初始化报告的路径
DEFINE_string(dump_init_report_path, "./init_report.txt", "dump init report path");

//! 是否注册信号处理函数
DEFINE_bool(register_signal, true, "register handle for sigint and sigterm");
//! 运行时长（秒），0表示永久运行
DEFINE_int32(running_duration, 0, "running duration, seconds");

//! 帮助标志
DEFINE_bool(h, false, "help");
//! 版本标志
DEFINE_bool(v, false, "version");
/** @} */

// 声明gflags内部的帮助和版本标志
DECLARE_bool(help);
DECLARE_bool(helpshort);
DECLARE_bool(version);

using namespace aimrt::runtime::core;

//! 全局核心运行时指针，用于信号处理
AimRTCore* global_core_ptr = nullptr;

/**
 * @brief 信号处理函数
 * @details 处理SIGINT和SIGTERM信号，触发核心运行时的优雅关闭
 * @param sig 信号值
 */
void SignalHandler(int sig) {
  if (global_core_ptr && (sig == SIGINT || sig == SIGTERM)) {
    global_core_ptr->Shutdown();
    return;
  }

  raise(sig);
};

/**
 * @brief 打印版本信息
 */
void PrintVersion() {
  std::cout << "AimRT Version: " << util::GetAimRTVersion() << std::endl;
}

/**
 * @brief 打印使用说明
 * @details 输出完整的命令行参数说明，包括所有可用的选项和其默认值
 */
void PrintUsage() {
  std::cout << "OVERVIEW: AimRT is a high-performance runtime framework for modern robotics.\n\n"
               "VERSION: "
            << util::GetAimRTVersion()
            << "\n\nUSAGE: aimrt_main --cfg_file_path=<string> [options]\n\n"
               "OPTIONS:\n\n"
               "Generic Options:\n\n"
               "  --version                         - Show version\n"
               "  --help                            - Show help\n\n"
               "AimRT Options:\n\n"
               "  --cfg_file_path=<string>          - Path to the configuration file, default is empty\n"
               "  --dump_cfg_file                   - Dump the configuration file to a file, default is false\n"
               "  --dump_cfg_file_path=<string>     - Path to dump the configuration file, default is ./dump_cfg.yaml\n"
               "  --dump_init_report                - Dump the initialization report, default is false\n"
               "  --dump_init_report_path=<string>  - Path to dump the initialization report, default is ./init_report.txt\n"
               "  --register_signal                 - Register signal handlers for SIGINT and SIGTERM, default is true\n"
               "  --running_duration=<int>          - Running duration in seconds, default is 0 which means running forever\n"
               "  --process_name=<string>           - Process name shown in ps command, default is empty\n";
}

/**
 * @brief 处理命令行参数
 * @details 解析和处理所有命令行参数，包括：
 *          - 解析非帮助标志
 *          - 处理版本信息显示
 *          - 处理帮助信息显示
 *          - 设置进程名称
 * @param argc 参数数量
 * @param argv 参数数组
 */
void HandleCommandLineFlags(int32_t argc, char** argv) {
  if (argc == 1) {
    PrintUsage();
    exit(0);
  }

  // 解析非帮助标志
  gflags::ParseCommandLineNonHelpFlags(&argc, &argv, true);
  
  // 单独处理版本
  if (FLAGS_version || FLAGS_v) {
    PrintVersion();
    exit(0);
  }
  // 单独处理帮助标志
  if (FLAGS_help || FLAGS_h) {
    PrintUsage();
    exit(0);
  }

  // 设置进程名称（如果指定）
  if (!FLAGS_process_name.empty()) {
    std::cout << "Set process name: " << FLAGS_process_name.c_str() << std::endl;
    std::cout << "argv[0]: " << argv[0] << std::endl;
    strncpy(argv[0], FLAGS_process_name.c_str(), strlen(argv[0]));
    // 同时设置线程名称
    prctl(PR_SET_NAME, FLAGS_process_name.c_str(), 0, 0, 0);
  }
}

/**
 * @brief 主函数
 * @details 实现了AimRT运行时的主要流程：
 *          1. 处理命令行参数
 *          2. 注册信号处理函数
 *          3. 初始化核心运行时
 *          4. 根据需要导出配置文件和初始化报告
 *          5. 根据运行时长参数决定同步或异步运行
 *          6. 处理异常并优雅退出
 * 
 * @param argc 参数数量
 * @param argv 参数数组
 * @return int32_t 返回值，0表示正常退出，-1表示异常退出
 */
int32_t main(int32_t argc, char** argv) {
  HandleCommandLineFlags(argc, argv);

  if (FLAGS_register_signal) {
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);
  }

  std::cout << "AimRT start, version: " << util::GetAimRTVersion() << std::endl;

  try {
    AimRTCore core;
    global_core_ptr = &core;

    AimRTCore::Options options;
    options.cfg_file_path = FLAGS_cfg_file_path;

    core.Initialize(options);

    // 导出配置文件（如果需要）
    if (FLAGS_dump_cfg_file) {
      std::ofstream ofs(FLAGS_dump_cfg_file_path, std::ios::trunc);
      ofs << core.GetConfiguratorManager().GetRootOptionsNode();
      ofs.close();
    }

    // 导出初始化报告（如果需要）
    if (FLAGS_dump_init_report) {
      std::ofstream ofs(FLAGS_dump_init_report_path, std::ios::trunc);
      ofs << core.GenInitializationReport();
      ofs.close();
    }

    // 根据运行时长参数决定运行模式
    if (FLAGS_running_duration == 0) {
      // 同步运行模式
      core.Start();
      core.Shutdown();
    } else {
      // 异步运行模式
      auto fu = core.AsyncStart();
      std::this_thread::sleep_for(std::chrono::seconds(FLAGS_running_duration));
      core.Shutdown();
      fu.wait();
    }

    global_core_ptr = nullptr;
  } catch (const std::exception& e) {
    std::cout << "AimRT run with exception and exit. " << e.what()
              << std::endl;
    return -1;
  }

  std::cout << "AimRT exit." << std::endl;

  return 0;
}
