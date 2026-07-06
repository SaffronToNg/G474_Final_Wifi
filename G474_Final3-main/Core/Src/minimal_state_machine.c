#include "minimal_state_machine.h"
#include "boost_mode.h"
#include "boost_state_machine.h"
#include "hrtim.h"
#include "lowrate_loop_calc.h"
#include "power_mode.h"
#include "power_mode_hrtim.h"
#include "power_protection.h"
#include "runtime_mode.h"

#define RISE_MIN_DUTY 100
#define RISE_MAX_DUTY 30767
#define RISE_DUTY_STEP 300
#define TA1_SAFE_MAX_DUTY_Q15 30767
#define HRTIM_PERIOD_COUNTS 23039
#define HRTIM_ADC_TRIGGER_MAX 7000
#define HRTIM_Q2_OFFSET_COUNTS 921
#define HRTIM_Q2_END_MAX_COUNTS 22120
#define AUTO_MODE_DELTA_COUNTS 100
#define AUTO_MODE_VALID_COUNTS 200
#define BUCK_RISE_DUTY_STEP 360
#define BUCK_RISE_MAX_DUTY 16384
#define BUCK_RISE_TA2_ENABLE_DUTY 1024
#define BUCK_HANDOVER_MIN_MARGIN 60

static void MinimalStateMachine_ApplyTa1Duty(void)
{
  int32_t applied_q1_duty = g_minimal_state_machine.rise_q1_duty;
  int32_t comp1;
  int32_t comp4;

  if (applied_q1_duty < RISE_MIN_DUTY)
  {
    applied_q1_duty = RISE_MIN_DUTY;
  }

  if (applied_q1_duty > TA1_SAFE_MAX_DUTY_Q15)
  {
    applied_q1_duty = TA1_SAFE_MAX_DUTY_Q15;
  }

  g_minimal_state_machine.rise_q1_applied_duty = applied_q1_duty;

  comp1 = (applied_q1_duty * HRTIM_PERIOD_COUNTS) >> 15;
  if (comp1 < 1)
  {
    comp1 = 1;
  }
  if (comp1 > HRTIM_PERIOD_COUNTS)
  {
    comp1 = HRTIM_PERIOD_COUNTS;
  }

  comp4 = comp1 >> 1;
  if (comp4 < 1)
  {
    comp4 = 1;
  }
  if (comp4 > HRTIM_ADC_TRIGGER_MAX)
  {
    comp4 = HRTIM_ADC_TRIGGER_MAX;
  }

  __HAL_HRTIM_SETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A, HRTIM_COMPAREUNIT_1, (uint32_t)comp1);
  __HAL_HRTIM_SETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A, HRTIM_COMPAREUNIT_4, (uint32_t)comp4);
}

static void MinimalStateMachine_ApplyTa2Window(void)
{
  int32_t applied_q1_duty = g_minimal_state_machine.rise_q1_applied_duty;
  int32_t applied_q2_duty = g_minimal_state_machine.rise_q2_duty;
  int32_t comp1;
  int32_t comp2;
  int32_t comp3;

  if (applied_q2_duty < RISE_MIN_DUTY)
  {
    applied_q2_duty = RISE_MIN_DUTY;
  }

  if (applied_q2_duty > g_minimal_state_machine.rise_q2_max_duty)
  {
    applied_q2_duty = g_minimal_state_machine.rise_q2_max_duty;
  }

  if (applied_q2_duty > RISE_MAX_DUTY)
  {
    applied_q2_duty = RISE_MAX_DUTY;
  }

  g_minimal_state_machine.rise_q2_applied_duty = applied_q2_duty;

  comp1 = (applied_q1_duty * HRTIM_PERIOD_COUNTS) >> 15;
  if (comp1 < 1)
  {
    comp1 = 1;
  }
  if (comp1 > HRTIM_PERIOD_COUNTS)
  {
    comp1 = HRTIM_PERIOD_COUNTS;
  }

  comp2 = comp1 + HRTIM_Q2_OFFSET_COUNTS;
  if (comp2 > HRTIM_Q2_END_MAX_COUNTS)
  {
    comp2 = HRTIM_Q2_END_MAX_COUNTS;
  }

  comp3 = comp2 + ((applied_q2_duty * HRTIM_PERIOD_COUNTS) >> 15);
  if (comp3 > HRTIM_Q2_END_MAX_COUNTS)
  {
    comp3 = HRTIM_Q2_END_MAX_COUNTS;
  }

  __HAL_HRTIM_SETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A, HRTIM_COMPAREUNIT_2, (uint32_t)comp2);
  __HAL_HRTIM_SETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A, HRTIM_COMPAREUNIT_3, (uint32_t)comp3);
}

static void MinimalStateMachine_UpdateIndicators(void)
{
  HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LED_Y_GPIO_Port, LED_Y_Pin, GPIO_PIN_RESET);

  switch (g_minimal_state_machine.state)
  {
  case STATE_INIT:
    HAL_GPIO_WritePin(LED_Y_GPIO_Port, LED_Y_Pin, GPIO_PIN_SET);
    break;

  case STATE_WAIT:
    HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_SET);
    break;

  case STATE_RISE:
    HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED_Y_GPIO_Port, LED_Y_Pin, GPIO_PIN_SET);
    break;

  case STATE_RUN:
    HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_SET);
    break;

  case STATE_ERR:
    break;

  default:
    break;
  }
}

volatile MinimalStateMachine_t g_minimal_state_machine = {
    STATE_INIT,
    0U,
    0U,
    0U,
    0,
    0,
    0U,
    0,
    0,
    RISE_MIN_DUTY,
    RISE_MIN_DUTY,
    RISE_MIN_DUTY,
    RISE_MIN_DUTY,
    RISE_MIN_DUTY,
    RISE_MIN_DUTY,
    0U,
    0U,
    0U,
    0U,
    0U,
    0U,
    0U,
    0U,
    0U,
    POWER_MODE_BUCK,
    0U};

void MinimalStateMachine_Init(void)
{
  g_minimal_state_machine.state = STATE_INIT;
  g_minimal_state_machine.wait_counter = 0U;
  g_minimal_state_machine.offset_capture_complete = 0U;
  g_minimal_state_machine.output_enabled_request = 0U;
  g_minimal_state_machine.vref_snapshot = 0;
  g_minimal_state_machine.iref_snapshot = 0;
  g_minimal_state_machine.rise_stage = 0U;
  g_minimal_state_machine.rise_vref_start = 0;
  g_minimal_state_machine.rise_iref_start = 0;
  g_minimal_state_machine.rise_q1_max_duty = RISE_MIN_DUTY;
  g_minimal_state_machine.rise_q2_max_duty = RISE_MIN_DUTY;
  g_minimal_state_machine.rise_q1_duty = RISE_MIN_DUTY;
  g_minimal_state_machine.rise_q1_applied_duty = RISE_MIN_DUTY;
  g_minimal_state_machine.rise_q2_duty = RISE_MIN_DUTY;
  g_minimal_state_machine.rise_q2_applied_duty = RISE_MIN_DUTY;
  g_minimal_state_machine.rise_q1_counter = 0U;
  g_minimal_state_machine.rise_q2_counter = 0U;
  g_minimal_state_machine.pwm_enable_request = 0U;
  g_minimal_state_machine.q1_handover_ready = 0U;
  g_minimal_state_machine.q2_handover_ready = 0U;
  g_minimal_state_machine.ta1_output_enabled = 0U;
  g_minimal_state_machine.ta2_output_enabled = 0U;
  g_minimal_state_machine.loop_control_enable = 0U;
  g_minimal_state_machine.q1_closed_loop_enable = 0U;
  g_minimal_state_machine.selected_mode = POWER_MODE_BUCK;
  g_minimal_state_machine.mode_confirmed = 0U;
  PowerMode_Reset();
  PowerProtection_Reset();
  MinimalStateMachine_UpdateIndicators();
}

static void MinimalStateMachine_StopToWait(void)
{
  HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TA1 | HRTIM_OUTPUT_TA2);
  g_minimal_state_machine.output_enabled_request = 0U;
  g_minimal_state_machine.rise_stage = 0U;
  g_minimal_state_machine.rise_q1_max_duty = RISE_MIN_DUTY;
  g_minimal_state_machine.rise_q2_max_duty = RISE_MIN_DUTY;
  g_minimal_state_machine.rise_q1_duty = RISE_MIN_DUTY;
  g_minimal_state_machine.rise_q1_applied_duty = RISE_MIN_DUTY;
  g_minimal_state_machine.rise_q2_duty = RISE_MIN_DUTY;
  g_minimal_state_machine.rise_q2_applied_duty = RISE_MIN_DUTY;
  g_minimal_state_machine.rise_q1_counter = 0U;
  g_minimal_state_machine.rise_q2_counter = 0U;
  g_minimal_state_machine.pwm_enable_request = 0U;
  g_minimal_state_machine.q1_handover_ready = 0U;
  g_minimal_state_machine.q2_handover_ready = 0U;
  g_minimal_state_machine.ta1_output_enabled = 0U;
  g_minimal_state_machine.ta2_output_enabled = 0U;
  g_minimal_state_machine.loop_control_enable = 0U;
  g_minimal_state_machine.q1_closed_loop_enable = 0U;
  RuntimeMode_Reset();
  LowRateLoopCalc_Reset();
  PowerProtection_Reset();
  g_minimal_state_machine.state = STATE_WAIT;
}

static uint8_t MinimalStateMachine_BuckReadyForRun(void)
{
  int32_t vref_target = g_target_calibration.vref_target;
  int32_t vout_feedback = g_target_calibration.vy_calibrated;
  int32_t handover_threshold;

  if ((g_minimal_state_machine.ta1_output_enabled == 0U) ||
      (g_minimal_state_machine.ta2_output_enabled == 0U))
  {
    return 0U;
  }

  if (vref_target <= TARGET_MIN_VREF)
  {
    return 1U;
  }

  handover_threshold = vref_target - BUCK_HANDOVER_MIN_MARGIN;
  if (handover_threshold < TARGET_MIN_VREF)
  {
    handover_threshold = TARGET_MIN_VREF;
  }

  if (vout_feedback >= handover_threshold)
  {
    return 1U;
  }

  if (g_minimal_state_machine.rise_q1_applied_duty >= BUCK_RISE_MAX_DUTY)
  {
    return 1U;
  }

  return 0U;
}

void MinimalStateMachine_Tick5ms(void)
{
  if ((RuntimeMode_Get() == RUNTIME_MODE_BUCK) &&
      (g_minimal_state_machine.state != STATE_WAIT) &&
      (HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin) != GPIO_PIN_RESET))
  {
    MinimalStateMachine_StopToWait();
    MinimalStateMachine_UpdateIndicators();
    return;
  }

  switch (g_minimal_state_machine.state)
  {
  case STATE_INIT:
    g_minimal_state_machine.vref_snapshot = 0;
    g_minimal_state_machine.iref_snapshot = 0;

    if (g_sample_processed.offset_ready != 0U)
    {
      g_minimal_state_machine.offset_capture_complete = 1U;
      g_minimal_state_machine.wait_counter = 0U;
      g_minimal_state_machine.state = STATE_WAIT;
    }
    break;

  case STATE_WAIT:
    if (g_minimal_state_machine.wait_counter < 100U)
    {
      g_minimal_state_machine.wait_counter++;
    }

    g_minimal_state_machine.vref_snapshot = g_target_calibration.vref_target;
    g_minimal_state_machine.iref_snapshot = g_target_calibration.iref_target;

    if ((g_minimal_state_machine.mode_confirmed == 0U) &&
        (g_minimal_state_machine.wait_counter >= 100U))
    {
      if ((g_sample_processed.vx_avg > AUTO_MODE_VALID_COUNTS) ||
          (g_sample_processed.vy_avg > AUTO_MODE_VALID_COUNTS))
      {
        if (g_sample_processed.vx_avg > (g_sample_processed.vy_avg + AUTO_MODE_DELTA_COUNTS))
        {
          PowerMode_Request(POWER_MODE_BUCK);
          g_minimal_state_machine.selected_mode = POWER_MODE_BUCK;
          g_minimal_state_machine.mode_confirmed = 1U;
        }
        else
        {
          PowerMode_Request(POWER_MODE_BOOST);
          g_minimal_state_machine.selected_mode = POWER_MODE_BOOST;
          g_minimal_state_machine.mode_confirmed = 1U;
          BoostStateMachine_Init();
        }
      }
    }

    if ((g_minimal_state_machine.mode_confirmed != 0U) &&
        (HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin) == GPIO_PIN_RESET))
    {
      PowerMode_Lock();
      PowerModeHrtim_ApplyOutputRole(PowerMode_GetActive());
      if (PowerMode_GetActive() == POWER_MODE_BOOST)
      {
        BoostStateMachine_Init();
        g_boost_state_machine.output_enabled_request = 1U;
        RuntimeMode_Set(RUNTIME_MODE_BOOST);
        g_minimal_state_machine.rise_stage = 0U;
        g_minimal_state_machine.state = STATE_RISE;
      }
      else
      {
        g_minimal_state_machine.output_enabled_request = 1U;
        g_minimal_state_machine.rise_stage = 0U;
        g_minimal_state_machine.state = STATE_RISE;
        RuntimeMode_Set(RUNTIME_MODE_BUCK);
      }
    }
    break;

  case STATE_RISE:
    if (PowerMode_GetActive() == POWER_MODE_BOOST)
    {
      g_minimal_state_machine.rise_stage = 1U;
      g_minimal_state_machine.rise_q1_max_duty = g_boost_mode_control.q1_max_duty;
      g_minimal_state_machine.rise_q2_max_duty = g_boost_mode_control.q2_max_duty;
      g_minimal_state_machine.rise_q1_duty = g_boost_mode_control.q1_duty;
      g_minimal_state_machine.rise_q2_duty = g_boost_mode_control.q2_duty;
      g_minimal_state_machine.rise_q1_applied_duty = g_boost_mode_control.q1_duty;
      g_minimal_state_machine.rise_q2_applied_duty = g_boost_mode_control.q2_duty;
      g_minimal_state_machine.ta1_output_enabled = g_boost_state_machine.pwm_enabled;
      g_minimal_state_machine.ta2_output_enabled = g_boost_state_machine.pwm_enabled;

      if (g_boost_state_machine.state == BOOST_STATE_RUN)
      {
        g_minimal_state_machine.rise_stage = 2U;
        g_minimal_state_machine.loop_control_enable = 1U;
        g_minimal_state_machine.q1_closed_loop_enable = 1U;
        g_minimal_state_machine.state = STATE_RUN;
      }
      break;
    }

    if (g_minimal_state_machine.rise_stage == 0U)
    {
      g_minimal_state_machine.rise_vref_start = 0;
      g_minimal_state_machine.rise_iref_start = 0;
      g_minimal_state_machine.rise_q1_max_duty = RISE_MIN_DUTY;
      g_minimal_state_machine.rise_q2_max_duty = RISE_MIN_DUTY;
      g_minimal_state_machine.rise_q1_duty = RISE_MIN_DUTY;
      g_minimal_state_machine.rise_q1_applied_duty = RISE_MIN_DUTY;
      g_minimal_state_machine.rise_q2_duty = RISE_MIN_DUTY;
      g_minimal_state_machine.rise_q2_applied_duty = RISE_MIN_DUTY;
      g_minimal_state_machine.rise_q1_counter = 0U;
      g_minimal_state_machine.rise_q2_counter = 0U;
      g_minimal_state_machine.pwm_enable_request = 0U;
      g_minimal_state_machine.q1_handover_ready = 0U;
      g_minimal_state_machine.q2_handover_ready = 0U;
      g_minimal_state_machine.ta1_output_enabled = 0U;
      g_minimal_state_machine.ta2_output_enabled = 0U;
      g_minimal_state_machine.loop_control_enable = 0U;
      g_minimal_state_machine.q1_closed_loop_enable = 0U;
      LowRateLoopCalc_Reset();
      MinimalStateMachine_ApplyTa1Duty();
      MinimalStateMachine_ApplyTa2Window();
      g_minimal_state_machine.rise_stage = 1U;
    }
    else if (g_minimal_state_machine.rise_stage == 1U)
    {
      if (g_minimal_state_machine.pwm_enable_request == 0U)
      {
        g_minimal_state_machine.pwm_enable_request = 1U;
        g_minimal_state_machine.q1_handover_ready = 1U;
      }

      if ((g_minimal_state_machine.q1_handover_ready != 0U) &&
          (g_minimal_state_machine.ta1_output_enabled == 0U))
      {
        if (HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TA1) == HAL_OK)
        {
          g_minimal_state_machine.ta1_output_enabled = 1U;
        }
      }

      if (g_minimal_state_machine.rise_q1_max_duty < BUCK_RISE_MAX_DUTY)
      {
        g_minimal_state_machine.rise_q1_counter++;
        g_minimal_state_machine.rise_q1_max_duty =
            RISE_MIN_DUTY + ((int32_t)g_minimal_state_machine.rise_q1_counter * BUCK_RISE_DUTY_STEP);

        if (g_minimal_state_machine.rise_q1_max_duty > BUCK_RISE_MAX_DUTY)
        {
          g_minimal_state_machine.rise_q1_max_duty = BUCK_RISE_MAX_DUTY;
        }
      }

      g_minimal_state_machine.rise_q1_duty = g_minimal_state_machine.rise_q1_max_duty;
      MinimalStateMachine_ApplyTa1Duty();

      if ((g_minimal_state_machine.rise_q1_applied_duty >= BUCK_RISE_TA2_ENABLE_DUTY) &&
          (g_minimal_state_machine.q2_handover_ready == 0U))
      {
        g_minimal_state_machine.q2_handover_ready = 1U;
      }

      if ((g_minimal_state_machine.q2_handover_ready != 0U) &&
          (g_minimal_state_machine.ta2_output_enabled == 0U))
      {
        if (HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TA2) == HAL_OK)
        {
          g_minimal_state_machine.ta2_output_enabled = 1U;
        }
      }

      if (g_minimal_state_machine.ta2_output_enabled != 0U)
      {
        g_minimal_state_machine.rise_q2_max_duty = BUCK_RISE_MAX_DUTY;
        g_minimal_state_machine.rise_q2_duty = 32767 - g_minimal_state_machine.rise_q1_applied_duty;
        if (g_minimal_state_machine.rise_q2_duty > g_minimal_state_machine.rise_q2_max_duty)
        {
          g_minimal_state_machine.rise_q2_duty = g_minimal_state_machine.rise_q2_max_duty;
        }

        MinimalStateMachine_ApplyTa2Window();

        if (MinimalStateMachine_BuckReadyForRun() != 0U)
        {
          g_minimal_state_machine.rise_stage = 2U;
          g_minimal_state_machine.rise_q1_max_duty = TA1_SAFE_MAX_DUTY_Q15;
          g_minimal_state_machine.rise_q2_max_duty = RISE_MAX_DUTY;
          LowRateLoopCalc_SeedFromDuty(g_minimal_state_machine.rise_q1_applied_duty);
          g_minimal_state_machine.loop_control_enable = 1U;
          g_minimal_state_machine.q1_closed_loop_enable = 1U;
          g_minimal_state_machine.state = STATE_RUN;
        }
      }
    }
    break;

  case STATE_RUN:
  case STATE_ERR:
  default:
    break;
  }

  MinimalStateMachine_UpdateIndicators();
}

void MinimalStateMachine_RunControlTick(void)
{
  int32_t q1_clamped;
  int32_t q2_clamped;

  if ((g_minimal_state_machine.state != STATE_RUN) ||
      (g_minimal_state_machine.loop_control_enable == 0U) ||
      (g_minimal_state_machine.q1_closed_loop_enable == 0U))
  {
    return;
  }

  if (PowerMode_GetActive() == POWER_MODE_BOOST)
  {
    return;
  }

  q1_clamped = g_lowrate_loop_calc.q1_duty_cmd;
  if (q1_clamped < RISE_MIN_DUTY)
  {
    q1_clamped = RISE_MIN_DUTY;
  }
  if (q1_clamped > TA1_SAFE_MAX_DUTY_Q15)
  {
    q1_clamped = TA1_SAFE_MAX_DUTY_Q15;
  }

  g_minimal_state_machine.rise_q1_duty = q1_clamped;
  MinimalStateMachine_ApplyTa1Duty();

  q2_clamped = 32767 - g_minimal_state_machine.rise_q1_applied_duty;
  if (q2_clamped < RISE_MIN_DUTY)
  {
    q2_clamped = RISE_MIN_DUTY;
  }
  if (q2_clamped > g_minimal_state_machine.rise_q2_max_duty)
  {
    q2_clamped = g_minimal_state_machine.rise_q2_max_duty;
  }

  g_minimal_state_machine.rise_q2_duty = q2_clamped;
  MinimalStateMachine_ApplyTa2Window();
}

const char *MinimalStateMachine_StateName(MinimalState_t state)
{
  switch (state)
  {
  case STATE_INIT:
    return "Init";
  case STATE_WAIT:
    return "Wait";
  case STATE_RISE:
    return "Rise";
  case STATE_RUN:
    return "Run";
  case STATE_ERR:
    return "Err";
  default:
    return "Unknown";
  }
}
