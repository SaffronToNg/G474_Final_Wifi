#include "power_mode_hrtim.h"

#include "hrtim.h"

void PowerModeHrtim_ApplyOutputRole(PowerMode_t mode)
{
  HRTIM_OutputCfgTypeDef pOutputCfg = {0};

  pOutputCfg.Polarity = HRTIM_OUTPUTPOLARITY_HIGH;
  pOutputCfg.IdleMode = HRTIM_OUTPUTIDLEMODE_NONE;
  pOutputCfg.IdleLevel = HRTIM_OUTPUTIDLELEVEL_INACTIVE;
  pOutputCfg.FaultLevel = HRTIM_OUTPUTFAULTLEVEL_NONE;
  pOutputCfg.ChopperModeEnable = HRTIM_OUTPUTCHOPPERMODE_DISABLED;
  pOutputCfg.BurstModeEntryDelayed = HRTIM_OUTPUTBURSTMODEENTRY_REGULAR;

  if (mode == POWER_MODE_BOOST)
  {
    pOutputCfg.SetSource = HRTIM_OUTPUTSET_TIMCMP2;
    pOutputCfg.ResetSource = HRTIM_OUTPUTRESET_TIMCMP3;
    if (HAL_HRTIM_WaveformOutputConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A, HRTIM_OUTPUT_TA1, &pOutputCfg) != HAL_OK)
    {
      Error_Handler();
    }

    pOutputCfg.SetSource = HRTIM_OUTPUTSET_TIMPER;
    pOutputCfg.ResetSource = HRTIM_OUTPUTRESET_TIMCMP1;
    if (HAL_HRTIM_WaveformOutputConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A, HRTIM_OUTPUT_TA2, &pOutputCfg) != HAL_OK)
    {
      Error_Handler();
    }
  }
  else
  {
    pOutputCfg.SetSource = HRTIM_OUTPUTSET_TIMPER;
    pOutputCfg.ResetSource = HRTIM_OUTPUTRESET_TIMCMP1;
    if (HAL_HRTIM_WaveformOutputConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A, HRTIM_OUTPUT_TA1, &pOutputCfg) != HAL_OK)
    {
      Error_Handler();
    }

    pOutputCfg.SetSource = HRTIM_OUTPUTSET_TIMCMP2;
    pOutputCfg.ResetSource = HRTIM_OUTPUTRESET_TIMCMP3;
    if (HAL_HRTIM_WaveformOutputConfig(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A, HRTIM_OUTPUT_TA2, &pOutputCfg) != HAL_OK)
    {
      Error_Handler();
    }
  }
}
