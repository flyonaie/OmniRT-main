// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

#pragma once

#include <string>
#include <vector>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Module manager interface
 *
 */
typedef struct {
  /**
   * @brief Function to get module list
   * @note
   * Input 1: Implement pointer to module manager handle
   */
  std::vector<std::string> (*get_module_list)(void* impl);

  /**
   * @brief Function to get module
   * @note
   * Input 1: Implement pointer to module manager handle
   * Input 2: Module name
   */
  void* (*get_module)(void* impl, const std::string& module_name);

  /// Implement pointer
  void* impl;
} aimrt_module_manager_base_t;

#ifdef __cplusplus
}
#endif
