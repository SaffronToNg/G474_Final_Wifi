
/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dac.h"
#include "dma.h"
#include "hrtim.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"
#include "sample_semantics.h"
#include "sample_processing.h"
#include "power_mode.h"
#include "power_mode_hrtim.h"
#include "power_protection.h"
#include "runtime_mode.h"
#include "target_calibration.h"
#include "boost_mode.h"
#include "boost_state_machine.h"
#include "minimal_state_machine.h"
#include "lowrate_loop_calc.h"
#include "tft_debug_view.h"
#include "hrtim.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "stm32g4xx_ll_system.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define STAGE1_STATUS_PRINT_INTERVAL_MS 1000U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
__IO uint16_t g_adc1_dma_buffer[5] = {0};
__IO uint16_t g_adc2_dma_buffer[3] = {0};
__IO uint8_t g_usart3_rx_byte = 0;
__IO uint8_t g_usart3_last_rx_byte = 0;
__IO uint32_t g_tim2_irq_count = 0;
__IO uint32_t g_tim3_irq_count = 0;
__IO uint32_t g_hrtim_tima_rep_count = 0;
__IO uint32_t g_adc1_dma_cplt_count = 0;
__IO uint32_t g_adc2_dma_cplt_count = 0;
__IO uint32_t g_usart3_rx_event_count = 0;
__IO uint32_t g_usart3_rx_error_count = 0;
__IO uint8_t g_stage1_startup_complete = 0;
static uint32_t s_stage1_last_status_tick = 0;
__IO uint16_t g_uart3_print_div = 0;

/* USART3 命令接收状态：固定格式 V#### */
static uint8_t s_usart3_cmd_active = 0U;
static uint8_t s_usart3_cmd_digits = 0U;
static int32_t s_usart3_cmd_value = 0;

/* USART3 待应用命令：中断里只更新 pending，主循环再真正赋值 */
volatile uint8_t  s_vtar_pending_flag = 0U;
volatile int32_t  s_vtar_pending_display = 0;  /* 上位机发来的 TFT 显示值 */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static void Stage1_StartSequence(void);
static void Stage1_PrintStatus(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void Stage1_StartSequence(void)
{
  if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)g_adc1_dma_buffer, 5U) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_ADC_Start_DMA(&hadc2, (uint32_t *)g_adc2_dma_buffer, 3U) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_HRTIM_WaveformOutputStop(&hhrtim1, HRTIM_OUTPUT_TA1 | HRTIM_OUTPUT_TA2) != HAL_OK)
  {
    Error_Handler();
  }

  __HAL_HRTIM_TIMER_DISABLE_IT(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A, HRTIM_TIM_IT_REP);

  if (HAL_HRTIM_WaveformCountStart(&hhrtim1, HRTIM_TIMERID_TIMER_A) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_TIM_Base_Start_IT(&htim2) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_TIM_Base_Start_IT(&htim3) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_UART_Receive_IT(&huart3, (uint8_t *)&g_usart3_rx_byte, 1U) != HAL_OK)
  {
    Error_Handler();
  }

  MinimalStateMachine_Init();
  BoostStateMachine_Init();
  RuntimeMode_Reset();

  g_stage1_startup_complete = 1U;
  s_stage1_last_status_tick = HAL_GetTick();
}

/*
 * 周期发送 TFT 最终显示值（所有 mV 换算、滤波、offset、增益、死区补偿已算完）
 * 与 TFT 屏幕上 LCDShowFnum 喂入的数值完全一致
 */
static void Stage1_PrintStatus(void)
{
  uint8_t frame[14];
  uint16_t vals[6];
  uint8_t i, xsum;

  if ((HAL_GetTick() - s_stage1_last_status_tick) < 1000U)
    return;
  s_stage1_last_status_tick = HAL_GetTick();

  vals[0] = (uint16_t)PowerMode_GetActive();
  vals[1] = g_tft_vin_display;
  vals[2] = g_tft_vout_display;
  vals[3] = g_tft_ii_display;
  vals[4] = g_tft_io_display;
  vals[5] = g_tft_vtar_display;

  frame[0] = 0xAA;
  xsum = 0xAA;
  for (i = 0; i < 6; i++)
  {
    frame[1 + i*2]     = (uint8_t)(vals[i] & 0xFF);
    frame[1 + i*2 + 1] = (uint8_t)(vals[i] >> 8);
    xsum ^= frame[1 + i*2] ^ frame[1 + i*2 + 1];
  }
  frame[13] = xsum;

  {
    const uint8_t *p = frame;
    uint16_t rem = 14;
    uint32_t t0 = HAL_GetTick();
    while (rem > 0U)
    {
      if ((USART3->ISR & USART_ISR_TXE_TXFNF) != 0U)
      {
        USART3->TDR = *p++;
        rem--;
      }
      if ((HAL_GetTick() - t0) > 5U) { break; }
    }
  }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  LL_DBGMCU_SetTracePinAssignment(LL_DBGMCU_TRACE_NONE);

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_ADC2_Init();
  MX_HRTIM1_Init();
  MX_TIM2_Init();
  MX_USART3_UART_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
  HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, GPIO_PIN_SET);
  Stage1_StartSequence();
  TftDebugView_Init();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    /* 应用 USART3 中断里解析好的 VTAR 显示值，直接设最终 vref_target */
    if (s_vtar_pending_flag != 0U)
    {
      int32_t display_val = s_vtar_pending_display;
      int32_t vref_new;
      uint32_t tft_display;

      /*
       * TFT VTAR 正向链:
       *   1. vtar_raw = vref_target * 6800 >> 12
       *   2. if BUCK: vtar = (vtar_raw * 981 + 500) / 1000
       *      if BOOST: vtar = vtar_raw
       *
       * 此处反向: display_val(目标显示值) → vref_target
       */
      if (PowerMode_GetActive() == POWER_MODE_BOOST)
      {
        tft_display = (uint32_t)display_val;
      }
      else
      {
        /* 先还原 BUCK 增益: x = (display * 1000 + 490) / 981 */
        tft_display = ((uint32_t)display_val * 1000U + 490U) / 981U;
      }
      /* 再还原 vref = tft_display * 4096 / 6800，四舍五入 */
      vref_new = (int32_t)((tft_display * 4096U + 3400U) / 6800U);
      if (vref_new < 0) vref_new = 0;
      if (vref_new > TARGET_MAX_VREF) vref_new = TARGET_MAX_VREF;

      /* 先置接管标志再写 vref，避免 TIM2 中断在间隙里步进跑偏 */
      g_vtar_serial_override = 1U;
      g_vtar_serial_anchor = g_sample_processed.vadj_avg;
      g_target_calibration.vref_target = vref_new;
      s_vtar_pending_flag = 0U;
    }

    /* 周期打印：仅在 TIM3 累计够间隔时才输出，避免阻塞 TX 与中断 RX 冲突 */
    if (g_uart3_print_div >= 200)
    {
      g_uart3_print_div = 0;
      Stage1_PrintStatus();
    }

    TftDebugView_Update();
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV1;
  RCC_OscInitStruct.PLL.PLLN = 18;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM2)
  {
    g_tim2_irq_count++;
    SampleSemantic_UpdateFromBuffers();
    SampleProcessing_Update();
    TargetCalibration_UpdateInstant();
    TargetCalibration_UpdateStepped();
    if (RuntimeMode_Get() == RUNTIME_MODE_BOOST)
    {
      BoostStateMachine_RunControlTick();
    }
    else if (RuntimeMode_Get() == RUNTIME_MODE_BUCK)
    {
      LowRateLoopCalc_Tick();
      MinimalStateMachine_RunControlTick();
    }
    PowerProtection_Evaluate();
  }
  else if (htim->Instance == TIM3)
  {
    g_uart3_print_div++;
    g_tim3_irq_count++;
    if (RuntimeMode_Get() == RUNTIME_MODE_BOOST)
    {
      BoostStateMachine_Tick5ms();
    }
    else
    {
      MinimalStateMachine_Tick5ms();
    }
  }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
  if (hadc->Instance == ADC1)
  {
    g_adc1_dma_cplt_count++;
  }
  else if (hadc->Instance == ADC2)
  {
    g_adc2_dma_cplt_count++;
  }
}

void HAL_HRTIM_RepetitionEventCallback(HRTIM_HandleTypeDef *hhrtim, uint32_t TimerIdx)
{
  if ((hhrtim->Instance == HRTIM1) && (TimerIdx == HRTIM_TIMERINDEX_TIMER_A))
  {
    g_hrtim_tima_rep_count++;
  }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART3)
  {
    uint8_t byte = g_usart3_rx_byte;
    g_usart3_last_rx_byte = byte;
    g_usart3_rx_event_count++;

    if (s_usart3_cmd_active == 0U)
    {
      if (byte == 'V')
      {
        s_usart3_cmd_active = 1U;
        s_usart3_cmd_digits = 0U;
        s_usart3_cmd_value = 0;
      }
    }
    else
    {
      if ((byte >= '0') && (byte <= '9'))
      {
        s_usart3_cmd_value = (s_usart3_cmd_value * 10) + (int32_t)(byte - '0');
        s_usart3_cmd_digits++;

        if (s_usart3_cmd_digits >= 4U)
        {
          s_vtar_pending_display = s_usart3_cmd_value;
          s_vtar_pending_flag = 1U;
          s_usart3_cmd_active = 0U;
          s_usart3_cmd_digits = 0U;
          s_usart3_cmd_value = 0;
        }
      }
      else
      {
        s_usart3_cmd_active = 0U;
        s_usart3_cmd_digits = 0U;
        s_usart3_cmd_value = 0;
      }
    }

    if (HAL_UART_Receive_IT(&huart3, (uint8_t *)&g_usart3_rx_byte, 1U) != HAL_OK)
    {
      g_usart3_rx_error_count++;
    }
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART3)
  {
    g_usart3_rx_error_count++;
    (void)HAL_UART_Receive_IT(&huart3, (uint8_t *)&g_usart3_rx_byte, 1U);
  }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
