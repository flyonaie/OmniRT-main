// Copyright (c) 2023, AgiBot Inc.
// All rights reserved.

#include "aimrt_pkg_c_interface/pkg_macro.h"
#include "jsonconf_module/jsonconf_module.h"

static std::tuple<std::string_view, std::function<aimrt::ModuleBase*()>> aimrt_module_register_array[]{
    {"JsonconfModule", []() -> aimrt::ModuleBase* {
       return new aimrt::examples::cpp::jsonconf::jsonconf_module::JsonconfModule();
     }}};

AIMRT_PKG_MAIN(aimrt_module_register_array)
