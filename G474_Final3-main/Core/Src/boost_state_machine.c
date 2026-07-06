#include "boost_state_machine.h"

#include "boost_mode.h"
#include "hrtim.h"
#include "sample_processing.h"
#include "target_calibration.h"
#include "runtime_mode.h"
#include "minimal_state_machine.h"
#include "main.h"

volatile BoostStateMachine_t g_boost_state_machine = {BOOST_STATE_INIT, 0U, 0U, 0U, 0U};

static void BoostStateMachine_StopToWait(void)
{
  HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TA1 | HRTIM_OUTPUT_TA2);
  g_boost_state_machine.state = BOOST_STATE_WAIT;
  g_boost_state_machine.wait_counter = 0U;
  g_boost_state_machine.rise_counter = 0U;
  g_boost_state_machine.pwm_enabled = 0U;
  g_boost_state_machine.output_enabled_request = 0U;
  g_minimal_state_machine.state = STATE_WAIT;
  g_minimal_state_machine.wait_counter = 0U;
  g_minimal_state_machine.rise_stage = 0U;
  g_minimal_state_machine.ta1_output_enabled = 0U;
  g_minimal_state_machine.ta2_output_enabled = 0U;
  g_minimal_state_machine.loop_control_enable = 0U;
  g_minimal_state_machine.q1_closed_loop_enable = 0U;
  BoostMode_Reset();
  RuntimeMode_Reset();
  PowerProtection_Reset();
}

void BoostStateMachine_Init(void)
{
  g_boost_state_machine.state = BOOST_STATE_WAIT;
  g_boost_state_machine.wait_counter = 100U;
  g_boost_state_machine.rise_counter = 0U;
  g_boost_state_machine.pwm_enabled = 0U;
  g_boost_state_machine.output_enabled_request = 0U;
  BoostMode_Reset();
}

void BoostStateMachine_Tick5ms(void)
{
  if ((RuntimeMode_Get() == RUNTIME_MODE_BOOST) &&
      (g_boost_state_machine.state != BOOST_STATE_WAIT) &&
      (HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin) != GPIO_PIN_RESET))
  {
    BoostStateMachine_StopToWait();
    return;
  }

  switch (g_boost_state_machine.state)
  {
  case BOOST_STATE_INIT:
    if (g_sample_processed.offset_ready != 0U)
    {
      g_boost_state_machine.wait_counter = 0U;
      g_boost_state_machine.state = BOOST_STATE_WAIT;
    }
    break;

  case BOOST_STATE_WAIT:
    if (g_boost_state_machine.wait_counter < 100U)
    {
      g_boost_state_machine.wait_counter++;
    }

    if ((g_boost_state_machine.wait_counter >= 100U) &&
        (HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin) == GPIO_PIN_RESET))
    {
      g_boost_state_machine.output_enabled_request = 1U;
    }

    if (g_boost_state_machine.output_enabled_request != 0U)
    {
      BoostMode_SeedStartup();
      g_boost_state_machine.rise_counter = 0U;
      g_boost_state_machine.state = BOOST_STATE_RISE;
    }
    break;

  case BOOST_STATE_RISE:
    if (g_boost_state_machine.pwm_enabled == 0U)
    {
      HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TA1 | HRTIM_OUTPUT_TA2);
      g_boost_state_machine.pwm_enabled = 1U;
    }

    BoostMode_UpdateStartupCeilings();
    BoostMode_ApplyPwm();

    if (g_boost_state_machine.rise_counter < 200U)
    {
      g_boost_state_machine.rise_counter++;
    }
    else
    {
      g_boost_state_machine.state = BOOST_STATE_RUN;
    }
    break;

  case BOOST_STATE_RUN:
  case BOOST_STATE_ERR:
  default:
    break;
  }
}

void BoostStateMachine_RunControlTick(void)
{
  if (g_boost_state_machine.state != BOOST_STATE_RUN)
  {
    return;
  }

  BoostMode_RunLoop();
  BoostMode_ApplyPwm();
}

const char *BoostStateMachine_StateName(BoostState_t state)
{
  switch (state)
  {
  case BOOST_STATE_INIT:
    return "Init";
  case BOOST_STATE_WAIT:
    return "Wait";
  case BOOST_STATE_RISE:
    return "Rise";
  case BOOST_STATE_RUN:
    return "Run";
  case BOOST_STATE_ERR:
    return "Err";
  default:
    return "Unknown";
  }
}
