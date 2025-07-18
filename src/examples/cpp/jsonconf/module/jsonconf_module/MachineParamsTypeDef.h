#ifndef CONFIGS_MECHANICAL_ARM_MACHINE_PARAMS_H
#define CONFIGS_MECHANICAL_ARM_MACHINE_PARAMS_H

#include <cstdint>
#include <stdint.h>
#include <string>
#include <vector>
#include "util/nlohmann_json_util.h"

// 使用nlohmann::json命名空间
using json = nlohmann::json;

namespace robot_bodyparams {

#define AXIS_NUM (8)

/**
 * 变量阈值
 */
typedef struct {
  double dSpeedWave[AXIS_NUM];
  double dHighFrequencyPercentage[AXIS_NUM];
  double dAvoidAlarm[AXIS_NUM];
}S_VAR_THRESHOLD;

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(S_VAR_THRESHOLD,
  dSpeedWave,
  dHighFrequencyPercentage,
  dAvoidAlarm
);

/**
 * 扭矩限制参数
 */
typedef struct {
  int iAvrgTrqCheckFlag;
  int iMaxTrqCheckFlag;
  int trqMaxLimitPercent[AXIS_NUM];
  int trqAvrgLimitPercent[AXIS_NUM];
  int DefaulttrqMaxLimit[AXIS_NUM];
  int DefaulttrqAvrgLimit[AXIS_NUM];
  int SvoMaxTrqLimit[AXIS_NUM];
  int MaxTrqAvrgLimit[AXIS_NUM];
}S_TORQUE_LIMIT;

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(S_TORQUE_LIMIT,
  iAvrgTrqCheckFlag,
  iMaxTrqCheckFlag,
  trqMaxLimitPercent,
  trqAvrgLimitPercent,
  DefaulttrqMaxLimit,
  DefaulttrqAvrgLimit,
  SvoMaxTrqLimit,
  MaxTrqAvrgLimit
);

/**
 * 关节参数
 */
struct S_JOINT_PARAMS {
  /*零点 */
  double dAbsZero[AXIS_NUM];
  /*减速比*/
  double dRatio[AXIS_NUM];
  /*电机转动方向*/
  int32_t i32MotorDir[AXIS_NUM];
  /*编码器位数*/
  int32_t i32EncBit[AXIS_NUM];
  /*耦合参数*/
  double dCoupParamMaster[AXIS_NUM];
  double dCoupParamSlave[AXIS_NUM];
  /*限位*/
  double dPosLimit[AXIS_NUM];
  double dNegLimit[AXIS_NUM];
  /*最大速度*/
  double dMaxVelLimit[AXIS_NUM];
  /*跟随误差限制*/
  double dErrPos[AXIS_NUM];
  /*变量阈值*/
  S_VAR_THRESHOLD stVarThreshold;
  /*扭矩限制*/
  S_TORQUE_LIMIT stTorqueLimit;
  /**/
  double dCoupParam[AXIS_NUM][AXIS_NUM];
  /*DB阻值*/
  uint16_t u16DBMaxValue[AXIS_NUM];
  uint16_t u16DBMinValue[AXIS_NUM];
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(S_JOINT_PARAMS,
  dAbsZero,
  dRatio,
  i32MotorDir,
  i32EncBit,
  dCoupParamMaster,
  dCoupParamSlave,
  dPosLimit,
  dNegLimit,
  dMaxVelLimit,
  dErrPos,
  stVarThreshold,
  stTorqueLimit,
  dCoupParam,
  u16DBMaxValue,
  u16DBMinValue
);


} // namespace robot_bodyparams

#endif