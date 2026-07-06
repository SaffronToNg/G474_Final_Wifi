#include "tft_debug_view.h"

#include "LCDDriver.h"
#include "boot_image_160.h"
#include "boost_state_machine.h"
#include "lowrate_loop_calc.h"
#include "minimal_state_machine.h"
#include "power_mode.h"
#include "sample_processing.h"
#include "target_calibration.h"

#define TFT_DEBUG_UPDATE_INTERVAL_MS 50U
#define TFT_DEBUG_BOOT_IMAGE_HOLD_MS 80U
#define TFT_CHAR_WIDTH               16U
#define TFT_LINE_HEIGHT              36U
#define TFT_VALUE_X_LARGE            96U
#define TFT_BOOT_IMAGE_SIZE          160U
#define TFT_BOOT_IMAGE_X             40U
#define TFT_BOOT_IMAGE_Y             40U
/*
 * 显示层临时电流量纲：abs(Vsample - 1.64V) * 10。
 * LCDShowFnum() 以“数值 / 100.0”显示，因此：
 * - 先把实时 ADC 采样值换算成 mV
 * - 再取它与 1640mV 的绝对差
 * - 该 mV 差值恰好就是要传给 LCD 的显示整数值
 */
#define TFT_DISPLAY_SAMPLE_MV_NUMERATOR     3300U
#define TFT_DISPLAY_SAMPLE_Q12_DENOMINATOR  4096U
#define TFT_DISPLAY_SAMPLE_ROUNDING         2048U
#define TFT_DISPLAY_CURRENT_ZERO_MV         1640U
#define TFT_DISPLAY_FILTER_SHIFT            2U
#define TFT_DISPLAY_STABLE_DEADBAND_BUCK    3U
#define TFT_DISPLAY_STABLE_DEADBAND_II      4U
/* 不同模式下根据实测点对 IX/IY 显示做独立微调，仅影响显示层。 */
#define TFT_DISPLAY_GAIN_DENOMINATOR             1000U
#define TFT_DISPLAY_GAIN_ROUNDING                500U
#define TFT_DISPLAY_IX_GAIN_NUMERATOR_BOOST      1135U
#define TFT_DISPLAY_IY_GAIN_NUMERATOR_BOOST      1376U
#define TFT_DISPLAY_IX_GAIN_NUMERATOR_BUCK       1080U
#define TFT_DISPLAY_IY_GAIN_NUMERATOR_BUCK       1360U
#define TFT_DISPLAY_VTAR_GAIN_NUMERATOR_BUCK      995U
#define TFT_DISPLAY_VTAR_GAIN_NUMERATOR_BOOST     998U
/* BUCK 下 IY/IOUT 基于当前新板最新固定点位实测点重拟合：Iy_real ≈ 0.964 * Iy_display + 0.02A */
#define TFT_DISPLAY_IY_LINEAR_NUMERATOR_BUCK     964U
#define TFT_DISPLAY_IY_LINEAR_DENOMINATOR_BUCK   1000U
#define TFT_DISPLAY_IY_LINEAR_ROUNDING_BUCK      500U
#define TFT_DISPLAY_IY_LINEAR_OFFSET_BUCK        2
#define TFT_MODE_BADGE_X_START       52U
#define TFT_MODE_BADGE_Y_START       4U
#define TFT_MODE_BADGE_X_END         187U
#define TFT_MODE_BADGE_Y_END         35U
#define TFT_VTAR_LABEL_X             80U
#define TFT_VTAR_LABEL_Y             56U
#define TFT_VTAR_PANEL_X_START       56U
#define TFT_VTAR_PANEL_Y_START       92U
#define TFT_VTAR_PANEL_X_END         183U
#define TFT_VTAR_PANEL_Y_END         123U
#define TFT_VTAR_VALUE_X             72U
#define TFT_VTAR_VALUE_Y             92U
#define TFT_STATE_X                  80U
#define TFT_STATE_Y                  128U
#define TFT_BOTTOM_ROW1_Y            168U
#define TFT_BOTTOM_ROW2_Y            204U
#define TFT_BOTTOM_LEFT_LABEL_X      0U
#define TFT_BOTTOM_LEFT_VALUE_X      48U
#define TFT_BOTTOM_LEFT_UNIT_X       128U
#define TFT_BOTTOM_RIGHT_LABEL_X     128U
#define TFT_BOTTOM_RIGHT_VALUE_X     160U
#define TFT_BOTTOM_RIGHT_UNIT_X      208U

static uint32_t s_last_update_tick = 0U;
static uint16_t s_last_vin_display = 0xFFFFU;
static uint16_t s_last_vout_display = 0xFFFFU;
static uint16_t s_last_ix_display = 0xFFFFU;
static uint16_t s_last_vtar_display = 0xFFFFU;
static uint16_t s_last_iy_display = 0xFFFFU;
static uint16_t s_last_ii_lcd = 0xFFFFU;
static uint16_t s_last_io_lcd = 0xFFFFU;
static uint32_t s_ix_sample_mv_filter_accum = 0U;
static uint32_t s_iy_sample_mv_filter_accum = 0U;
static uint8_t s_display_filter_initialized = 0U;
static PowerMode_t s_last_mode_display = (PowerMode_t)0xFF;
static uint8_t s_last_state_display = 0xFFU;

uint16_t g_tft_vin_display = 0U;
uint16_t g_tft_vout_display = 0U;
uint16_t g_tft_ii_display = 0U;
uint16_t g_tft_io_display = 0U;
uint16_t g_tft_vtar_display = 0U;

static uint16_t TftDebugView_ColorForState(MinimalState_t state)
{
  switch (state)
  {
  case STATE_INIT:
    return YELLOW;
  case STATE_WAIT:
    return GREEN;
  case STATE_RISE:
    return BLUE;
  case STATE_RUN:
    return GREEN;
  case STATE_ERR:
    return RED;
  default:
    return WHITE;
  }
}

static void TftDebugView_DrawGlyph(uint16_t x, uint16_t y, char ch, uint16_t color, uint16_t background)
{
  if ((ch >= 'A') && (ch <= 'Z'))
  {
    LCDshowChar(x, y, color, background, (uint16_t)(ch - 'A'));
  }
  else if ((ch >= '0') && (ch <= '9'))
  {
    LCDshowDate(x, y, color, background, (uint16_t)(ch - '0'));
  }
  else if (ch == ':')
  {
    LCDshowDot(x, y, color, background, 1U);
  }
  else if (ch == '.')
  {
    LCDshowDot(x, y, color, background, 2U);
  }
  else
  {
    LCDshowDot(x, y, background, background, 0U);
  }
}

static void TftDebugView_DrawText(uint16_t x, uint16_t y, const char *text, uint16_t color, uint16_t background)
{
  uint16_t cursor_x = x;

  while (*text != '\0')
  {
    TftDebugView_DrawGlyph(cursor_x, y, *text, color, background);
    cursor_x = (uint16_t)(cursor_x + TFT_CHAR_WIDTH);
    ++text;
  }
}

static void TftDebugView_DrawUnsignedRaw(uint16_t x, uint16_t y, uint32_t value, uint8_t digits, uint16_t color, uint16_t background)
{
  uint8_t index;
  uint32_t divisor = 1U;

  for (index = 1U; index < digits; ++index)
  {
    divisor *= 10U;
  }

  for (index = 0U; index < digits; ++index)
  {
    LCDShowNum(x, y, color, background, (uint16_t)((value / divisor) % 10U));
    x = (uint16_t)(x + TFT_CHAR_WIDTH);
    divisor /= 10U;
  }
}

static void TftDebugView_DrawSignedRaw(uint16_t x, uint16_t y, int32_t value, uint8_t digits, uint16_t color, uint16_t background)
{
  uint32_t magnitude;
  uint32_t max_value = 1U;
  uint8_t index;

  for (index = 0U; index < digits; ++index)
  {
    max_value *= 10U;
  }
  max_value -= 1U;

  if (value < 0)
  {
    TftDebugView_DrawText(x, y, "N", color, background);
    magnitude = (uint32_t)(-value);
  }
  else
  {
    TftDebugView_DrawText(x, y, "P", color, background);
    magnitude = (uint32_t)value;
  }

  if (magnitude > max_value)
  {
    magnitude = max_value;
  }

  TftDebugView_DrawUnsignedRaw((uint16_t)(x + TFT_CHAR_WIDTH), y, magnitude, digits, color, background);
}

static void TftDebugView_DrawBootImage(void)
{
  Lcd_Clear(g_boot_image_160[0]);
  Lcd_DrawImage(TFT_BOOT_IMAGE_X, TFT_BOOT_IMAGE_Y, TFT_BOOT_IMAGE_SIZE, TFT_BOOT_IMAGE_SIZE, g_boot_image_160);
}

static void TftDebugView_ShowBootPattern(uint32_t boot_tick)
{
  (void)boot_tick;
  TftDebugView_DrawBootImage();
}

static void TftDebugView_DrawMainLabels(void)
{
  Lcd_Clear(BLACK);
  TftDebugView_DrawText(TFT_VTAR_LABEL_X, TFT_VTAR_LABEL_Y, "VTAR:", GRAY0, BLACK);
  TftDebugView_DrawText(TFT_BOTTOM_LEFT_LABEL_X, TFT_BOTTOM_ROW1_Y, "VI:", WHITE, BLACK);
  TftDebugView_DrawText(TFT_BOTTOM_RIGHT_LABEL_X, TFT_BOTTOM_ROW1_Y, "VO", BLUE, BLACK);
  TftDebugView_DrawText(TFT_BOTTOM_LEFT_LABEL_X, TFT_BOTTOM_ROW2_Y, "II:", GREEN, BLACK);
  TftDebugView_DrawText(TFT_BOTTOM_RIGHT_LABEL_X, TFT_BOTTOM_ROW2_Y, "IO", YELLOW, BLACK);
  TftDebugView_DrawText((uint16_t)(TFT_VTAR_VALUE_X + (5U * TFT_CHAR_WIDTH)), TFT_VTAR_VALUE_Y, "V", YELLOW, BLACK);
  TftDebugView_DrawText(TFT_BOTTOM_RIGHT_UNIT_X, TFT_BOTTOM_ROW1_Y, "V", BLUE, BLACK);
  TftDebugView_DrawText(TFT_BOTTOM_RIGHT_UNIT_X, TFT_BOTTOM_ROW2_Y, "A", YELLOW, BLACK);
}

static void TftDebugView_DrawModeValue(uint16_t x, uint16_t y)
{
  PowerMode_t mode = PowerMode_GetActive();

  (void)x;
  (void)y;

  if (mode == s_last_mode_display)
  {
    return;
  }

  if (mode == POWER_MODE_BOOST)
  {
    Lcd_FillRect(TFT_MODE_BADGE_X_START, TFT_MODE_BADGE_Y_START, TFT_MODE_BADGE_X_END, TFT_MODE_BADGE_Y_END, BLUE);
    TftDebugView_DrawText(80U, TFT_MODE_BADGE_Y_START, "BOOST", WHITE, BLUE);
  }
  else
  {
    Lcd_FillRect(TFT_MODE_BADGE_X_START, TFT_MODE_BADGE_Y_START, TFT_MODE_BADGE_X_END, TFT_MODE_BADGE_Y_END, GREEN);
    TftDebugView_DrawText(88U, TFT_MODE_BADGE_Y_START, "BUCK", WHITE, GREEN);
  }

  s_last_mode_display = mode;
}

static void TftDebugView_DrawStateCompact(uint16_t x, uint16_t y)
{
  const char *state_name;
  uint16_t state_color;
  uint8_t state_index;

  (void)x;
  (void)y;

  if (PowerMode_GetActive() == POWER_MODE_BOOST)
  {
    state_name = BoostStateMachine_StateName(g_boost_state_machine.state);
    switch (g_boost_state_machine.state)
    {
    case BOOST_STATE_INIT:
      state_index = 0U;
      break;
    case BOOST_STATE_WAIT:
      state_index = 1U;
      break;
    case BOOST_STATE_RISE:
      state_index = 2U;
      break;
    case BOOST_STATE_RUN:
      state_index = 3U;
      break;
    case BOOST_STATE_ERR:
      state_index = 4U;
      break;
    default:
      state_index = 0xFFU;
      break;
    }
  }
  else
  {
    MinimalState_t state = g_minimal_state_machine.state;
    state_name = MinimalStateMachine_StateName(state);
    state_index = (uint8_t)state;
  }

  if (state_index == s_last_state_display)
  {
    return;
  }

  if (state_name[0] == 'I')
  {
    state_color = YELLOW;
    TftDebugView_DrawText(TFT_STATE_X, TFT_STATE_Y, "INIT", state_color, BLACK);
  }
  else if (state_name[0] == 'W')
  {
    state_color = GREEN;
    TftDebugView_DrawText(TFT_STATE_X, TFT_STATE_Y, "WAIT", state_color, BLACK);
  }
  else if (state_name[0] == 'R')
  {
    state_color = BLUE;
    if (state_name[1] == 'i')
    {
      TftDebugView_DrawText(TFT_STATE_X, TFT_STATE_Y, "RISE", state_color, BLACK);
    }
    else
    {
      TftDebugView_DrawText(TFT_STATE_X, TFT_STATE_Y, "RUN ", state_color, BLACK);
    }
  }
  else if (state_name[0] == 'E')
  {
    state_color = RED;
    TftDebugView_DrawText(TFT_STATE_X, TFT_STATE_Y, "ERR ", state_color, BLACK);
  }
  else
  {
    TftDebugView_DrawText(72U, TFT_STATE_Y, "UNKN", WHITE, BLACK);
  }

  s_last_state_display = state_index;
}

static void TftDebugView_DrawMainValues(void)
{
  uint16_t vin_display;
  uint16_t vout_display;
  uint16_t vtar_display = (uint16_t)((g_target_calibration.vref_target * 6800) >> 12);

  if (PowerMode_GetActive() == POWER_MODE_BOOST)
  {
    vtar_display = (uint16_t)((((uint32_t)vtar_display * TFT_DISPLAY_VTAR_GAIN_NUMERATOR_BOOST) +
                               TFT_DISPLAY_GAIN_ROUNDING) /
                              TFT_DISPLAY_GAIN_DENOMINATOR);
  }
  else
  {
    vtar_display = (uint16_t)((((uint32_t)vtar_display * TFT_DISPLAY_VTAR_GAIN_NUMERATOR_BUCK) +
                               TFT_DISPLAY_GAIN_ROUNDING) /
                              TFT_DISPLAY_GAIN_DENOMINATOR);
  }
  uint16_t ix_sample_mv;
  uint16_t iy_sample_mv;
  uint16_t ix_sample_mv_filtered;
  uint16_t iy_sample_mv_filtered;
  uint16_t ix_display_current;
  uint16_t iy_display_current;
  uint16_t io_display_current_buck;

  if (PowerMode_GetActive() == POWER_MODE_BOOST)
  {
    vin_display = (uint16_t)((g_target_calibration.vy_calibrated * 6800) >> 12);
    vout_display = (uint16_t)((g_target_calibration.vx_calibrated * 6800) >> 12);
  }
  else
  {
    vin_display = (uint16_t)((g_target_calibration.vx_calibrated * 6800) >> 12);
    vout_display = (uint16_t)((g_target_calibration.vy_calibrated * 6800) >> 12);
  }

  ix_sample_mv = (uint16_t)((((uint32_t)g_sample_processed.ix_raw * TFT_DISPLAY_SAMPLE_MV_NUMERATOR) +
                             TFT_DISPLAY_SAMPLE_ROUNDING) /
                            TFT_DISPLAY_SAMPLE_Q12_DENOMINATOR);
  iy_sample_mv = (uint16_t)((((uint32_t)g_sample_processed.iy_raw * TFT_DISPLAY_SAMPLE_MV_NUMERATOR) +
                             TFT_DISPLAY_SAMPLE_ROUNDING) /
                            TFT_DISPLAY_SAMPLE_Q12_DENOMINATOR);

  if (s_display_filter_initialized == 0U)
  {
    s_ix_sample_mv_filter_accum = ((uint32_t)ix_sample_mv) << TFT_DISPLAY_FILTER_SHIFT;
    s_iy_sample_mv_filter_accum = ((uint32_t)iy_sample_mv) << TFT_DISPLAY_FILTER_SHIFT;
    s_display_filter_initialized = 1U;
  }
  else
  {
    s_ix_sample_mv_filter_accum = s_ix_sample_mv_filter_accum + (uint32_t)ix_sample_mv -
                                  (s_ix_sample_mv_filter_accum >> TFT_DISPLAY_FILTER_SHIFT);
    s_iy_sample_mv_filter_accum = s_iy_sample_mv_filter_accum + (uint32_t)iy_sample_mv -
                                  (s_iy_sample_mv_filter_accum >> TFT_DISPLAY_FILTER_SHIFT);
  }

  ix_sample_mv_filtered = (uint16_t)(s_ix_sample_mv_filter_accum >> TFT_DISPLAY_FILTER_SHIFT);
  iy_sample_mv_filtered = (uint16_t)(s_iy_sample_mv_filter_accum >> TFT_DISPLAY_FILTER_SHIFT);

  if (ix_sample_mv_filtered >= TFT_DISPLAY_CURRENT_ZERO_MV)
  {
    ix_display_current = (uint16_t)(ix_sample_mv_filtered - TFT_DISPLAY_CURRENT_ZERO_MV);
  }
  else
  {
    ix_display_current = (uint16_t)(TFT_DISPLAY_CURRENT_ZERO_MV - ix_sample_mv_filtered);
  }

  if (iy_sample_mv_filtered >= TFT_DISPLAY_CURRENT_ZERO_MV)
  {
    iy_display_current = (uint16_t)(iy_sample_mv_filtered - TFT_DISPLAY_CURRENT_ZERO_MV);
  }
  else
  {
    iy_display_current = (uint16_t)(TFT_DISPLAY_CURRENT_ZERO_MV - iy_sample_mv_filtered);
  }

  if (vin_display > 9999U)
  {
    vin_display = 9999U;
  }

  if (vout_display > 9999U)
  {
    vout_display = 9999U;
  }

  if (PowerMode_GetActive() == POWER_MODE_BOOST)
  {
    ix_display_current = (uint16_t)((((uint32_t)ix_display_current * TFT_DISPLAY_IX_GAIN_NUMERATOR_BOOST) +
                                     TFT_DISPLAY_GAIN_ROUNDING) /
                                    TFT_DISPLAY_GAIN_DENOMINATOR);
    iy_display_current = (uint16_t)((((uint32_t)iy_display_current * TFT_DISPLAY_IY_GAIN_NUMERATOR_BOOST) +
                                     TFT_DISPLAY_GAIN_ROUNDING) /
                                    TFT_DISPLAY_GAIN_DENOMINATOR);
  }
  else
  {
    io_display_current_buck = (uint16_t)((((uint32_t)iy_display_current * TFT_DISPLAY_IY_GAIN_NUMERATOR_BUCK) +
                                          TFT_DISPLAY_GAIN_ROUNDING) /
                                         TFT_DISPLAY_GAIN_DENOMINATOR);
    io_display_current_buck = (uint16_t)((((uint32_t)io_display_current_buck * TFT_DISPLAY_IY_LINEAR_NUMERATOR_BUCK) +
                                          TFT_DISPLAY_IY_LINEAR_ROUNDING_BUCK) /
                                         TFT_DISPLAY_IY_LINEAR_DENOMINATOR_BUCK);
    if ((int32_t)io_display_current_buck + TFT_DISPLAY_IY_LINEAR_OFFSET_BUCK > 0)
    {
      io_display_current_buck = (uint16_t)((int32_t)io_display_current_buck + TFT_DISPLAY_IY_LINEAR_OFFSET_BUCK);
    }
    else
    {
      io_display_current_buck = 0U;
    }

    ix_display_current = (uint16_t)((((uint32_t)ix_display_current * TFT_DISPLAY_IX_GAIN_NUMERATOR_BUCK) +
                                     TFT_DISPLAY_GAIN_ROUNDING) /
                                    TFT_DISPLAY_GAIN_DENOMINATOR);

    if (s_last_ix_display != 0xFFFFU)
    {
      uint16_t ix_delta = (ix_display_current > s_last_ix_display) ?
                              (uint16_t)(ix_display_current - s_last_ix_display) :
                              (uint16_t)(s_last_ix_display - ix_display_current);
      if (ix_delta <= TFT_DISPLAY_STABLE_DEADBAND_BUCK)
      {
        ix_display_current = s_last_ix_display;
      }
    }

    if (s_last_iy_display != 0xFFFFU)
    {
      uint16_t io_delta = (io_display_current_buck > s_last_iy_display) ?
                              (uint16_t)(io_display_current_buck - s_last_iy_display) :
                              (uint16_t)(s_last_iy_display - io_display_current_buck);
      if (io_delta <= TFT_DISPLAY_STABLE_DEADBAND_BUCK)
      {
        io_display_current_buck = s_last_iy_display;
      }
    }

    iy_display_current = io_display_current_buck;
  }

  if (ix_display_current > 9999U)
  {
    ix_display_current = 9999U;
  }

  if (iy_display_current > 9999U)
  {
    iy_display_current = 9999U;
  }

  g_tft_vin_display   = vin_display;
  g_tft_vout_display  = vout_display;
  g_tft_vtar_display  = vtar_display;

  /* 模式语义映射：II/IO 与 VI/VO 一致，LCD 和串口都用切换后的值 */
  {
    uint16_t tft_left_current;
    uint16_t tft_right_current;

    if (PowerMode_GetActive() == POWER_MODE_BOOST)
    {
      tft_left_current  = iy_display_current;  /* BOOST: II=输入=Y侧 */
      tft_right_current = ix_display_current;  /* BOOST: IO=输出=X侧 */
    }
    else
    {
      tft_left_current  = ix_display_current;  /* BUCK:  II=输入=X侧 */
      tft_right_current = iy_display_current;  /* BUCK:  IO=输出=Y侧 */
    }

    if (s_last_ii_lcd != 0xFFFFU)
    {
      uint16_t ii_delta = (tft_left_current > s_last_ii_lcd) ?
                              (uint16_t)(tft_left_current - s_last_ii_lcd) :
                              (uint16_t)(s_last_ii_lcd - tft_left_current);
      if (ii_delta <= TFT_DISPLAY_STABLE_DEADBAND_II)
      {
        tft_left_current = s_last_ii_lcd;
      }
    }

    g_tft_ii_display = tft_left_current;
    g_tft_io_display = tft_right_current;

    TftDebugView_DrawModeValue(TFT_VALUE_X_LARGE, 0U);
    TftDebugView_DrawStateCompact(TFT_VALUE_X_LARGE, TFT_LINE_HEIGHT);

    if (vtar_display != s_last_vtar_display)
    {
      LCDShowFnum(TFT_VTAR_VALUE_X, TFT_VTAR_VALUE_Y, YELLOW, BLACK, vtar_display);
      s_last_vtar_display = vtar_display;
    }

    if (vin_display != s_last_vin_display)
    {
      LCDShowFnum(TFT_BOTTOM_LEFT_VALUE_X, TFT_BOTTOM_ROW1_Y, WHITE, BLACK, vin_display);
      s_last_vin_display = vin_display;
    }

    if (vout_display != s_last_vout_display)
    {
      LCDShowFnum(TFT_BOTTOM_RIGHT_VALUE_X, TFT_BOTTOM_ROW1_Y, BLUE, BLACK, vout_display);
      s_last_vout_display = vout_display;
    }

    if (tft_left_current != s_last_ii_lcd)
    {
      LCDShowFnum(TFT_BOTTOM_LEFT_VALUE_X, TFT_BOTTOM_ROW2_Y, GREEN, BLACK, tft_left_current);
      s_last_ii_lcd = tft_left_current;
    }

    if (tft_right_current != s_last_io_lcd)
    {
      LCDShowFnum(TFT_BOTTOM_RIGHT_VALUE_X, TFT_BOTTOM_ROW2_Y, YELLOW, BLACK, tft_right_current);
      s_last_io_lcd = tft_right_current;
    }
  }
}

void TftDebugView_Init(void)
{
  Lcd_Init();
  s_last_update_tick = 0U;

  TftDebugView_DrawBootImage();
  HAL_Delay(TFT_DEBUG_BOOT_IMAGE_HOLD_MS);

  Lcd_Clear(BLACK);
  s_last_vin_display = 0xFFFFU;
  s_last_vout_display = 0xFFFFU;
  s_last_ix_display = 0xFFFFU;
  s_last_vtar_display = 0xFFFFU;
  s_last_iy_display = 0xFFFFU;
  s_last_ii_lcd = 0xFFFFU;
  s_last_io_lcd = 0xFFFFU;
  s_ix_sample_mv_filter_accum = 0U;
  s_iy_sample_mv_filter_accum = 0U;
  s_display_filter_initialized = 0U;
  s_last_mode_display = (PowerMode_t)0xFF;
  s_last_state_display = (MinimalState_t)0xFF;
  TftDebugView_DrawMainLabels();
  TftDebugView_DrawMainValues();
}

void TftDebugView_Update(void)
{
  uint32_t current_tick = HAL_GetTick();

  if ((current_tick - s_last_update_tick) < TFT_DEBUG_UPDATE_INTERVAL_MS)
  {
    return;
  }

  s_last_update_tick = current_tick;
  TftDebugView_DrawMainValues();
}
