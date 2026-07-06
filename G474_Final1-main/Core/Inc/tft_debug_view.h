#ifndef __TFT_DEBUG_VIEW_H__
#define __TFT_DEBUG_VIEW_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void TftDebugView_Init(void);
void TftDebugView_Update(void);

/* TFT 最终显示值（所有补偿已算完，直接喂入 LCDShowFnum 的 uint16_t） */
extern uint16_t g_tft_vin_display;
extern uint16_t g_tft_vout_display;
extern uint16_t g_tft_ii_display;
extern uint16_t g_tft_io_display;
extern uint16_t g_tft_vtar_display;

#ifdef __cplusplus
}
#endif

#endif /* __TFT_DEBUG_VIEW_H__ */
