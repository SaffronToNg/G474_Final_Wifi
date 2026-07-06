#ifndef __BOOST_MODE_H__
#define __BOOST_MODE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

typedef struct
{
  int32_t error;
  int32_t prop;
  int32_t inte;
  int32_t u0;
  int32_t q1_duty;
  int32_t q2_duty;
  int32_t q1_max_duty;
  int32_t q2_max_duty;
  int32_t cmp1;
  int32_t cmp2;
  int32_t cmp3;
  int32_t cmp4;
} BoostModeControl_t;

extern volatile BoostModeControl_t g_boost_mode_control;

void BoostMode_Reset(void);
void BoostMode_SeedStartup(void);
void BoostMode_UpdateStartupCeilings(void);
void BoostMode_RunLoop(void);
void BoostMode_ApplyPwm(void);

#ifdef __cplusplus
}
#endif

#endif /* __BOOST_MODE_H__ */
