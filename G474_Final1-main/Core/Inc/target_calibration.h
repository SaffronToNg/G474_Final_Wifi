#ifndef __TARGET_CALIBRATION_H__
#define __TARGET_CALIBRATION_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "power_mode.h"
#include "sample_processing.h"

#define TARGET_MAX_VREF           3012
#define TARGET_MIN_VREF           0
#define TARGET_VREF_STEP          16
#define TARGET_MAX_IREF           621
#define TARGET_MAX_IREF_BUCK      1242
#define TARGET_MIN_IREF           0
#define TARGET_IREF_STEP          25
#define TARGET_BUCK_VREF_OFFSET   3

#define TARGET_CAL_VY_K           4120
#define TARGET_CAL_VY_K_BUCK      4168
#define TARGET_CAL_VY_B           0
#define TARGET_CAL_VY_B_BOOST     12
#define TARGET_CAL_VX_K           4153
#define TARGET_CAL_VX_K_BOOST     4150
#define TARGET_CAL_VX_B           0
#define TARGET_CAL_VX_B_BOOST      0

typedef struct
{
  int32_t vy_calibrated;
  int32_t vx_calibrated;
  int32_t vadj_target_instant;
  int32_t iadj_target_instant;
  int32_t vref_target;
  int32_t iref_target;
  uint8_t current_limit_mode;
} TargetCalibration_t;

extern volatile TargetCalibration_t g_target_calibration;

/* VTAR 串口接管：1=串口直接设 vref_target，旋钮变动<10不退出；0=正常旋钮控制 */
extern uint8_t  g_vtar_serial_override;
extern int32_t  g_vtar_serial_anchor;  /* 接管瞬间的 vadj_avg 锚点 */

void TargetCalibration_UpdateInstant(void);
void TargetCalibration_UpdateStepped(void);

#ifdef __cplusplus
}
#endif

#endif /* __TARGET_CALIBRATION_H__ */
