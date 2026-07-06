#include "boost_mode.h"

#include "hrtim.h"
#include "sample_processing.h"
#include "target_calibration.h"

#define BOOST_MIN_Q1_DUTY     100
#define BOOST_MIN_Q2_DUTY     100
#define BOOST_MAX_Q1_DUTY   30767
#define BOOST_MAX_Q2_DUTY   30767
#define BOOST_PERIOD_COUNTS 23039
#define BOOST_CMP2_OFFSET     921
#define BOOST_CMP_LIMIT     22120
#define BOOST_ADC_CMP_LIMIT  7000

volatile BoostModeControl_t g_boost_mode_control = {0};

static int32_t BoostMode_ClampDuty(int32_t value, int32_t min_value, int32_t max_value)
{
  if (value > max_value)
  {
    return max_value;
  }

  if (value < min_value)
  {
    return min_value;
  }

  return value;
}

void BoostMode_Reset(void)
{
  g_boost_mode_control.error = 0;
  g_boost_mode_control.prop = 0;
  g_boost_mode_control.inte = 0;
  g_boost_mode_control.u0 = 0;
  g_boost_mode_control.q1_duty = BOOST_MIN_Q1_DUTY;
  g_boost_mode_control.q2_duty = BOOST_MIN_Q2_DUTY;
  g_boost_mode_control.q1_max_duty = BOOST_MIN_Q1_DUTY;
  g_boost_mode_control.q2_max_duty = BOOST_MIN_Q2_DUTY;
  g_boost_mode_control.cmp1 = 0;
  g_boost_mode_control.cmp2 = 0;
  g_boost_mode_control.cmp3 = 0;
  g_boost_mode_control.cmp4 = 0;
}

void BoostMode_SeedStartup(void)
{
  g_boost_mode_control.q1_max_duty = BOOST_MIN_Q1_DUTY;
  g_boost_mode_control.q2_max_duty = BOOST_MIN_Q2_DUTY;
  g_boost_mode_control.q1_duty = BOOST_MIN_Q1_DUTY;
  g_boost_mode_control.q2_duty = BOOST_MIN_Q2_DUTY;
  g_boost_mode_control.inte = g_boost_mode_control.q2_duty << 8;
  g_boost_mode_control.u0 = g_boost_mode_control.q2_duty;
}

void BoostMode_UpdateStartupCeilings(void)
{
  if (g_boost_mode_control.q2_max_duty < BOOST_MAX_Q2_DUTY)
  {
    g_boost_mode_control.q2_max_duty += 300;
    if (g_boost_mode_control.q2_max_duty > BOOST_MAX_Q2_DUTY)
    {
      g_boost_mode_control.q2_max_duty = BOOST_MAX_Q2_DUTY;
    }
  }

  if (g_boost_mode_control.q1_max_duty < BOOST_MAX_Q1_DUTY)
  {
    g_boost_mode_control.q1_max_duty += 300;
    if (g_boost_mode_control.q1_max_duty > BOOST_MAX_Q1_DUTY)
    {
      g_boost_mode_control.q1_max_duty = BOOST_MAX_Q1_DUTY;
    }
  }
}

void BoostMode_RunLoop(void)
{
  const int32_t kp = 100;
  const int32_t ki = 10;
  int32_t maxinte;

  g_boost_mode_control.error = g_target_calibration.vref_target - g_target_calibration.vx_calibrated;
  g_boost_mode_control.prop = g_boost_mode_control.error * kp;
  g_boost_mode_control.inte = g_boost_mode_control.inte + g_boost_mode_control.error * ki;

  maxinte = g_boost_mode_control.q2_max_duty << 8;
  if (g_boost_mode_control.inte > maxinte)
  {
    g_boost_mode_control.inte = maxinte;
  }
  else if (g_boost_mode_control.inte < 0)
  {
    g_boost_mode_control.inte = 0;
  }

  g_boost_mode_control.u0 = (g_boost_mode_control.prop + g_boost_mode_control.inte) >> 8;
  g_boost_mode_control.q2_duty = BoostMode_ClampDuty(
      g_boost_mode_control.u0,
      BOOST_MIN_Q2_DUTY,
      g_boost_mode_control.q2_max_duty);
  g_boost_mode_control.q1_duty = 32767 - g_boost_mode_control.q2_duty;
  if (g_boost_mode_control.q1_duty > g_boost_mode_control.q1_max_duty)
  {
    g_boost_mode_control.q1_duty = g_boost_mode_control.q1_max_duty;
  }
  if (g_boost_mode_control.q1_duty < BOOST_MIN_Q1_DUTY)
  {
    g_boost_mode_control.q1_duty = BOOST_MIN_Q1_DUTY;
  }
}

void BoostMode_ApplyPwm(void)
{
  g_boost_mode_control.cmp1 = (g_boost_mode_control.q2_duty * BOOST_PERIOD_COUNTS) >> 15;
  g_boost_mode_control.cmp2 = g_boost_mode_control.cmp1 + BOOST_CMP2_OFFSET;
  g_boost_mode_control.cmp3 = g_boost_mode_control.cmp2 + ((g_boost_mode_control.q1_duty * BOOST_PERIOD_COUNTS) >> 15);
  g_boost_mode_control.cmp4 = g_boost_mode_control.cmp1 >> 1;

  if (g_boost_mode_control.cmp2 > BOOST_CMP_LIMIT)
  {
    g_boost_mode_control.cmp2 = BOOST_CMP_LIMIT;
  }
  if (g_boost_mode_control.cmp3 > BOOST_CMP_LIMIT)
  {
    g_boost_mode_control.cmp3 = BOOST_CMP_LIMIT;
  }
  if (g_boost_mode_control.cmp4 > BOOST_ADC_CMP_LIMIT)
  {
    g_boost_mode_control.cmp4 = BOOST_ADC_CMP_LIMIT;
  }

  HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_A].CMP1xR = (uint32_t)g_boost_mode_control.cmp1;
  HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_A].CMP2xR = (uint32_t)g_boost_mode_control.cmp2;
  HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_A].CMP3xR = (uint32_t)g_boost_mode_control.cmp3;
  HRTIM1->sTimerxRegs[HRTIM_TIMERINDEX_TIMER_A].CMP4xR = (uint32_t)g_boost_mode_control.cmp4;
}
