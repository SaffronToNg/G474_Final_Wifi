#include "power_protection.h"

#include "hrtim.h"
#include "minimal_state_machine.h"
#include "sample_processing.h"

#define POWER_VIN_UVP_THRESHOLD   663
#define POWER_VIN_OVP_THRESHOLD   3072
#define POWER_VOUT_UVP_THRESHOLD  301
#define POWER_VOUT_OVP_THRESHOLD  3162
#define POWER_BUCK_OCP_THRESHOLD  1365
#define POWER_BOOST_OCP_THRESHOLD 745
#define POWER_SHORT_I_THRESHOLD   1862

volatile PowerProtection_t g_power_protection = {0U, 0, 0, 0U, 0U};

static void PowerProtection_TripToError(uint16_t error_flags)
{
  g_power_protection.error_flags = error_flags;
  HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TA1 | HRTIM_OUTPUT_TA2);
  g_minimal_state_machine.ta1_output_enabled = 0U;
  g_minimal_state_machine.ta2_output_enabled = 0U;
  g_minimal_state_machine.loop_control_enable = 0U;
  g_minimal_state_machine.q1_closed_loop_enable = 0U;
  g_minimal_state_machine.state = STATE_ERR;
}

void PowerProtection_Reset(void)
{
  g_power_protection.error_flags = POWER_ERR_NONE;
  g_power_protection.vin_feedback = 0;
  g_power_protection.vout_feedback = 0;
  g_power_protection.run_tick_count = 0U;
  g_power_protection.armed = 0U;
}

static int32_t PowerProtection_GetVinFeedback(void)
{
  if (PowerMode_GetActive() == POWER_MODE_BOOST)
  {
    return g_sample_processed.vy_raw;
  }

  return g_sample_processed.vx_raw;
}

static int32_t PowerProtection_GetVoutFeedback(void)
{
  if (PowerMode_GetActive() == POWER_MODE_BOOST)
  {
    return g_sample_processed.vx_raw;
  }

  return g_sample_processed.vy_raw;
}

void PowerProtection_Evaluate(void)
{
  int32_t vin_feedback = PowerProtection_GetVinFeedback();
  int32_t vout_feedback = PowerProtection_GetVoutFeedback();
  int32_t iout_feedback;
  uint16_t error_flags = POWER_ERR_NONE;

  g_power_protection.vin_feedback = vin_feedback;
  g_power_protection.vout_feedback = vout_feedback;

  if (PowerMode_GetActive() == POWER_MODE_BOOST)
  {
    iout_feedback = g_sample_processed.ix_zero_based;
  }
  else
  {
    iout_feedback = g_sample_processed.iy_zero_based;
  }

  if (g_minimal_state_machine.state != STATE_RUN)
  {
    g_power_protection.run_tick_count = 0U;
    g_power_protection.armed = 0U;
    return;
  }

  if (g_power_protection.run_tick_count < 200U)
  {
    g_power_protection.run_tick_count++;
  }

  if ((g_power_protection.armed == 0U) &&
      (g_power_protection.run_tick_count >= 200U) &&
      (vin_feedback > (POWER_VIN_UVP_THRESHOLD + 60)))
  {
    g_power_protection.armed = 1U;
  }

  if (g_power_protection.armed != 0U)
  {
    if (vin_feedback < POWER_VIN_UVP_THRESHOLD)
    {
      error_flags |= POWER_ERR_VIN_UVP;
    }
    else if (vin_feedback > POWER_VIN_OVP_THRESHOLD)
    {
      error_flags |= POWER_ERR_VIN_OVP;
    }

    if (vout_feedback < POWER_VOUT_UVP_THRESHOLD)
    {
      error_flags |= POWER_ERR_VOUT_UVP;
    }
    else if (vout_feedback > POWER_VOUT_OVP_THRESHOLD)
    {
      error_flags |= POWER_ERR_VOUT_OVP;
    }

    if (((PowerMode_GetActive() == POWER_MODE_BOOST) && (iout_feedback > POWER_BOOST_OCP_THRESHOLD)) ||
        ((PowerMode_GetActive() == POWER_MODE_BUCK) && (iout_feedback > POWER_BUCK_OCP_THRESHOLD)))
    {
      error_flags |= POWER_ERR_IOUT_OCP;
    }

    if ((iout_feedback > POWER_SHORT_I_THRESHOLD) && (vout_feedback < POWER_VOUT_UVP_THRESHOLD))
    {
      error_flags |= POWER_ERR_SHORT;
    }
  }

  if (g_power_protection.armed == 0U)
  {
    g_power_protection.error_flags = POWER_ERR_NONE;
    return;
  }

  g_power_protection.error_flags = error_flags;

  if (error_flags != POWER_ERR_NONE)
  {
    PowerProtection_TripToError(error_flags);
  }
}

uint16_t PowerProtection_GetFlags(void)
{
  return g_power_protection.error_flags;
}
